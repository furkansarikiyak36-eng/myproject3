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

/** An additional load-bearing contact besides the primary parent edge. */
USTRUCT(BlueprintType)
struct FStructuralSupportLink
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure")
    FGuid SupportNodeId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure")
    FName SupportAnchorId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure")
    FName OwnAnchorId = NAME_None;
};

USTRUCT()
struct FWorldStructuralNodeSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    TArray<FStructuralSupportLink> SecondarySupports;

    UPROPERTY(SaveGame)
    FGuid NodeId;

    UPROPERTY(SaveGame)
    FGuid ActorIdentityId;

    UPROPERTY(SaveGame)
    EStructuralNodeKind Kind = EStructuralNodeKind::Unknown;

    UPROPERTY(SaveGame)
    FGuid ParentNodeId;

    UPROPERTY(SaveGame)
    FName ParentAnchorId = NAME_None;

    UPROPERTY(SaveGame)
    FName OwnAnchorId = NAME_None;

    UPROPERTY(SaveGame)
    float LocalLoad = 0.0f;

    UPROPERTY(SaveGame)
    float MaxSupportedLoad = 100000.0f;

    UPROPERTY(SaveGame)
    bool bGroundSupported = false;

    /** A child node whose second physical contact rests directly on ground. */
    UPROPERTY(SaveGame)
    bool bHasGroundContact = false;

    UPROPERTY(SaveGame)
    float Damage = 0.0f;

    UPROPERTY(SaveGame)
    float DamageThreshold = 100.0f;

    UPROPERTY(SaveGame)
    int64 Revision = 0;
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

    /** Extra load-bearing contacts; the node stands while any support reaches ground. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    TArray<FStructuralSupportLink> SecondarySupports;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    float LocalLoad = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    float MaxSupportedLoad = 100000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    bool bGroundSupported = false;

    /** Direct ground contact at an additional support point; valid for a node with a primary parent. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure")
    bool bHasGroundContact = false;

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
 * Log, stone, and stick actors register themselves and synchronize attachment edges through this subsystem.
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

    /** Adds a load-bearing contact to a node that already has a primary parent. */
    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool TryAddSecondarySupport(FGuid ChildNodeId, FGuid SupportNodeId,
        FName SupportAnchorId, FName OwnAnchorId);

    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool TryRemoveSecondarySupport(FGuid ChildNodeId, FGuid SupportNodeId);

    /** Primary children and nodes that use this node as a secondary support. */
    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    TArray<FGuid> GetAllDependents(FGuid NodeId) const;

    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool SetGroundSupported(FGuid NodeId, bool bGroundSupported);

    /** Records a second-end ground contact for a child node; unlike root support this may coexist with a parent edge. */
    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool SetGroundContact(FGuid NodeId, bool bHasGroundContact);

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    bool HasValidSupportPath(FGuid NodeId) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    bool CanSafelyRemove(FGuid NodeId) const;

    /** Returns a deterministic leaf-first removal order for a subtree; does not mutate actors or graph. */
    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    bool BuildDependentRemovalOrder(FGuid RootNodeId, TArray<FGuid>& OutLeafFirstOrder) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    float GetAggregateLoad(FGuid NodeId) const;

    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool ApplyDamage(FGuid NodeId, float DamageAmount, bool& bOutThresholdExceeded);

    /** Takes up to Amount of damage off a piece; false when it had none. */
    UFUNCTION(BlueprintCallable, Category="Structure|Graph")
    bool RepairDamage(FGuid NodeId, float Amount);

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    bool GetNode(FGuid NodeId, FStructuralNodeRecord& OutNode) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    TArray<FGuid> GetDirectDependents(FGuid NodeId) const;

    UFUNCTION(BlueprintPure, Category="Structure|Graph")
    int32 GetNodeCount() const { return Nodes.Num(); }

    bool ExportSnapshot(TArray<FWorldStructuralNodeSnapshot>& OutSnapshot) const;
    bool RestoreSnapshot(const TArray<FWorldStructuralNodeSnapshot>& Snapshot);
    /** Rehydrates actor-local connections and anchor occupancy from canonical graph edges. */
    bool RebuildActorAttachmentsFromSnapshot();

private:
    UPROPERTY(Transient)
    TMap<FGuid, FStructuralNodeRecord> Nodes;

    bool WouldCreateCycle(FGuid ParentNodeId, FGuid ChildNodeId) const;
    /** Support search over primary and secondary links, optionally ignoring one node. */
    bool HasValidSupportPathInternal(FGuid NodeId, FGuid ExcludedNodeId, TSet<FGuid>& Visiting) const;
    bool IsAnchorInUse(FGuid SupportNodeId, FName SupportAnchorId) const;
    bool CollectDependentsDepthFirst(FGuid NodeId, TSet<FGuid>& Visited,
        TArray<FGuid>& OutOrder) const;
    float ComputeAggregateLoad(FGuid NodeId, TSet<FGuid>& Visiting,
        bool& bOutValid) const;
};
