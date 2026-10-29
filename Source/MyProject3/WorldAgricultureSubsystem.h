#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldAgricultureSubsystem.generated.h"

UENUM(BlueprintType)
enum class ECropGrowthStage : uint8 { Seed, Sprout, Growing, Mature, Withered, Harvested };

UENUM(BlueprintType)
enum class EWorldAgricultureActionKind : uint8 { PlantCrop, HarvestCrop, FeedAnimal, CollectAnimalProducts };

/** Immutable retry result for an item-backed farming operation. */
USTRUCT(BlueprintType)
struct FWorldAgricultureActionReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") EWorldAgricultureActionKind Action = EWorldAgricultureActionKind::PlantCrop;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") FGuid EntityId;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="1")) double SecondsPerStage = 3600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions", meta=(ClampMin="0", ClampMax="1")) float MinimumPlantMoisture = 0.15f;
}

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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FName HarvestDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int32 HarvestUnits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") FGuid HarvestOperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") int64 HarvestExpectedRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Transactions") TArray<FGuid> HarvestOutputItemIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm|Receipts") TArray<FWorldAgricultureActionReceipt> ActionReceipts;
};

USTRUCT(BlueprintType)
struct FWorldAnimalState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid AnimalId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName AnimalDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid OwnerSettlementId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Hunger = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName ProductDefinitionId = NAME_None;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxAnimals = 512;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm") float MinimumGrowthMoisture = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float HungerPerHour = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float ProductHungerLimit = 55.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="4096")) int32 MaxCropDefinitions = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="256")) int32 MaxOperationReceiptsPerEntity = 64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Definitions") TArray<FWorldCropDefinition> CropDefinitions;

    UFUNCTION(BlueprintCallable, Category="World|Farm|Definitions") bool RegisterCropDefinition(const FWorldCropDefinition& Definition);
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryPlantCrop(
        const FGuid& OperationId, FName CropDefinitionId, const FGuid& SeedItemId,
        int64 ExpectedSeedItemRevision, const FGuid& ExpectedInventoryOwnerId,
        const FIntVector& PlotCell, float InitialMoisture, FGuid& OutCropId);
    UFUNCTION(BlueprintCallable, Category="World|Farm|Transactions") bool TryHarvestCrop(
        const FGuid& OperationId, const FGuid& CropId, int64 ExpectedCropRevision,
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
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool FeedAnimal(const FGuid& AnimalId, float HungerReduction, int64 ExpectedRevision);
    UFUNCTION(BlueprintPure, Category="World|Simulation") bool CanAdvanceAgricultureStep(double DeltaSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Simulation") bool AdvanceAgricultureStep(double DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="World|Farm") bool GetCrop(const FGuid& CropId, FWorldCropState& OutCrop) const;
    UFUNCTION(BlueprintPure, Category="World|Animals") bool GetAnimal(const FGuid& AnimalId, FWorldAnimalState& OutAnimal) const;
    void ExportSnapshot(TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals) const;
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot);
private:
    const FWorldCropDefinition* FindCropDefinition(FName CropDefinitionId) const;
    const FWorldAgricultureActionReceipt* FindActionReceipt(const FGuid& OperationId) const;
    bool FindInventoryForOwner(const FGuid& InventoryOwnerId, class UInventoryCraftingComponent*& OutInventory) const;
    bool ConsumeInventoryItemAtomically(const FGuid& OperationId, const FGuid& ItemId,
        int64 ExpectedItemRevision, const FGuid& InventoryOwnerId,
        class UInventoryCraftingComponent* Inventory);
    bool RegisterWorldOutputsAtomically(const TArray<FName>& Definitions,
        const TArray<FVector>& Locations, TArray<FGuid>& OutItemIds);

    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Farm") TMap<FGuid, FWorldCropState> Crops;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Animals") TMap<FGuid, FWorldAnimalState> Animals;
};
