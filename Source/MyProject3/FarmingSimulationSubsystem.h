#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayCoreTypes.h"
#include "FarmingSimulationTypes.h"
#include "FarmingSimulationSubsystem.generated.h"

/**
 * Deterministic fixed-tick simulation ledger for crop and animal state.
 * It owns simulation data, not inventory, terrain geometry, or actor visuals.
 */
UCLASS()
class MYPROJECT3_API UFarmingSimulationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Farming|Definitions")
    FWorldOperationResult RegisterCropDefinition(const FCropDefinition& Definition, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Definitions")
    FWorldOperationResult RegisterAnimalDefinition(const FAnimalDefinition& Definition, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Clock")
    FWorldOperationResult SetSeason(FName SeasonId, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Clock")
    FWorldOperationResult AdvanceSimulationTicks(int32 TickCount, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Crops")
    FWorldOperationResult TryPlantCrop(FName DefinitionId, float SoilMoisture, float Nutrients,
        FGuid& OutCropId, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Crops")
    FWorldOperationResult TryWaterCrop(FGuid CropId, float WaterAmount, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Crops")
    FWorldOperationResult TryAddCropNutrients(FGuid CropId, float NutrientAmount, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Crops")
    FWorldOperationResult TryHarvestCrop(FGuid CropId, int32& OutYield, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Animals")
    FWorldOperationResult TryRegisterAnimal(FName DefinitionId, int32 HabitatCapacity,
        FGuid& OutAnimalId, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Animals")
    FWorldOperationResult TryFeedAnimal(FGuid AnimalId, float FeedAmount, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Farming|Animals")
    FWorldOperationResult TryCollectAnimalProducts(FGuid AnimalId, int32& OutUnits, FGuid OperationId);

    UFUNCTION(BlueprintPure, Category = "Farming|State")
    bool GetCropState(FGuid CropId, FCropRuntimeState& OutState) const;

    UFUNCTION(BlueprintPure, Category = "Farming|State")
    bool GetAnimalState(FGuid AnimalId, FAnimalRuntimeState& OutState) const;

    UFUNCTION(BlueprintPure, Category = "Farming|State")
    FFarmingSimulationSnapshot GetSnapshot() const;

private:
    UPROPERTY(Transient) TMap<FName, FCropDefinition> CropDefinitions;
    UPROPERTY(Transient) TMap<FName, FAnimalDefinition> AnimalDefinitions;
    UPROPERTY(Transient) TMap<FGuid, FCropRuntimeState> Crops;
    UPROPERTY(Transient) TMap<FGuid, FAnimalRuntimeState> Animals;
    UPROPERTY(Transient) FName CurrentSeason = NAME_None;
    UPROPERTY(Transient) int64 SimulationTick = 0;
    UPROPERTY(Transient) double WorldTimeSeconds = 0.0;
    UPROPERTY(Transient) float AccumulatedManureNutrients = 0.0f;
    UPROPERTY(Transient) float FixedTickSeconds = 60.0f;

    FWorldOperationResult Failure(EWorldOperationResultCode Code, const FGuid& OperationId,
        const TCHAR* Message) const;
    void AdvanceOneTick();
};