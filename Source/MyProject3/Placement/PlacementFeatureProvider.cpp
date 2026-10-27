#include "Placement/PlacementFeatureProvider.h"

#include "ConstructionHelpers.h"
#include "WorldItemOwnershipSubsystem.h"

namespace
{
    TArray<TSharedRef<IPlacementFeatureProvider>>& Providers()
    {
        static TArray<TSharedRef<IPlacementFeatureProvider>> List;
        return List;
    }
}

FTransform IPlacementFeatureProvider::MakeBodyTransform(const AActor* Piece, const FPlacementPose& Pose) const
{
    // Default: the actor's X runs along the body and its origin is the body centre.
    const FQuat Rotation = FRotationMatrix::MakeFromXZ(Pose.Axis.GetSafeNormal(), Pose.Up).ToQuat();
    return FTransform(Rotation, Pose.Centre, Piece ? Piece->GetActorScale3D() : FVector::OneVector);
}

bool IPlacementFeatureProvider::TransferToWorld(AActor* Piece, EWorldItemOwnerKind From) const
{
    return MyProject3Construction::TransferPiece(Piece, EWorldItemOwnerKind::World, FGuid(),
        Piece ? Piece->GetActorLocation() : FVector::ZeroVector);
}

void FPlacementProviderRegistry::Register(TSharedRef<IPlacementFeatureProvider> Provider)
{
    Providers().Add(Provider);
}

const IPlacementFeatureProvider* FPlacementProviderRegistry::Find(const AActor* Piece)
{
    if (!IsValid(Piece))
        return nullptr;
    for (const TSharedRef<IPlacementFeatureProvider>& Provider : Providers())
        if (Provider->Supports(Piece))
            return &Provider.Get();
    return nullptr;
}

bool FPlacementProviderRegistry::FindFeature(const AActor* Piece, FName FeatureId, FPlacementFeature& OutFeature)
{
    const IPlacementFeatureProvider* Provider = Find(Piece);
    if (!Provider)
        return false;
    TArray<FPlacementFeature> Features;
    Provider->GetFeatures(Piece, Features);
    for (const FPlacementFeature& Feature : Features)
    {
        if (Feature.Id == FeatureId)
        {
            OutFeature = Feature;
            return true;
        }
    }
    return false;
}

bool FPlacementProviderRegistry::GetBody(const AActor* Piece, FPlacementBody& OutBody)
{
    const IPlacementFeatureProvider* Provider = Find(Piece);
    return Provider && Provider->GetBody(Piece, OutBody);
}

bool FPlacementProviderRegistry::IsPlaced(const AActor* Piece)
{
    const IPlacementFeatureProvider* Provider = Find(Piece);
    return Provider && Provider->IsPlaced(Piece);
}

int32 FPlacementProviderRegistry::Num()
{
    return Providers().Num();
}
