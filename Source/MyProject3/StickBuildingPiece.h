#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayCoreTypes.h"
#include "StickBuildingTypes.h"
#include "StickBuildingPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * First-pass owner for a physical primitive stick. Inventory, recipes, tools,
 * durability and save/load remain separate systems.
 */
UCLASS(Blueprintable)
class MYPROJECT3_API AStickBuildingPiece : public AActor
{
    GENERATED_BODY()

public:
    AStickBuildingPiece();

    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stick")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick")
    EStickPieceType PieceType = EStickPieceType::RawStick;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick")
    bool bCanBeCarried = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick")
    bool bStartsPlaced = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick|Processing")
    TArray<FStickProcessingMeshSet> ProcessingMeshSets;

    UFUNCTION(BlueprintPure, Category = "Stick")
    bool IsHeld() const { return bIsHeld; }

    UFUNCTION(BlueprintPure, Category = "Stick")
    bool IsPlaced() const { return bIsPlaced; }

    UFUNCTION(BlueprintCallable, Category = "Stick")
    bool SetHeldState(bool bNewHeld);

    UFUNCTION(BlueprintPure, Category = "Stick|Processing")
    bool CanProcessStick(EStickProcessingAction Action) const;

    UFUNCTION(BlueprintCallable, Category = "Stick|Processing")
    FWorldOperationResult TryProcessStick(EStickProcessingAction Action, FGuid OperationId);

    /**
     * Surface query/ground-contact validation must be performed by the caller.
     * This method commits the supplied transform only when that validation passed.
     */
    UFUNCTION(BlueprintCallable, Category = "Stick|Placement")
    FWorldOperationResult TryCommitGroundedStakePlacement(
        const FVector& SurfaceLocation,
        const FRotator& SurfaceRotation,
        bool bGroundContactConfirmed,
        FGuid OperationId);

private:
    UPROPERTY(Transient)
    bool bIsHeld = false;

    UPROPERTY(Transient)
    bool bIsPlaced = false;

    UStaticMesh* ResolveOutputMesh(EStickProcessingAction Action,
        bool& bOutAmbiguousMapping) const;
};
