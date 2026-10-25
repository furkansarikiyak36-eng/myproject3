#include "WorldThreatDirectorSubsystem.h"

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
    NextRaidTimeSeconds = Clock->GetSimulationTimeSeconds() + FirstRaidDelaySeconds;
    bInitialized = FMath::IsFinite(NextRaidTimeSeconds);
    return bInitialized;
}

bool UWorldThreatDirectorSubsystem::AdvanceDirector()
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!bInitialized || !Clock || !Clock->IsClockInitialized())
    {
        return false;
    }

    const double Now = Clock->GetSimulationTimeSeconds();
    if (!FMath::IsFinite(Now))
    {
        return false;
    }

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
            static_cast<int32>(Clock->GetTickIndex()), Seed);
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
        CurrentRaid.TargetLocation = FVector(
            Stream.FRandRange(-5000.0f, 5000.0f),
            Stream.FRandRange(-5000.0f, 5000.0f), 0.0f);
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
