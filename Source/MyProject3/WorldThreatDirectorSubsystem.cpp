#include "WorldThreatDirectorSubsystem.h"
#include "WorldRegionalEventSubsystem.h"

#include "WorldSimulationClockSubsystem.h"
#include "Engine/World.h"

UWorldSimulationClockSubsystem* UWorldThreatDirectorSubsystem::GetClock() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
}

bool UWorldThreatDirectorSubsystem::InitializeDirector(double FirstRaidDelaySeconds)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!Clock || !Clock->IsClockInitialized() ||
        !FMath::IsFinite(FirstRaidDelaySeconds) || FirstRaidDelaySeconds < 0.0 ||
        !FMath::IsFinite(MinimumIntervalSeconds) ||
        !FMath::IsFinite(MaximumIntervalSeconds) ||
        MinimumIntervalSeconds < 1.0 || MaximumIntervalSeconds < MinimumIntervalSeconds ||
        !FMath::IsFinite(WarningDurationSeconds) || WarningDurationSeconds <= 0.0 ||
        !FMath::IsFinite(RaidDurationSeconds) || RaidDurationSeconds <= 0.0 ||
        MaxHistoryEntries < 1)
    {
        return false;
    }

    CurrentRaid = FThreatRaidRecord();
    RaidHistory.Reset();
    RaidSerial = 0;
    SeenSignalOperationIds.Reset();
    LastSignalRaidTimeSeconds = -1.0;
    NextRaidTimeSeconds = Clock->GetSimulationTimeSeconds() + FirstRaidDelaySeconds;
    bInitialized = FMath::IsFinite(NextRaidTimeSeconds);
    return bInitialized;
}

bool UWorldThreatDirectorSubsystem::CanAdvanceDirectorAtTime(
    double TargetTimeSeconds, int64 TargetTickIndex) const
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!bInitialized || !Clock || !Clock->IsClockInitialized() ||
        !FMath::IsFinite(TargetTimeSeconds) ||
        TargetTimeSeconds < Clock->GetSimulationTimeSeconds() ||
        TargetTickIndex < Clock->GetTickIndex())
    {
        return false;
    }

    const bool bWarningWillActivate =
        CurrentRaid.State == EThreatRaidState::Warning &&
        TargetTimeSeconds >= CurrentRaid.ActiveStartTime;
    const bool bWillFinishCurrent =
        (CurrentRaid.State == EThreatRaidState::Active ||
         (bWarningWillActivate && TargetTimeSeconds >= CurrentRaid.PlannedEndTime)) &&
        TargetTimeSeconds >= CurrentRaid.PlannedEndTime;

    if ((bWarningWillActivate && CurrentRaid.Revision == MAX_int64) ||
        (bWillFinishCurrent && CurrentRaid.Revision >=
            (bWarningWillActivate ? MAX_int64 - 1 : MAX_int64)))
    {
        return false;
    }

    const bool bWillNeedRaid =
        (CurrentRaid.State == EThreatRaidState::None || IsTerminal(CurrentRaid.State) ||
         bWillFinishCurrent) &&
        TargetTimeSeconds >= NextRaidTimeSeconds;
    if (!bWillNeedRaid)
    {
        return true;
    }

    if (RaidSerial == MAX_int64)
    {
        return false;
    }

    const int64 ProposedSerial = RaidSerial + 1;
    const int32 Seed = Clock->GetRandomSeed() ^ static_cast<int32>(ProposedSerial * 196613);
    const FGuid ProposedId(Clock->GetRandomSeed(), static_cast<int32>(ProposedSerial),
        static_cast<int32>(TargetTickIndex), Seed);
    const double ActiveStart = TargetTimeSeconds + WarningDurationSeconds;
    const double PlannedEnd = ActiveStart + RaidDurationSeconds;
    const double NextTime = PlannedEnd + MakeNextInterval(ProposedSerial);
    return ProposedId.IsValid() && FMath::IsFinite(ActiveStart) &&
        FMath::IsFinite(PlannedEnd) && FMath::IsFinite(NextTime);
}

bool UWorldThreatDirectorSubsystem::AdvanceDirectorAtTime(
    double TargetTimeSeconds, int64 TargetTickIndex)
{
    if (!CanAdvanceDirectorAtTime(TargetTimeSeconds, TargetTickIndex))
    {
        return false;
    }

    UWorldSimulationClockSubsystem* Clock = GetClock();
    const double Now = TargetTimeSeconds;

    if (CurrentRaid.State == EThreatRaidState::Warning && Now >= CurrentRaid.ActiveStartTime)
    {
        CurrentRaid.State = EThreatRaidState::Active;
        ++CurrentRaid.Revision;
    }

    if (CurrentRaid.State == EThreatRaidState::Active && Now >= CurrentRaid.PlannedEndTime)
    {
        FinishCurrentRaid(EThreatRaidState::Failed, Now);
    }

    if ((CurrentRaid.State == EThreatRaidState::None || IsTerminal(CurrentRaid.State)) &&
        Now >= NextRaidTimeSeconds)
    {
        if (RaidSerial == MAX_int64)
        {
            return false;
        }

        ++RaidSerial;
        const int32 Seed = Clock->GetRandomSeed() ^ static_cast<int32>(RaidSerial * 196613);
        FRandomStream Stream(Seed);
        const FGuid RaidId = FGuid(Clock->GetRandomSeed(), static_cast<int32>(RaidSerial),
            static_cast<int32>(TargetTickIndex), Seed);
        if (!RaidId.IsValid())
        {
            return false;
        }

        CurrentRaid = FThreatRaidRecord();
        CurrentRaid.RaidId = RaidId;
        CurrentRaid.Serial = RaidSerial;
        CurrentRaid.State = EThreatRaidState::Warning;
        CurrentRaid.WarningStartTime = Now;
        CurrentRaid.ActiveStartTime = Now + WarningDurationSeconds;
        CurrentRaid.PlannedEndTime = CurrentRaid.ActiveStartTime + RaidDurationSeconds;
        CurrentRaid.Pressure = Stream.FRandRange(1.0f, 3.0f);
        UWorldRegionalEventSubsystem* RegionalEvents = GetWorld()
            ? GetWorld()->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
        const TArray<FName> AvailableRegions = RegionalEvents && RegionalEvents->AreDefinitionsLoaded()
            ? RegionalEvents->GetRegionIds() : TArray<FName>();
        if (!AvailableRegions.IsEmpty())
        {
            const int32 RegionIndex = Stream.RandRange(0, AvailableRegions.Num() - 1);
            CurrentRaid.TargetRegionId = AvailableRegions[RegionIndex];
            FWorldRegionProfile RegionProfile;
            if (RegionalEvents->GetRegionProfile(CurrentRaid.TargetRegionId, RegionProfile))
                CurrentRaid.TargetLocation = RegionProfile.RaidTargetLocation;
        }
        else
        {
            CurrentRaid.TargetLocation = FVector(
                Stream.FRandRange(-5000.0f, 5000.0f),
                Stream.FRandRange(-5000.0f, 5000.0f), 0.0f);
        }
        CurrentRaid.Revision = 1;
        NextRaidTimeSeconds = CurrentRaid.PlannedEndTime + MakeNextInterval(RaidSerial);
        if (!FMath::IsFinite(NextRaidTimeSeconds))
        {
            CurrentRaid = FThreatRaidRecord();
            return false;
        }
    }

    return true;
}

bool UWorldThreatDirectorSubsystem::AdvanceDirector()
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!Clock || !Clock->IsClockInitialized() || Clock->GetTickIndex() == MAX_int64)
    {
        return false;
    }
    return AdvanceDirectorAtTime(Clock->GetSimulationTimeSeconds(), Clock->GetTickIndex());
}

bool UWorldThreatDirectorSubsystem::TryClaimDamagePulse(const FGuid& RaidId, int64 PulseIndex)
{
    if (!RaidId.IsValid() || PulseIndex < 0 ||
        CurrentRaid.RaidId != RaidId || CurrentRaid.State != EThreatRaidState::Active ||
        CurrentRaid.Revision >= MAX_int64 - 1 ||
        CurrentRaid.LastAppliedDamagePulseIndex >= PulseIndex)
        return false;

    // Persist the pulse claim before any actor damage/destruction. A failed
    // materialization/collapse may block the result, but must not replay damage.
    CurrentRaid.LastAppliedDamagePulseIndex = PulseIndex;
    ++CurrentRaid.Revision;
    return true;
}


void UWorldThreatDirectorSubsystem::RememberSignalOperation(const FGuid& OperationId)
{
    if (!OperationId.IsValid() || SeenSignalOperationIds.Contains(OperationId))
        return;
    SeenSignalOperationIds.Add(OperationId);
    constexpr int32 MaxSignalReceipts = 256;
    while (SeenSignalOperationIds.Num() > MaxSignalReceipts)
        SeenSignalOperationIds.RemoveAt(0, 1, EAllowShrinking::No);
}

bool UWorldThreatDirectorSubsystem::TryStartRaidFromSignals(
    const FGuid& OperationId, FName RegionId,
    float StorageValueSignal, float SmellSignal, float LightSignal, float SmokeSignal,
    bool& bOutRaidStarted)
{
    bOutRaidStarted = false;
    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock = GetClock();
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    const auto ValidSignal = [](float Value)
    {
        return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
    };
    if (!bInitialized || !Clock || !Clock->IsClockInitialized() || !Events ||
        !Events->AreDefinitionsLoaded() || !OperationId.IsValid() || RegionId.IsNone() ||
        SeenSignalOperationIds.Contains(OperationId) ||
        !ValidSignal(StorageValueSignal) || !ValidSignal(SmellSignal) ||
        !ValidSignal(LightSignal) || !ValidSignal(SmokeSignal) ||
        !FMath::IsFinite(SignalRaidThreshold) || SignalRaidThreshold <= 0.0f ||
        !FMath::IsFinite(SignalRaidCooldownSeconds) || SignalRaidCooldownSeconds < 0.0 ||
        !FMath::IsFinite(WarningDurationSeconds) || WarningDurationSeconds <= 0.0 ||
        !FMath::IsFinite(RaidDurationSeconds) || RaidDurationSeconds <= 0.0 ||
        !FMath::IsFinite(MinimumIntervalSeconds) || MinimumIntervalSeconds < 1.0 ||
        !FMath::IsFinite(MaximumIntervalSeconds) || MaximumIntervalSeconds < MinimumIntervalSeconds)
    {
        return false;
    }

    FWorldRegionProfile RegionProfile;
    if (!Events->GetRegionProfile(RegionId, RegionProfile))
        return false;

    const float Pressure = StorageValueSignal * 0.65f + SmellSignal * 0.80f +
        LightSignal * 0.65f + SmokeSignal * 0.90f;
    const double Now = Clock->GetSimulationTimeSeconds();
    const bool bRaidAlreadyActive =
        CurrentRaid.State == EThreatRaidState::Warning || CurrentRaid.State == EThreatRaidState::Active;
    const bool bOnCooldown = LastSignalRaidTimeSeconds >= 0.0 &&
        Now - LastSignalRaidTimeSeconds < SignalRaidCooldownSeconds;

    if (Pressure < SignalRaidThreshold || bRaidAlreadyActive || bOnCooldown)
    {
        RememberSignalOperation(OperationId);
        return true;
    }

    if (RaidSerial == MAX_int64 ||
        !FMath::IsFinite(Now + WarningDurationSeconds) ||
        !FMath::IsFinite(Now + WarningDurationSeconds + RaidDurationSeconds))
    {
        return false;
    }

    const int64 ProposedSerial = RaidSerial + 1;
    FThreatRaidRecord ProposedRaid;
    ProposedRaid.RaidId = OperationId;
    ProposedRaid.Serial = ProposedSerial;
    ProposedRaid.State = EThreatRaidState::Warning;
    ProposedRaid.WarningStartTime = Now;
    ProposedRaid.ActiveStartTime = Now + WarningDurationSeconds;
    ProposedRaid.PlannedEndTime = ProposedRaid.ActiveStartTime + RaidDurationSeconds;
    ProposedRaid.Pressure = FMath::Clamp(Pressure, 0.0f, 4.0f);
    ProposedRaid.TargetRegionId = RegionId;
    ProposedRaid.TargetLocation = RegionProfile.RaidTargetLocation;
    ProposedRaid.Revision = 1;

    const double ProposedNextRaidTime = ProposedRaid.PlannedEndTime + MakeNextInterval(ProposedSerial);
    if (!ProposedRaid.RaidId.IsValid() || !FMath::IsFinite(ProposedNextRaidTime))
        return false;

    RememberSignalOperation(OperationId);
    CurrentRaid = ProposedRaid;
    RaidSerial = ProposedSerial;
    NextRaidTimeSeconds = ProposedNextRaidTime;
    LastSignalRaidTimeSeconds = Now;
    bOutRaidStarted = true;
    return true;
}

bool UWorldThreatDirectorSubsystem::ResolveCurrentRaid(bool bRepelled)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!bInitialized || !Clock || !Clock->IsClockInitialized() ||
        (CurrentRaid.State != EThreatRaidState::Warning &&
         CurrentRaid.State != EThreatRaidState::Active))
    {
        return false;
    }

    FinishCurrentRaid(bRepelled ? EThreatRaidState::Repelled : EThreatRaidState::Failed,
        Clock->GetSimulationTimeSeconds());
    return true;
}

bool UWorldThreatDirectorSubsystem::CancelCurrentRaid()
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!bInitialized || !Clock || !Clock->IsClockInitialized() ||
        (CurrentRaid.State != EThreatRaidState::Warning &&
         CurrentRaid.State != EThreatRaidState::Active))
    {
        return false;
    }

    FinishCurrentRaid(EThreatRaidState::Cancelled, Clock->GetSimulationTimeSeconds());
    return true;
}

bool UWorldThreatDirectorSubsystem::GetCurrentRaid(FThreatRaidRecord& OutRaid) const
{
    if (CurrentRaid.State == EThreatRaidState::None)
    {
        return false;
    }
    OutRaid = CurrentRaid;
    return true;
}

bool UWorldThreatDirectorSubsystem::IsTerminal(EThreatRaidState State) const
{
    return State == EThreatRaidState::Repelled ||
        State == EThreatRaidState::Failed ||
        State == EThreatRaidState::Cancelled;
}

double UWorldThreatDirectorSubsystem::MakeNextInterval(int64 Serial) const
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!Clock)
    {
        return 0.0;
    }
    const int32 Seed = Clock->GetRandomSeed() ^ static_cast<int32>(Serial * 83492791);
    FRandomStream Stream(Seed);
    return Stream.FRandRange(
        static_cast<float>(MinimumIntervalSeconds),
        static_cast<float>(MaximumIntervalSeconds));
}

void UWorldThreatDirectorSubsystem::FinishCurrentRaid(EThreatRaidState FinalState, double Now)
{
    if (CurrentRaid.State == EThreatRaidState::None || IsTerminal(CurrentRaid.State))
    {
        return;
    }

    CurrentRaid.State = FinalState;
    ++CurrentRaid.Revision;
    if (MaxHistoryEntries < 1)
    {
        RaidHistory.Reset();
        return;
    }
    while (RaidHistory.Num() >= MaxHistoryEntries)
    {
        RaidHistory.RemoveAt(0, 1, EAllowShrinking::No);
    }
    RaidHistory.Add(CurrentRaid);
    if (!FMath::IsFinite(NextRaidTimeSeconds) || NextRaidTimeSeconds < Now)
    {
        NextRaidTimeSeconds = Now + MakeNextInterval(RaidSerial);
    }
}
