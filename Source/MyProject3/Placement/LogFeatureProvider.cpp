#include "Placement/LogFeatureProvider.h"

#include "Components/StaticMeshComponent.h"
#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "Placement/PlacementSolvers.h"
#include "WorldItemOwnershipSubsystem.h"

REGISTER_PLACEMENT_PROVIDER(FLogFeatureProvider)

const FName FLogFeatureProvider::Family(TEXT("log"));

namespace
{
    const ALogBuildPiece* AsLog(const AActor* Piece) { return Cast<ALogBuildPiece>(Piece); }

    EPlacementFeatureKind ClassifyPoint(const FLogSnapPoint& Point)
    {
        const FString Id = Point.PointId.ToString();
        if (Id.StartsWith(TEXT("End.")))
            return EPlacementFeatureKind::End;
        if (Id.StartsWith(TEXT("Station.")))
            return EPlacementFeatureKind::Station;
        return EPlacementFeatureKind::Authored;
    }
}

bool FLogFeatureProvider::Supports(const AActor* Piece) const
{
    return AsLog(Piece) != nullptr;
}

FName FLogFeatureProvider::GetFamily(const AActor* Piece) const
{
    return Family;
}

FName FLogFeatureProvider::GetSection(const AActor* Piece) const
{
    const ALogBuildPiece* Log = AsLog(Piece);
    return Log ? FName(*StaticEnum<ELogCrossSectionProfile>()->GetNameStringByValue(static_cast<int64>(Log->GetCrossSectionProfile())))
               : FName(NAME_None);
}

uint32 FLogFeatureProvider::GetGeometryRevision(const AActor* Piece) const
{
    const ALogBuildPiece* Log = AsLog(Piece);
    if (!Log)
        return 0;
    const UStaticMeshComponent* Mesh = Log->GetLogMesh();
    uint32 Hash = GetTypeHash(Mesh ? Mesh->GetStaticMesh() : nullptr);
    // Scale shortens a freely cut log; rounded so float noise does not count as a change.
    const FVector Scale = Log->GetActorScale3D() * 1000.0;
    Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt(Scale.X), FMath::RoundToInt(Scale.Y), FMath::RoundToInt(Scale.Z))));
    Hash = HashCombine(Hash, GetTypeHash(Log->GetSnapPoints().Num()));
    Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Log->LongitudinalAxis)));
    return Hash;
}

void FLogFeatureProvider::GetFeatures(const AActor* Piece, TArray<FPlacementFeature>& OutFeatures) const
{
    OutFeatures.Reset();
    const ALogBuildPiece* Log = AsLog(Piece);
    if (!Log)
        return;
    // A query only: stations are laid out again by the cut that changes the log
    // (RefreshStationsIfStale), never here; a stale preview is caught by the revision.
    MyProject3Construction::FLogGeometry Shape;
    const bool bHasShape = MyProject3Construction::GetLogGeometry(Log, Shape);
    const TArray<FLogSnapPoint>& Points = Log->GetSnapPoints();
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        const FLogSnapPoint& Point = Points[Index];
        FPlacementFeature Feature;
        Feature.Id = Point.PointId.IsNone() ? FName(*FString::Printf(TEXT("Point.%02d"), Index)) : Point.PointId;
        Feature.Kind = ClassifyPoint(Point);
        Feature.Tag = Point.SnapTag;
        Feature.LocalFrame = Point.LocalTransform;
        Feature.WorldFrame = Log->GetSnapPointWorldTransform(Index);
        Feature.bFlipIncoming = Point.bFlipIncomingOrientation;
        Feature.bCanSource = Point.bCanSource || Feature.Kind == EPlacementFeatureKind::Station;
        Feature.bCanReceive = Point.bCanReceive;
        Feature.bOccupied = Point.bOccupied;
        if (Feature.Kind == EPlacementFeatureKind::Station)
        {
            Feature.bRadialFree = true;
            Feature.RadiusCm = bHasShape ? Shape.Radius : 0.0f;
            // As a source (station to end) the station joins with the piece's underside.
            const FVector Along = MyProject3Construction::AxisVector(Log->LongitudinalAxis);
            const FVector Down = Log->LongitudinalAxis == ELogLongitudinalAxis::Z ? -FVector::XAxisVector : -FVector::ZAxisVector;
            const UStaticMeshComponent* Mesh = Log->GetLogMesh();
            const float LocalRadius = Mesh && Mesh->GetStaticMesh()
                ? FVector::DotProduct(Mesh->GetStaticMesh()->GetBounds().BoxExtent, Down.GetAbs()) : 0.0f;
            Feature.SideLocalFrame = FTransform(FRotationMatrix::MakeFromXZ(Down, Along).ToQuat(),
                Point.LocalTransform.GetLocation() + Down * LocalRadius);
        }
        OutFeatures.Add(Feature);
    }
}

bool FLogFeatureProvider::CanLink(const AActor* Target, const FPlacementFeature& TargetFeature,
    const AActor* Source, const FPlacementFeature& SourceFeature) const
{
    const ALogBuildPiece* TargetLog = AsLog(Target);
    const ALogBuildPiece* SourceLog = AsLog(Source);
    if (!TargetLog || !SourceLog || TargetLog == SourceLog || !TargetLog->IsPlaced() || TargetLog->IsHeld())
        return false;
    return TargetLog->CanJoinById(TargetFeature.Id, SourceLog, SourceFeature.Id);
}

bool FLogFeatureProvider::CommitLink(AActor* Target, const FPlacementFeature& TargetFeature,
    AActor* Source, const FPlacementFeature& SourceFeature) const
{
    ALogBuildPiece* TargetLog = Cast<ALogBuildPiece>(Target);
    ALogBuildPiece* SourceLog = Cast<ALogBuildPiece>(Source);
    // Attaches, reserves both points and adds the graph edge, or changes nothing.
    return TargetLog && SourceLog && TargetLog->TryJoinById(TargetFeature.Id, SourceLog, SourceFeature.Id);
}

bool FLogFeatureProvider::RevertLink(AActor* Target, const FPlacementFeature& TargetFeature,
    AActor* Source, const FPlacementFeature& SourceFeature) const
{
    ALogBuildPiece* SourceLog = Cast<ALogBuildPiece>(Source);
    // The graph edge and the actor link go together; a forced local cleanup counts as not undone.
    return SourceLog && SourceLog->DetachFromSnapParent();
}

bool FLogFeatureProvider::IsPlaced(const AActor* Piece) const
{
    const ALogBuildPiece* Log = AsLog(Piece);
    return Log && Log->IsPlaced() && !Log->IsHeld();
}

bool FLogFeatureProvider::GetBody(const AActor* Piece, FPlacementBody& OutBody) const
{
    MyProject3Construction::FLogGeometry Shape;
    if (!MyProject3Construction::GetLogGeometry(AsLog(Piece), Shape))
        return false;
    OutBody.Centre = Shape.Centre;
    OutBody.Axis = Shape.Axis;
    OutBody.HalfLength = Shape.HalfLength;
    OutBody.Radius = Shape.Radius;
    return true;
}

FTransform FLogFeatureProvider::MakeBodyTransform(const AActor* Piece, const FPlacementPose& Pose) const
{
    const ALogBuildPiece* Log = AsLog(Piece);
    // The log's own long axis and mesh pivot decide how its actor turns and sits.
    return MyProject3Construction::MakeLogTransform(Log, Pose.Centre,
        MyProject3Construction::MakeLogRotation(Log, Pose.Axis, Pose.Up));
}

bool FLogFeatureProvider::CanStandUpright(const AActor* Piece) const
{
    const ALogBuildPiece* Log = AsLog(Piece);
    if (!Log)
        return false;
    switch (Log->GetCrossSectionProfile())
    {
    case ELogCrossSectionProfile::HalfRound:
    case ELogCrossSectionProfile::QuarterRound:
    case ELogCrossSectionProfile::Plank:
    case ELogCrossSectionProfile::GroovedHalfRound:
    case ELogCrossSectionProfile::Trough:
        return false;
    default:
        return true;
    }
}

bool FLogFeatureProvider::TransferToWorld(AActor* Piece, EWorldItemOwnerKind From) const
{
    ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece);
    FWorldOwnedItemRecord Record;
    return Log && Log->TryTransferOwnership(From, EWorldItemOwnerKind::World, FGuid(), Log->GetActorLocation(), Record);
}

void FLogFeatureProvider::ApplyPlacedState(AActor* Piece, AActor* Parent) const
{
    ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece);
    if (!Log)
        return;
    Log->SetHeldState(false);
    Log->SetPlacedState(true);
    // A snap joint attaches by itself; a piece resting on a support follows it.
    if (Parent && !Log->GetAttachParentActor())
        Log->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
}
