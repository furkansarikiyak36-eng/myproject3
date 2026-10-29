#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldRaidAdapterSubsystem.generated.h"

class APawn;
struct FThreatRaidRecord;
class UWorldThreatDirectorSubsystem;
class UWorldStructuralGraphSubsystem;

/** Optional world-facing adapter. The configured pawn class must provide its own AI controller/behavior. */
UCLASS()
class MYPROJECT3_API UWorldRaidAdapterSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UWorldRaidAdapterSubsystem();

    UFUNCTION(BlueprintPure, Category="Threat|Adapter")
    bool CanAdvanceRaidWorldAdapterAtTime(double TargetTimeSeconds) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter")
    TSubclassOf<APawn> RaidPawnClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter", meta=(ClampMin="0.0"))
    float SpawnHeightOffset = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter", meta=(ClampMin="0.1"))
    float DamagePerPulse = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter", meta=(ClampMin="1.0"))
    double AttackIntervalSeconds = 30.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter")
    TArray<FGuid> DefenseTargetNodeIds;

    /** Number of visible raiders to materialize for a warning/active raid. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter|Wave", meta=(ClampMin="1", ClampMax="12"))
    int32 RaidWaveSize = 3;

    /** Deterministic radial offset from the target for each arriving raider. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter|Wave", meta=(ClampMin="0.0", ClampMax="20000.0"))
    float SpawnDistanceFromTargetCm = 1800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter|Wave", meta=(ClampMin="10.0", ClampMax="2000.0"))
    float RaidTargetAcceptanceRadiusCm = 250.0f;

    UFUNCTION(BlueprintCallable, Category="Threat|Adapter")
    bool AdvanceRaidWorldAdapter();

    UFUNCTION(BlueprintPure, Category="Threat|Adapter")
    FGuid GetMaterializedRaidId() const { return MaterializedRaidId; }

    UFUNCTION(BlueprintPure, Category="Threat|Adapter")
    bool IsRaidPawnMaterialized() const { return HasActiveRaidPawns(); }

private:
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<APawn>> ActiveRaidPawns;

    UPROPERTY(Transient)
    FGuid MaterializedRaidId;

    UPROPERTY(Transient)
    FGuid MaterializedTargetNodeId;

    UPROPERTY(Transient)
    FVector MaterializedRaidTargetLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    bool bHasMaterializedRaidTargetLocation = false;

    /** Cached boundary only; pulse identity is derived from raid ID + pulse index and stored in the operation ledger. */
    UPROPERTY(Transient)
    double NextDamageTimeSeconds = 0.0;

    bool HasActiveRaidPawns() const;
    bool HasRaidPawnReachedTarget() const;
    UWorldThreatDirectorSubsystem* GetThreatDirector() const;
    UWorldStructuralGraphSubsystem* GetStructuralGraph() const;
    bool ResolvePrimaryTargetNode(const FThreatRaidRecord& Raid, FGuid& OutNodeId, FVector& OutLocation) const;
    bool BuildDamageTargetList(const FThreatRaidRecord& Raid, TArray<FGuid>& OutTargetIds) const;
    bool ResolveCollapsedSubtree(FGuid RootNodeId);
};
