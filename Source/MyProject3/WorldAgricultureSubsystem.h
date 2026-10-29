#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldAgricultureSubsystem.generated.h"

UENUM(BlueprintType)
enum class ECropGrowthStage : uint8 { Seed, Sprout, Growing, Mature, Withered };

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
};

/** Authored, asset-independent crop output contract. Physical plant/soil visuals are separate adapters. */
USTRUCT(BlueprintType)
struct FWorldCropYieldRule
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Yield") FName CropDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Yield") FName HarvestItemDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Yield", meta=(ClampMin="1", ClampMax="100000")) int32 YieldUnits = 1;
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
    /** Soil-moisture rates are fractions of the [0,1] moisture scale per in-game hour. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="1")) float BaseSoilDryingPerHour = 0.01f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="1")) float RainMoistureGainPerHour = 0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="1")) float StormMoistureGainPerHour = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="1")) float WindMoistureLossPerHour = 0.04f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="1")) float HeatwaveMoistureLossPerHour = 0.15f;
    /** Full-severity weather multipliers; actual value interpolates from 1.0 by severity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="2")) float RainGrowthMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="2")) float WindGrowthMultiplier = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="2")) float SnowGrowthMultiplier = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="2")) float StormGrowthMultiplier = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Climate", meta=(ClampMin="0", ClampMax="2")) float HeatwaveGrowthMultiplier = 0.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float HungerPerHour = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float ProductHungerLimit = 55.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Yield") TArray<FWorldCropYieldRule> CropYieldRules;

    UFUNCTION(BlueprintCallable, Category="World|Farm") bool RegisterCrop(const FWorldCropState& Crop);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool RegisterAnimal(const FWorldAnimalState& Animal);
    UFUNCTION(BlueprintPure, Category="World|Farm") bool IsCropDefinitionConfigured(FName CropDefinitionId) const;
    UFUNCTION(BlueprintPure, Category="World|Farm") bool IsPlotCellOccupied(FIntVector PlotCell) const;
    UFUNCTION(BlueprintPure, Category="World|Farm") bool GetCropYield(FName CropDefinitionId, FName& OutItemDefinitionId, int32& OutYieldUnits) const;
    UFUNCTION(BlueprintCallable, Category="World|Farm") bool TryHarvestCrop(
        const FGuid& CropId, int64 ExpectedRevision, FName& OutItemDefinitionId, int32& OutYieldUnits);
    UFUNCTION(BlueprintCallable, Category="World|Farm") bool SetCropMoisture(const FGuid& CropId, float Moisture, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool FeedAnimal(const FGuid& AnimalId, float HungerReduction, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool TryCollectAnimalProduct(
        const FGuid& AnimalId, int64 ExpectedRevision, FName& OutProductDefinitionId, int32& OutProductUnits);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool TryCollectAnimalProduct(
        const FGuid& AnimalId, int64 ExpectedRevision, int32 Quantity,
        FName& OutProductDefinitionId, int32& OutCollectedQuantity);
    UFUNCTION(BlueprintPure, Category="World|Simulation") bool CanAdvanceAgricultureStep(double DeltaSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Simulation") bool AdvanceAgricultureStep(double DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="World|Farm") bool GetCrop(const FGuid& CropId, FWorldCropState& OutCrop) const;
    UFUNCTION(BlueprintPure, Category="World|Animals") bool GetAnimal(const FGuid& AnimalId, FWorldAnimalState& OutAnimal) const;
    void ExportSnapshot(TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals) const;
    bool RestoreSnapshot(const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot);
private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Farm") TMap<FGuid, FWorldCropState> Crops;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Animals") TMap<FGuid, FWorldAnimalState> Animals;
};
