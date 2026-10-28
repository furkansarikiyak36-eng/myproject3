#include "HeatSourceComponent.h"

#include "WorldSurvivalSubsystem.h"

#include "Engine/World.h"

UHeatSourceComponent::UHeatSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHeatSourceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr)
    {
        Survival->RegisterHeatSource(this);
    }
}

void UHeatSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr)
    {
        Survival->UnregisterHeatSource(this);
    }
    Super::EndPlay(EndPlayReason);
}

bool UHeatSourceComponent::AddFuel(float BurnSeconds)
{
    if (!FMath::IsFinite(BurnSeconds) || BurnSeconds <= 0.0f ||
        !FMath::IsFinite(FuelSeconds) || FuelSeconds < 0.0f ||
        !FMath::IsFinite(MaxFuelSeconds) || MaxFuelSeconds <= 0.0f ||
        FuelSeconds >= MaxFuelSeconds || !FMath::IsFinite(FuelSeconds + BurnSeconds))
        return false;
    FuelSeconds = FMath::Min(MaxFuelSeconds, FuelSeconds + BurnSeconds);
    return true;
}

bool UHeatSourceComponent::Ignite()
{
    if (FuelSeconds <= 0.0f)
        return false;
    bLit = true;
    return true;
}

void UHeatSourceComponent::Extinguish()
{
    bLit = false;
}

float UHeatSourceComponent::GetHeatAt(const FVector& WorldLocation) const
{
    if (!bLit || !FMath::IsFinite(RadiusCm) || RadiusCm <= 0.0f ||
        !FMath::IsFinite(HeatOutputCelsius) ||
        !FMath::IsFinite(WorldLocation.X) || !FMath::IsFinite(WorldLocation.Y) ||
        !FMath::IsFinite(WorldLocation.Z))
        return 0.0f;
    const float Distance = FVector::Dist(GetComponentLocation(), WorldLocation);
    if (Distance >= RadiusCm)
        return 0.0f;
    // Quadratic falloff: warm next to the fire, little at the edge.
    const float Closeness = 1.0f - Distance / RadiusCm;
    return HeatOutputCelsius * Closeness * Closeness;
}

void UHeatSourceComponent::AdvanceFuel(double DeltaSeconds, EWorldWeatherState Weather, float WeatherSeverity)
{
    if (!bLit || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
        return;

    // Heavy rain and storms drown a fire that is not sheltered enough.
    const bool bWetWeather = Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm;
    if (bWetWeather && WeatherSeverity > WeatherProtection)
    {
        bLit = false;
        return;
    }

    FuelSeconds = FMath::Max(0.0f, FuelSeconds - static_cast<float>(DeltaSeconds));
    if (FuelSeconds <= 0.0f)
        bLit = false;
}


bool UHeatSourceComponent::RestoreRuntimeState(float InFuelSeconds, bool bInLit)
{
    if (!FMath::IsFinite(InFuelSeconds) || !FMath::IsFinite(MaxFuelSeconds) ||
        MaxFuelSeconds <= 0.0f || InFuelSeconds < 0.0f || InFuelSeconds > MaxFuelSeconds ||
        (bInLit && InFuelSeconds <= 0.0f))
        return false;
    FuelSeconds = InFuelSeconds;
    bLit = bInLit;
    return true;
}
