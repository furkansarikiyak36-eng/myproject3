#include "WorldSurvivalSubsystem.h"

#include "HeatSourceComponent.h"

namespace
{
    bool InRange(float Value, float Min, float Max)
    {
        return FMath::IsFinite(Value) && Value >= Min && Value <= Max;
    }
}

bool UWorldSurvivalSubsystem::IsValidProfile(const FWorldSurvivalProfile& Profile) const
{
    return Profile.EntityId.IsValid() && Profile.Revision > 0 &&
        InRange(Profile.Health, 0.0f, 100.0f) && InRange(Profile.Hunger, 0.0f, 100.0f) &&
        InRange(Profile.Thirst, 0.0f, 100.0f) && InRange(Profile.Stamina, 0.0f, 100.0f) &&
        InRange(Profile.Fatigue, 0.0f, 100.0f) && InRange(Profile.ClothingInsulation, 0.0f, 1.0f) &&
        InRange(Profile.ShelterProtection, 0.0f, 1.0f) && InRange(Profile.Wetness, 0.0f, 1.0f) &&
        InRange(Profile.CaloriesKcal, 0.0f, MaxCaloriesKcal) &&
        InRange(Profile.CoreTemperatureCelsius, 20.0f, 45.0f) &&
        InRange(Profile.PendingActivityKcal, 0.0f, 100000.0f) &&
        FMath::IsFinite(Profile.Location.X) && FMath::IsFinite(Profile.Location.Y) &&
        FMath::IsFinite(Profile.Location.Z) && FMath::IsFinite(Profile.FeltTemperatureCelsius);
}

bool UWorldSurvivalSubsystem::RegisterProfile(const FWorldSurvivalProfile& Profile)
{
    if (Profiles.Contains(Profile.EntityId) || !IsValidProfile(Profile))
        return false;
    Profiles.Add(Profile.EntityId, Profile);
    return true;
}

bool UWorldSurvivalSubsystem::UpdateProtection(
    const FGuid& EntityId, float ClothingInsulation, float ShelterProtection, int64 ExpectedRevision)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision != ExpectedRevision || Profile->Revision == MAX_int64 ||
        !InRange(ClothingInsulation, 0.0f, 1.0f) || !InRange(ShelterProtection, 0.0f, 1.0f))
    {
        return false;
    }
    Profile->ClothingInsulation = ClothingInsulation;
    Profile->ShelterProtection = ShelterProtection;
    ++Profile->Revision;
    return true;
}

bool UWorldSurvivalSubsystem::SetWeather(EWorldWeatherState NewWeather, float Severity)
{
    if (static_cast<uint8>(NewWeather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        !InRange(Severity, 0.0f, 1.0f)) return false;
    Weather = NewWeather;
    WeatherSeverity = Severity;
    return true;
}

bool UWorldSurvivalSubsystem::UpdateLocation(const FGuid& EntityId, FVector NewLocation)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision == MAX_int64 ||
        !FMath::IsFinite(NewLocation.X) || !FMath::IsFinite(NewLocation.Y) ||
        !FMath::IsFinite(NewLocation.Z))
        return false;
    if (Profile->Location.Equals(NewLocation))
        return true;
    Profile->Location = NewLocation;
    ++Profile->Revision;
    return true;
}

float UWorldSurvivalSubsystem::GetActivityKcalPerHour(ESurvivalActivity Activity) const
{
    switch (Activity)
    {
    case ESurvivalActivity::Walk:  return 200.0f;
    case ESurvivalActivity::Run:   return 600.0f;
    case ESurvivalActivity::Chop:  return 450.0f;
    case ESurvivalActivity::Dig:   return 500.0f;
    case ESurvivalActivity::Build: return 300.0f;
    case ESurvivalActivity::Carry: return 250.0f;
    default:                       return 0.0f;
    }
}

bool UWorldSurvivalSubsystem::RecordActivity(
    const FGuid& EntityId, ESurvivalActivity Activity, float DurationSeconds, float CarriedMassKg)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || !InRange(DurationSeconds, 0.0f, 3600.0f) || DurationSeconds == 0.0f ||
        !InRange(CarriedMassKg, 0.0f, 1000.0f) || Profile->Revision == MAX_int64)
    {
        return false;
    }
    const float Hours = DurationSeconds / 3600.0f;
    const float Kcal = (GetActivityKcalPerHour(Activity) + CarriedMassKg * CarryKcalPerKgHour) * Hours;
    Profile->PendingActivityKcal = FMath::Min(100000.0f, Profile->PendingActivityKcal + Kcal);
    // Hard work drains stamina now; rest recovers it during simulation steps.
    float StaminaPerHour = 30.0f;
    switch (Activity)
    {
    case ESurvivalActivity::Run:   StaminaPerHour = 600.0f; break;
    case ESurvivalActivity::Chop:
    case ESurvivalActivity::Dig:   StaminaPerHour = 300.0f; break;
    case ESurvivalActivity::Build: StaminaPerHour = 150.0f; break;
    case ESurvivalActivity::Carry: StaminaPerHour = 120.0f; break;
    default: break;
    }
    const float StaminaCost = (StaminaPerHour + CarriedMassKg * 6.0f) * Hours;
    Profile->Stamina = FMath::Clamp(Profile->Stamina - StaminaCost, 0.0f, 100.0f);
    ++Profile->Revision;
    return true;
}

bool UWorldSurvivalSubsystem::ConsumeFood(const FGuid& EntityId, float Kcal, float ThirstRelief)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || !InRange(Kcal, 0.0f, 10000.0f) || !InRange(ThirstRelief, 0.0f, 100.0f) ||
        (Kcal == 0.0f && ThirstRelief == 0.0f) || Profile->Revision == MAX_int64)
    {
        return false;
    }
    Profile->CaloriesKcal = FMath::Min(MaxCaloriesKcal, Profile->CaloriesKcal + Kcal);
    Profile->Hunger = 100.0f * (1.0f - Profile->CaloriesKcal / MaxCaloriesKcal);
    Profile->Thirst = FMath::Max(0.0f, Profile->Thirst - ThirstRelief);
    ++Profile->Revision;
    return true;
}

float UWorldSurvivalSubsystem::GetWeatherTemperatureDelta() const
{
    switch (Weather)
    {
    case EWorldWeatherState::Rain:     return -4.0f * WeatherSeverity;
    case EWorldWeatherState::Wind:     return -2.0f * WeatherSeverity;
    case EWorldWeatherState::Snow:     return -14.0f * WeatherSeverity;
    case EWorldWeatherState::Storm:    return -10.0f * WeatherSeverity;
    case EWorldWeatherState::Heatwave: return 18.0f * WeatherSeverity;
    case EWorldWeatherState::Clear:
    default:                           return 0.0f;
    }
}

float UWorldSurvivalSubsystem::GetWindChill() const
{
    switch (Weather)
    {
    case EWorldWeatherState::Wind:  return 8.0f * WeatherSeverity;
    case EWorldWeatherState::Storm: return 10.0f * WeatherSeverity;
    case EWorldWeatherState::Snow:  return 4.0f * WeatherSeverity;
    default:                        return 0.0f;
    }
}

float UWorldSurvivalSubsystem::GetHeatAt(const FVector& Location) const
{
    float Heat = 0.0f;
    for (const TWeakObjectPtr<UHeatSourceComponent>& Source : HeatSources)
    {
        if (Source.IsValid())
            Heat += Source->GetHeatAt(Location);
    }
    return Heat;
}

float UWorldSurvivalSubsystem::ComputeFeltTemperature(const FWorldSurvivalProfile& Profile) const
{
    const float Air = AmbientTemperatureCelsius + GetWeatherTemperatureDelta();
    const float WindChill = GetWindChill() * (1.0f - Profile.ShelterProtection);
    return Air - WindChill + GetHeatAt(Profile.Location) +
        Profile.ClothingInsulation * ClothingWarmthCelsius +
        Profile.ShelterProtection * ShelterWarmthCelsius -
        Profile.Wetness * WetnessPenaltyCelsius;
}

void UWorldSurvivalSubsystem::RegisterHeatSource(UHeatSourceComponent* Source)
{
    if (IsValid(Source))
        HeatSources.AddUnique(Source);
}

void UWorldSurvivalSubsystem::UnregisterHeatSource(UHeatSourceComponent* Source)
{
    HeatSources.RemoveAll([Source](const TWeakObjectPtr<UHeatSourceComponent>& Item)
    {
        return !Item.IsValid() || Item.Get() == Source;
    });
}

bool UWorldSurvivalSubsystem::CanAdvanceSurvivalStep(
    double DeltaSeconds, double LogicalTimeSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        !FMath::IsFinite(LogicalTimeSeconds) || LogicalTimeSeconds < 0.0 ||
        static_cast<uint8>(Weather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        !InRange(WeatherSeverity, 0.0f, 1.0f) ||
        !FMath::IsFinite(HungerPerHour) || HungerPerHour < 0.0f ||
        !FMath::IsFinite(ThirstPerHour) || ThirstPerHour < 0.0f ||
        !FMath::IsFinite(FatiguePerHour) || FatiguePerHour < 0.0f ||
        !FMath::IsFinite(ColdDamagePerHour) || ColdDamagePerHour < 0.0f ||
        !FMath::IsFinite(MaxCaloriesKcal) || MaxCaloriesKcal <= 0.0f ||
        !FMath::IsFinite(BasalKcalPerHour) || BasalKcalPerHour < 0.0f ||
        !FMath::IsFinite(CarryKcalPerKgHour) || CarryKcalPerKgHour < 0.0f ||
        !FMath::IsFinite(ShiverKcalPerDegreeHour) || ShiverKcalPerDegreeHour < 0.0f ||
        !FMath::IsFinite(AmbientTemperatureCelsius) ||
        !FMath::IsFinite(ClothingWarmthCelsius) ||
        !FMath::IsFinite(ShelterWarmthCelsius) ||
        !FMath::IsFinite(WetnessPenaltyCelsius) || WetnessPenaltyCelsius < 0.0f ||
        !FMath::IsFinite(ComfortTemperatureCelsius) ||
        !FMath::IsFinite(HypothermiaThresholdCelsius) ||
        !FMath::IsFinite(CoreCoolingPerDegreeHour) || CoreCoolingPerDegreeHour < 0.0f ||
        !FMath::IsFinite(CoreRecoveryPerHour) || CoreRecoveryPerHour < 0.0f)
        return false;
    for (const TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
        if (Pair.Value.Revision == MAX_int64 || !IsValidProfile(Pair.Value)) return false;
    return true;
}

bool UWorldSurvivalSubsystem::AdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds)
{
    if (!CanAdvanceSurvivalStep(DeltaSeconds, LogicalTimeSeconds)) return false;

    // Fuel burns on the same clock that drives the bodies it warms.
    for (const TWeakObjectPtr<UHeatSourceComponent>& Source : HeatSources)
    {
        if (Source.IsValid())
            Source->AdvanceFuel(DeltaSeconds, Weather, WeatherSeverity);
    }

    const float Hours = static_cast<float>(DeltaSeconds / 3600.0);
    for (TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
    {
        FWorldSurvivalProfile& P = Pair.Value;
        P.Thirst = FMath::Clamp(P.Thirst + ThirstPerHour * Hours, 0.0f, 100.0f);
        P.Fatigue = FMath::Clamp(P.Fatigue + FatiguePerHour * Hours, 0.0f, 100.0f);

        // Exposure: one felt temperature drives core temperature and shivering.
        P.FeltTemperatureCelsius = ComputeFeltTemperature(P);
        const float ColdDegrees = FMath::Max(0.0f, ComfortTemperatureCelsius - P.FeltTemperatureCelsius);
        if (ColdDegrees > 0.0f)
        {
            P.CoreTemperatureCelsius = FMath::Max(20.0f,
                P.CoreTemperatureCelsius - ColdDegrees * CoreCoolingPerDegreeHour * Hours);
        }
        else
        {
            P.CoreTemperatureCelsius = FMath::Min(37.0f, P.CoreTemperatureCelsius + CoreRecoveryPerHour * Hours);
        }
        const float HypothermiaDegrees = FMath::Max(0.0f, HypothermiaThresholdCelsius - P.CoreTemperatureCelsius);
        P.Health = FMath::Max(0.0f, P.Health - ColdDamagePerHour * HypothermiaDegrees * Hours);
        if (P.FeltTemperatureCelsius > 32.0f)
            P.Thirst = FMath::Clamp(P.Thirst + (P.FeltTemperatureCelsius - 32.0f) * 0.2f * Hours, 0.0f, 100.0f);

        // Calories: basal metabolism, reported activity and shivering.
        const float BurnKcal = BasalKcalPerHour * Hours + P.PendingActivityKcal +
            ColdDegrees * ShiverKcalPerDegreeHour * Hours;
        P.PendingActivityKcal = 0.0f;
        P.CaloriesKcal = FMath::Clamp(P.CaloriesKcal - BurnKcal, 0.0f, MaxCaloriesKcal);
        P.Hunger = 100.0f * (1.0f - P.CaloriesKcal / MaxCaloriesKcal);

        if (Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm)
            P.Wetness = FMath::Clamp(P.Wetness + WeatherSeverity * (1.0f - P.ShelterProtection) * Hours * 0.2f, 0.0f, 1.0f);
        // Warmth dries clothing faster.
        const float DryingBoost = FMath::Max(0.0f, P.FeltTemperatureCelsius - ComfortTemperatureCelsius) * 0.01f;
        P.Wetness = FMath::Max(0.0f, P.Wetness - Hours * (0.05f + P.ShelterProtection * 0.25f + DryingBoost));

        if (P.CaloriesKcal <= 0.0f || P.Thirst >= 98.0f)
            P.Health = FMath::Max(0.0f, P.Health - Hours * 2.0f);
        P.Stamina = FMath::Clamp(P.Stamina + (P.Fatigue < 50.0f ? 2.0f : -2.0f) * Hours, 0.0f, 100.0f);
        ++P.Revision;
    }
    return true;
}

bool UWorldSurvivalSubsystem::RecordDiscovery(
    FName DiscoveryId, FVector Location, double LogicalTimeSeconds)
{
    if (DiscoveryId.IsNone() || Location.ContainsNaN() || !FMath::IsFinite(LogicalTimeSeconds) ||
        LogicalTimeSeconds < 0.0)
    {
        return false;
    }
    if (Discoveries.ContainsByPredicate([DiscoveryId](const FWorldDiscoveryRecord& D)
        { return D.DiscoveryId == DiscoveryId; }))
    {
        return true; // discovery is idempotent
    }
    FWorldDiscoveryRecord Record;
    Record.DiscoveryId = DiscoveryId;
    Record.Location = Location;
    Record.DiscoveredAtTime = LogicalTimeSeconds;
    Discoveries.Add(Record);
    return true;
}

bool UWorldSurvivalSubsystem::GetProfile(
    const FGuid& EntityId, FWorldSurvivalProfile& OutProfile) const
{
    const FWorldSurvivalProfile* Found = Profiles.Find(EntityId);
    if (!Found) return false;
    OutProfile = *Found;
    return true;
}

void UWorldSurvivalSubsystem::ExportSnapshot(
    TArray<FWorldSurvivalProfile>& OutProfiles, TArray<FWorldDiscoveryRecord>& OutDiscoveries,
    EWorldWeatherState& OutWeather, float& OutSeverity) const
{
    Profiles.GenerateValueArray(OutProfiles);
    OutProfiles.Sort([](const FWorldSurvivalProfile& A, const FWorldSurvivalProfile& B)
        { return A.EntityId.ToString() < B.EntityId.ToString(); });
    OutDiscoveries = Discoveries;
    OutWeather = Weather;
    OutSeverity = WeatherSeverity;
}

bool UWorldSurvivalSubsystem::RestoreSnapshot(
    const TArray<FWorldSurvivalProfile>& ProfileSnapshot,
    const TArray<FWorldDiscoveryRecord>& DiscoverySnapshot,
    EWorldWeatherState InWeather, float InSeverity)
{
    if (!InRange(InSeverity, 0.0f, 1.0f)) return false;
    TMap<FGuid, FWorldSurvivalProfile> Proposed;
    for (const FWorldSurvivalProfile& P : ProfileSnapshot)
    {
        if (Proposed.Contains(P.EntityId) || !IsValidProfile(P)) return false;
        Proposed.Add(P.EntityId, P);
    }
    TSet<FName> SeenDiscoveries;
    for (const FWorldDiscoveryRecord& D : DiscoverySnapshot)
    {
        if (D.DiscoveryId.IsNone() || SeenDiscoveries.Contains(D.DiscoveryId) ||
            D.Location.ContainsNaN() || !FMath::IsFinite(D.DiscoveredAtTime) ||
            D.DiscoveredAtTime < 0.0) return false;
        SeenDiscoveries.Add(D.DiscoveryId);
    }
    Profiles = MoveTemp(Proposed);
    Discoveries = DiscoverySnapshot;
    Weather = InWeather;
    WeatherSeverity = InSeverity;
    return true;
}
