#pragma once
#include "CoreMinimal.h"
#include "RaidDirectorTypes.generated.h"

UENUM(BlueprintType)
enum class ERaidPhase : uint8 { Idle, Scheduled, Preparing, Active, Recovering, Completed, Cancelled, Failed };

USTRUCT(BlueprintType)
struct FRaidDirectorConfig {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="1")) int32 MaxQueuedThreats=3;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="1")) int32 MaxAliveThreats=12;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="0.0")) float BaseDifficulty=1.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="0.0")) float MaxDifficulty=5.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="1")) int32 DefaultWaveCount=3;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="1")) int32 BaseEnemiesPerWave=2;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="0.0")) float NoiseWeight=0.25f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="0.0")) float StockWeight=0.25f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid", meta=(ClampMin="0.0")) float DefenseWeight=0.5f;
};

USTRUCT(BlueprintType)
struct FRaidThreatContext {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") float PlayerProgress=0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") float SettlementNoise=0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") float ValuableStock=0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") float DefenseRating=0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") int64 WorldTick=0;
};

USTRUCT(BlueprintType)
struct FRaidSpawnCandidate {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") FGuid CandidateId;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") FVector Location=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") bool bIsValid=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") bool bVisibleToPlayer=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") bool bHasNavigation=true;
};

USTRUCT(BlueprintType)
struct FRaidTargetCandidate {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") FGuid TargetId;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") FVector Location=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") float StrategicValue=1.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") bool bValid=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid") bool bProtectedFromRaid=false;
};

USTRUCT(BlueprintType)
struct FRaidWavePlan {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 WaveIndex=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 PlannedEnemies=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") float Difficulty=0.0f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 SpawnedEnemies=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 ResolvedEnemies=0;
};

USTRUCT(BlueprintType)
struct FRaidRuntimeState {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") FGuid RaidId;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") ERaidPhase Phase=ERaidPhase::Idle;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 CurrentWaveIndex=INDEX_NONE;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 QueuedEnemyCount=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 AliveEnemyCount=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") float Difficulty=0.0f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") FGuid SelectedTargetId;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") FVector LastSpawnLocation=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int64 Revision=0;
};

USTRUCT(BlueprintType)
struct FRaidDirectorSnapshot {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int64 LastEvaluatedWorldTick=INDEX_NONE;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 ActiveRaidCount=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 QueuedThreatCount=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid") int32 AliveThreatCount=0;
};