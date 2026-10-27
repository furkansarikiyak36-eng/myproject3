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

    /** Old snap points: automatic side points are body stations, other end tags are ends. */
    EPlacementFeatureKind ClassifyLegacyPoint(const ALogBuildPiece* Log, const FLogSnapPoint& Point)
    {
        if (Point.PointId.ToString().StartsWith(TEXT("Station.")))
            return EPlacementFeatureKind::Station;
        MyProject3Construction::FLogGeometry Shape;
        if (Point.SnapTag == TEXT("LogEnd") && MyProject3Construction::GetLogGeometry(Log, Shape) && Shape.HalfLength > KINDA_SMALL_NUMBER)
        {
            const FVector World = (Point.LocalTransform * Log->GetActorTransform()).GetLocation();
            const float Along = FMath::Abs(FVector::DotProduct(World - Shape.Centre, Shape.Axis));
            if (Along > 0.5f * Shape.HalfLength)
                return EPlacementFeatureKind::End;
        }
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
    // A log shortened by scale since its stations were laid out gets them again.
    const_cast<ALogBuildPiece*>(Log)->RefreshStationsIfStale();
    MyProject3Construction::FLogGeometry Shape;
    const bool bHasShape = MyProject3Construction::GetLogGeometry(Log, Shape);
    const TArray<FLogSnapPoint>& Points = Log->GetSnapPoints();
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        const FLogSnapPoint& Point = Points[Index];
        FPlacementFeature Feature;
        Feature.Id = Point.PointId.IsNone() ? FName(*FString::Printf(TEXT("Point.%02d"), Index)) : Point.PointId;
        Feature.Kind = ClassifyLegacyPoint(Log, Point);
        Feature.Tag = Point.SnapTag;
        Feature.LocalFrame = Point.LocalTransform;
        Feature.WorldFrame = Log->GetSnapPointWorldTransform(Index);
        Feature.bFlipIncoming = Point.bFlipIncomingOrientation;
        Feature.bCanSource = Point.bCanSource;
        Feature.bCanReceive = Point.bCanReceive;
        Feature.bOccupied = Point.bOccupied;
        Feature.LegacyIndex = Index;
        if (Feature.Kind == EPlacementFeatureKind::Station)
        {
            Feature.bRadialFree = true;
            Feature.RadiusCm = bHasShape ? Shape.Radius : 0.0f;
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
    return TargetLog->CanSnapPointAccept(TargetFeature.LegacyIndex, SourceLog, SourceFeature.LegacyIndex);
}

bool FLogFeatureProvider::CommitLink(AActor* Target, const FPlacementFeature& TargetFeature,
    AActor* Source, const FPlacementFeature& SourceFeature) const
{
    ALogBuildPiece* TargetLog = Cast<ALogBuildPiece>(Target);
    ALogBuildPiece* SourceLog = Cast<ALogBuildPiece>(Source);
    // Attaches, reserves both points and adds the graph edge, or changes nothing.
    return TargetLog && SourceLog &&
        TargetLog->TryReserveSnapPoints(TargetFeature.LegacyIndex, SourceLog, SourceFeature.LegacyIndex);
}

void FLogFeatureProvider::RevertLink(AActor* Target, const FPlacementFeature& TargetFeature,
    AActor* Source, const FPlacementFeature& SourceFeature) const
{
    if (ALogBuildPiece* SourceLog = Cast<ALogBuildPiece>(Source))
    {
        if (!SourceLog->DetachFromSnapParent())
            SourceLog->DetachFromSnapParent(true);
    }
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

void FLogFeatureProvider::BuildModularFeatures(const ALogBuildPiece* Log, float ModuleCm, TArray<FPlacementFeature>& OutFeatures)
{
    using namespace MyProject3Construction;
    OutFeatures.Reset();
    FLogGeometry Shape;
    if (!Log || !GetLogGeometry(Log, Shape) || Shape.HalfLength <= KINDA_SMALL_NUMBER)
        return;
    const FTransform ActorTransform = Log->GetActorTransform();
    const FVector Axis = Shape.Axis.GetSafeNormal();
    const FVector Up = FMath::Abs(Axis.Z) > 0.9f ? FVector::XAxisVector : FVector::UpVector;

    auto MakeFeature = [&](FName Id, EPlacementFeatureKind Kind, const FVector& Location, const FVector& Out)
    {
        FPlacementFeature Feature;
        Feature.Id = Id;
        Feature.Kind = Kind;
        Feature.Tag = TEXT("LogEnd");
        Feature.WorldFrame = FTransform(FRotationMatrix::MakeFromXZ(Out, Up).ToQuat(), Location);
        Feature.LocalFrame = Feature.WorldFrame.GetRelativeTransform(ActorTransform);
        Feature.bCanSource = Kind == EPlacementFeatureKind::End;
        Feature.bCanReceive = true;
        OutFeatures.Add(Feature);
    };

    MakeFeature(TEXT("End.A"), EPlacementFeatureKind::End, Shape.EndA(), -Axis);
    MakeFeature(TEXT("End.B"), EPlacementFeatureKind::End, Shape.EndB(), Axis);

    TArray<float> Offsets;
    PlacementMath::ComputeStationOffsets(Shape.Length(), ModuleCm, Offsets);
    for (int32 Index = 0; Index < Offsets.Num(); ++Index)
    {
        // A station has no fixed direction: its frame points along the log and
        // the solver turns the joint about it.
        MakeFeature(FName(*FString::Printf(TEXT("Station.%02d"), Index)), EPlacementFeatureKind::Station,
            Shape.EndA() + Axis * Offsets[Index], Axis);
    }
}
