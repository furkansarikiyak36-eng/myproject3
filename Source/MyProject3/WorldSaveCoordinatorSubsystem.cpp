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
#include "EngineUtils.h"
#include "Logging/LogMacros.h"

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
    if (!Clock || !Threat || !Items || !Terrain || !Agriculture || !Survival || !Production || !Cargo || !Storage || !OperationLedger)
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
    if (!Items->RestoreSnapshot(Snapshot->Items, Snapshot->ItemTransferReceipts, Snapshot->ItemSplitReceipts)) return false;
    if (!OperationLedger->RestoreSnapshot(Snapshot->OperationLedgerEntries)) return false;

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
