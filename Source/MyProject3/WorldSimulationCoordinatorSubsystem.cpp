#include "WorldSimulationCoordinatorSubsystem.h"

#include "Engine/World.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldCargoTransportSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

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

    // Capture every mutable participant before the first commit. Cargo delivery
    // also mutates item ownership, so the ledger must be part of the same rollback.
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!Ownership) return false;

    const FWorldSimulationClockSnapshot BeforeClock = Clock->ExportSnapshot();
    const FWorldThreatDirectorSnapshot BeforeThreat = Threat->ExportSnapshot();
    TArray<FWorldCropState> BeforeCrops;
    TArray<FWorldAnimalState> BeforeAnimals;
    Agriculture->ExportSnapshot(BeforeCrops, BeforeAnimals);
    TArray<FWorldSurvivalProfile> BeforeProfiles;
    TArray<FWorldDiscoveryRecord> BeforeDiscoveries;
    EWorldWeatherState BeforeWeather = EWorldWeatherState::Clear;
    float BeforeWeatherSeverity = 0.0f;
    Survival->ExportSnapshot(BeforeProfiles, BeforeDiscoveries, BeforeWeather, BeforeWeatherSeverity);
    TArray<FWorldProductionStation> BeforeStations;
    TArray<FWorldProductionRecipe> BeforeRecipes;
    Production->ExportSnapshot(BeforeStations, BeforeRecipes);
    TArray<FWorldCargoTransportRecord> BeforeCargo;
    Cargo->ExportSnapshot(BeforeCargo);
    TArray<FWorldOwnedItemRecord> BeforeItems;
    Ownership->ExportSnapshot(BeforeItems);

    auto RollbackStep = [&]() -> bool
    {
        bool bRollbackSucceeded = true;
        bRollbackSucceeded = Cargo->RestoreSnapshot(BeforeCargo) && bRollbackSucceeded;
        bRollbackSucceeded = Production->RestoreSnapshot(BeforeStations, BeforeRecipes) && bRollbackSucceeded;
        bRollbackSucceeded = Survival->RestoreSnapshot(BeforeProfiles, BeforeDiscoveries,
            BeforeWeather, BeforeWeatherSeverity) && bRollbackSucceeded;
        bRollbackSucceeded = Agriculture->RestoreSnapshot(BeforeCrops, BeforeAnimals) && bRollbackSucceeded;
        bRollbackSucceeded = Ownership->RestoreSnapshot(BeforeItems) && bRollbackSucceeded;
        bRollbackSucceeded = Threat->RestoreSnapshot(BeforeThreat) && bRollbackSucceeded;
        bRollbackSucceeded = Clock->RestoreSnapshot(BeforeClock) && bRollbackSucceeded;
        if (!bRollbackSucceeded)
        {
            UE_LOG(LogTemp, Error,
                TEXT("World fixed-step rollback failed. Stop simulation and capture diagnostics immediately."));
        }
        return bRollbackSucceeded;
    };

    // Every participant completed preflight above. If a commit still rejects,
    // restore all participating logical state instead of leaving a mixed tick.
    if (!Agriculture->AdvanceAgricultureStep(DeltaSeconds) ||
        !Survival->AdvanceSurvivalStep(DeltaSeconds, TargetTime) ||
        !Production->AdvanceProductionStep(DeltaSeconds) ||
        !Cargo->AdvanceCargoStep(DeltaSeconds) ||
        !Threat->AdvanceDirectorAtTime(TargetTime, TargetTick) ||
        !Clock->AdvanceSimulation(DeltaSeconds))
    {
        RollbackStep();
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
