#pragma once
#include "CoreMinimal.h"
#include "EnvironmentSurvivalTypes.generated.h"

UENUM(BlueprintType)
enum class EWorldSeason : uint8 { Spring, Summer, Autumn, Winter };

UENUM(BlueprintType)
enum class EWorldWeather : uint8 { Clear, Cloudy, Rain, HeavyRain, Snow, Storm, Heatwave, Blizzard };

USTRUCT(BlueprintType)
struct FEnvironmentWeatherState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    int64 WorldSeed = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    double WorldTimeSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    EWorldSeason Season = EWorldSeason::Spring;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    EWorldWeather Weather = EWorldWeather::Clear;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float AmbientTemperatureC = 12.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float Precipitation = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float WindIntensity = 0.1f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    int64 WeatherRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    int64 WeatherTransitionIndex = 0;
};

USTRUCT(BlueprintType)
struct FSurvivalProtectionProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ClothingInsulation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Waterproofing = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ShelterQuality = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival", meta=(ClampMin="0.0", ClampMax="1.0"))
    float HeatSourceStrength = 0.0f;
};

USTRUCT(BlueprintType)
struct FSurvivalTuning
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float HungerLossPerGameHour = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float ThirstLossPerGameHour = 1.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float FatigueGainPerGameHour = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float RestFatigueRecoveryPerGameHour = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float RestStaminaRecoveryPerGameHour = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float ActivityStaminaLossPerGameHour = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float ColdExposurePerGameHour = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float HeatExposurePerGameHour = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float WetnessGainPerGameHour = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float WetnessDryPerGameHour = 5.0f;
};
