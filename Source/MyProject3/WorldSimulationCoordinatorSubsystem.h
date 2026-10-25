#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.generated.h"

class UWorldSimulationClockSubsystem;
class UWorldThreatDirectorSubsystem;
class UWorldAgricultureSubsystem;
class UWorldSurvivalSubsystem;
class UWorldProductionSubsystem;
class UWorldCargoTransportSubsystem;

/**
 * Explicit orchestration boundary for deterministic world simulation.
 *
 * This subsystem does not tick itself. A game-mode/world bootstrap owns
 * initialization and a fixed-step driver calls AdvanceFixedStep. Systems that
 * are not yet wired here must not be assumed to advance automatically.
 */
UCLASS()
class MYPROJECT3_API UWorldSimulationCoordinatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Initializes the shared clock first, then the threat director. */
    UFUNCTION(BlueprintCallable, Category="World|Simulation")
    bool InitializeWorldSimulation(int32 RandomSeed, double InitialTimeSeconds = 0.0,
        double FirstRaidDelaySeconds = 900.0);

    /**
     * Preflights clock and threat state, then commits one fixed step.
     * DeltaSeconds must be the configured fixed step, not render-frame delta.
     */
    UFUNCTION(BlueprintCallable, Category="World|Simulation")
    bool AdvanceFixedStep(double DeltaSeconds);

    UFUNCTION(BlueprintPure, Category="World|Simulation")
    bool IsWorldSimulationReady() const;

    /** True after a failed rollback; callers must stop simulation and recover/reset the world. */
    UFUNCTION(BlueprintPure, Category="World|Simulation")
    bool IsSimulationFaulted() const { return bSimulationFaulted; }

    /** Human-readable explanation for a fail-closed simulation state. */
    UFUNCTION(BlueprintPure, Category="World|Simulation")
    FString GetSimulationFaultReason() const { return SimulationFaultReason; }

    UFUNCTION(BlueprintPure, Category="World|Simulation")
    int64 GetCompletedCoordinatorSteps() const { return CompletedCoordinatorSteps; }

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Simulation", meta=(AllowPrivateAccess="true"))
    int64 CompletedCoordinatorSteps = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Simulation", meta=(AllowPrivateAccess="true"))
    bool bSimulationFaulted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Simulation", meta=(AllowPrivateAccess="true"))
    FString SimulationFaultReason;

    UWorldSimulationClockSubsystem* GetClock() const;
    UWorldThreatDirectorSubsystem* GetThreatDirector() const;
    UWorldAgricultureSubsystem* GetAgriculture() const;
    UWorldSurvivalSubsystem* GetSurvival() const;
    UWorldProductionSubsystem* GetProduction() const;
    UWorldCargoTransportSubsystem* GetCargo() const;
};
