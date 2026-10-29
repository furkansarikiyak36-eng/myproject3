#include "WorldAgricultureSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "InventoryCraftingComponent.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldCalendar.h"

const FWorldCropDefinition* UWorldAgricultureSubsystem::FindCropDefinition(FName CropDefinitionId) const
{
    if (CropDefinitionId.IsNone())
        return nullptr;
    return CropDefinitions.FindByPredicate([CropDefinitionId](const FWorldCropDefinition& Definition)
    {
        return Definition.CropDefinitionId == CropDefinitionId;
    });
}

const FWorldAnimalDefinition* UWorldAgricultureSubsystem::FindAnimalDefinition(FName AnimalDefinitionId) const
{
    if (AnimalDefinitionId.IsNone())
        return nullptr;
    return AnimalDefinitions.FindByPredicate([AnimalDefinitionId](const FWorldAnimalDefinition& Definition)
    {
        return Definition.AnimalDefinitionId == AnimalDefinitionId;
    });
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
    for (const TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
    {
        if (const FWorldAgricultureActionReceipt* Found =
            Pair.Value.ActionReceipts.FindByPredicate([OperationId](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.OperationId == OperationId;
            }))
            return Found;
    }
    for (const TPair<FGuid, FWorldAnimalPenState>& Pair : AnimalPens)
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
    if (Definition.CropDefinitionId.IsNone() || Definition.SeedDefinitionId.IsNone() ||
        Definition.HarvestDefinitionId.IsNone() || Definition.HarvestUnits <= 0 ||
        Definition.HarvestUnits > 64 || !FMath::IsFinite(Definition.SecondsPerStage) ||
        Definition.SecondsPerStage <= 0.0 ||
        !FMath::IsFinite(Definition.MinimumPlantMoisture) ||
        Definition.MinimumPlantMoisture < 0.0f || Definition.MinimumPlantMoisture > 1.0f ||
        Definition.SeedReturnUnits < 0 || Definition.SeedReturnUnits > 64 ||
        !FMath::IsFinite(Definition.SoilExhaustionPerHarvest) ||
        Definition.SoilExhaustionPerHarvest < 0.0f || Definition.SoilExhaustionPerHarvest > 100.0f ||
        !FMath::IsFinite(Definition.MinimumFertility) ||
        Definition.MinimumFertility < 0.0f || Definition.MinimumFertility > 100.0f ||
        !FMath::IsFinite(Definition.MaximumPlantWeedPressure) ||
        Definition.MaximumPlantWeedPressure < 0.0f || Definition.MaximumPlantWeedPressure > 1.0f ||
        Definition.PlantMonths.IsEmpty() || MaxCropDefinitions <= 0 ||
        MaxCropDefinitions > 4096 || CropDefinitions.Num() >= MaxCropDefinitions ||
        FindCropDefinition(Definition.CropDefinitionId))
        return false;

    TSet<int32> SeenMonths;
    for (const int32 Month : Definition.PlantMonths)
    {
        if (Month < 1 || Month > 12 || SeenMonths.Contains(Month))
            return false;
        SeenMonths.Add(Month);
    }
    CropDefinitions.Add(Definition);
    return true;
}

bool UWorldAgricultureSubsystem::RegisterAnimalDefinition(const FWorldAnimalDefinition& Definition)
{
    if (Definition.AnimalDefinitionId.IsNone() || Definition.FeedDefinitionId.IsNone() ||
        Definition.CaptureToolDefinitionIds.IsEmpty() ||
        !FMath::IsFinite(Definition.ProductIntervalSeconds) || Definition.ProductIntervalSeconds <= 0.0 ||
        Definition.ProductUnitsPerInterval <= 0 || Definition.ProductUnitsPerInterval > 64 ||
        (Definition.ProductDefinitionId.IsNone() == false &&
            (Definition.ProductIntervalSeconds < 1.0 || Definition.ProductUnitsPerInterval < 1)) ||
        Definition.SlaughterYields.IsEmpty() || AnimalDefinitions.ContainsByPredicate(
            [&Definition](const FWorldAnimalDefinition& Existing)
            { return Existing.AnimalDefinitionId == Definition.AnimalDefinitionId; }) ||
        AnimalDefinitions.Num() >= 256)
        return false;

    TSet<FName> SeenTools;
    for (const FName ToolId : Definition.CaptureToolDefinitionIds)
    {
        if (ToolId.IsNone() || SeenTools.Contains(ToolId))
            return false;
        SeenTools.Add(ToolId);
    }
    TSet<FName> SeenYieldIds;
    int32 TotalYieldUnits = 0;
    for (const FWorldAnimalYieldDefinition& Yield : Definition.SlaughterYields)
    {
        if (Yield.ItemDefinitionId.IsNone() || Yield.Units <= 0 || Yield.Units > 64 ||
            SeenYieldIds.Contains(Yield.ItemDefinitionId) || TotalYieldUnits > 64 - Yield.Units)
            return false;
        SeenYieldIds.Add(Yield.ItemDefinitionId);
        TotalYieldUnits += Yield.Units;
    }
    AnimalDefinitions.Add(Definition);
    return true;
}

bool UWorldAgricultureSubsystem::TryPlantCrop(
    const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
    int64 ExpectedSeedItemRevision, const FGuid& ExpectedInventoryOwnerId,
    const FIntVector& PlotCell, float InitialMoisture, FGuid& OutCropId)
{
    FWorldFieldCellState FieldCell;
    if (!GetFieldCell(PlotCell, FieldCell))
    {
        OutCropId.Invalidate();
        return false;
    }
    return TryPlantCropOnField(OperationId, CropDefinitionId, SeedItemId,
        ExpectedSeedItemRevision, ExpectedInventoryOwnerId, PlotCell,
        FieldCell.Revision, InitialMoisture, OutCropId);
}

bool UWorldAgricultureSubsystem::TryPlantCropOnField(
    const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
    int64 ExpectedSeedItemRevision, const FGuid& ExpectedInventoryOwnerId,
    const FIntVector& PlotCell, int64 ExpectedFieldRevision,
    float InitialMoisture, FGuid& OutCropId)
{
    OutCropId.Invalidate();
    if (!OperationId.IsValid() || CropDefinitionId.IsNone() || !SeedItemId.IsValid() ||
        ExpectedSeedItemRevision <= 0 || !ExpectedInventoryOwnerId.IsValid() ||
        ExpectedFieldRevision <= 0 || !FMath::IsFinite(InitialMoisture) ||
        InitialMoisture < 0.0f || InitialMoisture > 1.0f)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldAgricultureActionKind::PlantCrop ||
            Existing->InputItemId != SeedItemId ||
            Existing->ExpectedItemRevision != ExpectedSeedItemRevision ||
            Existing->ExpectedInventoryOwnerId != ExpectedInventoryOwnerId ||
            Existing->DefinitionId != CropDefinitionId)
            return false;
        const FWorldCropState* ExistingCrop = Crops.Find(Existing->EntityId);
        if (!ExistingCrop || ExistingCrop->PlotCell != PlotCell)
            return false;
        OutCropId = Existing->EntityId;
        return true;
    }

    const FWorldCropDefinition* Definition = FindCropDefinition(CropDefinitionId);
    FWorldFieldCellState* FieldCell = FieldCells.Find(PlotCell);
    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    FWorldCalendarDate Date;
    if (!Definition || !FieldCell || !Clock || !Clock->GetCalendarDate(Date) ||
        !Definition->PlantMonths.Contains(Date.Month) ||
        FieldCell->Revision != ExpectedFieldRevision ||
        FieldCell->Revision >= MAX_int64 - 1 ||
        !FieldCell->bCleared || !FieldCell->bTilled || FieldCell->PlantedCropId.IsValid() ||
        FieldCell->Fertility < Definition->MinimumFertility ||
        FieldCell->WeedPressure > Definition->MaximumPlantWeedPressure ||
        FieldCell->Moisture < Definition->MinimumPlantMoisture ||
        InitialMoisture < Definition->MinimumPlantMoisture ||
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
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    FWorldOwnedItemRecord Seed;
    if (!Ownership || !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        !Ownership->TryGetItem(SeedItemId, Seed) ||
        Seed.Revision != ExpectedSeedItemRevision ||
        Seed.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Seed.OwnerId != ExpectedInventoryOwnerId || Seed.DefinitionId != Definition->SeedDefinitionId ||
        Seed.Quantity != 1 || Seed.Revision >= MAX_int64 - 1 ||
        FieldCell->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    FWorldCropState Crop;
    Crop.CropId = FGuid::NewGuid();
    Crop.CropDefinitionId = CropDefinitionId;
    Crop.PlotCell = PlotCell;
    Crop.Stage = ECropGrowthStage::Seed;
    Crop.Moisture = FieldCell->Moisture;
    Crop.SecondsPerStage = Definition->SecondsPerStage;
    Crop.Revision = 1;
    Crop.PlantOperationId = OperationId;
    Crop.SeedItemId = SeedItemId;
    Crop.SeedInventoryOwnerId = ExpectedInventoryOwnerId;
    Crop.SeedItemExpectedRevision = ExpectedSeedItemRevision;
    Crop.HarvestDefinitionId = Definition->HarvestDefinitionId;
    Crop.HarvestUnits = Definition->HarvestUnits;
    Crop.SeedReturnUnits = Definition->SeedReturnUnits;
    Crop.SoilExhaustionPerHarvest = Definition->SoilExhaustionPerHarvest;
    Crop.MinimumFertility = Definition->MinimumFertility;
    Crop.SeedOutputDefinitionId = Definition->SeedDefinitionId;

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
    FieldCell->PlantedCropId = Crop.CropId;
    ++FieldCell->Revision;
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
    if (!Crop || Crop->Revision != ExpectedCropRevision ||
        Crop->Revision >= MAX_int64 - 1 || Crop->Stage != ECropGrowthStage::Mature ||
        Crop->HarvestDefinitionId.IsNone() || Crop->HarvestUnits <= 0 ||
        Crop->HarvestUnits > 64 || Crop->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    FWorldFieldCellState* FieldCell = FieldCells.Find(Crop->PlotCell);
    if (FieldCell && (FieldCell->PlantedCropId != CropId ||
        FieldCell->Revision >= MAX_int64 - 1 ||
        FieldCell->ConsecutiveSameCropHarvests >= MAX_int32 - 1))
        return false;
    if (Crop->SeedReturnUnits < 0 || Crop->SeedReturnUnits > 64 ||
        (Crop->SeedReturnUnits > 0 && Crop->SeedOutputDefinitionId.IsNone()))
        return false;

    const int32 HarvestUnits = Crop->HarvestUnits;
    const int32 SeedUnits = Crop->SeedReturnUnits;
    TArray<FName> Definitions;
    TArray<FVector> Locations;
    Definitions.Reserve(HarvestUnits + SeedUnits);
    Locations.Reserve(HarvestUnits + SeedUnits);
    const FVector PlotOrigin(
        static_cast<double>(Crop->PlotCell.X) * 100.0,
        static_cast<double>(Crop->PlotCell.Y) * 100.0,
        static_cast<double>(Crop->PlotCell.Z) * 100.0);

    for (int32 Index = 0; Index < HarvestUnits; ++Index)
    {
        Definitions.Add(Crop->HarvestDefinitionId);
        Locations.Add(PlotOrigin + FVector(static_cast<double>(Index) * 25.0, 0.0, 10.0));
    }
    for (int32 Index = 0; Index < SeedUnits; ++Index)
    {
        Definitions.Add(Crop->SeedOutputDefinitionId);
        Locations.Add(PlotOrigin + FVector(static_cast<double>(HarvestUnits + Index) * 25.0, 15.0, 10.0));
    }

    TArray<FGuid> CreatedItems;
    if (!RegisterWorldOutputsAtomically(Definitions, Locations, CreatedItems))
        return false;

    TArray<FGuid> HarvestItems;
    TArray<FGuid> SeedItems;
    HarvestItems.Append(CreatedItems.GetData(), HarvestUnits);
    if (SeedUnits > 0)
        SeedItems.Append(CreatedItems.GetData() + HarvestUnits, SeedUnits);

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::HarvestCrop;
    Receipt.EntityId = CropId;
    Receipt.ExpectedEntityRevision = ExpectedCropRevision;
    Receipt.ResultingEntityRevision = ExpectedCropRevision + 1;
    Receipt.DefinitionId = Crop->HarvestDefinitionId;
    Receipt.RequestedOutputUnits = HarvestUnits;
    Receipt.OutputItemIds = CreatedItems;

    Crop->Stage = ECropGrowthStage::Harvested;
    Crop->HarvestOperationId = OperationId;
    Crop->HarvestExpectedRevision = ExpectedCropRevision;
    Crop->HarvestOutputItemIds = HarvestItems;
    Crop->SeedOutputItemIds = SeedItems;
    ++Crop->Revision;
    Crop->ActionReceipts.Add(Receipt);

    if (FieldCell)
    {
        FieldCell->PlantedCropId.Invalidate();
        if (FieldCell->LastHarvestedCropDefinitionId == Crop->CropDefinitionId)
            ++FieldCell->ConsecutiveSameCropHarvests;
        else
        {
            FieldCell->LastHarvestedCropDefinitionId = Crop->CropDefinitionId;
            FieldCell->ConsecutiveSameCropHarvests = 1;
        }
        const float ExhaustionMultiplier = 1.0f +
            0.25f * static_cast<float>(FMath::Max(0, FieldCell->ConsecutiveSameCropHarvests - 1));
        FieldCell->Fertility = FMath::Clamp(
            FieldCell->Fertility - Crop->SoilExhaustionPerHarvest * ExhaustionMultiplier, 0.0f, 100.0f);
        ++FieldCell->Revision;
    }

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
    FWorldAnimalPenState* Pen = Animal && Animal->PenId.IsValid()
        ? AnimalPens.Find(Animal->PenId) : nullptr;
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    FWorldOwnedItemRecord FeedItem;
    if (!Animal || !Pen || !Ownership || !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        Animal->Revision != ExpectedAnimalRevision || Animal->Revision >= MAX_int64 - 1 ||
        Animal->Health <= 0.0f || !Animal->bCaptured || Animal->bSlaughtered ||
        !Pen->bGateClosed || !Pen->AnimalIds.Contains(AnimalId) || Animal->FeedDefinitionId.IsNone() ||
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
    FWorldAnimalPenState* Pen = Animal && Animal->PenId.IsValid()
        ? AnimalPens.Find(Animal->PenId) : nullptr;
    if (!Animal || !Pen || Animal->Revision != ExpectedAnimalRevision ||
        Animal->Revision >= MAX_int64 - 1 || Animal->Health <= 0.0f ||
        !Animal->bCaptured || Animal->bSlaughtered || !Pen->bGateClosed ||
        !Pen->AnimalIds.Contains(AnimalId) ||
        Animal->ProductDefinitionId.IsNone() || Animal->ProductUnits <= 0 ||
        Animal->ProductUnits > 64 ||
        Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    const int32 Units = Animal->ProductUnits;
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

    Animal->ProductUnits = 0;
    ++Animal->Revision;
    Animal->ActionReceipts.Add(Receipt);
    OutItemIds = MoveTemp(CreatedItems);
    return true;
}

bool UWorldAgricultureSubsystem::TryCaptureAnimal(
    const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
    const FGuid& CaptureToolItemId, int64 ExpectedToolItemRevision,
    const FGuid& ExpectedInventoryOwnerId)
{
    if (!OperationId.IsValid() || !AnimalId.IsValid() || ExpectedAnimalRevision <= 0 ||
        !CaptureToolItemId.IsValid() || ExpectedToolItemRevision <= 0 ||
        !ExpectedInventoryOwnerId.IsValid())
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        return Existing->Action == EWorldAgricultureActionKind::CaptureAnimal &&
            Existing->EntityId == AnimalId &&
            Existing->InputItemId == CaptureToolItemId &&
            Existing->ExpectedInventoryOwnerId == ExpectedInventoryOwnerId &&
            Existing->ExpectedEntityRevision == ExpectedAnimalRevision &&
            Existing->ExpectedItemRevision == ExpectedToolItemRevision;
    }

    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    const FWorldAnimalDefinition* Definition =
        Animal ? FindAnimalDefinition(Animal->AnimalDefinitionId) : nullptr;
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    FWorldOwnedItemRecord Tool;
    if (!Animal || !Definition || !Ownership ||
        !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        Animal->Revision != ExpectedAnimalRevision || Animal->Revision >= MAX_int64 - 1 ||
        Animal->bCaptured || Animal->bSlaughtered || Animal->Health <= 0.0f ||
        Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity ||
        !Ownership->TryGetItem(CaptureToolItemId, Tool) ||
        Tool.Revision != ExpectedToolItemRevision || Tool.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Tool.OwnerId != ExpectedInventoryOwnerId || Tool.Quantity != 1 ||
        !Definition->CaptureToolDefinitionIds.Contains(Tool.DefinitionId) ||
        Tool.Revision >= MAX_int64 - 1 ||
        !Inventory->Items.ContainsByPredicate([CaptureToolItemId](const FInventoryStack& Stack)
        {
            return Stack.OwnedWorldItemId == CaptureToolItemId && Stack.Quantity == 1;
        }))
        return false;

    Animal->bCaptured = true;
    Animal->CaptureOperationId = OperationId;
    Animal->CaptureToolItemId = CaptureToolItemId;
    Animal->CaptureInventoryOwnerId = ExpectedInventoryOwnerId;
    Animal->CaptureToolExpectedRevision = ExpectedToolItemRevision;
    ++Animal->Revision;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::CaptureAnimal;
    Receipt.EntityId = AnimalId;
    Receipt.InputItemId = CaptureToolItemId;
    Receipt.ExpectedInventoryOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedEntityRevision = ExpectedAnimalRevision;
    Receipt.ExpectedItemRevision = ExpectedToolItemRevision;
    Receipt.ResultingEntityRevision = Animal->Revision;
    Receipt.ResultingItemRevision = ExpectedToolItemRevision; // Tool is reusable and not consumed.
    Receipt.DefinitionId = Animal->AnimalDefinitionId;
    Animal->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldAgricultureSubsystem::TryBuildAnimalPen(
    const FGuid& OperationId, const FGuid& ExpectedInventoryOwnerId,
    const FVector& Location, int32 Capacity, FGuid& OutPenId)
{
    OutPenId.Invalidate();
    if (!OperationId.IsValid() || !ExpectedInventoryOwnerId.IsValid() ||
        Location.ContainsNaN() || Capacity < 1 || Capacity > 4 ||
        MaxAnimalPens <= 0 || MaxAnimalPens > 1024 ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldAgricultureActionKind::BuildAnimalPen ||
            Existing->ExpectedInventoryOwnerId != ExpectedInventoryOwnerId ||
            Existing->DefinitionId != TEXT("AnimalPen") ||
            Existing->ExpectedEntityRevision != 0 || Existing->WorkAmount != Capacity)
            return false;
        const FWorldAnimalPenState* Pen = AnimalPens.Find(Existing->EntityId);
        if (!Pen || !Pen->Location.Equals(Location, 0.1f) || Pen->Capacity != Capacity)
            return false;
        OutPenId = Pen->PenId;
        return true;
    }

    if (AnimalPens.Num() >= MaxAnimalPens)
        return false;
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    if (!Ownership || !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        Inventory->GetItemCount(TEXT("Stake")) < 4 || Inventory->GetItemCount(TEXT("Rope")) < 1 ||
        Inventory->Items.Num() > Inventory->MaxSlots)
        return false;

    FGuid OwnerActorId;
    FGuid InventoryOwnerId;
    TArray<FInventoryStack> BeforeInventory;
    FName BeforeQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, InventoryOwnerId, BeforeInventory, BeforeQuickAccess);
    TArray<FWorldOwnedItemRecord> BeforeItems;
    TArray<FWorldItemTransferReceipt> BeforeTransfers;
    TArray<FWorldItemSplitReceipt> BeforeSplits;
    Ownership->ExportSnapshot(BeforeItems, BeforeTransfers, BeforeSplits);
    auto RollbackMaterials = [&]()
    {
        const bool bItemsRestored =
            Ownership->RestoreSnapshot(BeforeItems, BeforeTransfers, BeforeSplits);
        const bool bInventoryRestored =
            Inventory->RestoreSnapshot(InventoryOwnerId, BeforeInventory, BeforeQuickAccess);
        return bItemsRestored && bInventoryRestored;
    };

    if (!Inventory->TryRemoveItem(TEXT("Stake"), 4) ||
        !Inventory->TryRemoveItem(TEXT("Rope"), 1))
    {
        if (!RollbackMaterials())
            UE_LOG(LogTemp, Error, TEXT("Failed to roll back inventory after animal-pen construction was refused."));
        return false;
    }

    FWorldAnimalPenState Pen;
    Pen.PenId = FGuid::NewGuid();
    Pen.Location = Location;
    Pen.Capacity = Capacity;
    Pen.StakeUnits = 4;
    Pen.RopeUnits = 1;
    Pen.bGateClosed = false;
    Pen.Revision = 1;
    if (!Pen.PenId.IsValid())
    {
        if (!RollbackMaterials())
            UE_LOG(LogTemp, Error, TEXT("Failed to roll back inventory after an invalid animal-pen ID."));
        return false;
    }

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::BuildAnimalPen;
    Receipt.EntityId = Pen.PenId;
    Receipt.ExpectedInventoryOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedEntityRevision = 0;
    Receipt.ResultingEntityRevision = 1;
    Receipt.DefinitionId = TEXT("AnimalPen");
    Receipt.WorkAmount = static_cast<float>(Capacity);
    Pen.ActionReceipts.Add(Receipt);

    AnimalPens.Add(Pen.PenId, Pen);
    OutPenId = Pen.PenId;
    return true;
}

bool UWorldAgricultureSubsystem::TryAssignAnimalToPen(
    const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
    const FGuid& PenId, int64 ExpectedPenRevision)
{
    if (!OperationId.IsValid() || !AnimalId.IsValid() || ExpectedAnimalRevision <= 0 ||
        !PenId.IsValid() || ExpectedPenRevision <= 0)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        const FWorldAnimalState* ExistingAnimal = Animals.Find(AnimalId);
        const FWorldAnimalPenState* ExistingPen = AnimalPens.Find(PenId);
        return Existing->Action == EWorldAgricultureActionKind::AssignAnimalToPen &&
            Existing->EntityId == AnimalId && Existing->RelatedEntityId == PenId &&
            Existing->ExpectedEntityRevision == ExpectedAnimalRevision &&
            Existing->ExpectedRelatedEntityRevision == ExpectedPenRevision &&
            ExistingAnimal && ExistingPen && ExistingAnimal->PenId == PenId &&
            ExistingPen->AnimalIds.Contains(AnimalId);
    }

    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    FWorldAnimalPenState* Pen = AnimalPens.Find(PenId);
    if (!Animal || !Pen || Animal->Revision != ExpectedAnimalRevision ||
        Pen->Revision != ExpectedPenRevision || Animal->Revision >= MAX_int64 - 1 ||
        Pen->Revision >= MAX_int64 - 1 || Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity ||
        !Animal->bCaptured || Animal->bSlaughtered || Animal->Health <= 0.0f ||
        Animal->PenId.IsValid() || Pen->bGateClosed || Pen->AnimalIds.Contains(AnimalId) ||
        Pen->AnimalIds.Num() >= Pen->Capacity ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256)
        return false;

    Animal->PenId = PenId;
    Pen->AnimalIds.Add(AnimalId);
    Pen->AnimalIds.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
    const int64 NewPenRevision = ExpectedPenRevision + 1;
    const int64 NewAnimalRevision = ExpectedAnimalRevision + 1;
    Pen->Revision = NewPenRevision;
    Animal->Revision = NewAnimalRevision;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::AssignAnimalToPen;
    Receipt.EntityId = AnimalId;
    Receipt.RelatedEntityId = PenId;
    Receipt.ExpectedEntityRevision = ExpectedAnimalRevision;
    Receipt.ResultingEntityRevision = NewAnimalRevision;
    Receipt.ExpectedRelatedEntityRevision = ExpectedPenRevision;
    Receipt.ResultingRelatedEntityRevision = NewPenRevision;
    Receipt.DefinitionId = TEXT("AnimalPen");
    Animal->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldAgricultureSubsystem::TrySetAnimalPenGate(
    const FGuid& OperationId, const FGuid& PenId, int64 ExpectedPenRevision, bool bClosed)
{
    if (!OperationId.IsValid() || !PenId.IsValid() || ExpectedPenRevision <= 0)
        return false;

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        const FWorldAnimalPenState* ExistingPen = AnimalPens.Find(PenId);
        return Existing->Action == EWorldAgricultureActionKind::SetAnimalPenGate &&
            Existing->EntityId == PenId && Existing->ExpectedEntityRevision == ExpectedPenRevision &&
            Existing->WorkAmount == (bClosed ? 1.0f : 0.0f) &&
            ExistingPen != nullptr;
    }

    FWorldAnimalPenState* Pen = AnimalPens.Find(PenId);
    if (!Pen || Pen->Revision != ExpectedPenRevision || Pen->Revision >= MAX_int64 - 1 ||
        Pen->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256)
        return false;
    if (!bClosed && !Pen->AnimalIds.IsEmpty())
        return false;

    if (bClosed)
    {
        for (const FGuid& AnimalId : Pen->AnimalIds)
        {
            const FWorldAnimalState* Animal = Animals.Find(AnimalId);
            if (!Animal || Animal->PenId != PenId || !Animal->bCaptured ||
                Animal->bSlaughtered || Animal->Health <= 0.0f)
                return false;
        }
    }

    Pen->bGateClosed = bClosed;
    ++Pen->Revision;
    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::SetAnimalPenGate;
    Receipt.EntityId = PenId;
    Receipt.ExpectedEntityRevision = ExpectedPenRevision;
    Receipt.ResultingEntityRevision = Pen->Revision;
    Receipt.DefinitionId = TEXT("Gate");
    Receipt.WorkAmount = bClosed ? 1.0f : 0.0f;
    Pen->ActionReceipts.Add(Receipt);
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
            Existing->EntityId != AnimalId || Existing->ExpectedEntityRevision != ExpectedAnimalRevision)
            return false;
        OutItemIds = Existing->OutputItemIds;
        return !OutItemIds.IsEmpty();
    }

    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    const FWorldAnimalDefinition* Definition =
        Animal ? FindAnimalDefinition(Animal->AnimalDefinitionId) : nullptr;
    FWorldAnimalPenState* Pen = Animal && Animal->PenId.IsValid()
        ? AnimalPens.Find(Animal->PenId) : nullptr;
    if (!Animal || !Definition || !Pen || Animal->Revision != ExpectedAnimalRevision ||
        Animal->Revision >= MAX_int64 - 1 || Pen->Revision >= MAX_int64 - 1 ||
        !Animal->bCaptured || Animal->bSlaughtered || Animal->Health <= 0.0f ||
        !Pen->bGateClosed || !Pen->AnimalIds.Contains(AnimalId) ||
        Animal->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity ||
        Pen->AnimalIds.Num() <= 0 || Definition->SlaughterYields.IsEmpty())
        return false;

    TArray<FName> OutputDefinitions;
    TArray<FVector> OutputLocations;
    int32 TotalUnits = 0;
    for (const FWorldAnimalYieldDefinition& Yield : Definition->SlaughterYields)
    {
        if (Yield.ItemDefinitionId.IsNone() || Yield.Units <= 0 || Yield.Units > 64 ||
            TotalUnits > 64 - Yield.Units)
            return false;
        for (int32 Unit = 0; Unit < Yield.Units; ++Unit)
        {
            OutputDefinitions.Add(Yield.ItemDefinitionId);
            OutputLocations.Add(Animal->Location + FVector(static_cast<double>(TotalUnits) * 12.0, 0.0, 8.0));
            ++TotalUnits;
        }
    }
    if (TotalUnits <= 0 || TotalUnits > 64)
        return false;

    TArray<FGuid> CreatedItems;
    if (!RegisterWorldOutputsAtomically(OutputDefinitions, OutputLocations, CreatedItems))
        return false;

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = EWorldAgricultureActionKind::SlaughterAnimal;
    Receipt.EntityId = AnimalId;
    Receipt.RelatedEntityId = Pen->PenId;
    Receipt.ExpectedEntityRevision = ExpectedAnimalRevision;
    Receipt.ResultingEntityRevision = ExpectedAnimalRevision + 1;
    Receipt.ExpectedRelatedEntityRevision = Pen->Revision;
    Receipt.ResultingRelatedEntityRevision = Pen->Revision + 1;
    Receipt.DefinitionId = Animal->AnimalDefinitionId;
    Receipt.RequestedOutputUnits = TotalUnits;
    Receipt.OutputItemIds = CreatedItems;

    Animal->bSlaughtered = true;
    Animal->Health = 0.0f;
    Animal->SlaughterOperationId = OperationId;
    Animal->SlaughterOutputItemIds = CreatedItems;
    Animal->PenId.Invalidate();
    ++Animal->Revision;
    Animal->ActionReceipts.Add(Receipt);

    Pen->AnimalIds.Remove(AnimalId);
    ++Pen->Revision;
    OutItemIds = MoveTemp(CreatedItems);
    return true;
}

bool UWorldAgricultureSubsystem::GetAnimalPen(
    const FGuid& PenId, FWorldAnimalPenState& OutPen) const
{
    const FWorldAnimalPenState* Found = AnimalPens.Find(PenId);
    if (!Found)
        return false;
    OutPen = *Found;
    return true;
}

bool UWorldAgricultureSubsystem::RegisterFieldCell(const FWorldFieldCellState& FieldCell)
{
    if (FieldCells.Contains(FieldCell.Cell) ||
        !FMath::IsFinite(FieldCell.Fertility) || FieldCell.Fertility < 0.0f || FieldCell.Fertility > 100.0f ||
        !FMath::IsFinite(FieldCell.WeedPressure) || FieldCell.WeedPressure < 0.0f || FieldCell.WeedPressure > 1.0f ||
        !FMath::IsFinite(FieldCell.MulchCoverage) || FieldCell.MulchCoverage < 0.0f || FieldCell.MulchCoverage > 1.0f ||
        !FMath::IsFinite(FieldCell.Moisture) || FieldCell.Moisture < 0.0f || FieldCell.Moisture > 1.0f ||
        FieldCell.Revision <= 0 || FieldCell.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256 ||
        MaxFieldCells <= 0 || MaxFieldCells > 100000 || FieldCells.Num() >= MaxFieldCells ||
        (FieldCell.PlantedCropId.IsValid() && !Crops.Contains(FieldCell.PlantedCropId)))
        return false;

    FWorldFieldCellState NewCell = FieldCell;
    if (!NewCell.FieldCellId.IsValid())
        NewCell.FieldCellId = FGuid::NewGuid();
    if (!NewCell.FieldCellId.IsValid())
        return false;
    for (const TPair<FIntVector, FWorldFieldCellState>& Pair : FieldCells)
        if (Pair.Value.FieldCellId == NewCell.FieldCellId)
            return false;
    if (NewCell.PlantedCropId.IsValid())
    {
        const FWorldCropState* Crop = Crops.Find(NewCell.PlantedCropId);
        if (!Crop || Crop->PlotCell != NewCell.Cell ||
            Crop->Stage == ECropGrowthStage::Withered || Crop->Stage == ECropGrowthStage::Harvested)
            return false;
    }

    TSet<FGuid> SeenOperations;
    for (const FWorldAgricultureActionReceipt& Receipt : NewCell.ActionReceipts)
    {
        if (FindActionReceipt(Receipt.OperationId) ||
            !Receipt.OperationId.IsValid() || SeenOperations.Contains(Receipt.OperationId) ||
            Receipt.EntityId != NewCell.FieldCellId ||
            Receipt.ExpectedEntityRevision <= 0 || Receipt.ExpectedEntityRevision >= MAX_int64 - 1 ||
            Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
            Receipt.ResultingEntityRevision > NewCell.Revision ||
            Receipt.Action < EWorldAgricultureActionKind::ClearFieldCell ||
            Receipt.Action > EWorldAgricultureActionKind::WaterFieldCell ||
            Receipt.InputItemId.IsValid() || Receipt.ExpectedInventoryOwnerId.IsValid() ||
            Receipt.ExpectedItemRevision != 0 || Receipt.ResultingItemRevision != 0 ||
            !Receipt.OutputItemIds.IsEmpty() || !FMath::IsFinite(Receipt.WorkAmount) ||
            Receipt.WorkAmount <= 0.0f || Receipt.DefinitionId.IsNone())
            return false;
        SeenOperations.Add(Receipt.OperationId);
    }

    FieldCells.Add(NewCell.Cell, MoveTemp(NewCell));
    return true;
}

bool UWorldAgricultureSubsystem::GetFieldCell(
    const FIntVector& Cell, FWorldFieldCellState& OutFieldCell) const
{
    const FWorldFieldCellState* Found = FieldCells.Find(Cell);
    if (!Found)
        return false;
    OutFieldCell = *Found;
    return true;
}

bool UWorldAgricultureSubsystem::TryApplyFieldWork(
    const FGuid& OperationId, const FIntVector& Cell, int64 ExpectedFieldRevision,
    EWorldFieldWorkKind WorkKind, float WorkAmount, FWorldFieldCellState& OutFieldCell)
{
    OutFieldCell = FWorldFieldCellState();
    if (!OperationId.IsValid() || ExpectedFieldRevision <= 0 ||
        static_cast<uint8>(WorkKind) > static_cast<uint8>(EWorldFieldWorkKind::Water) ||
        !FMath::IsFinite(WorkAmount) || WorkAmount <= 0.0f ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256)
        return false;

    EWorldAgricultureActionKind Action = EWorldAgricultureActionKind::ClearFieldCell;
    FName WorkId = NAME_None;
    switch (WorkKind)
    {
    case EWorldFieldWorkKind::Clear:
        Action = EWorldAgricultureActionKind::ClearFieldCell;
        WorkId = TEXT("Clear");
        if (WorkAmount > 1.0f) return false;
        break;
    case EWorldFieldWorkKind::Till:
        Action = EWorldAgricultureActionKind::TillFieldCell;
        WorkId = TEXT("Till");
        if (WorkAmount > 1.0f) return false;
        break;
    case EWorldFieldWorkKind::Fertilize:
        Action = EWorldAgricultureActionKind::FertilizeFieldCell;
        WorkId = TEXT("Fertilize");
        if (WorkAmount > 25.0f) return false;
        break;
    case EWorldFieldWorkKind::Weed:
        Action = EWorldAgricultureActionKind::WeedFieldCell;
        WorkId = TEXT("Weed");
        if (WorkAmount > 1.0f) return false;
        break;
    case EWorldFieldWorkKind::Mulch:
        Action = EWorldAgricultureActionKind::MulchFieldCell;
        WorkId = TEXT("Mulch");
        if (WorkAmount > 1.0f) return false;
        break;
    case EWorldFieldWorkKind::Water:
        Action = EWorldAgricultureActionKind::WaterFieldCell;
        WorkId = TEXT("Water");
        if (WorkAmount > 1.0f) return false;
        break;
    default:
        return false;
    }

    if (const FWorldAgricultureActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != Action || Existing->ExpectedEntityRevision != ExpectedFieldRevision ||
            Existing->DefinitionId != WorkId || !FMath::IsNearlyEqual(Existing->WorkAmount, WorkAmount, 0.0001f))
            return false;
        const FWorldFieldCellState* ExistingField = FieldCells.Find(Cell);
        if (!ExistingField || Existing->EntityId != ExistingField->FieldCellId)
            return false;
        OutFieldCell = *ExistingField;
        return true;
    }

    FWorldFieldCellState* FieldCell = FieldCells.Find(Cell);
    if (!FieldCell || FieldCell->Revision != ExpectedFieldRevision ||
        FieldCell->Revision >= MAX_int64 - 1 ||
        FieldCell->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    switch (WorkKind)
    {
    case EWorldFieldWorkKind::Clear:
        if (FieldCell->PlantedCropId.IsValid())
            return false;
        FieldCell->bCleared = true;
        FieldCell->bTilled = false;
        FieldCell->WeedPressure = FMath::Clamp(FieldCell->WeedPressure - 0.9f * WorkAmount, 0.0f, 1.0f);
        break;
    case EWorldFieldWorkKind::Till:
        if (!FieldCell->bCleared || FieldCell->PlantedCropId.IsValid())
            return false;
        FieldCell->bTilled = true;
        break;
    case EWorldFieldWorkKind::Fertilize:
        FieldCell->Fertility = FMath::Clamp(FieldCell->Fertility + WorkAmount, 0.0f, 100.0f);
        break;
    case EWorldFieldWorkKind::Weed:
        FieldCell->WeedPressure = FMath::Clamp(FieldCell->WeedPressure - WorkAmount, 0.0f, 1.0f);
        break;
    case EWorldFieldWorkKind::Mulch:
        FieldCell->MulchCoverage = FMath::Clamp(FieldCell->MulchCoverage + WorkAmount, 0.0f, 1.0f);
        break;
    case EWorldFieldWorkKind::Water:
        FieldCell->Moisture = FMath::Clamp(FieldCell->Moisture + WorkAmount, 0.0f, 1.0f);
        break;
    default:
        return false;
    }

    FWorldAgricultureActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Action = Action;
    Receipt.EntityId = FieldCell->FieldCellId;
    Receipt.ExpectedEntityRevision = ExpectedFieldRevision;
    Receipt.ResultingEntityRevision = ExpectedFieldRevision + 1;
    Receipt.DefinitionId = WorkId;
    Receipt.WorkAmount = WorkAmount;
    FieldCell->ActionReceipts.Add(Receipt);
    ++FieldCell->Revision;
    OutFieldCell = *FieldCell;
    return true;
}

bool UWorldAgricultureSubsystem::RegisterCrop(const FWorldCropState& Crop)
{
    if (!Crop.CropId.IsValid() || Crop.CropDefinitionId.IsNone() ||
        static_cast<uint8>(Crop.Stage) > static_cast<uint8>(ECropGrowthStage::Harvested) ||
        !FMath::IsFinite(Crop.Moisture) || Crop.Moisture < 0.0f || Crop.Moisture > 1.0f ||
        !FMath::IsFinite(Crop.GrowthProgressSeconds) || Crop.GrowthProgressSeconds < 0.0 ||
        !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0 ||
        Crop.Revision <= 0 || Crop.SeedReturnUnits < 0 || Crop.SeedReturnUnits > 64 ||
        !FMath::IsFinite(Crop.SoilExhaustionPerHarvest) ||
        Crop.SoilExhaustionPerHarvest < 0.0f || Crop.SoilExhaustionPerHarvest > 100.0f ||
        !FMath::IsFinite(Crop.MinimumFertility) || Crop.MinimumFertility < 0.0f || Crop.MinimumFertility > 100.0f ||
        (Crop.SeedReturnUnits > 0 && Crop.SeedOutputDefinitionId.IsNone()) ||
        Crop.SeedOutputItemIds.Num() > 64 ||
        Crop.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
        MaxOperationReceiptsPerEntity <= 0 || Crops.Contains(Crop.CropId) ||
        MaxCrops <= 0 || Crops.Num() >= MaxCrops)
        return false;

    if (Crop.PlantOperationId.IsValid() &&
        (!Crop.SeedItemId.IsValid() || !Crop.SeedInventoryOwnerId.IsValid() ||
         Crop.SeedItemExpectedRevision <= 0))
        return false;
    if (Crop.HarvestOperationId.IsValid() &&
        (Crop.Stage != ECropGrowthStage::Harvested || Crop.HarvestExpectedRevision <= 0 ||
         Crop.HarvestDefinitionId.IsNone() || Crop.HarvestUnits <= 0 || Crop.HarvestUnits > 64 ||
         Crop.HarvestOutputItemIds.Num() != Crop.HarvestUnits ||
         Crop.SeedOutputItemIds.Num() != Crop.SeedReturnUnits))
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
        !FMath::IsFinite(Animal.ProductIntervalSeconds) || Animal.ProductIntervalSeconds < 3600.0 ||
        Animal.ProductProgressSeconds < 0.0 || Animal.ProductProgressSeconds >= Animal.ProductIntervalSeconds ||
        Animal.ProductUnitsPerInterval <= 0 || Animal.ProductUnitsPerInterval > 64 ||
        Animal.Location.ContainsNaN() || Animal.Revision <= 0 ||
        Animal.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256 ||
        Animals.Contains(Animal.AnimalId) || MaxAnimals <= 0 || Animals.Num() >= MaxAnimals)
        return false;

    FWorldAnimalState NewAnimal = Animal;
    if (const FWorldAnimalDefinition* Definition = FindAnimalDefinition(Animal.AnimalDefinitionId))
    {
        if ((!Animal.FeedDefinitionId.IsNone() && Animal.FeedDefinitionId != Definition->FeedDefinitionId) ||
            (!Animal.ProductDefinitionId.IsNone() && Animal.ProductDefinitionId != Definition->ProductDefinitionId))
            return false;
        NewAnimal.FeedDefinitionId = Definition->FeedDefinitionId;
        NewAnimal.ProductDefinitionId = Definition->ProductDefinitionId;
        NewAnimal.ProductIntervalSeconds = Definition->ProductIntervalSeconds;
        NewAnimal.ProductUnitsPerInterval = Definition->ProductUnitsPerInterval;
        if (NewAnimal.ProductProgressSeconds >= NewAnimal.ProductIntervalSeconds)
            return false;
    }

    if ((NewAnimal.bCaptured &&
         (!NewAnimal.CaptureOperationId.IsValid() || !NewAnimal.CaptureToolItemId.IsValid() ||
          !NewAnimal.CaptureInventoryOwnerId.IsValid() || NewAnimal.CaptureToolExpectedRevision <= 0)) ||
        (!NewAnimal.bCaptured &&
         (NewAnimal.CaptureOperationId.IsValid() || NewAnimal.CaptureToolItemId.IsValid() ||
          NewAnimal.CaptureInventoryOwnerId.IsValid() || NewAnimal.CaptureToolExpectedRevision != 0)) ||
        (NewAnimal.PenId.IsValid() && (!NewAnimal.bCaptured || NewAnimal.bSlaughtered)) ||
        (NewAnimal.bSlaughtered &&
         (!NewAnimal.bCaptured || NewAnimal.Health != 0.0f || NewAnimal.PenId.IsValid() ||
          !NewAnimal.SlaughterOperationId.IsValid() || NewAnimal.SlaughterOutputItemIds.IsEmpty())) ||
        (!NewAnimal.bSlaughtered &&
         (NewAnimal.SlaughterOperationId.IsValid() || !NewAnimal.SlaughterOutputItemIds.IsEmpty())))
        return false;

    TSet<FGuid> SeenOperations;
    for (const FWorldAgricultureActionReceipt& Receipt : NewAnimal.ActionReceipts)
    {
        if (!Receipt.OperationId.IsValid() || SeenOperations.Contains(Receipt.OperationId) ||
            FindActionReceipt(Receipt.OperationId) || Receipt.EntityId != NewAnimal.AnimalId ||
            Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
            Receipt.ResultingEntityRevision > NewAnimal.Revision ||
            (Receipt.Action != EWorldAgricultureActionKind::FeedAnimal &&
             Receipt.Action != EWorldAgricultureActionKind::CollectAnimalProducts &&
             Receipt.Action != EWorldAgricultureActionKind::CaptureAnimal &&
             Receipt.Action != EWorldAgricultureActionKind::AssignAnimalToPen &&
             Receipt.Action != EWorldAgricultureActionKind::SlaughterAnimal))
            return false;
        SeenOperations.Add(Receipt.OperationId);
    }

    if (NewAnimal.bCaptured &&
        !NewAnimal.ActionReceipts.ContainsByPredicate([&NewAnimal](const FWorldAgricultureActionReceipt& Receipt)
        {
            return Receipt.Action == EWorldAgricultureActionKind::CaptureAnimal &&
                Receipt.OperationId == NewAnimal.CaptureOperationId;
        }))
        return false;
    if (NewAnimal.bSlaughtered &&
        !NewAnimal.ActionReceipts.ContainsByPredicate([&NewAnimal](const FWorldAgricultureActionReceipt& Receipt)
        {
            return Receipt.Action == EWorldAgricultureActionKind::SlaughterAnimal &&
                Receipt.OperationId == NewAnimal.SlaughterOperationId;
        }))
        return false;

    Animals.Add(NewAnimal.AnimalId, MoveTemp(NewAnimal));
    return true;
}

bool UWorldAgricultureSubsystem::SetCropMoisture(
    const FGuid& CropId, float Moisture, int64 ExpectedRevision)
{
    FWorldCropState* Crop = Crops.Find(CropId);
    if (!Crop || Crop->Revision != ExpectedRevision || Crop->Revision >= MAX_int64 - 1 ||
        !FMath::IsFinite(Moisture) || Moisture < 0.0f || Moisture > 1.0f)
        return false;

    FWorldFieldCellState* FieldCell = FieldCells.Find(Crop->PlotCell);
    if (FieldCell && FieldCell->PlantedCropId == CropId && FieldCell->Revision >= MAX_int64 - 1)
        return false;
    Crop->Moisture = Moisture;
    ++Crop->Revision;
    if (FieldCell && FieldCell->PlantedCropId == CropId)
    {
        FieldCell->Moisture = Moisture;
        ++FieldCell->Revision;
    }
    return true;
}

bool UWorldAgricultureSubsystem::FeedAnimal(
    const FGuid& AnimalId, float HungerReduction, int64 ExpectedRevision)
{
    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    if (!Animal || Animal->Revision != ExpectedRevision || Animal->Revision >= MAX_int64 - 1 ||
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
        MinimumGrowthMoisture > 1.0f || !FMath::IsFinite(HungerPerHour) ||
        HungerPerHour < 0.0f || !FMath::IsFinite(ProductHungerLimit) ||
        ProductHungerLimit < 0.0f || ProductHungerLimit > 100.0f ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256)
        return false;

    for (const TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        const FWorldCropState& Crop = Pair.Value;
        if (Crop.Revision >= MAX_int64 - 1 ||
            static_cast<uint8>(Crop.Stage) > static_cast<uint8>(ECropGrowthStage::Harvested) ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds) || Crop.GrowthProgressSeconds < 0.0 ||
            !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0 ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds + DeltaSeconds))
            return false;

        const FWorldFieldCellState* FieldCell = FieldCells.Find(Crop.PlotCell);
        const float EffectiveMoisture =
            FieldCell && FieldCell->PlantedCropId == Crop.CropId ? FieldCell->Moisture : Crop.Moisture;
        if (Crop.Stage != ECropGrowthStage::Mature &&
            Crop.Stage != ECropGrowthStage::Withered &&
            Crop.Stage != ECropGrowthStage::Harvested &&
            EffectiveMoisture < MinimumGrowthMoisture &&
            FieldCell && FieldCell->PlantedCropId == Crop.CropId &&
            FieldCell->Revision >= MAX_int64 - 1)
            return false;
    }
    for (const TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
        if (Animal.Revision >= MAX_int64 - 1 ||
            !FMath::IsFinite(Animal.ProductProgressSeconds) ||
            !FMath::IsFinite(Animal.ProductIntervalSeconds) || Animal.ProductIntervalSeconds < 3600.0 ||
            Animal.ProductProgressSeconds < 0.0 || Animal.ProductProgressSeconds >= Animal.ProductIntervalSeconds ||
            Animal.ProductUnitsPerInterval <= 0 || Animal.ProductUnitsPerInterval > 64 ||
            !FMath::IsFinite(Animal.ProductProgressSeconds + DeltaSeconds) ||
            Animal.Location.ContainsNaN())
            return false;

        const FWorldAnimalPenState* ProductPen = Animal.PenId.IsValid()
            ? AnimalPens.Find(Animal.PenId) : nullptr;
        if (!Animal.bSlaughtered && Animal.bCaptured && Animal.Health > 0.0f &&
            ProductPen && ProductPen->bGateClosed && ProductPen->AnimalIds.Contains(Animal.AnimalId) &&
            Animal.Hunger <= ProductHungerLimit && !Animal.ProductDefinitionId.IsNone())
        {
            const double TotalProductTime = Animal.ProductProgressSeconds + DeltaSeconds;
            const int64 Intervals = static_cast<int64>(FMath::FloorToDouble(TotalProductTime / Animal.ProductIntervalSeconds));
            if (Intervals > 0 &&
                (Intervals > MAX_int32 / Animal.ProductUnitsPerInterval ||
                 Animal.ProductUnits > MAX_int32 - Intervals * Animal.ProductUnitsPerInterval))
                return false;
        }
    }
    for (const TPair<FGuid, FWorldAnimalPenState>& Pair : AnimalPens)
    {
        const FWorldAnimalPenState& Pen = Pair.Value;
        if (!Pen.PenId.IsValid() || Pen.Revision >= MAX_int64 - 1 ||
            Pen.Location.ContainsNaN() || Pen.Capacity < 1 || Pen.Capacity > 4 ||
            Pen.AnimalIds.Num() > Pen.Capacity || Pen.ActionReceipts.Num() > MaxOperationReceiptsPerEntity)
            return false;
    }
    return true;
}

bool UWorldAgricultureSubsystem::AdvanceAgricultureStep(double DeltaSeconds)
{
    if (!CanAdvanceAgricultureStep(DeltaSeconds)) return false;

    for (TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        FWorldCropState& Crop = Pair.Value;
        if (Crop.Stage == ECropGrowthStage::Mature || Crop.Stage == ECropGrowthStage::Withered ||
            Crop.Stage == ECropGrowthStage::Harvested)
            continue;

        FWorldFieldCellState* FieldCell = FieldCells.Find(Crop.PlotCell);
        if (FieldCell && FieldCell->PlantedCropId == Crop.CropId)
            Crop.Moisture = FieldCell->Moisture;

        if (Crop.Moisture < MinimumGrowthMoisture)
        {
            Crop.Stage = ECropGrowthStage::Withered;
            ++Crop.Revision;
            if (FieldCell && FieldCell->PlantedCropId == Crop.CropId)
            {
                FieldCell->PlantedCropId.Invalidate();
                ++FieldCell->Revision;
            }
            continue;
        }

        float GrowthMultiplier = FMath::Clamp(Crop.Moisture, 0.0f, 1.0f);
        if (FieldCell && FieldCell->PlantedCropId == Crop.CropId)
        {
            GrowthMultiplier *= FMath::Clamp(1.0f - FieldCell->WeedPressure * 0.25f, 0.5f, 1.0f);
            GrowthMultiplier *= 1.0f + FieldCell->MulchCoverage * 0.1f;
        }
        Crop.GrowthProgressSeconds += DeltaSeconds * GrowthMultiplier;
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
        if (Animal.bSlaughtered || !Animal.bCaptured)
            continue;

        Animal.Hunger = FMath::Clamp(Animal.Hunger + HungerPerHour *
            static_cast<float>(DeltaSeconds / 3600.0), 0.0f, 100.0f);
        if (Animal.Hunger >= 80.0f)
            Animal.Health = FMath::Max(0.0f, Animal.Health - static_cast<float>(DeltaSeconds / 3600.0) * 2.0f);

        if (Animal.Health > 0.0f && Animal.Hunger <= ProductHungerLimit &&
            !Animal.ProductDefinitionId.IsNone() && Animal.PenId.IsValid())
        {
            const FWorldAnimalPenState* Pen = AnimalPens.Find(Animal.PenId);
            if (Pen && Pen->bGateClosed && Pen->AnimalIds.Contains(Animal.AnimalId))
            {
                Animal.ProductProgressSeconds += DeltaSeconds;
                while (Animal.ProductProgressSeconds >= Animal.ProductIntervalSeconds &&
                    Animal.ProductUnits <= MAX_int32 - Animal.ProductUnitsPerInterval)
                {
                    Animal.ProductProgressSeconds -= Animal.ProductIntervalSeconds;
                    Animal.ProductUnits += Animal.ProductUnitsPerInterval;
                }
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
    TArray<FWorldFieldCellState> IgnoredFieldCells;
    TArray<FWorldAnimalPenState> IgnoredPens;
    ExportSnapshot(OutCrops, OutAnimals, IgnoredFieldCells, IgnoredPens);
}

void UWorldAgricultureSubsystem::ExportSnapshot(
    TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals,
    TArray<FWorldFieldCellState>& OutFieldCells) const
{
    TArray<FWorldAnimalPenState> IgnoredPens;
    ExportSnapshot(OutCrops, OutAnimals, OutFieldCells, IgnoredPens);
}

void UWorldAgricultureSubsystem::ExportSnapshot(
    TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals,
    TArray<FWorldFieldCellState>& OutFieldCells, TArray<FWorldAnimalPenState>& OutPens) const
{
    OutCrops.Reset();
    OutAnimals.Reset();
    OutFieldCells.Reset();
    OutPens.Reset();
    Crops.GenerateValueArray(OutCrops);
    Animals.GenerateValueArray(OutAnimals);
    FieldCells.GenerateValueArray(OutFieldCells);
    AnimalPens.GenerateValueArray(OutPens);
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
    OutPens.Sort([](const FWorldAnimalPenState& A, const FWorldAnimalPenState& B)
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
    const TArray<FWorldAnimalPenState>& PenSnapshot)
{
    if (CropSnapshot.Num() > MaxCrops || AnimalSnapshot.Num() > MaxAnimals ||
        FieldCellSnapshot.Num() > MaxFieldCells || PenSnapshot.Num() > MaxAnimalPens ||
        MaxCrops <= 0 || MaxAnimals <= 0 || MaxFieldCells <= 0 || MaxFieldCells > 100000 ||
        MaxAnimalPens <= 0 || MaxAnimalPens > 1024 ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256)
        return false;

    TMap<FGuid, FWorldCropState> NewCrops;
    TMap<FGuid, FWorldAnimalState> NewAnimals;
    TMap<FIntVector, FWorldFieldCellState> NewFieldCells;
    TMap<FGuid, FWorldAnimalPenState> NewPens;
    for (const FWorldCropState& C : CropSnapshot)
    {
        if (!C.CropId.IsValid() || C.CropDefinitionId.IsNone() || C.Revision <= 0 ||
            static_cast<uint8>(C.Stage) > static_cast<uint8>(ECropGrowthStage::Harvested) ||
            !FMath::IsFinite(C.Moisture) || C.Moisture < 0.0f || C.Moisture > 1.0f ||
            !FMath::IsFinite(C.GrowthProgressSeconds) || C.GrowthProgressSeconds < 0.0 ||
            !FMath::IsFinite(C.SecondsPerStage) || C.SecondsPerStage <= 0.0 ||
            C.SeedReturnUnits < 0 || C.SeedReturnUnits > 64 ||
            !FMath::IsFinite(C.SoilExhaustionPerHarvest) ||
            C.SoilExhaustionPerHarvest < 0.0f || C.SoilExhaustionPerHarvest > 100.0f ||
            !FMath::IsFinite(C.MinimumFertility) || C.MinimumFertility < 0.0f || C.MinimumFertility > 100.0f ||
            (C.SeedReturnUnits > 0 && C.SeedOutputDefinitionId.IsNone()) ||
            C.SeedOutputItemIds.Num() > 64 ||
            C.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
            (C.PlantOperationId.IsValid() &&
                (!C.SeedItemId.IsValid() || !C.SeedInventoryOwnerId.IsValid() || C.SeedItemExpectedRevision <= 0)) ||
            (C.HarvestOperationId.IsValid() &&
                (C.Stage != ECropGrowthStage::Harvested || C.HarvestExpectedRevision <= 0 ||
                 C.HarvestDefinitionId.IsNone() || C.HarvestUnits <= 0 || C.HarvestUnits > 64 ||
                 C.HarvestOutputItemIds.Num() != C.HarvestUnits ||
                 C.SeedOutputItemIds.Num() != C.SeedReturnUnits)) ||
            NewCrops.Contains(C.CropId))
            return false;

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

    for (const FWorldAnimalState& A : AnimalSnapshot)
    {
        if (!A.AnimalId.IsValid() || A.AnimalDefinitionId.IsNone() || A.Revision <= 0 ||
            !FMath::IsFinite(A.Hunger) || A.Hunger < 0.0f || A.Hunger > 100.0f ||
            !FMath::IsFinite(A.Health) || A.Health < 0.0f || A.Health > 100.0f ||
            A.ProductUnits < 0 || !FMath::IsFinite(A.ProductProgressSeconds) ||
            !FMath::IsFinite(A.ProductIntervalSeconds) || A.ProductIntervalSeconds < 3600.0 ||
            A.ProductProgressSeconds < 0.0 || A.ProductProgressSeconds >= A.ProductIntervalSeconds ||
            A.ProductUnitsPerInterval <= 0 || A.ProductUnitsPerInterval > 64 ||
            A.Location.ContainsNaN() || A.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
            (A.bCaptured &&
                (!A.CaptureOperationId.IsValid() || !A.CaptureToolItemId.IsValid() ||
                 !A.CaptureInventoryOwnerId.IsValid() || A.CaptureToolExpectedRevision <= 0)) ||
            (!A.bCaptured &&
                (A.CaptureOperationId.IsValid() || A.CaptureToolItemId.IsValid() ||
                 A.CaptureInventoryOwnerId.IsValid() || A.CaptureToolExpectedRevision != 0)) ||
            (A.PenId.IsValid() && (!A.bCaptured || A.bSlaughtered)) ||
            (A.bSlaughtered &&
                (!A.bCaptured || A.Health != 0.0f || A.PenId.IsValid() ||
                 !A.SlaughterOperationId.IsValid() || A.SlaughterOutputItemIds.IsEmpty())) ||
            (!A.bSlaughtered &&
                (A.SlaughterOperationId.IsValid() || !A.SlaughterOutputItemIds.IsEmpty())) ||
            NewAnimals.Contains(A.AnimalId))
            return false;
        const bool bHasCaptureReceipt = A.CaptureOperationId.IsValid() &&
            A.ActionReceipts.ContainsByPredicate([&A](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::CaptureAnimal &&
                    Receipt.OperationId == A.CaptureOperationId;
            });
        const bool bHasSlaughterReceipt = A.SlaughterOperationId.IsValid() &&
            A.ActionReceipts.ContainsByPredicate([&A](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::SlaughterAnimal &&
                    Receipt.OperationId == A.SlaughterOperationId;
            });
        if ((A.bCaptured && !bHasCaptureReceipt) || (A.bSlaughtered && !bHasSlaughterReceipt))
            return false;
        NewAnimals.Add(A.AnimalId, A);
    }

    TSet<FGuid> SeenPenIds;
    for (const FWorldAnimalPenState& P : PenSnapshot)
    {
        if (!P.PenId.IsValid() || SeenPenIds.Contains(P.PenId) ||
            !FMath::IsFinite(P.Location.X) || !FMath::IsFinite(P.Location.Y) || !FMath::IsFinite(P.Location.Z) ||
            P.Location.ContainsNaN() || P.Capacity < 1 || P.Capacity > 4 ||
            P.StakeUnits != 4 || P.RopeUnits != 1 ||
            P.AnimalIds.Num() > P.Capacity || P.Revision <= 0 ||
            P.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
            (!P.bGateClosed && !P.AnimalIds.IsEmpty()))
            return false;

        TSet<FGuid> SeenPenAnimals;
        for (const FGuid& AnimalId : P.AnimalIds)
        {
            const FWorldAnimalState* Animal = NewAnimals.Find(AnimalId);
            if (!Animal || SeenPenAnimals.Contains(AnimalId) || !Animal->bCaptured ||
                Animal->bSlaughtered || Animal->PenId != P.PenId)
                return false;
            SeenPenAnimals.Add(AnimalId);
        }

        TSet<FGuid> LocalOperations;
        for (const FWorldAgricultureActionReceipt& Receipt : P.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || LocalOperations.Contains(Receipt.OperationId) ||
                Receipt.EntityId != P.PenId ||
                Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                Receipt.ResultingEntityRevision > P.Revision ||
                !Receipt.OutputItemIds.IsEmpty() || Receipt.InputItemId.IsValid() ||
                Receipt.ExpectedItemRevision != 0 || Receipt.ResultingItemRevision != 0)
                return false;
            if (Receipt.Action == EWorldAgricultureActionKind::BuildAnimalPen)
            {
                if (Receipt.ExpectedEntityRevision != 0 || Receipt.ResultingEntityRevision != 1 ||
                    Receipt.DefinitionId != TEXT("AnimalPen") ||
                    !Receipt.ExpectedInventoryOwnerId.IsValid() ||
                    Receipt.WorkAmount != static_cast<float>(P.Capacity))
                    return false;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::SetAnimalPenGate)
            {
                if (Receipt.ExpectedEntityRevision <= 0 || Receipt.DefinitionId != TEXT("Gate") ||
                    (Receipt.WorkAmount != 0.0f && Receipt.WorkAmount != 1.0f) ||
                    Receipt.ExpectedInventoryOwnerId.IsValid())
                    return false;
            }
            else
                return false;
            LocalOperations.Add(Receipt.OperationId);
        }
        if (!P.ActionReceipts.ContainsByPredicate([&P](const FWorldAgricultureActionReceipt& Receipt)
            { return Receipt.Action == EWorldAgricultureActionKind::BuildAnimalPen; }))
            return false;
        SeenPenIds.Add(P.PenId);
        NewPens.Add(P.PenId, P);
    }

    TSet<FGuid> SeenFieldIdentityIds;
    for (const FWorldFieldCellState& F : FieldCellSnapshot)
    {
        if (!F.FieldCellId.IsValid() || SeenFieldIdentityIds.Contains(F.FieldCellId) ||
            NewFieldCells.Contains(F.Cell) ||
            !FMath::IsFinite(F.Fertility) || F.Fertility < 0.0f || F.Fertility > 100.0f ||
            !FMath::IsFinite(F.WeedPressure) || F.WeedPressure < 0.0f || F.WeedPressure > 1.0f ||
            !FMath::IsFinite(F.MulchCoverage) || F.MulchCoverage < 0.0f || F.MulchCoverage > 1.0f ||
            !FMath::IsFinite(F.Moisture) || F.Moisture < 0.0f || F.Moisture > 1.0f ||
            F.Revision <= 0 || F.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
            F.ConsecutiveSameCropHarvests < 0 ||
            (F.LastHarvestedCropDefinitionId.IsNone() && F.ConsecutiveSameCropHarvests != 0) ||
            (!F.LastHarvestedCropDefinitionId.IsNone() && F.ConsecutiveSameCropHarvests == 0))
            return false;

        TSet<FGuid> LocalOperations;
        for (const FWorldAgricultureActionReceipt& Receipt : F.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || LocalOperations.Contains(Receipt.OperationId) ||
                Receipt.EntityId != F.FieldCellId ||
                Receipt.Action < EWorldAgricultureActionKind::ClearFieldCell ||
                Receipt.Action > EWorldAgricultureActionKind::WaterFieldCell ||
                Receipt.ExpectedEntityRevision <= 0 || Receipt.ExpectedEntityRevision >= MAX_int64 - 1 ||
                Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                Receipt.ResultingEntityRevision > F.Revision ||
                !FMath::IsFinite(Receipt.WorkAmount) || Receipt.WorkAmount <= 0.0f ||
                Receipt.InputItemId.IsValid() || !Receipt.OutputItemIds.IsEmpty())
                return false;
            LocalOperations.Add(Receipt.OperationId);
        }
        SeenFieldIdentityIds.Add(F.FieldCellId);
        NewFieldCells.Add(F.Cell, F);
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
                TArray<FGuid> ExpectedOutputs = Crop.HarvestOutputItemIds;
                ExpectedOutputs.Append(Crop.SeedOutputItemIds);
                if (Receipt.OperationId != Crop.HarvestOperationId ||
                    Receipt.ExpectedEntityRevision != Crop.HarvestExpectedRevision ||
                    Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                    Receipt.DefinitionId != Crop.HarvestDefinitionId ||
                    Receipt.RequestedOutputUnits != Crop.HarvestUnits ||
                    Receipt.OutputItemIds != ExpectedOutputs ||
                    Receipt.OutputItemIds.Num() != Crop.HarvestUnits + Crop.SeedReturnUnits ||
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
                 Receipt.Action != EWorldAgricultureActionKind::AssignAnimalToPen &&
                 Receipt.Action != EWorldAgricultureActionKind::SlaughterAnimal) ||
                Receipt.ExpectedEntityRevision <= 0 ||
                Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                Receipt.ResultingEntityRevision > Animal.Revision)
                return false;

            if (Receipt.Action == EWorldAgricultureActionKind::FeedAnimal)
            {
                if (!Receipt.InputItemId.IsValid() || !Receipt.ExpectedInventoryOwnerId.IsValid() ||
                    Receipt.ExpectedItemRevision <= 0 ||
                    Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                    Receipt.DefinitionId.IsNone() || Receipt.DefinitionId != Animal.FeedDefinitionId ||
                    !FMath::IsFinite(Receipt.HungerReduction) ||
                    Receipt.HungerReduction <= 0.0f || Receipt.HungerReduction > 100.0f ||
                    !Receipt.OutputItemIds.IsEmpty() || Receipt.RelatedEntityId.IsValid())
                    return false;
                bNeedsItemValidation = true;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::CollectAnimalProducts)
            {
                if (Receipt.DefinitionId.IsNone() || Receipt.DefinitionId != Animal.ProductDefinitionId ||
                    Receipt.RequestedOutputUnits <= 0 || Receipt.RequestedOutputUnits > 64 ||
                    Receipt.OutputItemIds.Num() != Receipt.RequestedOutputUnits ||
                    Receipt.RelatedEntityId.IsValid())
                    return false;
                for (const FGuid& ItemId : Receipt.OutputItemIds)
                {
                    if (!ItemId.IsValid() || SeenOutputIds.Contains(ItemId))
                        return false;
                    SeenOutputIds.Add(ItemId);
                }
                bNeedsItemValidation = true;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::CaptureAnimal)
            {
                if (!Animal.bCaptured || Receipt.OperationId != Animal.CaptureOperationId ||
                    Receipt.InputItemId != Animal.CaptureToolItemId ||
                    Receipt.ExpectedInventoryOwnerId != Animal.CaptureInventoryOwnerId ||
                    Receipt.ExpectedItemRevision != Animal.CaptureToolExpectedRevision ||
                    Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision ||
                    Receipt.DefinitionId != Animal.AnimalDefinitionId ||
                    !Receipt.InputItemId.IsValid() || !Receipt.ExpectedInventoryOwnerId.IsValid() ||
                    !Receipt.OutputItemIds.IsEmpty() || Receipt.RelatedEntityId.IsValid() ||
                    Receipt.ExpectedRelatedEntityRevision != 0 ||
                    Receipt.ResultingRelatedEntityRevision != 0)
                    return false;
                bNeedsItemValidation = true;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::AssignAnimalToPen)
            {
                const FWorldAnimalPenState* RelatedPen = NewPens.Find(Receipt.RelatedEntityId);
                if (!Animal.bCaptured || !Receipt.RelatedEntityId.IsValid() || !RelatedPen ||
                    Receipt.DefinitionId != TEXT("AnimalPen") ||
                    Receipt.ExpectedRelatedEntityRevision <= 0 ||
                    Receipt.ResultingRelatedEntityRevision != Receipt.ExpectedRelatedEntityRevision + 1 ||
                    Receipt.ResultingRelatedEntityRevision > RelatedPen->Revision ||
                    Receipt.InputItemId.IsValid() || Receipt.ExpectedInventoryOwnerId.IsValid() ||
                    Receipt.ExpectedItemRevision != 0 || Receipt.ResultingItemRevision != 0 ||
                    !Receipt.OutputItemIds.IsEmpty() ||
                    (!Animal.bSlaughtered && Animal.PenId != Receipt.RelatedEntityId))
                    return false;
            }
            else
            {
                const FWorldAnimalPenState* RelatedPen = NewPens.Find(Receipt.RelatedEntityId);
                if (!Animal.bSlaughtered || Receipt.OperationId != Animal.SlaughterOperationId ||
                    !Receipt.RelatedEntityId.IsValid() || !RelatedPen ||
                    Receipt.DefinitionId != Animal.AnimalDefinitionId ||
                    Receipt.ExpectedRelatedEntityRevision <= 0 ||
                    Receipt.ResultingRelatedEntityRevision != Receipt.ExpectedRelatedEntityRevision + 1 ||
                    Receipt.ResultingRelatedEntityRevision > RelatedPen->Revision ||
                    Receipt.RequestedOutputUnits <= 0 || Receipt.RequestedOutputUnits > 64 ||
                    Receipt.OutputItemIds != Animal.SlaughterOutputItemIds ||
                    Receipt.OutputItemIds.Num() != Receipt.RequestedOutputUnits ||
                    Receipt.InputItemId.IsValid() || Receipt.ExpectedInventoryOwnerId.IsValid() ||
                    Receipt.ExpectedItemRevision != 0 || Receipt.ResultingItemRevision != 0)
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

    for (const TPair<FIntVector, FWorldFieldCellState>& Pair : NewFieldCells)
    {
        const FWorldFieldCellState& Field = Pair.Value;
        for (const FWorldAgricultureActionReceipt& Receipt : Field.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || SeenOperationIds.Contains(Receipt.OperationId) ||
                Receipt.EntityId != Field.FieldCellId ||
                Receipt.Action < EWorldAgricultureActionKind::ClearFieldCell ||
                Receipt.Action > EWorldAgricultureActionKind::WaterFieldCell ||
                Receipt.ExpectedEntityRevision <= 0 || Receipt.ExpectedEntityRevision >= MAX_int64 - 1 ||
                Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                Receipt.ResultingEntityRevision > Field.Revision ||
                !FMath::IsFinite(Receipt.WorkAmount) || Receipt.WorkAmount <= 0.0f ||
                Receipt.DefinitionId.IsNone() || Receipt.InputItemId.IsValid() ||
                Receipt.ExpectedInventoryOwnerId.IsValid() || Receipt.ExpectedItemRevision != 0 ||
                Receipt.ResultingItemRevision != 0 || Receipt.RelatedEntityId.IsValid() ||
                Receipt.ExpectedRelatedEntityRevision != 0 || Receipt.ResultingRelatedEntityRevision != 0 ||
                !Receipt.OutputItemIds.IsEmpty())
                return false;
            SeenOperationIds.Add(Receipt.OperationId);
        }
    }

    for (const TPair<FGuid, FWorldAnimalPenState>& Pair : NewPens)
    {
        const FWorldAnimalPenState& Pen = Pair.Value;
        int32 BuildReceiptCount = 0;
        for (const FWorldAgricultureActionReceipt& Receipt : Pen.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || SeenOperationIds.Contains(Receipt.OperationId) ||
                Receipt.EntityId != Pen.PenId ||
                Receipt.ResultingEntityRevision != Receipt.ExpectedEntityRevision + 1 ||
                Receipt.ResultingEntityRevision > Pen.Revision ||
                Receipt.InputItemId.IsValid() || Receipt.ExpectedItemRevision != 0 ||
                Receipt.ResultingItemRevision != 0 || !Receipt.OutputItemIds.IsEmpty() ||
                Receipt.RelatedEntityId.IsValid() ||
                Receipt.ExpectedRelatedEntityRevision != 0 || Receipt.ResultingRelatedEntityRevision != 0)
                return false;

            if (Receipt.Action == EWorldAgricultureActionKind::BuildAnimalPen)
            {
                ++BuildReceiptCount;
                if (Receipt.ExpectedEntityRevision != 0 || Receipt.ResultingEntityRevision != 1 ||
                    Receipt.DefinitionId != TEXT("AnimalPen") ||
                    !Receipt.ExpectedInventoryOwnerId.IsValid() ||
                    Receipt.WorkAmount != static_cast<float>(Pen.Capacity))
                    return false;
            }
            else if (Receipt.Action == EWorldAgricultureActionKind::SetAnimalPenGate)
            {
                if (Receipt.ExpectedEntityRevision <= 0 || Receipt.DefinitionId != TEXT("Gate") ||
                    (Receipt.WorkAmount != 0.0f && Receipt.WorkAmount != 1.0f) ||
                    Receipt.ExpectedInventoryOwnerId.IsValid())
                    return false;
            }
            else
                return false;
            SeenOperationIds.Add(Receipt.OperationId);
        }
        if (BuildReceiptCount != 1)
            return false;
    }

    for (const TPair<FGuid, FWorldCropState>& Pair : NewCrops)
    {
        const FWorldCropState& Crop = Pair.Value;
        const FWorldFieldCellState* Field = NewFieldCells.Find(Crop.PlotCell);
        if (Field && Crop.PlantOperationId.IsValid() &&
            Crop.Stage != ECropGrowthStage::Withered && Crop.Stage != ECropGrowthStage::Harvested &&
            Field->PlantedCropId != Crop.CropId)
            return false;
        if (Field && Crop.Stage == ECropGrowthStage::Harvested &&
            Field->PlantedCropId == Crop.CropId)
            return false;
    }
    for (const TPair<FIntVector, FWorldFieldCellState>& Pair : NewFieldCells)
    {
        const FWorldFieldCellState& Field = Pair.Value;
        if (Field.PlantedCropId.IsValid())
        {
            const FWorldCropState* Crop = NewCrops.Find(Field.PlantedCropId);
            if (!Crop || Crop->PlotCell != Field.Cell ||
                Crop->Stage == ECropGrowthStage::Withered || Crop->Stage == ECropGrowthStage::Harvested)
                return false;
        }
    }

    for (const TPair<FGuid, FWorldAnimalState>& Pair : NewAnimals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
        if (Animal.PenId.IsValid())
        {
            const FWorldAnimalPenState* Pen = NewPens.Find(Animal.PenId);
            if (!Pen || !Pen->AnimalIds.Contains(Animal.AnimalId) || Animal.bSlaughtered)
                return false;
        }
        else if (!Animal.bSlaughtered && Animal.ActionReceipts.ContainsByPredicate(
            [&Animal](const FWorldAgricultureActionReceipt& Receipt)
            {
                return Receipt.Action == EWorldAgricultureActionKind::AssignAnimalToPen;
            }))
            return false;

        if (Animal.bSlaughtered)
        {
            const FWorldAgricultureActionReceipt* SlaughterReceipt =
                Animal.ActionReceipts.FindByPredicate([&Animal](const FWorldAgricultureActionReceipt& Receipt)
                {
                    return Receipt.Action == EWorldAgricultureActionKind::SlaughterAnimal &&
                        Receipt.OperationId == Animal.SlaughterOperationId;
                });
            const FWorldAnimalPenState* FormerPen =
                SlaughterReceipt ? NewPens.Find(SlaughterReceipt->RelatedEntityId) : nullptr;
            if (!FormerPen || FormerPen->AnimalIds.Contains(Animal.AnimalId))
                return false;
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
            for (const FGuid& ItemId : Crop.SeedOutputItemIds)
                if (!Exists(ItemId)) return false;
        }
        for (const TPair<FGuid, FWorldAnimalState>& Pair : NewAnimals)
        {
            const FWorldAnimalState& Animal = Pair.Value;
            if (Animal.CaptureOperationId.IsValid() && !Exists(Animal.CaptureToolItemId))
                return false;
            for (const FGuid& ItemId : Animal.SlaughterOutputItemIds)
                if (!Exists(ItemId)) return false;
            for (const FWorldAgricultureActionReceipt& Receipt : Animal.ActionReceipts)
            {
                if ((Receipt.Action == EWorldAgricultureActionKind::FeedAnimal ||
                     Receipt.Action == EWorldAgricultureActionKind::CaptureAnimal) &&
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
    AnimalPens = MoveTemp(NewPens);
    return true;
}
