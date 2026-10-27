#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlacementKernelSubsystem.generated.h"

class AActor;
class UWorld;

enum class EPlacementOperationKind : uint8
{
    Connection,
    SurfacePlacement,
    ReceiverInteraction,
    VoxelOperation,
    GeometryProcessing
};

struct FPlacementProfileDefinition
{
    FString Id;
    FString Family;
    FString OperationKindText;
    FString Geometry;
    FString Description;
    FString Solver;
    FString OrientationPolicy;
    FString PhysicsPolicy;
    FString FailurePolicy;
    float TranslationToleranceCm = 2.0f;
    float AngularToleranceDeg = 8.0f;
    float MaxLengthMismatchCm = 0.0f;
    int32 MinRequiredConnections = 1;
    int32 MinimumDistinctSupports = 0;
    bool bRequiresCollisionValidation = false;
    bool bRequiresStructuralValidation = false;
    bool bAtomicCommit = false;
    bool bAllowsPhysicsFallback = false;
    bool bRequiresExplicitProfile = false;
    bool bRequiresJoineryOrExplicitOverlapPolicy = false;
    TArray<FString> SourceFeatures;
    TArray<FString> TargetFeatures;
    TArray<float> AngleRangeDeg;
    TArray<float> PreferredAnglesDeg;

    bool IsConnection() const { return OperationKindText == TEXT("connection"); }
};

struct FPlacementSolution
{
    bool bValid = false;
    FString ProfileId;
    FTransform ActorWorldTransform = FTransform::Identity;
    float Score = TNumericLimits<float>::Max();
    FString FailureReason;
    int64 SourceGeometryRevision = INDEX_NONE;
    int64 TargetGeometryRevision = INDEX_NONE;
};

UCLASS()
class MYPROJECT3_API UPlacementKernelSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Reloads the versioned runtime catalogue from Config/Placement. */
    bool ReloadProfileCatalog(FString* OutFailureReason = nullptr);
    bool IsCatalogReady() const { return bCatalogReady; }
    bool HasProfile(const FString& ProfileId) const;
    bool GetProfile(const FString& ProfileId, FPlacementProfileDefinition& OutProfile) const;
    bool ProfileRequiresCollisionValidation(const FString& ProfileId) const;

    /** Aligns one source-local feature frame with one resolved target-world frame. */
    bool SolveAnchorPair(
        const FString& ProfileId,
        const FTransform& SourceLocalAnchor,
        const FTransform& TargetWorldAnchor,
        const FTransform& CurrentActorWorldTransform,
        FPlacementSolution& OutSolution,
        FString& OutFailureReason) const;

    /**
     * Solves a rigid two-anchor relation without scaling or stretching the source
     * piece. The source/target span mismatch must fit the profile's explicit limit.
     */
    bool SolveTwoAnchorAlignment(
        const FString& ProfileId,
        const FTransform& SourceLocalAnchorA,
        const FTransform& SourceLocalAnchorB,
        const FTransform& TargetWorldAnchorA,
        const FTransform& TargetWorldAnchorB,
        const FTransform& CurrentActorWorldTransform,
        FPlacementSolution& OutSolution,
        FString& OutFailureReason) const;

    /** Conservative broad-phase clearance check; supports must be explicitly ignored. */
    bool ValidateCandidatePose(
        AActor* MovingActor,
        const FTransform& CandidateWorldTransform,
        const TArray<AActor*>& IgnoredActors,
        FString& OutFailureReason) const;

    /** Lower is better. Hard constraints must be validated before scoring. */
    float ScoreCandidate(
        const FPlacementSolution& Solution,
        const FTransform& CurrentActorTransform,
        const FVector& AimPoint,
        const FVector& TargetFeatureWorldLocation) const;

private:
    TMap<FString, FPlacementProfileDefinition> Profiles;
    bool bCatalogReady = false;
    FString CatalogFailureReason;

    bool LoadProfileArray(
        const TSharedPtr<class FJsonObject>& Root,
        const FString& ArrayName,
        TMap<FString, FPlacementProfileDefinition>& InOutProfiles,
        FString& OutFailureReason) const;
};
