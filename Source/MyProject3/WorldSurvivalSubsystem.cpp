#include "WorldSurvivalSubsystem.h"

bool UWorldSurvivalSubsystem::RegisterProfile(const FWorldSurvivalProfile& Profile)
{
    if (!Profile.EntityId.IsValid() || Profile.Revision <= 0 || Profiles.Contains(Profile.EntityId) ||
        !FMath::IsFinite(Profile.Health) || Profile.Health < 0.0f || Profile.Health > 100.0f ||
        !FMath::IsFinite(Profile.Hunger) || Profile.Hunger < 0.0f || Profile.Hunger > 100.0f ||
        !FMath::IsFinite(Profile.Thirst) || Profile.Thirst < 0.0f || Profile.Thirst > 100.0f ||
        !FMath::IsFinite(Profile.Stamina) || Profile.Stamina < 0.0f || Profile.Stamina > 100.0f ||
        !FMath::IsFinite(Profile.Fatigue) || Profile.Fatigue < 0.0f || Profile.Fatigue > 100.0f ||
        !FMath::IsFinite(Profile.ClothingInsulation) || Profile.ClothingInsulation < 0.0f || Profile.ClothingInsulation > 1.0f ||
        !FMath::IsFinite(Profile.ShelterProtection) || Profile.ShelterProtection < 0.0f || Profile.ShelterProtection > 1.0f ||
        !FMath::IsFinite(Profile.Wetness) || Profile.Wetness < 0.0f || Profile.Wetness > 1.0f)
    {
        return false;
    }
    Profiles.Add(Profile.EntityId, Profile);
    return true;
}

bool UWorldSurvivalSubsystem::UpdateProtection(
    const FGuid& EntityId, float ClothingInsulation, float ShelterProtection, int64 ExpectedRevision)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision != ExpectedRevision || Profile->Revision == MAX_int64 ||
        !FMath::IsFinite(ClothingInsulation) || ClothingInsulation < 0.0f || ClothingInsulation > 1.0f ||
        !FMath::IsFinite(ShelterProtection) || ShelterProtection < 0.0f || ShelterProtection > 1.0f)
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
    if (!FMath::IsFinite(Severity) || Severity < 0.0f || Severity > 1.0f) return false;
    Weather = NewWeather;
    WeatherSeverity = Severity;
    return true;
}

bool UWorldSurvivalSubsystem::CanAdvanceSurvivalStep(
    double DeltaSeconds, double LogicalTimeSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        !FMath::IsFinite(LogicalTimeSeconds) || LogicalTimeSeconds < 0.0 ||
        !FMath::IsFinite(WeatherSeverity) || WeatherSeverity < 0.0f || WeatherSeverity > 1.0f ||
        !FMath::IsFinite(HungerPerHour) || HungerPerHour < 0.0f ||
        !FMath::IsFinite(ThirstPerHour) || ThirstPerHour < 0.0f ||
        !FMath::IsFinite(FatiguePerHour) || FatiguePerHour < 0.0f ||
        !FMath::IsFinite(ColdDamagePerHour) || ColdDamagePerHour < 0.0f)
        return false;
    for (const TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
        if (Pair.Value.Revision == MAX_int64) return false;
    return true;
}

bool UWorldSurvivalSubsystem::AdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds)
{
    if (!CanAdvanceSurvivalStep(DeltaSeconds, LogicalTimeSeconds)) return false;

    const float Hours = static_cast<float>(DeltaSeconds / 3600.0);
    for (TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
    {
        FWorldSurvivalProfile& P = Pair.Value;
        P.Hunger = FMath::Clamp(P.Hunger + HungerPerHour * Hours, 0.0f, 100.0f);
        P.Thirst = FMath::Clamp(P.Thirst + ThirstPerHour * Hours, 0.0f, 100.0f);
        P.Fatigue = FMath::Clamp(P.Fatigue + FatiguePerHour * Hours, 0.0f, 100.0f);

        const bool bColdWeather = Weather == EWorldWeatherState::Snow ||
            Weather == EWorldWeatherState::Storm || Weather == EWorldWeatherState::Wind ||
            Weather == EWorldWeatherState::Rain;
        const bool bHeatWeather = Weather == EWorldWeatherState::Heatwave;
        if (bColdWeather)
        {
            const float Exposure = WeatherSeverity * (1.0f - P.ClothingInsulation) *
                (1.0f - P.ShelterProtection) * (1.0f - P.Wetness * 0.25f);
            P.Health = FMath::Max(0.0f, P.Health - ColdDamagePerHour * Hours * Exposure);
            if (Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm)
                P.Wetness = FMath::Clamp(P.Wetness + WeatherSeverity * Hours * 0.2f, 0.0f, 1.0f);
        }
        if (bHeatWeather)
        {
            const float Exposure = WeatherSeverity * (1.0f - P.ShelterProtection);
            P.Thirst = FMath::Clamp(P.Thirst + Exposure * Hours * 2.0f, 0.0f, 100.0f);
        }
        if (P.Hunger >= 95.0f || P.Thirst >= 98.0f)
            P.Health = FMath::Max(0.0f, P.Health - Hours * 2.0f);
        P.Stamina = FMath::Clamp(P.Stamina + (P.Fatigue < 50.0f ? 2.0f : -2.0f) * Hours, 0.0f, 100.0f);
        P.Wetness = FMath::Max(0.0f, P.Wetness - Hours * (0.05f + P.ShelterProtection * 0.25f));
        ++P.Revision;
    }
    return true;
}

bool UWorldSurvivalSubsystem::RecordDiscovery(
    FName DiscoveryId, FVector Location, double LogicalTimeSeconds)
{
    if (DiscoveryId.IsNone() || !FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) ||
        !FMath::IsFinite(Location.Z) || !FMath::IsFinite(LogicalTimeSeconds) ||
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
    if (!FMath::IsFinite(InSeverity) || InSeverity < 0.0f || InSeverity > 1.0f) return false;
    TMap<FGuid, FWorldSurvivalProfile> Proposed;
    for (const FWorldSurvivalProfile& P : ProfileSnapshot)
    {
        if (!P.EntityId.IsValid() || P.Revision <= 0 || Proposed.Contains(P.EntityId) ||
            !FMath::IsFinite(P.Health) || P.Health < 0.0f || P.Health > 100.0f ||
            !FMath::IsFinite(P.Hunger) || P.Hunger < 0.0f || P.Hunger > 100.0f ||
            !FMath::IsFinite(P.Thirst) || P.Thirst < 0.0f || P.Thirst > 100.0f ||
            !FMath::IsFinite(P.Stamina) || P.Stamina < 0.0f || P.Stamina > 100.0f ||
            !FMath::IsFinite(P.Fatigue) || P.Fatigue < 0.0f || P.Fatigue > 100.0f ||
            !FMath::IsFinite(P.ClothingInsulation) || P.ClothingInsulation < 0.0f || P.ClothingInsulation > 1.0f ||
            !FMath::IsFinite(P.ShelterProtection) || P.ShelterProtection < 0.0f || P.ShelterProtection > 1.0f ||
            !FMath::IsFinite(P.Wetness) || P.Wetness < 0.0f || P.Wetness > 1.0f) return false;
        Proposed.Add(P.EntityId, P);
    }
    TSet<FName> SeenDiscoveries;
    for (const FWorldDiscoveryRecord& D : DiscoverySnapshot)
    {
        if (D.DiscoveryId.IsNone() || SeenDiscoveries.Contains(D.DiscoveryId) ||
            !FMath::IsFinite(D.Location.X) || !FMath::IsFinite(D.Location.Y) ||
            !FMath::IsFinite(D.Location.Z) || !FMath::IsFinite(D.DiscoveredAtTime) ||
            D.DiscoveredAtTime < 0.0) return false;
        SeenDiscoveries.Add(D.DiscoveryId);
    }
    Profiles = MoveTemp(Proposed);
    Discoveries = DiscoverySnapshot;
    Weather = InWeather;
    WeatherSeverity = InSeverity;
    return true;
}
