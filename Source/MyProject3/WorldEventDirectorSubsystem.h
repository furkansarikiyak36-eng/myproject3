#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldEventDirectorSubsystem.generated.h"

UENUM(BlueprintType)
enum class EWorldNaturalEventState : uint8
{
    Waiting,
    Warning,
    Active,
    Ended
};

USTRUCT(BlueprintType)
struct FWorldNaturalEventDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events") FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events") FString DisplayName;
    /** Effect adapter key; actual terrain/health/structure mutation stays domain-owned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events") FName EffectId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events") FName TriggerAfterEventId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events", meta=(ClampMin="0.1")) double WarningDurationSeconds = 180.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events", meta=(ClampMin="0.1")) double ActiveDurationSeconds = 600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events", meta=(ClampMin="0")) double CooldownSeconds = 1800.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events", meta=(ClampMin="1")) double MinimumIntervalSeconds = 18000.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events", meta=(ClampMin="1")) double MaximumIntervalSeconds = 36000.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events", meta=(ClampMin="0", ClampMax="1")) float TriggerChance = 0.25f;
    /** Empty means year-round. Otherwise values must be month numbers 1..12. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events") TArray<int32> AllowedMonths;
};

USTRUCT(BlueprintType)
struct FWorldNaturalEventRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") FGuid EventInstanceId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") FName TriggeredByDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") EWorldNaturalEventState State = EWorldNaturalEventState::Waiting;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") int64 Serial = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") int64 AttemptCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") int64 LastCompletedSerial = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") int64 LastObservedPredecessorSerial = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double ScheduledStartTimeSeconds = -1.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double WarningStartTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double ActiveStartTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double PlannedEndTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") double LastCompletedEndTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events") int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct FWorldEventDirectorSnapshot
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") int32 SchemaVersion = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") bool bInitialized = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") int32 RandomSeed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") double LastProcessedTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") int64 StateRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") TArray<FWorldNaturalEventRecord> RuntimeRecords;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Events|Save") TArray<FWorldNaturalEventRecord> CompletedHistory;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldNaturalEventTransition, const FWorldNaturalEventRecord&, EventRecord);

/** Deterministic event scheduling. The domain owner applies the effect identified by EffectId. */
UCLASS()
class MYPROJECT3_API UWorldEventDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 CurrentSchemaVersion = 1;
    UPROPERTY(BlueprintAssignable, Category="World|Events") FOnWorldNaturalEventTransition OnEventTransition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events|Limits", meta=(ClampMin="1", ClampMax="128")) int32 MaxDefinitions = 32;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events|Limits", meta=(ClampMin="1", ClampMax="4096")) int32 MaxHistoryEntries = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Events|Limits", meta=(ClampMin="1", ClampMax="8192")) int32 MaxTransitionsPerStep = 512;

    UFUNCTION(BlueprintCallable, Category="World|Events") bool RegisterEventDefinition(const FWorldNaturalEventDefinition& Definition);
    UFUNCTION(BlueprintPure, Category="World|Events") bool ValidateDefinitions() const;
    UFUNCTION(BlueprintCallable, Category="World|Events") bool InitializeDirector(int32 InRandomSeed, double InitialTimeSeconds);
    UFUNCTION(BlueprintPure, Category="World|Events") bool IsDirectorInitialized() const { return bInitialized; }
    UFUNCTION(BlueprintPure, Category="World|Events") bool CanAdvanceToTime(double TargetTimeSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Events") bool AdvanceToTime(double TargetTimeSeconds);
    /** Call only after the coordinator commits all participants in the fixed step. */
    UFUNCTION(BlueprintCallable, Category="World|Events") void PublishPendingTransitions();
    UFUNCTION(BlueprintPure, Category="World|Events") TArray<FWorldNaturalEventRecord> GetCurrentEvents() const;
    UFUNCTION(BlueprintPure, Category="World|Events") TArray<FString> GetCurrentEventSummaries() const;
    UFUNCTION(BlueprintPure, Category="World|Events") FWorldEventDirectorSnapshot ExportSnapshot() const;
    UFUNCTION(BlueprintCallable, Category="World|Events") bool RestoreSnapshot(const FWorldEventDirectorSnapshot& Snapshot);

private:
    UPROPERTY(VisibleAnywhere, Category="World|Events") TArray<FWorldNaturalEventDefinition> Definitions;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Events") TArray<FWorldNaturalEventRecord> RuntimeRecords;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Events") TArray<FWorldNaturalEventRecord> CompletedHistory;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Events") int32 RandomSeed = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Events") double LastProcessedTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Events") int64 StateRevision = 0;
    TArray<FWorldNaturalEventRecord> PendingTransitions;
    bool bInitialized = false;
    const FWorldNaturalEventDefinition* FindDefinition(FName DefinitionId) const;
    FWorldNaturalEventRecord* FindRuntimeRecord(FName DefinitionId);
    const FWorldNaturalEventRecord* FindRuntimeRecord(FName DefinitionId) const;
    double SampleInterval(const FWorldNaturalEventDefinition& Definition, int64 Serial) const;
    float SampleChance(FName DefinitionId, int64 AttemptSerial) const;
    FGuid MakeEventInstanceId(FName DefinitionId, int64 Serial) const;
    bool IsMonthAllowed(const FWorldNaturalEventDefinition& Definition, double AtTime) const;
    bool ScheduleChainedEvents();
};
