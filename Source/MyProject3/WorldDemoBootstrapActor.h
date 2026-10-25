#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldDemoBootstrapActor.generated.h"

class UWorldSimulationCoordinatorSubsystem;

/**
 * Explicit level-placed bootstrap/driver for the integrated demo.
 *
 * WorldSimulationCoordinatorSubsystem intentionally does not tick itself.
 * Place one instance in the demo level to initialize the shared simulation
 * and advance it using a fixed-step accumulator. Do not place duplicates.
 */
UCLASS(Blueprintable)
class MYPROJECT3_API AWorldDemoBootstrapActor : public AActor
{
    GENERATED_BODY()

public:
    AWorldDemoBootstrapActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Initializes the world simulation if needed, then starts fixed-step advancement. */
    UFUNCTION(BlueprintCallable, Category="Demo|Simulation")
    bool StartSimulation();

    /** Stops this actor from advancing the shared simulation; it does not reset world state. */
    UFUNCTION(BlueprintCallable, Category="Demo|Simulation")
    void StopSimulation();

    UFUNCTION(BlueprintPure, Category="Demo|Simulation")
    bool IsDriverRunning() const { return bDriverRunning; }

    UFUNCTION(BlueprintPure, Category="Demo|Simulation")
    double GetAccumulatedSimulationSeconds() const { return AccumulatorSeconds; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Simulation")
    int32 RandomSeed = 2601026;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Simulation", meta=(ClampMin="0.0"))
    double InitialTimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Simulation", meta=(ClampMin="0.0"))
    double FirstRaidDelaySeconds = 900.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Simulation", meta=(ClampMin="0.01", ClampMax="1.0"))
    double FixedStepSeconds = 0.25;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Simulation", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxStepsPerFrame = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Simulation")
    bool bAutoStartOnBeginPlay = true;

private:
    UPROPERTY(Transient)
    TObjectPtr<UWorldSimulationCoordinatorSubsystem> Coordinator;

    double AccumulatorSeconds = 0.0;
    bool bDriverRunning = false;

    bool ValidateConfiguration() const;
};
