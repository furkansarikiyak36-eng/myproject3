#include "WorldThreatDirectorSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "Engine/World.h"

FWorldThreatDirectorSnapshot UWorldThreatDirectorSubsystem::ExportSnapshot() const
{
    FWorldThreatDirectorSnapshot Snapshot;
    Snapshot.SchemaVersion = 2;
    Snapshot.CurrentRaid = CurrentRaid;
    Snapshot.RaidHistory = RaidHistory;
    Snapshot.NextRaidTimeSeconds = NextRaidTimeSeconds;
    Snapshot.RaidSerial = RaidSerial;
    Snapshot.bInitialized = bInitialized;
    return Snapshot;
}

bool UWorldThreatDirectorSubsystem::RestoreSnapshot(const FWorldThreatDirectorSnapshot& Snapshot)
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!Clock || !Clock->IsClockInitialized() || Snapshot.SchemaVersion != 2 ||
        !FMath::IsFinite(Snapshot.NextRaidTimeSeconds) || Snapshot.NextRaidTimeSeconds < 0.0 ||
        Snapshot.RaidSerial < 0 || Snapshot.RaidHistory.Num() > MaxHistoryEntries ||
        Snapshot.RaidHistory.Num() < 0)
        return false;

    auto IsValidRaid = [](const FThreatRaidRecord& Raid, bool bMayBeEmpty)
    {
        if (static_cast<uint8>(Raid.State) > static_cast<uint8>(EThreatRaidState::Cancelled) ||
            Raid.LastCommittedAttackPulseIndex < -1)
            return false;
        if (Raid.State == EThreatRaidState::None && bMayBeEmpty) return true;
        return Raid.RaidId.IsValid() && Raid.Serial >= 0 && Raid.Revision >= 0 &&
            FMath::IsFinite(Raid.WarningStartTime) && Raid.WarningStartTime >= 0.0 &&
            FMath::IsFinite(Raid.ActiveStartTime) && Raid.ActiveStartTime >= Raid.WarningStartTime &&
            FMath::IsFinite(Raid.PlannedEndTime) && Raid.PlannedEndTime >= Raid.ActiveStartTime &&
            FMath::IsFinite(Raid.Pressure) && Raid.Pressure >= 0.0f &&
            FMath::IsFinite(Raid.TargetLocation.X) && FMath::IsFinite(Raid.TargetLocation.Y) &&
            FMath::IsFinite(Raid.TargetLocation.Z);
    };
    if (!IsValidRaid(Snapshot.CurrentRaid, true)) return false;
    for (const FThreatRaidRecord& Raid : Snapshot.RaidHistory)
        if (!IsValidRaid(Raid, false) || Raid.State == EThreatRaidState::Warning ||
            Raid.State == EThreatRaidState::Active) return false;

    CurrentRaid = Snapshot.CurrentRaid;
    RaidHistory = Snapshot.RaidHistory;
    NextRaidTimeSeconds = Snapshot.NextRaidTimeSeconds;
    RaidSerial = Snapshot.RaidSerial;
    bInitialized = Snapshot.bInitialized;
    return true;
}
