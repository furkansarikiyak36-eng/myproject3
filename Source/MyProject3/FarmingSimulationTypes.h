#pragma once

#include "CoreMinimal.h"
#include "FarmingSimulationTypes.generated.h"

UENUM(BlueprintType)
enum class ECropGrowthStage : uint8
{
    Seed UMETA(DisplayName = "Seed"),
    Sprout UMETA(DisplayName = "Sprout"),
    Growing UMETA(DisplayName = "Growing"),
    Mature UMETA(DisplayName = "Mature"),
    Harvested UMETA(DisplayName = "Harvested")
};

USTRUCT(BlueprintType)
struct FCropDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming") FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming", meta = (ClampMin = "1")) int32 TicksToMature = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming", meta = (ClampMin = "0.0")) float MinSoilMoisture = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming", meta = (ClampMin = "0.0")) float MaxSoilMoisture = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming", meta = (ClampMin = "0.0")) float MinNutrients = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming") FName RequiredSeason = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming", meta = (ClampMin = "1")) int32 BaseYield = 3;
};

USTRUCT(BlueprintType)
struct FCropRuntimeState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") FGuid CropId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") ECropGrowthStage Stage = ECropGrowthStage::Seed;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") int32 GrowthTicks = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") float SoilMoisture = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") float Nutrients = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") float Stress = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct FAnimalDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals") FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals") FName FeedResourceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals", meta = (ClampMin = "0.0")) float FeedPerTick = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals", meta = (ClampMin = "1")) int32 TicksToMature = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals") FName ProductResourceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals", meta = (ClampMin = "0.0")) float ProductPerTick = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals", meta = (ClampMin = "0.0")) float ManureNutrientsPerTick = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming|Animals", meta = (ClampMin = "1")) int32 ReproductionTicks = 30;
};

USTRUCT(BlueprintType)
struct FAnimalRuntimeState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") FGuid AnimalId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") float Health = 100.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") float FeedReserve = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") int32 GrowthTicks = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") int32 ProductUnitsReady = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") int32 ReproductionProgress = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") int64 Revision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming|Animals") bool bAlive = true;
};

USTRUCT(BlueprintType)
struct FFarmingSimulationSnapshot
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") int64 SimulationTick = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") double WorldTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") FName CurrentSeason = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") float AccumulatedManureNutrients = 0.0f;
};