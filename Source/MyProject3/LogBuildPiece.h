#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LogBuildingTypes.h"
#include "GameplayCoreTypes.h"
#include "LogBuildPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class MYPROJECT3_API ALogBuildPiece : public AActor
{
    GENERATED_BODY()

public:
    ALogBuildPiece();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Piece")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece")
    ELogPieceType PieceType = ELogPieceType::WholeLog;

    // Imported log meshes are expected to have their long axis along X by
    // default. Change this per mesh if its local orientation differs.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece")
    ELogLongitudinalAxis LongitudinalAxis = ELogLongitudinalAxis::X;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Snapping")
    bool bAutoGenerateEndSnapPoints = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Snapping",
        meta = (EditCondition = "bAutoGenerateEndSnapPoints"))
    FName DefaultEndSnapTag = FName(TEXT("LogEnd"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Snapping")
    bool bAutoGenerateSideSnapPoints = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Snapping",
        meta = (EditCondition = "bAutoGenerateSideSnapPoints", ClampMin = "50.0"))
    float SideSnapSpacing = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Snapping",
        meta = (EditCondition = "bAutoGenerateSideSnapPoints"))
    FName SideSnapTag = FName(TEXT("LogEnd"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Snapping")
    TArray<FLogSnapPoint> SnapPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece")
    bool bCanBeCarried = true;

    // Set true for pieces pre-placed as part of the level/structure.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece")
    bool bStartsPlaced = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> SplitHalfVariantMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> QuarterVariantMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> HewnVariantMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> GutterVariantMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> TroughVariantMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> KalasVariantMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> PlankVariantMesh;

    UFUNCTION(BlueprintPure, Category = "Log Piece")
    UStaticMeshComponent* GetLogMesh() const { return Mesh; }

    UFUNCTION(BlueprintPure, Category = "Log Piece")
    ELogPieceType GetPieceType() const { return PieceType; }

    UFUNCTION(BlueprintPure, Category = "Log Piece")
    bool IsPlaced() const { return bIsPlaced; }

    UFUNCTION(BlueprintPure, Category = "Log Piece")
    bool IsHeld() const { return bIsHeld; }

    UFUNCTION(BlueprintPure, Category = "Log Piece|Snapping")
    int32 GetSnapPointCount() const { return SnapPoints.Num(); }

    UFUNCTION(BlueprintPure, Category = "Log Piece|Snapping")
    FTransform GetSnapPointWorldTransform(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Log Piece")
    bool CanBePickedUp() const;

    UFUNCTION(BlueprintCallable, Category = "Log Piece")
    void SetPieceStaticMesh(UStaticMesh* NewMesh);

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    bool CanApplyCutAction(ELogCutAction Action) const;

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    FString GetCutActionBlockReason(ELogCutAction Action) const;

    UFUNCTION(BlueprintCallable, Category = "Log Piece|Processing")
    bool ApplyCutAction(ELogCutAction Action);

    /**
     * Result-bearing processing entry point; legacy bool API remains compatible.
     * OperationId is diagnostic/correlation metadata in this phase, not an
     * idempotency key: repeating a successful request is revalidated against
     * the new piece state and is not guaranteed to return the original result.
     * Callers must not blindly retry this operation expecting deduplication.
     */
    UFUNCTION(BlueprintCallable, Category = "Log Piece|Processing")
    FWorldOperationResult TryApplyCutAction(ELogCutAction Action, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Log Piece")
    void SetPieceTypeByName(FName TypeName);

    UFUNCTION(BlueprintCallable, Category = "Log Piece|Snapping")
    void AddCustomSnapPoint(FName PointId, FName SnapTag, FTransform LocalTransform,
        bool bCanSource, bool bCanReceive, bool bAcceptAnyPieceType,
        bool bFlipIncomingOrientation);

    UFUNCTION(BlueprintCallable, Category = "Log Piece")
    void SetPlacedState(bool bNewPlaced);

    UFUNCTION(BlueprintCallable, Category = "Log Piece")
    void SetHeldState(bool bNewHeld);

    // Called by LogCarryComponent after a snap has been validated.
    bool CanSnapPointAccept(int32 TargetPointIndex,
        const ALogBuildPiece* SourcePiece, int32 SourcePointIndex) const;

    bool IsSnapPointFreeAndSource(int32 Index) const;
    bool IsSnapPointFreeAndReceiver(int32 Index) const;
    bool TryReserveSnapPoints(int32 TargetPointIndex,
        ALogBuildPiece* SourcePiece, int32 SourcePointIndex);
    void ReleaseSnapPoint(int32 Index);
    bool AttachToSnapParent(ALogBuildPiece* Parent, int32 ParentPointIndex,
        int32 MyPointIndex);
    void DetachFromSnapParent();
    void RegisterAttachedPiece(ALogBuildPiece* Child);
    void UnregisterAttachedPiece(ALogBuildPiece* Child);
    int32 GetAttachedPieceCount() const;

    const TArray<FLogSnapPoint>& GetSnapPoints() const { return SnapPoints; }
    FTransform GetSnapPointLocalTransform(int32 Index) const;

private:
    UPROPERTY(Transient)
    bool bIsPlaced = false;

    UPROPERTY(Transient)
    bool bIsHeld = false;

    UPROPERTY(Transient)
    TWeakObjectPtr<ALogBuildPiece> SnapParent;

    UPROPERTY(Transient)
    int32 ParentSnapPointIndex = INDEX_NONE;

    UPROPERTY(Transient)
    int32 OwnSnapPointIndexUsed = INDEX_NONE;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<ALogBuildPiece>> AttachedPieces;

    bool HasOccupiedSnapPoints() const;
    void RegenerateAutomaticSnapPoints();
    void GenerateDefaultEndSnapPoints();
    void GenerateDefaultSideSnapPoints();
    FVector GetLongitudinalAxisVector() const;
    FRotator MakeOutwardRotation(const FVector& Direction) const;
};
