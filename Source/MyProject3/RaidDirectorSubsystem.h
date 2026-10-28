#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayCoreTypes.h"
#include "RaidDirectorTypes.h"
#include "RaidDirectorSubsystem.generated.h"

/** Planning ledger only: actor spawning, NavMesh, combat and structural mutation are adapters. */
UCLASS()
class MYPROJECT3_API URaidDirectorSubsystem : public UWorldSubsystem {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Raid|Configuration") FWorldOperationResult ConfigureDirector(const FRaidDirectorConfig& InConfig, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Threat") FWorldOperationResult EvaluateThreat(const FRaidThreatContext& Context, float& OutDifficulty, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Threat") FWorldOperationResult ScheduleRaid(const FRaidThreatContext& Context, int64 EarliestWorldTick, int32 RequestedWaves, FGuid& OutRaidId, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Planning") FWorldOperationResult BuildWavePlans(FGuid RaidId, int32 RequestedEnemiesPerWave, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Spawn") FWorldOperationResult ChooseSpawnCandidate(FGuid RaidId, const TArray<FRaidSpawnCandidate>& Candidates, FRaidSpawnCandidate& OutCandidate, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Targeting") FWorldOperationResult SelectTarget(FGuid RaidId, const TArray<FRaidTargetCandidate>& Candidates, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Waves") FWorldOperationResult BeginNextWave(FGuid RaidId, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Waves") FWorldOperationResult RecordSpawnedEnemies(FGuid RaidId, int32 Count, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Waves") FWorldOperationResult ResolveEnemies(FGuid RaidId, int32 Count, FGuid OperationId);
 UFUNCTION(BlueprintCallable, Category="Raid|Waves") FWorldOperationResult CancelRaid(FGuid RaidId, FGuid OperationId);
 UFUNCTION(BlueprintPure, Category="Raid|State") bool GetRaidState(FGuid RaidId, FRaidRuntimeState& OutState) const;
 UFUNCTION(BlueprintPure, Category="Raid|State") TArray<FRaidWavePlan> GetWavePlans(FGuid RaidId) const;
 UFUNCTION(BlueprintPure, Category="Raid|State") FRaidDirectorSnapshot GetSnapshot() const;
private:
 UPROPERTY(Transient) FRaidDirectorConfig Config;
 UPROPERTY(Transient) TMap<FGuid, FRaidRuntimeState> Raids;
 UPROPERTY(Transient) TMap<FGuid, TArray<FRaidWavePlan>> WavePlans;
 UPROPERTY(Transient) TSet<FGuid> ProcessedOperationIds;
 UPROPERTY(Transient) int64 LastEvaluatedWorldTick=INDEX_NONE;
 UPROPERTY(Transient) int32 QueuedThreatCount=0;
 UPROPERTY(Transient) int32 AliveThreatCount=0;
 UPROPERTY(Transient) FRandomStream DeterministicRandom;
 FWorldOperationResult Failure(EWorldOperationResultCode Code, const FGuid& OperationId, const TCHAR* Message) const;
 FWorldOperationResult ValidateNewOperation(const FGuid& OperationId) const;
 void RememberOperation(const FGuid& OperationId);
};