#include "WorldCargoTransportSubsystem.h"

#include "Engine/World.h"
#include "WorldLogisticsRouteSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldStorageSubsystem.h"

UWorldLogisticsRouteSubsystem* UWorldCargoTransportSubsystem::GetRoutes() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
}

UWorldItemOwnershipSubsystem* UWorldCargoTransportSubsystem::GetOwnership() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
}

UWorldStorageSubsystem* UWorldCargoTransportSubsystem::GetStorage() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
}

bool UWorldCargoTransportSubsystem::StartTransport(
    const FGuid& ItemId, int64 ExpectedItemRevision, const FGuid& SourceStorageId,
    FName OriginNode, FName DestinationNode, const FGuid& DestinationStorageId,
    float CostPerSecond, FGuid& OutCargoId)
{
    return StartTransportWithContext(ItemId, ExpectedItemRevision, SourceStorageId,
        OriginNode, DestinationNode, DestinationStorageId,
        FLogisticsRouteCostContext(), CostPerSecond, OutCargoId);
}

bool UWorldCargoTransportSubsystem::StartTransportWithContext(
    const FGuid& ItemId, int64 ExpectedItemRevision, const FGuid& SourceStorageId,
    FName OriginNode, FName DestinationNode, const FGuid& DestinationStorageId,
    const FLogisticsRouteCostContext& RouteContext, float CostPerSecond, FGuid& OutCargoId)
{
    OutCargoId.Invalidate();
    UWorldLogisticsRouteSubsystem* Routes = GetRoutes();
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    UWorldStorageSubsystem* Storage = GetStorage();
    if (!Routes || !Ownership || !Storage || !ItemId.IsValid() || ExpectedItemRevision <= 0 ||
        !SourceStorageId.IsValid() || !DestinationStorageId.IsValid() ||
        SourceStorageId == DestinationStorageId ||
        OriginNode.IsNone() || DestinationNode.IsNone() ||
        OriginNode == DestinationNode || !FMath::IsFinite(CostPerSecond) || CostPerSecond <= 0.0f ||
        MaxActiveCargo <= 0 || Cargo.Num() >= MaxActiveCargo)
        return false;

    FWorldStorageContainerRecord SourceStorage;
    FWorldStorageContainerRecord DestinationStorage;
    if (!Storage->GetStorage(SourceStorageId, SourceStorage) ||
        !Storage->GetStorage(DestinationStorageId, DestinationStorage) ||
        !Storage->IsStorageAvailable(SourceStorageId) ||
        !Storage->IsStorageAvailable(DestinationStorageId))
        return false;

    TArray<FName> Path;
    float RouteCost = 0.0f;
    if (!Routes->FindRouteWithContext(OriginNode, DestinationNode, RouteContext, Path, RouteCost) ||
        Path.Num() < 2 || !FMath::IsFinite(RouteCost) || RouteCost < 0.0f)
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(ItemId, Item) || Item.Revision != ExpectedItemRevision ||
        Item.OwnerKind != EWorldItemOwnerKind::Storage || Item.OwnerId != SourceStorageId)
        return false;

    FWorldCargoTransportRecord Record;
    Record.CargoId = FGuid::NewGuid();
    Record.ItemId = ItemId;
    Record.DestinationStorageId = DestinationStorageId;
    Record.OriginNode = OriginNode;
    Record.DestinationNode = DestinationNode;
    Record.Route = Path;
    Record.RouteCostContext = RouteContext;
    Record.RouteCost = RouteCost;
    Record.CostPerSecond = CostPerSecond;
    Record.DeliveryOperationId = FGuid::NewGuid();
    if (!Record.CargoId.IsValid() || !Record.DeliveryOperationId.IsValid() ||
        Cargo.Contains(Record.CargoId))
        return false;

    // Reserve destination capacity before changing item ownership. Manual
    // deposits count reservations, so multiple shipments cannot overbook it.
    if (!Storage->TryReserveIncoming(DestinationStorageId, Record.CargoId, ItemId))
        return false;

    FWorldOwnedItemRecord UpdatedItem;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Storage, EWorldItemOwnerKind::InTransit, Record.CargoId,
        Item.WorldLocation, UpdatedItem))
    {
        if (!Storage->ReleaseIncomingReservation(Record.CargoId))
            UE_LOG(LogTemp, Error, TEXT("Failed to release destination reservation for rejected cargo %s."),
                *Record.CargoId.ToString());
        return false;
    }

    Record.ItemRevision = UpdatedItem.Revision;
    Cargo.Add(Record.CargoId, Record);
    OutCargoId = Record.CargoId;
    return true;
}

bool UWorldCargoTransportSubsystem::TryDeliver(FWorldCargoTransportRecord& Record)
{
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    UWorldStorageSubsystem* Storage = GetStorage();
    if (!Ownership || !Storage || Record.Revision >= MAX_int64 - 1 ||
        !Record.ItemId.IsValid() || !Record.CargoId.IsValid() ||
        !Record.DestinationStorageId.IsValid() || !Record.DeliveryOperationId.IsValid() ||
        !Storage->CanCommitIncomingReservation(
            Record.DestinationStorageId, Record.CargoId, Record.ItemId))
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(Record.ItemId, Item) ||
        Item.Revision != Record.ItemRevision ||
        Item.OwnerKind != EWorldItemOwnerKind::InTransit || Item.OwnerId != Record.CargoId)
        return false;

    FWorldOwnedItemRecord Delivered;
    if (!Ownership->TryTransferItem(Record.DeliveryOperationId, Record.ItemId,
        Item.Revision, EWorldItemOwnerKind::InTransit, EWorldItemOwnerKind::Storage,
        Record.DestinationStorageId, Item.WorldLocation, Delivered))
        return false;

    if (!Storage->CommitIncomingReservation(
        Record.DestinationStorageId, Record.CargoId, Record.ItemId))
    {
        FWorldOwnedItemRecord RolledBack;
        if (Ownership->TryTransferItem(FGuid::NewGuid(), Record.ItemId, Delivered.Revision,
            EWorldItemOwnerKind::Storage, EWorldItemOwnerKind::InTransit,
            Record.CargoId, Item.WorldLocation, RolledBack))
        {
            Record.ItemRevision = RolledBack.Revision;
            // The previous delivery operation has an immutable success receipt.
            // Never reuse that id after compensation: the next attempt is new work.
            Record.DeliveryOperationId = FGuid::NewGuid();
            if (!Record.DeliveryOperationId.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Could not issue a fresh delivery operation ID for cargo %s."),
                    *Record.CargoId.ToString());
                return false;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Cargo delivery rollback failed; item %s must be reconciled."),
                *Record.ItemId.ToString());
        }
        return false;
    }

    Record.ItemRevision = Delivered.Revision;
    Record.State = ECargoTransportState::Delivered;
    Record.BlockedReason = NAME_None;
    ++Record.Revision;
    return true;
}

bool UWorldCargoTransportSubsystem::CanAdvanceCargoStep(double DeltaSeconds) const
{
    UWorldStorageSubsystem* Storage = GetStorage();
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    if (!Storage || !Ownership || !FMath::IsFinite(DeltaSeconds) ||
        DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0)
        return false;

    for (const TPair<FGuid, FWorldCargoTransportRecord>& Pair : Cargo)
    {
        const FWorldCargoTransportRecord& Record = Pair.Value;
        if (Record.State != ECargoTransportState::InTransit)
            continue;

        if (Record.Revision >= MAX_int64 - 1 ||
            !FMath::IsFinite(Record.RouteCost) || Record.RouteCost < 0.0f ||
            !FMath::IsFinite(Record.Progress) || Record.Progress < 0.0f ||
            Record.Progress > Record.RouteCost ||
            !FMath::IsFinite(Record.CostPerSecond) || Record.CostPerSecond <= 0.0f ||
            !Storage->HasIncomingReservation(
                Record.DestinationStorageId, Record.CargoId, Record.ItemId))
            return false;

        FWorldOwnedItemRecord Item;
        if (!Ownership->TryGetItem(Record.ItemId, Item) ||
            Item.Revision != Record.ItemRevision ||
            Item.OwnerKind != EWorldItemOwnerKind::InTransit || Item.OwnerId != Record.CargoId)
            return false;

        const float ProposedProgress = Record.Progress +
            Record.CostPerSecond * static_cast<float>(DeltaSeconds);
        if (!FMath::IsFinite(ProposedProgress))
            return false;

        const bool bWillArrive = FMath::Min(Record.RouteCost, ProposedProgress) >= Record.RouteCost;
        if (!bWillArrive && Record.Revision >= MAX_int64 - 2)
        {
            // A non-final progress update must not consume the last revision
            // and strand cargo permanently one step short of delivery.
            return false;
        }
    }
    return true;
}

bool UWorldCargoTransportSubsystem::AdvanceCargoStep(double DeltaSeconds)
{
    if (!CanAdvanceCargoStep(DeltaSeconds))
        return false;

    UWorldLogisticsRouteSubsystem* Routes = GetRoutes();
    for (TPair<FGuid, FWorldCargoTransportRecord>& Pair : Cargo)
    {
        FWorldCargoTransportRecord& Record = Pair.Value;
        if (Record.State != ECargoTransportState::InTransit)
            continue;

        // A road/bridge/rope can disappear or a weather/surface cost can change
        // after departure. Keep the item in transit ownership and block the
        // shipment rather than silently following a stale route or losing cargo.
        float LiveRouteCost = 0.0f;
        if (!Routes || !Routes->CalculatePathCost(
            Record.Route, Record.RouteCostContext, LiveRouteCost) ||
            !FMath::IsNearlyEqual(LiveRouteCost, Record.RouteCost, 0.01f))
        {
            ++Record.Revision;
            Record.State = ECargoTransportState::Blocked;
            Record.BlockedReason = FName(TEXT("RouteInvalidated"));
            continue;
        }

        const float ProposedProgress = Record.Progress +
            Record.CostPerSecond * static_cast<float>(DeltaSeconds);
        Record.Progress = FMath::Min(Record.RouteCost, ProposedProgress);

        if (Record.Progress >= Record.RouteCost)
        {
            // TryDeliver owns the single revision increment for successful
            // arrival. A rejected delivery becomes an explicit blocked result.
            if (!TryDeliver(Record))
            {
                ++Record.Revision;
                Record.State = ECargoTransportState::Blocked;
                Record.BlockedReason = FName(TEXT("OwnershipOrDestinationUnavailable"));
            }
        }
        else
        {
            ++Record.Revision;
        }
    }
    return true;
}

bool UWorldCargoTransportSubsystem::RetryBlockedDelivery(const FGuid& CargoId)
{
    FWorldCargoTransportRecord* Record = Cargo.Find(CargoId);
    UWorldStorageSubsystem* Storage = GetStorage();
    if (!Record || !Storage || Record->State != ECargoTransportState::Blocked ||
        Record->BlockedReason != FName(TEXT("OwnershipOrDestinationUnavailable")) ||
        Record->Progress < Record->RouteCost || Record->Revision >= MAX_int64 - 1 ||
        !Storage->HasIncomingReservation(Record->DestinationStorageId, Record->CargoId, Record->ItemId))
        return false;
    if (!TryDeliver(*Record))
        return false;
    return true;
}

bool UWorldCargoTransportSubsystem::ReplanBlockedCargo(
    const FGuid& CargoId, const FLogisticsRouteCostContext& RouteContext)
{
    FWorldCargoTransportRecord* Record = Cargo.Find(CargoId);
    UWorldLogisticsRouteSubsystem* Routes = GetRoutes();
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    UWorldStorageSubsystem* Storage = GetStorage();
    if (!Record || !Routes || !Ownership || !Storage ||
        Record->State != ECargoTransportState::Blocked ||
        Record->BlockedReason != FName(TEXT("RouteInvalidated")) ||
        Record->Revision >= MAX_int64 - 1 ||
        !Storage->HasIncomingReservation(Record->DestinationStorageId, Record->CargoId, Record->ItemId))
    {
        return false;
    }

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(Record->ItemId, Item) ||
        Item.Revision != Record->ItemRevision ||
        Item.OwnerKind != EWorldItemOwnerKind::InTransit || Item.OwnerId != Record->CargoId)
        return false;

    TArray<FName> NewRoute;
    float NewRouteCost = 0.0f;
    if (!Routes->FindRouteWithContext(
        Record->OriginNode, Record->DestinationNode, RouteContext, NewRoute, NewRouteCost) ||
        NewRoute.Num() < 2 || !FMath::IsFinite(NewRouteCost) || NewRouteCost < 0.0f)
    {
        return false;
    }

    // The route is a new plan; old progress is not transferable to a different
    // path. The item identity and destination reservation remain unchanged.
    Record->Route = MoveTemp(NewRoute);
    Record->RouteCostContext = RouteContext;
    Record->RouteCost = NewRouteCost;
    Record->Progress = 0.0f;
    Record->State = ECargoTransportState::InTransit;
    Record->BlockedReason = NAME_None;
    ++Record->Revision;
    return true;
}

bool UWorldCargoTransportSubsystem::GetCargo(
    const FGuid& CargoId, FWorldCargoTransportRecord& OutCargo) const
{
    const FWorldCargoTransportRecord* Found = Cargo.Find(CargoId);
    if (!Found) return false;
    OutCargo = *Found;
    return true;
}

void UWorldCargoTransportSubsystem::ExportSnapshot(TArray<FWorldCargoTransportRecord>& OutCargo) const
{
    Cargo.GenerateValueArray(OutCargo);
    OutCargo.Sort([](const FWorldCargoTransportRecord& A, const FWorldCargoTransportRecord& B)
        { return A.CargoId.ToString() < B.CargoId.ToString(); });
}

bool UWorldCargoTransportSubsystem::RestoreSnapshot(const TArray<FWorldCargoTransportRecord>& Snapshot)
{
    if (Snapshot.Num() > MaxActiveCargo || MaxActiveCargo <= 0) return false;
    TMap<FGuid, FWorldCargoTransportRecord> Proposed;
    for (const FWorldCargoTransportRecord& R : Snapshot)
    {
        if (!R.CargoId.IsValid() || !R.ItemId.IsValid() || !R.DestinationStorageId.IsValid() ||
            R.OriginNode.IsNone() || R.DestinationNode.IsNone() || R.Route.Num() < 2 ||
            R.Route[0] != R.OriginNode || R.Route.Last() != R.DestinationNode ||
            !FMath::IsFinite(R.RouteCost) || R.RouteCost < 0.0f ||
            !FMath::IsFinite(R.Progress) || R.Progress < 0.0f || R.Progress > R.RouteCost ||
            !FMath::IsFinite(R.CostPerSecond) || R.CostPerSecond <= 0.0f ||
            R.ItemRevision <= 0 || R.Revision <= 0 || !R.DeliveryOperationId.IsValid() ||
            static_cast<uint8>(R.State) > static_cast<uint8>(ECargoTransportState::Cancelled) ||
            Proposed.Contains(R.CargoId)) return false;
        Proposed.Add(R.CargoId, R);
    }

    // Cargo snapshots are only valid when their item-ledger ownership agrees.
    // This rejects orphaned cargo, duplicated ownership and stale delivery records.
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    UWorldStorageSubsystem* Storage = GetStorage();
    if (!Ownership || !Storage) return false;

    // The relationship is bidirectional: each active cargo needs one inbound
    // reservation, and each reservation must refer to exactly one active cargo
    // record. Reject orphan reservations instead of silently consuming capacity.
    TArray<FWorldStorageContainerRecord> StorageRecords;
    TArray<FWorldStorageIncomingReservation> Reservations;
    Storage->ExportSnapshot(StorageRecords, Reservations);
    for (const FWorldStorageIncomingReservation& Reservation : Reservations)
    {
        const FWorldCargoTransportRecord* RelatedCargo = Proposed.Find(Reservation.CargoId);
        if (!RelatedCargo ||
            (RelatedCargo->State != ECargoTransportState::InTransit &&
             RelatedCargo->State != ECargoTransportState::Blocked) ||
            RelatedCargo->ItemId != Reservation.ItemId ||
            RelatedCargo->DestinationStorageId != Reservation.DestinationStorageId)
            return false;
    }

    for (const TPair<FGuid, FWorldCargoTransportRecord>& Pair : Proposed)
    {
        const FWorldCargoTransportRecord& R = Pair.Value;
        FWorldOwnedItemRecord Item;
        if (!Ownership->TryGetItem(R.ItemId, Item))
            return false;

        if (R.State == ECargoTransportState::InTransit || R.State == ECargoTransportState::Blocked)
        {
            // Active cargo must still be the sole owner of the in-transit item
            // and hold one reserved slot at its destination.
            if (Item.Revision != R.ItemRevision ||
                Item.OwnerKind != EWorldItemOwnerKind::InTransit || Item.OwnerId != R.CargoId ||
                !Storage->HasIncomingReservation(R.DestinationStorageId, R.CargoId, R.ItemId))
                return false;
        }
        else if (R.State == ECargoTransportState::Delivered)
        {
            // Delivered cargo is historical: the item may since have been stored,
            // crafted, consumed or sent on a later shipment.
            if (R.Progress < R.RouteCost || Item.Revision < R.ItemRevision ||
                Storage->HasIncomingReservation(R.DestinationStorageId, R.CargoId, R.ItemId))
                return false;
        }
        else if (R.State == ECargoTransportState::Cancelled)
        {
            // A cancelled shipment must not still claim the item is in transit
            // or retain an inbound capacity reservation.
            if ((Item.OwnerKind == EWorldItemOwnerKind::InTransit && Item.OwnerId == R.CargoId) ||
                Storage->HasIncomingReservation(R.DestinationStorageId, R.CargoId, R.ItemId))
                return false;
        }
    }

    Cargo = MoveTemp(Proposed);
    return true;
}
