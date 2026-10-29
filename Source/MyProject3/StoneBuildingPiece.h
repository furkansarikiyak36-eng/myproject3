#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StoneBuildingTypes.h"
#include "StructuralPieceInterface.h"
#include "StoneBuildingPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UWallCoatingComponent;
class UWorldObjectIdentityComponent;

/**
 * Domain owner for one physical stone piece. Anchors and connection state are
 * explicit; mesh overlap alone never creates a structural connection.
 */
UCLASS(Blueprintable)
class MYPROJECT3_API AStoneBuildingPiece : public AActor, public IStructuralPieceInterface
{
    GENERATED_BODY()

public:
    AStoneBuildingPiece();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Registers a newly materialized actor after ledger ownership becomes World. */
    bool ActivateAfterOwnershipTransfer();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stone")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stone|Structure")
    TObjectPtr<UWallCoatingComponent> WallCoating;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stone|Structure")
    TObjectPtr<UWorldObjectIdentityComponent> StructuralIdentity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone")
    EStonePieceType PieceType = EStonePieceType::RawStone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone")
    bool bCanBeCarried = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone")
    bool bStartsPlaced = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Anchors")
    TArray<FStoneSnapAnchor> Anchors;

    UFUNCTION(BlueprintPure, Category = "Stone")
    bool IsPlaced() const { return bIsPlaced; }

    UFUNCTION(BlueprintPure, Category = "Stone")
    bool IsHeld() const { return bIsHeld; }

    UFUNCTION(BlueprintPure, Category = "Stone")
    bool HasStructuralParent() const { return ParentPiece.IsValid(); }

    UFUNCTION(BlueprintCallable, Category = "Stone")
    bool SetHeldState(bool bNewHeld);

    UFUNCTION(BlueprintCallable, Category = "Stone")
    bool TryCommitGroundPlacement(const FVector& SurfaceLocation, const FRotator& SurfaceRotation);

    UFUNCTION(BlueprintCallable, Category = "Stone|Anchors")
    bool TryConnectToAnchor(AStoneBuildingPiece* TargetPiece,
        FName TargetAnchorId, FName OwnAnchorId);

    UFUNCTION(BlueprintCallable, Category = "Stone|Anchors")
    bool CanConnectToAnchor(const AStoneBuildingPiece* TargetPiece,
        FName TargetAnchorId, FName OwnAnchorId) const;

    UFUNCTION(BlueprintCallable, Category = "Stone|Anchors")
    bool AddCustomAnchor(FName AnchorId, FName CompatibilityTag,
        FTransform LocalTransform, bool bCanReceive, bool bCanConnect);

    UFUNCTION(BlueprintCallable, Category = "Stone|Anchors")
    /** Keeps actor anchors and the canonical graph edge synchronized. */
    bool RemoveConnection(bool bAllowGraphFailure = false);

    UFUNCTION(BlueprintPure, Category = "Stone|Anchors")
    FTransform GetAnchorWorldTransform(FName AnchorId) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Interaction")
    bool bGenerateMovingLeaf = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Interaction", meta=(ClampMin="10.0", ClampMax="150.0"))
    float DoorOpenYawDegrees = 95.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Traversal", meta=(ClampMin="50.0"))
    float MaxSupportSpanCm = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Traversal", meta=(ClampMin="1.0"))
    float MaxConcurrentTraversalWeightKg = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Traversal", meta=(ClampMin="100.0"))
    float MaxRopeSpanCm = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Traversal", meta=(ClampMin="1.0"))
    float MaxRopeTravelerWeightKg = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Interaction")
    bool bStartsOpen = false;

    UFUNCTION(BlueprintCallable, Category="Structure|Interaction")
    bool TrySetOpen(bool bOpen);

    UFUNCTION(BlueprintPure, Category="Structure|Interaction")
    bool IsOpen() const { return bIsOpen; }

    /** True only where a full actor-sized passage is clear, not merely because a window shutter is open. */
    UFUNCTION(BlueprintPure, Category="Structure|Interaction")
    bool IsPassageClear() const;

    /** True when this authored structure blocks a raid's passage. */
    UFUNCTION(BlueprintPure, Category="Structure|Raid")
    bool IsRaidBlocking() const;

    /** Validates the two-ended bridge/walkway graph before permitting a traversal reservation. */
    UFUNCTION(BlueprintPure, Category="Structure|Traversal")
    bool CanCarryTraversalLoad(float WeightKg) const;

    UFUNCTION(BlueprintCallable, Category="Structure|Traversal")
    bool TryReserveBridgeTraversal(FGuid TraversalId, float WeightKg);

    UFUNCTION(BlueprintCallable, Category="Structure|Traversal")
    bool ReleaseBridgeTraversal(FGuid TraversalId);

    /** Makes a visible, anchor-to-anchor rope between two RopeCrossingAnchor pieces. */
    UFUNCTION(BlueprintCallable, Category="Structure|Traversal")
    bool TryConnectRopeTo(AStoneBuildingPiece* TargetPiece, FName OwnAnchorId, FName TargetAnchorId);

    UFUNCTION(BlueprintPure, Category="Structure|Traversal")
    bool CanBeginRopeTraversal(float WeightKg) const;

    /** Starts deterministic rope movement; callers advance Alpha from 0 to 1 each movement update. */
    UFUNCTION(BlueprintCallable, Category="Structure|Traversal")
    bool TryBeginRopeTraversal(AActor* Traveler, float WeightKg, FGuid& OutTraversalId);

    UFUNCTION(BlueprintCallable, Category="Structure|Traversal")
    bool AdvanceRopeTraversal(FGuid TraversalId, float Alpha);

    UFUNCTION(BlueprintCallable, Category="Structure|Traversal")
    bool CancelRopeTraversal(FGuid TraversalId);

    /** Preflight for graph-side secondary support creation of a bridge/walkway. */
    bool CanAcceptAdditionalSupport(FName OwnAnchorId) const;

    // IStructuralPieceInterface
    virtual EStructuralNodeKind GetStructuralKind() const override { return EStructuralNodeKind::Stone; }
    virtual FGuid GetStructuralNodeId() const override;
    virtual bool GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const override;
    virtual void GetStructuralAnchorIds(TArray<FName>& OutAnchorIds) const override;

    /**
     * Adds "AutoTop" (receives) and "AutoBottom" (connects) anchors from the mesh
     * bounds when they are missing, so loose pieces can be stacked and placed.
     */
    void EnsureAutomaticAnchors();
    /** Lightweight engine-cube placeholders for structural piece archetypes when no authored mesh is assigned. */
    void BuildStructuralProxyIfNeeded();
    void ApplyStructuralProxyCollision();
    virtual bool SetStructuralAnchorOccupied(FName AnchorId, bool bOccupied) override;
    virtual bool IsStructurallyPlaced() const override { return bIsPlaced; }
    virtual bool CanAttachAsCrossMaterialChild() const override;
    virtual void ApplyCrossMaterialAttachment(AActor* Parent) override;
    virtual void ClearCrossMaterialAttachment() override;
    virtual AActor* GetCrossMaterialParent() const override { return CrossMaterialParent.Get(); }

private:
    UPROPERTY(Transient)
    bool bIsPlaced = false;

    UPROPERTY(Transient)
    bool bUsesStructuralProxy = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> StructuralProxyParts;

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> MovingLeafPivot;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> MovingLeafParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> RopeVisualSegments;

    UPROPERTY(Transient)
    bool bIsOpen = false;

    UPROPERTY(Transient)
    bool bOwnsRopeConnection = false;

    UPROPERTY(Transient)
    TWeakObjectPtr<AStoneBuildingPiece> RopeOtherEnd;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AStoneBuildingPiece>> ConnectedRopeOwners;

    UPROPERTY(Transient)
    FName RopeLocalAnchorId = NAME_None;

    UPROPERTY(Transient)
    FName RopeRemoteAnchorId = NAME_None;

    TMap<FGuid, float> ActiveBridgeTraversalWeights;
    TMap<FGuid, float> ActiveRopeTravelerWeights;
    TMap<FGuid, TWeakObjectPtr<AActor>> ActiveRopeTravelers;
    TMap<FGuid, bool> ActiveRopeTraversalStartsAtOwner;

    UPROPERTY(Transient)
    bool bIsHeld = false;

    UPROPERTY(Transient)
    TWeakObjectPtr<AStoneBuildingPiece> ParentPiece;

    UPROPERTY(Transient)
    FName ParentAnchorId = NAME_None;

    UPROPERTY(Transient)
    FName OwnAnchorIdUsed = NAME_None;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AStoneBuildingPiece>> AttachedPieces;

    /** Support of another material this piece rests on; the graph holds the edge. */
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> CrossMaterialParent;

    /** True when any piece of any material rests on this piece in the canonical graph. */
    bool HasStructuralDependents() const;
    int32 FindAnchorIndex(FName AnchorId) const;
    bool HasValidSupportChain() const;
    void ReleaseAnchorReservation(FName AnchorId);
    void BuildMovingLeafIfNeeded();
    void ApplyMovingLeafState();
    void DisconnectRopeConnection();
    AStoneBuildingPiece* GetRopeConnectionOwner() const;
};
