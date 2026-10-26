#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StoneBuildingTypes.h"
#include "StructuralPieceInterface.h"
#include "StoneBuildingPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
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

    // IStructuralPieceInterface
    virtual EStructuralNodeKind GetStructuralKind() const override { return EStructuralNodeKind::Stone; }
    virtual FGuid GetStructuralNodeId() const override;
    virtual bool GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const override;
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
};
