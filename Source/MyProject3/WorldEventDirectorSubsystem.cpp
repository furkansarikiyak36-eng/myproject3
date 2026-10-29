#include "WorldEventDirectorSubsystem.h"

#include "WorldGameDataSubsystem.h"
#include "WorldCalendar.h"
#include "WorldClimateSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
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
    PresentedSignalIds.Reset();
    PresentedActiveIds.Reset();
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
        !FMath::IsNearlyEqual(TargetTimeSeconds, Clock->GetSimulationTimeSeconds() + DeltaSeconds, 0.001) ||
        TargetTickIndex <= State.LastProcessedTickIndex ||
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
            WeatherSeverity < Definition.TriggerMinimum || WeatherSeverity > Definition.TriggerMaximum)
            return false;
        OutSeverity = WeatherSeverity;
    }
    else if (Definition.Trigger == TEXT("ground_wetness"))
    {
        if (!FMath::IsFinite(Wetness) || Wetness < 0.0f || Wetness > 1.0f)
            return false;
        if (Wetness < Definition.TriggerMinimum || Wetness > Definition.TriggerMaximum)
            return false;
        OutSeverity = Definition.Effect == TEXT("crop_moisture_loss")
            ? 1.0f - Wetness
            : Wetness;
    }
    else if (Definition.Trigger == TEXT("snow_depth"))
    {
        const float MaxDepth = FMath::Max(Climate->MaxSnowDepthCm, 1.0f);
        const float DepthRatio = FMath::Clamp(SnowDepth / MaxDepth, 0.0f, 1.0f);
        if (DepthRatio < Definition.TriggerMinimum || DepthRatio > Definition.TriggerMaximum)
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
        if (OutSeverity < Definition.TriggerMinimum || OutSeverity > Definition.TriggerMaximum)
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
        else if (Instance.Phase == EWorldNaturalEventPhase::Active &&
            TargetTimeSeconds >= Instance.EndsAtSeconds)
        {
            if (Instance.Revision >= MAX_int64 - 1)
                return false;
            Instance.Phase = EWorldNaturalEventPhase::Completed;
            ++Instance.Revision;
        }
    }

    if (!ApplyActiveEventEffects(DeltaSeconds, TargetTimeSeconds, TargetTickIndex))
        return false;

    // Retire old terminal history before admitting new events. Otherwise a
    // long-running world would eventually fill the history with completed
    // events and permanently stop emitting any new natural events.
    for (int32 Index = State.Instances.Num() - 1; Index >= 0; --Index)
    {
        if (IsTerminalEvent(State.Instances[Index].Phase))
            State.Instances.RemoveAt(Index, 1, EAllowShrinking::No);
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

bool UWorldEventDirectorSubsystem::ApplyConditionEffect(
    const FGameDataWorldEvent& Definition, const FWorldNaturalEventInstance& Instance)
{
    UWorld* World = GetWorld();
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldGameDataSubsystem* Data = GetGameData();
    if (!Survival || !Climate || !Data)
        return false;

    FName ConditionId = NAME_None;
    if (Definition.Effect == TEXT("exposure"))
        ConditionId = TEXT("Illness.Cold");
    else if (Definition.Effect == TEXT("illness_exposure"))
        ConditionId = TEXT("Illness.ToxicRain");
    else if (Definition.Effect == TEXT("illness_spread"))
        ConditionId = TEXT("Illness.Epidemic");
    else
        return true;

    TArray<FWorldSurvivalProfile> Profiles;
    TArray<FWorldDiscoveryRecord> Discoveries;
    EWorldWeatherState IgnoredWeather = EWorldWeatherState::Clear;
    float IgnoredSeverity = 0.0f;
    Survival->ExportSnapshot(Profiles, Discoveries, IgnoredWeather, IgnoredSeverity);

    for (const FWorldSurvivalProfile& Profile : Profiles)
    {
        if (!Profile.EntityId.IsValid() || Profile.Revision <= 0)
            continue;

        const bool bAlreadyHasCondition = Profile.Conditions.ContainsByPredicate(
            [&ConditionId](const FWorldSurvivalConditionState& Condition)
            { return Condition.DefinitionId == ConditionId; });
        if (bAlreadyHasCondition)
            continue;

        float Protection = FMath::Clamp(Profile.ShelterProtection, 0.0f, 1.0f);
        if (Definition.Effect == TEXT("exposure") && Protection >= 0.8f)
            continue;
        if (Definition.Effect == TEXT("illness_exposure") && Protection >= 0.75f)
            continue;
        if (Definition.Effect == TEXT("illness_spread") &&
            Profile.Conditions.ContainsByPredicate([](const FWorldSurvivalConditionState& Condition)
            {
                return Condition.Kind == EWorldSurvivalConditionKind::Illness ||
                    Condition.Kind == EWorldSurvivalConditionKind::Poisoning;
            }))
            continue;

        const float EffectiveSeverity = FMath::Clamp(
            Instance.Severity * (Definition.Effect == TEXT("illness_spread") ? 0.8f : 1.0f - Protection),
            0.1f, 1.0f);
        FGuid OperationId(
            Instance.InstanceId.A ^ Profile.EntityId.A,
            Instance.InstanceId.B ^ Profile.EntityId.B,
            Instance.InstanceId.C ^ Profile.EntityId.C,
            Instance.InstanceId.D ^ Profile.EntityId.D);
        if (!OperationId.IsValid())
            OperationId.A = 1;

        // Each profile receipt is idempotent. A profile with a full condition
        // receipt list is skipped without blocking the rest of the simulation.
        Survival->TryApplyCondition(OperationId, Profile.EntityId, Profile.Revision,
            ConditionId, EffectiveSeverity);
    }
    return true;
}

bool UWorldEventDirectorSubsystem::ApplyActiveEventEffects(
    double DeltaSeconds, double TargetTimeSeconds, int64 TargetTickIndex)
{
    UWorld* World = GetWorld();
    UWorldGameDataSubsystem* Data = GetGameData();
    UWorldAgricultureSubsystem* Agriculture =
        World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* StructuralGraph =
        World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!World || !Data || !Agriculture || !StructuralGraph)
        return false;

    for (FWorldNaturalEventInstance& Instance : State.Instances)
    {
        if (Instance.Phase != EWorldNaturalEventPhase::Active)
            continue;
        const FGameDataWorldEvent* Definition = Data->GetWorldEvents().FindByPredicate(
            [&Instance](const FGameDataWorldEvent& Event) { return Event.Id == Instance.DefinitionId; });
        if (!Definition)
            return false;

        const bool bContinuous = Definition->Effect == TEXT("crop_moisture_loss");
        if (bContinuous && Instance.LastAppliedEffectTickIndex == TargetTickIndex)
            continue;
        if (!bContinuous && Instance.bOnsetEffectApplied)
            continue;

        if (Definition->Effect == TEXT("crop_moisture_loss") ||
            Definition->Effect == TEXT("crop_damage"))
        {
            if (!Agriculture->ApplyEnvironmentalEffect(Definition->Effect, Instance.Severity,
                bContinuous ? DeltaSeconds : 1.0))
                return false;
        }
        else if (Definition->Effect == TEXT("structure_damage"))
        {
            TArray<FWorldStructuralNodeSnapshot> Nodes;
            if (!StructuralGraph->ExportSnapshot(Nodes))
                return false;
            const float Damage = 8.0f * FMath::Clamp(Instance.Severity, 0.0f, 1.0f);
            for (const FWorldStructuralNodeSnapshot& Node : Nodes)
            {
                if (!Node.NodeId.IsValid())
                    return false;
                bool bThresholdExceeded = false;
                if (!StructuralGraph->ApplyDamage(Node.NodeId, Damage, bThresholdExceeded))
                    return false;
            }
        }
        else if (Definition->Effect == TEXT("exposure") ||
                 Definition->Effect == TEXT("illness_exposure") ||
                 Definition->Effect == TEXT("illness_spread"))
        {
            if (!ApplyConditionEffect(*Definition, Instance))
                return false;
        }
        // Wetness is authored and applied by the climate subsystem itself.
        // Flood-route closures and physical fire spread are owned by the
        // logistics/fire adapters; they remain explicit effects in event data
        // until those systems expose save-safe, reversible operations.
        Instance.bOnsetEffectApplied = true;
        Instance.LastAppliedEffectTickIndex = TargetTickIndex;
        if (Instance.Revision >= MAX_int64 - 1)
            return false;
        ++Instance.Revision;
    }
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
    // Natural-event overrides are temporary. Once their final active instance
    // ends, restore the weather currently owned by the climate system instead of
    // leaving the survival subsystem stuck on stale storm/rain state.
    const UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    if (bHasOverride)
        Survival->SetWeather(OverrideWeather, OverrideSeverity);
    else if (Climate && Climate->IsClimateInitialized())
        Survival->SetWeather(Climate->GetWeather(), Climate->GetWeatherSeverity());
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

void UWorldEventDirectorSubsystem::PublishEventNotifications()
{
    if (!GEngine)
        return;

    const UWorldGameDataSubsystem* Data = GetGameData();
    TSet<FGuid> LiveSignals;
    TSet<FGuid> LiveActive;
    for (const FWorldNaturalEventInstance& Instance : State.Instances)
    {
        if (Instance.Phase != EWorldNaturalEventPhase::Signal &&
            Instance.Phase != EWorldNaturalEventPhase::Active)
            continue;
        const FGameDataWorldEvent* Definition = Data
            ? Data->GetWorldEvents().FindByPredicate([&Instance](const FGameDataWorldEvent& Event)
                { return Event.Id == Instance.DefinitionId; })
            : nullptr;
        const FString Name = Definition ? Definition->Name : Instance.DefinitionId.ToString();
        const uint32 KeyHash = FCrc::StrCrc32(*Instance.InstanceId.ToString()) & 0x3FFFFFFF;
        if (Instance.Phase == EWorldNaturalEventPhase::Signal)
        {
            LiveSignals.Add(Instance.InstanceId);
            if (!PresentedSignalIds.Contains(Instance.InstanceId))
            {
                const double Remaining = FMath::Max(0.0, Instance.StartsAtSeconds - State.LastProcessedTimeSeconds);
                const FString Message = FString::Printf(TEXT("DOĞAL OLAY UYARISI: %s (başlangıca %.0f oyun dakikası)"),
                    *Name, Remaining / 60.0);
                GEngine->AddOnScreenDebugMessage(static_cast<int32>(KeyHash), 7.0f, FColor::Orange, Message);
                PresentedSignalIds.Add(Instance.InstanceId);
            }
        }
        else
        {
            LiveActive.Add(Instance.InstanceId);
            if (!PresentedActiveIds.Contains(Instance.InstanceId))
            {
                GEngine->AddOnScreenDebugMessage(static_cast<int32>(KeyHash | 0x40000000), 7.0f,
                    FColor::Red, FString::Printf(TEXT("DOĞAL OLAY BAŞLADI: %s"), *Name));
                PresentedActiveIds.Add(Instance.InstanceId);
            }
        }
    }

    for (auto It = PresentedSignalIds.CreateIterator(); It; ++It)
        if (!LiveSignals.Contains(*It))
            It.RemoveCurrent();
    for (auto It = PresentedActiveIds.CreateIterator(); It; ++It)
        if (!LiveActive.Contains(*It))
            It.RemoveCurrent();
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
    PresentedSignalIds.Reset();
    PresentedActiveIds.Reset();
    ApplyActiveEventWeather();
    return true;
}
