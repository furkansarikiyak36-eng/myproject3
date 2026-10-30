#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldRaidAdapterSubsystem.generated.h"

class APawn;
class UWorldThreatDirectorSubsystem;
class UWorldStructuralGraphSubsystem;

/** Optional world-facing adapter. The configured pawn class must provide its own AI controller/behavior. */
UCLASS()
class MYPROJECT3_API UWorldRaidAdapterSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DamageReductionPerDefenseRating = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Threat|Adapter", meta=(ClampMin="0.01", ClampMax="1.0"))
    float MinimumRaidDamageFraction = 0.10f;

    UFUNCTION(BlueprintCallable, Category="Threat|Adapter")
    bool AdvanceRaidWorldAdapter();

    /** Apply regional event structural damage for one committed fixed simulation step. */
    UFUNCTION(BlueprintCallable, Category="World|Events")
    bool AdvanceRegionalHazardDamage(double DeltaSeconds);

    /** Sample region-tagged actors and submit their storage/smell/light/smoke signals. */
    UFUNCTION(BlueprintCallable, Category="Threat|Signals")
    bool AdvanceRegionalRaidSignals();

    /** Consume one unique resource/building-part item to repair a graph node once. */
    UFUNCTION(BlueprintCallable, Category="Structure|Maintenance")
    bool TryRepairStructureWithMaterial(const FGuid& OperationId, const FGuid& NodeId,
        int64 ExpectedNodeRevision, const FGuid& MaterialItemId, int64 ExpectedItemRevision,
        const FGuid& InventoryOwnerId, float RepairAmount);

    UFUNCTION(BlueprintPure, Category="Threat|Adapter")
    FGuid GetMaterializedRaidId() const { return MaterializedRaidId; }

    UFUNCTION(BlueprintPure, Category="Threat|Adapter")
    bool IsRaidPawnMaterialized() const { return ActiveRaidPawn.IsValid(); }

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<APawn> ActiveRaidPawn;

    UPROPERTY(Transient)
    FGuid MaterializedRaidId;

    UPROPERTY(Transient)
    double NextDamageTimeSeconds = 0.0;

    UWorldThreatDirectorSubsystem* GetThreatDirector() const;
    UWorldStructuralGraphSubsystem* GetStructuralGraph() const;
    bool ResolveCollapsedSubtree(FGuid RootNodeId);
};
