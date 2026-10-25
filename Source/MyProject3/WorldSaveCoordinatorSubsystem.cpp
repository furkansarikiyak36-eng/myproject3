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
    TArray<FWorldOwnedItemRecord>& Items)
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
        if (Item.OwnerKind != EWorldItemOwnerKind::World ||
            Item.Quantity != 1 ||
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

    if (CurrentActors.Num() != Snapshot.Num())
        return false;

    TSet<FGuid> Seen;
    for (const FWorldPhysicalActorSnapshot& State : Snapshot)
    {
        AActor* const* Found = CurrentActors.Find(State.IdentityId);
        if (!State.IdentityId.IsValid() || Seen.Contains(State.IdentityId) || !Found ||
            !IsValid(*Found) || State.ActorClass.IsNull() ||
            FSoftClassPath((*Found)->GetClass()) != State.ActorClass ||
            !State.ActorTransform.IsValid() ||
            static_cast<uint8>(State.LifecycleState) >
                static_cast<uint8>(EWorldObjectLifecycleState::Destroyed) ||
            State.StateRevision < 0)
        {
            return false;
        }
        Seen.Add(State.IdentityId);

        TSet<FName> ComponentNames;
        for (const FWorldPhysicalComponentSnapshot& Component : State.Components)
        {
            if (Component.ComponentName.IsNone() || ComponentNames.Contains(Component.ComponentName) ||
                !Component.bIsPrimitive || Component.ComponentClass.IsNull() ||
                static_cast<uint8>(Component.CollisionEnabled.GetValue()) >
                    static_cast<uint8>(ECollisionEnabled::QueryAndPhysics) ||
                !Component.LinearVelocity.IsFinite() || !Component.AngularVelocityRadians.IsFinite())
            {
                return false;
            }
            bool bFoundNamedComponent = false;
            TArray<UPrimitiveComponent*> Primitives;
            (*Found)->GetComponents<UPrimitiveComponent>(Primitives);
            for (UPrimitiveComponent* Primitive : Primitives)
            {
                if (IsValid(Primitive) && Primitive->GetFName() == Component.ComponentName)
                {
                    bFoundNamedComponent = true;
                    if (FSoftClassPath(Primitive->GetClass()) != Component.ComponentClass)
                        return false;
                    if (!Component.StaticMeshAsset.IsNull())
                    {
                        if (!Cast<UStaticMeshComponent>(Primitive) ||
                            (!Component.StaticMeshAsset.IsValid() && !Component.StaticMeshAsset.TryLoad()))
                            return false;
                    }
                    break;
                }
            }
            if (!bFoundNamedComponent)
                return false;
            ComponentNames.Add(Component.ComponentName);
        }
    }
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

    // Apply transforms first so the following component physics state is applied
    // at the saved location. Actor spawning is intentionally not guessed here.
    for (const FWorldPhysicalActorSnapshot& State : Snapshot)
    {
        AActor* Actor = CurrentActors.FindRef(State.IdentityId);
        if (!Actor || !Actor->SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics))
            return false;
        Actor->SetActorHiddenInGame(State.bActorHiddenInGame);
        Actor->SetActorEnableCollision(State.bActorCollisionEnabled);
        Actor->SetActorTickEnabled(State.bActorTickEnabled);
    }

    for (const FWorldPhysicalActorSnapshot& State : Snapshot)
    {
        AActor* Actor = CurrentActors.FindRef(State.IdentityId);
        UWorldObjectIdentityComponent* Identity =
            Actor ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        if (!Actor || !Identity)
            return false;

        {
            FMemoryReader Reader(State.ActorSaveGameData, true);
            FObjectAndNameAsStringProxyArchive ActorArchive(Reader, true);
            ActorArchive.ArIsSaveGame = true;
            Actor->Serialize(ActorArchive);
            if (Reader.IsError())
                return false;
        }
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
            if (!Primitive)
                return false;

            if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Primitive))
            {
                if (!ComponentState.StaticMeshAsset.IsNull())
                {
                    UStaticMesh* MeshAsset = Cast<UStaticMesh>(ComponentState.StaticMeshAsset.TryLoad());
                    if (!MeshAsset)
                        return false;
                    StaticMesh->SetStaticMesh(MeshAsset);
                }
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
    if (!CapturePhysicalActorSnapshots(World, OutSave->PhysicalActors) ||
        !ReconcilePhysicalActorSnapshotsWithItems(OutSave->PhysicalActors, OutSave->Items))
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
    Terrain->ExportSnapshot(OutSave->TerrainCells);
    Agriculture->ExportSnapshot(OutSave->Crops, OutSave->Animals);
    Survival->ExportSnapshot(OutSave->SurvivalProfiles, OutSave->Discoveries,
        OutSave->Weather, OutSave->WeatherSeverity);
    Production->ExportSnapshot(OutSave->ProductionStations, OutSave->ProductionRecipes);
    Cargo->ExportSnapshot(OutSave->Cargo);
    Storage->ExportSnapshot(OutSave->StorageContainers, OutSave->StorageReservations);
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
    if (!ReconcilePhysicalActorSnapshotsWithItems(Snapshot->PhysicalActors, ValidatedPhysicalItems))
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

    if (!Terrain->RestoreSnapshot(Snapshot->TerrainCells)) return false;
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
        UE_LOG(LogTemp, Error,
            TEXT("World save load failed and rollback also failed. World state may be mixed; stop simulation and capture diagnostics."));
    }
    return false;
}
