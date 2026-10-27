#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LogBuildingTypes.h"
#include "Placement/PlacementTypes.h"
#include "LogCarryComponent.generated.h"

class ALogBuildPiece;
enum class EWorldItemOwnerKind : uint8;
class UInputComponent;

/** How a cut chosen by the aim is made. */
enum class ELogCutKind : uint8
{
    /** A cut from the mesh family's recipes (split, hew, 1/2, 3/4 + 1/4 ...). */
    Recipe,
    /** A length cut anywhere along a loose log, on a 25 cm step; both parts are scaled meshes. */
    FreeLength,
    /** Shortening a standing post that holds nothing up. */
    ShortenPost,
    /** Sharpening the top of a standing log into a stake (aim at its top). */
    Sharpen
};

/** The cut the aim picks: the recipe action, where the saw goes and how it is made. */
struct FLogCutChoice
{
    ELogCutAction Action = ELogCutAction::SplitHalf;
    /** Saw position, 0..1 from the mesh's -X end; -1 for cuts along the log. */
    float Share = -1.0f;
    /** Turn the log end for end first (3/4 cut aimed at the -X end). */
    bool bFlip = false;
    ELogCutKind Kind = ELogCutKind::Recipe;
};

/** What the carried log will do where the player looks; chosen by ResolveContext, never by a mode. */
enum class ELogContextAction : uint8
{
    None,
    /** A joint: end to end, end to a station (T), a station on an end (across a post top), notches. */
    Snap,
    /** On top of a lying log: the next row of a wall, a corner, or a post on it. */
    Stack,
    /** A board across two parallel beams. */
    Deck,
    /** Between a post and the beam on it. */
    Strut,
    /** Lying on the ground. */
    Ground,
    /** Standing on the ground as a post (the player looks steeply down). */
    Post
};

/** Where the player looks, as ResolveContext needs it; filled from the camera in play, by hand in tests. */
struct FLogContextAim
{
    AActor* AimActor = nullptr;
    FVector AimPoint = FVector::ZeroVector;
    FVector ViewDirection = FVector::ForwardVector;
    /** Camera pitch in degrees; negative looks down. */
    float ViewPitchDeg = 0.0f;
    float CarryYawDeg = 0.0f;
    /** Placed pieces near the aim (the other contextual candidates). */
    TArray<AActor*> Candidates;
    /** The previous choice, so the preview does not flicker between two of them. */
    ELogContextAction PreviousAction = ELogContextAction::None;
};

struct FLogContextResult
{
    ELogContextAction Action = ELogContextAction::None;
    /** The kernel's solution for Snap, Stack, Deck and Strut (Ground and Post rest on the surface). */
    FPlacementSolution Solution;
    bool bUpright = false;
    /** Why nothing fits, in a few words, when Action is None. */
    FString Reason;
};

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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Trace",
        meta = (ClampMin = "50.0"))
    float PickupRange = 350.0f;

    /** Normalized processing work contributed by each deliberate F press. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Processing", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float CutWorkPerPress = 0.25f;

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

    /** Radius around the aim point used to gather nearby placed logs as snap candidates. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement",
        meta = (ClampMin = "25.0", ClampMax = "500.0"))
    float PlacementCandidateSearchRadiusCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement",
        meta = (ClampMin = "50.0"))
    float CarryDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    bool bAllowFreePlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    bool bRequireSurfaceForFreePlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    ELogPlacementMode PlacementMode = ELogPlacementMode::Horizontal;

    /**
     * The game chooses how the log goes from where the player looks (ResolveContext).
     * The mode key (right mouse) still switches to the old modes as a manual override
     * until the log is put down.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    bool bContextualPlacement = true;

    /** Looking down steeper than this (degrees) stands the log up as a post. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement", meta = (ClampMin = "20.0", ClampMax = "89.0"))
    float PostLookDownDeg = 55.0f;

    /** How close (cm, from a log's surface) the aim must be for that log to be meant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement", meta = (ClampMin = "0.0"))
    float ContextReachCm = 40.0f;

    /** The axe chooses the cut from where it lands: an end face splits, the side cuts across. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Processing")
    bool bContextualCut = true;

    /** What the carried log does where the player looks. Camera free, so tests can ask it directly. */
    FLogContextResult ResolveContext(const FLogContextAim& Aim) const;

    /** True after the mode key: the old modes decide until the log is put down. */
    bool IsManualPlacementMode() const { return bManualPlacementMode; }
    ELogContextAction GetContextAction() const { return LastContextAction; }

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

    /**
     * A post set under the end of a placed beam and reaching above it lifts that end
     * onto its top (the beam turns about its other end). Returns the beams raised.
     */
    int32 RaiseBeamsOntoPost(ALogBuildPiece* Post);

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

    /**
     * The cut the player's aim chooses on Log. For a length cut the place along the
     * log decides it: the middle of a full log gives two halves, near an end gives
     * 3/4 + 1/4 with the short piece at that end. OutShare is where the saw goes,
     * 0..1 from the mesh's -X end (-1 for split cuts). bOutFlip means the log is
     * turned end for end first, so the short piece comes off the aimed end.
     */
    bool ResolveAimedCut(const ALogBuildPiece* Log, ELogCutAction& OutAction, float& OutShare, bool& bOutFlip) const;

    /** The same, for a point on the log's surface (used by the aim and by tests). */
    bool ResolveCutAtPoint(const ALogBuildPiece* Log, const FVector& WorldPoint,
        ELogCutAction& OutAction, float& OutShare, bool& bOutFlip) const;

    /**
     * The full choice for a point: near a recipe point (1/4, 1/2, 3/4) the recipe
     * cut; elsewhere on a loose log a free cut on a 25 cm step; on a standing post,
     * shortening it from the top. Length cuts only; other cuts are recipes.
     */
    bool ResolveCutChoice(const ALogBuildPiece* Log, const FVector& WorldPoint, FLogCutChoice& Out) const;
    bool ResolveAimedCutChoice(const ALogBuildPiece* Log, FLogCutChoice& Out) const;
    bool CanMakeCut(const ALogBuildPiece* Log, const FLogCutChoice& Choice, FString& OutReason) const;
    /** Player-facing name of the cut, with the resulting lengths. */
    FString DescribeCut(const ALogBuildPiece* Log, const FLogCutChoice& Choice) const;
    bool GetArmedCutChoice(const ALogBuildPiece*& OutLog, FLogCutChoice& OutChoice) const;

    /**
     * Cuts Log in two at Share (0..1 from the mesh's -X end). A loose log becomes two
     * loose pieces. A standing post keeps its lower part; the top falls off and
     * every piece resting on the top comes down by the cut length (a beam on two
     * posts tilts about its other end).
     */
    bool CutLogAtShare(ALogBuildPiece* Log, float Share);

    /** A second log carried on the shoulder; it comes to the hands when the first is put down. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    TObjectPtr<ALogBuildPiece> ShoulderPiece;

    /** Takes a second log onto the shoulder while carrying one (Sons of the Forest: two logs). */
    bool TryShoulderLog(ALogBuildPiece* Target);
    /** Moves the shoulder log to the hands once they are empty. */
    void PromoteShoulderLog();
    int32 GetCarriedLogCount() const { return (HeldPiece ? 1 : 0) + (ShoulderPiece ? 1 : 0); }

    /** A wall of logs stands at most this many rows high (profile log.parallel_stack, param maxRows). */
    int32 GetMaxWallRows() const;

    /** Rows in the wall Target belongs to, counting Target and the parallel logs under it. */
    int32 CountWallRows(const ALogBuildPiece* Target) const;

    /** True when anything rests on the post: a cut there is refused (Sons of the Forest). */
    bool PostHoldsAnything(const ALogBuildPiece* Post) const;

    /** Split, quartered and plank pieces cannot stand as posts; round and hewn logs can. */
    static bool CanStandAsPost(const ALogBuildPiece* Log);

    /**
     * Splits the round log in the hands lengthwise (as the game does when a full log
     * is used for a floor, ramp or door): one half stays in the hands, the other goes
     * on the shoulder, or to the ground when the shoulder is taken.
     */
    bool SplitCarriedLogInHalf();

    /** Badly damaged pieces take no new parts until repaired. */
    bool IsBadlyDamaged(const AActor* Piece) const;

    /**
     * A strut (payanda) from a post up to the beam resting on it, on the side of
     * AimPoint: from 90 cm down the post to 90 cm along the beam. OutBeamPoint is
     * where it meets the beam.
     */
    bool ComputeStrutPlacement(const ALogBuildPiece* Held, const ALogBuildPiece* Post, const ALogBuildPiece* Beam,
        const FVector& AimPoint, FTransform& OutTransform, float& OutLength, FVector& OutBeamPoint) const;

    /** Placed beams (lying, not upright) resting on the top of Post. */
    void FindBeamsOnPost(const ALogBuildPiece* Post, TArray<ALogBuildPiece*>& OutBeams) const;

    /**
     * Moves a post's base so it stands one log length (150/300/450/600 cm) from the
     * nearest standing post when it is close to that, so a wall fits between them.
     */
    bool SnapPostSpacing(const FVector& Base, FVector& OutBase, ALogBuildPiece*& OutPartner, float& OutSpan) const;

    /** The post the carried post lines up with, and the span between them. */
    bool GetPostSpan(const ALogBuildPiece*& OutPartner, float& OutSpan) const;

    /** White dashes on the ground: a rectangle under a lying log, a circle under a post, and a line to a post in line. */
    bool BuildGroundMarkerSegments(TArray<FVector>& OutPoints) const;

    /** Dashed cone on the top of a standing log where it will be sharpened. */
    bool BuildSharpenSegments(const ALogBuildPiece* Log, TArray<FVector>& OutPoints) const;

    bool HasStrutPlacement() const { return bHasStrutPlacement; }

    /** Dashed ring where a length cut at Share (0..1 from -X) would go. */
    bool BuildCutRingSegments(const ALogBuildPiece* Log, float Share, TArray<FVector>& OutSegmentPoints) const;

    /** The cut marked by the first F press: F again cuts it, C or another log cancels. */
    bool GetArmedCut(const ALogBuildPiece*& OutLog, ELogCutAction& OutAction, float& OutShare) const;
    void CancelArmedCut();

    /**
     * White dashed outline of the carried log where it will be put down, and when it
     * joins another piece, an arrow (pairs of points) pointing into the joint.
     */
    bool BuildPlacementPreviewSegments(TArray<FVector>& OutOutline, TArray<FVector>& OutArrow) const;

    /** True when the carried log is joined to a piece (snap or grid stack), not free placed. */
    bool HasConnectionPreview() const { return bHasStackPlacement || bHasValidSnap || bHasStrutPlacement; }

    /** Throw speed (cm/s) for a 20 kg piece; heavier pieces fly slower. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics", meta = (ClampMin = "100.0"))
    float ThrowSpeed = 1400.0f;

    /** Throws the carried log, stone or stick along the view; fast hits deal damage. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool ThrowHeldPiece();

    /** Picks up a specific loose piece without aiming at it. */
    bool TryPickupPiece(AActor* Target);

    /**
     * Places the carried log at an exact transform resting on PrimarySupport (and
     * SecondarySupport). RestingOnPiece will also rest on the placed log (a beam on
     * a strut). All or nothing: when any of these links fails, the log stays in the hands.
     */
    bool CommitHeldLogAt(const FTransform& WorldTransform, AActor* PrimarySupport, AActor* SecondarySupport,
        AActor* RestingOnPiece = nullptr);

    /** Hands the carried piece to a tool (storage, fire, zipline); it stays held in the ledger. */
    AActor* ReleaseHeldToTool();

    /** Links supports found at both ends: beams on posts, braces under beams, logs on trees. */
    void ApplyEndContactSupports(ALogBuildPiece* Piece);

    void SetPlacementStatus(const FString& NewStatus) { PlacementStatus = NewStatus; }

    /**
     * Places the carried log exactly as the placement kernel solved it (stack,
     * floor, strut, step, plan slot ...), all or nothing. The grid, interlock and
     * other numbers live in Config/Placement/PlacementProfiles.json.
     */
    bool CommitHeldSolution(const FPlacementSolution& Solution);

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
    // Default to aim-resolved length cutting; recipe operations remain optional overrides via CycleCutAction().
    ELogCutAction SelectedCutAction = ELogCutAction::SplitLengthHalf;
    FTransform CurrentSnapActorTransform = FTransform::Identity;
    /** The placement kernel's answer shown now; E commits exactly this. */
    FPlacementSolution CurrentSolution;
    bool bManualPlacementMode = false;
    ELogContextAction LastContextAction = ELogContextAction::None;
    FTransform LastContextTransform = FTransform::Identity;
    bool bHasDeckPlacement = false;
    FPlacementSolution DeckSolution;
    TWeakObjectPtr<AActor> DeckSupport;
    FVector DeckAim = FVector::ZeroVector;
    FVector DeckView = FVector::ForwardVector;
    void UpdateContextPlacement(float DeltaTime);
    void ApplyContextResult(const FLogContextResult& Result);
    FPlacementSolution StackSolution;
    FPlacementSolution StrutSolution;
    FPlacementSolution MaterialSolution;
    FDelegateHandle PlacementCommittedHandle;

    class UPlacementKernelSubsystem* GetKernel() const;
    FPlacementSolution SolveStackOn(ALogBuildPiece* Held, ALogBuildPiece* Target, const FVector& AimPoint, float CarryYaw) const;
    FPlacementSolution SolveStrut(const ALogBuildPiece* Held, const ALogBuildPiece* Aimed, const ALogBuildPiece* Beam,
        const FVector& AimPoint) const;
    FPlacementSolution SolveMaterialOn(AActor* Target, const FVector& AimPoint) const;
    /** Every placement, by anyone: end contacts become supports, beams rise onto new posts, fallen ends lift. */
    void HandlePlacementCommitted(AActor* Piece, const FPlacementSolution& Solution);
    TWeakObjectPtr<ALogBuildPiece> DraggedPiece;
    bool bIsDraggingLog = false;

    bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
    bool TraceFromView(float Distance, FHitResult& OutHit) const;
    void UpdateHeldPreview();
    void FindBestSnapCandidate();
    void GatherContextualSnapCandidates(const FVector& AimPoint, AActor* DirectAimActor,
        TArray<AActor*>& OutCandidates, ALogBuildPiece*& OutPreferredTarget) const;
    void ClearSnapCandidate();
    void SetSnapCandidate(ALogBuildPiece* Target, int32 SourceIndex,
        int32 TargetIndex, const FTransform& DesiredActorTransform);
    FRotator MakePlacementRotation(float ViewYaw) const;
    void ProcessAimedPieceInput();
    void PushOrRollAimedLogInput();
    void BeginDragAimedLogInput();
    void ReleaseHeldFromLoad();

    FString PlacementStatus;

    TWeakObjectPtr<ALogBuildPiece> ArmedLog;
    ELogCutAction ArmedAction = ELogCutAction::SplitHalf;
    float ArmedShare = -1.0f;
    bool bArmedFlipPending = false;
    ELogCutKind ArmedKind = ELogCutKind::Recipe;
    float ArmedWork = 0.0f;

    bool bHasStrutPlacement = false;
    FTransform StrutTransform = FTransform::Identity;
    float StrutLength = 0.0f;
    FVector StrutJoint = FVector::ZeroVector;
    TWeakObjectPtr<ALogBuildPiece> StrutPost;
    TWeakObjectPtr<ALogBuildPiece> StrutBeam;
    void UpdateStrutPlacement();
    bool PlaceStrut();

    TWeakObjectPtr<ALogBuildPiece> SpanPartner;
    float SpanCm = 0.0f;

    /** Lowers the pieces that rested on a post's old top by Drop cm. */
    void LowerPiecesOnPost(ALogBuildPiece* Post, const FVector& OldTop, float Drop, const TArray<AActor*>& Children);

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
