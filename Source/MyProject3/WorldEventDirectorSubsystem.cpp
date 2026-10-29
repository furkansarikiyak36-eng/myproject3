#include "WorldEventDirectorSubsystem.h"

#include "WorldGameDataSubsystem.h"
#include "WorldCalendar.h"
#include "WorldClimateSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "Engine/World.h"
#include "Misc/Crc.h"

namespace
{
    bool IsTerminalEvent(EWorldNaturalEventPhase Phase)
    {
        return Phase == EWorldNaturalEventPhase::Completed || Phase == EWorldNaturalEventPhase::Cancelled;
    }

    bool IsWeatherTrigger(FName Trigger, EWorldWeatherState Weather, float TemperatureCelsius)
    {
        if (Trigger == TEXT("weather.snow"))
            return Weather == EWorldWeatherState::Snow ||
                (Weather == EWorldWeatherState::Storm && TemperatureCelsius <= 1.0f);
        if (Trigger == TEXT("weather.rain"))
            return Weather == EWorldWeatherState::Rain ||
                (Weather == EWorldWeatherState::Storm && TemperatureCelsius > 1.0f);
        if (Trigger == TEXT("weather.storm"))
            return Weather == EWorldWeatherState::Storm;
        if (Trigger == TEXT("weather.heatwave"))
            return Weather == EWorldWeatherState::Heatwave;
        return false;
    }
}

UWorldGameDataSubsystem* UWorldEventDirectorSubsystem::GetGameData() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
}

bool UWorldEventDirectorSubsystem::InitializeEventDirector()
{
    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldGameDataSubsystem* Data = GetGameData();
    if (!World || !Clock || !Climate || !Data || !Clock->IsClockInitialized() ||
        !Climate->IsClimateInitialized() || MaxEventInstances < 1 || MaxEventInstances > 512 ||
        MaxNewSignalsPerStep < 1 || MaxNewSignalsPerStep > 128)
    {
        return false;
    }
    if ((!Data->IsLoaded() && !Data->LoadDefault()) ||
        (!Data->IsApplied() && !Data->ApplyToWorld()) ||
        Data->GetWorldEvents().IsEmpty())
    {
        return false;
    }

    FWorldEventDirectorSnapshot Fresh;
    Fresh.SchemaVersion = 1;
    Fresh.bInitialized = true;
    Fresh.Serial = 0;
    Fresh.LastProcessedTickIndex = Clock->GetTickIndex();
    Fresh.LastProcessedTimeSeconds = Clock->GetSimulationTimeSeconds();
    Fresh.PreviousSnowDepthCm = Climate->GetSnowDepthCm();
    State = MoveTemp(Fresh);
    return true;
}

bool UWorldEventDirectorSubsystem::CanAdvanceEventStep(
    double DeltaSeconds, double TargetTimeSeconds, int64 TargetTickIndex) const
{
    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldGameDataSubsystem* Data = GetGameData();
    if (!State.bInitialized || !Clock || !Clock->IsClockInitialized() ||
        !Climate || !Climate->IsClimateInitialized() || !Data || !Data->IsLoaded() ||
        Data->GetWorldEvents().IsEmpty() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 ||
        !FMath::IsFinite(TargetTimeSeconds) || TargetTimeSeconds < State.LastProcessedTimeSeconds ||
        !FMath::IsFinite(Clock->GetSimulationTimeSeconds() + DeltaSeconds) ||
        TargetTickIndex < State.LastProcessedTickIndex ||
        State.Serial < 0 || State.Serial >= MAX_int32 - 1 ||
        MaxEventInstances < 1 || MaxEventInstances > 512 ||
        State.Instances.Num() > MaxEventInstances)
    {
        return false;
    }
    for (const FWorldNaturalEventInstance& Instance : State.Instances)
    {
        if (!Instance.InstanceId.IsValid() || Instance.DefinitionId.IsNone() ||
            !FMath::IsFinite(Instance.SignalStartedAtSeconds) ||
            !FMath::IsFinite(Instance.StartsAtSeconds) ||
            !FMath::IsFinite(Instance.EndsAtSeconds) ||
            Instance.SignalStartedAtSeconds < 0.0 ||
            Instance.StartsAtSeconds < Instance.SignalStartedAtSeconds ||
            Instance.EndsAtSeconds <= Instance.StartsAtSeconds ||
            !FMath::IsFinite(Instance.Severity) || Instance.Severity < 0.0f ||
            Instance.Severity > 1.0f || Instance.Revision <= 0)
        {
            return false;
        }
    }
    return true;
}

bool UWorldEventDirectorSubsystem::EvaluateTrigger(
    const FGameDataWorldEvent& Definition, float& OutSeverity) const
{
    OutSeverity = 0.0f;
    UWorld* World = GetWorld();
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!Climate || !Climate->IsClimateInitialized())
        return false;

    const EWorldWeatherState Weather = Climate->GetWeather();
    const float WeatherSeverity = Climate->GetWeatherSeverity();
    const float Wetness = Climate->GetGroundWetness();
    const float SnowDepth = Climate->GetSnowDepthCm();

    if (Definition.Trigger == TEXT("weather.snow") ||
        Definition.Trigger == TEXT("weather.rain") ||
        Definition.Trigger == TEXT("weather.storm") ||
        Definition.Trigger == TEXT("weather.heatwave"))
    {
        if (!IsWeatherTrigger(Definition.Trigger, Weather, Climate->GetTemperatureCelsius()) ||
            WeatherSeverity < Definition.MinimumSeverity)
            return false;
        OutSeverity = WeatherSeverity;
    }
    else if (Definition.Trigger == TEXT("ground_wetness"))
    {
        if (!FMath::IsFinite(Wetness) || Wetness < 0.0f || Wetness > 1.0f)
            return false;
        if (Definition.Effect == TEXT("crop_moisture_loss"))
        {
            if (Wetness > Definition.MaximumSeverity)
                return false;
            OutSeverity = 1.0f - Wetness;
        }
        else
        {
            if (Wetness < Definition.MinimumSeverity)
                return false;
            OutSeverity = Wetness;
        }
    }
    else if (Definition.Trigger == TEXT("snow_depth"))
    {
        const float MaxDepth = FMath::Max(Climate->MaxSnowDepthCm, 1.0f);
        const float DepthRatio = FMath::Clamp(SnowDepth / MaxDepth, 0.0f, 1.0f);
        if (DepthRatio < Definition.MinimumSeverity)
            return false;
        OutSeverity = DepthRatio;
    }
    else if (Definition.Trigger == TEXT("world.condition_pressure"))
    {
        if (!Survival)
            return false;
        TArray<FWorldSurvivalProfile> Profiles;
        TArray<FWorldDiscoveryRecord> Discoveries;
        EWorldWeatherState IgnoredWeather = EWorldWeatherState::Clear;
        float IgnoredSeverity = 0.0f;
        Survival->ExportSnapshot(Profiles, Discoveries, IgnoredWeather, IgnoredSeverity);
        if (Profiles.IsEmpty())
            return false;
        int32 Affected = 0;
        for (const FWorldSurvivalProfile& Profile : Profiles)
        {
            if (Profile.Conditions.ContainsByPredicate([](const FWorldSurvivalConditionState& Condition)
                {
                    return Condition.Kind == EWorldSurvivalConditionKind::Illness ||
                        Condition.Kind == EWorldSurvivalConditionKind::Poisoning;
                }))
                ++Affected;
        }
        OutSeverity = static_cast<float>(Affected) / static_cast<float>(Profiles.Num());
        if (OutSeverity < Definition.MinimumSeverity)
            return false;
    }
    else
    {
        return false;
    }

    OutSeverity = FMath::Clamp(OutSeverity, Definition.MinimumSeverity, Definition.MaximumSeverity);
    return FMath::IsFinite(OutSeverity);
}

bool UWorldEventDirectorSubsystem::AdvanceEventStep(
    double DeltaSeconds, double TargetTimeSeconds, int64 TargetTickIndex)
{
    if (!CanAdvanceEventStep(DeltaSeconds, TargetTimeSeconds, TargetTickIndex))
        return false;

    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldGameDataSubsystem* Data = GetGameData();
    FWorldCalendarDate TargetDate;
    if (!Clock || !Climate || !Data ||
        !WorldCalendar::DateFromTime(TargetTimeSeconds, Clock->CalendarConfig, TargetDate))
        return false;

    // Advance the existing signal/effect lifecycle before evaluating new signals.
    for (FWorldNaturalEventInstance& Instance : State.Instances)
    {
        if (Instance.Phase == EWorldNaturalEventPhase::Signal &&
            TargetTimeSeconds >= Instance.StartsAtSeconds)
        {
            if (Instance.Revision >= MAX_int64 - 1)
                return false;
            Instance.Phase = EWorldNaturalEventPhase::Active;
            ++Instance.Revision;
        }
        if (Instance.Phase == EWorldNaturalEventPhase::Active &&
            TargetTimeSeconds >= Instance.EndsAtSeconds)
        {
            if (Instance.Revision >= MAX_int64 - 1)
                return false;
            Instance.Phase = EWorldNaturalEventPhase::Completed;
            ++Instance.Revision;
        }
    }

    int32 NewSignals = 0;
    for (const FGameDataWorldEvent& Definition : Data->GetWorldEvents())
    {
        if (NewSignals >= MaxNewSignalsPerStep || State.Instances.Num() >= MaxEventInstances)
            break;
        const int64* LastDay = State.LastTriggeredDayByEvent.Find(Definition.Id);
        if (LastDay && *LastDay == TargetDate.DaysElapsed)
            continue;

        float Severity = 0.0f;
        if (!EvaluateTrigger(Definition, Severity))
            continue;

        const int64 NewSerial = State.Serial + 1;
        if (NewSerial <= 0 || NewSerial >= MAX_int32)
            return false;
        const int32 Seed = Clock->GetRandomSeed();
        const uint32 DefinitionHash = FCrc::StrCrc32(*Definition.Id.ToString());
        FWorldNaturalEventInstance& Instance = State.Instances.AddDefaulted_GetRef();
        Instance.InstanceId = FGuid(Seed, static_cast<int32>(NewSerial),
            static_cast<int32>(TargetTickIndex), static_cast<int32>(DefinitionHash ^ static_cast<uint32>(NewSerial)));
        if (!Instance.InstanceId.IsValid())
            return false;
        Instance.DefinitionId = Definition.Id;
        Instance.Phase = EWorldNaturalEventPhase::Signal;
        Instance.SignalStartedAtSeconds = TargetTimeSeconds;
        Instance.StartsAtSeconds = TargetTimeSeconds + Definition.WarningSeconds;
        Instance.EndsAtSeconds = Instance.StartsAtSeconds + Definition.DurationSeconds;
        Instance.Severity = Severity;
        Instance.TriggerDayIndex = TargetDate.DaysElapsed;
        Instance.Revision = 1;
        State.Serial = NewSerial;
        State.LastTriggeredDayByEvent.Add(Definition.Id, TargetDate.DaysElapsed);
        ++NewSignals;
    }

    // Keep a bounded history without discarding a signal or active event.
    while (State.Instances.Num() > MaxEventInstances)
    {
        const int32 RemoveIndex = State.Instances.IndexOfByPredicate(
            [](const FWorldNaturalEventInstance& I) { return IsTerminalEvent(I.Phase); });
        if (RemoveIndex == INDEX_NONE)
            return false;
        State.Instances.RemoveAt(RemoveIndex, 1, EAllowShrinking::No);
    }
    ApplyActiveEventWeather();
    State.LastProcessedTickIndex = TargetTickIndex;
    State.LastProcessedTimeSeconds = TargetTimeSeconds;
    State.PreviousSnowDepthCm = Climate->GetSnowDepthCm();
    return true;
}

void UWorldEventDirectorSubsystem::ApplyActiveEventWeather() const
{
    UWorld* World = GetWorld();
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldGameDataSubsystem* Data = GetGameData();
    if (!Survival || !Data)
        return;

    bool bHasOverride = false;
    EWorldWeatherState OverrideWeather = EWorldWeatherState::Clear;
    float OverrideSeverity = 0.0f;
    for (const FWorldNaturalEventInstance& Instance : State.Instances)
    {
        if (Instance.Phase != EWorldNaturalEventPhase::Active)
            continue;
        const FGameDataWorldEvent* Definition = Data->GetWorldEvents().FindByPredicate(
            [&Instance](const FGameDataWorldEvent& Event) { return Event.Id == Instance.DefinitionId; });
        if (!Definition)
            continue;

        EWorldWeatherState CandidateWeather = EWorldWeatherState::Clear;
        bool bCandidate = true;
        if (Definition->Trigger == TEXT("weather.snow"))
            CandidateWeather = EWorldWeatherState::Snow;
        else if (Definition->Trigger == TEXT("weather.rain"))
            CandidateWeather = EWorldWeatherState::Rain;
        else if (Definition->Trigger == TEXT("weather.storm"))
            CandidateWeather = EWorldWeatherState::Storm;
        else if (Definition->Trigger == TEXT("weather.heatwave"))
            CandidateWeather = EWorldWeatherState::Heatwave;
        else
            bCandidate = false;

        if (bCandidate && (!bHasOverride || Instance.Severity > OverrideSeverity))
        {
            bHasOverride = true;
            OverrideWeather = CandidateWeather;
            OverrideSeverity = Instance.Severity;
        }
    }
    if (bHasOverride)
        Survival->SetWeather(OverrideWeather, OverrideSeverity);
}

TArray<FWorldNaturalEventInstance> UWorldEventDirectorSubsystem::GetActiveNaturalEvents() const
{
    TArray<FWorldNaturalEventInstance> Result;
    for (const FWorldNaturalEventInstance& Instance : State.Instances)
        if (Instance.Phase == EWorldNaturalEventPhase::Signal ||
            Instance.Phase == EWorldNaturalEventPhase::Active)
            Result.Add(Instance);
    return Result;
}

FString UWorldEventDirectorSubsystem::GetEventStatusText() const
{
    const UWorldGameDataSubsystem* Data = GetGameData();
    FString Result;
    for (const FWorldNaturalEventInstance& Instance : State.Instances)
    {
        if (Instance.Phase != EWorldNaturalEventPhase::Signal &&
            Instance.Phase != EWorldNaturalEventPhase::Active)
            continue;
        const FGameDataWorldEvent* Definition = Data
            ? Data->GetWorldEvents().FindByPredicate([&Instance](const FGameDataWorldEvent& Event)
                { return Event.Id == Instance.DefinitionId; })
            : nullptr;
        if (!Result.IsEmpty())
            Result += TEXT(" | ");
        Result += Definition ? Definition->Name : Instance.DefinitionId.ToString();
        Result += Instance.Phase == EWorldNaturalEventPhase::Signal ? TEXT(" (uyarı)") : TEXT(" (aktif)");
    }
    return Result;
}

bool UWorldEventDirectorSubsystem::IsValidSnapshot(const FWorldEventDirectorSnapshot& Snapshot) const
{
    if (Snapshot.SchemaVersion != 1 || Snapshot.Serial < 0 ||
        Snapshot.LastProcessedTickIndex < -1 ||
        !FMath::IsFinite(Snapshot.LastProcessedTimeSeconds) ||
        Snapshot.LastProcessedTimeSeconds < 0.0 ||
        !FMath::IsFinite(Snapshot.PreviousSnowDepthCm) || Snapshot.PreviousSnowDepthCm < 0.0f ||
        Snapshot.Instances.Num() > MaxEventInstances ||
        Snapshot.LastTriggeredDayByEvent.Num() > 256)
        return false;

    TSet<FGuid> SeenIds;
    for (const FWorldNaturalEventInstance& Instance : Snapshot.Instances)
    {
        if (!Instance.InstanceId.IsValid() || SeenIds.Contains(Instance.InstanceId) ||
            Instance.DefinitionId.IsNone() ||
            static_cast<uint8>(Instance.Phase) > static_cast<uint8>(EWorldNaturalEventPhase::Cancelled) ||
            !FMath::IsFinite(Instance.SignalStartedAtSeconds) ||
            !FMath::IsFinite(Instance.StartsAtSeconds) || !FMath::IsFinite(Instance.EndsAtSeconds) ||
            Instance.SignalStartedAtSeconds < 0.0 || Instance.StartsAtSeconds < Instance.SignalStartedAtSeconds ||
            Instance.EndsAtSeconds <= Instance.StartsAtSeconds || !FMath::IsFinite(Instance.Severity) ||
            Instance.Severity < 0.0f || Instance.Severity > 1.0f ||
            Instance.TriggerDayIndex < 0 || Instance.Revision <= 0 ||
            Instance.Revision >= MAX_int64)
            return false;
        SeenIds.Add(Instance.InstanceId);
    }
    for (const TPair<FName, int64>& Pair : Snapshot.LastTriggeredDayByEvent)
        if (Pair.Key.IsNone() || Pair.Value < 0)
            return false;
    return true;
}

bool UWorldEventDirectorSubsystem::RestoreSnapshot(const FWorldEventDirectorSnapshot& Snapshot)
{
    // A v27/legacy save has no event manifest. Start a new deterministic event
    // history from its restored clock instead of freezing the simulation.
    if (!Snapshot.bInitialized)
        return InitializeEventDirector();
    if (!IsValidSnapshot(Snapshot))
        return false;

    const UWorldGameDataSubsystem* Data = GetGameData();
    if (!Data || !Data->IsLoaded())
        return false;
    TSet<FName> DefinitionIds;
    for (const FGameDataWorldEvent& Definition : Data->GetWorldEvents())
        DefinitionIds.Add(Definition.Id);
    for (const FWorldNaturalEventInstance& Instance : Snapshot.Instances)
        if (!DefinitionIds.Contains(Instance.DefinitionId))
            return false;
    for (const TPair<FName, int64>& Pair : Snapshot.LastTriggeredDayByEvent)
        if (!DefinitionIds.Contains(Pair.Key))
            return false;

    State = Snapshot;
    ApplyActiveEventWeather();
    return true;
}
