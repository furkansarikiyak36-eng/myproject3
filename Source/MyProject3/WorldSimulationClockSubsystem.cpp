#include "WorldSimulationClockSubsystem.h"

bool UWorldSimulationClockSubsystem::InitializeClock(
    int32 InRandomSeed,
    double InitialSimulationTimeSeconds)
{
    if (!FMath::IsFinite(InitialSimulationTimeSeconds) ||
        InitialSimulationTimeSeconds < 0.0)
    {
        return false;
    }

    RandomSeed = InRandomSeed;
    SimulationTimeSeconds = InitialSimulationTimeSeconds;
    TickIndex = 0;
    StateRevision = 1;
    bInitialized = true;
    return true;
}

bool UWorldSimulationClockSubsystem::CanAdvanceSimulation(double DeltaSeconds) const
{
    return bInitialized &&
        FMath::IsFinite(DeltaSeconds) && DeltaSeconds >= 0.0 &&
        FMath::IsFinite(SimulationTimeSeconds + DeltaSeconds) &&
        TickIndex < MAX_int64 && StateRevision < MAX_int64;
}

bool UWorldSimulationClockSubsystem::AdvanceSimulation(double DeltaSeconds)
{
    if (!CanAdvanceSimulation(DeltaSeconds))
    {
        return false;
    }

    SimulationTimeSeconds += DeltaSeconds;
    ++TickIndex;
    ++StateRevision;
    return true;
}

FWorldSimulationClockSnapshot
UWorldSimulationClockSubsystem::ExportSnapshot() const
{
    FWorldSimulationClockSnapshot Snapshot;
    Snapshot.SchemaVersion = CurrentSchemaVersion;
    Snapshot.SimulationTimeSeconds = SimulationTimeSeconds;
    Snapshot.TickIndex = TickIndex;
    Snapshot.RandomSeed = RandomSeed;
    Snapshot.StateRevision = StateRevision;
    return Snapshot;
}

bool UWorldSimulationClockSubsystem::RestoreSnapshot(
    const FWorldSimulationClockSnapshot& Snapshot)
{
    if (Snapshot.SchemaVersion != CurrentSchemaVersion ||
        !FMath::IsFinite(Snapshot.SimulationTimeSeconds) ||
        Snapshot.SimulationTimeSeconds < 0.0 ||
        Snapshot.TickIndex < 0 ||
        Snapshot.StateRevision < 0)
    {
        return false;
    }

    SimulationTimeSeconds = Snapshot.SimulationTimeSeconds;
    TickIndex = Snapshot.TickIndex;
    RandomSeed = Snapshot.RandomSeed;
    StateRevision = Snapshot.StateRevision;
    bInitialized = true;
    return true;
}

int32 UWorldSimulationClockSubsystem::GetDeterministicSeedForOperation(
    const FGuid& OperationId) const
{
    if (!OperationId.IsValid())
    {
        return RandomSeed;
    }

    // Fixed integer mixing only; no frame time, global RNG or call-order state.
    uint32 Mixed = static_cast<uint32>(RandomSeed);
    Mixed ^= OperationId.A + 0x9e3779b9u + (Mixed << 6) + (Mixed >> 2);
    Mixed ^= OperationId.B + 0x85ebca6bu + (Mixed << 6) + (Mixed >> 2);
    Mixed ^= OperationId.C + 0xc2b2ae35u + (Mixed << 6) + (Mixed >> 2);
    Mixed ^= OperationId.D + 0x27d4eb2fu + (Mixed << 6) + (Mixed >> 2);
    return static_cast<int32>(Mixed);
}
