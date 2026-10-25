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

    UFUNCTION(BlueprintCallable, Category="Threat|Adapter")
    bool AdvanceRaidWorldAdapter();

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
