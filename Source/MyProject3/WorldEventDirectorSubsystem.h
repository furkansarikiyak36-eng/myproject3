#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldEventDirectorSubsystem.generated.h"

class UWorldGameDataSubsystem;

/** The signal/effect lifecycle of a deterministic natural event. */
UENUM(BlueprintType)
enum class EWorldNaturalEventPhase : uint8
{
    Signal,
    Active,
    Completed,
    Cancelled
};

/** Saved instance of one authored event definition. */
USTRUCT(BlueprintType)
struct FWorldNaturalEventInstance
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") FGuid InstanceId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") EWorldNaturalEventPhase Phase = EWorldNaturalEventPhase::Signal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double SignalStartedAtSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double StartsAtSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double EndsAtSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events", meta=(ClampMin="0", ClampMax="1")) float Severity = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") int64 TriggerDayIndex = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") int64 Revision = 1;
};

/** Complete deterministic event state; included in the world-save manifest and fixed-step rollback. */
USTRUCT(BlueprintType)
struct FWorldEventDirectorSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") int32 SchemaVersion = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") bool bInitialized = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") int64 Serial = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") int64 LastProcessedTickIndex = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") double LastProcessedTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") float PreviousSnowDepthCm = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") TMap<FName, int64> LastTriggeredDayByEvent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") TArray<FWorldNaturalEventInstance> Instances;
};

/**
 * Data-driven signal/effect state machine for natural world events (P1.3/P8.1).
 * It is stepped only by UWorldSimulationCoordinatorSubsystem; it does not tick per frame.
 * World-side effects consume the active event state, and the coordinator snapshots it for rollback.
 */
UCLASS()
class MYPROJECT3_API UWorldEventDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events|Limits", meta=(ClampMin="1", ClampMax="512"))
    int32 MaxEventInstances = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events|Limits", meta=(ClampMin="1", ClampMax="128"))
    int32 MaxNewSignalsPerStep = 8;

    UFUNCTION(BlueprintCallable, Category="World|Events")
    bool InitializeEventDirector();

    UFUNCTION(BlueprintPure, Category="World|Events")
    bool IsEventDirectorInitialized() const { return State.bInitialized; }

    UFUNCTION(BlueprintPure, Category="World|Events")
    bool CanAdvanceEventStep(double DeltaSeconds, double TargetTimeSeconds, int64 TargetTickIndex) const;

    UFUNCTION(BlueprintCallable, Category="World|Events")
    bool AdvanceEventStep(double DeltaSeconds, double TargetTimeSeconds, int64 TargetTickIndex);

    UFUNCTION(BlueprintPure, Category="World|Events")
    TArray<FWorldNaturalEventInstance> GetEventInstances() const { return State.Instances; }

    UFUNCTION(BlueprintPure, Category="World|Events")
    TArray<FWorldNaturalEventInstance> GetActiveNaturalEvents() const;

    /** Human-readable short state for HUD/debug feedback; empty when no signal is pending. */
    UFUNCTION(BlueprintPure, Category="World|Events")
    FString GetEventStatusText() const;

    /** Called after a committed simulation step; emits transition-only on-screen notices. */
    void PublishEventNotifications();

    FWorldEventDirectorSnapshot ExportSnapshot() const { return State; }
    bool RestoreSnapshot(const FWorldEventDirectorSnapshot& Snapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Events")
    FWorldEventDirectorSnapshot State;

    TSet<FGuid> PresentedSignalIds;
    TSet<FGuid> PresentedActiveIds;

    UWorldGameDataSubsystem* GetGameData() const;
    bool IsValidSnapshot(const FWorldEventDirectorSnapshot& Snapshot) const;
    bool EvaluateTrigger(const struct FGameDataWorldEvent& Definition, float& OutSeverity) const;
    void ApplyActiveEventWeather() const;
};
