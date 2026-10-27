#pragma once

#include "CoreMinimal.h"
#include "Placement/PlacementFeatureProvider.h"

class ALogBuildPiece;

/**
 * Logs for the placement kernel. Until the 75 cm switch (plan, step 8) the
 * features are the log's existing snap points, so the game behaves exactly as
 * before; BuildModularFeatures is the 75 cm model the switch will use.
 */
class MYPROJECT3_API FLogFeatureProvider : public IPlacementFeatureProvider
{
public:
    virtual bool Supports(const AActor* Piece) const override;
    virtual FName GetFamily(const AActor* Piece) const override;
    virtual uint32 GetGeometryRevision(const AActor* Piece) const override;
    virtual void GetFeatures(const AActor* Piece, TArray<FPlacementFeature>& OutFeatures) const override;
    virtual bool CanLink(const AActor* Target, const FPlacementFeature& TargetFeature,
        const AActor* Source, const FPlacementFeature& SourceFeature) const override;
    virtual bool CommitLink(AActor* Target, const FPlacementFeature& TargetFeature,
        AActor* Source, const FPlacementFeature& SourceFeature) const override;
    virtual void RevertLink(AActor* Target, const FPlacementFeature& TargetFeature,
        AActor* Source, const FPlacementFeature& SourceFeature) const override;
    virtual bool IsPlaced(const AActor* Piece) const override;
    virtual bool GetBody(const AActor* Piece, FPlacementBody& OutBody) const override;
    virtual FTransform MakeBodyTransform(const AActor* Piece, const FPlacementPose& Pose) const override;
    /** Split, quartered and plank pieces cannot stand as posts; round and hewn logs can. */
    virtual bool CanStandUpright(const AActor* Piece) const override;
    virtual bool TransferToWorld(AActor* Piece, EWorldItemOwnerKind From) const override;
    virtual void ApplyPlacedState(AActor* Piece, AActor* Parent) const override;

    static const FName Family;

    /**
     * Two end features (End.A at the -axis end, End.B) and one body station per
     * complete module (Station.00 ...), from the log's real world length, so a
     * log shortened by scale keeps 75 cm between stations in the world.
     */
    static void BuildModularFeatures(const ALogBuildPiece* Log, float ModuleCm, TArray<FPlacementFeature>& OutFeatures);
};
