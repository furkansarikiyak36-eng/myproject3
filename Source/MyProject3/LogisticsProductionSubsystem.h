#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayCoreTypes.h"
#include "LogisticsProductionTypes.h"
#include "LogisticsProductionSubsystem.generated.h"
UCLASS() class MYPROJECT3_API ULogisticsProductionSubsystem:public UWorldSubsystem{
GENERATED_BODY()
public:
UFUNCTION(BlueprintCallable,Category="Logistics") FWorldOperationResult RegisterContainer(FTransportContainerState& InOutContainer,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Logistics") FWorldOperationResult TransferResource(FGuid SourceContainer,FGuid TargetContainer,FName ResourceId,double Quantity,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Production") FWorldOperationResult RegisterRecipe(const FProductionRecipeDefinition& Recipe,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Production") FWorldOperationResult QueueProduction(FName RecipeId,FGuid& OutJobId,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Production") FWorldOperationResult AdvanceProduction(FGuid JobId,float DeltaSeconds,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Production") FWorldOperationResult CancelProduction(FGuid JobId,FGuid OperationId);
UFUNCTION(BlueprintPure,Category="Logistics") float CalculateRouteCost(float Distance,float TerrainPenalty,bool bRoadOrBridge)const;
UFUNCTION(BlueprintCallable,Category="Survival") FWorldOperationResult UpdateSurvival(FSurvivalState& InOutState,float DeltaSeconds,float AmbientTemperature,float RainIntensity,bool bSheltered,bool bResting,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Survival") FWorldOperationResult ApplyExposure(FSurvivalState& InOutState,ESurvivalExposureType Exposure,float Severity,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Food") FWorldOperationResult RegisterFood(FFoodQualityRecord& InOutFood,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Food") FWorldOperationResult AdvanceFoodQuality(FGuid FoodId,double WorldHour,bool bPreserved,FGuid OperationId);
UFUNCTION(BlueprintCallable,Category="Exploration") FWorldOperationResult RecordDiscovery(FWorldDiscoveryRecord& InOutDiscovery,double WorldHour,FGuid OperationId);
UFUNCTION(BlueprintPure,Category="Production") TArray<FProductionJob> GetProductionJobs()const;
UFUNCTION(BlueprintPure,Category="Exploration") TArray<FWorldDiscoveryRecord> GetDiscoveries()const;
private:
UPROPERTY(Transient) TMap<FGuid,FTransportContainerState> Containers;
UPROPERTY(Transient) TMap<FName,FProductionRecipeDefinition> Recipes;
UPROPERTY(Transient) TMap<FGuid,FProductionJob> Jobs;
UPROPERTY(Transient) TMap<FGuid,FFoodQualityRecord> Foods;
UPROPERTY(Transient) TMap<FGuid,FWorldDiscoveryRecord> Discoveries;
UPROPERTY(Transient) TSet<FGuid> ProcessedOperationIds;
FWorldOperationResult Failure(EWorldOperationResultCode Code,const FGuid& OperationId,const TCHAR* Message)const;
FWorldOperationResult ValidateOperation(const FGuid& OperationId)const;
void RememberOperation(const FGuid& OperationId);
};