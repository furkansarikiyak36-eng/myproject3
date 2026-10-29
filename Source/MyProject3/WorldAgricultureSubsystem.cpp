#include "WorldAgricultureSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "InventoryCraftingComponent.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"

const FWorldCropDefinition* UWorldAgricultureSubsystem::FindCropDefinition(FName CropDefinitionId) const
{
    if (CropDefinitionId.IsNone())
        return nullptr;
    return CropDefinitions.FindByPredicate([CropDefinitionId](const FWorldCropDefinition& Definition)
    {
        return Definition.CropDefinitionId == CropDefinitionId;
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
        MaxCropDefinitions <= 0 || MaxCropDefinitions > 4096 ||
        CropDefinitions.Num() >= MaxCropDefinitions || FindCropDefinition(Definition.CropDefinitionId))
        return false;
    CropDefinitions.Add(Definition);
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
    if (!Definition || InitialMoisture < Definition->MinimumPlantMoisture ||
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

    FWorldCropState Crop;
    Crop.CropId = FGuid::NewGuid();
    Crop.CropDefinitionId = CropDefinitionId;
    Crop.PlotCell = PlotCell;
    Crop.Stage = ECropGrowthStage::Seed;
    Crop.Moisture = InitialMoisture;
    Crop.SecondsPerStage = Definition->SecondsPerStage;
    Crop.Revision = 1;
    Crop.PlantOperationId = OperationId;
    Crop.SeedItemId = SeedItemId;
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
    if (!Crop || Crop->Revision != ExpectedCropRevision ||
        Crop->Revision >= MAX_int64 - 1 || Crop->Stage != ECropGrowthStage::Mature ||
        Crop->HarvestDefinitionId.IsNone() || Crop->HarvestUnits <= 0 ||
        Crop->HarvestUnits > 64 || Crop->ActionReceipts.Num() >= MaxOperationReceiptsPerEntity)
        return false;

    TArray<FName> Definitions;
    TArray<FVector> Locations;
    Definitions.Reserve(Crop->HarvestUnits);
    Locations.Reserve(Crop->HarvestUnits);
    const FVector PlotOrigin(
        static_cast<double>(Crop->PlotCell.X) * 100.0,
        static_cast<double>(Crop->PlotCell.Y) * 100.0,
        static_cast<double>(Crop->PlotCell.Z) * 100.0);
    for (int32 Index = 0; Index < Crop->HarvestUnits; ++Index)
    {
        Definitions.Add(Crop->HarvestDefinitionId);
        Locations.Add(PlotOrigin + FVector(static_cast<double>(Index) * 25.0, 0.0, 10.0));
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
    Receipt.RequestedOutputUnits = Crop->HarvestUnits;
    Receipt.OutputItemIds = CreatedItems;

    Crop->Stage = ECropGrowthStage::Harvested;
    Crop->HarvestOperationId = OperationId;
    Crop->HarvestExpectedRevision = ExpectedCropRevision;
    Crop->HarvestOutputItemIds = CreatedItems;
    ++Crop->Revision;
    Crop->ActionReceipts.Add(Receipt);
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
         Crop.HarvestOutputItemIds.Num() != Crop.HarvestUnits))
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
        ProductHungerLimit < 0.0f || ProductHungerLimit > 100.0f)
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
    }
    for (const TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
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

    for (TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        FWorldCropState& Crop = Pair.Value;
        if (Crop.Stage == ECropGrowthStage::Mature || Crop.Stage == ECropGrowthStage::Withered ||
            Crop.Stage == ECropGrowthStage::Harvested)
            continue;
        if (Crop.Moisture < MinimumGrowthMoisture)
        {
            Crop.Stage = ECropGrowthStage::Withered;
            ++Crop.Revision;
            continue;
        }
        Crop.GrowthProgressSeconds += DeltaSeconds * FMath::Clamp(Crop.Moisture, 0.0f, 1.0f);
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
        Animal.Hunger = FMath::Clamp(Animal.Hunger + HungerPerHour *
            static_cast<float>(DeltaSeconds / 3600.0), 0.0f, 100.0f);
        if (Animal.Hunger >= 80.0f)
            Animal.Health = FMath::Max(0.0f, Animal.Health - static_cast<float>(DeltaSeconds / 3600.0) * 2.0f);
        if (Animal.Health > 0.0f && Animal.Hunger <= ProductHungerLimit &&
            !Animal.ProductDefinitionId.IsNone())
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
    Crops.GenerateValueArray(OutCrops);
    Animals.GenerateValueArray(OutAnimals);
    OutCrops.Sort([](const FWorldCropState& A, const FWorldCropState& B)
        { return A.CropId.ToString() < B.CropId.ToString(); });
    OutAnimals.Sort([](const FWorldAnimalState& A, const FWorldAnimalState& B)
        { return A.AnimalId.ToString() < B.AnimalId.ToString(); });
}

bool UWorldAgricultureSubsystem::RestoreSnapshot(
    const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot)
{
    if (CropSnapshot.Num() > MaxCrops || AnimalSnapshot.Num() > MaxAnimals ||
        MaxCrops <= 0 || MaxAnimals <= 0 ||
        MaxOperationReceiptsPerEntity <= 0 || MaxOperationReceiptsPerEntity > 256) return false;
    TMap<FGuid, FWorldCropState> NewCrops;
    TMap<FGuid, FWorldAnimalState> NewAnimals;
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
            (C.HarvestOperationId.IsValid() &&
                (C.Stage != ECropGrowthStage::Harvested || C.HarvestExpectedRevision <= 0 ||
                 C.HarvestDefinitionId.IsNone() || C.HarvestUnits <= 0 || C.HarvestUnits > 64 ||
                 C.HarvestOutputItemIds.Num() != C.HarvestUnits)) ||
            NewCrops.Contains(C.CropId)) return false;
        NewCrops.Add(C.CropId, C);
    }
    for (const FWorldAnimalState& A : AnimalSnapshot)
    {
        if (!A.AnimalId.IsValid() || A.AnimalDefinitionId.IsNone() || A.Revision <= 0 ||
            !FMath::IsFinite(A.Hunger) || A.Hunger < 0.0f || A.Hunger > 100.0f ||
            !FMath::IsFinite(A.Health) || A.Health < 0.0f || A.Health > 100.0f ||
            A.ProductUnits < 0 || !FMath::IsFinite(A.ProductProgressSeconds) ||
            A.ProductProgressSeconds < 0.0 || A.ProductProgressSeconds >= 3600.0 ||
            A.Location.ContainsNaN() || A.ActionReceipts.Num() > MaxOperationReceiptsPerEntity ||
            NewAnimals.Contains(A.AnimalId)) return false;
        NewAnimals.Add(A.AnimalId, A);
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
                    Receipt.RequestedOutputUnits != Crop.HarvestUnits ||
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
                 Receipt.Action != EWorldAgricultureActionKind::CollectAnimalProducts) ||
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
                    !Receipt.OutputItemIds.IsEmpty())
                    return false;
            }
            else
            {
                if (Receipt.DefinitionId.IsNone() || Receipt.DefinitionId != Animal.ProductDefinitionId ||
                    Receipt.RequestedOutputUnits <= 0 || Receipt.RequestedOutputUnits > 64 ||
                    Receipt.OutputItemIds.Num() != Receipt.RequestedOutputUnits)
                    return false;
                for (const FGuid& ItemId : Receipt.OutputItemIds)
                {
                    if (!ItemId.IsValid() || SeenOutputIds.Contains(ItemId))
                        return false;
                    SeenOutputIds.Add(ItemId);
                }
            }
            bNeedsItemValidation = true;
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
    return true;
}
