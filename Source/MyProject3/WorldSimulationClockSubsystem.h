#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldSimulationClockSubsystem.generated.h"

/**
 * Serializable snapshot for the deterministic simulation clock.
 * A save coordinator must explicitly export and persist this record.
 */
USTRUCT(BlueprintType)
struct FWorldSimulationClockSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Clock")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Clock")
    double SimulationTimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Clock")
    int64 TickIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Clock")
    int32 RandomSeed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World|Clock")
    int64 StateRevision = 0;
};

/**
 * Explicit deterministic simulation time/seed owner.
 *
 * This subsystem deliberately does not advance itself every frame. The future
 * simulation driver must call AdvanceSimulation with fixed/bounded steps, and
 * the save coordinator must explicitly persist ExportSnapshot().
 */
UCLASS()
class MYPROJECT3_API UWorldSimulationClockSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchemaVersion = 1;

    UFUNCTION(BlueprintCallable, Category = "World|Clock")
    bool InitializeClock(int32 InRandomSeed, double InitialSimulationTimeSeconds = 0.0);

    /** Advances logical simulation time; rejects negative, NaN and infinite deltas. */
    UFUNCTION(BlueprintCallable, Category = "World|Clock")
    bool AdvanceSimulation(double DeltaSeconds);

    UFUNCTION(BlueprintPure, Category = "World|Clock")
    double GetSimulationTimeSeconds() const { return SimulationTimeSeconds; }

    UFUNCTION(BlueprintPure, Category = "World|Clock")
    int64 GetTickIndex() const { return TickIndex; }

    UFUNCTION(BlueprintPure, Category = "World|Clock")
    int32 GetRandomSeed() const { return RandomSeed; }

    UFUNCTION(BlueprintPure, Category = "World|Clock")
    int64 GetStateRevision() const { return StateRevision; }

    UFUNCTION(BlueprintPure, Category = "World|Clock")
    FWorldSimulationClockSnapshot ExportSnapshot() const;

    UFUNCTION(BlueprintCallable, Category = "World|Clock")
    bool RestoreSnapshot(const FWorldSimulationClockSnapshot& Snapshot);

    /** Stable per-operation seed, independent of operation call ordering. */
    UFUNCTION(BlueprintPure, Category = "World|Clock")
    int32 GetDeterministicSeedForOperation(const FGuid& OperationId) const;

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category = "World|Clock")
    double SimulationTimeSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, SaveGame, Category = "World|Clock")
    int64 TickIndex = 0;

    UPROPERTY(VisibleAnywhere, SaveGame, Category = "World|Clock")
    int32 RandomSeed = 1337;

    UPROPERTY(VisibleAnywhere, SaveGame, Category = "World|Clock")
    int64 StateRevision = 0;

    bool bInitialized = false;
};
