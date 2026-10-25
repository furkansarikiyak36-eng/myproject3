#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldStructuralGraphSubsystem.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EStructuralNodeKind : uint8
{
    Unknown,
    Wood,
    Stone,
    Stick,
    Foundation,
    TerrainAnchor,
    Other
};

USTRUCT(BlueprintType)
struct FStructuralNodeRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    FGuid NodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    TWeakObjectPtr<AActor> Actor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    EStructuralNodeKind Kind = EStructuralNodeKind::Unknown;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    FGuid ParentNodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    FName ParentAnchorId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    FName OwnAnchorId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    float LocalLoad = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    float MaxSupportedLoad = 100000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    bool bGroundSupported = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    float Damage = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    float DamageThreshold = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    int64 Revision = 0;
};

/**
 * World-local canonical graph for cross-domain construction. Domain actors keep
 * their geometry and anchor definitions; this subsystem owns graph relations,
 * support-path checks and aggregate-load validation.
 *
 * Existing log/stone/stick actors are not automatically migrated by this class.
 */
UCLASS()
class MYPROJECT3_API UWorldStructuralGraphSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool RegisterNode(AActor* Actor, FGuid NodeId, EStructuralNodeKind Kind,
        bool bGroundSupported, float LocalLoad, float MaxSupportedLoad,
        float DamageThreshold);

    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool UnregisterNode(FGuid NodeId, bool bRequireNoDependents = true);

    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool TryConnect(FGuid ParentNodeId, FName ParentAnchorId,
        FGuid ChildNodeId, FName OwnAnchorId);

    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool TryDisconnect(FGuid ChildNodeId);

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    bool HasValidSupportPath(FGuid NodeId) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    bool CanSafelyRemove(FGuid NodeId) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    float GetAggregateLoad(FGuid NodeId) const;

    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool ApplyDamage(FGuid NodeId, float DamageAmount, bool& bOutThresholdExceeded);

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    bool GetNode(FGuid NodeId, FStructuralNodeRecord& OutNode) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    TArray<FGuid> GetDirectDependents(FGuid NodeId) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    int32 GetNodeCount() const { return Nodes.Num(); }

private:
    UPROPERTY(Transient)
    TMap<FGuid, FStructuralNodeRecord> Nodes;

    bool WouldCreateCycle(FGuid ParentNodeId, FGuid ChildNodeId) const;
    bool CollectDependentsDepthFirst(FGuid NodeId, TSet<FGuid>& Visited,
        TArray<FGuid>& OutOrder) const;
    float ComputeAggregateLoad(FGuid NodeId, TSet<FGuid>& Visiting,
        bool& bOutValid) const;
};
