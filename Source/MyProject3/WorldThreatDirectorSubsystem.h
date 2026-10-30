#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldThreatDirectorSubsystem.generated.h"

class UWorldSimulationClockSubsystem;

UENUM(BlueprintType)
enum class EThreatRaidState : uint8
{
    None,
    Warning,
    Active,
    Repelled,
    Failed,
    Cancelled
};

USTRUCT(BlueprintType)
struct FThreatRaidRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    FGuid RaidId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    int64 Serial = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    EThreatRaidState State = EThreatRaidState::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    double WarningStartTime = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    double ActiveStartTime = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    double PlannedEndTime = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    float Pressure = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    FVector TargetLocation = FVector::ZeroVector;

    /** Authored region selected for this encounter; NAME_None is allowed for legacy saves. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat|Region")
    FName TargetRegionId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    int64 Revision = 0;

    /** Last raid damage pulse claimed by the world adapter; -1 means none was committed. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat|Damage")
    int64 LastAppliedDamagePulseIndex = -1;
};

/**
 * Deterministic, bounded raid schedule. This subsystem owns planning/state only;
 * an AI/spawn adapter must materialize the raid in the world.
 */

USTRUCT(BlueprintType)
struct FWorldThreatDirectorSnapshot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") int32 SchemaVersion = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") FThreatRaidRecord CurrentRaid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") TArray<FThreatRaidRecord> RaidHistory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") double NextRaidTimeSeconds = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") int64 RaidSerial = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") bool bInitialized = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") TArray<FGuid> SeenSignalOperationIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Threat|Save") double LastSignalRaidTimeSeconds = -1.0;
};

UCLASS()
class MYPROJECT3_API UWorldThreatDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Tuning", meta=(ClampMin="60.0"))
    double MinimumIntervalSeconds = 1800.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Tuning", meta=(ClampMin="60.0"))
    double MaximumIntervalSeconds = 3600.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Tuning", meta=(ClampMin="1.0"))
    double WarningDurationSeconds = 180.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Tuning", meta=(ClampMin="1.0"))
    double RaidDurationSeconds = 600.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Tuning", meta=(ClampMin="1"))
    int32 MaxHistoryEntries = 64;

    /** Weighted normalized storage-value, smell, light and smoke score needed to provoke a raid. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Signals", meta=(ClampMin="0.1"))
    float SignalRaidThreshold = 1.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Signals", meta=(ClampMin="0.0"))
    double SignalRaidCooldownSeconds = 3600.0;

    UFUNCTION(BlueprintCallable, Category="Threat")
    bool InitializeDirector(double FirstRaidDelaySeconds = 900.0);

    UFUNCTION(BlueprintPure, Category="Threat")
    bool IsDirectorInitialized() const { return bInitialized; }

    /** Pure preflight for a coordinator-owned fixed step. */
    UFUNCTION(BlueprintPure, Category="Threat")
    bool CanAdvanceDirectorAtTime(double TargetTimeSeconds, int64 TargetTickIndex) const;

    /** Commit one director step against the prevalidated target time/tick. */
    UFUNCTION(BlueprintCallable, Category="Threat")
    bool AdvanceDirectorAtTime(double TargetTimeSeconds, int64 TargetTickIndex);

    /** Call from the fixed-step world simulation driver, not every render frame. */
    UFUNCTION(BlueprintCallable, Category="Threat")
    bool AdvanceDirector();

    /** Accepts normalized 0..1 signals from world-owned store/smell/light/smoke sensors. */
    UFUNCTION(BlueprintCallable, Category="Threat|Signals")
    bool TryStartRaidFromSignals(const FGuid& OperationId, FName RegionId,
        float StorageValueSignal, float SmellSignal, float LightSignal, float SmokeSignal,
        bool& bOutRaidStarted);

    UFUNCTION(BlueprintCallable, Category="Threat")
    bool ResolveCurrentRaid(bool bRepelled);

    /** Claims a deterministic pulse once; the claimed index persists with the raid record. */
    bool TryClaimDamagePulse(const FGuid& RaidId, int64 PulseIndex);

    UFUNCTION(BlueprintCallable, Category="Threat")
    bool CancelCurrentRaid();

    UFUNCTION(BlueprintPure, Category="Threat")
    bool GetCurrentRaid(FThreatRaidRecord& OutRaid) const;

    UFUNCTION(BlueprintPure, Category="Threat")
    TArray<FThreatRaidRecord> GetRaidHistory() const { return RaidHistory; }

    UFUNCTION(BlueprintPure, Category="Threat")
    double GetNextRaidTimeSeconds() const { return NextRaidTimeSeconds; }

    FWorldThreatDirectorSnapshot ExportSnapshot() const;
    bool RestoreSnapshot(const FWorldThreatDirectorSnapshot& Snapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="Threat")
    FThreatRaidRecord CurrentRaid;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="Threat")
    TArray<FThreatRaidRecord> RaidHistory;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="Threat")
    double NextRaidTimeSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="Threat")
    int64 RaidSerial = 0;

    bool bInitialized = false;
    TArray<FGuid> SeenSignalOperationIds;
    double LastSignalRaidTimeSeconds = -1.0;

    void RememberSignalOperation(const FGuid& OperationId);
    UWorldSimulationClockSubsystem* GetClock() const;
    bool IsTerminal(EThreatRaidState State) const;
    double MakeNextInterval(int64 Serial) const;
    void FinishCurrentRaid(EThreatRaidState FinalState, double Now);
};
