#include "WorldSaveCoordinatorSubsystem.h"

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
    const TArray<FWorldStorageContainerRecord>& Storages)
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
        if ((!bWorldOwned && !bStorageOwned) || Item.Quantity != 1 ||
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
    // Held actor/carry-component state is not yet part of this manifest. Refuse
    // a lossy save rather than restoring an item to an orphaned Held owner.
    for (const FWorldOwnedItemRecord& Item : OutSave->Items)
    {
        if (Item.OwnerKind == EWorldItemOwnerKind::Held)
        {
            UE_LOG(LogTemp, Warning, TEXT("World save refused while item %s is held; drop or store it first."),
                *Item.ItemId.ToString());
            return false;
        }
    }
    Storage->ExportSnapshot(OutSave->StorageContainers, OutSave->StorageReservations);
    if (!CapturePhysicalActorSnapshots(World, OutSave->PhysicalActors) ||
        !ReconcilePhysicalActorSnapshotsWithItems(
            OutSave->PhysicalActors, OutSave->Items, OutSave->StorageContainers))
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
            if (!Inventory->RestoreSnapshot(Record.InventoryOwnerId, Record.Items, Record.QuickAccessItemId))
                return false;
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
    // Refuse a save that would restore orphaned cargo or exceed storage capacity.
    if (!Storage->RestoreSnapshot(OutSave->StorageContainers, OutSave->StorageReservations) ||
        !Cargo->RestoreSnapshot(OutSave->Cargo))
        return false;
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
    if (!ReconcilePhysicalActorSnapshotsWithItems(
        Snapshot->PhysicalActors, ValidatedPhysicalItems, Snapshot->StorageContainers))
        return false;

    // Durable saves must never restore a half-finished operation. Runtime rollback
    // may restore Pending entries, but save/load only accepts Completed records.
    for (const FWorldOperationLedgerEntry& Entry : Snapshot->OperationLedgerEntries)
    {
        if (Entry.State != EWorldOperationLedgerEntryState::Completed)
            return false;
    }

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
    if (!Cargo->RestoreSnapshot(Snapshot->Cargo)) return false;
    if (!Threat->RestoreSnapshot(Snapshot->Threat)) return false;
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

    // Schema v5 adds the optional ownership-transfer receipt snapshot. A v4
    // save has the complete item ledger but no receipt array; migrate it with
    // an empty receipt list so state restore remains safe and old revisions
    // are still rejected by the ownership revision guard.
    if (Candidate->SchemaVersion == 4)
    {
        Candidate->ItemTransferReceipts.Reset();
        Candidate->SchemaVersion = 5;
    }
    if (Candidate->SchemaVersion == 5)
    {
        Candidate->OperationLedgerEntries.Reset();
        Candidate->SchemaVersion = 6;
    }
    if (Candidate->SchemaVersion == 6)
    {
        Candidate->ItemSplitReceipts.Reset();
        Candidate->SchemaVersion = 7;
    }
    if (Candidate->SchemaVersion == 7)
    {
        // Schema v8 adds MeshVariantSetId to authoritative item records. Older
        // saves deserialize that optional FName as NAME_None; no filename-based
        // or unverified nominal-size inference is performed during migration.
        Candidate->SchemaVersion = 8;
    }
    if (Candidate->SchemaVersion == 8)
    {
        // Schema v9 adds durable physical-item → production-stock intake receipts
        // inside station snapshots. Older station records start with an empty array.
        Candidate->SchemaVersion = 9;
    }
    if (Candidate->SchemaVersion == 9)
    {
        // Schema v10 adds durable enqueue/collect receipts to production station snapshots.
        // Older snapshots migrate with empty operation receipt arrays.
        Candidate->SchemaVersion = 10;
    }
    if (Candidate->SchemaVersion == 10)
    {
        // Schema v11 adds durable materialization receipts mapping station stock
        // units to their generated physical world item IDs.
        Candidate->SchemaVersion = 11;
    }
    if (Candidate->SchemaVersion == 11)
    {
        // Schema v12 adds identity-bound actor transform/component physics snapshots.
        // Historical physical state cannot be inferred from the currently loaded level.
        // ApplySnapshot therefore fails closed when an older save lacks required actors.
        Candidate->SchemaVersion = 12;
    }
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
    if (Candidate->SchemaVersion != UWorldSimulationSaveGame::CurrentSchemaVersion)
        return false;

    UWorldSimulationSaveGame* Before = Cast<UWorldSimulationSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UWorldSimulationSaveGame::StaticClass()));
    if (!Before || !CaptureCurrentState(Before)) return false;
    if (ApplySnapshot(Candidate)) return true;

    // Every subsystem validates its replacement in local temporaries before
    // swapping state. If a later domain rejects a payload, restore the captured
    // pre-load manifest so callers do not continue with a mixed-world snapshot.
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
