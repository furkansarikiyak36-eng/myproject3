#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldAgricultureSubsystem.generated.h"

class UInventoryCraftingComponent;
class AWildlifeActor;

UENUM(BlueprintType)
enum class ECropGrowthStage : uint8 { Seed, Sprout, Growing, Mature, Withered, Harvested };

UENUM(BlueprintType)
enum class EWorldAgricultureActionKind : uint8
{
    PlantCrop, HarvestCrop, FeedAnimal, CollectAnimalProducts,
    CaptureAnimal, MoveAnimalToPen, SlaughterAnimal
};

UENUM(BlueprintType)
enum class EWorldAnimalLifecycleState : uint8 { Managed, Wild, Captured, Penned, Slaughtered };

UENUM(BlueprintType)
enum class EWorldFieldActionKind : uint8 { Clear, Till, Fertilize, Water, Weed, Mulch, Fence };

USTRUCT(BlueprintType)
struct FWorldFieldActionReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") EWorldFieldActionKind Action = EWorldFieldActionKind::Clear;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ExpectedRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ResultingRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") float Amount = 0.0f;
};

/** Persistent state of one logical farm cell. Empty cells can be prepared before planting. */
USTRUCT(BlueprintType)
struct FWorldFieldActionCost
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions") FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions", meta=(ClampMin="1", ClampMax="64")) int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FWorldFieldActionDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions") FName Id = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions") EWorldFieldActionKind Action = EWorldFieldActionKind::Clear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions") TArray<FWorldFieldActionCost> Inputs;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions") TArray<FName> RequiredTools;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions", meta=(ClampMin="0", ClampMax="1")) float Amount = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Actions", meta=(ClampMin="1", ClampMax="3600")) double LaborSeconds = 300.0;
};

USTRUCT(BlueprintType)
struct FWorldFieldCellState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") bool bCleared = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") bool bTilled = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") bool bFenced = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float Fertility = 0.5f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float WeedPressure = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float Mulch = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float Moisture = 0.5f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") int32 HarvestCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") int64 Revision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") TArray<FWorldFieldActionReceipt> ActionReceipts;
};

/** Immutable retry result for an item-backed farming operation. */
USTRUCT(BlueprintType)
struct FWorldAgricultureActionReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") EWorldAgricultureActionKind Action = EWorldAgricultureActionKind::PlantCrop;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid EntityId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid SourceWildlifeIdentityId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid PenId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ExpectedPenRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ResultingPenRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid InputItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid ExpectedInventoryOwnerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ExpectedEntityRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ExpectedItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ResultingEntityRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ResultingItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") float HungerReduction = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int32 RequestedOutputUnits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") TArray<FGuid> OutputItemIds;
};

USTRUCT(BlueprintType)
struct FWorldCropDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") FName CropDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") FName SeedDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") FName HarvestDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="1", ClampMax="64")) int32 HarvestUnits = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="64")) int32 SeedReturnUnits = 1;
    /** Soil fertility lost per harvest; legume crops can use a lower value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="1")) float SoilExhaustionPerHarvest = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="1")) double SecondsPerStage = 3600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="1", ClampMax="3600")) double PlantLaborSeconds = 600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="1", ClampMax="3600")) double HarvestLaborSeconds = 450.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="1")) float MinimumPlantMoisture = 0.15f;
    /** Empty means year-round; otherwise planting is allowed only in these calendar months (1..12). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") TArray<int32> PlantMonths;
    /** Discovery zone in which this crop can be found; discovery placement is handled by P7.3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="4")) int32 FoundInZone = 0;
};

USTRUCT(BlueprintType)
struct FWorldCropState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") FGuid CropId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") FName CropDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") FIntVector PlotCell = FIntVector::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") ECropGrowthStage Stage = ECropGrowthStage::Seed;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float Moisture = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm", meta=(ClampMin="0")) double GrowthProgressSeconds = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm", meta=(ClampMin="1")) double SecondsPerStage = 3600.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") int64 Revision = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FGuid PlantOperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FGuid SeedItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FName SeedDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FGuid SeedInventoryOwnerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int64 SeedItemExpectedRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FName HarvestDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int32 HarvestUnits = 0;
    /** Fixed at planting time so harvesting remains deterministic after save or data tuning. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int32 SeedReturnUnits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FGuid HarvestOperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int64 HarvestExpectedRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") TArray<FGuid> HarvestOutputItemIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") TArray<FWorldAgricultureActionReceipt> ActionReceipts;
};

/** Data copied from an authored animal type when a wildlife actor is captured. */
USTRUCT(BlueprintType)
struct FWorldAnimalCaptureDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName AnimalDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName FeedDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName ProductDefinitionId = NAME_None;
    /** Units delivered by each successful product collection cycle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions", meta=(ClampMin="0", ClampMax="64")) int32 ProductUnitsPerCollection = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName MeatDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions", meta=(ClampMin="0", ClampMax="64")) int32 MeatUnits = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName HideDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions", meta=(ClampMin="0", ClampMax="64")) int32 HideUnits = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName BoneDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions", meta=(ClampMin="0", ClampMax="64")) int32 BoneUnits = 1;
};

/** Capacity-limited logical pen. Visual fence/buildings remain world actors. */
USTRUCT(BlueprintType)
struct FWorldAnimalPenState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pens") FGuid PenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pens") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pens", meta=(ClampMin="1", ClampMax="128")) int32 Capacity = 4;
    /** Empty allows all animal definitions; otherwise the pen admits only listed definitions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pens") TArray<FName> AllowedAnimalDefinitionIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Pens") TArray<FGuid> AnimalIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Pens") int64 Revision = 1;
};

USTRUCT(BlueprintType)
struct FWorldAnimalState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid AnimalId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName AnimalDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid OwnerSettlementId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") EWorldAnimalLifecycleState LifecycleState = EWorldAnimalLifecycleState::Managed;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") FGuid SourceWildlifeIdentityId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") FGuid PenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Hunger = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName FeedDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName ProductDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName MeatDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="64")) int32 MeatUnits = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName HideDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="64")) int32 HideUnits = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName BoneDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="64")) int32 BoneUnits = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0")) int32 ProductUnits = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0")) double ProductProgressSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") int64 Revision = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FVector Location = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Receipts") TArray<FWorldAgricultureActionReceipt> ActionReceipts;
};

/** Bounded logical crop/animal lifecycle; visuals and physical actors are adapters. */
UCLASS()
class MYPROJECT3_API UWorldAgricultureSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxCrops = 2048;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="16")) int32 MaxFieldActionDefinitions = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxAnimals = 512;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Limits", meta=(ClampMin="1", ClampMax="4096")) int32 MaxAnimalPens = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm") float MinimumGrowthMoisture = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float HungerPerHour = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float ProductHungerLimit = 55.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="4096")) int32 MaxCropDefinitions = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="256")) int32 MaxOperationReceiptsPerEntity = 64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") TArray<FWorldCropDefinition> CropDefinitions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="65536")) int32 MaxFieldCells = 4096;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="512")) int32 MaxFieldActionReceiptsPerCell = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float SoilExhaustionPerHarvest = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float FieldMoistureLossPerHour = 0.015f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float WeedGrowthPerHour = 0.005f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float MulchMoistureRetention = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm") bool bRequirePreparedFieldForPlanting = true;

    UFUNCTION(BlueprintCallable, Category="World|Farm|Field") bool TryApplyFieldAction(
        const FGuid& OperationId, const FIntVector& Cell, EWorldFieldActionKind Action,
        int64 ExpectedRevision, float Amount = 0.25f);
    bool RegisterFieldActionDefinition(const FWorldFieldActionDefinition& Definition);
    /** Consumes configured item costs, applies the field state change and charges labor as one rollback-safe action. */
    bool TryApplyFieldActionWithInventory(const FGuid& OperationId,
        UInventoryCraftingComponent* Inventory, const FGuid& SurvivalEntityId,
        const FIntVector& Cell, EWorldFieldActionKind Action, int64 ExpectedRevision,
        FWorldFieldCellState& OutCell);
    UFUNCTION(BlueprintPure, Category="World|Farm|Field") bool GetFieldCell(
        const FIntVector& Cell, FWorldFieldCellState& OutCell) const;

    UFUNCTION(BlueprintCallable, Category="World|Farm|Definitions") bool RegisterCropDefinition(const FWorldCropDefinition& Definition);
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryPlantCrop(
        const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
        int64 ExpectedSeedItemRevision, const FGuid& ExpectedInventoryOwnerId,
        const FIntVector& PlotCell, float InitialMoisture, FGuid& OutCropId);
    /** Player-facing planting: requires a unique owned seed and charges authored work labor atomically. */
    bool TryPlantCropWithLabor(const FGuid& OperationId, FName CropDefinitionId,
        const FGuid& SeedItemId, int64 ExpectedSeedItemRevision,
        UInventoryCraftingComponent* Inventory, const FGuid& SurvivalEntityId,
        const FIntVector& PlotCell, float InitialMoisture, FGuid& OutCropId);
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryHarvestCrop(
        const FGuid& OperationId, const FGuid& CropId, int64 ExpectedCropRevision,
        TArray<FGuid>& OutItemIds);
    /** Harvests a mature crop directly into a player's inventory as owned items; farm, ledger, inventory and work costs roll back together. */
    bool TryHarvestCropToInventory(const FGuid& OperationId, const FGuid& CropId,
        int64 ExpectedCropRevision, UInventoryCraftingComponent* Inventory,
        const FGuid& SurvivalEntityId, TArray<FGuid>& OutItemIds);
    /** Finds the unique active crop on a soil cell; false for an empty or ambiguous cell. */
    bool GetCropAtPlotCell(const FIntVector& PlotCell, FWorldCropState& OutCrop) const;
    UFUNCTION(BlueprintCallable, Category="World|Animals|Pens") bool RegisterAnimalPen(const FWorldAnimalPenState& Pen);
    UFUNCTION(BlueprintPure, Category="World|Animals|Pens") bool GetAnimalPen(const FGuid& PenId, FWorldAnimalPenState& OutPen) const;
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryCaptureWildlife(
        const FGuid& OperationId, const FGuid& SourceWildlifeIdentityId, AWildlifeActor* Wildlife,
        const FWorldAnimalCaptureDefinition& Definition, FGuid& OutAnimalId);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryMoveAnimalToPen(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        const FGuid& PenId, int64 ExpectedPenRevision);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TrySlaughterAnimal(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        TArray<FGuid>& OutItemIds);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryFeedAnimal(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        const FGuid& FeedItemId, int64 ExpectedFeedItemRevision,
        const FGuid& ExpectedInventoryOwnerId, float HungerReduction);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryCollectAnimalProducts(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        TArray<FGuid>& OutItemIds);

    UFUNCTION(BlueprintCallable, Category="World|Farm") bool RegisterCrop(const FWorldCropState& Crop);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool RegisterAnimal(const FWorldAnimalState& Animal);
    UFUNCTION(BlueprintCallable, Category="World|Farm") bool SetCropMoisture(const FGuid& CropId, float Moisture, int64 ExpectedRevision);
    /** Applies a deterministic climate/event effect to prepared fields and growing crops. Called inside the shared fixed-step transaction. */
    bool ApplyEnvironmentalEffect(FName EffectId, float Severity, double DeltaSeconds);
    /** Legacy numeric simulation hook; player-facing feeding must use TryFeedAnimal so food ownership is consumed. */
    bool FeedAnimal(const FGuid& AnimalId, float HungerReduction, int64 ExpectedRevision);
    UFUNCTION(BlueprintPure, Category="World|Simulation") bool CanAdvanceAgricultureStep(double DeltaSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Simulation") bool AdvanceAgricultureStep(double DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="World|Farm") bool GetCrop(const FGuid& CropId, FWorldCropState& OutCrop) const;
    UFUNCTION(BlueprintPure, Category="World|Animals") bool GetAnimal(const FGuid& AnimalId, FWorldAnimalState& OutAnimal) const;
    void ExportSnapshot(TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals) const;
    void ExportSnapshot(TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals,
        TArray<FWorldFieldCellState>& OutFieldCells) const;
    void ExportSnapshot(TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals,
        TArray<FWorldFieldCellState>& OutFieldCells, TArray<FWorldAnimalPenState>& OutAnimalPens) const;
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot);
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot,
        const TArray<FWorldFieldCellState>& FieldCellSnapshot);
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot,
        const TArray<FWorldFieldCellState>& FieldCellSnapshot, const TArray<FWorldAnimalPenState>& AnimalPenSnapshot);
private:
    const FWorldCropDefinition* FindCropDefinition(FName CropDefinitionId) const;
    const FWorldFieldActionReceipt* FindFieldActionReceipt(const FGuid& OperationId) const;
    const FWorldAgricultureActionReceipt* FindActionReceipt(const FGuid& OperationId) const;
    AWildlifeActor* FindWildlifeActor(const FGuid& IdentityId) const;
    bool FindInventoryForOwner(const FGuid& InventoryOwnerId, UInventoryCraftingComponent*& OutInventory) const;
    bool ConsumeInventoryItemAtomically(const FGuid& OperationId, const FGuid& ItemId,
        int64 ExpectedItemRevision, const FGuid& InventoryOwnerId,
        UInventoryCraftingComponent* Inventory);
    bool RegisterWorldOutputsAtomically(const TArray<FName>& Definitions,
        const TArray<FVector>& Locations, TArray<FGuid>& OutItemIds);

    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Farm") TMap<FGuid, FWorldCropState> Crops;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Animals") TMap<FGuid, FWorldAnimalState> Animals;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Animals|Pens") TMap<FGuid, FWorldAnimalPenState> AnimalPens;
    UPROPERTY(VisibleAnywhere, Category="World|Farm|Actions") TArray<FWorldFieldActionDefinition> FieldActionDefinitions;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Farm") TMap<FIntVector, FWorldFieldCellState> FieldCells;
};
