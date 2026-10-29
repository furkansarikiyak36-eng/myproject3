#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StickPrimitiveTypes.h"
#include "StructuralPieceInterface.h"
#include "StickPrimitivePiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UWallCoatingComponent;
class UWorldObjectIdentityComponent;

/**
 * Explicit state owner for one stick-derived physical piece.
 * This domain layer never treats mesh overlap as a structural connection.
 */
UCLASS(Blueprintable)
class MYPROJECT3_API AStickPrimitivePiece : public AActor, public IStructuralPieceInterface
{
    GENERATED_BODY()

public:
    AStickPrimitivePiece();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Registers a newly materialized actor after ledger ownership becomes World. */
    bool ActivateAfterOwnershipTransfer();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Primitive")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Primitive|Structure")
    TObjectPtr<UWallCoatingComponent> WallCoating;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Primitive|Structure")
    TObjectPtr<UWorldObjectIdentityComponent> StructuralIdentity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive")
    EStickPrimitiveState PieceState = EStickPrimitiveState::Raw;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive")
    bool bCanBeCarried = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Anchors")
    TArray<FPrimitiveAnchor> Anchors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Trap")
    bool bTrapRequiresGrounding = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Trap")
    bool bTrapHasAllComponents = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Trap")
    bool bTrapHasValidTrigger = false;

    UFUNCTION(BlueprintPure, Category="Primitive")
    bool IsHeld() const { return bIsHeld; }

    UFUNCTION(BlueprintPure, Category="Primitive")
    bool IsPlaced() const { return bIsPlaced; }

    UFUNCTION(BlueprintPure, Category="Primitive|Trap")
    EPrimitiveTrapState GetTrapState() const { return TrapState; }

    UFUNCTION(BlueprintCallable, Category="Primitive")
    bool SetHeldState(bool bNewHeld);

    UFUNCTION(BlueprintCallable, Category="Primitive")
    bool TryCommitGroundPlacement(const FVector& SurfaceLocation, const FRotator& SurfaceRotation);

    UFUNCTION(BlueprintCallable, Category="Primitive")
    bool TryProcessTo(EStickPrimitiveState NewState);

    UFUNCTION(BlueprintCallable, Category="Primitive|Trap")
    bool TryArmTrap();

    UFUNCTION(BlueprintCallable, Category="Primitive|Trap")
    bool TryTriggerTrap();

    UFUNCTION(BlueprintCallable, Category="Primitive|Trap")
    bool DisarmTrap();

    UFUNCTION(BlueprintCallable, Category="Primitive|Anchors")
    bool AddAnchor(FName AnchorId, FName CompatibilityTag, FTransform LocalTransform);

    UFUNCTION(BlueprintCallable, Category="Primitive|Anchors")
    bool TryConnectToAnchor(AStickPrimitivePiece* TargetPiece, FName TargetAnchorId, FName OwnAnchorId);

    UFUNCTION(BlueprintCallable, Category="Primitive|Anchors")
    /** Keeps actor anchors and the canonical graph edge synchronized. */
    bool RemoveConnection(bool bAllowGraphFailure = false);

    UFUNCTION(BlueprintPure, Category="Primitive|Anchors")
    FTransform GetAnchorWorldTransform(FName AnchorId) const;

    // IStructuralPieceInterface
    virtual EStructuralNodeKind GetStructuralKind() const override { return EStructuralNodeKind::Stick; }
    virtual FGuid GetStructuralNodeId() const override;
    virtual bool GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const override;
    virtual void GetStructuralAnchorIds(TArray<FName>& OutAnchorIds) const override;

    /**
     * Adds "AutoTop" (receives) and "AutoBottom" (connects) anchors from the mesh
     * bounds when they are missing, so loose pieces can be stacked and placed.
     */
    void EnsureAutomaticAnchors();
    /** Rebuilds actor-local same-material links after canonical graph rollback. */
    void ResetActorLocalAttachmentStateForGraphRestore(bool bPlaced);
    bool RestoreSameMaterialAttachmentFromGraph(AStickPrimitivePiece* Parent, FName ParentAnchorId, FName OwnAnchorId);
    virtual bool SetStructuralAnchorOccupied(FName AnchorId, bool bOccupied) override;
    virtual bool IsStructurallyPlaced() const override { return bIsPlaced; }
    virtual bool CanAttachAsCrossMaterialChild() const override;
    virtual void ApplyCrossMaterialAttachment(AActor* Parent) override;
    virtual void ClearCrossMaterialAttachment() override;
    virtual AActor* GetCrossMaterialParent() const override { return CrossMaterialParent.Get(); }

private:
    UPROPERTY(Transient)
    bool bIsHeld = false;

    UPROPERTY(Transient)
    bool bIsPlaced = false;

    UPROPERTY(Transient)
    EPrimitiveTrapState TrapState = EPrimitiveTrapState::Incomplete;

    UPROPERTY(Transient)
    TWeakObjectPtr<AStickPrimitivePiece> ParentPiece;

    UPROPERTY(Transient)
    FName ParentAnchorId = NAME_None;

    UPROPERTY(Transient)
    FName OwnAnchorIdUsed = NAME_None;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AStickPrimitivePiece>> AttachedPieces;

    /** Support of another material this piece rests on; the graph holds the edge. */
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> CrossMaterialParent;

    /** True when any piece of any material rests on this piece in the canonical graph. */
    bool HasStructuralDependents() const;
    int32 FindAnchorIndex(FName AnchorId) const;
    bool HasValidSupportChain() const;
    void ReleaseAnchor(FName AnchorId);
};
