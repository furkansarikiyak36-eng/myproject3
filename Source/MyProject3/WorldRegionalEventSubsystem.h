#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldClimateSubsystem.h"
#include "WorldRegionalEventSubsystem.generated.h"

/** Authored regional hazards are evaluated from world time, seed, region and event IDs. */
USTRUCT(BlueprintType)
struct FWorldRegionProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") FName Id = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float ElevationMeters = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float SlopeRatio = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float FloodExposure = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float VegetationFuelLoad = 0.0f;
    /** Structural-load units per snow centimetre and square metre of exposed roof. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float SnowLoadUnitsPerCmM2 = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") FVector RaidTargetLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FWorldRegionalEventDefinition
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") FName Id = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") FName Type = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") TArray<FName> RegionIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") TArray<int32> Months;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") TArray<EWorldWeatherState> RequiredWeatherStates;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") FName ChainParentEventId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MinTemperatureCelsius = -100.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MaxTemperatureCelsius = 100.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MinSnowDepthCm = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MinGroundWetness = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MaxGroundWetness = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MinSlopeRatio = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MinElevationMeters = -10000.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MinFloodExposure = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float MinVegetationFuelLoad = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float ChancePerDay = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float DurationHours = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float StructureDamagePerHour = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float RouteCostMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float RaidDefenseMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float SnowLoadMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float SurvivalSeverityMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FWorldRegionalEventEvaluation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") FName RegionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") TArray<FName> ActiveEventIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float RouteCostMultiplier = 1.0f;
    /** Multiplies authored structural defenses; lower values mean worse visibility/coordination. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float RaidDefenseMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float SnowLoadMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float StructureDamagePerHour = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Events") float SurvivalSeverityMultiplier = 1.0f;
};

/**
 * Data-driven, deterministic evaluator for regional natural hazards.
 * The active event set is derived from the shared clock/seed instead of a second
 * ticking timer, so saving/reloading cannot reroll an event or duplicate a chain.
 * This class evaluates conditions and modifiers; domain owners still commit their own mutations.
 */
UCLASS()
class MYPROJECT3_API UWorldRegionalEventSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category="World|Events") bool LoadDefaultDefinitions();
    UFUNCTION(BlueprintCallable, Category="World|Events") bool LoadDefinitionsFromDirectory(const FString& Directory);
    UFUNCTION(BlueprintPure, Category="World|Events") bool AreDefinitionsLoaded() const { return bDefinitionsLoaded; }
    UFUNCTION(BlueprintPure, Category="World|Events") FString GetLastError() const { return LastError; }

    UFUNCTION(BlueprintPure, Category="World|Events") bool GetRegionProfile(FName RegionId, FWorldRegionProfile& OutProfile) const;
    UFUNCTION(BlueprintPure, Category="World|Events") bool GetEventDefinition(
        FName EventId, FWorldRegionalEventDefinition& OutDefinition) const;
    UFUNCTION(BlueprintPure, Category="World|Events") TArray<FName> GetRegionIds() const;
    UFUNCTION(BlueprintPure, Category="World|Events") float GetSnowLoadUnitsPerCmM2(FName RegionId) const;

    /** Evaluates region conditions using current shared time, climate and deterministic daily rolls. */
    UFUNCTION(BlueprintCallable, Category="World|Events") bool EvaluateRegion(FName RegionId, FWorldRegionalEventEvaluation& OutEvaluation) const;

private:
    bool bDefinitionsLoaded = false;
    FString LastError;
    TMap<FName, FWorldRegionProfile> Regions;
    TArray<FWorldRegionalEventDefinition> EventDefinitions;
    TMap<FName, int32> EventIndexById;

    bool IsDefinitionActive(const FWorldRegionalEventDefinition& Definition,
        const FWorldRegionProfile& Region, int32 Month, float HourOfDay,
        int64 DayIndex, int32 RandomSeed, EWorldWeatherState Weather,
        float TemperatureCelsius, float SnowDepthCm, float GroundWetness,
        TMap<FName, bool>& Memo, TSet<FName>& Visiting) const;
};
