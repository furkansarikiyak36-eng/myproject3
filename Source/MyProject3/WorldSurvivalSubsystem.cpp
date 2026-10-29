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
    if (!Profile.EntityId.IsValid() || Profile.Revision <= 0 ||
        !InRange(Profile.Health, 0.0f, 100.0f) || !InRange(Profile.Hunger, 0.0f, 100.0f) ||
        !InRange(Profile.Thirst, 0.0f, 100.0f) || !InRange(Profile.Stamina, 0.0f, 100.0f) ||
        !InRange(Profile.Fatigue, 0.0f, 100.0f) || !InRange(Profile.ClothingInsulation, 0.0f, 1.0f) ||
        !InRange(Profile.ShelterProtection, 0.0f, 1.0f) || !InRange(Profile.Wetness, 0.0f, 1.0f) ||
        !InRange(Profile.CaloriesKcal, 0.0f, MaxCaloriesKcal) ||
        !InRange(Profile.CoreTemperatureCelsius, 20.0f, 45.0f) ||
        !InRange(Profile.PendingActivityKcal, 0.0f, 100000.0f) ||
        !FMath::IsFinite(Profile.Location.X) || !FMath::IsFinite(Profile.Location.Y) ||
        !FMath::IsFinite(Profile.Location.Z) || !FMath::IsFinite(Profile.FeltTemperatureCelsius) ||
        Profile.Afflictions.Num() > 128)
        return false;

    TSet<FName> SeenAfflictions;
    for (const FWorldSurvivalAffliction& Affliction : Profile.Afflictions)
    {
        if (Affliction.AfflictionId.IsNone() || SeenAfflictions.Contains(Affliction.AfflictionId) ||
            !InRange(Affliction.Severity, 0.0f, 1.0f) ||
            !FMath::IsFinite(Affliction.RemainingSeconds) || Affliction.RemainingSeconds < 0.0 ||
            Affliction.RemainingSeconds > 315360000.0 ||
            !InRange(Affliction.HealthLossPerHour, 0.0f, 1000.0f) ||
            !InRange(Affliction.ThirstGainPerHour, 0.0f, 1000.0f) ||
            !InRange(Affliction.FatigueGainPerHour, 0.0f, 1000.0f))
            return false;
        SeenAfflictions.Add(Affliction.AfflictionId);
    }
    return true;
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
    Profile->bResting = false;
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
    Profile->bResting = false;
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

bool UWorldSurvivalSubsystem::SetResting(
    const FGuid& EntityId, bool bResting, int64 ExpectedRevision)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision != ExpectedRevision ||
        Profile->Revision == MAX_int64 || !IsValidProfile(*Profile) ||
        (bResting && Profile->Health <= 0.0f))
        return false;
    if (Profile->bResting == bResting) return true;
    Profile->bResting = bResting;
    ++Profile->Revision;
    return true;
}

bool UWorldSurvivalSubsystem::TryApplyAffliction(
    const FGuid& EntityId, const FWorldSurvivalAffliction& Affliction, int64 ExpectedRevision)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision != ExpectedRevision || Profile->Revision == MAX_int64 ||
        !IsValidProfile(*Profile) || Affliction.AfflictionId.IsNone() ||
        !InRange(Affliction.Severity, 0.0f, 1.0f) ||
        !FMath::IsFinite(Affliction.RemainingSeconds) || Affliction.RemainingSeconds <= 0.0 ||
        Affliction.RemainingSeconds > 315360000.0 ||
        !InRange(Affliction.HealthLossPerHour, 0.0f, 1000.0f) ||
        !InRange(Affliction.ThirstGainPerHour, 0.0f, 1000.0f) ||
        !InRange(Affliction.FatigueGainPerHour, 0.0f, 1000.0f) ||
        Profile->Afflictions.Num() >= 128 ||
        Profile->Afflictions.ContainsByPredicate([&Affliction](const FWorldSurvivalAffliction& Existing)
            { return Existing.AfflictionId == Affliction.AfflictionId; }))
        return false;

    Profile->Afflictions.Add(Affliction);
    ++Profile->Revision;
    return true;
}

bool UWorldSurvivalSubsystem::TryRemoveAffliction(
    const FGuid& EntityId, FName AfflictionId, int64 ExpectedRevision)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || AfflictionId.IsNone() || Profile->Revision != ExpectedRevision ||
        Profile->Revision == MAX_int64 || !IsValidProfile(*Profile))
        return false;
    const int32 Index = Profile->Afflictions.IndexOfByPredicate(
        [AfflictionId](const FWorldSurvivalAffliction& Existing)
            { return Existing.AfflictionId == AfflictionId; });
    if (Index == INDEX_NONE) return false;
    Profile->Afflictions.RemoveAt(Index);
    ++Profile->Revision;
    return true;
}

bool UWorldSurvivalSubsystem::TryApplyMedicalTreatment(
    const FGuid& EntityId, float HealthRestore, FName AfflictionId, int64 ExpectedRevision)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision != ExpectedRevision ||
        Profile->Revision == MAX_int64 || !IsValidProfile(*Profile) ||
        !InRange(HealthRestore, 0.0f, 100.0f) ||
        (HealthRestore <= 0.0f && AfflictionId.IsNone()))
        return false;

    int32 AfflictionIndex = INDEX_NONE;
    if (!AfflictionId.IsNone())
    {
        AfflictionIndex = Profile->Afflictions.IndexOfByPredicate(
            [AfflictionId](const FWorldSurvivalAffliction& Existing)
                { return Existing.AfflictionId == AfflictionId; });
        if (AfflictionIndex == INDEX_NONE) return false;
    }

    const float RestoredHealth = FMath::Min(100.0f, Profile->Health + HealthRestore);
    const bool bHealthChanged = RestoredHealth > Profile->Health;
    if (!bHealthChanged && AfflictionIndex == INDEX_NONE)
        return false;

    Profile->Health = RestoredHealth;
    if (AfflictionIndex != INDEX_NONE)
        Profile->Afflictions.RemoveAt(AfflictionIndex);
    ++Profile->Revision;
    return true;
}

float UWorldSurvivalSubsystem::GetWeatherTemperatureDelta() const
{
    switch (Weather)
    {
    case EWorldWeatherState::Rain:     return -RainCoolingCelsius * WeatherSeverity;
    case EWorldWeatherState::Wind:     return -WindCoolingCelsius * WeatherSeverity;
    case EWorldWeatherState::Snow:     return -SnowCoolingCelsius * WeatherSeverity;
    case EWorldWeatherState::Storm:    return -StormCoolingCelsius * WeatherSeverity;
    case EWorldWeatherState::Heatwave: return HeatwaveWarmingCelsius * WeatherSeverity;
    case EWorldWeatherState::Clear:
    default:                           return 0.0f;
    }
}

float UWorldSurvivalSubsystem::GetWindChill() const
{
    switch (Weather)
    {
    case EWorldWeatherState::Wind:  return WindChillWindCelsius * WeatherSeverity;
    case EWorldWeatherState::Storm: return WindChillStormCelsius * WeatherSeverity;
    case EWorldWeatherState::Snow:  return WindChillSnowCelsius * WeatherSeverity;
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
        !InRange(RestFatigueRecoveryPerHour, 0.0f, 100.0f) ||
        !InRange(RestStaminaRecoveryPerHour, 0.0f, 100.0f) ||
        MaxDiscoveryRecords < 1 || MaxDiscoveryRecords > 4096 ||
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
        !FMath::IsFinite(CoreRecoveryPerHour) || CoreRecoveryPerHour < 0.0f ||
        !InRange(RainCoolingCelsius, 0.0f, 60.0f) ||
        !InRange(WindCoolingCelsius, 0.0f, 60.0f) ||
        !InRange(SnowCoolingCelsius, 0.0f, 60.0f) ||
        !InRange(StormCoolingCelsius, 0.0f, 60.0f) ||
        !InRange(HeatwaveWarmingCelsius, 0.0f, 60.0f) ||
        !InRange(WindChillWindCelsius, 0.0f, 60.0f) ||
        !InRange(WindChillStormCelsius, 0.0f, 60.0f) ||
        !InRange(WindChillSnowCelsius, 0.0f, 60.0f) ||
        !InRange(RainWetnessPerHour, 0.0f, 1.0f) ||
        !InRange(StormWetnessPerHour, 0.0f, 1.0f) ||
        !InRange(BaseDryingPerHour, 0.0f, 1.0f) ||
        !InRange(ShelterDryingBonusPerHour, 0.0f, 1.0f) ||
        !InRange(WarmthDryingPerDegree, 0.0f, 1.0f) ||
        !InRange(HeatThirstPerDegreeHour, 0.0f, 10.0f) ||
        !InRange(HeatThirstThresholdCelsius, -20.0f, 60.0f))
        return false;
    for (const TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
        if (Pair.Value.Revision == MAX_int64 || !IsValidProfile(Pair.Value)) return false;

    // Heat fuel is mutated in this step too; every source must pass preflight before
    // the first profile or fuel value is changed.
    for (const TWeakObjectPtr<UHeatSourceComponent>& Source : HeatSources)
        if (Source.IsValid() && !Source->CanAdvanceFuel(DeltaSeconds, Weather, WeatherSeverity))
            return false;
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
        P.Fatigue = P.bResting
            ? FMath::Max(0.0f, P.Fatigue - RestFatigueRecoveryPerHour * Hours)
            : FMath::Clamp(P.Fatigue + FatiguePerHour * Hours, 0.0f, 100.0f);

        // Afflictions use logical time and per-hour rates, not rendered frames.
        for (FWorldSurvivalAffliction& Affliction : P.Afflictions)
        {
            const double ActiveSeconds = FMath::Min(DeltaSeconds, Affliction.RemainingSeconds);
            const float ActiveHours = static_cast<float>(ActiveSeconds / 3600.0);
            const float Rate = Affliction.Severity * ActiveHours;
            P.Health = FMath::Max(0.0f, P.Health - Affliction.HealthLossPerHour * Rate);
            P.Thirst = FMath::Clamp(P.Thirst + Affliction.ThirstGainPerHour * Rate, 0.0f, 100.0f);
            P.Fatigue = FMath::Clamp(P.Fatigue + Affliction.FatigueGainPerHour * Rate, 0.0f, 100.0f);
            Affliction.RemainingSeconds = FMath::Max(0.0, Affliction.RemainingSeconds - DeltaSeconds);
        }
        P.Afflictions.RemoveAll([](const FWorldSurvivalAffliction& Affliction)
            { return Affliction.RemainingSeconds <= 0.0; });

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
        if (P.FeltTemperatureCelsius > HeatThirstThresholdCelsius)
            P.Thirst = FMath::Clamp(P.Thirst +
                (P.FeltTemperatureCelsius - HeatThirstThresholdCelsius) * HeatThirstPerDegreeHour * Hours,
                0.0f, 100.0f);

        // Calories: basal metabolism, reported activity and shivering.
        const float BurnKcal = BasalKcalPerHour * Hours + P.PendingActivityKcal +
            ColdDegrees * ShiverKcalPerDegreeHour * Hours;
        P.PendingActivityKcal = 0.0f;
        P.CaloriesKcal = FMath::Clamp(P.CaloriesKcal - BurnKcal, 0.0f, MaxCaloriesKcal);
        P.Hunger = 100.0f * (1.0f - P.CaloriesKcal / MaxCaloriesKcal);

        if (Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm)
        {
            const float WetnessRate = Weather == EWorldWeatherState::Rain
                ? RainWetnessPerHour : StormWetnessPerHour;
            P.Wetness = FMath::Clamp(P.Wetness + WeatherSeverity *
                (1.0f - P.ShelterProtection) * Hours * WetnessRate, 0.0f, 1.0f);
        }
        // Warmth dries clothing faster; all rates are in the authored per-hour units.
        const float DryingBoost = FMath::Max(0.0f,
            P.FeltTemperatureCelsius - ComfortTemperatureCelsius) * WarmthDryingPerDegree;
        P.Wetness = FMath::Max(0.0f, P.Wetness - Hours *
            (BaseDryingPerHour + P.ShelterProtection * ShelterDryingBonusPerHour + DryingBoost));

        if (P.CaloriesKcal <= 0.0f || P.Thirst >= 98.0f)
            P.Health = FMath::Max(0.0f, P.Health - Hours * 2.0f);
        const float StaminaDelta = P.bResting
            ? RestStaminaRecoveryPerHour * Hours
            : (P.Fatigue < 50.0f ? 2.0f : -2.0f) * Hours;
        P.Stamina = FMath::Clamp(P.Stamina + StaminaDelta, 0.0f, 100.0f);
        ++P.Revision;
    }
    return true;
}

bool UWorldSurvivalSubsystem::RecordDiscovery(
    FName DiscoveryId, FVector Location, double LogicalTimeSeconds)
{
    if (DiscoveryId.IsNone() || !FMath::IsFinite(Location.X) ||
        !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z) ||
        !FMath::IsFinite(LogicalTimeSeconds) || LogicalTimeSeconds < 0.0 ||
        MaxDiscoveryRecords < 1 || MaxDiscoveryRecords > 4096)
    {
        return false;
    }
    if (Discoveries.ContainsByPredicate([DiscoveryId](const FWorldDiscoveryRecord& D)
        { return D.DiscoveryId == DiscoveryId; }))
    {
        return true; // discovery is idempotent
    }
    if (Discoveries.Num() >= MaxDiscoveryRecords) return false;
    FWorldDiscoveryRecord Record;
    Record.DiscoveryId = DiscoveryId;
    Record.Location = Location;
    Record.DiscoveredAtTime = LogicalTimeSeconds;
    Discoveries.Add(Record);
    return true;
}

bool UWorldSurvivalSubsystem::TryDiscover(
    FName DiscoveryId, FVector PlayerLocation, FVector DiscoveryLocation,
    double LogicalTimeSeconds, const TArray<FName>& AvailableEquipmentTags,
    TArray<FName>& OutUnlockedContentIds)
{
    OutUnlockedContentIds.Reset();
    if (DiscoveryId.IsNone()) return false;

    // A committed discovery always replays its saved unlock result, independent
    // of subsequent tuning/gear changes; it never grants a second reward.
    const FWorldDiscoveryRecord* ExistingRecord = Discoveries.FindByPredicate(
        [DiscoveryId](const FWorldDiscoveryRecord& Record) { return Record.DiscoveryId == DiscoveryId; });
    if (ExistingRecord)
    {
        OutUnlockedContentIds = ExistingRecord->UnlockedContentIds;
        return true;
    }

    const FWorldDiscoveryDefinition* Definition = nullptr;
    for (const FWorldDiscoveryDefinition& Candidate : DiscoveryDefinitions)
    {
        if (Candidate.DiscoveryId != DiscoveryId) continue;
        if (Definition) return false; // ambiguous authored discovery IDs fail closed
        Definition = &Candidate;
    }
    if (!Definition || !FMath::IsFinite(PlayerLocation.X) ||
        !FMath::IsFinite(PlayerLocation.Y) || !FMath::IsFinite(PlayerLocation.Z) ||
        !FMath::IsFinite(DiscoveryLocation.X) || !FMath::IsFinite(DiscoveryLocation.Y) ||
        !FMath::IsFinite(DiscoveryLocation.Z) ||
        !FMath::IsFinite(LogicalTimeSeconds) || LogicalTimeSeconds < 0.0 ||
        !InRange(Definition->InteractionRadiusCm, 10.0f, 10000.0f) ||
        Discoveries.Num() >= MaxDiscoveryRecords)
        return false;

    const double DistanceSquared = FVector::DistSquared(PlayerLocation, DiscoveryLocation);
    const double RadiusSquared = FMath::Square(static_cast<double>(Definition->InteractionRadiusCm));
    if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > RadiusSquared)
        return false;
    if (!Definition->RequiredEquipmentTag.IsNone() &&
        !AvailableEquipmentTags.Contains(Definition->RequiredEquipmentTag))
        return false;

    TSet<FName> UniqueUnlocks;
    for (const FName UnlockId : Definition->UnlockedContentIds)
    {
        if (UnlockId.IsNone() || UniqueUnlocks.Contains(UnlockId)) return false;
        UniqueUnlocks.Add(UnlockId);
    }

    FWorldDiscoveryRecord Record;
    Record.DiscoveryId = DiscoveryId;
    Record.Location = DiscoveryLocation;
    Record.DiscoveredAtTime = LogicalTimeSeconds;
    Record.UnlockedContentIds = Definition->UnlockedContentIds;
    Discoveries.Add(Record);
    OutUnlockedContentIds = Record.UnlockedContentIds;
    return true;
}

bool UWorldSurvivalSubsystem::IsContentUnlocked(FName ContentId) const
{
    if (ContentId.IsNone()) return false;
    for (const FWorldDiscoveryRecord& Record : Discoveries)
        if (Record.UnlockedContentIds.Contains(ContentId)) return true;
    return false;
}

bool UWorldSurvivalSubsystem::GetProfile(
    const FGuid& EntityId, FWorldSurvivalProfile& OutProfile) constbool UWorldSurvivalSubsystem::GetProfile(
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
    if (!InRange(InSeverity, 0.0f, 1.0f) || MaxDiscoveryRecords < 1 ||
        MaxDiscoveryRecords > 4096 || DiscoverySnapshot.Num() > MaxDiscoveryRecords)
        return false;
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
            !FMath::IsFinite(D.Location.X) || !FMath::IsFinite(D.Location.Y) ||
            !FMath::IsFinite(D.Location.Z) || !FMath::IsFinite(D.DiscoveredAtTime) ||
            D.DiscoveredAtTime < 0.0 || D.UnlockedContentIds.Contains(NAME_None))
            return false;
        TSet<FName> UniqueUnlocks;
        for (const FName UnlockId : D.UnlockedContentIds)
        {
            if (UniqueUnlocks.Contains(UnlockId)) return false;
            UniqueUnlocks.Add(UnlockId);
        }
        SeenDiscoveries.Add(D.DiscoveryId);
    }
    Profiles = MoveTemp(Proposed);
    Discoveries = DiscoverySnapshot;
    Weather = InWeather;
    WeatherSeverity = InSeverity;
    return true;
}


void UWorldSurvivalSubsystem::ExportHeatSourceRuntimeSnapshot(
    TArray<FWorldHeatSourceRuntimeSnapshot>& OutSnapshot) const
{
    OutSnapshot.Reset();
    for (const TWeakObjectPtr<UHeatSourceComponent>& Source : HeatSources)
    {
        if (!Source.IsValid())
            continue;
        FWorldHeatSourceRuntimeSnapshot& State = OutSnapshot.AddDefaulted_GetRef();
        State.Source = Source;
        State.FuelSeconds = Source->GetFuelSeconds();
        State.bLit = Source->IsLit();
    }
}

bool UWorldSurvivalSubsystem::RestoreHeatSourceRuntimeSnapshot(
    const TArray<FWorldHeatSourceRuntimeSnapshot>& Snapshot)
{
    for (const FWorldHeatSourceRuntimeSnapshot& State : Snapshot)
    {
        UHeatSourceComponent* Source = State.Source.Get();
        if (!IsValid(Source) || !Source->RestoreRuntimeState(State.FuelSeconds, State.bLit))
            return false;
    }
    return true;
}
