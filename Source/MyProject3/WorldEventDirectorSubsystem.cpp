#include "WorldEventDirectorSubsystem.h"
#include "Engine/World.h"
#include "WorldCalendar.h"
#include "WorldSimulationClockSubsystem.h"

namespace
{
    uint64 StableHash(const FString& Text, uint32 Seed, int64 Serial, uint32 Salt)
    {
        uint64 Hash = 1469598103934665603ull;
        for (const TCHAR* It = *Text; *It; ++It) { Hash ^= static_cast<uint32>(*It); Hash *= 1099511628211ull; }
        Hash ^= Seed; Hash *= 1099511628211ull;
        Hash ^= static_cast<uint64>(Serial); Hash *= 1099511628211ull;
        Hash ^= Salt; Hash *= 1099511628211ull;
        return Hash;
    }
    float UnitFromHash(uint64 Hash)
    {
        return static_cast<float>(static_cast<uint32>(Hash & 0xffffffffull)) / 4294967296.0f;
    }
    bool IsValidState(EWorldNaturalEventState State)
    {
        return static_cast<uint8>(State) <= static_cast<uint8>(EWorldNaturalEventState::Ended);
    }
}

bool UWorldEventDirectorSubsystem::RegisterEventDefinition(const FWorldNaturalEventDefinition& D)
{
    if (D.DefinitionId.IsNone() || D.DisplayName.IsEmpty() || D.EffectId.IsNone() ||
        !FMath::IsFinite(D.WarningDurationSeconds) || D.WarningDurationSeconds < 0.1 ||
        !FMath::IsFinite(D.ActiveDurationSeconds) || D.ActiveDurationSeconds < 0.1 ||
        !FMath::IsFinite(D.CooldownSeconds) || D.CooldownSeconds < 0.0 ||
        !FMath::IsFinite(D.MinimumIntervalSeconds) || D.MinimumIntervalSeconds < 1.0 ||
        !FMath::IsFinite(D.MaximumIntervalSeconds) || D.MaximumIntervalSeconds < D.MinimumIntervalSeconds ||
        !FMath::IsFinite(D.TriggerChance) || D.TriggerChance < 0.0f || D.TriggerChance > 1.0f ||
        D.AllowedMonths.Num() > 12)
        return false;
    TSet<int32> Months;
    for (const int32 Month : D.AllowedMonths)
    {
        if (Month < 1 || Month > 12 || Months.Contains(Month)) return false;
        Months.Add(Month);
    }
    if (const FWorldNaturalEventDefinition* Existing = FindDefinition(D.DefinitionId))
    {
        return Existing->DisplayName == D.DisplayName && Existing->EffectId == D.EffectId &&
            Existing->TriggerAfterEventId == D.TriggerAfterEventId &&
            FMath::IsNearlyEqual(Existing->WarningDurationSeconds, D.WarningDurationSeconds) &&
            FMath::IsNearlyEqual(Existing->ActiveDurationSeconds, D.ActiveDurationSeconds) &&
            FMath::IsNearlyEqual(Existing->CooldownSeconds, D.CooldownSeconds) &&
            FMath::IsNearlyEqual(Existing->MinimumIntervalSeconds, D.MinimumIntervalSeconds) &&
            FMath::IsNearlyEqual(Existing->MaximumIntervalSeconds, D.MaximumIntervalSeconds) &&
            FMath::IsNearlyEqual(Existing->TriggerChance, D.TriggerChance) &&
            Existing->AllowedMonths == D.AllowedMonths;
    }
    if (bInitialized || MaxDefinitions < 1 || Definitions.Num() >= MaxDefinitions) return false;
    Definitions.Add(D);
    return true;
}

bool UWorldEventDirectorSubsystem::ValidateDefinitions() const
{
    if (Definitions.Num() > MaxDefinitions) return false;
    TSet<FName> Ids;
    TMap<FName, FName> Previous;
    for (const FWorldNaturalEventDefinition& D : Definitions)
    {
        if (D.DefinitionId.IsNone() || D.DisplayName.IsEmpty() || D.EffectId.IsNone() ||
            Ids.Contains(D.DefinitionId) || !FMath::IsFinite(D.WarningDurationSeconds) ||
            D.WarningDurationSeconds < 0.1 || !FMath::IsFinite(D.ActiveDurationSeconds) ||
            D.ActiveDurationSeconds < 0.1 || !FMath::IsFinite(D.CooldownSeconds) ||
            D.CooldownSeconds < 0.0 || !FMath::IsFinite(D.MinimumIntervalSeconds) ||
            D.MinimumIntervalSeconds < 1.0 || !FMath::IsFinite(D.MaximumIntervalSeconds) ||
            D.MaximumIntervalSeconds < D.MinimumIntervalSeconds || !FMath::IsFinite(D.TriggerChance) ||
            D.TriggerChance < 0.0f || D.TriggerChance > 1.0f)
            return false;
        for (const int32 Month : D.AllowedMonths)
            if (Month < 1 || Month > 12) return false;
        Ids.Add(D.DefinitionId);
        if (!D.TriggerAfterEventId.IsNone()) Previous.Add(D.DefinitionId, D.TriggerAfterEventId);
    }
    for (const FWorldNaturalEventDefinition& D : Definitions)
    {
        if (!D.TriggerAfterEventId.IsNone() && !Ids.Contains(D.TriggerAfterEventId)) return false;
        TSet<FName> Seen;
        FName Cursor = D.DefinitionId;
        while (!Cursor.IsNone())
        {
            if (Seen.Contains(Cursor)) return false;
            Seen.Add(Cursor);
            const FName* Next = Previous.Find(Cursor);
            Cursor = Next ? *Next : NAME_None;
        }
    }
    return true;
}

const FWorldNaturalEventDefinition* UWorldEventDirectorSubsystem::FindDefinition(FName Id) const
{
    return Definitions.FindByPredicate([Id](const FWorldNaturalEventDefinition& D) { return D.DefinitionId == Id; });
}
FWorldNaturalEventRecord* UWorldEventDirectorSubsystem::FindRuntimeRecord(FName Id)
{
    return RuntimeRecords.FindByPredicate([Id](const FWorldNaturalEventRecord& R) { return R.DefinitionId == Id; });
}
const FWorldNaturalEventRecord* UWorldEventDirectorSubsystem::FindRuntimeRecord(FName Id) const
{
    return RuntimeRecords.FindByPredicate([Id](const FWorldNaturalEventRecord& R) { return R.DefinitionId == Id; });
}
double UWorldEventDirectorSubsystem::SampleInterval(const FWorldNaturalEventDefinition& D, int64 Serial) const
{
    const float Alpha = UnitFromHash(StableHash(D.DefinitionId.ToString(), static_cast<uint32>(RandomSeed), Serial, 0xA511E9B3u));
    return FMath::Lerp(D.MinimumIntervalSeconds, D.MaximumIntervalSeconds, static_cast<double>(Alpha));
}
float UWorldEventDirectorSubsystem::SampleChance(FName Id, int64 Serial) const
{
    return UnitFromHash(StableHash(Id.ToString(), static_cast<uint32>(RandomSeed), Serial, 0x63D83595u));
}
FGuid UWorldEventDirectorSubsystem::MakeEventInstanceId(FName Id, int64 Serial) const
{
    const FString Key = Id.ToString();
    const uint32 Seed = static_cast<uint32>(RandomSeed);
    const uint64 H0 = StableHash(Key, Seed, Serial, 0x243F6A88u);
    const uint64 H1 = StableHash(Key, Seed, Serial, 0x85A308D3u);
    uint32 A = static_cast<uint32>(H0 >> 32), B = static_cast<uint32>(H0);
    uint32 C = static_cast<uint32>(H1 >> 32), D = static_cast<uint32>(H1);
    if ((A | B | C | D) == 0) A = 1;
    return FGuid(A, B, C, D);
}
bool UWorldEventDirectorSubsystem::IsMonthAllowed(const FWorldNaturalEventDefinition& D, double Time) const
{
    if (D.AllowedMonths.IsEmpty()) return true;
    const UWorld* World = GetWorld();
    const UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    FWorldCalendarDate Date;
    return Clock && WorldCalendar::DateFromTime(Time, Clock->CalendarConfig, Date) && D.AllowedMonths.Contains(Date.Month);
}
bool UWorldEventDirectorSubsystem::InitializeDirector(int32 Seed, double InitialTimeSeconds)
{
    if (bInitialized || !FMath::IsFinite(InitialTimeSeconds) || InitialTimeSeconds < 0.0 ||
        MaxDefinitions < 1 || MaxHistoryEntries < 1 || MaxTransitionsPerStep < 1 || !ValidateDefinitions())
        return false;
    RandomSeed = Seed; LastProcessedTimeSeconds = InitialTimeSeconds; StateRevision = 1;
    RuntimeRecords.Reset(); CompletedHistory.Reset(); PendingTransitions.Reset();
    for (const FWorldNaturalEventDefinition& D : Definitions)
    {
        FWorldNaturalEventRecord& R = RuntimeRecords.AddDefaulted_GetRef();
        R.DefinitionId = D.DefinitionId; R.State = EWorldNaturalEventState::Waiting; R.Revision = 1;
        R.ScheduledStartTimeSeconds = D.TriggerAfterEventId.IsNone() ? InitialTimeSeconds + SampleInterval(D, 1) : -1.0;
    }
    bInitialized = true; return true;
}
bool UWorldEventDirectorSubsystem::CanAdvanceToTime(double TargetTimeSeconds) const
{
    if (!bInitialized || !FMath::IsFinite(TargetTimeSeconds) || TargetTimeSeconds < LastProcessedTimeSeconds ||
        RuntimeRecords.Num() != Definitions.Num() || CompletedHistory.Num() > MaxHistoryEntries ||
        StateRevision == MAX_int64 || !ValidateDefinitions()) return false;
    for (const FWorldNaturalEventRecord& R : RuntimeRecords)
    {
        if (!FindDefinition(R.DefinitionId) || !IsValidState(R.State) || R.Serial < 0 ||
            R.AttemptCount < 0 || R.LastCompletedSerial < 0 || R.LastObservedPredecessorSerial < 0 ||
            R.Revision < 0 || !FMath::IsFinite(R.LastCompletedEndTimeSeconds) ||
            (R.ScheduledStartTimeSeconds >= 0.0 && !FMath::IsFinite(R.ScheduledStartTimeSeconds))) return false;
        if (R.State == EWorldNaturalEventState::Warning &&
            (!R.EventInstanceId.IsValid() || !FMath::IsFinite(R.WarningStartTimeSeconds) ||
             !FMath::IsFinite(R.ActiveStartTimeSeconds) || R.ActiveStartTimeSeconds < R.WarningStartTimeSeconds)) return false;
        if (R.State == EWorldNaturalEventState::Active &&
            (!R.EventInstanceId.IsValid() || !FMath::IsFinite(R.ActiveStartTimeSeconds) ||
             !FMath::IsFinite(R.PlannedEndTimeSeconds) || R.PlannedEndTimeSeconds < R.ActiveStartTimeSeconds)) return false;
    }
    return true;
}
bool UWorldEventDirectorSubsystem::ScheduleChainedEvents()
{
    bool Changed = false;
    for (FWorldNaturalEventRecord& R : RuntimeRecords)
    {
        if (R.State != EWorldNaturalEventState::Waiting || R.ScheduledStartTimeSeconds >= 0.0) continue;
        const FWorldNaturalEventDefinition* D = FindDefinition(R.DefinitionId);
        if (!D || D->TriggerAfterEventId.IsNone()) continue;
        const FWorldNaturalEventRecord* Prev = FindRuntimeRecord(D->TriggerAfterEventId);
        if (!Prev || Prev->LastCompletedSerial <= R.LastObservedPredecessorSerial) continue;
        R.LastObservedPredecessorSerial = Prev->LastCompletedSerial;
        R.TriggeredByDefinitionId = Prev->DefinitionId;
        R.ScheduledStartTimeSeconds = Prev->LastCompletedEndTimeSeconds + SampleInterval(*D, R.AttemptCount + R.Serial + 1);
        ++R.Revision; Changed = true;
    }
    return Changed;
}
bool UWorldEventDirectorSubsystem::AdvanceToTime(double TargetTimeSeconds)
{
    if (!CanAdvanceToTime(TargetTimeSeconds)) return false;
    if (TargetTimeSeconds == LastProcessedTimeSeconds) return true;
    const FWorldEventDirectorSnapshot Before = ExportSnapshot();
    TArray<FWorldNaturalEventRecord> Transitions;
    int32 Iterations = 0;
    bool bStable = false;
    while (Iterations++ < MaxTransitionsPerStep)
    {
        bool bChanged = ScheduleChainedEvents();
        for (FWorldNaturalEventRecord& R : RuntimeRecords)
        {
            const FWorldNaturalEventDefinition* D = FindDefinition(R.DefinitionId);
            if (!D) { RestoreSnapshot(Before); return false; }
            if (R.State == EWorldNaturalEventState::Waiting && R.ScheduledStartTimeSeconds >= 0.0 &&
                R.ScheduledStartTimeSeconds <= TargetTimeSeconds)
            {
                if (!IsMonthAllowed(*D, R.ScheduledStartTimeSeconds))
                {
                    R.ScheduledStartTimeSeconds += 86400.0; ++R.Revision; bChanged = true; continue;
                }
                ++R.AttemptCount;
                if (SampleChance(R.DefinitionId, R.AttemptCount) >= D->TriggerChance)
                {
                    R.ScheduledStartTimeSeconds += SampleInterval(*D, R.AttemptCount + R.Serial + 1);
                    ++R.Revision; bChanged = true; continue;
                }
                ++R.Serial; R.EventInstanceId = MakeEventInstanceId(R.DefinitionId, R.Serial);
                R.WarningStartTimeSeconds = R.ScheduledStartTimeSeconds;
                R.ActiveStartTimeSeconds = R.WarningStartTimeSeconds + D->WarningDurationSeconds;
                R.PlannedEndTimeSeconds = R.ActiveStartTimeSeconds + D->ActiveDurationSeconds;
                R.State = EWorldNaturalEventState::Warning; ++R.Revision; Transitions.Add(R); bChanged = true; continue;
            }
            if (R.State == EWorldNaturalEventState::Warning && R.ActiveStartTimeSeconds <= TargetTimeSeconds)
            {
                R.State = EWorldNaturalEventState::Active; ++R.Revision; Transitions.Add(R); bChanged = true; continue;
            }
            if (R.State == EWorldNaturalEventState::Active && R.PlannedEndTimeSeconds <= TargetTimeSeconds)
            {
                FWorldNaturalEventRecord Ended = R; Ended.State = EWorldNaturalEventState::Ended; ++Ended.Revision;
                Transitions.Add(Ended);
                R.LastCompletedSerial = R.Serial; R.LastCompletedEndTimeSeconds = R.PlannedEndTimeSeconds;
                R.EventInstanceId.Invalidate(); R.State = EWorldNaturalEventState::Waiting;
                R.WarningStartTimeSeconds = 0.0; R.ActiveStartTimeSeconds = 0.0; R.PlannedEndTimeSeconds = 0.0;
                R.ScheduledStartTimeSeconds = D->TriggerAfterEventId.IsNone()
                    ? R.LastCompletedEndTimeSeconds + D->CooldownSeconds + SampleInterval(*D, R.AttemptCount + R.Serial + 1)
                    : -1.0;
                ++R.Revision; CompletedHistory.Add(Ended);
                if (CompletedHistory.Num() > MaxHistoryEntries)
                    CompletedHistory.RemoveAt(0, CompletedHistory.Num() - MaxHistoryEntries, EAllowShrinking::No);
                bChanged = true;
            }
        }
        if (!bChanged) { bStable = true; break; }
    }
    if (!bStable || StateRevision == MAX_int64) { RestoreSnapshot(Before); return false; }
    LastProcessedTimeSeconds = TargetTimeSeconds; ++StateRevision;
    PendingTransitions.Append(Transitions);
    return true;
}
void UWorldEventDirectorSubsystem::PublishPendingTransitions()
{
    TArray<FWorldNaturalEventRecord> ToPublish = MoveTemp(PendingTransitions);
    PendingTransitions.Reset();
    for (const FWorldNaturalEventRecord& Record : ToPublish) OnEventTransition.Broadcast(Record);
}
TArray<FWorldNaturalEventRecord> UWorldEventDirectorSubsystem::GetCurrentEvents() const
{
    TArray<FWorldNaturalEventRecord> Result;
    for (const FWorldNaturalEventRecord& R : RuntimeRecords)
        if (R.State == EWorldNaturalEventState::Warning || R.State == EWorldNaturalEventState::Active) Result.Add(R);
    return Result;
}
TArray<FString> UWorldEventDirectorSubsystem::GetCurrentEventSummaries() const
{
    TArray<FString> Result;
    for (const FWorldNaturalEventRecord& R : RuntimeRecords)
    {
        if (R.State != EWorldNaturalEventState::Warning && R.State != EWorldNaturalEventState::Active) continue;
        const FWorldNaturalEventDefinition* D = FindDefinition(R.DefinitionId);
        const FString Label = D ? D->DisplayName : R.DefinitionId.ToString();
        Result.Add(FString::Printf(TEXT("%s: %s"),
            R.State == EWorldNaturalEventState::Warning ? TEXT("UYARI") : TEXT("ETKİN"), *Label));
    }
    return Result;
}
FWorldEventDirectorSnapshot UWorldEventDirectorSubsystem::ExportSnapshot() const
{
    FWorldEventDirectorSnapshot S;
    S.SchemaVersion = CurrentSchemaVersion; S.bInitialized = bInitialized; S.RandomSeed = RandomSeed;
    S.LastProcessedTimeSeconds = LastProcessedTimeSeconds; S.StateRevision = StateRevision;
    S.RuntimeRecords = RuntimeRecords; S.CompletedHistory = CompletedHistory; return S;
}
bool UWorldEventDirectorSubsystem::RestoreSnapshot(const FWorldEventDirectorSnapshot& S)
{
    if (S.SchemaVersion != CurrentSchemaVersion || S.RuntimeRecords.Num() > MaxDefinitions ||
        S.CompletedHistory.Num() > MaxHistoryEntries || !FMath::IsFinite(S.LastProcessedTimeSeconds) ||
        S.LastProcessedTimeSeconds < 0.0 || S.StateRevision < 0) return false;
    if (!S.bInitialized)
    {
        RuntimeRecords.Reset(); CompletedHistory.Reset(); PendingTransitions.Reset(); RandomSeed = S.RandomSeed;
        LastProcessedTimeSeconds = S.LastProcessedTimeSeconds; StateRevision = S.StateRevision; bInitialized = false; return true;
    }
    if (!ValidateDefinitions() || S.RuntimeRecords.Num() != Definitions.Num()) return false;
    TSet<FName> Seen;
    for (const FWorldNaturalEventRecord& R : S.RuntimeRecords)
    {
        if (!FindDefinition(R.DefinitionId) || Seen.Contains(R.DefinitionId) || !IsValidState(R.State) ||
            R.State == EWorldNaturalEventState::Ended || R.Serial < 0 || R.AttemptCount < 0 ||
            R.LastCompletedSerial < 0 || R.LastObservedPredecessorSerial < 0 || R.Revision < 0 ||
            !FMath::IsFinite(R.LastCompletedEndTimeSeconds) ||
            (R.ScheduledStartTimeSeconds >= 0.0 && !FMath::IsFinite(R.ScheduledStartTimeSeconds))) return false;
        if (R.State == EWorldNaturalEventState::Warning &&
            (!R.EventInstanceId.IsValid() || R.ActiveStartTimeSeconds < R.WarningStartTimeSeconds)) return false;
        if (R.State == EWorldNaturalEventState::Active &&
            (!R.EventInstanceId.IsValid() || R.PlannedEndTimeSeconds < R.ActiveStartTimeSeconds)) return false;
        Seen.Add(R.DefinitionId);
    }
    for (const FWorldNaturalEventRecord& R : S.CompletedHistory)
        if (!R.EventInstanceId.IsValid() || R.State != EWorldNaturalEventState::Ended ||
            !FindDefinition(R.DefinitionId) || R.Serial < 1 ||
            !FMath::IsFinite(R.WarningStartTimeSeconds) || !FMath::IsFinite(R.ActiveStartTimeSeconds) ||
            !FMath::IsFinite(R.PlannedEndTimeSeconds) || R.ActiveStartTimeSeconds < R.WarningStartTimeSeconds ||
            R.PlannedEndTimeSeconds < R.ActiveStartTimeSeconds) return false;
    RandomSeed = S.RandomSeed; LastProcessedTimeSeconds = S.LastProcessedTimeSeconds;
    StateRevision = S.StateRevision; RuntimeRecords = S.RuntimeRecords; CompletedHistory = S.CompletedHistory;
    PendingTransitions.Reset(); bInitialized = true; return true;
}
