#pragma once

#include "CoreMinimal.h"
#include "Placement/PlacementTypes.h"

enum class EWorldItemOwnerKind : uint8;

/**
 * What a material tells the placement kernel about its pieces. Adding a new
 * material means writing one of these (and profiles), not changing the kernel.
 * Providers are stateless; all state stays on the piece actors.
 */
class MYPROJECT3_API IPlacementFeatureProvider
{
public:
    virtual ~IPlacementFeatureProvider() = default;

    /** True when this provider describes Piece. */
    virtual bool Supports(const AActor* Piece) const = 0;

    /** Material family used by profiles ("log", "stone", "stick", "rope"). */
    virtual FName GetFamily(const AActor* Piece) const = 0;

    /** Changes whenever the piece's shape changes (cut, new mesh, rescale). */
    virtual uint32 GetGeometryRevision(const AActor* Piece) const = 0;

    /** Every feature of the piece, in world space. */
    virtual void GetFeatures(const AActor* Piece, TArray<FPlacementFeature>& OutFeatures) const = 0;

    /** True when Target can take Source's feature right now (rules owned by the material). */
    virtual bool CanLink(const AActor* Target, const FPlacementFeature& TargetFeature,
        const AActor* Source, const FPlacementFeature& SourceFeature) const = 0;

    /** Makes the link (reserve both features, attach, structural edge). False leaves nothing changed. */
    virtual bool CommitLink(AActor* Target, const FPlacementFeature& TargetFeature,
        AActor* Source, const FPlacementFeature& SourceFeature) const = 0;

    /** Undoes a link made by CommitLink. */
    virtual void RevertLink(AActor* Target, const FPlacementFeature& TargetFeature,
        AActor* Source, const FPlacementFeature& SourceFeature) const = 0;

    /** True when the piece is built in (other pieces may rest on it). */
    virtual bool IsPlaced(const AActor* Piece) const = 0;

    /** The piece as a long body (logs, sticks, ropes). False for shapes that are not long. */
    virtual bool GetBody(const AActor* Piece, FPlacementBody& OutBody) const { return false; }

    /** Actor transform that puts the body's centre at Pose.Centre along Pose.Axis with Pose.Up up. */
    virtual FTransform MakeBodyTransform(const AActor* Piece, const FPlacementPose& Pose) const;

    /** Material rule: may this piece stand upright as a post? */
    virtual bool CanStandUpright(const AActor* Piece) const { return true; }

    /** Moves the piece to the world in the ownership ledger; the last step of a commit. */
    virtual bool TransferToWorld(AActor* Piece, EWorldItemOwnerKind From) const;

    /** After a commit: the piece's own placed flags and its attachment to Parent (may be null). */
    virtual void ApplyPlacedState(AActor* Piece, AActor* Parent) const {}

    /** True when the material sets its pieces on the ground itself (state, graph, lifecycle). */
    virtual bool HasOwnGroundPlacement() const { return false; }
    virtual bool CommitGroundPlacement(AActor* Piece, const FTransform& Transform) const { return false; }
};

/** Providers register themselves once at module load; the kernel only looks them up. */
class MYPROJECT3_API FPlacementProviderRegistry
{
public:
    static void Register(TSharedRef<IPlacementFeatureProvider> Provider);
    static const IPlacementFeatureProvider* Find(const AActor* Piece);
    static bool FindFeature(const AActor* Piece, FName FeatureId, FPlacementFeature& OutFeature);
    /** The piece as a body, through its provider. */
    static bool GetBody(const AActor* Piece, FPlacementBody& OutBody);
    static bool IsPlaced(const AActor* Piece);
    static int32 Num();
};

/** Registers a provider type at module load: REGISTER_PLACEMENT_PROVIDER(FLogFeatureProvider) */
#define REGISTER_PLACEMENT_PROVIDER(ProviderType) \
    namespace { struct F##ProviderType##Registrar { F##ProviderType##Registrar() { \
        FPlacementProviderRegistry::Register(MakeShared<ProviderType>()); } } G##ProviderType##Registrar; }
