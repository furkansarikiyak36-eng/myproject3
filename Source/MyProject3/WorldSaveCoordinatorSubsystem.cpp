#include "WorldSaveCoordinatorSubsystem.h"
#include "LogContainerActor.h"
#include "LogCarryComponent.h"
#include "LogBuildPiece.h"
#include "StoneBuildingPiece.h"
#include "StickPrimitivePiece.h"
#include "Misc/GuardValue.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "WorldSimulationSaveGame.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldOperationLedgerSubsystem.h"
#include "WorldTerrainStateSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldCargoTransportSubsystem.h"
#include "WorldStorageSubsystem.h"
#include "InventoryCraftingComponent.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "EngineUtils.h"
#include "Logging/LogMacros.h"

namespace
{
bool ValidateProductionReceiptItemReferences(
    const TArray<FWorldOwnedItemRecord>& Items,
    const TArray<FWorldProductionStation>& Stations)
{
    TMap<FGuid, FWorldOwnedItemRecord> ItemsById;
    ItemsById.Reserve(Items.Num());
    for (const FWorldOwnedItemRecord& Item : Items)
    {
        if (!Item.ItemId.IsValid() || ItemsById.Contains(Item.ItemId))
            return false;
        ItemsById.Add(Item.ItemId, Item);
    }

    for (const FWorldProductionStation& Station : Stations)
    {
        for (const FWorldProductionPhysicalIntakeReceipt& Receipt : Station.PhysicalIntakeReceipts)
        {
            const FWorldOwnedItemRecord* Source = ItemsById.Find(Receipt.SourceItemId);
            if (!Source || Source->OwnerKind != EWorldItemOwnerKind::Transformed ||
                Source->DefinitionId != Receipt.SourceDefinitionId ||
                Source->MeshVariantSetId != Receipt.SourceMeshVariantSetId ||
                Source->Revision < Receipt.ResultingItemRevision)
            {
                return false;
            }
        }

        for (const FWorldProductionMaterializationReceipt& Receipt : Station.MaterializationReceipts)
        {
            // A materialized output may later be held, stored, split or consumed,
            // but its unique identity must remain in the ownership ledger history.
            if (!ItemsById.Contains(Receipt.WorldItemId))
                return false;
        }
    }
    return true;
}

bool CapturePhysicalActorSnapshots(UWorld* World, TArray<FWorldPhysicalActorSnapshot>& OutSnapshots)
{
    if (!World)
        return false;

    OutSnapshots.Reset();
    TSet<FGuid> SeenIdentityIds;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        UWorldObjectIdentityComponent* Identity =
            IsValid(Actor) ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        if (!IsValid(Identity) || !Identity->InstanceId.IsValid())
            continue;

        if (SeenIdentityIds.Contains(Identity->InstanceId))
        {
            UE_LOG(LogTemp, Warning, TEXT("Physical actor snapshot refused duplicate identity %s."),
                *Identity->InstanceId.ToString());
            return false;
        }
        SeenIdentityIds.Add(Identity->InstanceId);

        FWorldPhysicalActorSnapshot& ActorState = OutSnapshots.AddDefaulted_GetRef();
        ActorState.IdentityId = Identity->InstanceId;
        ActorState.ActorClass = FSoftClassPath(Actor->GetClass());
        ActorState.ActorTransform = Actor->GetActorTransform();
        ActorState.bActorHiddenInGame = Actor->IsHidden();
        ActorState.bActorCollisionEnabled = Actor->GetActorEnableCollision();
        ActorState.bActorTickEnabled = Actor->IsActorTickEnabled();
        ActorState.DefinitionId = Identity->DefinitionId;
        ActorState.LifecycleState = Identity->LifecycleState;
        ActorState.StateRevision = Identity->StateRevision;
        {
            FMemoryWriter Writer(ActorState.ActorSaveGameData, true);
            FObjectAndNameAsStringProxyArchive ActorArchive(Writer, true);
            ActorArchive.ArIsSaveGame = true;
            Actor->Serialize(ActorArchive);
        }

        TArray<UPrimitiveComponent*> Primitives;
        Actor->GetComponents<UPrimitiveComponent>(Primitives);
        for (UPrimitiveComponent* Primitive : Primitives)
        {
            if (!IsValid(Primitive))
                return false;

            FWorldPhysicalComponentSnapshot& ComponentState = ActorState.Components.AddDefaulted_GetRef();
            ComponentState.ComponentName = Primitive->GetFName();
            ComponentState.ActorRelativeTransform = Primitive == Actor->GetRootComponent()
                ? FTransform::Identity
                : Primitive->GetRelativeTransform();
            ComponentState.ComponentClass = FSoftClassPath(Primitive->GetClass());
            ComponentState.bIsPrimitive = true;
            ComponentState.bSimulatePhysics = Primitive->IsSimulatingPhysics();
            ComponentState.CollisionEnabled = Primitive->GetCollisionEnabled();
            ComponentState.LinearVelocity = Primitive->GetPhysicsLinearVelocity();
            ComponentState.AngularVelocityRadians = Primitive->GetPhysicsAngularVelocityInRadians();

            if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Primitive))
            {
                if (UStaticMesh* MeshAsset = StaticMesh->GetStaticMesh())
                    ComponentState.StaticMeshAsset = FSoftObjectPath(MeshAsset);
            }
        }
    }
    return true;
}

bool ReconcilePhysicalActorSnapshotsWithItems(
    const TArray<FWorldPhysicalActorSnapshot>& PhysicalActors,
    TArray<FWorldOwnedItemRecord>& Items,
    const TArray<FWorldStorageContainerRecord>& Storages,
    const TArray<FWorldCarryComponentSnapshot>& CarrySnapshots)
{
    TMap<FGuid, int32> ItemIndices;
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        const FWorldOwnedItemRecord& Item = Items[Index];
        if (!Item.ItemId.IsValid() || ItemIndices.Contains(Item.ItemId))
            return false;
        ItemIndices.Add(Item.ItemId, Index);
    }

    for (const FWorldPhysicalActorSnapshot& ActorState : PhysicalActors)
    {
        const int32* ItemIndex = ItemIndices.Find(ActorState.IdentityId);
        if (!ItemIndex)
            continue; // Non-item world actors, such as harvestable trees.

        FWorldOwnedItemRecord& Item = Items[*ItemIndex];
        const bool bWorldOwned = Item.OwnerKind == EWorldItemOwnerKind::World;
        const bool bStorageOwned = Item.OwnerKind == EWorldItemOwnerKind::Storage &&
            Item.OwnerId.IsValid() &&
            Storages.ContainsByPredicate([&Item](const FWorldStorageContainerRecord& Storage)
            {
                return Storage.StorageId == Item.OwnerId;
            });
        const bool bHeldOwned = Item.OwnerKind == EWorldItemOwnerKind::Held &&
            Item.OwnerId.IsValid() &&
            ActorState.LifecycleState == EWorldObjectLifecycleState::Held &&
            CarrySnapshots.ContainsByPredicate([&Item](const FWorldCarryComponentSnapshot& Carry)
            {
                return Carry.HeldItemId == Item.ItemId && Carry.CarryOwnerId == Item.OwnerId;
            });
        if ((!bWorldOwned && !bStorageOwned && !bHeldOwned) || Item.Quantity != 1 ||
            Item.DefinitionId != ActorState.DefinitionId)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Physical actor %s conflicts with item ownership/definition state."),
                *ActorState.IdentityId.ToString());
            return false;
        }
        // Save the authoritative item record at the same world location as its
        // physics actor; otherwise a load would restore a stale ledger position.
        Item.WorldLocation = ActorState.ActorTransform.GetLocation();
    }
    return true;
}

bool CaptureCarryComponentSnapshots(
    UWorld* World,
    TArray<FWorldCarryComponentSnapshot>& OutSnapshots)
{
    if (!World)
        return false;

    OutSnapshots.Reset();
    TSet<FGuid> SeenCarryIds;
    TSet<FGuid> SeenItemIds;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* OwnerActor = *It;
        TArray<ULogCarryComponent*> CarryComponents;
        if (!IsValid(OwnerActor))
            continue;
        OwnerActor->GetComponents<ULogCarryComponent>(CarryComponents);
        for (ULogCarryComponent* Carry : CarryComponents)
        {
            if (!IsValid(Carry))
                return false;
            AActor* HeldItem = nullptr;
            bool bIsMaterial = false;
            if (!Carry->GetHeldItemForSnapshot(HeldItem, bIsMaterial))
                return false;
            if (!HeldItem)
                continue;

            UWorldObjectIdentityComponent* Identity =
                HeldItem->FindComponentByClass<UWorldObjectIdentityComponent>();
            const FGuid CarryOwnerId = Carry->GetCarryOwnerIdForSnapshot();
            if (!Identity || !Identity->HasValidIdentity() ||
                Identity->GetLifecycleState() != EWorldObjectLifecycleState::Held ||
                !CarryOwnerId.IsValid() ||
                SeenCarryIds.Contains(CarryOwnerId) ||
                SeenItemIds.Contains(Identity->GetInstanceId()))
                return false;

            FWorldCarryComponentSnapshot& State = OutSnapshots.AddDefaulted_GetRef();
            State.OwnerActorPath = FSoftObjectPath(OwnerActor);
            State.CarryComponentName = Carry->GetFName();
            State.CarryOwnerId = CarryOwnerId;
            State.HeldItemId = Identity->GetInstanceId();
            State.HeldItemRelativeTransform =
                HeldItem->GetActorTransform().GetRelativeTransform(OwnerActor->GetActorTransform());
            State.bHeldMaterialPiece = bIsMaterial;
            const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
            State.bPlayerControlledOwner = OwnerPawn && OwnerPawn->IsPlayerControlled();
            if (State.OwnerActorPath.IsNull() || State.CarryComponentName.IsNone() ||
                !State.HeldItemRelativeTransform.IsValid())
                return false;

            SeenCarryIds.Add(CarryOwnerId);
            SeenItemIds.Add(Identity->GetInstanceId());
        }
    }
    OutSnapshots.Sort([](const FWorldCarryComponentSnapshot& A,
        const FWorldCarryComponentSnapshot& B)
    {
        return A.HeldItemId.ToString() < B.HeldItemId.ToString();
    });
    return true;
}

bool ValidateCarryComponentSnapshots(
    const TArray<FWorldOwnedItemRecord>& Items,
    const TArray<FWorldPhysicalActorSnapshot>& PhysicalActors,
    const TArray<FWorldCarryComponentSnapshot>& CarrySnapshots)
{
    TMap<FGuid, const FWorldOwnedItemRecord*> ItemsById;
    for (const FWorldOwnedItemRecord& Item : Items)
    {
        if (!Item.ItemId.IsValid() || ItemsById.Contains(Item.ItemId))
            return false;
        ItemsById.Add(Item.ItemId, &Item);
    }

    TMap<FGuid, const FWorldPhysicalActorSnapshot*> ActorsById;
    for (const FWorldPhysicalActorSnapshot& Actor : PhysicalActors)
    {
        if (!Actor.IdentityId.IsValid() || ActorsById.Contains(Actor.IdentityId))
            return false;
        ActorsById.Add(Actor.IdentityId, &Actor);
    }

    TSet<FGuid> CarryIds;
    TSet<FGuid> HeldItemIds;
    for (const FWorldCarryComponentSnapshot& Carry : CarrySnapshots)
    {
        if (!Carry.CarryOwnerId.IsValid() || !Carry.HeldItemId.IsValid() ||
            Carry.OwnerActorPath.IsNull() || Carry.CarryComponentName.IsNone() ||
            !Carry.HeldItemRelativeTransform.IsValid() ||
            CarryIds.Contains(Carry.CarryOwnerId) || HeldItemIds.Contains(Carry.HeldItemId))
            return false;

        const FWorldOwnedItemRecord* Item = ItemsById.FindRef(Carry.HeldItemId);
        const FWorldPhysicalActorSnapshot* Actor = ActorsById.FindRef(Carry.HeldItemId);
        if (!Item || !Actor || Item->OwnerKind != EWorldItemOwnerKind::Held ||
            Item->OwnerId != Carry.CarryOwnerId || Item->Quantity != 1 ||
            Actor->LifecycleState != EWorldObjectLifecycleState::Held)
            return false;

        CarryIds.Add(Carry.CarryOwnerId);
        HeldItemIds.Add(Carry.HeldItemId);
    }

    for (const FWorldOwnedItemRecord& Item : Items)
    {
        if (Item.OwnerKind == EWorldItemOwnerKind::Held && !HeldItemIds.Contains(Item.ItemId))
            return false;
    }
    return true;
}

void ClearCarryComponentsForSnapshotRestore(UWorld* World)
{
    if (!World)
        return;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<ULogCarryComponent*> CarryComponents;
        if (IsValid(*It))
            It->GetComponents<ULogCarryComponent>(CarryComponents);
        for (ULogCarryComponent* Carry : CarryComponents)
            if (IsValid(Carry))
                Carry->ClearHeldStateForSnapshotRestore();
    }
}

bool RestoreCarryComponentSnapshots(
    UWorld* World,
    const TArray<FWorldCarryComponentSnapshot>& CarrySnapshots)
{
    if (!World)
        return false;

    TSet<ULogCarryComponent*> RestoredComponents;
    TSet<FGuid> RestoredItemIds;
    for (const FWorldCarryComponentSnapshot& State : CarrySnapshots)
    {
        AActor* OwnerActor = Cast<AActor>(State.OwnerActorPath.ResolveObject());
        if (!IsValid(OwnerActor) && State.bPlayerControlledOwner)
            OwnerActor = UGameplayStatics::GetPlayerPawn(World, 0);
        if (!IsValid(OwnerActor))
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (IsValid(*It) && It->GetPathName() == State.OwnerActorPath.ToString())
                {
                    OwnerActor = *It;
                    break;
                }
            }
        }
        if (!IsValid(OwnerActor))
            return false;

        TArray<ULogCarryComponent*> CarryComponents;
        OwnerActor->GetComponents<ULogCarryComponent>(CarryComponents);
        ULogCarryComponent* Carry = nullptr;
        for (ULogCarryComponent* Candidate : CarryComponents)
        {
            if (IsValid(Candidate) && Candidate->GetFName() == State.CarryComponentName)
            {
                Carry = Candidate;
                break;
            }
        }
        if (!Carry && State.bPlayerControlledOwner && CarryComponents.Num() == 1)
            Carry = CarryComponents[0];
        if (!IsValid(Carry) || RestoredComponents.Contains(Carry) ||
            RestoredItemIds.Contains(State.HeldItemId))
            return false;

        AActor* ItemActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Candidate = *It;
            UWorldObjectIdentityComponent* Identity = IsValid(Candidate)
                ? Candidate->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
            if (IsValid(Identity) && Identity->HasValidIdentity() &&
                Identity->GetInstanceId() == State.HeldItemId)
            {
                if (ItemActor)
                    return false;
                ItemActor = Candidate;
            }
        }
        if (!IsValid(ItemActor) ||
            !Carry->RestoreHeldItemFromSnapshot(State.CarryOwnerId, ItemActor,
                State.bHeldMaterialPiece, State.HeldItemRelativeTransform))
            return false;
        RestoredComponents.Add(Carry);
        RestoredItemIds.Add(State.HeldItemId);
    }
    return true;
}

bool ValidateSnapshotPayloadPreflight(UWorld* World, const UWorldSimulationSaveGame& Snapshot)
{
    if (!World)
        return false;

    UWorldItemOwnershipSubsystem* OwnershipSubsystem =
        World->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldOperationLedgerSubsystem* OperationSubsystem =
        World->GetSubsystem<UWorldOperationLedgerSubsystem>();
    UWorldTerrainStateSubsystem* TerrainSubsystem =
        World->GetSubsystem<UWorldTerrainStateSubsystem>();
    UWorldAgricultureSubsystem* AgricultureSubsystem =
        World->GetSubsystem<UWorldAgricultureSubsystem>();
    UWorldSurvivalSubsystem* SurvivalSubsystem =
        World->GetSubsystem<UWorldSurvivalSubsystem>();
    UWorldProductionSubsystem* ProductionSubsystem =
        World->GetSubsystem<UWorldProductionSubsystem>();
    UWorldCargoTransportSubsystem* CargoSubsystem =
        World->GetSubsystem<UWorldCargoTransportSubsystem>();
    UWorldStorageSubsystem* StorageSubsystem =
        World->GetSubsystem<UWorldStorageSubsystem>();
    UWorldStructuralGraphSubsystem* StructuralSubsystem =
        World->GetSubsystem<UWorldStructuralGraphSubsystem>();

    if (!World || !OwnershipSubsystem || !OperationSubsystem || !TerrainSubsystem ||
        !AgricultureSubsystem || !SurvivalSubsystem || !ProductionSubsystem ||
        !CargoSubsystem || !StorageSubsystem || !StructuralSubsystem ||
        Snapshot.SchemaVersion != UWorldSimulationSaveGame::CurrentSchemaVersion ||
        Snapshot.Clock.SchemaVersion != UWorldSimulationClockSubsystem::CurrentSchemaVersion ||
        !FMath::IsFinite(Snapshot.Clock.SimulationTimeSeconds) ||
        Snapshot.Clock.SimulationTimeSeconds < 0.0 ||
        Snapshot.Clock.TickIndex < 0 || Snapshot.Clock.StateRevision < 0 ||
        Snapshot.Threat.SchemaVersion != 2 || !Snapshot.Threat.bInitialized ||
        !FMath::IsFinite(Snapshot.Threat.NextRaidTimeSeconds) ||
        Snapshot.Threat.NextRaidTimeSeconds < 0.0 || Snapshot.Threat.RaidSerial < 0 ||
        static_cast<uint8>(Snapshot.Weather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        !FMath::IsFinite(Snapshot.WeatherSeverity) ||
        Snapshot.WeatherSeverity < 0.0f || Snapshot.WeatherSeverity > 1.0f ||
        !FMath::IsFinite(SurvivalSubsystem->MaxCaloriesKcal) ||
        SurvivalSubsystem->MaxCaloriesKcal <= 0.0f ||
        Snapshot.Items.Num() > OwnershipSubsystem->MaxTrackedItems ||
        Snapshot.ItemTransferReceipts.Num() > OwnershipSubsystem->MaxTransferReceipts ||
        Snapshot.ItemSplitReceipts.Num() > OwnershipSubsystem->MaxSplitReceipts ||
        Snapshot.OperationLedgerEntries.Num() > OperationSubsystem->MaxEntries ||
        Snapshot.TerrainCells.Num() > TerrainSubsystem->MaxEditedCells ||
        Snapshot.TerrainReceipts.Num() > TerrainSubsystem->MaxOperationReceipts ||
        TerrainSubsystem->MaxEditableDepthLayers <= 0 || TerrainSubsystem->MaxDepthPerCell <= 0 ||
        Snapshot.Crops.Num() > AgricultureSubsystem->MaxCrops ||
        Snapshot.Animals.Num() > AgricultureSubsystem->MaxAnimals ||
        Snapshot.ProductionStations.Num() > ProductionSubsystem->MaxStations ||
        Snapshot.ProductionRecipes.Num() > ProductionSubsystem->MaxRecipes ||
        Snapshot.Cargo.Num() > CargoSubsystem->MaxActiveCargo ||
        Snapshot.StorageContainers.Num() > StorageSubsystem->MaxStorages)
    {
        return false;
    }

    auto IsValidRaidRecord = [](const FThreatRaidRecord& Raid, bool bAllowEmpty)
    {
        if (static_cast<uint8>(Raid.State) > static_cast<uint8>(EThreatRaidState::Cancelled))
            return false;
        if (Raid.State == EThreatRaidState::None && bAllowEmpty)
            return true;
        return Raid.RaidId.IsValid() && Raid.Serial > 0 && Raid.Revision >= 0 &&
            FMath::IsFinite(Raid.WarningStartTime) && Raid.WarningStartTime >= 0.0 &&
            FMath::IsFinite(Raid.ActiveStartTime) && Raid.ActiveStartTime >= Raid.WarningStartTime &&
            FMath::IsFinite(Raid.PlannedEndTime) && Raid.PlannedEndTime >= Raid.ActiveStartTime &&
            FMath::IsFinite(Raid.Pressure) && Raid.Pressure >= 0.0f &&
            FMath::IsFinite(Raid.TargetLocation.X) &&
            FMath::IsFinite(Raid.TargetLocation.Y) &&
            FMath::IsFinite(Raid.TargetLocation.Z) &&
            Raid.LastCommittedAttackPulseIndex >= -1 &&
            FMath::IsFinite(Raid.AttackIntervalSeconds) &&
            (Raid.State == EThreatRaidState::None
                ? Raid.AttackIntervalSeconds >= 0.0
                : Raid.AttackIntervalSeconds > 0.0);
    };
    if (!IsValidRaidRecord(Snapshot.Threat.CurrentRaid, true))
        return false;
    TSet<FGuid> RaidIds;
    for (const FThreatRaidRecord& Raid : Snapshot.Threat.RaidHistory)
    {
        if (!IsValidRaidRecord(Raid, false) ||
            Raid.State == EThreatRaidState::Warning || Raid.State == EThreatRaidState::Active ||
            RaidIds.Contains(Raid.RaidId))
        {
            return false;
        }
        RaidIds.Add(Raid.RaidId);
    }
    TMap<FGuid, FWorldOwnedItemRecord> ItemsById;
    ItemsById.Reserve(Snapshot.Items.Num());
    for (const FWorldOwnedItemRecord& Item : Snapshot.Items)
    {
        const bool bNeedsOwnerId = Item.OwnerKind == EWorldItemOwnerKind::Held ||
            Item.OwnerKind == EWorldItemOwnerKind::Inventory ||
            Item.OwnerKind == EWorldItemOwnerKind::Storage ||
            Item.OwnerKind == EWorldItemOwnerKind::InTransit;
        if (!Item.ItemId.IsValid() || ItemsById.Contains(Item.ItemId) ||
            Item.DefinitionId.IsNone() || Item.Quantity < 1 || Item.Revision < 1 ||
            static_cast<uint8>(Item.OwnerKind) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
            (bNeedsOwnerId && !Item.OwnerId.IsValid()) || Item.WorldLocation.ContainsNaN())
        {
            return false;
        }
        ItemsById.Add(Item.ItemId, Item);
    }

    if (!ValidateCarryComponentSnapshots(
        Snapshot.Items, Snapshot.PhysicalActors, Snapshot.CarryComponents))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World load refused because held-item ownership has no unique matching carry snapshot."));
        return false;
    }

    TSet<FGuid> OperationIds;
    for (const FWorldOperationLedgerEntry& Entry : Snapshot.OperationLedgerEntries)
    {
        if (!Entry.OperationId.IsValid() ||
            Entry.State != EWorldOperationLedgerEntryState::Completed ||
            OperationIds.Contains(Entry.OperationId) ||
            Entry.Result.OperationId != Entry.OperationId ||
            static_cast<uint8>(Entry.Result.Code) >
                static_cast<uint8>(EWorldOperationResultCode::InternalError) ||
            Entry.Result.bSucceeded !=
                (Entry.Result.Code == EWorldOperationResultCode::Success))
        {
            return false;
        }
        for (const FGuid& AffectedId : Entry.Result.AffectedObjectIds)
            if (!AffectedId.IsValid())
                return false;
        OperationIds.Add(Entry.OperationId);
    }

    TMap<FGuid, FWorldStorageContainerRecord> StoragesById;
    for (const FWorldStorageContainerRecord& Storage : Snapshot.StorageContainers)
    {
        if (!Storage.StorageId.IsValid() || StoragesById.Contains(Storage.StorageId) ||
            Storage.CapacityUnits < 1 || Storage.Revision < 1)
        {
            return false;
        }
        StoragesById.Add(Storage.StorageId, Storage);
    }

    TMap<FGuid, FWorldCargoTransportRecord> CargoById;
    TMap<FGuid, FGuid> ActiveCargoByItemId;
    for (const FWorldCargoTransportRecord& Cargo : Snapshot.Cargo)
    {
        if (!Cargo.CargoId.IsValid() || CargoById.Contains(Cargo.CargoId) ||
            !Cargo.ItemId.IsValid() || !Cargo.DestinationStorageId.IsValid() ||
            !StoragesById.Contains(Cargo.DestinationStorageId) ||
            Cargo.OriginNode.IsNone() || Cargo.DestinationNode.IsNone() ||
            Cargo.Route.Num() < 2 || Cargo.Route[0] != Cargo.OriginNode ||
            Cargo.Route.Last() != Cargo.DestinationNode ||
            static_cast<uint8>(Cargo.State) > static_cast<uint8>(ECargoTransportState::Cancelled) ||
            !FMath::IsFinite(Cargo.RouteCost) || Cargo.RouteCost < 0.0f ||
            !FMath::IsFinite(Cargo.Progress) || Cargo.Progress < 0.0f ||
            Cargo.Progress > Cargo.RouteCost ||
            !FMath::IsFinite(Cargo.CostPerSecond) || Cargo.CostPerSecond <= 0.0f ||
            Cargo.ItemRevision <= 0 || Cargo.Revision < 1 ||
            !Cargo.DeliveryOperationId.IsValid())
        {
            return false;
        }
        CargoById.Add(Cargo.CargoId, Cargo);
        if (Cargo.State == ECargoTransportState::InTransit ||
            Cargo.State == ECargoTransportState::Blocked)
        {
            if (ActiveCargoByItemId.Contains(Cargo.ItemId))
                return false;
            ActiveCargoByItemId.Add(Cargo.ItemId, Cargo.CargoId);
        }
    }

    TSet<FGuid> ReservedCargoIds;
    for (const FWorldStorageIncomingReservation& Reservation : Snapshot.StorageReservations)
    {
        if (!Reservation.CargoId.IsValid() || !Reservation.ItemId.IsValid() ||
            !Reservation.DestinationStorageId.IsValid() || Reservation.Quantity < 1 ||
            !StoragesById.Contains(Reservation.DestinationStorageId) ||
            !CargoById.Contains(Reservation.CargoId) ||
            ReservedCargoIds.Contains(Reservation.CargoId))
        {
            return false;
        }
        const FWorldCargoTransportRecord& Cargo = CargoById.FindChecked(Reservation.CargoId);
        const FWorldOwnedItemRecord* Item = ItemsById.Find(Reservation.ItemId);
        if (Cargo.ItemId != Reservation.ItemId ||
            Cargo.DestinationStorageId != Reservation.DestinationStorageId ||
            (Cargo.State != ECargoTransportState::InTransit &&
             Cargo.State != ECargoTransportState::Blocked) ||
            !Item || Item->OwnerKind != EWorldItemOwnerKind::InTransit ||
            Item->OwnerId != Reservation.CargoId ||
            Item->Quantity != Reservation.Quantity ||
            Item->Revision != Cargo.ItemRevision)
        {
            return false;
        }
        ReservedCargoIds.Add(Reservation.CargoId);
    }

    for (const TPair<FGuid, FWorldCargoTransportRecord>& Pair : CargoById)
    {
        const FWorldCargoTransportRecord& Cargo = Pair.Value;
        const FWorldOwnedItemRecord* Item = ItemsById.Find(Cargo.ItemId);
        if (!Item)
            return false;

        if (Cargo.State == ECargoTransportState::InTransit ||
            Cargo.State == ECargoTransportState::Blocked)
        {
            if (!ReservedCargoIds.Contains(Cargo.CargoId) ||
                Item->OwnerKind != EWorldItemOwnerKind::InTransit ||
                Item->OwnerId != Cargo.CargoId ||
                Item->Revision != Cargo.ItemRevision)
                return false;
        }
        else if (Cargo.State == ECargoTransportState::Delivered)
        {
            if (Cargo.Progress < Cargo.RouteCost || Item->Revision < Cargo.ItemRevision ||
                ReservedCargoIds.Contains(Cargo.CargoId))
                return false;
        }
        else if (Cargo.State == ECargoTransportState::Cancelled)
        {
            if ((Item->OwnerKind == EWorldItemOwnerKind::InTransit && Item->OwnerId == Cargo.CargoId) ||
                ReservedCargoIds.Contains(Cargo.CargoId))
                return false;
        }
    }

    // Catch over-capacity storage snapshots before the live ownership/storage
    // subsystems are touched by ApplySnapshot.
    for (const TPair<FGuid, FWorldStorageContainerRecord>& Pair : StoragesById)
    {
        int64 StoredQuantity = 0;
        for (const TPair<FGuid, FWorldOwnedItemRecord>& ItemPair : ItemsById)
        {
            const FWorldOwnedItemRecord& Item = ItemPair.Value;
            if (Item.OwnerKind == EWorldItemOwnerKind::Storage && Item.OwnerId == Pair.Key)
            {
                if (StoredQuantity > MAX_int64 - Item.Quantity)
                    return false;
                StoredQuantity += Item.Quantity;
            }
        }
        int64 ReservedQuantity = 0;
        for (const FWorldStorageIncomingReservation& Reservation : Snapshot.StorageReservations)
        {
            if (Reservation.DestinationStorageId == Pair.Key)
            {
                if (ReservedQuantity > MAX_int64 - Reservation.Quantity)
                    return false;
                ReservedQuantity += Reservation.Quantity;
            }
        }
        int64 EffectiveCapacity = Pair.Value.CapacityUnits;
        if (StorageSubsystem->IsStorageAvailable(Pair.Key))
        {
            FWorldStorageContainerRecord LiveStorage;
            if (!StorageSubsystem->GetStorage(Pair.Key, LiveStorage) ||
                LiveStorage.CapacityUnits <= 0)
            {
                return false;
            }
            // The live authored container configuration wins over an old save's
            // stale capacity, matching UWorldStorageSubsystem::RestoreSnapshot.
            EffectiveCapacity = LiveStorage.CapacityUnits;
        }
        if (StoredQuantity > EffectiveCapacity ||
            ReservedQuantity > EffectiveCapacity - StoredQuantity)
        {
            return false;
        }
    }

    TSet<FGuid> PhysicalActorIds;
    for (const FWorldPhysicalActorSnapshot& Actor : Snapshot.PhysicalActors)
    {
        if (!Actor.IdentityId.IsValid() || PhysicalActorIds.Contains(Actor.IdentityId))
            return false;
        PhysicalActorIds.Add(Actor.IdentityId);
    }

    for (const TPair<FGuid, FWorldStorageContainerRecord>& Pair : StoragesById)
    {
        bool bHasStoredItems = false;
        for (const TPair<FGuid, FWorldOwnedItemRecord>& ItemPair : ItemsById)
        {
            const FWorldOwnedItemRecord& Item = ItemPair.Value;
            if (Item.OwnerKind == EWorldItemOwnerKind::Storage && Item.OwnerId == Pair.Key)
            {
                bHasStoredItems = true;
                break;
            }
        }

        // Physical storage actors are identity-bound to StorageId. An available
        // storage, or one that owns items, must have a physical actor in the
        // snapshot so its runtime Stored pointer list can be rebuilt after load.
        if ((Pair.Value.bAvailable || bHasStoredItems) && !PhysicalActorIds.Contains(Pair.Key))
            return false;
    }

    TSet<FGuid> InventoryOwnerIds;
    TSet<FGuid> ReferencedInventoryWorldItems;
    for (const FWorldInventorySnapshot& Inventory : Snapshot.Inventories)
    {
        if (!Inventory.InventoryOwnerId.IsValid() ||
            InventoryOwnerIds.Contains(Inventory.InventoryOwnerId) ||
            (Inventory.OwnerActorId.IsValid() && !PhysicalActorIds.Contains(Inventory.OwnerActorId)))
        {
            return false;
        }
        InventoryOwnerIds.Add(Inventory.InventoryOwnerId);
        TSet<FName> StackItemNames;
        for (const FInventoryStack& Stack : Inventory.Items)
        {
            if (Stack.ItemId.IsNone() || StackItemNames.Contains(Stack.ItemId) ||
                Stack.Quantity < 1 || Stack.MaxStack < 1 || Stack.Quantity > Stack.MaxStack ||
                static_cast<uint8>(Stack.Category) > static_cast<uint8>(EInventoryItemCategory::Other))
            {
                return false;
            }
            StackItemNames.Add(Stack.ItemId);
            if (Stack.OwnedWorldItemId.IsValid())
            {
                const FWorldOwnedItemRecord* Item = ItemsById.Find(Stack.OwnedWorldItemId);
                if (!Item || ReferencedInventoryWorldItems.Contains(Stack.OwnedWorldItemId) ||
                    Item->OwnerKind != EWorldItemOwnerKind::Inventory ||
                    Item->OwnerId != Inventory.InventoryOwnerId || Item->Quantity != 1 ||
                    Item->DefinitionId != Stack.ItemId || Stack.MaxStack != 1)
                {
                    return false;
                }
                ReferencedInventoryWorldItems.Add(Stack.OwnedWorldItemId);
            }
        }
    }

    for (const TPair<FGuid, FWorldOwnedItemRecord>& Pair : ItemsById)
    {
        const FWorldOwnedItemRecord& Item = Pair.Value;
        if (Item.OwnerKind == EWorldItemOwnerKind::Storage && !StoragesById.Contains(Item.OwnerId))
            return false;
        if (Item.OwnerKind == EWorldItemOwnerKind::Inventory &&
            (!InventoryOwnerIds.Contains(Item.OwnerId) ||
             !ReferencedInventoryWorldItems.Contains(Item.ItemId)))
            return false;
        if (Item.OwnerKind == EWorldItemOwnerKind::InTransit &&
            ActiveCargoByItemId.FindRef(Item.ItemId) != Item.OwnerId)
            return false;
    }

    TMap<FIntVector, FWorldTerrainCellState> TerrainByCell;
    for (const FWorldTerrainCellState& Cell : Snapshot.TerrainCells)
    {
        if (TerrainByCell.Contains(Cell.Cell) ||
            Cell.DepthLayer < 0 || Cell.DepthLayer >= TerrainSubsystem->MaxEditableDepthLayers ||
            Cell.DepthLayer >= TerrainSubsystem->MaxDepthPerCell || Cell.Revision <= 0 ||
            (!Cell.bExcavated && Cell.MaterialId.IsNone()) ||
            (TerrainSubsystem->PermittedDepthLayers.Num() > 0 &&
             !TerrainSubsystem->PermittedDepthLayers.Contains(Cell.DepthLayer)))
        {
            return false;
        }
        TerrainByCell.Add(Cell.Cell, Cell);
    }

    TSet<FGuid> TerrainOperationIds;
    for (const FWorldTerrainOperationReceipt& Receipt : Snapshot.TerrainReceipts)
    {
        const FWorldTerrainCellState* Cell = TerrainByCell.Find(Receipt.Cell);
        if (!Receipt.OperationId.IsValid() || TerrainOperationIds.Contains(Receipt.OperationId) ||
            !Cell || Receipt.ExpectedRevision < 0 || Receipt.ExpectedRevision >= MAX_int64 ||
            (Receipt.ExpectedRevision == 0 && Receipt.RequestedEdit != ETerrainCellEdit::Excavate) ||
            Receipt.ResultingRevision != Receipt.ExpectedRevision + 1 ||
            Receipt.ResultState.Cell != Receipt.Cell ||
            Receipt.ResultState.Revision != Receipt.ResultingRevision ||
            Receipt.RequestedDepthLayer != Receipt.ResultState.DepthLayer ||
            Receipt.RequestedDepthLayer < 0 ||
            Receipt.RequestedDepthLayer >= TerrainSubsystem->MaxEditableDepthLayers ||
            Receipt.RequestedDepthLayer >= TerrainSubsystem->MaxDepthPerCell ||
            static_cast<uint8>(Receipt.RequestedEdit) >
                static_cast<uint8>(ETerrainCellEdit::ReplaceMaterial) ||
            Cell->Revision < Receipt.ResultingRevision ||
            (TerrainSubsystem->PermittedDepthLayers.Num() > 0 &&
             !TerrainSubsystem->PermittedDepthLayers.Contains(Receipt.RequestedDepthLayer)) ||
            ((Receipt.RequestedEdit == ETerrainCellEdit::Fill ||
              Receipt.RequestedEdit == ETerrainCellEdit::ReplaceMaterial) &&
             Receipt.RequestedMaterialId.IsNone()) ||
            (Receipt.RequestedEdit == ETerrainCellEdit::Excavate && !Receipt.ResultState.bExcavated) ||
            (Receipt.RequestedEdit == ETerrainCellEdit::Fill &&
             (Receipt.ResultState.bExcavated ||
              Receipt.ResultState.MaterialId != Receipt.RequestedMaterialId)) ||
            (Receipt.RequestedEdit == ETerrainCellEdit::ReplaceMaterial &&
             Receipt.ResultState.MaterialId != Receipt.RequestedMaterialId))
        {
            return false;
        }
        TerrainOperationIds.Add(Receipt.OperationId);
    }

    TSet<FGuid> SurvivalProfileIds;
    for (const FWorldSurvivalProfile& Profile : Snapshot.SurvivalProfiles)
    {
        if (!Profile.EntityId.IsValid() || SurvivalProfileIds.Contains(Profile.EntityId) ||
            Profile.Revision <= 0 ||
            !FMath::IsFinite(Profile.Health) || Profile.Health < 0.0f || Profile.Health > 100.0f ||
            !FMath::IsFinite(Profile.Hunger) || Profile.Hunger < 0.0f || Profile.Hunger > 100.0f ||
            !FMath::IsFinite(Profile.Thirst) || Profile.Thirst < 0.0f || Profile.Thirst > 100.0f ||
            !FMath::IsFinite(Profile.Stamina) || Profile.Stamina < 0.0f || Profile.Stamina > 100.0f ||
            !FMath::IsFinite(Profile.Fatigue) || Profile.Fatigue < 0.0f || Profile.Fatigue > 100.0f ||
            !FMath::IsFinite(Profile.ClothingInsulation) || Profile.ClothingInsulation < 0.0f || Profile.ClothingInsulation > 1.0f ||
            !FMath::IsFinite(Profile.ShelterProtection) || Profile.ShelterProtection < 0.0f || Profile.ShelterProtection > 1.0f ||
            !FMath::IsFinite(Profile.Wetness) || Profile.Wetness < 0.0f || Profile.Wetness > 1.0f ||
            !FMath::IsFinite(Profile.CaloriesKcal) || Profile.CaloriesKcal < 0.0f ||
            Profile.CaloriesKcal > SurvivalSubsystem->MaxCaloriesKcal ||
            !FMath::IsFinite(Profile.CoreTemperatureCelsius) ||
            Profile.CoreTemperatureCelsius < 20.0f || Profile.CoreTemperatureCelsius > 45.0f ||
            !FMath::IsFinite(Profile.PendingActivityKcal) ||
            Profile.PendingActivityKcal < 0.0f || Profile.PendingActivityKcal > 100000.0f ||
            !FMath::IsFinite(Profile.FeltTemperatureCelsius) ||
            !FMath::IsFinite(Profile.Location.X) ||
            !FMath::IsFinite(Profile.Location.Y) ||
            !FMath::IsFinite(Profile.Location.Z))
        {
            return false;
        }
        SurvivalProfileIds.Add(Profile.EntityId);
    }

    TSet<FName> DiscoveryIds;
    for (const FWorldDiscoveryRecord& Discovery : Snapshot.Discoveries)
    {
        if (Discovery.DiscoveryId.IsNone() || DiscoveryIds.Contains(Discovery.DiscoveryId) ||
            Discovery.Location.ContainsNaN() ||
            !FMath::IsFinite(Discovery.DiscoveredAtTime) || Discovery.DiscoveredAtTime < 0.0)
        {
            return false;
        }
        DiscoveryIds.Add(Discovery.DiscoveryId);
    }

    TSet<FGuid> CropIds;
    for (const FWorldCropState& Crop : Snapshot.Crops)
    {
        if (!Crop.CropId.IsValid() || CropIds.Contains(Crop.CropId) ||
            Crop.CropDefinitionId.IsNone() ||
            static_cast<uint8>(Crop.Stage) > static_cast<uint8>(ECropGrowthStage::Withered) ||
            Crop.Revision < 1 ||
            !FMath::IsFinite(Crop.Moisture) || Crop.Moisture < 0.0f || Crop.Moisture > 1.0f ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds) || Crop.GrowthProgressSeconds < 0.0 ||
            !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0)
        {
            return false;
        }
        CropIds.Add(Crop.CropId);
    }

    TSet<FGuid> AnimalIds;
    for (const FWorldAnimalState& Animal : Snapshot.Animals)
    {
        if (!Animal.AnimalId.IsValid() || AnimalIds.Contains(Animal.AnimalId) ||
            Animal.AnimalDefinitionId.IsNone() || Animal.Revision < 1 ||
            !FMath::IsFinite(Animal.Hunger) || Animal.Hunger < 0.0f || Animal.Hunger > 100.0f ||
            !FMath::IsFinite(Animal.Health) || Animal.Health < 0.0f || Animal.Health > 100.0f ||
            Animal.ProductUnits < 0 || !FMath::IsFinite(Animal.ProductProgressSeconds) ||
            Animal.ProductProgressSeconds < 0.0 || Animal.ProductProgressSeconds >= 3600.0)
        {
            return false;
        }
        AnimalIds.Add(Animal.AnimalId);
    }

    // Validate production recipes, stock, queued output reservations, jobs and
    // durable receipts before the first domain is mutated by ApplySnapshot.
    TMap<FName, FWorldProductionRecipe> RecipesById;
    for (const FWorldProductionRecipe& Recipe : Snapshot.ProductionRecipes)
    {
        if (Recipe.RecipeId.IsNone() || RecipesById.Contains(Recipe.RecipeId) ||
            Recipe.Inputs.IsEmpty() || Recipe.Outputs.IsEmpty() ||
            !FMath::IsFinite(Recipe.DurationSeconds) || Recipe.DurationSeconds <= 0.0)
        {
            return false;
        }
        for (const FProductionIngredient& Input : Recipe.Inputs)
            if (Input.ItemId.IsNone() || Input.Quantity <= 0)
                return false;
        for (const FProductionIngredient& Output : Recipe.Outputs)
            if (Output.ItemId.IsNone() || Output.Quantity <= 0)
                return false;
        RecipesById.Add(Recipe.RecipeId, Recipe);
    }

    TSet<FGuid> StationIds;
    TSet<FGuid> ProductionJobIds;
    TSet<FGuid> EnqueueReceiptJobIds;
    TSet<FGuid> ProductionReceiptOperationIds;
    TSet<FGuid> IntakeSourceItemIds;
    TSet<FGuid> MaterializedWorldItemIds;
    for (const FWorldProductionStation& Station : Snapshot.ProductionStations)
    {
        if (!Station.StationId.IsValid() || StationIds.Contains(Station.StationId) ||
            Station.StationTag.IsNone() || Station.StorageCapacityUnits <= 0 ||
            Station.Revision <= 0 || Station.Jobs.Num() > ProductionSubsystem->MaxJobsPerStation)
        {
            return false;
        }
        StationIds.Add(Station.StationId);

        int64 StockUnits = 0;
        for (const TPair<FName, int32>& Stock : Station.Stock)
        {
            if (Stock.Key.IsNone() || Stock.Value < 0 || StockUnits > MAX_int64 - Stock.Value)
                return false;
            StockUnits += Stock.Value;
        }
        if (StockUnits > Station.StorageCapacityUnits)
            return false;

        int64 ReservedOutputUnits = 0;
        TSet<FGuid> StationJobIds;
        for (const FWorldProductionJob& Job : Station.Jobs)
        {
            const FWorldProductionRecipe* Recipe = RecipesById.Find(Job.RecipeId);
            if (!Job.JobId.IsValid() || StationJobIds.Contains(Job.JobId) ||
                ProductionJobIds.Contains(Job.JobId) || !Recipe ||
                static_cast<uint8>(Job.State) > static_cast<uint8>(EProductionJobState::Blocked) ||
                !FMath::IsFinite(Job.ProgressSeconds) || Job.ProgressSeconds < 0.0 ||
                Job.ProgressSeconds > Recipe->DurationSeconds)
            {
                return false;
            }
            StationJobIds.Add(Job.JobId);
            ProductionJobIds.Add(Job.JobId);

            if (Job.State == EProductionJobState::Queued ||
                Job.State == EProductionJobState::Running)
            {
                for (const FProductionIngredient& Output : Recipe->Outputs)
                {
                    if (ReservedOutputUnits > MAX_int64 - Output.Quantity)
                        return false;
                    ReservedOutputUnits += Output.Quantity;
                }
            }
        }
        if (ReservedOutputUnits > Station.StorageCapacityUnits - StockUnits)
            return false;

        for (const FWorldProductionPhysicalIntakeReceipt& Receipt : Station.PhysicalIntakeReceipts)
        {
            if (!Receipt.OperationId.IsValid() || !Receipt.SourceItemId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 || Receipt.ExpectedItemRevision == MAX_int64 ||
                Receipt.ExpectedStationRevision <= 0 || Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.SourceDefinitionId.IsNone() || Receipt.StockItemId.IsNone() ||
                Receipt.StockItemId != UWorldProductionSubsystem::MakePhysicalStockItemId(
                    Receipt.SourceDefinitionId, Receipt.SourceMeshVariantSetId) ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionReceiptOperationIds.Contains(Receipt.OperationId) ||
                IntakeSourceItemIds.Contains(Receipt.SourceItemId))
            {
                return false;
            }
            ProductionReceiptOperationIds.Add(Receipt.OperationId);
            IntakeSourceItemIds.Add(Receipt.SourceItemId);
        }

        for (const FWorldProductionEnqueueReceipt& Receipt : Station.EnqueueReceipts)
        {
            if (!Receipt.OperationId.IsValid() || Receipt.RecipeId.IsNone() ||
                !RecipesById.Contains(Receipt.RecipeId) || !Receipt.JobId.IsValid() ||
                Receipt.ExpectedStationRevision <= 0 || Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionReceiptOperationIds.Contains(Receipt.OperationId) ||
                EnqueueReceiptJobIds.Contains(Receipt.JobId))
            {
                return false;
            }
            ProductionReceiptOperationIds.Add(Receipt.OperationId);
            EnqueueReceiptJobIds.Add(Receipt.JobId);
        }

        for (const FWorldProductionCollectReceipt& Receipt : Station.CollectReceipts)
        {
            if (!Receipt.OperationId.IsValid() || !Receipt.JobId.IsValid() ||
                Receipt.ExpectedStationRevision <= 0 || Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionReceiptOperationIds.Contains(Receipt.OperationId) ||
                StationJobIds.Contains(Receipt.JobId))
            {
                return false;
            }
            ProductionReceiptOperationIds.Add(Receipt.OperationId);
        }

        for (const FWorldProductionMaterializationReceipt& Receipt : Station.MaterializationReceipts)
        {
            if (!Receipt.OperationId.IsValid() || Receipt.StockItemId.IsNone() ||
                Receipt.DefinitionId.IsNone() || Receipt.BaseDefinitionId.IsNone() ||
                !Receipt.WorldItemId.IsValid() ||
                Receipt.StockItemId != UWorldProductionSubsystem::MakePhysicalStockItemId(
                    Receipt.DefinitionId, Receipt.MeshVariantSetId) ||
                Receipt.ExpectedStationRevision <= 0 || Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionReceiptOperationIds.Contains(Receipt.OperationId) ||
                MaterializedWorldItemIds.Contains(Receipt.WorldItemId) ||
                IntakeSourceItemIds.Contains(Receipt.WorldItemId))
            {
                return false;
            }
            ProductionReceiptOperationIds.Add(Receipt.OperationId);
            MaterializedWorldItemIds.Add(Receipt.WorldItemId);
        }
    }

    TSet<FGuid> StructuralNodeIds;
    for (const FWorldStructuralNodeSnapshot& Node : Snapshot.StructuralGraph)
    {
        if (!Node.NodeId.IsValid() || StructuralNodeIds.Contains(Node.NodeId) ||
            !Node.ActorIdentityId.IsValid() || !PhysicalActorIds.Contains(Node.ActorIdentityId) ||
            !FMath::IsFinite(Node.LocalLoad) || Node.LocalLoad < 0.0f ||
            !FMath::IsFinite(Node.MaxSupportedLoad) || Node.MaxSupportedLoad <= 0.0f ||
            !FMath::IsFinite(Node.Damage) || Node.Damage < 0.0f ||
            !FMath::IsFinite(Node.DamageThreshold) || Node.DamageThreshold <= 0.0f ||
            Node.Revision < 0)
        {
            return false;
        }
        StructuralNodeIds.Add(Node.NodeId);
    }
    for (const FWorldStructuralNodeSnapshot& Node : Snapshot.StructuralGraph)
    {
        if (Node.ParentNodeId.IsValid() && !StructuralNodeIds.Contains(Node.ParentNodeId))
            return false;
        for (const FStructuralSupportLink& Support : Node.SecondarySupports)
            if (!Support.SupportNodeId.IsValid() || !StructuralNodeIds.Contains(Support.SupportNodeId))
                return false;
    }

    return true;
}

bool ValidatePhysicalActorSnapshotSet(UWorld* World, const TArray<FWorldPhysicalActorSnapshot>& Snapshot)
{
    if (!World)
        return false;

    TMap<FGuid, AActor*> CurrentActors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        UWorldObjectIdentityComponent* Identity =
            IsValid(Actor) ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        if (!IsValid(Identity) || !Identity->InstanceId.IsValid())
            continue;
        if (CurrentActors.Contains(Identity->InstanceId))
            return false;
        CurrentActors.Add(Identity->InstanceId, Actor);
    }

    TSet<FGuid> SeenIdentityIds;
    for (const FWorldPhysicalActorSnapshot& State : Snapshot)
    {
        if (!State.IdentityId.IsValid() || SeenIdentityIds.Contains(State.IdentityId) ||
            State.ActorClass.IsNull() || !State.ActorTransform.IsValid() ||
            static_cast<uint8>(State.LifecycleState) >
                static_cast<uint8>(EWorldObjectLifecycleState::Destroyed) ||
            State.StateRevision < 0)
        {
            return false;
        }
        SeenIdentityIds.Add(State.IdentityId);

        UClass* ActorClass = State.ActorClass.TryLoadClass<AActor>();
        if (!ActorClass || ActorClass->HasAnyClassFlags(
            CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            UE_LOG(LogTemp, Warning, TEXT("World load refused: actor class '%s' cannot be restored."),
                *State.ActorClass.ToString());
            return false;
        }

        AActor* ClassDefault = Cast<AActor>(ActorClass->GetDefaultObject());
        if (!IsValid(ClassDefault) ||
            !ClassDefault->FindComponentByClass<UWorldObjectIdentityComponent>())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("World load refused: actor class '%s' has no stable identity component."),
                *State.ActorClass.ToString());
            return false;
        }

        TArray<UPrimitiveComponent*> DefaultPrimitives;
        ClassDefault->GetComponents<UPrimitiveComponent>(DefaultPrimitives);
        if (DefaultPrimitives.Num() != State.Components.Num())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("World load refused: primitive component layout changed for actor class '%s'."),
                *State.ActorClass.ToString());
            return false;
        }

        TSet<FName> SeenComponentNames;
        for (const FWorldPhysicalComponentSnapshot& Component : State.Components)
        {
            if (Component.ComponentName.IsNone() ||
                SeenComponentNames.Contains(Component.ComponentName) ||
                !Component.bIsPrimitive || Component.ComponentClass.IsNull() ||
                !Component.ActorRelativeTransform.IsValid() ||
                static_cast<uint8>(Component.CollisionEnabled.GetValue()) >
                    static_cast<uint8>(ECollisionEnabled::QueryAndPhysics) ||
                Component.LinearVelocity.ContainsNaN() ||
                Component.AngularVelocityRadians.ContainsNaN())
            {
                return false;
            }

            UPrimitiveComponent* DefaultPrimitive = nullptr;
            for (UPrimitiveComponent* Candidate : DefaultPrimitives)
            {
                if (IsValid(Candidate) && Candidate->GetFName() == Component.ComponentName)
                {
                    DefaultPrimitive = Candidate;
                    break;
                }
            }
            if (!DefaultPrimitive ||
                FSoftClassPath(DefaultPrimitive->GetClass()) != Component.ComponentClass)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("World load refused: primitive '%s' is not part of the saved actor class layout."),
                    *Component.ComponentName.ToString());
                return false;
            }

            if (!Component.StaticMeshAsset.IsNull())
            {
                if (!Cast<UStaticMeshComponent>(DefaultPrimitive) ||
                    !Cast<UStaticMesh>(Component.StaticMeshAsset.TryLoad()))
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("World load refused: static mesh reference for component '%s' is unavailable."),
                        *Component.ComponentName.ToString());
                    return false;
                }
            }
            SeenComponentNames.Add(Component.ComponentName);
        }

        if (AActor* const* Existing = CurrentActors.Find(State.IdentityId))
        {
            if (!IsValid(*Existing) ||
                FSoftClassPath((*Existing)->GetClass()) != State.ActorClass)
            {
                return false;
            }
            TArray<UPrimitiveComponent*> ExistingPrimitives;
            (*Existing)->GetComponents<UPrimitiveComponent>(ExistingPrimitives);
            if (ExistingPrimitives.Num() != State.Components.Num())
                return false;

            // Validate the live instance too; matching only the class default
            // object would allow a dynamically replaced component to fail later,
            // after ownership or the clock has already changed.
            for (const FWorldPhysicalComponentSnapshot& Component : State.Components)
            {
                UPrimitiveComponent* ExistingPrimitive = nullptr;
                for (UPrimitiveComponent* Candidate : ExistingPrimitives)
                {
                    if (IsValid(Candidate) && Candidate->GetFName() == Component.ComponentName)
                    {
                        ExistingPrimitive = Candidate;
                        break;
                    }
                }
                if (!ExistingPrimitive ||
                    FSoftClassPath(ExistingPrimitive->GetClass()) != Component.ComponentClass ||
                    (!Component.StaticMeshAsset.IsNull() &&
                     !Cast<UStaticMeshComponent>(ExistingPrimitive)))
                {
                    return false;
                }
            }
        }
    }

    // A differing actor population is valid: missing identities will be spawned
    // from their recorded class, and identities absent from the snapshot removed.
    return true;
}

bool ApplyPhysicalActorSnapshots(UWorld* World, const TArray<FWorldPhysicalActorSnapshot>& Snapshot)
{
    if (!ValidatePhysicalActorSnapshotSet(World, Snapshot))
        return false;

    TMap<FGuid, AActor*> CurrentActors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        UWorldObjectIdentityComponent* Identity =
            IsValid(Actor) ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        if (IsValid(Identity) && Identity->InstanceId.IsValid())
            CurrentActors.Add(Identity->InstanceId, Actor);
    }

    TSet<FGuid> DesiredIds;
    DesiredIds.Reserve(Snapshot.Num());

    // Spawn missing actors deferred so their canonical identity is installed
    // before BeginPlay registers it with the world identity subsystem.
    for (const FWorldPhysicalActorSnapshot& State : Snapshot)
    {
        DesiredIds.Add(State.IdentityId);
        if (CurrentActors.Contains(State.IdentityId))
            continue;

        UClass* ActorClass = State.ActorClass.TryLoadClass<AActor>();
        if (!ActorClass)
            return false;

        AActor* Spawned = World->SpawnActorDeferred<AActor>(
            ActorClass, State.ActorTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Spawned)
            return false;

        UWorldObjectIdentityComponent* Identity =
            Spawned->FindComponentByClass<UWorldObjectIdentityComponent>();
        if (!Identity)
        {
            Spawned->Destroy();
            return false;
        }
        // Restore actor-level SaveGame fields before construction/BeginPlay
        // so initialization can observe its saved domain state. Reapply after
        // construction below because construction scripts may reset components.
        {
            FMemoryReader Reader(State.ActorSaveGameData, true);
            FObjectAndNameAsStringProxyArchive ActorArchive(Reader, true);
            ActorArchive.ArIsSaveGame = true;
            Spawned->Serialize(ActorArchive);
            if (Reader.IsError())
            {
                Spawned->Destroy();
                return false;
            }
        }
        Identity = Spawned->FindComponentByClass<UWorldObjectIdentityComponent>();
        if (!IsValid(Identity))
        {
            Spawned->Destroy();
            return false;
        }
        Identity->InstanceId = State.IdentityId;
        Identity->DefinitionId = State.DefinitionId;
        Identity->LifecycleState = State.LifecycleState;
        Identity->StateRevision = State.StateRevision;

        Spawned->FinishSpawning(State.ActorTransform);
        if (!IsValid(Spawned) ||
            !Spawned->GetClass()->IsChildOf(ActorClass))
        {
            if (IsValid(Spawned))
                Spawned->Destroy();
            return false;
        }
        TArray<UPrimitiveComponent*> SpawnedPrimitives;
        Spawned->GetComponents<UPrimitiveComponent>(SpawnedPrimitives);
        if (SpawnedPrimitives.Num() != State.Components.Num())
        {
            Spawned->Destroy();
            return false;
        }
        Identity = Spawned->FindComponentByClass<UWorldObjectIdentityComponent>();
        if (!IsValid(Identity) || Identity->InstanceId != State.IdentityId)
        {
            Spawned->Destroy();
            return false;
        }
        CurrentActors.Add(State.IdentityId, Spawned);
    }

    // Remove tracked actors that did not exist in the target snapshot. This
    // makes restore independent of the live world's dynamic actor population.
    TArray<FGuid> ExtraIds;
    for (const TPair<FGuid, AActor*>& Pair : CurrentActors)
    {
        if (!DesiredIds.Contains(Pair.Key))
            ExtraIds.Add(Pair.Key);
    }
    for (const FGuid& ExtraId : ExtraIds)
    {
        AActor* Actor = CurrentActors.FindRef(ExtraId);
        if (IsValid(Actor) && !Actor->Destroy())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("World load could not remove extra physical actor %s."),
                *ExtraId.ToString());
            return false;
        }
        CurrentActors.Remove(ExtraId);
    }

    // Restore actor transforms first, then actor-level SaveGame payload and
    // component/physics state. Cross-actor attachments are rebuilt by the
    // authoritative structural-graph restore immediately afterward.
    for (const FWorldPhysicalActorSnapshot& State : Snapshot)
    {
        AActor* Actor = CurrentActors.FindRef(State.IdentityId);
        if (!IsValid(Actor) ||
            !Actor->SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics))
        {
            return false;
        }
        Actor->SetActorHiddenInGame(State.bActorHiddenInGame);
        Actor->SetActorEnableCollision(State.bActorCollisionEnabled);
        Actor->SetActorTickEnabled(State.bActorTickEnabled);
    }

    for (const FWorldPhysicalActorSnapshot& State : Snapshot)
    {
        AActor* Actor = CurrentActors.FindRef(State.IdentityId);
        UWorldObjectIdentityComponent* Identity =
            IsValid(Actor) ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        if (!IsValid(Identity))
            return false;

        {
            FMemoryReader Reader(State.ActorSaveGameData, true);
            FObjectAndNameAsStringProxyArchive ActorArchive(Reader, true);
            ActorArchive.ArIsSaveGame = true;
            Actor->Serialize(ActorArchive);
            if (Reader.IsError())
                return false;
        }
        Identity->InstanceId = State.IdentityId;
        Identity->DefinitionId = State.DefinitionId;
        Identity->LifecycleState = State.LifecycleState;
        Identity->StateRevision = State.StateRevision;

        for (const FWorldPhysicalComponentSnapshot& ComponentState : State.Components)
        {
            UPrimitiveComponent* Primitive = nullptr;
            TArray<UPrimitiveComponent*> Primitives;
            Actor->GetComponents<UPrimitiveComponent>(Primitives);
            for (UPrimitiveComponent* Candidate : Primitives)
            {
                if (IsValid(Candidate) && Candidate->GetFName() == ComponentState.ComponentName)
                {
                    Primitive = Candidate;
                    break;
                }
            }
            if (!Primitive || FSoftClassPath(Primitive->GetClass()) != ComponentState.ComponentClass)
                return false;

            if (Primitive != Actor->GetRootComponent())
            {
                Primitive->SetRelativeTransform(
                    ComponentState.ActorRelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
            }

            if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Primitive))
            {
                UStaticMesh* MeshAsset = ComponentState.StaticMeshAsset.IsNull()
                    ? nullptr
                    : Cast<UStaticMesh>(ComponentState.StaticMeshAsset.TryLoad());
                if (!ComponentState.StaticMeshAsset.IsNull() && !MeshAsset)
                    return false;
                StaticMesh->SetStaticMesh(MeshAsset);
            }
            else if (!ComponentState.StaticMeshAsset.IsNull())
            {
                return false;
            }

            Primitive->SetCollisionEnabled(ComponentState.CollisionEnabled.GetValue());
            Primitive->SetSimulatePhysics(ComponentState.bSimulatePhysics);
            if (ComponentState.bSimulatePhysics)
            {
                Primitive->SetPhysicsLinearVelocity(ComponentState.LinearVelocity, false);
                Primitive->SetPhysicsAngularVelocityInRadians(ComponentState.AngularVelocityRadians, false);
            }
        }
    }
    return true;
}

} // namespace

bool UWorldSaveCoordinatorSubsystem::CaptureCurrentState(UWorldSimulationSaveGame* OutSave) const
{
    UWorld* World = GetWorld();
    if (!World || !OutSave) return false;
    UWorldSimulationClockSubsystem* Clock = World->GetSubsystem<UWorldSimulationClockSubsystem>();
    UWorldThreatDirectorSubsystem* Threat = World->GetSubsystem<UWorldThreatDirectorSubsystem>();
    UWorldItemOwnershipSubsystem* Items = World->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldTerrainStateSubsystem* Terrain = World->GetSubsystem<UWorldTerrainStateSubsystem>();
    UWorldAgricultureSubsystem* Agriculture = World->GetSubsystem<UWorldAgricultureSubsystem>();
    UWorldSurvivalSubsystem* Survival = World->GetSubsystem<UWorldSurvivalSubsystem>();
    UWorldProductionSubsystem* Production = World->GetSubsystem<UWorldProductionSubsystem>();
    UWorldCargoTransportSubsystem* Cargo = World->GetSubsystem<UWorldCargoTransportSubsystem>();
    UWorldStorageSubsystem* Storage = World->GetSubsystem<UWorldStorageSubsystem>();
    UWorldOperationLedgerSubsystem* OperationLedger = World->GetSubsystem<UWorldOperationLedgerSubsystem>();
    if (!Clock || !Threat || !Items || !Terrain || !Agriculture || !Survival || !Production || !Cargo || !Storage || !OperationLedger ||
        !Clock->IsClockInitialized() || !Threat->IsDirectorInitialized()) return false;
    if (OperationLedger->HasPendingOperations())
    {
        UE_LOG(LogTemp, Warning, TEXT("World save refused while a world operation is pending."));
        return false;
    }

    OutSave->SchemaVersion = UWorldSimulationSaveGame::CurrentSchemaVersion;
    OutSave->Clock = Clock->ExportSnapshot();
    OutSave->Threat = Threat->ExportSnapshot();
    Items->ExportSnapshot(OutSave->Items, OutSave->ItemTransferReceipts, OutSave->ItemSplitReceipts);
    OperationLedger->ExportSnapshot(OutSave->OperationLedgerEntries);
    if (!CaptureCarryComponentSnapshots(World, OutSave->CarryComponents))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World save refused because held-item/carry-component state could not be captured consistently."));
        return false;
    }
    if (!ValidateCarryComponentSnapshots(
        OutSave->Items, OutSave->PhysicalActors, OutSave->CarryComponents))
    {
        // PhysicalActors is captured below. Validate identity/ownership now,
        // then repeat after the full actor manifest has been captured.
        for (const FWorldOwnedItemRecord& Item : OutSave->Items)
        {
            if (Item.OwnerKind == EWorldItemOwnerKind::Held &&
                !OutSave->CarryComponents.ContainsByPredicate([&Item](const FWorldCarryComponentSnapshot& Carry)
                {
                    return Carry.HeldItemId == Item.ItemId && Carry.CarryOwnerId == Item.OwnerId;
                }))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("World save refused: held item %s has no matching carry component."),
                    *Item.ItemId.ToString());
                return false;
            }
        }
    }
    Storage->ExportSnapshot(OutSave->StorageContainers, OutSave->StorageReservations);
    if (!CapturePhysicalActorSnapshots(World, OutSave->PhysicalActors) ||
        !ValidatePhysicalActorSnapshotSet(World, OutSave->PhysicalActors) ||
        !ValidateCarryComponentSnapshots(
            OutSave->Items, OutSave->PhysicalActors, OutSave->CarryComponents) ||
        !ReconcilePhysicalActorSnapshotsWithItems(
            OutSave->PhysicalActors, OutSave->Items, OutSave->StorageContainers,
            OutSave->CarryComponents))
    {
        UE_LOG(LogTemp, Warning, TEXT("World save refused because physical actor state could not be reconciled with item ownership."));
        return false;
    }
    UWorldStructuralGraphSubsystem* StructuralGraph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!StructuralGraph || !StructuralGraph->ExportSnapshot(OutSave->StructuralGraph))
    {
        UE_LOG(LogTemp, Warning, TEXT("World save refused because structural graph snapshot export failed."));
        return false;
    }
    OutSave->Inventories.Reset();
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (UInventoryCraftingComponent* Inventory = It->FindComponentByClass<UInventoryCraftingComponent>())
        {
            FWorldInventorySnapshot& Record = OutSave->Inventories.AddDefaulted_GetRef();
            Inventory->ExportSnapshot(Record.OwnerActorId, Record.InventoryOwnerId,
                Record.Items, Record.QuickAccessItemId);
        }
    }
    Terrain->ExportSnapshot(OutSave->TerrainCells, OutSave->TerrainReceipts);
    Agriculture->ExportSnapshot(OutSave->Crops, OutSave->Animals);
    Survival->ExportSnapshot(OutSave->SurvivalProfiles, OutSave->Discoveries,
        OutSave->Weather, OutSave->WeatherSeverity);
    Production->ExportSnapshot(OutSave->ProductionStations, OutSave->ProductionRecipes);
    Cargo->ExportSnapshot(OutSave->Cargo);
    if (!ValidateProductionReceiptItemReferences(OutSave->Items, OutSave->ProductionStations))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World save refused because production receipts do not match authoritative item ownership records."));
        return false;
    }
    // Validate the captured manifest without mutating live subsystem state.
    // Local domain RestoreSnapshot methods still validate again at commit time.
    if (!ValidateSnapshotPayloadPreflight(World, *OutSave))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World save refused because its cross-domain snapshot preflight failed."));
        return false;
    }
    return true;
}

bool UWorldSaveCoordinatorSubsystem::ApplySnapshot(const UWorldSimulationSaveGame* Snapshot)
{
    UWorld* World = GetWorld();
    if (!World || !Snapshot || Snapshot->SchemaVersion != UWorldSimulationSaveGame::CurrentSchemaVersion)
        return false;
    UWorldSimulationClockSubsystem* Clock = World->GetSubsystem<UWorldSimulationClockSubsystem>();
    UWorldThreatDirectorSubsystem* Threat = World->GetSubsystem<UWorldThreatDirectorSubsystem>();
    UWorldItemOwnershipSubsystem* Items = World->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldTerrainStateSubsystem* Terrain = World->GetSubsystem<UWorldTerrainStateSubsystem>();
    UWorldAgricultureSubsystem* Agriculture = World->GetSubsystem<UWorldAgricultureSubsystem>();
    UWorldSurvivalSubsystem* Survival = World->GetSubsystem<UWorldSurvivalSubsystem>();
    UWorldProductionSubsystem* Production = World->GetSubsystem<UWorldProductionSubsystem>();
    UWorldCargoTransportSubsystem* Cargo = World->GetSubsystem<UWorldCargoTransportSubsystem>();
    UWorldStorageSubsystem* Storage = World->GetSubsystem<UWorldStorageSubsystem>();
    UWorldOperationLedgerSubsystem* OperationLedger = World->GetSubsystem<UWorldOperationLedgerSubsystem>();
    UWorldStructuralGraphSubsystem* StructuralGraph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Clock || !Threat || !Items || !Terrain || !Agriculture || !Survival || !Production || !Cargo || !Storage || !OperationLedger || !StructuralGraph)
        return false;

    // Reject malformed cross-domain references before clock, ownership or actor state mutates.
    if (!ValidateSnapshotPayloadPreflight(World, *Snapshot))
    {
        UE_LOG(LogTemp, Warning, TEXT("World load refused by cross-domain snapshot preflight."));
        return false;
    }

    if (!ValidateProductionReceiptItemReferences(Snapshot->Items, Snapshot->ProductionStations))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World load refused because production receipts do not match the snapshot item ownership ledger."));
        return false;
    }
    if (!ValidatePhysicalActorSnapshotSet(World, Snapshot->PhysicalActors))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World load refused because physical actor identities, classes or component state do not match this level."));
        return false;
    }
    TArray<FWorldOwnedItemRecord> ValidatedPhysicalItems = Snapshot->Items;
    if (!ValidateCarryComponentSnapshots(
            Snapshot->Items, Snapshot->PhysicalActors, Snapshot->CarryComponents) ||
        !ReconcilePhysicalActorSnapshotsWithItems(
            Snapshot->PhysicalActors, ValidatedPhysicalItems, Snapshot->StorageContainers,
            Snapshot->CarryComponents))
        return false;

    // Durable saves must never restore a half-finished operation. Runtime rollback
    // may restore Pending entries, but save/load only accepts Completed records.
    for (const FWorldOperationLedgerEntry& Entry : Snapshot->OperationLedgerEntries)
    {
        if (Entry.State != EWorldOperationLedgerEntryState::Completed)
            return false;
    }

    // Suppress ordinary EndPlay ownership transfers while reconciling the actor
    // set. The snapshot's ledgers are authoritative and will be restored directly.
    TGuardValue<bool> ApplyingSnapshotGuard(bApplyingSnapshot, true);
    ClearCarryComponentsForSnapshotRestore(World);

    // Restore order is deliberate: logical time/seed first, then world state,
    // ownership, simulation domains, and cargo references last.
    if (!Clock->RestoreSnapshot(Snapshot->Clock)) return false;
    if (!Items->RestoreSnapshot(ValidatedPhysicalItems, Snapshot->ItemTransferReceipts, Snapshot->ItemSplitReceipts)) return false;
    if (!OperationLedger->RestoreSnapshot(Snapshot->OperationLedgerEntries)) return false;
    if (!ApplyPhysicalActorSnapshots(World, Snapshot->PhysicalActors))
    {
        UE_LOG(LogTemp, Warning, TEXT("World load failed while applying physical actor state."));
        return false;
    }
    if (!StructuralGraph->RestoreSnapshot(Snapshot->StructuralGraph))
    {
        UE_LOG(LogTemp, Warning, TEXT("World load failed because the structural graph snapshot could not be restored safely."));
        return false;
    }
    if (!StructuralGraph->RebuildLogActorAttachmentsFromSnapshot())
    {
        UE_LOG(LogTemp, Warning, TEXT("World load failed while rebuilding actor-local log snap links from the canonical structural graph."));
        return false;
    }

    TArray<UInventoryCraftingComponent*> CurrentInventories;
    TMap<FGuid, UInventoryCraftingComponent*> ByOwnerActorId;
    TMap<FGuid, UInventoryCraftingComponent*> ByInventoryOwnerId;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (UInventoryCraftingComponent* Inventory = It->FindComponentByClass<UInventoryCraftingComponent>())
        {
            CurrentInventories.Add(Inventory);
            if (UWorldObjectIdentityComponent* Identity = It->FindComponentByClass<UWorldObjectIdentityComponent>())
                if (Identity->HasValidIdentity()) ByOwnerActorId.Add(Identity->GetInstanceId(), Inventory);
            if (Inventory->GetInventoryOwnerId().IsValid())
                ByInventoryOwnerId.Add(Inventory->GetInventoryOwnerId(), Inventory);
        }
    }
    if (CurrentInventories.Num() != Snapshot->Inventories.Num()) return false;
    TSet<UInventoryCraftingComponent*> RestoredInventories;
    for (const FWorldInventorySnapshot& Record : Snapshot->Inventories)
    {
        UInventoryCraftingComponent* Inventory = nullptr;
        if (Record.OwnerActorId.IsValid()) ByOwnerActorId.RemoveAndCopyValue(Record.OwnerActorId, Inventory);
        if (!Inventory && Record.InventoryOwnerId.IsValid()) ByInventoryOwnerId.RemoveAndCopyValue(Record.InventoryOwnerId, Inventory);
        if (!Inventory || RestoredInventories.Contains(Inventory)) return false;
        const FGuid PreviousInventoryOwnerId = Inventory->GetInventoryOwnerId();
        if (Record.OwnerActorId.IsValid()) ByOwnerActorId.Remove(Record.OwnerActorId);
        ByInventoryOwnerId.Remove(PreviousInventoryOwnerId);
        ByInventoryOwnerId.Remove(Record.InventoryOwnerId);
        if (!Inventory->RestoreSnapshot(Record.InventoryOwnerId, Record.Items, Record.QuickAccessItemId))
            return false;
        RestoredInventories.Add(Inventory);
    }
    if (RestoredInventories.Num() != CurrentInventories.Num()) return false;

    if (!Terrain->RestoreSnapshot(Snapshot->TerrainCells, Snapshot->TerrainReceipts)) return false;
    if (!Agriculture->RestoreSnapshot(Snapshot->Crops, Snapshot->Animals)) return false;
    if (!Survival->RestoreSnapshot(Snapshot->SurvivalProfiles, Snapshot->Discoveries,
        Snapshot->Weather, Snapshot->WeatherSeverity)) return false;
    if (!Production->RestoreSnapshot(Snapshot->ProductionStations, Snapshot->ProductionRecipes)) return false;
    if (!Storage->RestoreSnapshot(Snapshot->StorageContainers, Snapshot->StorageReservations)) return false;

    // Container actor Stored arrays are runtime-only pointers, not durable
    // ownership. Rebuild them from the restored item ledger and storage manifest.
    for (TActorIterator<ALogContainerActor> It(World); It; ++It)
    {
        if (!It->RebuildStoredContentsFromOwnership())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("World load failed while rebuilding physical storage contents from ownership."));
            return false;
        }
    }

    if (!Cargo->RestoreSnapshot(Snapshot->Cargo)) return false;
    if (!Threat->RestoreSnapshot(Snapshot->Threat)) return false;
    if (!RestoreCarryComponentSnapshots(World, Snapshot->CarryComponents))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World load failed while restoring held-item/carry-component relationships."));
        return false;
    }
    return true;
}

bool UWorldSaveCoordinatorSubsystem::SaveWorldState(const FString& SlotName, int32 UserIndex)
{
    if (SlotName.IsEmpty() || UserIndex < 0) return false;
    UWorldSimulationSaveGame* Save = Cast<UWorldSimulationSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UWorldSimulationSaveGame::StaticClass()));
    if (!Save || !CaptureCurrentState(Save)) return false;
    return UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex);
}

bool UWorldSaveCoordinatorSubsystem::LoadWorldState(const FString& SlotName, int32 UserIndex)
{
    if (SlotName.IsEmpty() || UserIndex < 0 || !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
        return false;
    UWorldSimulationSaveGame* Candidate = Cast<UWorldSimulationSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    if (!Candidate) return false;

    // Preserve the on-disk version before migration. Schemas before v12 do not
    // contain a trustworthy identity-bound physical actor manifest; with
    // dynamic reconciliation an empty legacy manifest must never mean "delete
    // every physical actor in the current world".
    const int32 OriginalSchemaVersion = Candidate->SchemaVersion;
    if (OriginalSchemaVersion < 12)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("World load refused: save schema %d has no complete physical actor manifest."),
            OriginalSchemaVersion);
        return false;
    }

    // Only schema v12+ can be restored: older saves cannot reconstruct the
    // historical identity-bound physical actor population. Remaining migrations
    // normalize schemas that already contain that required manifest.
    if (Candidate->SchemaVersion == 12)
    {
        // Schema v13 persists structural graph edges, anchors, loads, damage and revisions.
        // Missing historical graph data is not synthesized from the currently loaded world.
        Candidate->SchemaVersion = 13;
    }
    if (Candidate->SchemaVersion == 13)
    {
        // Schema v14 persists terrain edit idempotency receipts. Earlier saves have no
        // receipt history, so migrate them with an empty receipt list.
        Candidate->TerrainReceipts.Reset();
        Candidate->SchemaVersion = 14;
    }
    if (Candidate->SchemaVersion == 14)
    {
        // Schema v15 adds a monotonic sequence to ownership transfer/split receipts.
        // Older receipts are deterministically ordered by operation ID so that the
        // bounded receipt window can be restored without inventing a chronology.
        Candidate->ItemTransferReceipts.Sort([](const FWorldItemTransferReceipt& A,
            const FWorldItemTransferReceipt& B)
        {
            return A.OperationId.ToString() < B.OperationId.ToString();
        });
        int64 ReceiptSequence = 1;
        for (FWorldItemTransferReceipt& Receipt : Candidate->ItemTransferReceipts)
            Receipt.Sequence = ReceiptSequence++;

        Candidate->ItemSplitReceipts.Sort([](const FWorldItemSplitReceipt& A,
            const FWorldItemSplitReceipt& B)
        {
            return A.OperationId.ToString() < B.OperationId.ToString();
        });
        for (FWorldItemSplitReceipt& Receipt : Candidate->ItemSplitReceipts)
            Receipt.Sequence = ReceiptSequence++;

        Candidate->SchemaVersion = 15;
    }
    if (Candidate->SchemaVersion == 15)
    {
        // Schema v16 adds the persisted raid attack-pulse watermark and freezes the
        // interval used to map simulation time to pulse indices. Before the adapter
        // was wired into the fixed-step coordinator, these saves could not contain
        // committed adapter pulses; initialize the sentinel and default interval.
        Candidate->Threat.SchemaVersion = 2;
        Candidate->Threat.CurrentRaid.LastCommittedAttackPulseIndex = -1;
        if (Candidate->Threat.CurrentRaid.State != EThreatRaidState::None)
            Candidate->Threat.CurrentRaid.AttackIntervalSeconds = 30.0;
        for (FThreatRaidRecord& Raid : Candidate->Threat.RaidHistory)
        {
            Raid.LastCommittedAttackPulseIndex = -1;
            Raid.AttackIntervalSeconds = 30.0;
        }
        Candidate->SchemaVersion = 16;
    }
    if (Candidate->SchemaVersion == 16)
    {
        // Schema v17 persists component-local transforms. Older schemas never
        // captured them, so migrate deterministically from the saved actor
        // class default. Runtime-only custom child offsets/scales from a legacy
        // save cannot be reconstructed because those values were not recorded.
        for (FWorldPhysicalActorSnapshot& ActorState : Candidate->PhysicalActors)
        {
            UClass* ActorClass = ActorState.ActorClass.TryLoadClass<AActor>();
            AActor* ClassDefault = ActorClass
                ? Cast<AActor>(ActorClass->GetDefaultObject())
                : nullptr;
            if (!IsValid(ClassDefault))
                return false;

            TArray<UPrimitiveComponent*> DefaultPrimitives;
            ClassDefault->GetComponents<UPrimitiveComponent>(DefaultPrimitives);
            if (DefaultPrimitives.Num() != ActorState.Components.Num())
                return false;

            for (FWorldPhysicalComponentSnapshot& ComponentState : ActorState.Components)
            {
                UPrimitiveComponent* DefaultPrimitive = nullptr;
                for (UPrimitiveComponent* CandidatePrimitive : DefaultPrimitives)
                {
                    if (IsValid(CandidatePrimitive) &&
                        CandidatePrimitive->GetFName() == ComponentState.ComponentName)
                    {
                        DefaultPrimitive = CandidatePrimitive;
                        break;
                    }
                }
                if (!DefaultPrimitive ||
                    FSoftClassPath(DefaultPrimitive->GetClass()) != ComponentState.ComponentClass)
                    return false;

                ComponentState.ActorRelativeTransform =
                    DefaultPrimitive == ClassDefault->GetRootComponent()
                    ? FTransform::Identity
                    : DefaultPrimitive->GetRelativeTransform();
            }
        }
        Candidate->SchemaVersion = 17;
    }

    if (Candidate->SchemaVersion == 17)
    {
        // Schema v18 adds explicit carry-component/held-item relationships.
        // Schema v17 capture already refused Held items, so this can be empty.
        Candidate->CarryComponents.Reset();
        Candidate->SchemaVersion = 18;
    }

    if (Candidate->SchemaVersion != UWorldSimulationSaveGame::CurrentSchemaVersion)
        return false;

    UWorldSimulationSaveGame* Before = Cast<UWorldSimulationSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UWorldSimulationSaveGame::StaticClass()));
    if (!Before || !CaptureCurrentState(Before)) return false;
    if (ApplySnapshot(Candidate)) return true;

    // Domain RestoreSnapshot methods validate their own replacement before
    // swapping that domain, but the world has no single cross-domain atomic commit.
    // If a later domain rejects a payload, attempt to restore the pre-load manifest;
    // if that compensation also fails, the simulation is faulted below.
    if (!ApplySnapshot(Before))
    {
        const FString FaultReason = TEXT(
            "World save load failed and rollback also failed; authoritative world state may be mixed. Stop simulation and recover/reset the world.");
        UE_LOG(LogTemp, Error, TEXT("%s"), *FaultReason);
        if (UWorld* World = GetWorld())
        {
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(FaultReason);
            }
        }
    }
    return false;
}
