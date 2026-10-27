#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Placement/PlacementProfiles.h"
#include "Placement/PlacementTypes.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "PlacementKernelSubsystem.generated.h"

class FPlacementTransaction;
class IPlacementFeatureProvider;

/** Where the piece comes from when it is committed. */
struct FPlacementCommitOptions
{
    /** Ledger owner before the commit: Held (player hands), InTransit (helper) ... */
    EWorldItemOwnerKind FromOwner = EWorldItemOwnerKind::Held;
    /** Lifecycle before the commit: Held for the player, World for the helper. */
    EWorldObjectLifecycleState FromLifecycle = EWorldObjectLifecycleState::Held;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlacementCommitted, AActor* /*Piece*/, const FPlacementSolution& /*Solution*/);

/**
 * The one place where "where does this piece go" is decided, for every
 * material and every placer (player, helper, plans). Solve is pure: it changes
 * nothing. The preview draws its result, and CommitPlacement applies exactly
 * that result, all or nothing.
 *
 * The kernel never names a material: features and bodies come from providers
 * (FPlacementProviderRegistry), rules and numbers from profiles
 * (Config/Placement/PlacementProfiles.json).
 */
UCLASS()
class MYPROJECT3_API UPlacementKernelSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** The best valid placement for the intent among the allowed profiles, or an invalid one with the reason. */
    FPlacementSolution Solve(const FPlacementIntent& Intent, const FPlacementSolution* Previous = nullptr) const;

    /** False when a piece or feature in the solution changed since it was solved. */
    bool IsStillCurrent(const FPlacementSolution& Solution, FString& OutReason) const;

    /** Adds every feature link of the solution to Tx with its undo; false (Tx rolled back) when one fails. */
    bool CommitLinks(const FPlacementSolution& Solution, FPlacementTransaction& Tx, FString& OutReason) const;

    /**
     * Places Piece as the solution says: transform, joints, structural links,
     * lifecycle and ownership, all or nothing. On success the piece's own state
     * is updated and OnPlacementCommitted fires.
     */
    bool CommitPlacement(AActor* Piece, const FPlacementSolution& Solution, const FPlacementCommitOptions& Options,
        FString& OutReason);

    /**
     * Sets Piece on the ground (or a non-structural surface) at Transform. Materials
     * that place themselves do so; others go through CommitPlacement with no support.
     */
    bool CommitGroundPlacement(AActor* Piece, const FTransform& Transform, const FPlacementCommitOptions& Options,
        FString& OutReason);

    /**
     * A solution for a place decided elsewhere (a plan slot, a tested pose): the
     * piece goes to Transform and rests on the given pieces.
     */
    FPlacementSolution MakeDirectSolution(AActor* Piece, const FTransform& Transform, FName ProfileId,
        AActor* Primary, AActor* Secondary = nullptr, AActor* Carried = nullptr) const;

    /** Actor transform that lays Piece's body along Pose (through its provider). */
    FTransform MakeBodyTransform(const AActor* Piece, const FPlacementPose& Pose) const;

    // Material-neutral questions about built pieces.

    /** Placed pieces lying on the top of Post. */
    void FindRestingOnTop(const AActor* Post, TArray<AActor*>& OutPieces) const;
    /** Rows in the wall Top belongs to, counting Top and the parallel pieces under it. */
    int32 CountParallelRows(const AActor* Top, int32 MaxRows) const;
    /** Badly damaged pieces take no new parts until repaired. */
    bool IsDamaged(const AActor* Piece) const;
    /** False when a piece other than Ignore overlaps the sphere. */
    bool IsSpaceFree(const FVector& Centre, float Radius, const TArray<const AActor*>& Ignore, bool bPlacedOnly) const;
    /** Every built-in piece with a body, near Centre (all of them when RadiusCm <= 0). */
    void GatherPlacedBodies(const FVector& Centre, float RadiusCm, TArray<AActor*>& OutPieces) const;

    const FPlacementProfileRegistry& GetProfiles() const { return Profiles; }
    FPlacementProfileRegistry& GetMutableProfiles() { return Profiles; }

    /** Fires after every successful CommitPlacement (end contacts, raised beams, lifted ends react to it). */
    FOnPlacementCommitted OnPlacementCommitted;

    /** How much closer (cm) a new snap candidate must be before the current one is dropped. */
    UPROPERTY(EditAnywhere, Category = "Placement")
    float StickyBonusCm = 12.0f;

    static const TCHAR* DamagedReason();

    /** The world frame a joint uses at Feature: a station's side is the one facing Toward, rounded to StepDeg. */
    static FTransform ResolveFeatureFrame(const FPlacementFeature& Feature, const FVector& Toward, float StepDeg);

private:
    FPlacementProfileRegistry Profiles;

    float GetParam(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FName Name, float Default) const;

    void SolveSnap(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
        const FPlacementSolution* Previous, FPlacementSolution& InOutBest) const;
    void SolveAnchor(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FPlacementSolution& InOutBest) const;
    void SolveStack(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FPlacementSolution& InOutBest) const;
    void SolveDeck(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FPlacementSolution& InOutBest) const;
    void SolveStep(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FPlacementSolution& InOutBest) const;
    void SolveGable(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FPlacementSolution& InOutBest) const;
    void SolveStrut(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FPlacementSolution& InOutBest) const;
    void SolvePost(const FPlacementProfile& Profile, const FPlacementIntent& Intent, FPlacementSolution& InOutBest) const;

    /** Keeps Candidate when it is valid and better than InOutBest; an invalid one only adds its reason. */
    static void Offer(FPlacementSolution&& Candidate, FPlacementSolution& InOutBest);

public:
    /** A structural link between whole bodies (no anchor reserved). */
    static FPlacementLink MakeBodyLink(const AActor* Held, const AActor* Target, EPlacementLinkRole Role);
};
