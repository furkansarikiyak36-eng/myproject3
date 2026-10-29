#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldClimateSubsystem.generated.h"

/** The climate of one month. Defaults: a cold mountain valley (game plan, section 7). */
USTRUCT(BlueprintType)
struct FWorldMonthClimate
{
    GENERATED_BODY()

    /** Coldest air temperature of a normal day (around 05:00). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float MinTemperatureCelsius = 0.0f;
    /** Warmest air temperature of a normal day (around 15:00). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float MaxTemperatureCelsius = 10.0f;
    /** Chance that a day brings rain or snow (snow when the day stays below freezing). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate", meta=(ClampMin="0", ClampMax="1")) float PrecipitationChance = 0.3f;
    /** Chance that a wet day is a storm instead. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate", meta=(ClampMin="0", ClampMax="1")) float StormChance = 0.1f;
    /** Chance that a dry day is windy. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate", meta=(ClampMin="0", ClampMax="1")) float WindChance = 0.2f;
    /** Chance that a dry day is a heatwave (summer only by default). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate", meta=(ClampMin="0", ClampMax="1")) float HeatwaveChance = 0.0f;
};

/** Saved climate state. */
USTRUCT(BlueprintType)
struct FWorldClimateSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") int32 SchemaVersion = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") bool bInitialized = false;
    /** Days-elapsed index whose weather is current; -1 before the first roll. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") int64 WeatherDayIndex = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") EWorldWeatherState Weather = EWorldWeatherState::Clear;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") float Severity = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") float TemperatureCelsius = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") float SnowDepthCm = 0.0f;
    /** 0 = dry ground, 1 = soaked ground / mud. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") float GroundWetness = 0.0f;
    /** Running total of rain (severity x hours); cisterns collect from its growth (P5.2). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Climate") double RainfallIndex = 0.0;
};

/**
 * Monthly climate (P1.2). Each day's weather is rolled from the clock seed and the
 * day number, so a save always replays the same weather. Air temperature follows a
 * daily curve between the month's min and max. Snow depth and ground wetness build
 * up and melt/dry hour by hour. The result is pushed to the survival subsystem
 * (weather, severity, ambient temperature). Driven by the fixed-step coordinator.
 */
UCLASS()
class MYPROJECT3_API UWorldClimateSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UWorldClimateSubsystem();

    static constexpr int32 CurrentSchemaVersion = 1;

    /** January .. December. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") TArray<FWorldMonthClimate> Months;
    /** Snow added per hour of full-severity snowfall. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float SnowfallCmPerHour = 2.0f;
    /** Snow melted per hour for each degree above zero. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float MeltCmPerDegreeHour = 0.15f;
    /** Rain on snow melts it this many times faster. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float RainMeltMultiplier = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float MaxSnowDepthCm = 300.0f;
    /** Wetness added per hour of full-severity rain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float WetnessPerRainHour = 0.12f;
    /** Wetness added per centimetre of melted snow. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float WetnessPerMeltedCm = 0.02f;
    /** Wetness removed per hour for each degree above zero on a dry day. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Climate") float DryingPerDegreeHour = 0.003f;

    /** Starts the climate at the clock's current time (rolls today's weather). */
    UFUNCTION(BlueprintCallable, Category="World|Climate") bool InitializeClimate();
    UFUNCTION(BlueprintPure, Category="World|Climate") bool IsClimateInitialized() const { return State.bInitialized; }

    /** Pure preflight for the fixed-step coordinator. TargetTime is the step's end time. */
    UFUNCTION(BlueprintPure, Category="World|Climate") bool CanAdvanceClimateStep(double DeltaSeconds, double TargetTimeSeconds) const;
    /** Advances snow, wetness, temperature and the daily weather to TargetTime and pushes them to survival. */
    UFUNCTION(BlueprintCallable, Category="World|Climate") bool AdvanceClimateStep(double DeltaSeconds, double TargetTimeSeconds);

    UFUNCTION(BlueprintPure, Category="World|Climate") EWorldWeatherState GetWeather() const { return State.Weather; }
    UFUNCTION(BlueprintPure, Category="World|Climate") float GetWeatherSeverity() const { return State.Severity; }
    UFUNCTION(BlueprintPure, Category="World|Climate") float GetTemperatureCelsius() const { return State.TemperatureCelsius; }
    UFUNCTION(BlueprintPure, Category="World|Climate") float GetSnowDepthCm() const { return State.SnowDepthCm; }
    UFUNCTION(BlueprintPure, Category="World|Climate") float GetGroundWetness() const { return State.GroundWetness; }
    UFUNCTION(BlueprintPure, Category="World|Climate") double GetRainfallIndex() const { return State.RainfallIndex; }

    /** Air temperature of a month at an hour of the day (no weather effect). */
    UFUNCTION(BlueprintPure, Category="World|Climate") float GetBaseTemperature(int32 Month, float HourOfDay) const;
    /** The weather a given day rolls (deterministic). */
    bool RollWeatherForDay(int32 Seed, int64 DayIndex, int32 Month, EWorldWeatherState& OutWeather, float& OutSeverity) const;

    FWorldClimateSnapshot ExportSnapshot() const { return State; }
    bool RestoreSnapshot(const FWorldClimateSnapshot& Snapshot);

private:
    bool IsValidConfig() const;
    void PushToSurvival() const;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Climate") FWorldClimateSnapshot State;
};
