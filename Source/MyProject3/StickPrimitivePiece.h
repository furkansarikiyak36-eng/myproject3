#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StickPrimitiveTypes.h"
#include "StickPrimitivePiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Explicit state owner for one stick-derived physical piece.
 * This domain layer never treats mesh overlap as a structural connection.
 */
UCLASS(Blueprintable)
class MYPROJECT3_API AStickPrimitivePiece : public AActor
{
    GENERATED_BODY()

public:
    AStickPrimitivePiece();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Primitive")
    TObjectPtr<UStaticMeshComponent> Mesh;

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
    bool RemoveConnection();

    UFUNCTION(BlueprintPure, Category="Primitive|Anchors")
    FTransform GetAnchorWorldTransform(FName AnchorId) const;

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

    int32 FindAnchorIndex(FName AnchorId) const;
    bool HasValidSupportChain() const;
    void ReleaseAnchor(FName AnchorId);
};
