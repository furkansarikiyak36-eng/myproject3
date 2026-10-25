#include "WorldSimulationCoordinatorSubsystem.h"

#include "Engine/World.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldCargoTransportSubsystem.h"

UWorldSimulationClockSubsystem* UWorldSimulationCoordinatorSubsystem::GetClock() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
}

UWorldThreatDirectorSubsystem* UWorldSimulationCoordinatorSubsystem::GetThreatDirector() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldThreatDirectorSubsystem>() : nullptr;
}

UWorldAgricultureSubsystem* UWorldSimulationCoordinatorSubsystem::GetAgriculture() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
}

UWorldSurvivalSubsystem* UWorldSimulationCoordinatorSubsystem::GetSurvival() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
}

UWorldProductionSubsystem* UWorldSimulationCoordinatorSubsystem::GetProduction() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
}

UWorldCargoTransportSubsystem* UWorldSimulationCoordinatorSubsystem::GetCargo() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldCargoTransportSubsystem>() : nullptr;
}

bool UWorldSimulationCoordinatorSubsystem::InitializeWorldSimulation(
    int32 RandomSeed, double InitialTimeSeconds, double FirstRaidDelaySeconds)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    if (!Clock || !Threat || Clock->IsClockInitialized() || Threat->IsDirectorInitialized() ||
        !FMath::IsFinite(InitialTimeSeconds) || InitialTimeSeconds < 0.0 ||
        !FMath::IsFinite(FirstRaidDelaySeconds) || FirstRaidDelaySeconds < 0.0 ||
        !FMath::IsFinite(InitialTimeSeconds + FirstRaidDelaySeconds) ||
        !FMath::IsFinite(Threat->MinimumIntervalSeconds) ||
        !FMath::IsFinite(Threat->MaximumIntervalSeconds) ||
        Threat->MinimumIntervalSeconds < 1.0 ||
        Threat->MaximumIntervalSeconds < Threat->MinimumIntervalSeconds ||
        !FMath::IsFinite(Threat->WarningDurationSeconds) || Threat->WarningDurationSeconds <= 0.0 ||
        !FMath::IsFinite(Threat->RaidDurationSeconds) || Threat->RaidDurationSeconds <= 0.0 ||
        Threat->MaxHistoryEntries < 1)
    {
        return false;
    }

    if (!Clock->InitializeClock(RandomSeed, InitialTimeSeconds))
    {
        return false;
    }

    // Configuration and time arithmetic are validated before either subsystem
    // is initialized, preventing predictable partial bootstrap failures.
    if (!Threat->InitializeDirector(FirstRaidDelaySeconds))
    {
        return false; // Defensive only; the same conditions were preflighted above.
    }

    CompletedCoordinatorSteps = 0;
    return true;
}

bool UWorldSimulationCoordinatorSubsystem::AdvanceFixedStep(double DeltaSeconds)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    UWorldAgricultureSubsystem* Agriculture = GetAgriculture();
    UWorldSurvivalSubsystem* Survival = GetSurvival();
    UWorldProductionSubsystem* Production = GetProduction();
    UWorldCargoTransportSubsystem* Cargo = GetCargo();
    if (!Clock || !Threat || !Agriculture || !Survival || !Production || !Cargo ||
        !Clock->IsClockInitialized() || !Threat->IsDirectorInitialized() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 ||
        CompletedCoordinatorSteps == MAX_int64 || !Clock->CanAdvanceSimulation(DeltaSeconds))
    {
        return false;
    }

    const double TargetTime = Clock->GetSimulationTimeSeconds() + DeltaSeconds;
    const int64 TargetTick = Clock->GetTickIndex() + 1;
    if (!Agriculture->CanAdvanceAgricultureStep(DeltaSeconds) ||
        !Survival->CanAdvanceSurvivalStep(DeltaSeconds, TargetTime) ||
        !Production->CanAdvanceProductionStep(DeltaSeconds) ||
        !Cargo->CanAdvanceCargoStep(DeltaSeconds) ||
        !Threat->CanAdvanceDirectorAtTime(TargetTime, TargetTick))
    {
        return false;
    }

    // Every participating subsystem has completed a non-mutating preflight.
    // No delegate is broadcast during these commits; the fixed-step driver is
    // the sole writer under the game's single-threaded world-simulation rule.
    if (!Agriculture->AdvanceAgricultureStep(DeltaSeconds) ||
        !Survival->AdvanceSurvivalStep(DeltaSeconds, TargetTime) ||
        !Production->AdvanceProductionStep(DeltaSeconds) ||
        !Cargo->AdvanceCargoStep(DeltaSeconds) ||
        !Threat->AdvanceDirectorAtTime(TargetTime, TargetTick) ||
        !Clock->AdvanceSimulation(DeltaSeconds))
    {
        // A failure here is an invariant violation: stop driving simulation and
        // capture state for diagnosis rather than silently counting the step.
        return false;
    }

    ++CompletedCoordinatorSteps;
    return true;
}

bool UWorldSimulationCoordinatorSubsystem::IsWorldSimulationReady() const
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    const UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    const UWorldAgricultureSubsystem* Agriculture = GetAgriculture();
    const UWorldSurvivalSubsystem* Survival = GetSurvival();
    const UWorldProductionSubsystem* Production = GetProduction();
    const UWorldCargoTransportSubsystem* Cargo = GetCargo();
    return Clock && Threat && Agriculture && Survival && Production && Cargo &&
        Clock->IsClockInitialized() && Threat->IsDirectorInitialized();
}
