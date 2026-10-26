#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LogBuildingTypes.h"
#include "LogCarryComponent.generated.h"

class ALogBuildPiece;
enum class EWorldItemOwnerKind : uint8;
class UInputComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FLogSnapStateChanged, bool, bHasValidSnap);

UCLASS(ClassGroup = (Gameplay), BlueprintType, Blueprintable,
    meta = (BlueprintSpawnableComponent))
class MYPROJECT3_API ULogCarryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULogCarryComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Trace",
        meta = (ClampMin = "50.0"))
    float PickupRange = 350.0f;

    /** Impulse used by the optional loose-log push/roll interaction. Tune per project physics scale. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics",
        meta = (ClampMin = "0.0"))
    float PushImpulseStrength = 25000.0f;

    /** Hard cap prevents accidental Blueprint values from launching a log at extreme speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics",
        meta = (ClampMin = "0.0"))
    float MaxPushImpulseMagnitude = 50000.0f;

    /** Continuous horizontal drag force while the drag key is held. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics", meta=(ClampMin="0.0"))
    float DragForceStrength = 180000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics", meta=(ClampMin="0.0"))
    float MaxDragForceMagnitude = 250000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics", meta=(ClampMin="50.0"))
    float DragTargetDistance = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Trace",
        meta = (ClampMin = "100.0"))
    float PlacementTraceDistance = 4500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement",
        meta = (ClampMin = "20.0"))
    float SnapSearchRadius = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement",
        meta = (ClampMin = "50.0"))
    float CarryDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    bool bAllowFreePlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    bool bRequireSurfaceForFreePlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    ELogPlacementMode PlacementMode = ELogPlacementMode::Horizontal;

    UPROPERTY(BlueprintAssignable, Category = "Log Carry|Events")
    FLogSnapStateChanged OnSnapStateChanged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    TObjectPtr<ALogBuildPiece> HeldPiece;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    TObjectPtr<ALogBuildPiece> CurrentSnapTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    int32 CurrentSourceSnapPointIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    int32 CurrentTargetSnapPointIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    bool bHasValidSnap = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    bool bHasPlacementSurface = false;

    // Can be called from a Character/Controller Blueprint with its InputComponent.
    // If using Enhanced Input, bind these same functions to your Input Actions instead.
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Input")
    void BindDefaultInput(UInputComponent* InputComponent);

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void Interact();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool TryPickupTarget();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool PlaceHeldPiece();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void DropHeldPiece();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void RotateHeldPiece();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void CyclePlacementMode();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void CycleCutAction();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool ProcessAimedPiece();

    /** Starts a continuous drag tether on an aimed loose log; bind release to EndDragAimedLog. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool BeginDragAimedLog();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void EndDragAimedLog();

    /** Applies a bounded off-centre impulse to an aimed, loose, world-owned physics log. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool TryPushOrRollAimedLog();

    UFUNCTION(BlueprintPure, Category = "Log Carry|State")
    ELogCutAction GetSelectedCutAction() const { return SelectedCutAction; }

    UFUNCTION(BlueprintPure, Category = "Log Carry|State")
    bool IsCarryingLog() const { return HeldPiece.Get() != nullptr; }

    /** Stone or stick currently held in the hands (logs use HeldPiece). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    TObjectPtr<AActor> HeldMaterialPiece;

    /** How far below a log's free end to look for something it can rest on. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement", meta = (ClampMin = "5.0"))
    float FarEndSupportProbeCm = 30.0f;

    /**
     * How far the joined end of a log is pushed into the log it meets, along its own
     * length. 1 = the end reaches the target's centre line, so the end grain is hidden.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement", meta = (ClampMin = "0.0", ClampMax = "1.8"))
    float EndInsetFraction = 1.0f;

    /** How deep a log resting across another sinks into it, as a share of the target radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement", meta = (ClampMin = "0.0", ClampMax = "0.9"))
    float SnapInsetFraction = 0.5f;

    /** Number of logs currently swinging down around their joint. */
    int32 GetSwingingLogCount() const { return SwingingLogs.Num(); }

    /** Remembers a log hanging from one joint whose free end rests low or on the ground. */
    void RegisterDroopingLog(ALogBuildPiece* Piece, int32 OwnSnapIndex);

    int32 GetDroopingLogCount() const { return DroopingLogs.Num(); }

    /**
     * Turns every drooping log whose free end lies over NewSupport back up around its
     * joint so that end rests on top of NewSupport. Returns true when one was lifted.
     */
    bool LiftDroopingEndsOnto(AActor* NewSupport);

    /** World position of a log's end opposite the joint, the joint itself and the log radius. */
    bool GetLogFreeEnd(const ALogBuildPiece* Piece, int32 OwnSnapIndex,
        FVector& OutFreeEnd, FVector& OutPivot, float& OutRadius) const;

    UFUNCTION(BlueprintPure, Category = "Log Carry|State")
    bool IsHoldingMaterialPiece() const { return HeldMaterialPiece.Get() != nullptr; }

    /** Short player-facing status of the current placement or the last action. */
    UFUNCTION(BlueprintPure, Category = "Log Carry|State")
    FString GetPlacementStatus() const { return PlacementStatus; }

    /** Picks a loose stone or stick into the hands through the ownership ledger. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Materials")
    bool TryPickupMaterialPiece(AActor* Piece);

    /** Places the held stone/stick on a receiving anchor of a placed piece. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Materials")
    bool TryPlaceHeldMaterialOn(AActor* Target, FName TargetAnchorId);

    /** Places the held stone/stick upright on the ground at a world point. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Materials")
    bool TryPlaceHeldMaterialOnGround(FVector GroundPoint);

    /**
     * True when a horizontal log joined at one end has something under its
     * other end, or when the check does not apply (posts, middle joints).
     */
    bool EvaluateFarEndSupport(const ALogBuildPiece* Piece, int32 OwnSnapPointIndex,
        AActor*& OutSupport, bool& bOutGround) const;

    /** World-space dashed segments (pairs of points) showing where a cut would go. */
    bool BuildCutPreviewSegments(const ALogBuildPiece* Log, ELogCutAction Action,
        TArray<FVector>& OutSegmentPoints) const;

    /** The loose log under the crosshair, if any. */
    ALogBuildPiece* GetAimedLooseLog() const;

    /** Throw speed (cm/s) for a 20 kg piece; heavier pieces fly slower. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics", meta = (ClampMin = "100.0"))
    float ThrowSpeed = 1400.0f;

    /** Throws the carried log, stone or stick along the view; fast hits deal damage. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool ThrowHeldPiece();

    /** Picks up a specific loose piece without aiming at it. */
    bool TryPickupPiece(AActor* Target);

    /** Places the carried log at an exact transform resting on PrimarySupport (and SecondarySupport). */
    bool CommitHeldLogAt(const FTransform& WorldTransform, AActor* PrimarySupport, AActor* SecondarySupport);

    /** Hands the carried piece to a tool (storage, fire, zipline); it stays held in the ledger. */
    AActor* ReleaseHeldToTool();

    /** Links supports found at both ends: beams on posts, braces under beams, logs on trees. */
    void ApplyEndContactSupports(ALogBuildPiece* Piece);

    void SetPlacementStatus(const FString& NewStatus) { PlacementStatus = NewStatus; }

    /** Logs stack on placed logs on a grid of this step along the lower log (600 cm = 8 steps). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement", meta = (ClampMin = "10.0"))
    float StackGridCm = 75.0f;

    /** How far a stacked log sits down into the one below, as a share of the two radii. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement", meta = (ClampMin = "0.5", ClampMax = "1.0"))
    float StackInterlock = 0.82f;

    /**
     * Where Held would lie on the placed horizontal log Target: lying along it or
     * across it (by the carry direction), or standing on it as a post, with its
     * ends or its crossing on Target's grid.
     */
    bool ComputeStackPlacement(ALogBuildPiece* Held, ALogBuildPiece* Target, const FVector& AimPoint,
        float CarryYaw, FTransform& OutTransform) const;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    float SnapRefreshInterval = 0.06f;

private:
    float TimeUntilSnapRefresh = 0.0f;
    FGuid CarryOwnerId;
    float HeldYawOffset = 0.0f;
    ELogCutAction SelectedCutAction = ELogCutAction::SplitHalf;
    FTransform CurrentSnapActorTransform = FTransform::Identity;
    TWeakObjectPtr<ALogBuildPiece> DraggedPiece;
    bool bIsDraggingLog = false;

    bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
    bool TraceFromView(float Distance, FHitResult& OutHit) const;
    void UpdateHeldPreview();
    void FindBestSnapCandidate();
    void ClearSnapCandidate();
    void SetSnapCandidate(ALogBuildPiece* Target, int32 SourceIndex,
        int32 TargetIndex, const FTransform& DesiredActorTransform);
    FRotator MakePlacementRotation(float ViewYaw) const;
    void ProcessAimedPieceInput();
    void PushOrRollAimedLogInput();
    void BeginDragAimedLogInput();
    void ReleaseHeldFromLoad();

    FString PlacementStatus;

    bool bHasStackPlacement = false;
    FTransform StackTransform = FTransform::Identity;
    TWeakObjectPtr<ALogBuildPiece> StackTarget;
    void UpdateStackPlacement();

    /** Tilt for a carried stick from the placement mode: upright, 45 degrees or lying. */
    float GetStickTiltDegrees() const;
    FQuat MakeTiltAroundView(float Degrees) const;

    /** What the free-placement trace hit, so a post on a floor joins that floor. */
    TWeakObjectPtr<AActor> PlacementSurfaceActor;

    UPROPERTY(Transient)
    TObjectPtr<AActor> MaterialSnapTarget;

    FName MaterialSnapAnchor = NAME_None;
    FTransform MaterialPreviewTransform = FTransform::Identity;
    bool bMaterialGroundPreview = false;

    /** A log joined at one end that swings down around the joint until it settles. */
    struct FSwingingLog
    {
        TWeakObjectPtr<ALogBuildPiece> Piece;
        TWeakObjectPtr<AActor> Parent;
        TWeakObjectPtr<class UPhysicsConstraintComponent> Hinge;
        int32 OwnSnapIndex = INDEX_NONE;
        float Elapsed = 0.0f;
        float StillTime = 0.0f;
    };
    TArray<FSwingingLog> SwingingLogs;

    struct FDroopingLog
    {
        TWeakObjectPtr<ALogBuildPiece> Piece;
        int32 OwnSnapIndex = INDEX_NONE;
    };
    TArray<FDroopingLog> DroopingLogs;

    /** Drooping logs currently resting on the log in the hands, with their pose before. */
    struct FHeldLift
    {
        TWeakObjectPtr<ALogBuildPiece> Piece;
        FTransform Original = FTransform::Identity;
    };
    TArray<FHeldLift> HeldLifts;

    void UpdateHeldLifts();
    void RestoreHeldLifts();
    void CommitHeldLifts(AActor* NewSupport);

    /** Rotation about the joint that brings the free end onto Support, if it lies over it. */
    bool ComputeDroopLift(const ALogBuildPiece* Piece, int32 OwnSnapIndex, const AActor* Support,
        FQuat& OutRotation, FVector& OutPivot) const;

    bool StartHingeSwing(ALogBuildPiece* Piece, int32 OwnSnapIndex);
    void UpdateHingeSwings(float DeltaTime);
    void SettleHingeSwing(FSwingingLog& Swing);

    void UpdateHeldMaterialPreview();
    bool PlaceHeldMaterialPiece();
    void DropHeldMaterialPiece();
    bool TransferMaterialOwnership(AActor* Piece, EWorldItemOwnerKind From, EWorldItemOwnerKind To,
        const FVector& Location) const;
    FTransform MakeUprightTransform(AActor* Piece, const FVector& BottomWorldPoint) const;
    void FinishMaterialPlacement(AActor* Piece);
};
