#include "WorldCargoTransportSubsystem.h"

#include "Engine/World.h"
#include "WorldLogisticsRouteSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

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

bool UWorldCargoTransportSubsystem::StartTransport(
    const FGuid& ItemId, int64 ExpectedItemRevision, const FGuid& SourceStorageId,
    FName OriginNode, FName DestinationNode, const FGuid& DestinationStorageId,
    float CostPerSecond, FGuid& OutCargoId)
{
    UWorldLogisticsRouteSubsystem* Routes = GetRoutes();
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    if (!Routes || !Ownership || !ItemId.IsValid() || ExpectedItemRevision <= 0 ||
        !SourceStorageId.IsValid() || !DestinationStorageId.IsValid() ||
        OriginNode.IsNone() || DestinationNode.IsNone() ||
        OriginNode == DestinationNode || !FMath::IsFinite(CostPerSecond) || CostPerSecond <= 0.0f ||
        MaxActiveCargo <= 0 || Cargo.Num() >= MaxActiveCargo)
        return false;

    TArray<FName> Path;
    float RouteCost = 0.0f;
    if (!Routes->FindRoute(OriginNode, DestinationNode, Path, RouteCost) ||
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
    Record.RouteCost = RouteCost;
    Record.CostPerSecond = CostPerSecond;
    Record.DeliveryOperationId = FGuid::NewGuid();
    if (!Record.CargoId.IsValid() || !Record.DeliveryOperationId.IsValid() ||
        Cargo.Contains(Record.CargoId))
        return false;

    FWorldOwnedItemRecord UpdatedItem;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Storage, EWorldItemOwnerKind::InTransit, Record.CargoId,
        Item.WorldLocation, UpdatedItem))
        return false;

    Record.ItemRevision = UpdatedItem.Revision;
    Cargo.Add(Record.CargoId, Record);
    OutCargoId = Record.CargoId;
    return true;
}

bool UWorldCargoTransportSubsystem::TryDeliver(FWorldCargoTransportRecord& Record)
{
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership();
    if (!Ownership || !Record.ItemId.IsValid() || !Record.CargoId.IsValid() ||
        !Record.DestinationStorageId.IsValid() || !Record.DeliveryOperationId.IsValid())
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(Record.ItemId, Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::InTransit || Item.OwnerId != Record.CargoId)
        return false;

    FWorldOwnedItemRecord Delivered;
    if (!Ownership->TryTransferItem(Record.DeliveryOperationId, Record.ItemId,
        Item.Revision, EWorldItemOwnerKind::InTransit, EWorldItemOwnerKind::Storage,
        Record.DestinationStorageId, Item.WorldLocation, Delivered))
        return false;

    Record.ItemRevision = Delivered.Revision;
    Record.State = ECargoTransportState::Delivered;
    Record.BlockedReason = NAME_None;
    ++Record.Revision;
    return true;
}

bool UWorldCargoTransportSubsystem::CanAdvanceCargoStep(double DeltaSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0)
        return false;
    for (const TPair<FGuid, FWorldCargoTransportRecord>& Pair : Cargo)
    {
        const FWorldCargoTransportRecord& Record = Pair.Value;
        if (Record.State == ECargoTransportState::InTransit &&
            (Record.Revision == MAX_int64 ||
             !FMath::IsFinite(Record.Progress + Record.CostPerSecond * static_cast<float>(DeltaSeconds))))
            return false;
    }
    return true;
}

bool UWorldCargoTransportSubsystem::AdvanceCargoStep(double DeltaSeconds)
{
    if (!CanAdvanceCargoStep(DeltaSeconds)) return false;

    for (TPair<FGuid, FWorldCargoTransportRecord>& Pair : Cargo)
    {
        FWorldCargoTransportRecord& Record = Pair.Value;
        if (Record.State == ECargoTransportState::Delivered ||
            Record.State == ECargoTransportState::Cancelled)
            continue;
        if (Record.State == ECargoTransportState::Blocked)
            continue;

        Record.Progress = FMath::Min(Record.RouteCost,
            Record.Progress + Record.CostPerSecond * static_cast<float>(DeltaSeconds));
        ++Record.Revision;
        if (Record.Progress >= Record.RouteCost && !TryDeliver(Record))
        {
            Record.State = ECargoTransportState::Blocked;
            Record.BlockedReason = FName(TEXT("OwnershipOrDestinationUnavailable"));
        }
    }
    return true;
}

bool UWorldCargoTransportSubsystem::RetryBlockedDelivery(const FGuid& CargoId)
{
    FWorldCargoTransportRecord* Record = Cargo.Find(CargoId);
    if (!Record || Record->State != ECargoTransportState::Blocked ||
        Record->Revision == MAX_int64)
        return false;
    if (!TryDeliver(*Record))
        return false;
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
            !FMath::IsFinite(R.RouteCost) || R.RouteCost < 0.0f ||
            !FMath::IsFinite(R.Progress) || R.Progress < 0.0f || R.Progress > R.RouteCost ||
            !FMath::IsFinite(R.CostPerSecond) || R.CostPerSecond <= 0.0f ||
            R.ItemRevision <= 0 || R.Revision <= 0 || !R.DeliveryOperationId.IsValid() ||
            Proposed.Contains(R.CargoId)) return false;
        Proposed.Add(R.CargoId, R);
    }
    Cargo = MoveTemp(Proposed);
    return true;
}
