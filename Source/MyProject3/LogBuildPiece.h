#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LogBuildingTypes.h"
#include "GameplayCoreTypes.h"
#include "StructuralPieceInterface.h"
#include "LogBuildPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UWorldObjectIdentityComponent;
enum class EWorldItemOwnerKind : uint8;
struct FWorldOwnedItemRecord;

UCLASS(Blueprintable)
class MYPROJECT3_API ALogBuildPiece : public AActor, public IStructuralPieceInterface
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

    /**
     * Legacy composite label retained for existing Blueprint/content compatibility.
     * LengthFraction and CrossSectionProfile are the explicit independent dimensions.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece")
    ELogPieceType PieceType = ELogPieceType::WholeLog;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Log Piece|Geometry")
    ELogLengthFraction LengthFraction = ELogLengthFraction::FullLength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Log Piece|Geometry")
    ELogCrossSectionProfile CrossSectionProfile = ELogCrossSectionProfile::Round;

    /**
     * False maps the legacy PieceType into the independent dimensions. Set true
     * when an authored/runtime piece needs a combination not expressible by the
     * old enum (for example, half-length + half-round).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Log Piece|Geometry")
    bool bUseCanonicalGeometryDimensions = false;

    /**
     * Explicit nominal asset families. If this list is non-empty, a valid set must be
     * selected and all geometry/length-cut data resolves from that set only.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Processing")
    TArray<FLogProcessingMeshSet> ProcessingMeshSets;

    /** Persisted selector so processed pieces keep resolving variants from the same source family. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Log Piece|Processing")
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
    float SideSnapSpacing = 75.0f;

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

    /** Raw/base mesh for the authored length recorded immediately below. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Log Piece|Geometry")
    TObjectPtr<UStaticMesh> BaseLogVariantMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Geometry")
    ELogLengthFraction BaseLogVariantLengthFraction = ELogLengthFraction::FullLength;

    /** Exact length+cross-section mesh table. Non-full-length processed outputs require an exact entry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Geometry")
    TArray<FLogGeometryVariantMesh> GeometryVariantMeshes;

    /** Explicit output positions/rotations for longitudinal length cuts; no placement is guessed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    TArray<FLogLengthCutVariant> LengthCutVariants;

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

    UFUNCTION(BlueprintPure, Category = "Log Piece|Geometry")
    ELogLengthFraction GetLengthFraction() const;

    UFUNCTION(BlueprintPure, Category = "Log Piece|Geometry")
    ELogCrossSectionProfile GetCrossSectionProfile() const;

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
    /** Applies a configured initial geometry state to a newly spawned, world-owned output. */
    bool TrySetInitialPieceType(ELogPieceType DesiredType, FGuid OperationId);

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    bool CanApplyCutAction(ELogCutAction Action) const;

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    FString GetCutActionBlockReason(ELogCutAction Action) const;

    UFUNCTION(BlueprintCallable, Category = "Log Piece|Processing")
    bool ApplyCutAction(ELogCutAction Action);

    /** Result-bearing processing entry point; legacy bool API remains compatible. */
    UFUNCTION(BlueprintCallable, Category = "Log Piece|Processing")
    FWorldOperationResult TryApplyCutAction(ELogCutAction Action, FGuid OperationId);
    /** Adds normalized cut work (0..1) before committing the geometry transaction. */
    UFUNCTION(BlueprintCallable, Category = "Log Piece|Processing")
    FWorldOperationResult TryAdvanceCutProgress(ELogCutAction Action, float NormalizedWork, FGuid OperationId);

    /** Runtime progress for the current processing action; 1.0 is committed immediately. */
    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    float GetCutProgress() const { return CutProgress; }

    UFUNCTION(BlueprintPure, Category = "Log Piece|Processing")
    bool HasActiveCutProgress() const { return bHasActiveCutProgress; }

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
    /** Rebuild transient actor/anchor bookkeeping after canonical graph restore; never mutates the graph. */
    void ResetActorLocalSnapStateForGraphRestore();
    bool RestoreActorLocalSnapLinkFromGraph(ALogBuildPiece* Parent, FName ParentAnchorId,
        FName OwnAnchorId, bool bPlaced);
    void RegisterAttachedPiece(ALogBuildPiece* Child);
    void UnregisterAttachedPiece(ALogBuildPiece* Child);
    int32 GetAttachedPieceCount() const;

    const TArray<FLogSnapPoint>& GetSnapPoints() const { return SnapPoints; }

    /**
     * Lays the body stations out again when the log's world length changed (a free
     * cut scales it). Only on a log nothing is joined to, so no joint moves.
     */
    void RefreshStationsIfStale();

    /**
     * Index of the joint point with this id (End.A, End.B, Station.00 ...), or
     * INDEX_NONE. Ids from saves made before the station layout (AutoEnd_*) map
     * to the matching end.
     */
    int32 FindSnapPointIndex(FName PointId) const;
    /** True when SourcePointId of Source can join this log's TargetPointId now. */
    bool CanJoinById(FName TargetPointId, const ALogBuildPiece* Source, FName SourcePointId) const;
    /** Joins Source's SourcePointId to this log's TargetPointId (attach, reserve, graph edge), or changes nothing. */
    bool TryJoinById(FName TargetPointId, ALogBuildPiece* Source, FName SourcePointId);
    /** The log's length in the world, scale included. */
    float GetWorldLengthCm() const;

    // IStructuralPieceInterface
    virtual EStructuralNodeKind GetStructuralKind() const override { return EStructuralNodeKind::Wood; }
    virtual FGuid GetStructuralNodeId() const override;
    virtual bool GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const override;
    virtual void GetStructuralAnchorIds(TArray<FName>& OutAnchorIds) const override;
    virtual bool SetStructuralAnchorOccupied(FName AnchorId, bool bOccupied) override;
    virtual bool IsStructurallyPlaced() const override { return bIsPlaced; }
    virtual bool CanAttachAsCrossMaterialChild() const override;
    virtual void ApplyCrossMaterialAttachment(AActor* Parent) override;
    virtual void ClearCrossMaterialAttachment() override;
    virtual AActor* GetCrossMaterialParent() const override { return CrossMaterialParent.Get(); }
    FTransform GetSnapPointLocalTransform(int32 Index) const;

private:
    /** Persisted in-progress cut; completion still requires a valid configured output mesh. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Log Piece|Processing", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
    float CutProgress = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Log Piece|Processing", meta=(AllowPrivateAccess="true"))
    ELogCutAction ActiveCutAction = ELogCutAction::SplitHalf;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Log Piece|Processing", meta=(AllowPrivateAccess="true"))
    bool bHasActiveCutProgress = false;

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

    /** Non-log support (stone, stick) this log rests on; the graph holds the edge. */
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> CrossMaterialParent;

    /** True when any piece of any material rests on this log in the canonical graph. */
    bool HasStructuralDependents() const;

    void RegenerateAutomaticSnapPoints();
    void GenerateDefaultEndSnapPoints();
    void GenerateDefaultSideSnapPoints();
    /** Every socket on the mesh becomes a joint point "Socket.<name>" (notches, sockets made in the mesh editor). */
    void GenerateSocketSnapPoints();
    /** World length the body stations were laid out for. */
    float StationsBuiltForLengthCm = 0.0f;
    FVector GetLongitudinalAxisVector() const;
    FRotator MakeOutwardRotation(const FVector& Direction) const;

    static ELogLengthFraction LengthFractionFromLegacyType(ELogPieceType Type);
    static ELogCrossSectionProfile CrossSectionFromLegacyType(ELogPieceType Type);
    static ELogPieceType LegacyTypeFromCanonicalDimensions(
        ELogLengthFraction InLength, ELogCrossSectionProfile InProfile);

    void SyncCanonicalGeometryFromLegacyPieceType();
    void SetCanonicalCrossSectionFromPieceType(ELogPieceType Type);

    /** Selects a set only when the current mesh matches exactly one authored set mesh. */
    bool SelectProcessingMeshSetFromCurrentMesh();
    /** Derives canonical dimensions from an exact, unambiguous mesh match in the active set. */
    void SynchronizeGeometryStateFromCurrentMeshSet();
    const FLogProcessingMeshSet* GetActiveProcessingMeshSet() const;
    /** True when sets are configured but none is uniquely and validly selected. */
    bool HasInvalidProcessingMeshSetSelection() const;

    FWorldOperationResult CommitCutAction(ELogCutAction Action, FGuid OperationId);
    bool TryResolveGeometryVariant(ELogLengthFraction InLength,
        ELogCrossSectionProfile InProfile, UStaticMesh*& OutMesh,
        FVector* OutSecondPieceCenterOffset = nullptr,
        FRotator* OutSecondPieceRotationOffset = nullptr) const;
    bool TryResolveLengthCutVariant(ELogCutAction Action,
        FLogLengthCutVariant& OutVariant) const;
    bool IsLengthCutAction(ELogCutAction Action) const;
    bool IsValidLengthCutLayout(const FLogLengthCutVariant& Variant) const;

    FWorldOperationResult TrySplitOutputPieces(ELogCutAction Action,
        const FGuid& OperationId, UStaticMesh* OutputMesh, ELogPieceType OutputType,
        FName OutputDefinitionId, const FWorldOwnedItemRecord& CurrentItem);
    FWorldOperationResult TrySplitLengthOutputPieces(ELogCutAction Action,
        const FGuid& OperationId, const FWorldOwnedItemRecord& CurrentItem);
    FWorldOperationResult TryCommitPhysicalSplitOutputs(ELogCutAction Action,
        const FGuid& OperationId, const FWorldOwnedItemRecord& CurrentItem,
        const TArray<FName>& OutputDefinitionIds,
        const TArray<ELogLengthFraction>& OutputLengths,
        const TArray<ELogCrossSectionProfile>& OutputProfiles,
        const TArray<ELogPieceType>& OutputLegacyTypes,
        const TArray<UStaticMesh*>& OutputMeshes,
        const TArray<FTransform>& OutputTransforms,
        const FText& SuccessMessage);
};
