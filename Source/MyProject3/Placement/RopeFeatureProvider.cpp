#include "Placement/PlacementFeatureProvider.h"

#include "RopeLineActor.h"

/**
 * Ropes for the placement kernel: a long body boards can rest on (rope
 * bridges). A rope has no joints of its own.
 */
class FRopeFeatureProvider : public IPlacementFeatureProvider
{
public:
    virtual bool Supports(const AActor* Piece) const override { return Cast<ARopeLineActor>(Piece) != nullptr; }
    virtual FName GetFamily(const AActor* Piece) const override { return TEXT("rope"); }

    virtual uint32 GetGeometryRevision(const AActor* Piece) const override
    {
        const ARopeLineActor* Rope = Cast<ARopeLineActor>(Piece);
        if (!Rope)
            return 0;
        auto Round = [](const FVector& Point) { return FIntVector(FMath::RoundToInt(Point.X), FMath::RoundToInt(Point.Y), FMath::RoundToInt(Point.Z)); };
        return HashCombine(GetTypeHash(Round(Rope->GetStart())), GetTypeHash(Round(Rope->GetEnd())));
    }

    virtual void GetFeatures(const AActor* Piece, TArray<FPlacementFeature>& OutFeatures) const override { OutFeatures.Reset(); }
    virtual bool CanLink(const AActor*, const FPlacementFeature&, const AActor*, const FPlacementFeature&) const override { return false; }
    virtual bool CommitLink(AActor*, const FPlacementFeature&, AActor*, const FPlacementFeature&) const override { return false; }
    virtual void RevertLink(AActor*, const FPlacementFeature&, AActor*, const FPlacementFeature&) const override {}

    virtual bool IsPlaced(const AActor* Piece) const override
    {
        const ARopeLineActor* Rope = Cast<ARopeLineActor>(Piece);
        return Rope && Rope->IsStructurallyPlaced();
    }

    virtual bool GetBody(const AActor* Piece, FPlacementBody& OutBody) const override
    {
        const ARopeLineActor* Rope = Cast<ARopeLineActor>(Piece);
        if (!Rope)
            return false;
        const FVector Line = Rope->GetEnd() - Rope->GetStart();
        if (Line.IsNearlyZero())
            return false;
        OutBody.Centre = 0.5f * (Rope->GetStart() + Rope->GetEnd());
        OutBody.Axis = Line.GetSafeNormal();
        OutBody.HalfLength = 0.5f * Line.Size();
        OutBody.Radius = 3.0f;
        return true;
    }
};

REGISTER_PLACEMENT_PROVIDER(FRopeFeatureProvider)
