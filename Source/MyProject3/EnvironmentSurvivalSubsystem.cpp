#include "EnvironmentSurvivalSubsystem.h"

FWorldOperationResult UEnvironmentSurvivalSubsystem::Failure(EWorldOperationResultCode Code, const FGuid& OperationId, const TCHAR* Message) const
{
    return FWorldOperationResult::Failure(Code, OperationId, FText::FromString(Message));
}

bool UEnvironmentSurvivalSubsystem::ValidateAndRemember(const FGuid& OperationId, FWorldOperationResult& OutFailure)
{
    if (!OperationId.IsValid())
    {
        OutFailure = Failure(EWorldOperationResultCode::InvalidState, OperationId, TEXT("Operation ID must be valid."));
        return false;
    }
    if (ProcessedOperationIds.Contains(OperationId))
    {
        OutFailure = Failure(EWorldOperationResultCode::Conflict, OperationId, TEXT("Operation ID already processed."));
        return false;
    }
    ProcessedOperationIds.Add(OperationId);
    return true;
}

FWorldOperationResult UEnvironmentSurvivalSubsystem::InitializeEnvironment(int64 WorldSeed, double InitialWorldTimeSeconds, FGuid OperationId)
{
    if (bInitialized)
    {
        return Failure(EWorldOperationResultCode::Conflict, OperationId, TEXT("Environment is already initialized."));
    }
    if (!OperationId.IsValid() || !FMath::IsFinite(InitialWorldTimeSeconds) || InitialWorldTimeSeconds < 0.0)
    {
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Seed context or initial world time is invalid."));
    }
    EnvironmentState = FEnvironmentWeatherState();
    EnvironmentState.WorldSeed = WorldSeed;
    EnvironmentState.WorldTimeSeconds = InitialWorldTimeSeconds;
    EnvironmentState.WeatherTransitionIndex = FMath::FloorToInt64(InitialWorldTimeSeconds / SecondsPerWeatherPeriod);
    bInitialized = true;
    UpdateSeason();
    ChooseWeatherForCurrentPeriod();
    EnvironmentState.WeatherRevision = 1;
    ProcessedOperationIds.Add(OperationId);
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Environment initialized deterministically.")), {});
}

FWorldOperationResult UEnvironmentSurvivalSubsystem::AdvanceEnvironment(double DeltaSeconds, FGuid OperationId)
{
    FWorldOperationResult FailureResult;
    if (!ValidateAndRemember(OperationId, FailureResult))
    {
        return FailureResult;
    }
    if (!bInitialized || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 7.0 * 24.0 * SecondsPerGameHour)
    {
        ProcessedOperationIds.Remove(OperationId);
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Environment must be initialized and delta must be positive and bounded."));
    }

    const EWorldSeason OldSeason = EnvironmentState.Season;
    const EWorldWeather OldWeather = EnvironmentState.Weather;
    const int64 OldPeriod = EnvironmentState.WeatherTransitionIndex;
    EnvironmentState.WorldTimeSeconds += DeltaSeconds;
    UpdateSeason();
    const int64 NewPeriod = FMath::FloorToInt64(EnvironmentState.WorldTimeSeconds / SecondsPerWeatherPeriod);
    if (NewPeriod != OldPeriod)
    {
        EnvironmentState.WeatherTransitionIndex = NewPeriod;
        ChooseWeatherForCurrentPeriod();
    }
    if (OldSeason != EnvironmentState.Season || OldWeather != EnvironmentState.Weather)
    {
        ++EnvironmentState.WeatherRevision;
    }
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Environment time advanced.")), {});
}

void UEnvironmentSurvivalSubsystem::UpdateSeason()
{
    const int64 SeasonIndex = FMath::FloorToInt64(EnvironmentState.WorldTimeSeconds / SecondsPerSeason);
    EnvironmentState.Season = static_cast<EWorldSeason>(SeasonIndex % 4);
}

void UEnvironmentSurvivalSubsystem::ChooseWeatherForCurrentPeriod()
{
    const uint32 Seed = static_cast<uint32>(EnvironmentState.WorldSeed) ^ static_cast<uint32>(EnvironmentState.WeatherTransitionIndex * 2654435761LL);
    FRandomStream Stream(static_cast<int32>(Seed));
    const int32 Roll = Stream.RandRange(0, 99);
    switch (EnvironmentState.Season)
    {
    case EWorldSeason::Spring:
        EnvironmentState.Weather = Roll < 35 ? EWorldWeather::Clear : Roll < 60 ? EWorldWeather::Cloudy : Roll < 82 ? EWorldWeather::Rain : Roll < 95 ? EWorldWeather::HeavyRain : EWorldWeather::Storm;
        break;
    case EWorldSeason::Summer:
        EnvironmentState.Weather = Roll < 48 ? EWorldWeather::Clear : Roll < 68 ? EWorldWeather::Cloudy : Roll < 82 ? EWorldWeather::Rain : Roll < 94 ? EWorldWeather::Heatwave : EWorldWeather::Storm;
        break;
    case EWorldSeason::Autumn:
        EnvironmentState.Weather = Roll < 25 ? EWorldWeather::Clear : Roll < 45 ? EWorldWeather::Cloudy : Roll < 72 ? EWorldWeather::Rain : Roll < 90 ? EWorldWeather::HeavyRain : EWorldWeather::Storm;
        break;
    case EWorldSeason::Winter:
        EnvironmentState.Weather = Roll < 25 ? EWorldWeather::Clear : Roll < 42 ? EWorldWeather::Cloudy : Roll < 70 ? EWorldWeather::Snow : Roll < 91 ? EWorldWeather::Blizzard : EWorldWeather::Storm;
        break;
    }
    ApplyWeatherProfile();
}

void UEnvironmentSurvivalSubsystem::ApplyWeatherProfile()
{
    const float SeasonalBase = EnvironmentState.Season == EWorldSeason::Summer ? 22.0f :
        EnvironmentState.Season == EWorldSeason::Spring ? 12.0f :
        EnvironmentState.Season == EWorldSeason::Autumn ? 8.0f : -5.0f;
    EnvironmentState.AmbientTemperatureC = SeasonalBase;
    EnvironmentState.Precipitation = 0.0f;
    EnvironmentState.WindIntensity = 0.15f;
    switch (EnvironmentState.Weather)
    {
    case EWorldWeather::Clear: EnvironmentState.WindIntensity = 0.1f; break;
    case EWorldWeather::Cloudy: EnvironmentState.WindIntensity = 0.25f; break;
    case EWorldWeather::Rain: EnvironmentState.Precipitation = 0.45f; EnvironmentState.WindIntensity = 0.35f; EnvironmentState.AmbientTemperatureC -= 3.0f; break;
    case EWorldWeather::HeavyRain: EnvironmentState.Precipitation = 0.8f; EnvironmentState.WindIntensity = 0.6f; EnvironmentState.AmbientTemperatureC -= 5.0f; break;
    case EWorldWeather::Snow: EnvironmentState.Precipitation = 0.5f; EnvironmentState.WindIntensity = 0.35f; EnvironmentState.AmbientTemperatureC -= 4.0f; break;
    case EWorldWeather::Storm: EnvironmentState.Precipitation = 0.9f; EnvironmentState.WindIntensity = 0.95f; EnvironmentState.AmbientTemperatureC -= 4.0f; break;
    case EWorldWeather::Heatwave: EnvironmentState.AmbientTemperatureC += 12.0f; EnvironmentState.WindIntensity = 0.1f; break;
    case EWorldWeather::Blizzard: EnvironmentState.Precipitation = 0.9f; EnvironmentState.WindIntensity = 1.0f; EnvironmentState.AmbientTemperatureC -= 12.0f; break;
    }
}

FWorldOperationResult UEnvironmentSurvivalSubsystem::SimulateSurvivalStep(FSurvivalState& InOutState, const FSurvivalProtectionProfile& Protection, float ActivityLoad, bool bResting, double DeltaSeconds, FGuid OperationId)
{
    FWorldOperationResult FailureResult;
    if (!ValidateAndRemember(OperationId, FailureResult))
    {
        return FailureResult;
    }
    if (!bInitialized || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 24.0 * SecondsPerGameHour ||
        !FMath::IsFinite(ActivityLoad) || ActivityLoad < 0.0f || ActivityLoad > 2.0f)
    {
        ProcessedOperationIds.Remove(OperationId);
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Survival step inputs are invalid or outside supported bounds."));
    }

    const FSurvivalState Before = InOutState;
    const double GameHours = DeltaSeconds / SecondsPerGameHour;
    const float Shelter = FMath::Clamp(Protection.ShelterQuality, 0.0f, 1.0f);
    const float Insulation = FMath::Clamp(Protection.ClothingInsulation, 0.0f, 1.0f);
    const float Waterproof = FMath::Clamp(Protection.Waterproofing, 0.0f, 1.0f);
    const float HeatSource = FMath::Clamp(Protection.HeatSourceStrength, 0.0f, 1.0f);
    const float RainExposure = EnvironmentState.Precipitation * (1.0f - Shelter) * (1.0f - Waterproof);
    const float WindExposure = EnvironmentState.WindIntensity * (1.0f - Shelter);
    const float EffectiveTemperature = EnvironmentState.AmbientTemperatureC + (HeatSource * 18.0f * Shelter) - (WindExposure * 8.0f);

    FSurvivalState Next = Before;
    Next.Hunger = FMath::Clamp(Before.Hunger - Tuning.HungerLossPerGameHour * static_cast<float>(GameHours) * (1.0f + ActivityLoad * 0.4f), 0.0f, 100.0f);
    Next.Thirst = FMath::Clamp(Before.Thirst - Tuning.ThirstLossPerGameHour * static_cast<float>(GameHours) * (1.0f + ActivityLoad * 0.7f + FMath::Max(0.0f, EffectiveTemperature - 25.0f) * 0.025f), 0.0f, 100.0f);

    if (bResting)
    {
        Next.Fatigue = FMath::Clamp(Before.Fatigue - Tuning.RestFatigueRecoveryPerGameHour * static_cast<float>(GameHours), 0.0f, 100.0f);
        Next.Stamina = FMath::Clamp(Before.Stamina + Tuning.RestStaminaRecoveryPerGameHour * static_cast<float>(GameHours), 0.0f, 100.0f);
    }
    else
    {
        Next.Fatigue = FMath::Clamp(Before.Fatigue + Tuning.FatigueGainPerGameHour * static_cast<float>(GameHours) * (0.5f + ActivityLoad), 0.0f, 100.0f);
        Next.Stamina = FMath::Clamp(Before.Stamina - Tuning.ActivityStaminaLossPerGameHour * static_cast<float>(GameHours) * ActivityLoad, 0.0f, 100.0f);
    }

    if (RainExposure > 0.0f)
    {
        Next.Wetness = FMath::Clamp(Before.Wetness + Tuning.WetnessGainPerGameHour * RainExposure * static_cast<float>(GameHours), 0.0f, 100.0f);
    }
    else
    {
        Next.Wetness = FMath::Clamp(Before.Wetness - Tuning.WetnessDryPerGameHour * (0.25f + Shelter) * static_cast<float>(GameHours), 0.0f, 100.0f);
    }

    const float ColdSeverity = FMath::Clamp((10.0f - EffectiveTemperature) / 25.0f, 0.0f, 1.0f) * (1.0f - Insulation);
    const float HeatSeverity = FMath::Clamp((EffectiveTemperature - 28.0f) / 20.0f, 0.0f, 1.0f);
    const float WetColdMultiplier = 1.0f + Next.Wetness / 100.0f;
    Next.Warmth = FMath::Clamp(Before.Warmth - Tuning.ColdExposurePerGameHour * ColdSeverity * WetColdMultiplier * static_cast<float>(GameHours)
        + Tuning.HeatExposurePerGameHour * HeatSeverity * static_cast<float>(GameHours), 0.0f, 100.0f);

    if (Next.Hunger <= 0.0f || Next.Thirst <= 0.0f || Next.Warmth <= 0.0f)
    {
        Next.Health = FMath::Clamp(Before.Health - 2.0f * static_cast<float>(GameHours), 0.0f, 100.0f);
    }
    if (Next.Fatigue >= 95.0f)
    {
        Next.Stamina = FMath::Min(Next.Stamina, 20.0f);
    }
    Next.Revision = Before.Revision + 1;
    InOutState = Next;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Survival state advanced with weather, shelter and clothing modifiers.")), {});
}

void UEnvironmentSurvivalSubsystem::SetSurvivalTuning(const FSurvivalTuning& NewTuning)
{
    Tuning.HungerLossPerGameHour = FMath::Max(0.0f, NewTuning.HungerLossPerGameHour);
    Tuning.ThirstLossPerGameHour = FMath::Max(0.0f, NewTuning.ThirstLossPerGameHour);
    Tuning.FatigueGainPerGameHour = FMath::Max(0.0f, NewTuning.FatigueGainPerGameHour);
    Tuning.RestFatigueRecoveryPerGameHour = FMath::Max(0.0f, NewTuning.RestFatigueRecoveryPerGameHour);
    Tuning.RestStaminaRecoveryPerGameHour = FMath::Max(0.0f, NewTuning.RestStaminaRecoveryPerGameHour);
    Tuning.ActivityStaminaLossPerGameHour = FMath::Max(0.0f, NewTuning.ActivityStaminaLossPerGameHour);
    Tuning.ColdExposurePerGameHour = FMath::Max(0.0f, NewTuning.ColdExposurePerGameHour);
    Tuning.HeatExposurePerGameHour = FMath::Max(0.0f, NewTuning.HeatExposurePerGameHour);
    Tuning.WetnessGainPerGameHour = FMath::Max(0.0f, NewTuning.WetnessGainPerGameHour);
    Tuning.WetnessDryPerGameHour = FMath::Max(0.0f, NewTuning.WetnessDryPerGameHour);
}
