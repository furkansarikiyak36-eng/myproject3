#include "WorldStorageSubsystem.h"

#include "Engine/World.h"
#include "WorldItemOwnershipSubsystem.h"

UWorldItemOwnershipSubsystem* UWorldStorageSubsystem::GetOwnership() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
}

int64 UWorldStorageSubsystem::CountReservationsForStorage(const FGuid& StorageId) const
{
    int64 Quantity = 0;
    for (const TPair<FGuid, FWorldStorageIncomingReservation>& Pair : IncomingReservations)
    {
        if (Pair.Value.DestinationStorageId == StorageId)
            Quantity += FMath::Max(0, Pair.Value.Quantity);
    }
    return Quantity;
}

bool UWorldStorageSubsystem::RegisterStorage(const FGuid& StorageId, int32 CapacityUnits)
{
    if (!StorageId.IsValid() || CapacityUnits <= 0 || MaxStorages <= 0)
        return false;

    if (FWorldStorageContainerRecord* Existing = Storages.Find(StorageId))
    {
        if (Existing->CapacityUnits != CapacityUnits)
            return false;
        if (!Existing->bAvailable)
        {
            if (Existing->Revision == MAX_int64)
                return false;
            Existing->bAvailable = true;
            ++Existing->Revision;
        }
        LiveStorageIds.Add(StorageId);
        return true;
    }

    if (Storages.Num() >= MaxStorages)
        return false;

    FWorldStorageContainerRecord Record;
    Record.StorageId = StorageId;
    Record.CapacityUnits = CapacityUnits;
    Storages.Add(StorageId, Record);
    LiveStorageIds.Add(StorageId);
    return true;
}

bool UWorldStorageSubsystem::GetStorage(
    const FGuid& StorageId, FWorldStorageContainerRecord& OutStorage) const
{
    const FWorldStorageContainerRecord* Found = Storages.Find(StorageId);
    if (!Found)
        return false;
    OutStorage = *Found;
    return true;
}

bool UWorldStorageSubsystem::IsStorageAvailable(const FGuid& StorageId) const
{
    const FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    return Storage && Storage->bAvailable;
}

bool UWorldStorageSubsystem::SetStorageAvailable(const FGuid& StorageId, bool bAvailable)
{
    FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    if (!Storage)
        return false;

    if (Storage->bAvailable != bAvailable)
    {
        if (Storage->Revision == MAX_int64)
            return false;
        Storage->bAvailable = bAvailable;
        ++Storage->Revision;
    }

    if (bAvailable)
        LiveStorageIds.Add(StorageId);
    else
        LiveStorageIds.Remove(StorageId);
    return true;
}

bool UWorldStorageSubsystem::CanAcceptItem(const FGuid& StorageId, const FGuid& ItemId) const
{
    const FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    if (!Storage || !Storage->bAvailable || !Ownership || !StorageId.IsValid() || !ItemId.IsValid())
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(ItemId, Item) ||
        (Item.OwnerKind == EWorldItemOwnerKind::Storage && Item.OwnerId == StorageId))
        return false;

    int64 StoredQuantity = 0;
    TArray<FWorldOwnedItemRecord> AllItems;
    Ownership->ExportSnapshot(AllItems);
    for (const FWorldOwnedItemRecord& Candidate : AllItems)
    {
        if (Candidate.OwnerKind == EWorldItemOwnerKind::Storage &&
            Candidate.OwnerId == StorageId)
            StoredQuantity += FMath::Max(0, Candidate.Quantity);
    }

    return StoredQuantity + CountReservationsForStorage(StorageId) + Item.Quantity
        <= Storage->CapacityUnits;
}

bool UWorldStorageSubsystem::TryReserveIncoming(
    const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId)
{
    if (!StorageId.IsValid() || !CargoId.IsValid() || !ItemId.IsValid())
        return false;

    FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    // Reserve and eventual commit/release each increment the container revision.
    // Leave room for the second transition so a reservation cannot become stranded.
    if (!Storage || !Storage->bAvailable || !Ownership || Storage->Revision >= MAX_int64 - 1)
        return false;

    if (const FWorldStorageIncomingReservation* Existing = IncomingReservations.Find(CargoId))
    {
        return Existing->DestinationStorageId == StorageId && Existing->ItemId == ItemId;
    }

    for (const TPair<FGuid, FWorldStorageIncomingReservation>& Pair : IncomingReservations)
    {
        if (Pair.Value.ItemId == ItemId)
            return false;
    }

    FWorldOwnedItemRecord Item;
    // Cargo may only reserve capacity for a unique item that is currently
    // stored at a different registered source; prevent inventory/world bypass.
    if (!Ownership->TryGetItem(ItemId, Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::Storage ||
        Item.OwnerId == StorageId)
        return false;

    int64 StoredQuantity = 0;
    TArray<FWorldOwnedItemRecord> AllItems;
    Ownership->ExportSnapshot(AllItems);
    for (const FWorldOwnedItemRecord& Candidate : AllItems)
    {
        if (Candidate.OwnerKind == EWorldItemOwnerKind::Storage &&
            Candidate.OwnerId == StorageId)
            StoredQuantity += FMath::Max(0, Candidate.Quantity);
    }
    if (StoredQuantity + CountReservationsForStorage(StorageId) + Item.Quantity >
        Storage->CapacityUnits)
        return false;

    FWorldStorageIncomingReservation Reservation;
    Reservation.CargoId = CargoId;
    Reservation.ItemId = ItemId;
    Reservation.DestinationStorageId = StorageId;
    Reservation.Quantity = Item.Quantity;
    IncomingReservations.Add(CargoId, Reservation);
    ++Storage->Revision;
    return true;
}

bool UWorldStorageSubsystem::HasIncomingReservation(
    const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId) const
{
    const FWorldStorageIncomingReservation* Reservation = IncomingReservations.Find(CargoId);
    return Reservation && Reservation->CargoId == CargoId &&
        Reservation->ItemId == ItemId &&
        Reservation->DestinationStorageId == StorageId;
}

bool UWorldStorageSubsystem::CanCommitIncomingReservation(
    const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId) const
{
    const FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    return Storage && Storage->bAvailable && Storage->Revision != MAX_int64 &&
        HasIncomingReservation(StorageId, CargoId, ItemId);
}

bool UWorldStorageSubsystem::CommitIncomingReservation(
    const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId)
{
    if (!CanCommitIncomingReservation(StorageId, CargoId, ItemId))
        return false;

    FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    IncomingReservations.Remove(CargoId);
    ++Storage->Revision;
    return true;
}

bool UWorldStorageSubsystem::ReleaseIncomingReservation(const FGuid& CargoId)
{
    const FWorldStorageIncomingReservation* Existing = IncomingReservations.Find(CargoId);
    if (!Existing)
        return false;

    FWorldStorageContainerRecord* Storage = Storages.Find(Existing->DestinationStorageId);
    if (!Storage || Storage->Revision == MAX_int64)
        return false;

    IncomingReservations.Remove(CargoId);
    ++Storage->Revision;
    return true;
}

bool UWorldStorageSubsystem::GetStoredItemIds(
    const FGuid& StorageId, TArray<FGuid>& OutItemIds) const
{
    OutItemIds.Reset();
    if (!Storages.Contains(StorageId))
        return false;

    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    if (!Ownership)
        return false;

    TArray<FWorldOwnedItemRecord> AllItems;
    Ownership->ExportSnapshot(AllItems);
    for (const FWorldOwnedItemRecord& Item : AllItems)
    {
        if (Item.OwnerKind == EWorldItemOwnerKind::Storage && Item.OwnerId == StorageId)
            OutItemIds.Add(Item.ItemId);
    }
    OutItemIds.Sort([](const FGuid& A, const FGuid& B)
        { return A.ToString() < B.ToString(); });
    return true;
}

void UWorldStorageSubsystem::ExportSnapshot(
    TArray<FWorldStorageContainerRecord>& OutStorages,
    TArray<FWorldStorageIncomingReservation>& OutReservations) const
{
    Storages.GenerateValueArray(OutStorages);
    OutStorages.Sort([](const FWorldStorageContainerRecord& A, const FWorldStorageContainerRecord& B)
        { return A.StorageId.ToString() < B.StorageId.ToString(); });

    IncomingReservations.GenerateValueArray(OutReservations);
    OutReservations.Sort([](const FWorldStorageIncomingReservation& A,
        const FWorldStorageIncomingReservation& B)
        { return A.CargoId.ToString() < B.CargoId.ToString(); });
}

bool UWorldStorageSubsystem::RestoreSnapshot(
    const TArray<FWorldStorageContainerRecord>& StorageSnapshot,
    const TArray<FWorldStorageIncomingReservation>& ReservationSnapshot)
{
    if (MaxStorages <= 0 || StorageSnapshot.Num() > MaxStorages)
        return false;

    TMap<FGuid, FWorldStorageContainerRecord> ProposedStorages;
    for (const FWorldStorageContainerRecord& Storage : StorageSnapshot)
    {
        if (!Storage.StorageId.IsValid() || Storage.CapacityUnits <= 0 ||
            !FMath::IsFinite(Storage.SpoilageRateMultiplier) ||
            Storage.SpoilageRateMultiplier < 0.0f || Storage.SpoilageRateMultiplier > 4.0f ||
            Storage.Revision <= 0 || ProposedStorages.Contains(Storage.StorageId))
            return false;
        FWorldStorageContainerRecord RestoredStorage = Storage;
        // Current authored container configuration wins over a stale capacity
        // saved by an older build. Only materialized actors can accept cargo.
        if (LiveStorageIds.Contains(Storage.StorageId))
        {
            if (const FWorldStorageContainerRecord* Current = Storages.Find(Storage.StorageId))
                RestoredStorage.CapacityUnits = Current->CapacityUnits;
        }
        RestoredStorage.bAvailable = LiveStorageIds.Contains(Storage.StorageId);
        ProposedStorages.Add(Storage.StorageId, RestoredStorage);
    }

    // Keep newly authored live containers that did not exist in an older save.
    for (const FGuid& LiveId : LiveStorageIds)
    {
        if (ProposedStorages.Contains(LiveId))
            continue;
        if (const FWorldStorageContainerRecord* Current = Storages.Find(LiveId))
        {
            ProposedStorages.Add(LiveId, *Current);
            if (ProposedStorages.Num() > MaxStorages)
                return false;
        }
    }

    TMap<FGuid, FWorldStorageIncomingReservation> ProposedReservations;
    TSet<FGuid> ReservedItems;
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    if (!Ownership && !ReservationSnapshot.IsEmpty())
        return false;

    for (const FWorldStorageIncomingReservation& Reservation : ReservationSnapshot)
    {
        if (!Reservation.CargoId.IsValid() || !Reservation.ItemId.IsValid() ||
            Reservation.Quantity <= 0 ||
            !ProposedStorages.Contains(Reservation.DestinationStorageId) ||
            ProposedReservations.Contains(Reservation.CargoId) ||
            ReservedItems.Contains(Reservation.ItemId))
            return false;

        FWorldOwnedItemRecord Item;
        if (!Ownership->TryGetItem(Reservation.ItemId, Item) ||
            Item.OwnerKind != EWorldItemOwnerKind::InTransit ||
            Item.OwnerId != Reservation.CargoId || Item.Quantity != Reservation.Quantity)
            return false;

        ProposedReservations.Add(Reservation.CargoId, Reservation);
        ReservedItems.Add(Reservation.ItemId);
    }

    for (const TPair<FGuid, FWorldStorageContainerRecord>& Pair : ProposedStorages)
    {
        int64 StoredQuantity = 0;
        TArray<FWorldOwnedItemRecord> AllItems;
        if (Ownership)
            Ownership->ExportSnapshot(AllItems);
        for (const FWorldOwnedItemRecord& Item : AllItems)
        {
            if (Item.OwnerKind == EWorldItemOwnerKind::Storage && Item.OwnerId == Pair.Key)
                StoredQuantity += FMath::Max(0, Item.Quantity);
        }
        int64 ReservedQuantity = 0;
        for (const TPair<FGuid, FWorldStorageIncomingReservation>& Reservation : ProposedReservations)
            if (Reservation.Value.DestinationStorageId == Pair.Key)
                ReservedQuantity += FMath::Max(0, Reservation.Value.Quantity);
        if (StoredQuantity + ReservedQuantity > Pair.Value.CapacityUnits)
            return false;
    }

    Storages = MoveTemp(ProposedStorages);
    IncomingReservations = MoveTemp(ProposedReservations);
    return true;
}

bool UWorldStorageSubsystem::SetStorageSpoilageRate(const FGuid& StorageId, float Multiplier)
{
    FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    if (!Storage || !FMath::IsFinite(Multiplier) || Multiplier < 0.0f || Multiplier > 4.0f ||
        Storage->Revision == MAX_int64)
        return false;
    if (Storage->SpoilageRateMultiplier != Multiplier)
    {
        Storage->SpoilageRateMultiplier = Multiplier;
        ++Storage->Revision;
    }
    return true;
}
