#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayCoreTypes.h"
#include "LogisticsProductionTypes.h"
#include "EnvironmentSurvivalTypes.h"
#include "EnvironmentSurvivalSubsystem.generated.h"

UCLASS()
class MYPROJECT3_API UEnvironmentSurvivalSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Environment")
    FWorldOperationResult InitializeEnvironment(int64 WorldSeed, double InitialWorldTimeSeconds, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category="Environment")
    FWorldOperationResult AdvanceEnvironment(double DeltaSeconds, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category="Environment")
    FWorldOperationResult SimulateSurvivalStep(FSurvivalState& InOutState, const FSurvivalProtectionProfile& Protection, float ActivityLoad, bool bResting, double DeltaSeconds, FGuid OperationId);

    UFUNCTION(BlueprintPure, Category="Environment")
    FEnvironmentWeatherState GetEnvironmentState() const { return EnvironmentState; }

    UFUNCTION(BlueprintPure, Category="Environment")
    FSurvivalTuning GetSurvivalTuning() const { return Tuning; }

    UFUNCTION(BlueprintCallable, Category="Environment")
    void SetSurvivalTuning(const FSurvivalTuning& NewTuning);

private:
    UPROPERTY(Transient)
    FEnvironmentWeatherState EnvironmentState;

    UPROPERTY(EditAnywhere, Category="Environment")
    FSurvivalTuning Tuning;

    UPROPERTY(Transient)
    bool bInitialized = false;

    UPROPERTY(Transient)
    TSet<FGuid> ProcessedOperationIds;

    static constexpr double SecondsPerGameHour = 60.0;
    static constexpr double SecondsPerSeason = 90.0 * 24.0 * SecondsPerGameHour;
    static constexpr double SecondsPerWeatherPeriod = 6.0 * SecondsPerGameHour;

    FWorldOperationResult Failure(EWorldOperationResultCode Code, const FGuid& OperationId, const TCHAR* Message) const;
    bool ValidateAndRemember(const FGuid& OperationId, FWorldOperationResult& OutFailure);
    void UpdateSeason();
    void ChooseWeatherForCurrentPeriod();
    void ApplyWeatherProfile();
};
