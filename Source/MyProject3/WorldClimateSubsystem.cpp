#include "WorldClimateSubsystem.h"

#include "WorldCalendar.h"
#include "WorldSimulationClockSubsystem.h"
#include "Engine/World.h"

namespace
{
    FWorldMonthClimate MakeMonth(float Min, float Max, float Wet, float Storm, float Wind, float Heat)
    {
        FWorldMonthClimate Month;
        Month.MinTemperatureCelsius = Min;
        Month.MaxTemperatureCelsius = Max;
        Month.PrecipitationChance = Wet;
        Month.StormChance = Storm;
        Month.WindChance = Wind;
        Month.HeatwaveChance = Heat;
        return Month;
    }

    /** Deterministic 0..1 value from a seed, a day and a salt. */
    float Hash01(int32 Seed, int64 Day, uint32 Salt)
    {
        uint64 X = static_cast<uint64>(static_cast<uint32>(Seed)) * 0x9E3779B97F4A7C15ull;
        X ^= static_cast<uint64>(Day) + 0xBF58476D1CE4E5B9ull + (X << 6) + (X >> 2);
        X ^= static_cast<uint64>(Salt) * 0x94D049BB133111EBull;
        X ^= X >> 31;
        X *= 0xD6E8FEB86659FD93ull;
        X ^= X >> 29;
        return static_cast<float>((X >> 40) & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    bool IsWet(EWorldWeatherState Weather)
    {
        return Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Snow ||
            Weather == EWorldWeatherState::Storm;
    }
}

UWorldClimateSubsystem::UWorldClimateSubsystem()
{
    // Min, max, wet days, storm share of wet days, wind share of dry days, heatwave share of dry days.
    Months = {
        MakeMonth(-14.0f, -4.0f, 0.45f, 0.30f, 0.30f, 0.00f), // January: deep snow, blizzards
        MakeMonth(-12.0f, -2.0f, 0.40f, 0.25f, 0.30f, 0.00f), // February: frost
        MakeMonth( -5.0f,  5.0f, 0.45f, 0.10f, 0.25f, 0.00f), // March: thaw
        MakeMonth(  0.0f, 11.0f, 0.55f, 0.10f, 0.20f, 0.00f), // April: rain, late frost
        MakeMonth(  5.0f, 17.0f, 0.40f, 0.15f, 0.15f, 0.00f), // May: mild, hail storms
        MakeMonth(  9.0f, 22.0f, 0.30f, 0.25f, 0.15f, 0.05f), // June: warm, storms
        MakeMonth( 12.0f, 27.0f, 0.15f, 0.20f, 0.10f, 0.25f), // July: dry, hot
        MakeMonth( 11.0f, 26.0f, 0.15f, 0.30f, 0.15f, 0.20f), // August: dry, lightning
        MakeMonth(  6.0f, 18.0f, 0.30f, 0.10f, 0.20f, 0.00f), // September: cooling
        MakeMonth(  1.0f, 11.0f, 0.50f, 0.20f, 0.35f, 0.00f), // October: rain and wind
        MakeMonth( -4.0f,  4.0f, 0.45f, 0.15f, 0.30f, 0.00f), // November: first snow
        MakeMonth(-10.0f, -1.0f, 0.45f, 0.25f, 0.30f, 0.00f)  // December: snow
    };
}

bool UWorldClimateSubsystem::IsValidConfig() const
{
    if (Months.Num() != 12)
        return false;
    for (const FWorldMonthClimate& Month : Months)
    {
        auto Chance = [](float Value) { return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f; };
        if (!FMath::IsFinite(Month.MinTemperatureCelsius) || !FMath::IsFinite(Month.MaxTemperatureCelsius) ||
            Month.MinTemperatureCelsius > Month.MaxTemperatureCelsius ||
            !Chance(Month.PrecipitationChance) || !Chance(Month.StormChance) ||
            !Chance(Month.WindChance) || !Chance(Month.HeatwaveChance))
            return false;
    }
    auto NonNegative = [](float Value) { return FMath::IsFinite(Value) && Value >= 0.0f; };
    return NonNegative(SnowfallCmPerHour) && NonNegative(MeltCmPerDegreeHour) &&
        NonNegative(RainMeltMultiplier) && NonNegative(MaxSnowDepthCm) &&
        NonNegative(WetnessPerRainHour) && NonNegative(WetnessPerMeltedCm) && NonNegative(DryingPerDegreeHour);
}

float UWorldClimateSubsystem::GetBaseTemperature(int32 Month, float HourOfDay) const
{
    if (!Months.IsValidIndex(Month - 1) || !FMath::IsFinite(HourOfDay))
        return 0.0f;
    const FWorldMonthClimate& Climate = Months[Month - 1];
    // Coldest at 05:00, warmest at 15:00: 10 hours of warming, 14 hours of cooling.
    const float Hour = FMath::Fmod(FMath::Fmod(HourOfDay, 24.0f) + 24.0f, 24.0f);
    const float SinceColdest = FMath::Fmod(Hour - 5.0f + 24.0f, 24.0f);
    const float Fraction = SinceColdest <= 10.0f
        ? SinceColdest / 10.0f                     // 0 at 05:00 -> 1 at 15:00
        : 1.0f - (SinceColdest - 10.0f) / 14.0f;   // 1 at 15:00 -> 0 at 05:00
    const float Warmth = 0.5f - 0.5f * FMath::Cos(Fraction * PI);
    return FMath::Lerp(Climate.MinTemperatureCelsius, Climate.MaxTemperatureCelsius, Warmth);
}

bool UWorldClimateSubsystem::RollWeatherForDay(
    int32 Seed, int64 DayIndex, int32 Month, EWorldWeatherState& OutWeather, float& OutSeverity) const
{
    if (!Months.IsValidIndex(Month - 1) || DayIndex < 0)
        return false;
    const FWorldMonthClimate& Climate = Months[Month - 1];
    const float RollWet = Hash01(Seed, DayIndex, 1);
    const float RollKind = Hash01(Seed, DayIndex, 2);
    const float RollSeverity = Hash01(Seed, DayIndex, 3);
    // Snow instead of rain when the warmest hour of the month stays near freezing.
    const bool bFreezing = Climate.MaxTemperatureCelsius <= 1.0f ||
        (Climate.MinTemperatureCelsius < 0.0f && Hash01(Seed, DayIndex, 4) < 0.5f);
    if (RollWet < Climate.PrecipitationChance)
    {
        if (RollKind < Climate.StormChance)
            OutWeather = EWorldWeatherState::Storm;
        else
            OutWeather = bFreezing ? EWorldWeatherState::Snow : EWorldWeatherState::Rain;
        OutSeverity = 0.3f + 0.7f * RollSeverity;
        return true;
    }
    if (RollKind < Climate.HeatwaveChance)
    {
        OutWeather = EWorldWeatherState::Heatwave;
        OutSeverity = 0.4f + 0.6f * RollSeverity;
    }
    else if (RollKind < Climate.HeatwaveChance + Climate.WindChance)
    {
        OutWeather = EWorldWeatherState::Wind;
        OutSeverity = 0.2f + 0.6f * RollSeverity;
    }
    else
    {
        OutWeather = EWorldWeatherState::Clear;
        OutSeverity = 0.0f;
    }
    return true;
}

bool UWorldClimateSubsystem::InitializeClimate()
{
    UWorld* World = GetWorld();
    const UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    FWorldCalendarDate Date;
    if (!Clock || !IsValidConfig() || !Clock->GetCalendarDate(Date))
        return false;
    FWorldClimateSnapshot Fresh;
    Fresh.bInitialized = true;
    Fresh.WeatherDayIndex = Date.DaysElapsed;
    if (!RollWeatherForDay(Clock->GetRandomSeed(), Date.DaysElapsed, Date.Month, Fresh.Weather, Fresh.Severity))
        return false;
    Fresh.TemperatureCelsius = GetBaseTemperature(Date.Month, Date.HourOfDay);
    // A start in winter or early spring finds the snow of the past months on the ground.
    if (Date.Month <= 3 || Date.Month == 12)
        Fresh.SnowDepthCm = Date.Month == 3 ? 20.0f : 40.0f;
    State = Fresh;
    PushToSurvival();
    return true;
}

bool UWorldClimateSubsystem::CanAdvanceClimateStep(double DeltaSeconds, double TargetTimeSeconds) const
{
    if (!State.bInitialized || !IsValidConfig() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        !FMath::IsFinite(TargetTimeSeconds) || TargetTimeSeconds < DeltaSeconds)
        return false;
    const UWorld* World = GetWorld();
    const UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    FWorldCalendarDate Date;
    return Clock && WorldCalendar::DateFromTime(TargetTimeSeconds, Clock->CalendarConfig, Date);
}

bool UWorldClimateSubsystem::AdvanceClimateStep(double DeltaSeconds, double TargetTimeSeconds)
{
    if (!CanAdvanceClimateStep(DeltaSeconds, TargetTimeSeconds))
        return false;
    const UWorldSimulationClockSubsystem* Clock = GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>();
    FWorldCalendarDate Date;
    if (!WorldCalendar::DateFromTime(TargetTimeSeconds, Clock->CalendarConfig, Date))
        return false;

    FWorldClimateSnapshot Next = State;
    if (Date.DaysElapsed != Next.WeatherDayIndex)
    {
        if (!RollWeatherForDay(Clock->GetRandomSeed(), Date.DaysElapsed, Date.Month, Next.Weather, Next.Severity))
            return false;
        Next.WeatherDayIndex = Date.DaysElapsed;
    }

    // Weather shifts the day's air: storms and snow are colder, a heatwave hotter.
    float Temperature = GetBaseTemperature(Date.Month, Date.HourOfDay);
    if (Next.Weather == EWorldWeatherState::Heatwave)
        Temperature += 6.0f * Next.Severity;
    else if (Next.Weather == EWorldWeatherState::Storm || Next.Weather == EWorldWeatherState::Snow)
        Temperature -= 3.0f * Next.Severity;
    Next.TemperatureCelsius = Temperature;

    const float Hours = static_cast<float>(DeltaSeconds / 3600.0);
    const bool bPrecipitating = IsWet(Next.Weather);
    const bool bSnowing = Next.Weather == EWorldWeatherState::Snow ||
        (Next.Weather == EWorldWeatherState::Storm && Temperature <= 0.0f);
    if (bPrecipitating && !bSnowing)
        Next.RainfallIndex += static_cast<double>(Next.Severity) * Hours;
    if (bSnowing)
        Next.SnowDepthCm += SnowfallCmPerHour * Next.Severity * Hours;
    float Melted = 0.0f;
    if (Temperature > 0.0f && Next.SnowDepthCm > 0.0f)
    {
        const float RainFactor = (bPrecipitating && !bSnowing) ? RainMeltMultiplier : 1.0f;
        Melted = FMath::Min(Next.SnowDepthCm, MeltCmPerDegreeHour * Temperature * RainFactor * Hours);
        Next.SnowDepthCm -= Melted;
    }
    Next.SnowDepthCm = FMath::Clamp(Next.SnowDepthCm, 0.0f, MaxSnowDepthCm);

    float Wetness = Next.GroundWetness + Melted * WetnessPerMeltedCm;
    if (bPrecipitating && !bSnowing)
        Wetness += WetnessPerRainHour * Next.Severity * Hours;
    else if (!bPrecipitating && Temperature > 0.0f)
        Wetness -= DryingPerDegreeHour * Temperature * Hours *
            (Next.Weather == EWorldWeatherState::Wind || Next.Weather == EWorldWeatherState::Heatwave ? 2.0f : 1.0f);
    Next.GroundWetness = FMath::Clamp(Wetness, 0.0f, 1.0f);

    State = Next;
    PushToSurvival();
    return true;
}

void UWorldClimateSubsystem::PushToSurvival() const
{
    UWorld* World = GetWorld();
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!Survival)
        return;
    Survival->SetWeather(State.Weather, State.Severity);
    Survival->AmbientTemperatureCelsius = State.TemperatureCelsius;
}

bool UWorldClimateSubsystem::RestoreSnapshot(const FWorldClimateSnapshot& Snapshot)
{
    if (Snapshot.SchemaVersion != CurrentSchemaVersion ||
        !FMath::IsFinite(Snapshot.Severity) || Snapshot.Severity < 0.0f || Snapshot.Severity > 1.0f ||
        !FMath::IsFinite(Snapshot.TemperatureCelsius) ||
        !FMath::IsFinite(Snapshot.SnowDepthCm) || Snapshot.SnowDepthCm < 0.0f ||
        !FMath::IsFinite(Snapshot.RainfallIndex) || Snapshot.RainfallIndex < 0.0 ||
        !FMath::IsFinite(Snapshot.GroundWetness) || Snapshot.GroundWetness < 0.0f || Snapshot.GroundWetness > 1.0f ||
        static_cast<uint8>(Snapshot.Weather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        (Snapshot.bInitialized && Snapshot.WeatherDayIndex < 0))
        return false;
    State = Snapshot;
    if (State.bInitialized)
        PushToSurvival();
    return true;
}
