#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LogBuildingTypes.h"
#include "GameplayCoreTypes.h"
#include "LogBuildPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UWorldObjectIdentityComponent;
enum class EWorldItemOwnerKind : uint8;
struct FWorldOwnedItemRecord;

UCLASS(Blueprintable)
class MYPROJECT3_API ALogBuildPiece : public AActor
{
    GENERATED_BODY()

public:
    ALogBuildPiece();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Piece")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Piece|Structure")
    TObjectPtr<UWorldObjectIdentityComponent> StructuralIdentity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece")
    ELogPieceType PieceType = ELogPieceType::WholeLog;

    /** Orthogonal geometry metadata: piece length fraction and cross-section profile. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Log Piece|Geometry")
    FLogPieceGeometryState GeometryState;

    UFUNCTION(BlueprintPure, Category = "Log Piece|Geometry")
    FLogPieceGeometryState GetGeometryState() const { return GeometryState; }

    /** Explicit nominal asset families. If this list is non-empty, a valid set must be selected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TArray<FLogProcessingMeshSet> ProcessingMeshSets;

    /** Persisted selector so processed pieces keep resolving variants from the same source family. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Log Piece|Processing")
    FName SelectedProcessingMeshSetId = NAME_None;

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    FName GetSelectedProcessingMeshSetId() const { return SelectedProcessingMeshSetId; }

    /** Stable root definition; the ledger DefinitionId may change after processing. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Log Piece|Processing")
    FName BaseDefinitionId = NAME_None;

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

    /**
     * Local-space vector between the centers of the two resulting half-round
     * pieces. Configure per mesh/Blueprint; zero or longitudinal offsets are
     * rejected rather than creating overlapping/misplaced output pieces.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    FVector SplitHalfPieceCenterOffsetLocal = FVector::ZeroVector;
    /** Local mesh-space rotation applied only to the second split-half output. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    FRotator SplitHalfChildRotationOffsetLocal = FRotator(0.0f, 0.0f, 180.0f);


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TObjectPtr<UStaticMesh> QuarterVariantMesh;

    /** Local-space center-to-center offset for the two quarter-section outputs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    FVector QuarterPieceCenterOffsetLocal = FVector::ZeroVector;
    /** Local mesh-space rotation applied only to the second quarter-section output. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    FRotator QuarterChildRotationOffsetLocal = FRotator(0.0f, 180.0f, 0.0f);


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

    /** Transfers this physical piece through the authoritative world-item ledger. */
    bool TryTransferOwnership(EWorldItemOwnerKind ExpectedOwner, EWorldItemOwnerKind NewOwner,
        const FGuid& NewOwnerId, const FVector& NewWorldLocation, FWorldOwnedItemRecord& OutRecord);

    // Completes the actor-side registration after a unique inventory item becomes world-owned.
    bool ActivateAfterOwnershipTransfer();

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    bool CanApplyCutAction(ELogCutAction Action) const;

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    FString GetCutActionBlockReason(ELogCutAction Action) const;

    UFUNCTION(BlueprintCallable, Category = "Log Piece|Processing")
    bool ApplyCutAction(ELogCutAction Action);

    /** Result-bearing processing entry point; legacy bool API remains compatible. */
    UFUNCTION(BlueprintCallable, Category = "Log Piece|Processing")
    FWorldOperationResult TryApplyCutAction(ELogCutAction Action, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Log Piece")
    void SetPieceTypeByName(FName TypeName);

    /** Applies a processed definition suffix using this actor's configured variant meshes. */
    bool ApplyDefinitionVariant(FName CurrentDefinitionId);

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
    /** Disconnects graph and actor state together; force-local cleanup is reserved for EndPlay. */
    bool DetachFromSnapParent(bool bAllowGraphFailure = false);
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

    /** Keeps orthogonal geometry metadata aligned with the legacy PieceType label. */
    void UpdateGeometryStateFromPieceType(bool bPreserveLengthFraction);

    /** Selects a set only when the current mesh matches exactly one authored base-round mesh. */
    bool SelectProcessingMeshSetFromCurrentMesh();
    const FLogProcessingMeshSet* GetActiveProcessingMeshSet() const;
    UStaticMesh* GetConfiguredMeshForPieceType(ELogPieceType Type) const;
    FVector GetConfiguredSplitCenterOffset(ELogCutAction Action) const;
    FRotator GetConfiguredSplitChildRotation(ELogCutAction Action) const;

    void RegenerateAutomaticSnapPoints();
    void GenerateDefaultEndSnapPoints();
    void GenerateDefaultSideSnapPoints();
    FVector GetLongitudinalAxisVector() const;
    FRotator MakeOutwardRotation(const FVector& Direction) const;

    FWorldOperationResult TrySplitOutputPieces(ELogCutAction Action,
        const FGuid& OperationId, UStaticMesh* OutputMesh, ELogPieceType OutputType,
        FName OutputDefinitionId, const FWorldOwnedItemRecord& CurrentItem);
};
