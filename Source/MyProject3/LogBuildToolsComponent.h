#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuildPlanActor.h"
#include "ConstructionOpeningActor.h"
#include "Placement/PlacementTypes.h"
#include "LogBuildToolsComponent.generated.h"

class ALogBuildPiece;
class ALogContainerActor;
class ARopeLineActor;
class ULogCarryComponent;
class UInputComponent;

/**
 * Player building tools on top of log carrying: floors, roofs, bridges,
 * stairs and ramps; door, window and gate openings; sharpened stakes;
 * removing supports; ropes and ziplines; building plans; storage, sleds,
 * firewood and throwing.
 */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class MYPROJECT3_API ULogBuildToolsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULogBuildToolsComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools")
    float InteractRange = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools|Openings")
    float DoorWidth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools|Openings")
    float DoorHeight = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools|Openings")
    float WindowWidth = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools|Openings")
    float WindowHeight = 70.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools|Openings")
    float WindowSill = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools|Openings")
    float GateWidth = 120.0f;

    /** Burn time added to a fire per kilogram of wood. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build Tools|Fire")
    float FuelSecondsPerKg = 600.0f;

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    void BindDefaultInput(UInputComponent* InputComponent);

    /** Called first by E; returns true when a building tool used the press. */
    bool TryHandleInteract();

    bool SharpenLog(ALogBuildPiece* Log);
    bool CutOpening(ALogBuildPiece* WallLog, const FVector& AimPoint, EConstructionOpeningKind Kind,
        AConstructionOpeningActor*& OutOpening);
    /** Takes a piece out of a structure when everything on it has another support. */
    bool DismantlePiece(ALogBuildPiece* Piece, bool bChargeSurvivalCost = true);
    /** Where Board would lie across Support and a parallel second support. */
    bool ComputeDeckPlacement(ALogBuildPiece* Board, AActor* Support, const FVector& AimPoint,
        const FVector& ViewDirection, bool bStairs, FTransform& OutTransform, AActor*& OutSecondSupport,
        float& OutSpan, FString& OutReason) const;
    bool TryPlaceDeck(AActor* Support, const FVector& AimPoint, const FVector& ViewDirection, bool bStairs);
    bool TryFillPlanSlot(ABuildPlanActor* Plan, int32 SlotIndex);
    ABuildPlanActor* PlacePlan(EBuildPlanKind Kind, const FVector& Location, float Yaw);

    /** A defensive stake wall plan drawn from one point to another (one stake per 40 cm). */
    ABuildPlanActor* PlaceDefenseLine(const FVector& From, const FVector& To);

    /**
     * One-sided apex: fills the next free row of the triangle under a sloped placed
     * log with the carried log, cut to fit between the slope and its high end.
     */
    bool TryFillApex(ALogBuildPiece* SlopedBeam);

    /** Longest stretch a beam may span between supports; a post leaving more is not removed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction", meta = (ClampMin = "100.0"))
    float MaxBeamSpanCm = 400.0f;

    /** How far a beam may stick out past its last support. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction", meta = (ClampMin = "50.0"))
    float MaxBeamOverhangCm = 220.0f;

    /**
     * The longest unsupported stretch of Beam, counting posts, struts and logs under
     * it, but not Ignore. Struts at the posts shorten it, so middle posts can go.
     */
    float GetLongestUnsupportedSpan(const ALogBuildPiece* Beam, const ALogBuildPiece* Ignore, bool& bOutOverhangTooLong) const;

    /** A single step in front of a floor or beam edge (no second support needed). */
    bool TryPlaceSingleStep(ALogBuildPiece* Edge, const FVector& AimPoint);
    ARopeLineActor* TieRope(const FVector& From, AActor* FromActor, const FVector& To, AActor* ToActor);
    bool FeedFire(AActor* Fire);

    FString GetStatus() const { return Status; }
    bool IsHelpVisible() const { return bShowHelp; }
    EBuildPlanKind GetSelectedPlan() const { return SelectedPlan; }
    static FString GetHelpText();

private:
    /** Floor or stairs across Support and a parallel support, as the placement kernel solves it. */
    FPlacementSolution SolveDeck(ALogBuildPiece* Board, AActor* Support, const FVector& AimPoint,
        const FVector& ViewDirection, bool bStairs) const;
    class UPlacementKernelSubsystem* GetKernel() const;

    FString Status;
    float StatusHold = 0.0f;
    bool bShowHelp = false;
    EBuildPlanKind SelectedPlan = EBuildPlanKind::LogWall;
    bool bRopeStarted = false;
    bool bWallLineStarted = false;
    FVector WallLineStart = FVector::ZeroVector;
    void ApexInput();
    FVector RopeStart = FVector::ZeroVector;
    TWeakObjectPtr<AActor> RopeStartActor;

    ULogCarryComponent* GetCarry() const;
    bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
    bool TraceView(float Distance, FHitResult& OutHit) const;
    void SetStatus(const FString& NewStatus, float HoldSeconds = 4.0f);
    void SetPreviewStatus(const FString& NewStatus);
    bool GetSupportLine(const AActor* Support, FVector& OutA, FVector& OutB, float& OutRadius) const;
    AConstructionOpeningActor* FindOpening(const FHitResult& Hit) const;
    ALogContainerActor* FindContainer(const FHitResult& Hit) const;
    /** A part cut out of a built log stays built, through the placement kernel. */
    void MakePlaced(ALogBuildPiece* Log, const ALogBuildPiece* CutFrom) const;
    void DropReleasedPiece(AActor* Piece) const;
    bool CutGate(ALogBuildPiece* Post, const FVector& AimPoint, AConstructionOpeningActor*& OutOpening);
    void UpdatePreview();

    void SharpenInput();
    void DoorInput();
    void WindowInput();
    void LockInput();
    void RopeInput();
    void TowInput();
    void CyclePlanInput();
    void PlacePlanInput();
    void DismantleInput();
    void ThrowInput();
    void HelpInput();
};
