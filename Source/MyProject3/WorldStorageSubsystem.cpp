#include "WorldStorageSubsystem.h"

#include "Engine/World.h"
#include "WorldItemOwnershipSubsystem.h"

UWorldItemOwnershipSubsystem* UWorldStorageSubsystem::GetOwnership() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
}

int32 UWorldStorageSubsystem::CountReservationsForStorage(const FGuid& StorageId) const
{
    int32 Count = 0;
    for (const TPair<FGuid, FWorldStorageIncomingReservation>& Pair : IncomingReservations)
    {
        if (Pair.Value.DestinationStorageId == StorageId)
            ++Count;
    }
    return Count;
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
    // Only a unique item presently held by a different source storage may
    // reserve destination capacity for cargo. Inventory/world items must use
    // their own explicit transfer path instead of bypassing storage ownership.
    if (!Ownership->TryGetItem(ItemId, Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::Storage ||
        Item.OwnerId == StorageId)
        return false;

    int32 StoredCount = 0;
    TArray<FWorldOwnedItemRecord> AllItems;
    Ownership->ExportSnapshot(AllItems);
    for (const FWorldOwnedItemRecord& Candidate : AllItems)
    {
        if (Candidate.OwnerKind == EWorldItemOwnerKind::Storage &&
            Candidate.OwnerId == StorageId)
            ++StoredCount;
    }

    return StoredCount + CountReservationsForStorage(StorageId) < Storage->CapacityUnits;
}

bool UWorldStorageSubsystem::TryReserveIncoming(
    const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId)
{
    if (!StorageId.IsValid() || !CargoId.IsValid() || !ItemId.IsValid())
        return false;

    FWorldStorageContainerRecord* Storage = Storages.Find(StorageId);
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    if (!Storage || !Storage->bAvailable || !Ownership || Storage->Revision == MAX_int64)
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
    if (!Ownership->TryGetItem(ItemId, Item) ||
        (Item.OwnerKind == EWorldItemOwnerKind::Storage && Item.OwnerId == StorageId))
        return false;

    int32 StoredCount = 0;
    TArray<FWorldOwnedItemRecord> AllItems;
    Ownership->ExportSnapshot(AllItems);
    for (const FWorldOwnedItemRecord& Candidate : AllItems)
    {
        if (Candidate.OwnerKind == EWorldItemOwnerKind::Storage &&
            Candidate.OwnerId == StorageId)
            ++StoredCount;
    }
    if (StoredCount + CountReservationsForStorage(StorageId) >= Storage->CapacityUnits)
        return false;

    FWorldStorageIncomingReservation Reservation;
    Reservation.CargoId = CargoId;
    Reservation.ItemId = ItemId;
    Reservation.DestinationStorageId = StorageId;
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
            !ProposedStorages.Contains(Reservation.DestinationStorageId) ||
            ProposedReservations.Contains(Reservation.CargoId) ||
            ReservedItems.Contains(Reservation.ItemId))
            return false;

        FWorldOwnedItemRecord Item;
        if (!Ownership->TryGetItem(Reservation.ItemId, Item) ||
            Item.OwnerKind != EWorldItemOwnerKind::InTransit ||
            Item.OwnerId != Reservation.CargoId)
            return false;

        ProposedReservations.Add(Reservation.CargoId, Reservation);
        ReservedItems.Add(Reservation.ItemId);
    }

    for (const TPair<FGuid, FWorldStorageContainerRecord>& Pair : ProposedStorages)
    {
        int32 StoredCount = 0;
        TArray<FWorldOwnedItemRecord> AllItems;
        if (Ownership)
            Ownership->ExportSnapshot(AllItems);
        for (const FWorldOwnedItemRecord& Item : AllItems)
        {
            if (Item.OwnerKind == EWorldItemOwnerKind::Storage && Item.OwnerId == Pair.Key)
                ++StoredCount;
        }
        int32 ReservedCount = 0;
        for (const TPair<FGuid, FWorldStorageIncomingReservation>& Reservation : ProposedReservations)
            if (Reservation.Value.DestinationStorageId == Pair.Key)
                ++ReservedCount;
        if (StoredCount + ReservedCount > Pair.Value.CapacityUnits)
            return false;
    }

    Storages = MoveTemp(ProposedStorages);
    IncomingReservations = MoveTemp(ProposedReservations);
    return true;
}
