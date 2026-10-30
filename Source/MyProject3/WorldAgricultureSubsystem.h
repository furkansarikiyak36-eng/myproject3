#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldAgricultureSubsystem.generated.h"

class UInventoryCraftingComponent;
class AProductionStationActor;

UENUM(BlueprintType)
enum class ECropGrowthStage : uint8 { Seed, Sprout, Growing, Mature, Withered, Harvested };

UENUM(BlueprintType)
enum class EWorldAgricultureActionKind : uint8
{
    PlantCrop,
    HarvestCrop,
    FeedAnimal,
    CollectAnimalProducts,
    ClearFieldCell,
    TillFieldCell,
    FertilizeFieldCell,
    WeedFieldCell,
    MulchFieldCell,
    WaterFieldCell,
    CaptureAnimal,
    BuildAnimalPen,
    AssignAnimalToPen,
    SetAnimalPenGate,
    SlaughterAnimal,
    FallowFieldCell,
    FenceFieldCell
};

UENUM(BlueprintType)
enum class EWorldFieldWorkKind : uint8 { Clear, Till, Fertilize, Weed, Mulch, Water, Fallow, Fence };

/** Immutable retry result for an item-backed farming operation or a field-state operation. */
USTRUCT(BlueprintType)
struct FWorldAgricultureActionReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") EWorldAgricultureActionKind Action = EWorldAgricultureActionKind::PlantCrop;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid EntityId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid RelatedEntityId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid InputItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid ExpectedInventoryOwnerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ExpectedEntityRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ExpectedItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ExpectedRelatedEntityRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ResultingRelatedEntityRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ResultingEntityRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") int64 ResultingItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FName RequestedInputDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") float HungerReduction = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") float WorkAmount = 0.0f;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="1")) double SecondsPerStage = 3600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="1")) float MinimumPlantMoisture = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") TArray<int32> PlantMonths;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="64")) int32 SeedReturnUnits = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="100")) float SoilExhaustionPerHarvest = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="100")) float MinimumFertility = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="1")) float MaximumPlantWeedPressure = 0.75f;
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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FGuid SeedInventoryOwnerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int64 SeedItemExpectedRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FName SeedOutputDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FName HarvestDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int32 HarvestUnits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int32 SeedReturnUnits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") float SoilExhaustionPerHarvest = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") float MinimumFertility = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FGuid HarvestOperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int64 HarvestExpectedRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") TArray<FGuid> HarvestOutputItemIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") TArray<FGuid> SeedOutputItemIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") TArray<FWorldAgricultureActionReceipt> ActionReceipts;
};

/** A logical 1x1 m farm cell. Tools/visuals are adapters; the field state is authoritative. */
USTRUCT(BlueprintType)
struct FWorldFieldCellState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field") FGuid FieldCellId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field") FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field") bool bCleared = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field") bool bTilled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field") bool bFenced = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field", meta=(ClampMin="0", ClampMax="100")) float Fertility = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field", meta=(ClampMin="0", ClampMax="1")) float WeedPressure = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field", meta=(ClampMin="0", ClampMax="1")) float MulchCoverage = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm|Field", meta=(ClampMin="0", ClampMax="1")) float Moisture = 0.5f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Field") FGuid PlantedCropId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Field") FName LastHarvestedCropDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Field") int32 ConsecutiveSameCropHarvests = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Field") int64 Revision = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Field|Receipts") TArray<FWorldAgricultureActionReceipt> ActionReceipts;
};

USTRUCT(BlueprintType)
struct FWorldAnimalYieldDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName ItemDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions", meta=(ClampMin="1", ClampMax="64")) int32 Units = 1;
};

USTRUCT(BlueprintType)
struct FWorldAnimalDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName AnimalDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName FeedDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") FName ProductDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions", meta=(ClampMin="1", ClampMax="64")) int32 ProductUnitsPerInterval = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions", meta=(ClampMin="1")) double ProductIntervalSeconds = 3600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") TArray<FName> CaptureToolDefinitionIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") TArray<FWorldAnimalYieldDefinition> SlaughterYields;
};

/** Logical livestock pen; geometry/material adapters are deliberately separate. */
USTRUCT(BlueprintType)
struct FWorldAnimalPenState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pen") FGuid PenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pen") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pen", meta=(ClampMin="1", ClampMax="4")) int32 Capacity = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pen") int32 StakeUnits = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pen") int32 RopeUnits = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals|Pen") bool bGateClosed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Pen") TArray<FGuid> AnimalIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Pen") int64 Revision = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Pen|Receipts") TArray<FWorldAgricultureActionReceipt> ActionReceipts;
};

USTRUCT(BlueprintType)
struct FWorldAnimalState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid AnimalId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName AnimalDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid OwnerSettlementId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") bool bCaptured = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") bool bSlaughtered = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") FGuid PenId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Hunger = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName FeedDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName ProductDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0")) int32 ProductUnits = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0")) double ProductProgressSeconds = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="1")) double ProductIntervalSeconds = 3600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="1", ClampMax="64")) int32 ProductUnitsPerInterval = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") int64 Revision = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FVector Location = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Transactions") FGuid CaptureOperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Transactions") FGuid CaptureToolItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Transactions") FName CaptureToolDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Transactions") FGuid CaptureInventoryOwnerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Transactions") int64 CaptureToolExpectedRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Transactions") FGuid SlaughterOperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Transactions") TArray<FGuid> SlaughterOutputItemIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals|Receipts") TArray<FWorldAgricultureActionReceipt> ActionReceipts;
};

/** Bounded logical crop/animal lifecycle; visuals and physical actors are adapters. */
UCLASS()
class MYPROJECT3_API UWorldAgricultureSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxCrops = 2048;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxFieldCells = 10000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxAnimals = 512;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm") float MinimumGrowthMoisture = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float HungerPerHour = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float ProductHungerLimit = 55.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="4096")) int32 MaxCropDefinitions = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="256")) int32 MaxOperationReceiptsPerEntity = 64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") TArray<FWorldCropDefinition> CropDefinitions;

    UFUNCTION(BlueprintCallable, Category="World|Farm|Definitions") bool RegisterCropDefinition(const FWorldCropDefinition& Definition);
    /** Compatibility entry point; uses the current field revision for the plot. */
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryPlantCrop(
        const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
        int64 ExpectedSeedItemRevision, const FGuid& ExpectedInventoryOwnerId,
        const FIntVector& PlotCell, float InitialMoisture, FGuid& OutCropId);
    /** Revision-guarded player-facing planting contract. */
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryPlantCropOnField(
        const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
        int64 ExpectedSeedItemRevision, const FGuid& ExpectedInventoryOwnerId,
        const FIntVector& PlotCell, int64 ExpectedFieldRevision, float InitialMoisture, FGuid& OutCropId);
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryHarvestCrop(
        const FGuid& OperationId, const FGuid& CropId, int64 ExpectedCropRevision,
        TArray<FGuid>& OutItemIds);
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryHarvestCropWithFieldAction(
        const FGuid& OperationId, const FGuid& CropId, int64 ExpectedCropRevision,
        const FGuid& InventoryOwnerId, TArray<FGuid>& OutItemIds);
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryPlantCropWithFieldAction(
        const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
        int64 ExpectedSeedItemRevision, const FGuid& InventoryOwnerId,
        const FIntVector& PlotCell, int64 ExpectedFieldRevision, float InitialMoisture,
        FGuid& OutCropId);
    /** Moves one harvested unique item into a registered storage container using the Field.Store tool gate. */
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryStoreHarvestItemWithFieldAction(
        const FGuid& OperationId, const FGuid& ItemId, int64 ExpectedItemRevision,
        const FGuid& InventoryOwnerId, const FGuid& StorageId);
    /** Runs a configured hand-processing recipe or starts it at its authored station. */
    UFUNCTION(BlueprintCallable, Category="World|Farm|Processing") bool TryExecuteFieldProcessingAction(
        const FGuid& OperationId, FName FieldActionId, FName TransformationId,
        const FGuid& InventoryOwnerId, AProductionStationActor* Station,
        FGuid& OutJobId);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryFeedAnimal(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        const FGuid& FeedItemId, int64 ExpectedFeedItemRevision,
        const FGuid& ExpectedInventoryOwnerId, float HungerReduction);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryCollectAnimalProducts(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        TArray<FGuid>& OutItemIds);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Definitions") bool RegisterAnimalDefinition(const FWorldAnimalDefinition& Definition);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryCaptureAnimal(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        const FGuid& CaptureToolItemId, int64 ExpectedToolItemRevision,
        const FGuid& ExpectedInventoryOwnerId);
    /** Capture with a count-based inventory tool stack such as a crafted net or basket. */
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TryCaptureAnimalWithToolStack(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        FName CaptureToolDefinitionId, const FGuid& ExpectedInventoryOwnerId);
    /** Builds a logical 4-stake/1-rope pen and consumes those materials from the named inventory. */
    UFUNCTION(BlueprintCallable, Category="World|Animals|Pen") bool TryBuildAnimalPen(
        const FGuid& OperationId, const FGuid& ExpectedInventoryOwnerId,
        const FVector& Location, int32 Capacity, FGuid& OutPenId);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Pen") bool TryAssignAnimalToPen(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        const FGuid& PenId, int64 ExpectedPenRevision);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Pen") bool TrySetAnimalPenGate(
        const FGuid& OperationId, const FGuid& PenId, int64 ExpectedPenRevision, bool bClosed);
    UFUNCTION(BlueprintCallable, Category="World|Animals|Transactions") bool TrySlaughterAnimal(
        const FGuid& OperationId, const FGuid& AnimalId, int64 ExpectedAnimalRevision,
        TArray<FGuid>& OutItemIds);
    UFUNCTION(BlueprintPure, Category="World|Animals|Pen") bool GetAnimalPen(
        const FGuid& PenId, FWorldAnimalPenState& OutPen) const;

    UFUNCTION(BlueprintCallable, Category="World|Farm|Field") bool RegisterFieldCell(const FWorldFieldCellState& FieldCell);
    UFUNCTION(BlueprintPure, Category="World|Farm|Field") bool GetFieldCell(const FIntVector& Cell, FWorldFieldCellState& OutFieldCell) const;
    UFUNCTION(BlueprintCallable, Category="World|Farm|Field") bool TryApplyFieldWork(
        const FGuid& OperationId, const FIntVector& Cell, int64 ExpectedFieldRevision,
        EWorldFieldWorkKind WorkKind, float WorkAmount, FWorldFieldCellState& OutFieldCell);
    /** Data-driven, inventory-gated wrapper for the six field-state actions plus fallow. */
    UFUNCTION(BlueprintCallable, Category="World|Farm|Field") bool TryApplyFieldAction(
        const FGuid& OperationId, FName FieldActionId, const FIntVector& Cell,
        int64 ExpectedFieldRevision, const FGuid& InventoryOwnerId,
        FWorldFieldCellState& OutFieldCell);

    UFUNCTION(BlueprintCallable, Category="World|Farm") bool RegisterCrop(const FWorldCropState& Crop);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool RegisterAnimal(const FWorldAnimalState& Animal);
    UFUNCTION(BlueprintCallable, Category="World|Farm") bool SetCropMoisture(const FGuid& CropId, float Moisture, int64 ExpectedRevision);
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
        TArray<FWorldFieldCellState>& OutFieldCells, TArray<FWorldAnimalPenState>& OutPens) const;
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot);
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot,
        const TArray<FWorldFieldCellState>& FieldCellSnapshot);
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot,
        const TArray<FWorldFieldCellState>& FieldCellSnapshot, const TArray<FWorldAnimalPenState>& PenSnapshot);
private:
    const FWorldCropDefinition* FindCropDefinition(FName CropDefinitionId) const;
    const FWorldAnimalDefinition* FindAnimalDefinition(FName AnimalDefinitionId) const;
    const FWorldAgricultureActionReceipt* FindActionReceipt(const FGuid& OperationId) const;
    bool FindInventoryForOwner(const FGuid& InventoryOwnerId, UInventoryCraftingComponent*& OutInventory) const;
    bool ConsumeInventoryItemAtomically(const FGuid& OperationId, const FGuid& ItemId,
        int64 ExpectedItemRevision, const FGuid& InventoryOwnerId,
        UInventoryCraftingComponent* Inventory);
    bool RegisterWorldOutputsAtomically(const TArray<FName>& Definitions,
        const TArray<FVector>& Locations, TArray<FGuid>& OutItemIds);

    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Farm") TMap<FGuid, FWorldCropState> Crops;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Farm|Field") TMap<FIntVector, FWorldFieldCellState> FieldCells;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Animals|Pen") TMap<FGuid, FWorldAnimalPenState> AnimalPens;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Limits", meta=(ClampMin="1", ClampMax="1024")) int32 MaxAnimalPens = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Definitions") TArray<FWorldAnimalDefinition> AnimalDefinitions;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Animals") TMap<FGuid, FWorldAnimalState> Animals;
};
