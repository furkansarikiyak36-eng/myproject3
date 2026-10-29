#include "WorldDemoBootstrapActor.h"

#include "Engine/World.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "WorldRaidAdapterSubsystem.h"
#include "WorldGameDataSubsystem.h"

AWorldDemoBootstrapActor::AWorldDemoBootstrapActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AWorldDemoBootstrapActor::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoStartOnBeginPlay)
    {
        StartSimulation();
    }
}

void AWorldDemoBootstrapActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bDriverRunning)
    {
        return;
    }

    if (!Coordinator || Coordinator->IsSimulationFaulted() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0 ||
        !ValidateConfiguration())
    {
        StopSimulation();
        return;
    }

    // Preserve accumulated time when the per-frame work limit is reached;
    // never silently discard simulation time to hide a slow frame.
    AccumulatorSeconds += static_cast<double>(DeltaSeconds) * SimulationTimeScale;
    if (!FMath::IsFinite(AccumulatorSeconds))
    {
        StopSimulation();
        return;
    }

    int32 StepsThisFrame = 0;
    while (AccumulatorSeconds >= FixedStepSeconds && StepsThisFrame < MaxStepsPerFrame)
    {
        if (!Coordinator->AdvanceFixedStep(FixedStepSeconds))
        {
            StopSimulation();
            return;
        }

        // Physical raid effects use the same fixed-step clock as the logical
        // threat director. Failure after this committed step must fail closed.
        UWorldRaidAdapterSubsystem* RaidAdapter =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
        if (!RaidAdapter || !RaidAdapter->AdvanceRaidWorldAdapter())
        {
            Coordinator->MarkSimulationFaulted(
                TEXT("World raid adapter failed after a committed fixed step; stop and recover/reset the world."));
            StopSimulation();
            return;
        }

        AccumulatorSeconds -= FixedStepSeconds;
        ++StepsThisFrame;
    }
}

bool AWorldDemoBootstrapActor::StartSimulation()
{
    if (!ValidateConfiguration())
    {
        UE_LOG(LogTemp, Error, TEXT("World demo bootstrap has invalid fixed-step configuration."));
        bDriverRunning = false;
        return false;
    }

    UWorld* World = GetWorld();
    Coordinator = World ? World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>() : nullptr;
    if (!Coordinator || Coordinator->IsSimulationFaulted())
    {
        UE_LOG(LogTemp, Error, TEXT("World demo bootstrap cannot start: coordinator is missing or faulted."));
        bDriverRunning = false;
        return false;
    }

    // A previously initialized coordinator is reused. This avoids resetting
    // the clock or raid schedule if another explicit bootstrap already ran.
    if (!Coordinator->IsWorldSimulationReady())
    {
        // Game data first: calendar, climate, recipes and survival definitions.
        UWorldGameDataSubsystem* GameData = GetWorld()->GetSubsystem<UWorldGameDataSubsystem>();
        if (GameData && !GameData->IsApplied() && (!GameData->LoadDefault() || !GameData->ApplyToWorld()))
            UE_LOG(LogTemp, Error, TEXT("World demo bootstrap: game data failed: %s"), *GameData->GetLastError());
        if (!Coordinator->InitializeWorldSimulation(
            RandomSeed, InitialTimeSeconds, FirstRaidDelaySeconds))
        {
            UE_LOG(LogTemp, Error, TEXT("World demo bootstrap failed to initialize world simulation."));
            bDriverRunning = false;
            return false;
        }
    }

    if (!Coordinator->IsWorldSimulationReady())
    {
        UE_LOG(LogTemp, Error, TEXT("World demo bootstrap initialization did not produce a ready simulation."));
        bDriverRunning = false;
        return false;
    }

    AccumulatorSeconds = 0.0;
    bDriverRunning = true;
    SetActorTickEnabled(true);
    return true;
}

void AWorldDemoBootstrapActor::StopSimulation()
{
    bDriverRunning = false;
    AccumulatorSeconds = 0.0;
}

bool AWorldDemoBootstrapActor::ValidateConfiguration() const
{
    return FMath::IsFinite(InitialTimeSeconds) && InitialTimeSeconds >= 0.0 &&
        FMath::IsFinite(FirstRaidDelaySeconds) && FirstRaidDelaySeconds >= 0.0 &&
        FMath::IsFinite(FixedStepSeconds) && FixedStepSeconds >= 0.01 &&
        FixedStepSeconds <= 1.0 &&
        MaxStepsPerFrame >= 1 && MaxStepsPerFrame <= 64 &&
        FMath::IsFinite(SimulationTimeScale) && SimulationTimeScale >= 0.01 && SimulationTimeScale <= 600.0;
}
