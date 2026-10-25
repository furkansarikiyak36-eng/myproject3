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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Threat")
    int64 Revision = 0;
};

/**
 * Deterministic, bounded raid schedule. This subsystem owns planning/state only;
 * an AI/spawn adapter must materialize the raid in the world.
 */
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

    UFUNCTION(BlueprintCallable, Category="Threat")
    bool InitializeDirector(double FirstRaidDelaySeconds = 900.0);

    /** Call from the fixed-step world simulation driver, not every render frame. */
    UFUNCTION(BlueprintCallable, Category="Threat")
    bool AdvanceDirector();

    UFUNCTION(BlueprintCallable, Category="Threat")
    bool ResolveCurrentRaid(bool bRepelled);

    UFUNCTION(BlueprintCallable, Category="Threat")
    bool CancelCurrentRaid();

    UFUNCTION(BlueprintPure, Category="Threat")
    bool GetCurrentRaid(FThreatRaidRecord& OutRaid) const;

    UFUNCTION(BlueprintPure, Category="Threat")
    TArray<FThreatRaidRecord> GetRaidHistory() const { return RaidHistory; }

    UFUNCTION(BlueprintPure, Category="Threat")
    double GetNextRaidTimeSeconds() const { return NextRaidTimeSeconds; }

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

    UWorldSimulationClockSubsystem* GetClock() const;
    bool IsTerminal(EThreatRaidState State) const;
    double MakeNextInterval(int64 Serial) const;
    void FinishCurrentRaid(EThreatRaidState FinalState, double Now);
};
