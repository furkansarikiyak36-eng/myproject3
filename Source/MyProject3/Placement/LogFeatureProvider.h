#pragma once

#include "CoreMinimal.h"
#include "Placement/PlacementFeatureProvider.h"

class ALogBuildPiece;

/**
 * Logs for the placement kernel. A log has two end faces (End.A at the -axis
 * end, End.B) and one body station per complete 75 cm module of its real
 * length (Station.00 at 37.5 cm from End.A, then every 75 cm: a 600 cm log has
 * 8). Hand-made points on a mesh come along as Authored features. Joints are
 * made by id, never by array position.
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

};
