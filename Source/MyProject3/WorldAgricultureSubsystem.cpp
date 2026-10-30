#include "WorldAgricultureSubsystem.h"
#include "PhysicalHealthComponent.h"
#include "WorldSimulationClockSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "InventoryCraftingComponent.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "ShelterSensorComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WildlifeActor.h"
#include "WorldObjectIdentityComponent.h"

const FWorldCropDefinition* UWorldAgricultureSubsystem::FindCropDefinition(FName CropDefinitionId) const
{
    if (CropDefinitionId.IsNone())
        return nullptr;
    return CropDefinitions.FindByPredicate([CropDefinitionId](const FWorldCropDefinition& Definition)
    {
        return Definition.CropDefinitionId == CropDefinitionId;
    });
}

const FWorldFieldActionReceipt* UWorldAgricultureSubsystem::FindFieldActionReceipt(const FGuid& OperationId) const
{
    if (!OperationId.IsValid())
        return nullptr;
    for (const TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
    {
        const FWorldFieldActionReceipt* Found = Pair.Value.ActionReceipts.FindByPredicate(
            [&OperationId](const FWorldFieldActionReceipt& Receipt) { return Receipt.OperationId == OperationId; });
        if (Found)
            return Found;
    }
    return nullptr;
}

bool UWorldAgricultureSubsystem::GetFieldCell(const FIntVector& Cell, FWorldFieldCellState& OutCell) const
{
    const FWorldFieldCellState* Found = FieldCells.Find(Cell);
    if (!Found)
        return false;
    OutCell = *Found;
    return true;
}

bool UWorldAgricultureSubsystem::TryApplyFieldAction(
    const FGuid& OperationId, const FIntVector& Cell, EWorldFieldActionKind Action,
    int64 ExpectedRevision, float Amount)
{
    if (!OperationId.IsValid() || ExpectedRevision < 0 ||
        static_cast<uint8>(Action) > static_cast<uint8>(EWorldFieldActionKind::Fence) ||
        !FMath::IsFinite(Amount) || Amount < 0.0f || Amount > 1.0f ||
        MaxFieldCells <= 0 || MaxFieldCells > 65536 ||
        MaxFieldActionReceiptsPerCell <= 0 || MaxFieldActionReceiptsPerCell > 512)
        return false;

    if (const FWorldFieldActionReceipt* ExistingReceipt = FindFieldActionReceipt(OperationId))
    {
        return ExistingReceipt->Cell == Cell && ExistingReceipt->Action == Action &&
            ExistingReceipt->ExpectedRevision == ExpectedRevision &&
            FMath::IsNearlyEqual(ExistingReceipt->Amount, Amount, 0.0001f);
    }

    FWorldFieldCellState* Existing = FieldCells.Find(Cell);
    const int64 CurrentRevision = Existing ? Existing->Revision : 0;
    if (CurrentRevision != ExpectedRevision || CurrentRevision >= MAX_int64 - 1 ||
        (Existing && Existing->ActionReceipts.Num() >= MaxFieldActionReceiptsPerCell) ||
        (!Existing && FieldCells.Num() >= MaxFieldCells))
        return false;

    const bool bNeedsAmount = Action == EWorldFieldActionKind::Fertilize ||
        Action == EWorldFieldActionKind::Water || Action == EWorldFieldActionKind::Weed ||
        Action == EWorldFieldActionKind::Mulch;
    if (bNeedsAmount && Amount <= 0.0f)
        return false;

    FWorldFieldCellState Updated;
    if (Existing)
        Updated = *Existing;
    else
        Updated.Cell = Cell;

    switch (Action)
    {
    case EWorldFieldActionKind::Clear:
        Updated.bCleared = true;
        Updated.WeedPressure = 0.0f;
        break;
    case EWorldFieldActionKind::Till:
        if (!Updated.bCleared)
            return false;
        Updated.bTilled = true;
        break;
    case EWorldFieldActionKind::Fertilize:
        if (!Updated.bCleared || !Updated.bTilled)
            return false;
        Updated.Fertility = FMath::Clamp(Updated.Fertility + Amount, 0.0f, 1.0f);
        break;
    case EWorldFieldActionKind::Water:
        Updated.Moisture = FMath::Clamp(Updated.Moisture + Amount, 0.0f, 1.0f);
        break;
    case EWorldFieldActionKind::Weed:
        if (!Updated.bCleared || !Updated.bTilled)
            return false;
        Updated.WeedPressure = FMath::Max(0.0f, Updated.WeedPressure - Amount);
        break;
    case EWorldFieldActionKind::Mulch:
        if (!Updated.bCleared || !Updated.bTilled)
            return false;
        Updated.Mulch = FMath::Clamp(Updated.Mulch + Amount, 0.0f, 1.0f);
        break;
    case EWorldFieldActionKind::Fence:
        Updated.bFenced = true;
        break;
    default:
        return false;
    }

    Updated.Cell = Cell;
    Updated.Revision = CurrentRevision + 1;
    FWorldFieldActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Cell = Cell;
    Receipt.Action = Action;
    Receipt.ExpectedRevision = CurrentRevision;
    Receipt.ResultingRevision = Updated.Revision;
    Receipt.Amount = Amount;
    Updated.ActionReceipts.Add(Receipt);
    FieldCells.Add(Cell, MoveTemp(Updated));
    return true;
}

bool UWorldAgricultureSubsystem::RegisterFieldActionDefinition(
    const FWorldFieldActionDefinition& Definition)
{
    if (Definition.Id.IsNone() ||
        static_cast<uint8>(Definition.Action) > static_cast<uint8>(EWorldFieldActionKind::Fence) ||
        !FMath::IsFinite(Definition.Amount) || Definition.Amount < 0.0f || Definition.Amount > 1.0f ||
        !FMath::IsFinite(Definition.LaborSeconds) || Definition.LaborSeconds < 1.0 ||
        Definition.LaborSeconds > 3600.0 ||
        MaxFieldActionDefinitions < 1 || MaxFieldActionDefinitions > 16)
        return false;

    const bool bNeedsAmount = Definition.Action == EWorldFieldActionKind::Fertilize ||
        Definition.Action == EWorldFieldActionKind::Water ||
        Definition.Action == EWorldFieldActionKind::Weed ||
        Definition.Action == EWorldFieldActionKind::Mulch;
    if ((bNeedsAmount && Definition.Amount <= 0.0f) ||
        (!bNeedsAmount && Definition.Amount != 0.0f))
        return false;

    if (FieldActionDefinitions.ContainsByPredicate(
        [&Definition](const FWorldFieldActionDefinition& Existing)
        {
            return Existing.Id == Definition.Id || Existing.Action == Definition.Action;
        }) || FieldActionDefinitions.Num() >= MaxFieldActionDefinitions)
        return false;

    TSet<FName> SeenInputs;
    for (const FWorldFieldActionCost& Input : Definition.Inputs)
    {
        if (Input.ItemId.IsNone() || Input.Quantity <= 0 || Input.Quantity > 64 ||
            SeenInputs.Contains(Input.ItemId))
            return false;
        SeenInputs.Add(Input.ItemId);
    }
    TSet<FName> SeenTools;
    for (const FName ToolId : Definition.RequiredTools)
    {
        if (ToolId.IsNone() || SeenTools.Contains(ToolId))
            return false;
        SeenTools.Add(ToolId);
    }

    FieldActionDefinitions.Add(Definition);
    FieldActionDefinitions.Sort([](const FWorldFieldActionDefinition& A,
        const FWorldFieldActionDefinition& B)
    {
        return A.Id.ToString() < B.Id.ToString();
    });
    return true;
}

bool UWorldAgricultureSubsystem::TryApplyFieldActionWithInventory(
    const FGuid& OperationId, UInventoryCraftingComponent* Inventory,
    const FGuid& SurvivalEntityId, const FIntVector& Cell,
    EWorldFieldActionKind Action, int64 ExpectedRevision, FWorldFieldCellState& OutCell)
{
    OutCell = FWorldFieldCellState();
    if (!OperationId.IsValid() || !IsValid(Inventory) || !SurvivalEntityId.IsValid() ||
        ExpectedRevision < 0 ||
        static_cast<uint8>(Action) > static_cast<uint8>(EWorldFieldActionKind::Fence))
        return false;

    const FWorldFieldActionDefinition* Definition =
        FieldActionDefinitions.FindByPredicate([Action](const FWorldFieldActionDefinition& Candidate)
        {
            return Candidate.Action == Action;
        });
    UWorld* World = GetWorld();
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSimulationCoordinatorSubsystem* Coordinator =
        World ? World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>() : nullptr;
    if (!Definition || !World || !Survival || !Ownership)
        return false;

    if (const FWorldFieldActionReceipt* ExistingReceipt = FindFieldActionReceipt(OperationId))
    {
        const bool bSameRequest = ExistingReceipt->Cell == Cell &&
            ExistingReceipt->Action == Action &&
            ExistingReceipt->ExpectedRevision == ExpectedRevision &&
            FMath::IsNearlyEqual(ExistingReceipt->Amount, Definition->Amount, 0.0001f);
        return bSameRequest && GetFieldCell(Cell, OutCell);
    }

    FWorldFieldCellState ExistingCell;
    const bool bCellExists = GetFieldCell(Cell, ExistingCell);
    const int64 CurrentRevision = bCellExists ? ExistingCell.Revision : 0;
    if (CurrentRevision != ExpectedRevision || CurrentRevision >= MAX_int64 - 1 ||
        (bCellExists && ExistingCell.ActionReceipts.Num() >= MaxFieldActionReceiptsPerCell) ||
        (!bCellExists && FieldCells.Num() >= MaxFieldCells) ||
        MaxFieldCells <= 0 || MaxFieldCells > 65536 ||
        MaxFieldActionReceiptsPerCell <= 0 || MaxFieldActionReceiptsPerCell > 512)
        return false;

    FWorldFieldCellState ProposedCell;
    ProposedCell.Cell = Cell;
    if (bCellExists)
        ProposedCell = ExistingCell;
    if ((Action == EWorldFieldActionKind::Till && !ProposedCell.bCleared) ||
        ((Action == EWorldFieldActionKind::Fertilize ||
          Action == EWorldFieldActionKind::Weed ||
          Action == EWorldFieldActionKind::Mulch) &&
         (!ProposedCell.bCleared || !ProposedCell.bTilled)))
        return false;

    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    if (!InventoryOwnerId.IsValid())
        return false;

    for (const FWorldFieldActionCost& Input : Definition->Inputs)
        if (Inventory->GetItemCount(Input.ItemId) < Input.Quantity)
            return false;
    for (const FName ToolId : Definition->RequiredTools)
        if (Inventory->GetItemCount(ToolId) < 1)
            return false;
    if (!Survival->CanRecordActivity(
        SurvivalEntityId, ESurvivalActivity::Build, Definition->LaborSeconds, 0.0f))
        return false;

    FGuid InventoryActorBefore;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(InventoryActorBefore, InventoryOwnerBefore,
        InventoryItemsBefore, QuickAccessBefore);
    if (InventoryOwnerBefore != InventoryOwnerId)
        return false;

    TArray<FWorldOwnedItemRecord> OwnershipItemsBefore;
    TArray<FWorldItemTransferReceipt> OwnershipTransfersBefore;
    TArray<FWorldItemSplitReceipt> OwnershipSplitsBefore;
    Ownership->ExportSnapshot(OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);

    TArray<FWorldCropState> CropsBefore;
    TArray<FWorldAnimalState> AnimalsBefore;
    TArray<FWorldFieldCellState> FieldCellsBefore;
    TArray<FWorldAnimalPenState> AnimalPensBefore;
    ExportSnapshot(CropsBefore, AnimalsBefore, FieldCellsBefore, AnimalPensBefore);

    TArray<FWorldSurvivalProfile> ProfilesBefore;
    TArray<FWorldDiscoveryRecord> DiscoveriesBefore;
    EWorldWeatherState WeatherBefore = EWorldWeatherState::Clear;
    float WeatherSeverityBefore = 0.0f;
    Survival->ExportSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);

    auto Rollback = [&]() -> bool
    {
        const bool bFarmRestored =
            RestoreSnapshot(CropsBefore, AnimalsBefore, FieldCellsBefore, AnimalPensBefore);
        const bool bOwnershipRestored =
            Ownership->RestoreSnapshot(OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);
        const bool bInventoryRestored =
            Inventory->RestoreSnapshot(InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        const bool bSurvivalRestored =
            Survival->RestoreSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);
        if (!bFarmRestored || !bOwnershipRestored || !bInventoryRestored || !bSurvivalRestored)
        {
            if (Coordinator)
                Coordinator->MarkSimulationFaulted(
                    TEXT("Field-action rollback failed; stop simulation and recover/reset the world."));
            UE_LOG(LogTemp, Error, TEXT("Field-action rollback failed for operation %s."),
                *OperationId.ToString());
            return false;
        }
        return true;
    };

    for (const FWorldFieldActionCost& Input : Definition->Inputs)
    {
        if (!Inventory->TryRemoveItem(Input.ItemId, Input.Quantity))
        {
            Rollback();
            return false;
        }
    }

    if (!TryApplyFieldAction(OperationId, Cell, Action, ExpectedRevision, Definition->Amount) ||
        !GetFieldCell(Cell, OutCell))
    {
        Rollback();
        OutCell = FWorldFieldCellState();
        return false;
    }

    if (!Survival->RecordActivity(
        SurvivalEntityId, ESurvivalActivity::Build, Definition->LaborSeconds, 0.0f))
    {
        Rollback();
        OutCell = FWorldFieldCellState();
        return false;
    }

    return true;
}

const FWorldAgricultureActionReceipt*
UWorldAgricultureSubsystem::FindActionReceipt(const FGuid& OperationId) const
{
    if (!OperationId.IsValid())
        return nullptr;
    for (const TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        if (const FWorldAgricultureActionReceipt* Found =
            Pair.Value.ActionReceipts.FindByPredicate([OperationId](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.OperationId == OperationId;
            }))
            return Found;
    }
    for (const TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        if (const FWorldAgricultureActionReceipt* Found =
            Pair.Value.ActionReceipts.FindByPredicate([OperationId](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.OperationId == OperationId;
            }))
            return Found;
    }
    return nullptr;
}

AWildlifeActor* UWorldAgricultureSubsystem::FindWildlifeActor(const FGuid& IdentityId) const
{
    if (!IdentityId.IsValid() || !GetWorld())
        return nullptr;

    AWildlifeActor* Found = nullptr;
    for (TActorIterator<AWildlifeActor> It(GetWorld()); It; ++It)
    {
        AWildlifeActor* Candidate = *It;
        if (!IsValid(Candidate) || !IsValid(Candidate->Identity) ||
            Candidate->Identity->GetInstanceId() != IdentityId)
            continue;
        if (Found)
            return nullptr; // Duplicate stable identities are ambiguous; fail closed.
        Found = Candidate;
    }
    return Found;
}

bool UWorldAgricultureSubsystem::RegisterAnimalPen(const FWorldAnimalPenState& Pen)
{
    if (!Pen.PenId.IsValid() || Pen.Location.ContainsNaN() ||
        Pen.Capacity <= 0 || Pen.Capacity > 128 || Pen.Revision <= 0 ||
        !Pen.AnimalIds.IsEmpty() || MaxAnimalPens <= 0 || MaxAnimalPens > 4096)
        return false;

    TSet<FName> AllowedTypes;
    for (const FName DefinitionId : Pen.AllowedAnimalDefinitionIds)
    {
        if (DefinitionId.IsNone() || AllowedTypes.Contains(DefinitionId))
            return false;
        AllowedTypes.Add(DefinitionId);
    }

    if (FWorldAnimalPenState* Existing = AnimalPens.Find(Pen.PenId))
    {
        return Existing->Capacity == Pen.Capacity &&
            Existing->AllowedAnimalDefinitionIds == Pen.AllowedAnimalDefinitionIds &&
            FVector::DistSquared(Existing->Location, Pen.Location) <= 1.0;
    }
    if (AnimalPens.Num() >= MaxAnimalPens)
        return false;

    FWorldAnimalPenState NewPen = Pen;
    NewPen.AnimalIds.Reset();
    NewPen.Revision = FMath::Max<int64>(1, NewPen.Revision);
    AnimalPens.Add(NewPen.PenId, MoveTemp(NewPen));
    return true;
}

bool UWorldAgricultureSubsystem::GetAnimalPen(
    const FGuid& PenId, FWorldAnimalPenState& OutPen) const
{
    const FWorldAnimalPenState* Pen = AnimalPens.Find(PenId);
    if (!Pen)
        return false;
    OutPen = *Pen;
    return true;
}

bool UWorldAgricultureSubsystem::FindInventoryForOwner(
    const FGuid& InventoryOwnerId, UInventoryCraftingComponent*& OutInventory) const
{
    OutInventory = nullptr;
    UWorld* World = GetWorld();
    if (!World || !InventoryOwnerId.IsValid())
        return false;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        UInventoryCraftingComponent* Candidate =
            IsValid(*It) ? It->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
        if (IsValid(Candidate) && Candidate->GetInventoryOwnerId() == InventoryOwnerId)
        {
            if (OutInventory)
            {
                OutInventory = nullptr;
                return false;
            }
            OutInventory = Candidate;
        }
    }
    return OutInventory != nullptr;
}

bool UWorldAgricultureSubsystem::ConsumeInventoryItemAtomically(
    const FGuid& OperationId, const FGuid& ItemId, int64 ExpectedItemRevision,
    const FGuid& InventoryOwnerId, UInventoryCraftingComponent* Inventory)
{
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!World || !Ownership || !Inventory || !OperationId.IsValid() ||
        !ItemId.IsValid() || ExpectedItemRevision <= 0 || !InventoryOwnerId.IsValid() ||
        Inventory->GetInventoryOwnerId() != InventoryOwnerId)
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(ItemId, Item) || Item.Revision != ExpectedItemRevision ||
        Item.OwnerKind != EWorldItemOwnerKind::Inventory || Item.OwnerId != InventoryOwnerId ||
        Item.Quantity != 1 || Item.Revision >= MAX_int64 - 1)
        return false;

    FGuid OwnerActorId;
    FGuid PreviousInventoryOwnerId;
    TArray<FInventoryStack> PreviousInventory;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousInventoryOwnerId,
        PreviousInventory, PreviousQuickAccess);
    if (PreviousInventoryOwnerId != InventoryOwnerId)
        return false;

    bool bFoundStack = false;
    for (const FInventoryStack& Stack : PreviousInventory)
    {
        if (Stack.OwnedWorldItemId == ItemId)
        {
            if (Stack.Quantity != 1 || Stack.ItemId.IsNone())
                return false;
            bFoundStack = true;
            break;
        }
    }
    if (!bFoundStack)
        return false;

    TArray<FWorldOwnedItemRecord> PreviousItems;
    TArray<FWorldItemTransferReceipt> PreviousTransferReceipts;
    TArray<FWorldItemSplitReceipt> PreviousSplitReceipts;
    Ownership->ExportSnapshot(PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);

    auto Rollback = [&]() -> bool
    {
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);
        if (!bInventoryRestored || !bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Agriculture input rollback failed for item %s."),
                *ItemId.ToString());
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Agriculture input rollback failed; stop simulation and recover/reset the world."));
            }
            return false;
        }
        return true;
    };

    FInventoryStack RemovedStack;
    if (!Inventory->TryRemoveOwnedItemReference(ItemId, RemovedStack))
        return false;

    if (!Ownership->TryConsumeItem(OperationId, ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Inventory))
    {
        Rollback();
        return false;
    }
    return true;
}

bool UWorldAgricultureSubsystem::RegisterWorldOutputsAtomically(
    const TArray<FName>& Definitions, const TArray<FVector>& Locations, TArray<FGuid>& OutItemIds)
{
    OutItemIds.Reset();
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!Ownership || Definitions.IsEmpty() || Definitions.Num() != Locations.Num() ||
        Ownership->MaxTrackedItems <= 0 ||
        Definitions.Num() > Ownership->MaxTrackedItems - Ownership->GetTrackedItemCount())
        return false;

    for (int32 Index = 0; Index < Definitions.Num(); ++Index)
    {
        if (Definitions[Index].IsNone() || Locations[Index].ContainsNaN())
            return false;
    }

    TArray<FWorldOwnedItemRecord> BeforeItems;
    TArray<FWorldItemTransferReceipt> BeforeTransfers;
    TArray<FWorldItemSplitReceipt> BeforeSplits;
    Ownership->ExportSnapshot(BeforeItems, BeforeTransfers, BeforeSplits);

    auto Rollback = [&]() -> bool
    {
        if (Ownership->RestoreSnapshot(BeforeItems, BeforeTransfers, BeforeSplits))
            return true;
        UE_LOG(LogTemp, Error, TEXT("Agriculture output rollback failed; world item ledger may be inconsistent."));
        if (UWorldSimulationCoordinatorSubsystem* Coordinator =
            World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
        {
            Coordinator->MarkSimulationFaulted(
                TEXT("Agriculture output rollback failed; stop simulation and recover/reset the world."));
        }
        return false;
    };

    for (int32 Index = 0; Index < Definitions.Num(); ++Index)
    {
        FWorldOwnedItemRecord Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = Definitions[Index];
        Item.BaseDefinitionId = Definitions[Index];
        Item.Quantity = 1;
        Item.OwnerKind = EWorldItemOwnerKind::World;
        Item.WorldLocation = Locations[Index];
        if (!Item.ItemId.IsValid() || !Ownership->RegisterItem(Item))
        {
            Rollback();
            OutItemIds.Reset();
            return false;
        }
        OutItemIds.Add(Item.ItemId);
    }
    return OutItemIds.Num() == Definitions.Num();
}

bool UWorldAgricultureSubsystem::RegisterCropDefinition(const FWorldCropDefinition& Definition)
{
    TSet<int32> SeenPlantMonths;
    for (const int32 Month : Definition.PlantMonths)
    {
        if (Month < 1 || Month > 12 || SeenPlantMonths.Contains(Month))
            return false;
        SeenPlantMonths.Add(Month);
    }
    if (Definition.FoundInZone < 0 || Definition.FoundInZone > 4 ||
        Definition.CropDefinitionId.IsNone() || Definition.SeedDefinitionId.IsNone() ||
        Definition.HarvestDefinitionId.IsNone() || Definition.HarvestUnits <= 0 ||
        Definition.HarvestUnits > 64 || Definition.SeedReturnUnits < 0 ||
        Definition.SeedReturnUnits > 64 || Definition.HarvestUnits + Definition.SeedReturnUnits > 64 ||
        !FMath::IsFinite(Definition.SecondsPerStage) ||
        Definition.SecondsPerStage <= 0.0 ||
        !FMath::IsFinite(Definition.PlantLaborSeconds) || Definition.PlantLaborSeconds < 1.0 ||
        Definition.PlantLaborSeconds > 3600.0 ||
        !FMath::IsFinite(Definition.HarvestLaborSeconds) || Definition.HarvestLaborSeconds < 1.0 ||
        Definition.HarvestLaborSeconds > 3600.0 ||
        !FMath::IsFinite(Definition.SoilExhaustionPerHarvest) ||
        Definition.SoilExhaustionPerHarvest < 0.0f || Definition.SoilExhaustionPerHarvest > 1.0f ||
        !FMath::IsFinite(Definition.MinimumPlantMoisture) ||
        Definition.MinimumPlantMoisture < 0.0f || Definition.MinimumPlantMoisture > 1.0f ||
        MaxCropDefinitions <= 0 || MaxCropDefinitions > 4096 ||
        CropDefinitions.Num() >= MaxCropDefinitions || FindCropDefinition(Definition.CropDefinitionId))
        return false;
    CropDefinitions.Add(Definition);
    return true;
}

bool UWorldAgricultureSubsystem::TryPlantCropWithLabor(
    const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
    int64 ExpectedSeedItemRevision, UInventoryCraftingComponent* Inventory,
    const FGuid& SurvivalEntityId, const FIntVector& PlotCell, float InitialMoisture,
    FGuid& OutCropId)
{
    OutCropId.Invalidate();
    if (!OperationId.IsValid() || CropDefinitionId.IsNone() || !SeedItemId.IsValid() ||
        ExpectedSeedItemRevision <= 0 || !IsValid(Inventory) || !SurvivalEntityId.IsValid())
        return false;

    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationCoordinatorSubsystem* Coordinator =
        World ? World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>() : nullptr;
    const FWorldCropDefinition* Definition = FindCropDefinition(CropDefinitionId);
    if (!World || !Ownership || !Survival || !Definition || !InventoryOwnerId.IsValid() ||
        !FMath::IsFinite(Definition->PlantLaborSeconds) ||
        Definition->PlantLaborSeconds < 1.0 || Definition->PlantLaborSeconds > 3600.0)
        return false;

    // A retry returns the already committed crop without consuming the seed
    // twice or charging labor again.
    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        const FWorldCropState* ExistingCrop = Crops.Find(Existing->EntityId);
        if (Existing->Action != EWorldAgricultureActionKind::PlantCrop ||
            Existing->InputItemId != SeedItemId ||
            Existing->ExpectedItemRevision != ExpectedSeedItemRevision ||
            Existing->ExpectedInventoryOwnerId != InventoryOwnerId ||
            Existing->DefinitionId != CropDefinitionId || !ExistingCrop ||
            ExistingCrop->CropDefinitionId != CropDefinitionId ||
            ExistingCrop->PlotCell != PlotCell ||
            ExistingCrop->SeedItemId != SeedItemId)
            return false;
        OutCropId = Existing->EntityId;
        return true;
    }

    if (!Survival->CanRecordActivity(
        SurvivalEntityId, ESurvivalActivity::Build,
        static_cast<float>(Definition->PlantLaborSeconds), 0.0f))
        return false;

    FGuid InventoryActorBefore;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(InventoryActorBefore, InventoryOwnerBefore,
        InventoryItemsBefore, QuickAccessBefore);
    if (InventoryOwnerBefore != InventoryOwnerId)
        return false;
    (void)InventoryActorBefore;

    TArray<FWorldOwnedItemRecord> OwnershipItemsBefore;
    TArray<FWorldItemTransferReceipt> OwnershipTransfersBefore;
    TArray<FWorldItemSplitReceipt> OwnershipSplitsBefore;
    Ownership->ExportSnapshot(OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);

    TArray<FWorldCropState> CropsBefore;
    TArray<FWorldAnimalState> AnimalsBefore;
    TArray<FWorldFieldCellState> FieldCellsBefore;
    TArray<FWorldAnimalPenState> AnimalPensBefore;
    ExportSnapshot(CropsBefore, AnimalsBefore, FieldCellsBefore, AnimalPensBefore);

    TArray<FWorldSurvivalProfile> ProfilesBefore;
    TArray<FWorldDiscoveryRecord> DiscoveriesBefore;
    EWorldWeatherState WeatherBefore = EWorldWeatherState::Clear;
    float WeatherSeverityBefore = 0.0f;
    Survival->ExportSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);

    auto Rollback = [&]() -> bool
    {
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        const bool bFarmRestored =
            RestoreSnapshot(CropsBefore, AnimalsBefore, FieldCellsBefore, AnimalPensBefore);
        const bool bSurvivalRestored =
            Survival->RestoreSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);
        if (!bOwnershipRestored || !bInventoryRestored || !bFarmRestored || !bSurvivalRestored)
        {
            if (Coordinator)
                Coordinator->MarkSimulationFaulted(
                    TEXT("Crop-planting rollback failed; stop simulation and recover/reset the world."));
            UE_LOG(LogTemp, Error, TEXT("Crop-planting rollback failed for operation %s."),
                *OperationId.ToString());
            return false;
        }
        return true;
    };

    if (!TryPlantCrop(OperationId, CropDefinitionId, SeedItemId,
        ExpectedSeedItemRevision, InventoryOwnerId, PlotCell, InitialMoisture, OutCropId))
    {
        Rollback();
        OutCropId.Invalidate();
        return false;
    }

    if (!Survival->RecordActivity(SurvivalEntityId, ESurvivalActivity::Build,
        static_cast<float>(Definition->PlantLaborSeconds), 0.0f))
    {
        Rollback();
        OutCropId.Invalidate();
        return false;
    }
    return true;
}

bool UWorldAgricultureSubsystem::TryPlantCrop(
    const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
    int64 ExpectedSeedItemRevision, const FGuid& ExpectedInventoryOwnerId,
    const FIntVector& PlotCell, float InitialMoisture, FGuid& OutCropId)
{
    OutCropId.Invalidate();
    if (!OperationId.IsValid() || CropDefinitionId.IsNone() || !SeedItemId.IsValid() ||
        ExpectedSeedItemRevision <= 0 || !ExpectedInventoryOwnerId.IsValid() ||
        !FMath::IsFinite(InitialMoisture) || InitialMoisture < 0.0f || InitialMoisture > 1.0f)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldAgricultureActionKind::PlantCrop ||
            Existing->InputItemId != SeedItemId ||
            Existing->ExpectedItemRevision != ExpectedSeedItemRevision ||
            Existing->ExpectedInventoryOwnerId != ExpectedInventoryOwnerId ||
            Existing->DefinitionId != CropDefinitionId)
            return false;
        if (!Crops.Contains(Existing->EntityId))
            return false;
        OutCropId = Existing->EntityId;
        return true;
    }

    const FWorldCropDefinition* Definition = FindCropDefinition(CropDefinitionId);
    if (!Definition)
        return false;

    // Data-defined crops obey the shared calendar. Empty PlantMonths is reserved
    // for tests/special crops that explicitly permit year-round planting.
    if (!Definition->PlantMonths.IsEmpty())
    {
        const UWorldSimulationClockSubsystem* Clock =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
        FWorldCalendarDate Date;
        if (!Clock || !Clock->GetCalendarDate(Date) || !Definition->PlantMonths.Contains(Date.Month))
            return false;
    }

    const FWorldFieldCellState* Field = FieldCells.Find(PlotCell);
    if (bRequirePreparedFieldForPlanting &&
        (!Field || !Field->bCleared || !Field->bTilled || Field->Fertility <= 0.0f))
        return false;
    const float PlantMoisture = Field ? Field->Moisture : InitialMoisture;
    if (PlantMoisture < Definition->MinimumPlantMoisture ||
        MaxCrops <= 0 || Crops.Num() >= MaxCrops ||
        MaxOperationReceiptsPerEntity <= 0 ||
        MaxOperationReceiptsPerEntity > 256)
        return false;
    for (const TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        const FWorldCropState& ExistingCrop = Pair.Value;
        if (ExistingCrop.PlotCell == PlotCell &&
            ExistingCrop.Stage != ECropGrowthStage::Withered &&
            ExistingCrop.Stage != ECropGrowthStage::Harvested)
            return false;
    }

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    FWorldOwnedItemRecord Seed;
    if (!Ownership || !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        !Ownership->TryGetItem(SeedItemId, Seed) ||
        Seed.Revision != ExpectedSeedItemRevision ||
        Seed.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Seed.OwnerId != ExpectedInventoryOwnerId || Seed.DefinitionId != Definition->SeedDefinitionId ||
        Seed.Quantity != 1 || Seed.Revision >= MAX_int64 - 1)
        return false;

    // Seeds that a discovery explicitly unlocks stay gated at the authoritative
    // planting transaction too; possession of a spoofed/raw seed record is not
    // enough to bypass the player's knowledge ledger.
    const UWorldGameDataSubsystem* GameData =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (GameData && GameData->IsApplied() &&
        GameData->IsDiscoveryGatedDefinition(Definition->SeedDefinitionId))
    {
        const AActor* InventoryActor = Inventory->GetOwner();
        const UShelterSensorComponent* Shelter =
            InventoryActor ? InventoryActor->FindComponentByClass<UShelterSensorComponent>() : nullptr;
        const UWorldSurvivalSubsystem* Survival =
            GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
        if (!Shelter || !Shelter->SurvivalEntityId.IsValid() || !Survival ||
            !Survival->IsDefinitionUnlocked(Shelter->SurvivalEntityId, Definition->SeedDefinitionId))
            return false;
    }

    FWorldCropState Crop;
    Crop.CropId = FGuid::NewGuid();
    Crop.CropDefinitionId = CropDefinitionId;
    Crop.PlotCell = PlotCell;
    Crop.Stage = ECropGrowthStage::Seed;
    Crop.Moisture = PlantMoisture;
    Crop.SecondsPerStage = Definition->SecondsPerStage;
    Crop.Revision = 1;
    Crop.PlantOperationId = OperationId;
    Crop.SeedItemId = SeedItemId;
    Crop.SeedDefinitionId = Definition->SeedDefinitionId;
    Crop.SeedReturnUnits = Definition->SeedReturnUnits;
    Crop.SeedInventoryOwnerId = ExpectedInventoryOwnerId;
    Crop.SeedItemExpectedRevision = ExpectedSeedItemRevision;
    Crop.HarvestDefinitionId = Definition->HarvestDefinitionId;
    Crop.HarvestUnits = Definition->HarvestUnits;

    if (!Crop.CropId.IsValid())
        return false;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::PlantCrop;
    Receipt.EntityId = Crop.CropId;
    Receipt.InputItemId = SeedItemId;
    Receipt.ExpectedInventoryOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedItemRevision = ExpectedSeedItemRevision;
    Receipt.ResultingItemRevision = Seed.Revision + 1;
    Receipt.DefinitionId = CropDefinitionId;
    Receipt.ResultingEntityRevision = Crop.Revision;
    Crop.ActionReceipts.Add(Receipt);

    if (!ConsumeInventoryItemAtomically(OperationId, SeedItemId,
        ExpectedSeedItemRevision, ExpectedInventoryOwnerId, Inventory))
        return false;

    Crops.Add(Crop.CropId, Crop);
    OutCropId = Crop.CropId;
    return true;
}

bool UWorldAgricultureSubsystem::TryHarvestCrop(
    const FGuid& OperationId, const FGuid& CropId, int64 ExpectedCropRevision,
    TArray<FGuid>& OutItemIds)
{
    OutItemIds.Reset();
    if (!OperationId.IsValid() || !CropId.IsValid() || ExpectedCropRevision <= 0)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldAgricultureActionKind::HarvestCrop ||
            Existing->EntityId != CropId ||
            Existing->ExpectedEntityRevision != ExpectedCropRevision)
            return false;
        OutItemIds = Existing->OutputItemIds;
        return !OutItemIds.IsEmpty();
    }

    FWorldCropState* Crop = Crops.Find(CropId);
    const FWorldCropDefinition* CropDefinition = Crop ? FindCropDefinition(Crop->CropDefinitionId) : nullptr;
    // A crop without a data definition can still be harvested; it just leaves no soil fatigue.
    if (!Crop || Crop->Revision != ExpectedCropRevision ||
        Crop->Revision >= MAX_int64 - 1 || Crop->Stage != ECropGrowthStage::Mature ||
        Crop->HarvestDefinitionId.IsNone() || Crop->HarvestUnits <= 0 ||
        Crop->HarvestUnits > 64 || Crop->SeedReturnUnits < 0 || Crop->SeedReturnUnits > 64 ||
        Crop->HarvestUnits + Crop->SeedReturnUnits > 64 ||
        (Crop->SeedReturnUnits > 0 && Crop->SeedDefinitionId.IsNone()) ||
        Crop->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    FWorldFieldCellState* FieldForHarvest = FieldCells.Find(Crop->PlotCell);
    if (FieldForHarvest && FieldForHarvest->Revision >= MAX_int64 - 1)
        return false;

    const int32 TotalOutputUnits = Crop->HarvestUnits + Crop->SeedReturnUnits;
    TArray<FName> Definitions;
    TArray<FVector> Locations;
    Definitions.Reserve(TotalOutputUnits);
    Locations.Reserve(TotalOutputUnits);
    const FVector PlotOrigin(
        static_cast<double>(Crop->PlotCell.X) * 100.0,
        static_cast<double>(Crop->PlotCell.Y) * 100.0,
        static_cast<double>(Crop->PlotCell.Z) * 100.0);
    for (int32 Index = 0; Index < Crop->HarvestUnits; ++Index)
    {
        Definitions.Add(Crop->HarvestDefinitionId);
        Locations.Add(PlotOrigin + FVector(static_cast<double>(Index) * 25.0, 0.0, 10.0));
    }
    for (int32 Index = 0; Index < Crop->SeedReturnUnits; ++Index)
    {
        Definitions.Add(Crop->SeedDefinitionId);
        Locations.Add(PlotOrigin + FVector(
            static_cast<double>(Crop->HarvestUnits + Index) * 25.0, 0.0, 10.0));
    }

    TArray<FGuid> CreatedItems;
    if (!RegisterWorldOutputsAtomically(Definitions, Locations, CreatedItems))
        return false;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::HarvestCrop;
    Receipt.EntityId = CropId;
    Receipt.ExpectedEntityRevision = ExpectedCropRevision;
    Receipt.ResultingEntityRevision = ExpectedCropRevision + 1;
    Receipt.DefinitionId = Crop->HarvestDefinitionId;
    Receipt.RequestedOutputUnits = TotalOutputUnits;
    Receipt.OutputItemIds = CreatedItems;

    if (FieldForHarvest)
    {
        if (CropDefinition)
            FieldForHarvest->Fertility = FMath::Max(0.0f, FieldForHarvest->Fertility - CropDefinition->SoilExhaustionPerHarvest);
        ++FieldForHarvest->HarvestCount;
        ++FieldForHarvest->Revision;
    }
    Crop->Stage = ECropGrowthStage::Harvested;
    Crop->HarvestOperationId = OperationId;
    Crop->HarvestExpectedRevision = ExpectedCropRevision;
    Crop->HarvestOutputItemIds = CreatedItems;
    ++Crop->Revision;
    Crop->ActionReceipts.Add(Receipt);
    OutItemIds = MoveTemp(CreatedItems);
    return true;
}

bool UWorldAgricultureSubsystem::GetCropAtPlotCell(
    const FIntVector& PlotCell, FWorldCropState& OutCrop) const
{
    OutCrop = FWorldCropState();
    const FWorldCropState* Found = nullptr;
    for (const TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        const FWorldCropState& Crop = Pair.Value;
        if (Crop.PlotCell != PlotCell || Crop.Stage == ECropGrowthStage::Harvested ||
            Crop.Stage == ECropGrowthStage::Withered)
            continue;
        if (Found)
            return false; // Ambiguous saved state: never guess which crop to mutate.
        Found = &Crop;
    }
    if (!Found)
        return false;
    OutCrop = *Found;
    return true;
}

bool UWorldAgricultureSubsystem::TryHarvestCropToInventory(
    const FGuid& OperationId, const FGuid& CropId, int64 ExpectedCropRevision,
    UInventoryCraftingComponent* Inventory, const FGuid& SurvivalEntityId,
    TArray<FGuid>& OutItemIds)
{
    OutItemIds.Reset();
    if (!OperationId.IsValid() || !CropId.IsValid() || ExpectedCropRevision <= 0 ||
        !IsValid(Inventory) || !SurvivalEntityId.IsValid())
        return false;

    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !Ownership || !Survival || !GameData || !GameData->IsApplied())
        return false;

    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    if (!InventoryOwnerId.IsValid())
        return false;

    FWorldCropState Crop;
    if (!GetCrop(CropId, Crop))
        return false;
    const FWorldCropDefinition* CropDefinition = FindCropDefinition(Crop.CropDefinitionId);
    if (!CropDefinition || !FMath::IsFinite(CropDefinition->HarvestLaborSeconds) ||
        CropDefinition->HarvestLaborSeconds < 1.0 || CropDefinition->HarvestLaborSeconds > 3600.0)
        return false;

    // A retry after a save/reload is successful only when all harvest outputs
    // remain owned by this exact inventory and each ID is present there.
    if (Crop.HarvestOperationId == OperationId)
    {
        if (Crop.HarvestExpectedRevision != ExpectedCropRevision ||
            Crop.HarvestOutputItemIds.IsEmpty())
            return false;
        for (const FGuid& ItemId : Crop.HarvestOutputItemIds)
        {
            FWorldOwnedItemRecord Item;
            if (!Ownership->TryGetItem(ItemId, Item) ||
                Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
                Item.OwnerId != InventoryOwnerId ||
                !Inventory->Items.ContainsByPredicate([&ItemId](const FInventoryStack& Stack)
                    { return Stack.OwnedWorldItemId == ItemId; }))
                return false;
        }
        OutItemIds = Crop.HarvestOutputItemIds;
        return true;
    }

    if (Crop.Revision != ExpectedCropRevision || Crop.Stage != ECropGrowthStage::Mature ||
        Crop.HarvestUnits <= 0 || Crop.SeedReturnUnits < 0 ||
        Crop.HarvestUnits + Crop.SeedReturnUnits <= 0 ||
        Crop.HarvestUnits + Crop.SeedReturnUnits > 64 ||
        (Crop.SeedReturnUnits > 0 && Crop.SeedDefinitionId.IsNone()) ||
        Ownership->MaxTrackedItems <= 0 ||
        Crop.HarvestUnits + Crop.SeedReturnUnits >
            Ownership->MaxTrackedItems - Ownership->GetTrackedItemCount() ||
        !Survival->CanRecordActivity(SurvivalEntityId, ESurvivalActivity::Build,
            static_cast<float>(CropDefinition->HarvestLaborSeconds), 0.0f))
        return false;

    FGuid InventoryActorBefore;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(InventoryActorBefore, InventoryOwnerBefore,
        InventoryItemsBefore, QuickAccessBefore);
    if (InventoryOwnerBefore != InventoryOwnerId)
        return false;
    (void)InventoryActorBefore;

    // Every output becomes its own ledger-owned item. Preflight the real item
    // weights and inventory capacity before a crop or world-item record changes.
    TArray<FInventoryStack> CapacityProbe;
    CapacityProbe.Reserve(Crop.HarvestUnits + Crop.SeedReturnUnits);
    auto ToInventoryCategory = [](FName Category)
    {
        if (Category == TEXT("Tool")) return EInventoryItemCategory::Tool;
        if (Category == TEXT("Weapon")) return EInventoryItemCategory::Weapon;
        if (Category == TEXT("BuildingPart")) return EInventoryItemCategory::BuildingPart;
        if (Category == TEXT("Consumable")) return EInventoryItemCategory::Consumable;
        if (Category == TEXT("Protection")) return EInventoryItemCategory::Protection;
        return EInventoryItemCategory::Resource;
    };
    auto AddCapacityProbe = [&](FName DefinitionId) -> bool
    {
        const FGameDataItem* Definition = GameData->GetItems().Find(DefinitionId);
        if (!Definition || Definition->WeightGrams <= 0)
            return false;
        if (!Inventory->ItemWeightGrams.Contains(DefinitionId))
            Inventory->ItemWeightGrams.Add(DefinitionId, Definition->WeightGrams);

        FInventoryStack Stack;
        Stack.ItemId = DefinitionId;
        Stack.Quantity = 1;
        Stack.Category = ToInventoryCategory(Definition->Category);
        Stack.MaxStack = 1;
        Stack.bIsTool = Stack.Category == EInventoryItemCategory::Tool;
        Stack.OwnedWorldItemId = FGuid::NewGuid();
        if (!Stack.OwnedWorldItemId.IsValid())
            return false;
        CapacityProbe.Add(Stack);
        return true;
    };
    for (int32 Index = 0; Index < Crop.HarvestUnits; ++Index)
        if (!AddCapacityProbe(Crop.HarvestDefinitionId))
            return false;
    for (int32 Index = 0; Index < Crop.SeedReturnUnits; ++Index)
        if (!AddCapacityProbe(Crop.SeedDefinitionId))
            return false;
    if (!Inventory->CanAddItems(CapacityProbe))
        return false;

    TArray<FWorldCropState> CropsBefore;
    TArray<FWorldAnimalState> AnimalsBefore;
    TArray<FWorldFieldCellState> FieldCellsBefore;
    TArray<FWorldAnimalPenState> AnimalPensBefore;
    ExportSnapshot(CropsBefore, AnimalsBefore, FieldCellsBefore, AnimalPensBefore);

    TArray<FWorldOwnedItemRecord> OwnershipItemsBefore;
    TArray<FWorldItemTransferReceipt> OwnershipTransfersBefore;
    TArray<FWorldItemSplitReceipt> OwnershipSplitsBefore;
    Ownership->ExportSnapshot(OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);

    TArray<FWorldSurvivalProfile> ProfilesBefore;
    TArray<FWorldDiscoveryRecord> DiscoveriesBefore;
    EWorldWeatherState WeatherBefore = EWorldWeatherState::Clear;
    float WeatherSeverityBefore = 0.0f;
    Survival->ExportSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);

    auto Rollback = [&]() -> bool
    {
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        const bool bFarmRestored =
            RestoreSnapshot(CropsBefore, AnimalsBefore, FieldCellsBefore, AnimalPensBefore);
        const bool bSurvivalRestored =
            Survival->RestoreSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);
        if (!bOwnershipRestored || !bInventoryRestored || !bFarmRestored || !bSurvivalRestored)
        {
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(
                    TEXT("Harvest-to-inventory rollback failed; stop simulation and recover/reset the world."));
            UE_LOG(LogTemp, Error, TEXT("Harvest-to-inventory rollback failed for crop %s."),
                *CropId.ToString());
            return false;
        }
        return true;
    };

    TArray<FGuid> WorldOutputIds;
    if (!TryHarvestCrop(OperationId, CropId, ExpectedCropRevision, WorldOutputIds) ||
        WorldOutputIds.Num() != CapacityProbe.Num())
    {
        Rollback();
        return false;
    }

    for (const FGuid& ItemId : WorldOutputIds)
    {
        FWorldOwnedItemRecord WorldItem;
        if (!Ownership->TryGetItem(ItemId, WorldItem) ||
            WorldItem.OwnerKind != EWorldItemOwnerKind::World ||
            WorldItem.Revision <= 0 || WorldItem.Revision >= MAX_int64)
        {
            Rollback();
            return false;
        }

        FWorldOwnedItemRecord InventoryItem;
        if (!Ownership->TryTransferItem(FGuid::NewGuid(), ItemId, WorldItem.Revision,
            EWorldItemOwnerKind::World, EWorldItemOwnerKind::Inventory,
            InventoryOwnerId, WorldItem.WorldLocation, InventoryItem))
        {
            Rollback();
            return false;
        }

        const FGameDataItem* Definition = GameData->GetItems().Find(InventoryItem.DefinitionId);
        if (!Definition)
        {
            Rollback();
            return false;
        }
        FInventoryStack Stack;
        Stack.ItemId = InventoryItem.DefinitionId;
        Stack.Quantity = 1;
        Stack.Category = ToInventoryCategory(Definition->Category);
        Stack.MaxStack = 1;
        Stack.bIsTool = Stack.Category == EInventoryItemCategory::Tool;
        Stack.OwnedWorldItemId = InventoryItem.ItemId;
        if (!Inventory->TryAddItem(Stack))
        {
            Rollback();
            return false;
        }
    }

    if (!Survival->RecordActivity(SurvivalEntityId, ESurvivalActivity::Build,
        static_cast<float>(CropDefinition->HarvestLaborSeconds), 0.0f))
    {
        Rollback();
        return false;
    }

    OutItemIds = MoveTemp(WorldOutputIds);
    return true;
}

bool UWorldAgricultureSubsystem::TryCaptureWildlife(
    const FGuid& OperationId, const FGuid& SourceWildlifeIdentityId, AWildlifeActor* Wildlife,
    const FWorldAnimalCaptureDefinition& Definition, FGuid& OutAnimalId)
{
    OutAnimalId.Invalidate();
    if (!OperationId.IsValid() || !SourceWildlifeIdentityId.IsValid() ||
        Definition.AnimalDefinitionId.IsNone() ||
        Definition.ProductUnitsPerCollection < 0 || Definition.ProductUnitsPerCollection > 64 ||
        (Definition.ProductUnitsPerCollection > 0 && Definition.ProductDefinitionId.IsNone()) ||
        Definition.MeatUnits < 0 || Definition.MeatUnits > 64 ||
        Definition.HideUnits < 0 || Definition.HideUnits > 64 ||
        Definition.BoneUnits < 0 || Definition.BoneUnits > 64 ||
        Definition.MeatUnits + Definition.HideUnits + Definition.BoneUnits <= 0 ||
        Definition.MeatUnits + Definition.HideUnits + Definition.BoneUnits > 64 ||
        (Definition.MeatUnits > 0 && Definition.MeatDefinitionId.IsNone()) ||
        (Definition.HideUnits > 0 && Definition.HideDefinitionId.IsNone()) ||
        (Definition.BoneUnits > 0 && Definition.BoneDefinitionId.IsNone()))
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldAgricultureActionKind::CaptureAnimal ||
            Existing->SourceWildlifeIdentityId != SourceWildlifeIdentityId ||
            Existing->DefinitionId != Definition.AnimalDefinitionId ||
            !Animals.Contains(Existing->EntityId))
            return false;
        const FWorldAnimalState& ExistingAnimal = Animals.FindChecked(Existing->EntityId);
        if (ExistingAnimal.FeedDefinitionId != Definition.FeedDefinitionId ||
            ExistingAnimal.ProductDefinitionId != Definition.ProductDefinitionId ||
            ExistingAnimal.MeatDefinitionId != Definition.MeatDefinitionId ||
            ExistingAnimal.MeatUnits != Definition.MeatUnits ||
            ExistingAnimal.HideDefinitionId != Definition.HideDefinitionId ||
            ExistingAnimal.HideUnits != Definition.HideUnits ||
            ExistingAnimal.BoneDefinitionId != Definition.BoneDefinitionId ||
            ExistingAnimal.BoneUnits != Definition.BoneUnits)
            return false;
        OutAnimalId = ExistingAnimal.AnimalId;
        return true;
    }

    if (!IsValid(Wildlife) || !IsValid(Wildlife->Identity) || !IsValid(Wildlife->Health) ||
        Wildlife->Identity->GetInstanceId() != SourceWildlifeIdentityId ||
        Wildlife->Kind != EWildlifeKind::SmallAnimal || Wildlife->Health->IsDead() ||
        Wildlife->bCapturedForAnimalManagement || MaxAnimals <= 0 ||
        Animals.Num() >= MaxAnimals || MaxOperationReceiptsPerEntity <= 0 ||
        MaxOperationReceiptsPerEntity > 256)
        return false;

    for (const TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        if (Pair.Value.SourceWildlifeIdentityId == SourceWildlifeIdentityId)
            return false; // One live wildlife actor can become only one logical animal.
    }

    FWorldAnimalState Animal;
    Animal.AnimalId = FGuid::NewGuid();
    if (!Animal.AnimalId.IsValid() || Animals.Contains(Animal.AnimalId))
        return false;
    Animal.AnimalDefinitionId = Definition.AnimalDefinitionId;
    Animal.LifecycleState = EWorldAnimalLifecycleState::Captured;
    Animal.SourceWildlifeIdentityId = SourceWildlifeIdentityId;
    Animal.Location = Wildlife->GetActorLocation();
    Animal.Health = 100.0f;
    Animal.Hunger = 0.0f;
    Animal.FeedDefinitionId = Definition.FeedDefinitionId;
    Animal.ProductDefinitionId = Definition.ProductDefinitionId;
    Animal.ProductUnits = 0;
    Animal.MeatDefinitionId = Definition.MeatDefinitionId;
    Animal.MeatUnits = Definition.MeatUnits;
    Animal.HideDefinitionId = Definition.HideDefinitionId;
    Animal.HideUnits = Definition.HideUnits;
    Animal.BoneDefinitionId = Definition.BoneDefinitionId;
    Animal.BoneUnits = Definition.BoneUnits;
    Animal.Revision = 1;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::CaptureAnimal;
    Receipt.EntityId = Animal.AnimalId;
    Receipt.SourceWildlifeIdentityId = SourceWildlifeIdentityId;
    Receipt.ExpectedEntityRevision = 0;
    Receipt.ResultingEntityRevision = Animal.Revision;
    Receipt.DefinitionId = Definition.AnimalDefinitionId;
    Animal.ActionReceipts.Add(Receipt);

    if (!Wildlife->ApplyAnimalManagementState(Animal.AnimalId, FGuid(), Animal.Location))
        return false;

    Animals.Add(Animal.AnimalId, Animal);
    OutAnimalId = Animal.AnimalId;
    return true;
}

bool UWorldAgricultureSubsystem::TryMoveAnimalToPen(
    const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
    const FGuid& PenId, int64 ExpectedPenRevision)
{
    if (!OperationId.IsValid() || !AnimalId.IsValid() || ExpectedAnimalRevision <= 0 ||
        !PenId.IsValid() || ExpectedPenRevision <= 0)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        return Existing->Action == EWorldAgricultureActionKind::MoveAnimalToPen &&
            Existing->EntityId == AnimalId && Existing->PenId == PenId &&
            Existing->ExpectedEntityRevision == ExpectedAnimalRevision &&
            Existing->ExpectedPenRevision == ExpectedPenRevision;
    }

    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    FWorldAnimalPenState* Pen = AnimalPens.Find(PenId);
    if (!Animal || !Pen || Animal->LifecycleState != EWorldAnimalLifecycleState::Captured ||
        Animal->Revision != ExpectedAnimalRevision || Animal->Revision >= MAX_int64 - 1 ||
        Pen->Revision != ExpectedPenRevision || Pen->Revision >= MAX_int64 - 1 ||
        Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity ||
        Pen->AnimalIds.Num() >= Pen->Capacity || Pen->AnimalIds.Contains(AnimalId))
        return false;

    if (!Pen->AllowedAnimalDefinitionIds.IsEmpty() &&
        !Pen->AllowedAnimalDefinitionIds.Contains(Animal->AnimalDefinitionId))
        return false;

    AWildlifeActor* Wildlife = nullptr;
    if (Animal->SourceWildlifeIdentityId.IsValid())
    {
        Wildlife = FindWildlifeActor(Animal->SourceWildlifeIdentityId);
        if (!IsValid(Wildlife) || !Wildlife->bCapturedForAnimalManagement ||
            Wildlife->CapturedAnimalId != AnimalId)
            return false;
    }

    if (IsValid(Wildlife) &&
        !Wildlife->ApplyAnimalManagementState(AnimalId, PenId, Pen->Location))
        return false;

    const int64 PreviousAnimalRevision = Animal->Revision;
    const int64 PreviousPenRevision = Pen->Revision;
    Animal->LifecycleState = EWorldAnimalLifecycleState::Penned;
    Animal->PenId = PenId;
    Animal->Location = Pen->Location;
    ++Animal->Revision;
    Pen->AnimalIds.Add(AnimalId);
    ++Pen->Revision;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::MoveAnimalToPen;
    Receipt.EntityId = AnimalId;
    Receipt.SourceWildlifeIdentityId = Animal->SourceWildlifeIdentityId;
    Receipt.PenId = PenId;
    Receipt.ExpectedEntityRevision = PreviousAnimalRevision;
    Receipt.ResultingEntityRevision = Animal->Revision;
    Receipt.ExpectedPenRevision = PreviousPenRevision;
    Receipt.ResultingPenRevision = Pen->Revision;
    Receipt.DefinitionId = Animal->AnimalDefinitionId;
    Animal->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldAgricultureSubsystem::TrySlaughterAnimal(
    const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
    TArray<FGuid>& OutItemIds)
{
    OutItemIds.Reset();
    if (!OperationId.IsValid() || !AnimalId.IsValid() || ExpectedAnimalRevision <= 0)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldAgricultureActionKind::SlaughterAnimal ||
            Existing->EntityId != AnimalId ||
            Existing->ExpectedEntityRevision != ExpectedAnimalRevision)
            return false;
        OutItemIds = Existing->OutputItemIds;
        return !OutItemIds.IsEmpty();
    }

    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    if (!Animal || Animal->LifecycleState != EWorldAnimalLifecycleState::Penned ||
        Animal->Revision != ExpectedAnimalRevision || Animal->Revision >= MAX_int64 - 1 ||
        !Animal->PenId.IsValid() || Animal->Health <= 0.0f || Animal->ProductUnits != 0 ||
        Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    FWorldAnimalPenState* Pen = AnimalPens.Find(Animal->PenId);
    if (!Pen || Pen->Revision >= MAX_int64 - 1 || !Pen->AnimalIds.Contains(AnimalId))
        return false;

    const int32 TotalOutputUnits = Animal->MeatUnits + Animal->HideUnits + Animal->BoneUnits;
    if (TotalOutputUnits <= 0 || TotalOutputUnits > 64 ||
        Animal->MeatUnits < 0 || Animal->MeatUnits > 64 ||
        Animal->HideUnits < 0 || Animal->HideUnits > 64 ||
        Animal->BoneUnits < 0 || Animal->BoneUnits > 64 ||
        (Animal->MeatUnits > 0 && Animal->MeatDefinitionId.IsNone()) ||
        (Animal->HideUnits > 0 && Animal->HideDefinitionId.IsNone()) ||
        (Animal->BoneUnits > 0 && Animal->BoneDefinitionId.IsNone()))
        return false;

    AWildlifeActor* Wildlife = nullptr;
    if (Animal->SourceWildlifeIdentityId.IsValid())
    {
        Wildlife = FindWildlifeActor(Animal->SourceWildlifeIdentityId);
        if (!IsValid(Wildlife) || !Wildlife->bCapturedForAnimalManagement ||
            Wildlife->CapturedAnimalId != AnimalId || Wildlife->AnimalPenId != Animal->PenId)
            return false;
    }

    TArray<FName> Definitions;
    TArray<FVector> Locations;
    Definitions.Reserve(TotalOutputUnits);
    Locations.Reserve(TotalOutputUnits);
    int32 OutputIndex = 0;
    auto AddOutputs = [&](FName DefinitionId, int32 Units)
    {
        for (int32 Index = 0; Index < Units; ++Index)
        {
            Definitions.Add(DefinitionId);
            Locations.Add(Animal->Location + FVector(
                static_cast<double>(OutputIndex++) * 20.0, 0.0, 15.0));
        }
    };
    AddOutputs(Animal->MeatDefinitionId, Animal->MeatUnits);
    AddOutputs(Animal->HideDefinitionId, Animal->HideUnits);
    AddOutputs(Animal->BoneDefinitionId, Animal->BoneUnits);

    TArray<FGuid> CreatedItems;
    if (!RegisterWorldOutputsAtomically(Definitions, Locations, CreatedItems))
        return false;

    const int64 PreviousAnimalRevision = Animal->Revision;
    const FGuid PreviousPenId = Animal->PenId;
    const int64 PreviousPenRevision = Pen->Revision;
    Pen->AnimalIds.RemoveSingle(AnimalId);
    ++Pen->Revision;
    Animal->LifecycleState = EWorldAnimalLifecycleState::Slaughtered;
    Animal->Health = 0.0f;
    Animal->PenId.Invalidate();
    Animal->ProductProgressSeconds = 0.0;
    ++Animal->Revision;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::SlaughterAnimal;
    Receipt.EntityId = AnimalId;
    Receipt.SourceWildlifeIdentityId = Animal->SourceWildlifeIdentityId;
    Receipt.PenId = PreviousPenId;
    Receipt.ExpectedEntityRevision = PreviousAnimalRevision;
    Receipt.ResultingEntityRevision = Animal->Revision;
    Receipt.ExpectedPenRevision = PreviousPenRevision;
    Receipt.ResultingPenRevision = Pen->Revision;
    Receipt.DefinitionId = Animal->AnimalDefinitionId;
    Receipt.RequestedOutputUnits = TotalOutputUnits;
    Receipt.OutputItemIds = CreatedItems;
    Animal->ActionReceipts.Add(Receipt);

    if (IsValid(Wildlife))
        Wildlife->Destroy();
    OutItemIds = MoveTemp(CreatedItems);
    return true;
}

bool UWorldAgricultureSubsystem::TryFeedAnimal(
    const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
    const FGuid& FeedItemId, int64 ExpectedFeedItemRevision,
    const FGuid& ExpectedInventoryOwnerId, float HungerReduction)
{
    if (!OperationId.IsValid() || !AnimalId.IsValid() || ExpectedAnimalRevision <= 0 ||
        !FeedItemId.IsValid() || ExpectedFeedItemRevision <= 0 ||
        !ExpectedInventoryOwnerId.IsValid() || !FMath::IsFinite(HungerReduction) ||
        HungerReduction <= 0.0f || HungerReduction > 100.0f)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        return Existing->Action == EWorldAgricultureActionKind::FeedAnimal &&
            Existing->EntityId == AnimalId && Existing->InputItemId == FeedItemId &&
            Existing->ExpectedInventoryOwnerId == ExpectedInventoryOwnerId &&
            Existing->ExpectedEntityRevision == ExpectedAnimalRevision &&
            Existing->ExpectedItemRevision == ExpectedFeedItemRevision &&
            FMath::IsNearlyEqual(Existing->HungerReduction, HungerReduction, 0.001f);
    }

    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    FWorldOwnedItemRecord FeedItem;
    if (!Animal || !Ownership || !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        Animal->Revision != ExpectedAnimalRevision || Animal->Revision >= MAX_int64 - 1 ||
        (Animal->LifecycleState != EWorldAnimalLifecycleState::Managed &&
         Animal->LifecycleState != EWorldAnimalLifecycleState::Penned) ||
        Animal->Health <= 0.0f || Animal->FeedDefinitionId.IsNone() ||
        Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity ||
        !Ownership->TryGetItem(FeedItemId, FeedItem) ||
        FeedItem.Revision != ExpectedFeedItemRevision || FeedItem.Revision >= MAX_int64 - 1 ||
        FeedItem.OwnerKind != EWorldItemOwnerKind::Inventory ||
        FeedItem.OwnerId != ExpectedInventoryOwnerId ||
        FeedItem.DefinitionId != Animal->FeedDefinitionId || FeedItem.Quantity != 1)
        return false;

    if (!ConsumeInventoryItemAtomically(OperationId, FeedItemId,
        ExpectedFeedItemRevision, ExpectedInventoryOwnerId, Inventory))
        return false;

    Animal->Hunger = FMath::Clamp(Animal->Hunger - HungerReduction, 0.0f, 100.0f);
    ++Animal->Revision;
    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::FeedAnimal;
    Receipt.EntityId = AnimalId;
    Receipt.InputItemId = FeedItemId;
    Receipt.ExpectedInventoryOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedEntityRevision = ExpectedAnimalRevision;
    Receipt.ExpectedItemRevision = ExpectedFeedItemRevision;
    Receipt.ResultingEntityRevision = Animal->Revision;
    Receipt.ResultingItemRevision = ExpectedFeedItemRevision + 1;
    Receipt.DefinitionId = FeedItem.DefinitionId;
    Receipt.HungerReduction = HungerReduction;
    Animal->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldAgricultureSubsystem::TryCollectAnimalProducts(
    const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
    TArray<FGuid>& OutItemIds)
{
    OutItemIds.Reset();
    if (!OperationId.IsValid() || !AnimalId.IsValid() || ExpectedAnimalRevision <= 0)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldAgricultureActionKind::CollectAnimalProducts ||
            Existing->EntityId != AnimalId ||
            Existing->ExpectedEntityRevision != ExpectedAnimalRevision)
            return false;
        OutItemIds = Existing->OutputItemIds;
        return !OutItemIds.IsEmpty();
    }

    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    if (!Animal || Animal->Revision != ExpectedAnimalRevision ||
        Animal->Revision >= MAX_int64 - 1 || Animal->Health <= 0.0f ||
        (Animal->LifecycleState != EWorldAnimalLifecycleState::Managed &&
         Animal->LifecycleState != EWorldAnimalLifecycleState::Penned) ||
        Animal->ProductDefinitionId.IsNone() || Animal->ProductUnits <= 0 ||
        Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    int32 UnitsPerCollection = 1;
    const UWorldGameDataSubsystem* GameData =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (GameData && GameData->IsLoaded())
    {
        if (const FWorldAnimalCaptureDefinition* Definition =
            GameData->GetAnimalCaptureDefinitions().FindByPredicate(
                [Animal](const FWorldAnimalCaptureDefinition& Candidate)
                { return Candidate.AnimalDefinitionId == Animal->AnimalDefinitionId; }))
        {
            if (Definition->ProductUnitsPerCollection > 0)
                UnitsPerCollection = Definition->ProductUnitsPerCollection;
        }
    }
    const int32 Units = FMath::Min(Animal->ProductUnits, UnitsPerCollection);
    TArray<FName> Definitions;
    TArray<FVector> Locations;
    Definitions.Init(Animal->ProductDefinitionId, Units);
    Locations.Reserve(Units);
    for (int32 Index = 0; Index < Units; ++Index)
        Locations.Add(Animal->Location + FVector(static_cast<double>(Index) * 20.0, 0.0, 0.0));

    TArray<FGuid> CreatedItems;
    if (!RegisterWorldOutputsAtomically(Definitions, Locations, CreatedItems))
        return false;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::CollectAnimalProducts;
    Receipt.EntityId = AnimalId;
    Receipt.ExpectedEntityRevision = ExpectedAnimalRevision;
    Receipt.ResultingEntityRevision = ExpectedAnimalRevision + 1;
    Receipt.DefinitionId = Animal->ProductDefinitionId;
    Receipt.RequestedOutputUnits = Units;
    Receipt.OutputItemIds = CreatedItems;

    Animal->ProductUnits -= Units;
    ++Animal->Revision;
    Animal->ActionReceipts.Add(Receipt);
    OutItemIds = MoveTemp(CreatedItems);
    return true;
}

bool UWorldAgricultureSubsystem::RegisterCrop(const FWorldCropState& Crop)
{
    if (!Crop.CropId.IsValid() || Crop.CropDefinitionId.IsNone() ||
        static_cast<uint8>(Crop.Stage) > static_cast<uint8>(ECropGrowthStage::Harvested) ||
        !FMath::IsFinite(Crop.Moisture) || Crop.Moisture < 0.0f || Crop.Moisture > 1.0f ||
        !FMath::IsFinite(Crop.GrowthProgressSeconds) || Crop.GrowthProgressSeconds < 0.0 ||
        !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0 ||
        Crop.Revision <= 0 || Crop.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
        MaxOperationReceiptsPerEntity <= 0 || Crops.Contains(Crop.CropId) ||
        MaxCrops <= 0 || Crops.Num() >= MaxCrops)
    {
        return false;
    }

    if (Crop.PlantOperationId.IsValid() &&
        (!Crop.SeedItemId.IsValid() || !Crop.SeedInventoryOwnerId.IsValid() ||
         Crop.SeedItemExpectedRevision <= 0))
        return false;
    if (Crop.HarvestOperationId.IsValid() &&
        (Crop.Stage != ECropGrowthStage::Harvested || Crop.HarvestExpectedRevision <= 0 ||
         Crop.HarvestDefinitionId.IsNone() || Crop.HarvestUnits <= 0 ||
         Crop.SeedReturnUnits < 0 || Crop.SeedReturnUnits > 64 ||
        Crop.HarvestUnits + Crop.SeedReturnUnits > 64 ||
        (Crop.SeedReturnUnits > 0 && Crop.SeedDefinitionId.IsNone()) ||
        Crop.HarvestOutputItemIds.Num() != Crop.HarvestUnits + Crop.SeedReturnUnits))
        return false;

    TSet<FGuid> SeenOperations;
    for (const FWorldAgricultureActionReceipt& Receipt : Crop.ActionReceipts)
    {
        if (!Receipt.OperationId.IsValid() || SeenOperations.Contains(Receipt.OperationId) ||
            Receipt.EntityId != Crop.CropId ||
            (Receipt.Action != EWorldAgricultureActionKind::PlantCrop &&
             Receipt.Action != EWorldAgricultureActionKind::HarvestCrop))
            return false;
        SeenOperations.Add(Receipt.OperationId);
    }

    Crops.Add(Crop.CropId, Crop);
    return true;
}

bool UWorldAgricultureSubsystem::RegisterAnimal(const FWorldAnimalState& Animal)
{
    if (!Animal.AnimalId.IsValid() || Animal.AnimalDefinitionId.IsNone() ||
        !FMath::IsFinite(Animal.Hunger) || Animal.Hunger < 0.0f || Animal.Hunger > 100.0f ||
        !FMath::IsFinite(Animal.Health) || Animal.Health < 0.0f || Animal.Health > 100.0f ||
        Animal.ProductUnits < 0 || !FMath::IsFinite(Animal.ProductProgressSeconds) ||
        Animal.ProductProgressSeconds < 0.0 || Animal.ProductProgressSeconds >= 3600.0 ||
        Animal.Location.ContainsNaN() || Animal.Revision <= 0 ||
        Animal.LifecycleState != EWorldAnimalLifecycleState::Managed ||
        Animal.SourceWildlifeIdentityId.IsValid() || Animal.PenId.IsValid() ||
        Animal.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
        MaxOperationReceiptsPerEntity <= 0 || Animals.Contains(Animal.AnimalId) ||
        MaxAnimals <= 0 || Animals.Num() >= MaxAnimals)
    {
        return false;
    }

    TSet<FGuid> SeenOperations;
    for (const FWorldAgricultureActionReceipt& Receipt : Animal.ActionReceipts)
    {
        if (!Receipt.OperationId.IsValid() || SeenOperations.Contains(Receipt.OperationId) ||
            Receipt.EntityId != Animal.AnimalId ||
            (Receipt.Action != EWorldAgricultureActionKind::FeedAnimal &&
             Receipt.Action != EWorldAgricultureActionKind::CollectAnimalProducts))
            return false;
        SeenOperations.Add(Receipt.OperationId);
    }
    Animals.Add(Animal.AnimalId, Animal);
    return true;
}

bool UWorldAgricultureSubsystem::ApplyEnvironmentalEffect(
    FName EffectId, float Severity, double DeltaSeconds)
{
    if (EffectId.IsNone() || !FMath::IsFinite(Severity) || Severity < 0.0f ||
        Severity > 1.0f || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 ||
        DeltaSeconds > 3600.0)
        return false;

    const bool bContinuousDrying = EffectId == TEXT("crop_moisture_loss");
    const bool bCropDamage = EffectId == TEXT("crop_damage");
    if (!bContinuousDrying && !bCropDamage)
        return false;

    // Preflight the entire domain first so revision exhaustion cannot leave a
    // partially affected set of fields/crops inside an otherwise valid fixed step.
    for (const TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
    {
        const FWorldFieldCellState& Field = Pair.Value;
        if (Field.Revision >= MAX_int64 - 1 || !FMath::IsFinite(Field.Moisture) ||
            Field.Moisture < 0.0f || Field.Moisture > 1.0f)
            return false;
    }
    for (const TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        const FWorldCropState& Crop = Pair.Value;
        if (Crop.Revision >= MAX_int64 - 1 || !FMath::IsFinite(Crop.Moisture) ||
            Crop.Moisture < 0.0f || Crop.Moisture > 1.0f ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds) || Crop.GrowthProgressSeconds < 0.0)
            return false;
    }

    const float Hours = static_cast<float>(DeltaSeconds / 3600.0);
    if (bContinuousDrying)
    {
        const float MoistureLoss = 0.02f * Severity * Hours;
        for (TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
        {
            FWorldFieldCellState& Field = Pair.Value;
            const float OldMoisture = Field.Moisture;
            Field.Moisture = FMath::Max(0.0f, Field.Moisture -
                MoistureLoss * (1.0f - FMath::Clamp(Field.Mulch, 0.0f, 1.0f) * MulchMoistureRetention));
            if (!FMath::IsNearlyEqual(OldMoisture, Field.Moisture))
                ++Field.Revision;
        }
        for (TPair<FGuid, FWorldCropState>& Pair : Crops)
        {
            FWorldCropState& Crop = Pair.Value;
            if (Crop.Stage == ECropGrowthStage::Mature ||
                Crop.Stage == ECropGrowthStage::Withered ||
                Crop.Stage == ECropGrowthStage::Harvested ||
                FieldCells.Contains(Crop.PlotCell))
                continue;

            const float NewMoisture = FMath::Max(0.0f, Crop.Moisture - MoistureLoss);
            if (!FMath::IsNearlyEqual(Crop.Moisture, NewMoisture))
            {
                Crop.Moisture = NewMoisture;
                ++Crop.Revision;
            }
        }
    }
    else
    {
        // Hail/crop-impact events remove part of accumulated growth and moisture
        // once at onset. Crop stage changes remain in the ordinary farm step.
        const float MoistureLoss = 0.25f * Severity;
        const float GrowthRetention = FMath::Clamp(1.0f - 0.45f * Severity, 0.0f, 1.0f);
        for (TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
        {
            FWorldFieldCellState& Field = Pair.Value;
            const float OldMoisture = Field.Moisture;
            Field.Moisture = FMath::Max(0.0f, Field.Moisture - MoistureLoss);
            if (!FMath::IsNearlyEqual(OldMoisture, Field.Moisture))
                ++Field.Revision;
        }
        for (TPair<FGuid, FWorldCropState>& Pair : Crops)
        {
            FWorldCropState& Crop = Pair.Value;
            if (Crop.Stage == ECropGrowthStage::Mature ||
                Crop.Stage == ECropGrowthStage::Withered ||
                Crop.Stage == ECropGrowthStage::Harvested)
                continue;

            const float OldMoisture = Crop.Moisture;
            if (FieldCells.Contains(Crop.PlotCell))
                Crop.Moisture = FieldCells.FindChecked(Crop.PlotCell).Moisture;
            else
                Crop.Moisture = FMath::Max(0.0f, Crop.Moisture - MoistureLoss);
            Crop.GrowthProgressSeconds *= GrowthRetention;
            if (!FMath::IsNearlyEqual(OldMoisture, Crop.Moisture) ||
                GrowthRetention < 1.0f)
                ++Crop.Revision;
        }
    }
    return true;
}

bool UWorldAgricultureSubsystem::SetCropMoisture(
    const FGuid& CropId, float Moisture, int64 ExpectedRevision)
{
    FWorldCropState* Crop = Crops.Find(CropId);
    if (!Crop || Crop->Revision != ExpectedRevision || Crop->Revision >= MAX_int64 - 1 ||
        !FMath::IsFinite(Moisture) || Moisture < 0.0f || Moisture > 1.0f)
    {
        return false;
    }
    Crop->Moisture = Moisture;
    ++Crop->Revision;
    return true;
}

bool UWorldAgricultureSubsystem::FeedAnimal(
    const FGuid& AnimalId, float HungerReduction, int64 ExpectedRevision)
{
    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    if (!Animal || Animal->Revision != ExpectedRevision || Animal->Revision >= MAX_int64 - 1 ||
        (Animal->LifecycleState != EWorldAnimalLifecycleState::Managed &&
         Animal->LifecycleState != EWorldAnimalLifecycleState::Penned) ||
        !FMath::IsFinite(HungerReduction) || HungerReduction <= 0.0f)
    {
        return false;
    }
    Animal->Hunger = FMath::Clamp(Animal->Hunger - HungerReduction, 0.0f, 100.0f);
    ++Animal->Revision;
    return true;
}

bool UWorldAgricultureSubsystem::CanAdvanceAgricultureStep(double DeltaSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        !FMath::IsFinite(MinimumGrowthMoisture) || MinimumGrowthMoisture < 0.0f ||
        MinimumGrowthMoisture > 1.0f || !FMath::IsFinite(SoilExhaustionPerHarvest) ||
        SoilExhaustionPerHarvest < 0.0f || SoilExhaustionPerHarvest > 1.0f ||
        !FMath::IsFinite(FieldMoistureLossPerHour) || FieldMoistureLossPerHour < 0.0f ||
        FieldMoistureLossPerHour > 1.0f || !FMath::IsFinite(WeedGrowthPerHour) ||
        WeedGrowthPerHour < 0.0f || WeedGrowthPerHour > 1.0f ||
        !FMath::IsFinite(MulchMoistureRetention) || MulchMoistureRetention < 0.0f ||
        MulchMoistureRetention > 1.0f || MaxFieldCells <= 0 || MaxFieldCells > 65536 ||
        MaxFieldActionReceiptsPerCell <= 0 || MaxFieldActionReceiptsPerCell > 512 ||
        !FMath::IsFinite(HungerPerHour) ||
        HungerPerHour < 0.0f || !FMath::IsFinite(ProductHungerLimit) ||
        ProductHungerLimit < 0.0f || ProductHungerLimit > 100.0f)
        return false;

    if (FieldCells.Num() > MaxFieldCells)
        return false;
    for (const TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
    {
        const FWorldFieldCellState& Field = Pair.Value;
        if (Field.Cell != Pair.Key || Field.Revision < 0 ||
            Field.ActionReceipts.Num() > MaxFieldActionReceiptsPerCell ||
            !FMath::IsFinite(Field.Fertility) || Field.Fertility < 0.0f || Field.Fertility > 1.0f ||
            !FMath::IsFinite(Field.WeedPressure) || Field.WeedPressure < 0.0f || Field.WeedPressure > 1.0f ||
            !FMath::IsFinite(Field.Mulch) || Field.Mulch < 0.0f || Field.Mulch > 1.0f ||
            !FMath::IsFinite(Field.Moisture) || Field.Moisture < 0.0f || Field.Moisture > 1.0f ||
            Field.HarvestCount < 0 || (Field.Revision == 0 && !Field.ActionReceipts.IsEmpty()))
            return false;
        TSet<FGuid> ReceiptIds;
        for (const FWorldFieldActionReceipt& Receipt : Field.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || ReceiptIds.Contains(Receipt.OperationId) ||
                Receipt.Cell != Field.Cell || Receipt.ExpectedRevision < 0 ||
                Receipt.ResultingRevision != Receipt.ExpectedRevision + 1 ||
                Receipt.ResultingRevision > Field.Revision ||
                static_cast<uint8>(Receipt.Action) > static_cast<uint8>(EWorldFieldActionKind::Fence) ||
                !FMath::IsFinite(Receipt.Amount) || Receipt.Amount < 0.0f || Receipt.Amount > 1.0f)
                return false;
            ReceiptIds.Add(Receipt.OperationId);
        }
    }
    for (const TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        const FWorldCropState& Crop = Pair.Value;
        if (Crop.Revision >= MAX_int64 - 1 ||
            static_cast<uint8>(Crop.Stage) > static_cast<uint8>(ECropGrowthStage::Harvested) ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds) || Crop.GrowthProgressSeconds < 0.0 ||
            !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0 ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds + DeltaSeconds))
            return false;
    }
    for (const TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
        if (Animal.LifecycleState == EWorldAnimalLifecycleState::Slaughtered)
            continue; // Slaughtered records remain as history and no longer simulate.
        if (Animal.Revision >= MAX_int64 - 1 ||
            !FMath::IsFinite(Animal.ProductProgressSeconds) ||
            Animal.ProductProgressSeconds < 0.0 || Animal.ProductProgressSeconds >= 3600.0 ||
            Animal.Location.ContainsNaN() ||
            !FMath::IsFinite(Animal.ProductProgressSeconds + DeltaSeconds) ||
            (Animal.Health > 0.0f && Animal.Hunger <= ProductHungerLimit &&
             !Animal.ProductDefinitionId.IsNone() &&
             Animal.ProductProgressSeconds + DeltaSeconds >= 3600.0 &&
             Animal.ProductUnits == MAX_int32))
            return false;
    }
    return true;
}

bool UWorldAgricultureSubsystem::AdvanceAgricultureStep(double DeltaSeconds)
{
    if (!CanAdvanceAgricultureStep(DeltaSeconds)) return false;

    const float Hours = static_cast<float>(DeltaSeconds / 3600.0);
    for (TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
    {
        FWorldFieldCellState& Field = Pair.Value;
        const float OldMoisture = Field.Moisture;
        const float OldWeeds = Field.WeedPressure;
        const float MoistureLoss = FieldMoistureLossPerHour * Hours *
            (1.0f - FMath::Clamp(Field.Mulch, 0.0f, 1.0f) * MulchMoistureRetention);
        Field.Moisture = FMath::Max(0.0f, Field.Moisture - MoistureLoss);
        if (Field.bTilled && !Field.bFenced)
            Field.WeedPressure = FMath::Clamp(Field.WeedPressure + WeedGrowthPerHour * Hours *
                (1.0f - Field.Mulch * 0.5f), 0.0f, 1.0f);
        if ((!FMath::IsNearlyEqual(OldMoisture, Field.Moisture) ||
             !FMath::IsNearlyEqual(OldWeeds, Field.WeedPressure)) &&
            Field.Revision < MAX_int64 - 1)
            ++Field.Revision;
    }

    for (TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        FWorldCropState& Crop = Pair.Value;
        if (Crop.Stage == ECropGrowthStage::Mature || Crop.Stage == ECropGrowthStage::Withered ||
            Crop.Stage == ECropGrowthStage::Harvested)
            continue;
        const FWorldFieldCellState* Field = FieldCells.Find(Crop.PlotCell);
        if (Field)
            Crop.Moisture = Field->Moisture;
        if (Crop.Moisture < MinimumGrowthMoisture)
        {
            Crop.Stage = ECropGrowthStage::Withered;
            ++Crop.Revision;
            continue;
        }
        const float FertilityFactor = Field ? (0.5f + 0.5f * Field->Fertility) : 1.0f;
        const float WeedFactor = Field ? (1.0f - 0.75f * Field->WeedPressure) : 1.0f;
        const float MulchFactor = Field ? (1.0f + 0.1f * Field->Mulch) : 1.0f;
        Crop.GrowthProgressSeconds += DeltaSeconds * FMath::Clamp(Crop.Moisture, 0.0f, 1.0f) *
            FertilityFactor * WeedFactor * MulchFactor;
        while (Crop.GrowthProgressSeconds >= Crop.SecondsPerStage &&
            Crop.Stage != ECropGrowthStage::Mature)
        {
            Crop.GrowthProgressSeconds -= Crop.SecondsPerStage;
            Crop.Stage = static_cast<ECropGrowthStage>(static_cast<uint8>(Crop.Stage) + 1);
        }
        ++Crop.Revision;
    }

    for (TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        FWorldAnimalState& Animal = Pair.Value;
        if (Animal.LifecycleState == EWorldAnimalLifecycleState::Slaughtered)
            continue; // Keep the slaughtered state and revision immutable.
        Animal.Hunger = FMath::Clamp(Animal.Hunger + HungerPerHour *
            static_cast<float>(DeltaSeconds / 3600.0), 0.0f, 100.0f);
        if (Animal.Hunger >= 80.0f)
            Animal.Health = FMath::Max(0.0f, Animal.Health - static_cast<float>(DeltaSeconds / 3600.0) * 2.0f);
        if (Animal.LifecycleState != EWorldAnimalLifecycleState::Slaughtered &&
            Animal.Health > 0.0f && Animal.Hunger <= ProductHungerLimit &&
            !Animal.ProductDefinitionId.IsNone() &&
            (Animal.LifecycleState == EWorldAnimalLifecycleState::Managed ||
             Animal.LifecycleState == EWorldAnimalLifecycleState::Penned))
        {
            Animal.ProductProgressSeconds += DeltaSeconds;
            while (Animal.ProductProgressSeconds >= 3600.0 && Animal.ProductUnits < MAX_int32)
            {
                Animal.ProductProgressSeconds -= 3600.0;
                ++Animal.ProductUnits;
            }
        }
        ++Animal.Revision;
    }
    return true;
}

bool UWorldAgricultureSubsystem::GetCrop(const FGuid& CropId, FWorldCropState& OutCrop) const
{
    const FWorldCropState* Found = Crops.Find(CropId);
    if (!Found) return false;
    OutCrop = *Found;
    return true;
}

bool UWorldAgricultureSubsystem::GetAnimal(const FGuid& AnimalId, FWorldAnimalState& OutAnimal) const
{
    const FWorldAnimalState* Found = Animals.Find(AnimalId);
    if (!Found) return false;
    OutAnimal = *Found;
    return true;
}

void UWorldAgricultureSubsystem::ExportSnapshot(
    TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals) const
{
    TArray<FWorldFieldCellState> ExistingFields;
    TArray<FWorldAnimalPenState> ExistingPens;
    FieldCells.GenerateValueArray(ExistingFields);
    AnimalPens.GenerateValueArray(ExistingPens);
    ExportSnapshot(OutCrops, OutAnimals, ExistingFields, ExistingPens);
}

void UWorldAgricultureSubsystem::ExportSnapshot(
    TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals,
    TArray<FWorldFieldCellState>& OutFieldCells) const
{
    TArray<FWorldAnimalPenState> ExistingPens;
    AnimalPens.GenerateValueArray(ExistingPens);
    ExportSnapshot(OutCrops, OutAnimals, OutFieldCells, ExistingPens);
}

void UWorldAgricultureSubsystem::ExportSnapshot(
    TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals,
    TArray<FWorldFieldCellState>& OutFieldCells,
    TArray<FWorldAnimalPenState>& OutAnimalPens) const
{
    Crops.GenerateValueArray(OutCrops);
    Animals.GenerateValueArray(OutAnimals);
    FieldCells.GenerateValueArray(OutFieldCells);
    AnimalPens.GenerateValueArray(OutAnimalPens);
    OutCrops.Sort([](const FWorldCropState& A, const FWorldCropState& B)
        { return A.CropId.ToString() < B.CropId.ToString(); });
    OutAnimals.Sort([](const FWorldAnimalState& A, const FWorldAnimalState& B)
        { return A.AnimalId.ToString() < B.AnimalId.ToString(); });
    OutFieldCells.Sort([](const FWorldFieldCellState& A, const FWorldFieldCellState& B)
    {
        if (A.Cell.X != B.Cell.X) return A.Cell.X < B.Cell.X;
        if (A.Cell.Y != B.Cell.Y) return A.Cell.Y < B.Cell.Y;
        return A.Cell.Z < B.Cell.Z;
    });
    OutAnimalPens.Sort([](const FWorldAnimalPenState& A, const FWorldAnimalPenState& B)
        { return A.PenId.ToString() < B.PenId.ToString(); });
}

bool UWorldAgricultureSubsystem::RestoreSnapshot(
    const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot)
{
    TArray<FWorldFieldCellState> ExistingFields;
    TArray<FWorldAnimalPenState> ExistingPens;
    FieldCells.GenerateValueArray(ExistingFields);
    AnimalPens.GenerateValueArray(ExistingPens);
    return RestoreSnapshot(CropSnapshot, AnimalSnapshot, ExistingFields, ExistingPens);
}

bool UWorldAgricultureSubsystem::RestoreSnapshot(
    const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot,
    const TArray<FWorldFieldCellState>& FieldCellSnapshot)
{
    TArray<FWorldAnimalPenState> ExistingPens;
    AnimalPens.GenerateValueArray(ExistingPens);
    return RestoreSnapshot(CropSnapshot, AnimalSnapshot, FieldCellSnapshot, ExistingPens);
}

bool UWorldAgricultureSubsystem::RestoreSnapshot(
    const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot,
    const TArray<FWorldFieldCellState>& FieldCellSnapshot,
    const TArray<FWorldAnimalPenState>& AnimalPenSnapshot){
    if (CropSnapshot.Num() > MaxCrops || AnimalSnapshot.Num() > MaxAnimals ||
        AnimalPenSnapshot.Num() > MaxAnimalPens || MaxAnimalPens <= 0 || MaxAnimalPens > 4096 ||
        FieldCellSnapshot.Num() > MaxFieldCells || MaxFieldCells <= 0 || MaxFieldCells > 65536 ||
        MaxFieldActionReceiptsPerCell <= 0 || MaxFieldActionReceiptsPerCell > 512 ||
        !FMath::IsFinite(SoilExhaustionPerHarvest) || SoilExhaustionPerHarvest < 0.0f || SoilExhaustionPerHarvest > 1.0f ||
        MaxCrops <= 0 || MaxAnimals <= 0 ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256) return false;
    TMap<FGuid, FWorldCropState> NewCrops;
    TMap<FGuid, FWorldAnimalState> NewAnimals;
    TMap<FIntVector, FWorldFieldCellState> NewFieldCells;
    TMap<FGuid, FWorldAnimalPenState> NewAnimalPens;
    TSet<FGuid> PenMemberAnimalIds;
    for (const FWorldAnimalPenState& Pen : AnimalPenSnapshot)
    {
        if (!Pen.PenId.IsValid() || Pen.Location.ContainsNaN() || Pen.Capacity <= 0 ||
            Pen.Capacity > 128 || Pen.Revision <= 0 || Pen.AnimalIds.Num() > Pen.Capacity ||
            NewAnimalPens.Contains(Pen.PenId))
            return false;
        TSet<FName> AllowedDefinitions;
        for (const FName DefinitionId : Pen.AllowedAnimalDefinitionIds)
        {
            if (DefinitionId.IsNone() || AllowedDefinitions.Contains(DefinitionId))
                return false;
            AllowedDefinitions.Add(DefinitionId);
        }
        TSet<FGuid> LocalAnimalIds;
        for (const FGuid& AnimalId : Pen.AnimalIds)
        {
            if (!AnimalId.IsValid() || LocalAnimalIds.Contains(AnimalId) ||
                PenMemberAnimalIds.Contains(AnimalId))
                return false;
            LocalAnimalIds.Add(AnimalId);
            PenMemberAnimalIds.Add(AnimalId);
        }
        NewAnimalPens.Add(Pen.PenId, Pen);
    }
    TSet<FGuid> SeenFieldOperations;
    for (const FWorldFieldCellState& Field : FieldCellSnapshot)
    {
        if (Field.Revision < 0 || Field.ActionReceipts.Num() > MaxFieldActionReceiptsPerCell ||
            !FMath::IsFinite(Field.Fertility) || Field.Fertility < 0.0f || Field.Fertility > 1.0f ||
            !FMath::IsFinite(Field.WeedPressure) || Field.WeedPressure < 0.0f || Field.WeedPressure > 1.0f ||
            !FMath::IsFinite(Field.Mulch) || Field.Mulch < 0.0f || Field.Mulch > 1.0f ||
            !FMath::IsFinite(Field.Moisture) || Field.Moisture < 0.0f || Field.Moisture > 1.0f ||
            Field.HarvestCount < 0 || (Field.Revision == 0 && !Field.ActionReceipts.IsEmpty()) ||
            NewFieldCells.Contains(Field.Cell))
            return false;
        for (const FWorldFieldActionReceipt& Receipt : Field.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || SeenFieldOperations.Contains(Receipt.OperationId) ||
                Receipt.Cell != Field.Cell || Receipt.ExpectedRevision < 0 ||
                Receipt.ResultingRevision != Receipt.ExpectedRevision + 1 ||
                Receipt.ResultingRevision > Field.Revision ||
                static_cast<uint8>(Receipt.Action) > static_cast<uint8>(EWorldFieldActionKind::Fence) ||
                !FMath::IsFinite(Receipt.Amount) || Receipt.Amount < 0.0f || Receipt.Amount > 1.0f)
                return false;
            SeenFieldOperations.Add(Receipt.OperationId);
        }
        NewFieldCells.Add(Field.Cell, Field);
    }
    for (const FWorldCropState& C : CropSnapshot)
    {
        if (!C.CropId.IsValid() || C.CropDefinitionId.IsNone() || C.Revision <= 0 ||
            static_cast<uint8>(C.Stage) > static_cast<uint8>(ECropGrowthStage::Harvested) ||
            !FMath::IsFinite(C.Moisture) || C.Moisture < 0.0f || C.Moisture > 1.0f ||
            !FMath::IsFinite(C.GrowthProgressSeconds) || C.GrowthProgressSeconds < 0.0 ||
            !FMath::IsFinite(C.SecondsPerStage) || C.SecondsPerStage <= 0.0 ||
            C.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
            (C.PlantOperationId.IsValid() &&
                (!C.SeedItemId.IsValid() || !C.SeedInventoryOwnerId.IsValid() || C.SeedItemExpectedRevision <= 0)) ||
            C.SeedReturnUnits < 0 || C.SeedReturnUnits > 64 ||
            C.HarvestUnits + C.SeedReturnUnits > 64 ||
            (C.SeedReturnUnits > 0 && C.SeedDefinitionId.IsNone()) ||
            (C.HarvestOperationId.IsValid() &&
                (C.Stage != ECropGrowthStage::Harvested || C.HarvestExpectedRevision <= 0 ||
                 C.HarvestDefinitionId.IsNone() || C.HarvestUnits <= 0 ||
                 C.HarvestOutputItemIds.Num() != C.HarvestUnits + C.SeedReturnUnits)) ||
            NewCrops.Contains(C.CropId)) return false;
        const bool bHasPlantReceipt = C.PlantOperationId.IsValid() &&
            C.ActionReceipts.ContainsByPredicate([&C](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::PlantCrop &&
                    Receipt.OperationId == C.PlantOperationId;
            });
        const bool bHasHarvestReceipt = C.HarvestOperationId.IsValid() &&
            C.ActionReceipts.ContainsByPredicate([&C](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::HarvestCrop &&
                    Receipt.OperationId == C.HarvestOperationId;
            });
        if ((C.PlantOperationId.IsValid() && !bHasPlantReceipt) ||
            (C.HarvestOperationId.IsValid() && !bHasHarvestReceipt) ||
            (C.Stage == ECropGrowthStage::Harvested && !bHasHarvestReceipt))
            return false;
        NewCrops.Add(C.CropId, C);
    }
    TSet<FGuid> SourceWildlifeIds;
    for (const FWorldAnimalState& A : AnimalSnapshot)
    {
        if (!A.AnimalId.IsValid() || A.AnimalDefinitionId.IsNone() || A.Revision <= 0 ||
            static_cast<uint8>(A.LifecycleState) > static_cast<uint8>(EWorldAnimalLifecycleState::Slaughtered) ||
            !FMath::IsFinite(A.Hunger) || A.Hunger < 0.0f || A.Hunger > 100.0f ||
            !FMath::IsFinite(A.Health) || A.Health < 0.0f || A.Health > 100.0f ||
            A.ProductUnits < 0 || !FMath::IsFinite(A.ProductProgressSeconds) ||
            A.ProductProgressSeconds < 0.0 || A.ProductProgressSeconds >= 3600.0 ||
            A.Location.ContainsNaN() || A.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
            NewAnimals.Contains(A.AnimalId) ||
            (A.LifecycleState == EWorldAnimalLifecycleState::Penned && !A.PenId.IsValid()) ||
            (A.LifecycleState != EWorldAnimalLifecycleState::Penned && A.PenId.IsValid()) ||
            (A.LifecycleState == EWorldAnimalLifecycleState::Slaughtered && A.Health > 0.0f))
            return false;
        if (A.SourceWildlifeIdentityId.IsValid())
        {
            if (SourceWildlifeIds.Contains(A.SourceWildlifeIdentityId))
                return false;
            SourceWildlifeIds.Add(A.SourceWildlifeIdentityId);
        }
        NewAnimals.Add(A.AnimalId, A);
    }

    for (const TPair<FGuid, FWorldAnimalPenState>& Pair : NewAnimalPens)
    {
        const FWorldAnimalPenState& Pen = Pair.Value;
        for (const FGuid& AnimalId : Pen.AnimalIds)
        {
            const FWorldAnimalState* Animal = NewAnimals.Find(AnimalId);
            if (!Animal || Animal->LifecycleState != EWorldAnimalLifecycleState::Penned ||
                Animal->PenId != Pen.PenId ||
                (!Pen.AllowedAnimalDefinitionIds.IsEmpty() &&
                 !Pen.AllowedAnimalDefinitionIds.Contains(Animal->AnimalDefinitionId)))
                return false;
        }
    }
    for (const TPair<FGuid, FWorldAnimalState>& Pair : NewAnimals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
        if (Animal.LifecycleState == EWorldAnimalLifecycleState::Penned)
        {
            const FWorldAnimalPenState* Pen = NewAnimalPens.Find(Animal.PenId);
            if (!Pen || !Pen->AnimalIds.Contains(Animal.AnimalId))
                return false;
        }
    }
    // The persisted lifecycle must be backed by the corresponding historical receipts.
    for (const TPair<FGuid, FWorldAnimalState>& Pair : NewAnimals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
        const bool bHasCapture = Animal.ActionReceipts.ContainsByPredicate(
            [&Animal](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::CaptureAnimal &&
                    Receipt.SourceWildlifeIdentityId == Animal.SourceWildlifeIdentityId;
            });
        const bool bHasMoveToCurrentPen = Animal.ActionReceipts.ContainsByPredicate(
            [&Animal](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::MoveAnimalToPen &&
                    Receipt.PenId == Animal.PenId;
            });
        const bool bHasSlaughter = Animal.ActionReceipts.ContainsByPredicate(
            [](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::SlaughterAnimal;
            });

        if (Animal.LifecycleState == EWorldAnimalLifecycleState::Managed)
        {
            if (Animal.SourceWildlifeIdentityId.IsValid() || Animal.PenId.IsValid() ||
                bHasCapture || bHasMoveToCurrentPen || bHasSlaughter)
                return false;
        }
        else if (Animal.LifecycleState == EWorldAnimalLifecycleState::Captured)
        {
            if (!Animal.SourceWildlifeIdentityId.IsValid() || !bHasCapture ||
                Animal.PenId.IsValid() || bHasMoveToCurrentPen || bHasSlaughter)
                return false;
        }
        else if (Animal.LifecycleState == EWorldAnimalLifecycleState::Penned)
        {
            if (!Animal.SourceWildlifeIdentityId.IsValid() || !bHasCapture ||
                !Animal.PenId.IsValid() || !bHasMoveToCurrentPen || bHasSlaughter)
                return false;
        }
        else if (Animal.LifecycleState == EWorldAnimalLifecycleState::Slaughtered)
        {
            if (!Animal.SourceWildlifeIdentityId.IsValid() || !bHasCapture || !bHasSlaughter ||
                Animal.PenId.IsValid() || Animal.Health > 0.0f || Animal.ProductUnits != 0)
                return false;
        }
        else
        {
            // Logical wildlife is represented by AWildlifeActor until capture;
            // only managed/captured/penned/slaughtered records are authoritative here.
            return false;
        }
    }

    TSet<FGuid> SeenOperationIds;
    TSet<FGuid> SeenOutputIds;
    bool bNeedsItemValidation = false;
    for (const TPair<FGuid, FWorldCropState>& Pair : NewCrops)
    {
        const FWorldCropState& Crop = Pair.Value;
        for (const FWorldAgricultureActionReceipt& Receipt : Crop.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || SeenOperationIds.Contains(Receipt.OperationId) ||
                Receipt.EntityId != Crop.CropId ||
                (Receipt.Action != EWorldAgricultureActionKind::PlantCrop &&
                 Receipt.Action != EWorldAgricultureActionKind::HarvestCrop))
                return false;
            if (Receipt.Action == EWorldAgricultureActionKind::PlantCrop)
            {
                if (Receipt.OperationId != Crop.PlantOperationId ||
                    Receipt.InputItemId != Crop.SeedItemId ||
                    Receipt.ExpectedInventoryOwnerId != Crop.SeedInventoryOwnerId ||
                    Receipt.ExpectedItemRevision != Crop.SeedItemExpectedRevision ||
                    Receipt.DefinitionId != Crop.CropDefinitionId ||
                    Receipt.ExpectedEntityRevision != 0 || Receipt.ResultingEntityRevision != 1 ||
                    Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                    !Receipt.InputItemId.IsValid() || !Receipt.ExpectedInventoryOwnerId.IsValid())
                    return false;
                bNeedsItemValidation = true;
            }
            else
            {
                if (Receipt.OperationId != Crop.HarvestOperationId ||
                    Receipt.ExpectedEntityRevision != Crop.HarvestExpectedRevision ||
                    Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                    Receipt.DefinitionId != Crop.HarvestDefinitionId ||
                    Receipt.RequestedOutputUnits != Crop.HarvestUnits + Crop.SeedReturnUnits ||
                    Receipt.OutputItemIds != Crop.HarvestOutputItemIds ||
                    Receipt.OutputItemIds.Num() != Receipt.RequestedOutputUnits ||
                    Receipt.OutputItemIds.IsEmpty())
                    return false;
                for (const FGuid& ItemId : Receipt.OutputItemIds)
                {
                    if (!ItemId.IsValid() || SeenOutputIds.Contains(ItemId))
                        return false;
                    SeenOutputIds.Add(ItemId);
                }
                bNeedsItemValidation = true;
            }
            SeenOperationIds.Add(Receipt.OperationId);
        }
    }

    for (const TPair<FGuid, FWorldAnimalState>& Pair : NewAnimals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
        for (const FWorldAgricultureActionReceipt& Receipt : Animal.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || SeenOperationIds.Contains(Receipt.OperationId) ||
                Receipt.EntityId != Animal.AnimalId ||
                (Receipt.Action != EWorldAgricultureActionKind::FeedAnimal &&
                 Receipt.Action != EWorldAgricultureActionKind::CollectAnimalProducts &&
                 Receipt.Action != EWorldAgricultureActionKind::CaptureAnimal &&
                 Receipt.Action != EWorldAgricultureActionKind::MoveAnimalToPen &&
                 Receipt.Action != EWorldAgricultureActionKind::SlaughterAnimal) ||
                Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                Receipt.ResultingEntityRevision > Animal.Revision)
                return false;

            if (Receipt.Action == EWorldAgricultureActionKind::FeedAnimal)
            {
                if (Receipt.ExpectedEntityRevision <= 0 ||
                    !Receipt.InputItemId.IsValid() || !Receipt.ExpectedInventoryOwnerId.IsValid() ||
                    Receipt.ExpectedItemRevision <= 0 ||
                    Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                    Receipt.DefinitionId.IsNone() || Receipt.DefinitionId != Animal.FeedDefinitionId ||
                    !FMath::IsFinite(Receipt.HungerReduction) ||
                    Receipt.HungerReduction <= 0.0f || Receipt.HungerReduction > 100.0f ||
                    !Receipt.OutputItemIds.IsEmpty())
                    return false;
                bNeedsItemValidation = true;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::CollectAnimalProducts)
            {
                if (Receipt.ExpectedEntityRevision <= 0 ||
                    Receipt.DefinitionId.IsNone() || Receipt.DefinitionId != Animal.ProductDefinitionId ||
                    Receipt.RequestedOutputUnits <= 0 || Receipt.RequestedOutputUnits > 64 ||
                    Receipt.OutputItemIds.Num() != Receipt.RequestedOutputUnits)
                    return false;
                for (const FGuid& ItemId : Receipt.OutputItemIds)
                {
                    if (!ItemId.IsValid() || SeenOutputIds.Contains(ItemId)) return false;
                    SeenOutputIds.Add(ItemId);
                }
                bNeedsItemValidation = true;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::CaptureAnimal)
            {
                if (Receipt.ExpectedEntityRevision != 0 || Receipt.ResultingEntityRevision != 1 ||
                    !Receipt.SourceWildlifeIdentityId.IsValid() ||
                    Receipt.SourceWildlifeIdentityId != Animal.SourceWildlifeIdentityId ||
                    Receipt.DefinitionId != Animal.AnimalDefinitionId ||
                    !Receipt.OutputItemIds.IsEmpty() || Receipt.InputItemId.IsValid())
                    return false;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::MoveAnimalToPen)
            {
                if (Receipt.ExpectedEntityRevision <= 0 ||
                    !Receipt.SourceWildlifeIdentityId.IsValid() ||
                    Receipt.SourceWildlifeIdentityId != Animal.SourceWildlifeIdentityId ||
                    !Receipt.PenId.IsValid() || Receipt.ExpectedPenRevision <= 0 ||
                    Receipt.ResultingPenRevision != Receipt.ExpectedPenRevision + 1 ||
                    !NewAnimalPens.Contains(Receipt.PenId) ||
                    Receipt.ResultingPenRevision > NewAnimalPens.FindChecked(Receipt.PenId).Revision ||
                    Receipt.DefinitionId != Animal.AnimalDefinitionId ||
                    !Receipt.OutputItemIds.IsEmpty())
                    return false;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::SlaughterAnimal)
            {
                if (Receipt.ExpectedEntityRevision <= 0 ||
                    !Receipt.SourceWildlifeIdentityId.IsValid() ||
                    Receipt.SourceWildlifeIdentityId != Animal.SourceWildlifeIdentityId ||
                    !Receipt.PenId.IsValid() || Receipt.ExpectedPenRevision <= 0 ||
                    Receipt.ResultingPenRevision != Receipt.ExpectedPenRevision + 1 ||
                    !NewAnimalPens.Contains(Receipt.PenId) ||
                    Receipt.ResultingPenRevision > NewAnimalPens.FindChecked(Receipt.PenId).Revision ||
                    Receipt.DefinitionId != Animal.AnimalDefinitionId ||
                    Receipt.RequestedOutputUnits != Animal.MeatUnits + Animal.HideUnits + Animal.BoneUnits ||
                    Receipt.RequestedOutputUnits <= 0 || Receipt.RequestedOutputUnits > 64 ||
                    Receipt.OutputItemIds.Num() != Receipt.RequestedOutputUnits ||
                    Animal.LifecycleState != EWorldAnimalLifecycleState::Slaughtered)
                    return false;
                for (const FGuid& ItemId : Receipt.OutputItemIds)
                {
                    if (!ItemId.IsValid() || SeenOutputIds.Contains(ItemId)) return false;
                    SeenOutputIds.Add(ItemId);
                }
                bNeedsItemValidation = true;
            }
            SeenOperationIds.Add(Receipt.OperationId);
        }
    }

    if (bNeedsItemValidation)
    {
        UWorldItemOwnershipSubsystem* Ownership =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
        if (!Ownership)
            return false;
        auto Exists = [Ownership](const FGuid& ItemId)
        {
            FWorldOwnedItemRecord Item;
            return ItemId.IsValid() && Ownership->TryGetItem(ItemId, Item);
        };
        for (const TPair<FGuid, FWorldCropState>& Pair : NewCrops)
        {
            const FWorldCropState& Crop = Pair.Value;
            if (Crop.PlantOperationId.IsValid() && !Exists(Crop.SeedItemId))
                return false;
            for (const FGuid& ItemId : Crop.HarvestOutputItemIds)
                if (!Exists(ItemId)) return false;
        }
        for (const TPair<FGuid, FWorldAnimalState>& Pair : NewAnimals)
        {
            for (const FWorldAgricultureActionReceipt& Receipt : Pair.Value.ActionReceipts)
            {
                if (Receipt.Action == EWorldAgricultureActionKind::FeedAnimal &&
                    !Exists(Receipt.InputItemId))
                    return false;
                for (const FGuid& ItemId : Receipt.OutputItemIds)
                    if (!Exists(ItemId)) return false;
            }
        }
    }

    Crops = MoveTemp(NewCrops);
    Animals = MoveTemp(NewAnimals);
    FieldCells = MoveTemp(NewFieldCells);
    AnimalPens = MoveTemp(NewAnimalPens);
    return true;
}
