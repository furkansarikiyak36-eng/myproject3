#include "Placement/PlacementKernelSubsystem.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MaterialConnectionSubsystem.h"
#include "Placement/PlacementBodyMath.h"
#include "Placement/PlacementFeatureProvider.h"
#include "Placement/PlacementSolvers.h"
#include "Placement/PlacementTransaction.h"
#include "StructuralPieceInterface.h"
#include "WorldStructuralGraphSubsystem.h"

namespace
{
    const IPlacementFeatureProvider* ProviderOf(const AActor* Piece)
    {
        return FPlacementProviderRegistry::Find(Piece);
    }

    FName FamilyOf(const AActor* Piece)
    {
        const IPlacementFeatureProvider* Provider = ProviderOf(Piece);
        return Provider ? Provider->GetFamily(Piece) : NAME_None;
    }

    bool FamilyAllowed(const FPlacementFeatureFilter& Filter, const AActor* Piece)
    {
        return Filter.Family.IsNone() || Filter.Family == FamilyOf(Piece);
    }

    FGuid NodeOf(const AActor* Piece)
    {
        const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
        return Structural ? Structural->GetStructuralNodeId() : FGuid();
    }

    bool IsStructurallyPlaced(const AActor* Piece)
    {
        const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
        return Structural && Structural->IsStructurallyPlaced();
    }
}

FTransform UPlacementKernelSubsystem::ResolveFeatureFrame(const FPlacementFeature& Feature, const FVector& Toward, float StepDeg)
{
    if (!Feature.bRadialFree)
        return Feature.WorldFrame;
    // Resolve one logical station into a surface contact frame. The radial normal
    // is selected from the aim point; the body's longitudinal axis remains the
    // frame's tangent so cross-joint angles have a stable, geometry-aware meaning.
    const FVector Centre = Feature.WorldFrame.GetLocation();
    const FVector Axis = Feature.WorldFrame.GetUnitAxis(EAxis::X).GetSafeNormal();
    FVector Reference = FVector::VectorPlaneProject(FVector::UpVector, Axis);
    if (!Reference.Normalize())
        Reference = FVector::VectorPlaneProject(FVector::XAxisVector, Axis).GetSafeNormal();
    FVector Direction = FVector::VectorPlaneProject(Toward - Centre, Axis);
    if (!Direction.Normalize())
        Direction = Reference;
    float Angle = FMath::RadiansToDegrees(FMath::Atan2(
        FVector::DotProduct(Direction, FVector::CrossProduct(Axis, Reference)), FVector::DotProduct(Direction, Reference)));
    if (StepDeg > KINDA_SMALL_NUMBER)
        Angle = FMath::RoundToFloat(Angle / StepDeg) * StepDeg;
    const FVector Out = FQuat(Axis, FMath::DegreesToRadians(Angle)).RotateVector(Reference);
    return FTransform(FRotationMatrix::MakeFromXY(Out, Axis).ToQuat(), Centre + Out * Feature.RadiusCm);
}

// Resolves the moving station in the held actor's local space. Unlike resolving a
// target station alone, the source frame must be local to the moving log so the
// solved actor transform preserves its actual length and cross-section.
FTransform ResolveLocalFeatureFrame(
    const FPlacementFeature& Feature,
    const FTransform& ActorWorldTransform,
    const FVector& TowardWorld,
    float StepDeg)
{
    if (!Feature.bRadialFree)
        return Feature.LocalFrame;

    const FVector Centre = Feature.LocalFrame.GetLocation();
    const FVector Axis = Feature.LocalFrame.GetUnitAxis(EAxis::X).GetSafeNormal();
    const FVector TowardLocal = ActorWorldTransform.InverseTransformPosition(TowardWorld);
    FVector Reference = FVector::VectorPlaneProject(FVector::UpVector, Axis);
    if (!Reference.Normalize())
        Reference = FVector::VectorPlaneProject(FVector::XAxisVector, Axis);
    if (!Reference.Normalize())
        Reference = FVector::VectorPlaneProject(FVector::YAxisVector, Axis).GetSafeNormal();

    FVector Direction = FVector::VectorPlaneProject(TowardLocal - Centre, Axis);
    if (!Direction.Normalize())
        Direction = Reference;

    float Angle = FMath::RadiansToDegrees(FMath::Atan2(
        FVector::DotProduct(Direction, FVector::CrossProduct(Axis, Reference)),
        FVector::DotProduct(Direction, Reference)));
    if (StepDeg > KINDA_SMALL_NUMBER)
        Angle = FMath::RoundToFloat(Angle / StepDeg) * StepDeg;
    const FVector Out = FQuat(Axis, FMath::DegreesToRadians(Angle)).RotateVector(Reference);

    const float WorldPerLocalUnit = ActorWorldTransform.TransformVector(Out).Size();
    const float LocalRadius = WorldPerLocalUnit > KINDA_SMALL_NUMBER
        ? Feature.RadiusCm / WorldPerLocalUnit : Feature.RadiusCm;
    return FTransform(FRotationMatrix::MakeFromXY(Out, Axis).ToQuat(), Centre + Out * LocalRadius);
}

const TCHAR* UPlacementKernelSubsystem::DamagedReason()
{
    return TEXT("This piece is damaged: repair it before building on it (LMB with the axe).");
}

void UPlacementKernelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Profiles.LoadDefault();
}

float UPlacementKernelSubsystem::GetParam(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FName Name, float Default) const
{
    if (const float* Override = Intent.ParamOverrides.Find(Name))
        return *Override;
    return Profile.GetParam(Name, Default);
}

void UPlacementKernelSubsystem::Offer(FPlacementSolution&& Candidate, FPlacementSolution& InOutBest)
{
    if (!Candidate.bValid)
    {
        // Remember the most useful explanation, but never replace a valid answer.
        if (!InOutBest.bValid && !Candidate.Reason.IsEmpty())
            InOutBest.Reason = Candidate.Reason;
        return;
    }
    if (!InOutBest.bValid || Candidate.Score < InOutBest.Score)
    {
        Candidate.HeldRevision = InOutBest.HeldRevision;
        InOutBest = MoveTemp(Candidate);
    }
}

FPlacementLink UPlacementKernelSubsystem::MakeBodyLink(const AActor* Held, const AActor* Target, EPlacementLinkRole Role)
{
    FPlacementLink Link;
    Link.Role = Role;
    Link.Source.Piece = const_cast<AActor*>(Held);
    Link.Source.FeatureId = PlacementBodyFeatureId();
    Link.Target.Piece = const_cast<AActor*>(Target);
    Link.Target.FeatureId = PlacementBodyFeatureId();
    if (const IPlacementFeatureProvider* Provider = ProviderOf(Held))
        Link.Source.Revision = Provider->GetGeometryRevision(Held);
    if (const IPlacementFeatureProvider* Provider = ProviderOf(Target))
        Link.Target.Revision = Provider->GetGeometryRevision(Target);
    return Link;
}

FTransform UPlacementKernelSubsystem::MakeBodyTransform(const AActor* Piece, const FPlacementPose& Pose) const
{
    const IPlacementFeatureProvider* Provider = ProviderOf(Piece);
    return Provider ? Provider->MakeBodyTransform(Piece, Pose) : FTransform(Pose.Centre);
}

FPlacementSolution UPlacementKernelSubsystem::MakeDirectSolution(AActor* Piece, const FTransform& Transform, FName ProfileId,
    AActor* Primary, AActor* Secondary, AActor* Carried) const
{
    FPlacementSolution Solution;
    Solution.bValid = Piece && !Transform.ContainsNaN();
    Solution.ProfileId = ProfileId;
    Solution.PieceTransform = Transform;
    Solution.Score = 0.0f;
    if (const IPlacementFeatureProvider* Provider = ProviderOf(Piece))
        Solution.HeldRevision = Provider->GetGeometryRevision(Piece);
    // Only built-in structural pieces carry load; anything else (the ground) holds nothing in the graph.
    if (IsStructurallyPlaced(Primary))
        Solution.Links.Add(MakeBodyLink(Piece, Primary, EPlacementLinkRole::Primary));
    if (IsStructurallyPlaced(Secondary))
    {
        if (!Solution.FindLink(EPlacementLinkRole::Primary))
        {
            Solution.bValid = false;
            Solution.Reason = TEXT("The second support needs a first one.");
        }
        Solution.Links.Add(MakeBodyLink(Piece, Secondary, EPlacementLinkRole::Secondary));
    }
    if (IsStructurallyPlaced(Carried))
        Solution.Links.Add(MakeBodyLink(Piece, Carried, EPlacementLinkRole::Carries));
    return Solution;
}

// ---------------------------------------------------------------- queries

void UPlacementKernelSubsystem::GatherPlacedBodies(const FVector& Centre, float RadiusCm, TArray<AActor*>& OutPieces) const
{
    OutPieces.Reset();
    if (!GetWorld())
        return;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        FPlacementBody Body;
        if (!FPlacementProviderRegistry::IsPlaced(Actor) || !FPlacementProviderRegistry::GetBody(Actor, Body))
            continue;
        if (RadiusCm > 0.0f && FVector::Dist(Body.Centre, Centre) > RadiusCm + Body.HalfLength)
            continue;
        OutPieces.Add(Actor);
    }
}

void UPlacementKernelSubsystem::FindRestingOnTop(const AActor* Post, TArray<AActor*>& OutPieces) const
{
    OutPieces.Reset();
    FPlacementBody PostBody;
    if (!FPlacementProviderRegistry::GetBody(Post, PostBody))
        return;
    TArray<AActor*> Nearby;
    GatherPlacedBodies(PostBody.Top(), 1200.0f, Nearby);
    for (AActor* Piece : Nearby)
    {
        FPlacementBody Body;
        if (Piece == Post || !FPlacementProviderRegistry::GetBody(Piece, Body) || Body.IsUpright())
            continue;
        if (PlacementBodyMath::RestsOnTop(PostBody, Body))
            OutPieces.Add(Piece);
    }
}

int32 UPlacementKernelSubsystem::CountParallelRows(const AActor* TopPiece, int32 MaxRows) const
{
    FPlacementBody Top;
    if (!FPlacementProviderRegistry::GetBody(TopPiece, Top) || !Top.IsLying())
        return 0;
    // Distinct heights of parallel built pieces in the same wall line, at or below the top one.
    TArray<float> Heights;
    Heights.Add(Top.Centre.Z);
    const FVector Line = Top.Axis.GetSafeNormal2D();
    TArray<AActor*> Nearby;
    GatherPlacedBodies(Top.Centre, Top.HalfLength + 1200.0f, Nearby);
    for (AActor* Piece : Nearby)
    {
        FPlacementBody Shape;
        if (Piece == TopPiece || !FPlacementProviderRegistry::GetBody(Piece, Shape) || !Shape.IsLying())
            continue;
        if (FMath::Abs(FVector::DotProduct(Shape.Axis.GetSafeNormal2D(), Line)) < 0.95f ||
            Shape.Centre.Z > Top.Centre.Z + 1.0f || Top.Centre.Z - Shape.Centre.Z > 2.0f * Top.Radius * MaxRows + 50.0f)
            continue;
        FVector Offset = Shape.Centre - Top.Centre;
        Offset.Z = 0.0f;
        const float Across = (Offset - Line * FVector::DotProduct(Offset, Line)).Size();
        const float AlongGap = FMath::Abs(FVector::DotProduct(Offset, Line)) - Shape.HalfLength - Top.HalfLength;
        if (Across > Top.Radius || AlongGap > 0.0f)
            continue;
        if (!Heights.ContainsByPredicate([&Shape, &Top](float Z) { return FMath::Abs(Z - Shape.Centre.Z) < Top.Radius; }))
            Heights.Add(Shape.Centre.Z);
    }
    return Heights.Num();
}

bool UPlacementKernelSubsystem::IsDamaged(const AActor* Piece) const
{
    const UWorldStructuralGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    FStructuralNodeRecord Node;
    return Piece && Graph && Graph->GetNode(NodeOf(Piece), Node) &&
        Node.DamageThreshold > 0.0f && Node.Damage >= 0.5f * Node.DamageThreshold;
}

bool UPlacementKernelSubsystem::IsSpaceFree(const FVector& Centre, float Radius, const TArray<const AActor*>& Ignore,
    bool bPlacedOnly) const
{
    if (!GetWorld())
        return true;
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PlacementSpaceFree), false);
    for (const AActor* Actor : Ignore)
        if (Actor)
            Params.AddIgnoredActor(Actor);
    GetWorld()->OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape::MakeSphere(Radius), Params);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        const AActor* Other = Overlap.GetActor();
        if (Other && ProviderOf(Other) && (!bPlacedOnly || FPlacementProviderRegistry::IsPlaced(Other)))
            return false;
    }
    return true;
}

// ---------------------------------------------------------------- solve

FPlacementSolution UPlacementKernelSubsystem::Solve(const FPlacementIntent& Intent, const FPlacementSolution* Previous) const
{
    FPlacementSolution Best;
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    if (HeldProvider)
        Best.HeldRevision = HeldProvider->GetGeometryRevision(Intent.Held);

    for (const FPlacementProfile& Profile : Profiles.GetProfiles())
    {
        if (!Profile.bEnabled)
            continue;
        // Without a list only joints are searched; other operations are asked for by name.
        if (Intent.AllowedProfiles.Num() > 0 ? !Intent.AllowedProfiles.Contains(Profile.Id)
                                             : Profile.Operation != EPlacementOperation::Connect || Profile.Mode != TEXT("snap"))
            continue;
        // Only surface rules (post spacing) can be asked about with empty hands.
        if (HeldProvider ? !FamilyAllowed(Profile.Source, Intent.Held) : Profile.Solver != EPlacementSolver::RestOnSurface)
            continue;
        const FString Mode = Profile.Mode.ToString();
        switch (Profile.Solver)
        {
        case EPlacementSolver::AlignOne:
            if (Mode == TEXT("anchor"))
                SolveAnchor(Profile, Intent, Best);
            else
                SolveSnap(Profile, Intent, Previous, Best);
            break;
        case EPlacementSolver::AlignParallel:
            if (Mode == TEXT("stack")) SolveStack(Profile, Intent, Best);
            else if (Mode == TEXT("deck")) SolveDeck(Profile, Intent, Best);
            else if (Mode == TEXT("step")) SolveStep(Profile, Intent, Best);
            else if (Mode == TEXT("gable")) SolveGable(Profile, Intent, Best);
            break;
        case EPlacementSolver::SpanTwo:
            SolveStrut(Profile, Intent, Best);
            break;
        case EPlacementSolver::RestOnSurface:
            if (Mode == TEXT("post"))
                SolvePost(Profile, Intent, Best);
            break;
        }
    }
    if (!HeldProvider && !Best.bValid && Best.Reason.IsEmpty())
        Best.Reason = TEXT("Nothing placeable is held.");
    return Best;
}

void UPlacementKernelSubsystem::SolveSnap(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    const FPlacementSolution* Previous, FPlacementSolution& InOutBest) const
{
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    const FName HeldFamily = HeldProvider->GetFamily(Intent.Held);
    TArray<FPlacementFeature> Sources;
    HeldProvider->GetFeatures(Intent.Held, Sources);
    Sources.RemoveAll([&](const FPlacementFeature& Feature)
    {
        return !Feature.bCanSource || Feature.bOccupied || !Profile.Source.Matches(HeldFamily, Feature);
    });
    if (Sources.Num() == 0)
        return;

    TArray<AActor*> Candidates = Intent.Candidates;
    if (Candidates.Num() == 0 && Intent.AimActor)
        Candidates.Add(Intent.AimActor);

    const FPlacementLink* PreviousLink = Previous && Previous->bValid ? Previous->GetPrimaryLink() : nullptr;
    const FTransform HeldTransform = Intent.Held->GetActorTransform();

    for (AActor* Target : Candidates)
    {
        const IPlacementFeatureProvider* TargetProvider = ProviderOf(Target);
        if (!TargetProvider || Target == Intent.Held || !FamilyAllowed(Profile.Target, Target))
            continue;
        if (IsDamaged(Target))
        {
            FPlacementSolution Damaged;
            Damaged.Reason = DamagedReason();
            Offer(MoveTemp(Damaged), InOutBest);
            continue;
        }
        const FName TargetFamily = TargetProvider->GetFamily(Target);
        TArray<FPlacementFeature> Targets;
        TargetProvider->GetFeatures(Target, Targets);
        const uint32 TargetRevision = TargetProvider->GetGeometryRevision(Target);

        for (const FPlacementFeature& TargetFeature : Targets)
        {
            if (!TargetFeature.bCanReceive || TargetFeature.bOccupied || !Profile.Target.Matches(TargetFamily, TargetFeature))
                continue;
            for (const FPlacementFeature& SourceFeature : Sources)
            {
                if (!TargetProvider->CanLink(Target, TargetFeature, Intent.Held, SourceFeature))
                    continue;
                // A target station resolves its contact side from the aim; a moving
                // station resolves its own local contact face toward that same contact.
                const FTransform TargetFrame = ResolveFeatureFrame(TargetFeature, Intent.AimPoint,
                    GetParam(Profile, Intent, TEXT("radialStepDeg"), TargetFeature.RadialStepDeg));
                const FVector TargetPoint = TargetFrame.GetLocation();
                const FVector SourcePoint = (SourceFeature.LocalFrame * HeldTransform).GetLocation();
                const FTransform SourceLocalFrame = ResolveLocalFeatureFrame(
                    SourceFeature, HeldTransform, TargetPoint,
                    GetParam(Profile, Intent, TEXT("sourceRadialStepDeg"), SourceFeature.RadialStepDeg));
                // The held piece's free preview may be half a log away from the joint;
                // aiming straight at the target feature still finds it.
                const float Distance = FMath::Min(FVector::Distance(SourcePoint, TargetPoint),
                    FVector::Distance(Intent.AimPoint, TargetPoint));
                if (Distance > Profile.SearchRadiusCm)
                    continue;

                const bool bSameAsPrevious = PreviousLink && PreviousLink->Target.Piece.Get() == Target &&
                    PreviousLink->Target.FeatureId == TargetFeature.Id && PreviousLink->Source.FeatureId == SourceFeature.Id &&
                    Previous->ProfileId == Profile.Id;
                const float PreviousAngle = bSameAsPrevious ? Previous->AngleDeg : 0.0f;
                const float Angle = PlacementMath::ChooseAngle(Profile, Intent.HeldAngleDeg,
                    bSameAsPrevious ? &PreviousAngle : nullptr);

                FPlacementSolution Candidate;
                Candidate.bValid = true;
                Candidate.ProfileId = Profile.Id;
                Candidate.Score = Distance - Profile.Priority * 0.01f - (bSameAsPrevious ? StickyBonusCm : 0.0f);
                Candidate.PieceTransform = PlacementMath::AlignOne(SourceLocalFrame, HeldTransform.GetScale3D(),
                    TargetFrame, TargetFeature.bFlipIncoming, Angle);
                Candidate.AngleDeg = Angle;
                Candidate.MarkerPoint = TargetPoint;
                FPlacementLink Link;
                Link.Role = EPlacementLinkRole::Primary;
                Link.Source.Piece = Intent.Held;
                Link.Source.FeatureId = SourceFeature.Id;
                Link.Source.Revision = InOutBest.HeldRevision;
                Link.Target.Piece = Target;
                Link.Target.FeatureId = TargetFeature.Id;
                Link.Target.Revision = TargetRevision;
                Candidate.Links.Add(Link);
                Offer(MoveTemp(Candidate), InOutBest);
            }
        }
    }
}

void UPlacementKernelSubsystem::SolveAnchor(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FPlacementSolution& InOutBest) const
{
    // A piece set on a receiving point of a built piece (a stone on a stone, a
    // stick on a log): it stands upright on the point, turned to the carry
    // direction and leaned by the held angle.
    AActor* Target = Intent.AimActor;
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    const IPlacementFeatureProvider* TargetProvider = ProviderOf(Target);
    if (!TargetProvider || Target == Intent.Held || !FamilyAllowed(Profile.Target, Target))
        return;
    if (!TargetProvider->IsPlaced(Target))
    {
        FPlacementSolution Loose;
        Loose.Reason = TEXT("That piece is loose; place on the ground or a placed piece.");
        Offer(MoveTemp(Loose), InOutBest);
        return;
    }
    const FName HeldFamily = HeldProvider->GetFamily(Intent.Held);
    const FName TargetFamily = TargetProvider->GetFamily(Target);
    const bool bSameFamily = HeldFamily == TargetFamily;
    // The piece rests on its lowest joining point (a stone's or stick's bottom).
    TArray<FPlacementFeature> Sources;
    HeldProvider->GetFeatures(Intent.Held, Sources);
    const FPlacementFeature* Source = nullptr;
    for (const FPlacementFeature& Feature : Sources)
    {
        if (Feature.bCanSource && !Feature.bOccupied && Profile.Source.Matches(HeldFamily, Feature) &&
            (!Source || Feature.LocalFrame.GetLocation().Z < Source->LocalFrame.GetLocation().Z))
            Source = &Feature;
    }
    if (!Source)
        return;

    FVector Origin, Extent;
    Target->GetActorBounds(true, Origin, Extent);
    UMaterialConnectionSubsystem* Connections = GetWorld() ? GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr;
    TArray<FPlacementFeature> Targets;
    TargetProvider->GetFeatures(Target, Targets);
    const uint32 TargetRevision = TargetProvider->GetGeometryRevision(Target);
    const float Tilt = PlacementMath::ChooseAngle(Profile, Intent.HeldAngleDeg);
    FPlacementBody HeldBody;
    const float Lift = HeldProvider->GetBody(Intent.Held, HeldBody) ? HeldBody.HalfLength * FMath::Sin(FMath::DegreesToRadians(Tilt)) : 0.0f;
    const FVector Scale = Intent.Held->GetActorScale3D();
    bool bAnyPoint = false;

    for (const FPlacementFeature& TargetFeature : Targets)
    {
        if (!TargetFeature.bCanReceive || TargetFeature.bOccupied || !Profile.Target.Matches(TargetFamily, TargetFeature))
            continue;
        // A piece rests on the top side of a station.
        const FVector Point = ResolveFeatureFrame(TargetFeature, TargetFeature.WorldFrame.GetLocation() + FVector::UpVector * 1000.0f,
            GetParam(Profile, Intent, TEXT("radialStepDeg"), 90.0f)).GetLocation();
        if (Point.Z < Origin.Z - 1.0f)
            continue; // A point on the underside cannot hold a piece resting on it.
        bAnyPoint = true;
        FString Reason;
        if (bSameFamily ? !TargetProvider->CanLink(Target, TargetFeature, Intent.Held, *Source)
                        : (!Connections || !Connections->CanConnect(Target, TargetFeature.Id, Intent.Held, Source->Id, Reason)))
            continue;
        const float Distance = FVector::Dist(Point, Intent.AimPoint);
        if (Distance > Profile.SearchRadiusCm)
            continue;

        FQuat Rotation = FRotator(0.0f, Intent.CarryYawDeg, 0.0f).Quaternion();
        float RaiseCm = 0.0f;
        if (bSameFamily && GetParam(Profile, Intent, TEXT("keepTargetRotation"), 0.0f) > 0.5f)
            Rotation = Target->GetActorQuat(); // A stack of stones keeps the lower stone's turn.
        else if (Tilt > 0.0f)
        {
            Rotation = Rotation * FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Tilt));
            RaiseCm = Lift;
        }
        FPlacementSolution Candidate;
        Candidate.bValid = true;
        Candidate.ProfileId = Profile.Id;
        Candidate.Score = Distance;
        Candidate.AngleDeg = Tilt;
        Candidate.MarkerPoint = Point;
        Candidate.PieceTransform = FTransform(Rotation,
            Point + FVector::UpVector * RaiseCm - Rotation.RotateVector(Source->LocalFrame.GetLocation() * Scale), Scale);
        FPlacementLink Link;
        Link.Role = EPlacementLinkRole::Primary;
        Link.Source.Piece = Intent.Held;
        Link.Source.FeatureId = Source->Id;
        Link.Source.Revision = InOutBest.HeldRevision;
        Link.Target.Piece = Target;
        Link.Target.FeatureId = TargetFeature.Id;
        Link.Target.Revision = TargetRevision;
        Candidate.Links.Add(Link);
        Offer(MoveTemp(Candidate), InOutBest);
    }
    if (!bAnyPoint)
    {
        FPlacementSolution None;
        None.Reason = TEXT("No free top anchor here for this material.");
        Offer(MoveTemp(None), InOutBest);
    }
}

void UPlacementKernelSubsystem::SolveStack(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FPlacementSolution& InOutBest) const
{
    AActor* Target = Intent.AimActor;
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    FPlacementBody Lower, Upper;
    if (!Target || Target == Intent.Held || !FamilyAllowed(Profile.Target, Target) || !FPlacementProviderRegistry::IsPlaced(Target) ||
        !FPlacementProviderRegistry::GetBody(Target, Lower) || !Lower.IsLying() || !HeldProvider->GetBody(Intent.Held, Upper))
        return;
    FPlacementSolution Candidate;
    if (IsDamaged(Target))
    {
        Candidate.Reason = DamagedReason();
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    if (Intent.bUpright && !HeldProvider->CanStandUpright(Intent.Held))
    {
        Candidate.Reason = TEXT("Split pieces cannot stand as posts: use a round or hewn log.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    FPlacementPose Pose;
    if (!PlacementBodyMath::SolveStack(Lower, Upper, Intent.AimPoint, Intent.CarryYawDeg, Intent.bUpright,
        GetParam(Profile, Intent, TEXT("moduleCm"), 75.0f), GetParam(Profile, Intent, TEXT("interlock"), 0.82f),
        GetParam(Profile, Intent, TEXT("postSeat"), 0.6f), Pose))
        return;
    // A wall stands at most maxRows logs high (Sons of the Forest: six).
    const int32 MaxRows = FMath::RoundToInt(GetParam(Profile, Intent, TEXT("maxRows"), 6.0f));
    if (!Intent.bUpright && FMath::Abs(FVector::DotProduct(Pose.Axis.GetSafeNormal2D(), Lower.Axis.GetSafeNormal2D())) > 0.95f &&
        CountParallelRows(Target, MaxRows) >= MaxRows)
    {
        Candidate.Reason = FString::Printf(TEXT("A log wall stands at most %d logs high."), MaxRows);
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    // Do not stack into a piece that is already there.
    if (!IsSpaceFree(Pose.Centre, Upper.Radius * GetParam(Profile, Intent, TEXT("slotRadiusShare"), 0.5f),
        { Intent.Held, Target }, true))
        return;
    Candidate.bValid = true;
    Candidate.ProfileId = Profile.Id;
    Candidate.Score = FVector::Dist(Pose.Centre, Intent.AimPoint);
    Candidate.PieceTransform = HeldProvider->MakeBodyTransform(Intent.Held, Pose);
    Candidate.Links.Add(MakeBodyLink(Intent.Held, Target, EPlacementLinkRole::Primary));
    // It also rests on every other built body it lies across (the far wall of a corner).
    if (!Intent.bUpright)
    {
        const FVector UpperA = Pose.Centre - Pose.Axis * Upper.HalfLength;
        const FVector UpperB = Pose.Centre + Pose.Axis * Upper.HalfLength;
        TArray<AActor*> Nearby;
        GatherPlacedBodies(Pose.Centre, Upper.HalfLength + 200.0f, Nearby);
        for (AActor* Other : Nearby)
        {
            FPlacementBody Below;
            if (Other == Target || Other == Intent.Held || !IsStructurallyPlaced(Other) ||
                !FPlacementProviderRegistry::GetBody(Other, Below) || !Below.IsLying() || Below.Centre.Z >= Pose.Centre.Z - 1.0f)
                continue;
            FVector OnUpper, OnBelow;
            FMath::SegmentDistToSegmentSafe(UpperA, UpperB, Below.EndA(), Below.EndB(), OnUpper, OnBelow);
            if (FVector::Dist(OnUpper, OnBelow) <= Upper.Radius + Below.Radius + 2.0f)
                Candidate.Links.Add(MakeBodyLink(Intent.Held, Other, EPlacementLinkRole::Secondary));
        }
    }
    Offer(MoveTemp(Candidate), InOutBest);
}

void UPlacementKernelSubsystem::SolveDeck(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FPlacementSolution& InOutBest) const
{
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    AActor* Support = Intent.AimActor;
    FPlacementSolution Candidate;
    FPlacementBody Board;
    if (!HeldProvider->GetBody(Intent.Held, Board))
    {
        Candidate.Reason = TEXT("Carry a log or board.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    FPlacementBody SupportBody;
    if (!Support || !FPlacementProviderRegistry::IsPlaced(Support) || !IsStructurallyPlaced(Support) ||
        !FPlacementProviderRegistry::GetBody(Support, SupportBody))
    {
        Candidate.Reason = TEXT("Floor mode: aim at a placed beam, stringer or rope.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    FVector A0 = SupportBody.EndA(), A1 = SupportBody.EndB();
    FVector Axis1 = (A1 - A0).GetSafeNormal();
    if (Axis1.Z < 0.0f)
    {
        Axis1 = -Axis1;
        Swap(A0, A1);
    }
    if (Axis1.Z > 0.95f)
    {
        Candidate.Reason = TEXT("Boards rest on beams, not on standing posts.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    const FVector Horizontal1 = FVector(Axis1.X, Axis1.Y, 0.0f).GetSafeNormal();
    const FVector Perp = FVector::CrossProduct(FVector::UpVector, Horizontal1).GetSafeNormal();
    const FVector P = FMath::ClosestPointOnSegment(Intent.AimPoint, A0, A1);
    const float MaxSpan = Board.Length() - GetParam(Profile, Intent, TEXT("spanMarginCm"), 20.0f);
    const float Reach = GetParam(Profile, Intent, TEXT("lineReachCm"), 80.0f);
    const float MinGap = GetParam(Profile, Intent, TEXT("minGapCm"), 30.0f);

    // The second support: a parallel built body across the gap, preferring the side looked at.
    AActor* Best = nullptr;
    FPlacementBody BestBody;
    float BestScore = TNumericLimits<float>::Max();
    TArray<AActor*> Nearby;
    GatherPlacedBodies(P, 2.0f * Board.Length() + 600.0f, Nearby);
    for (AActor* Other : Nearby)
    {
        FPlacementBody Body;
        if (Other == Support || Other == Intent.Held || !IsStructurallyPlaced(Other) || !FPlacementProviderRegistry::GetBody(Other, Body))
            continue;
        if (FMath::Abs(FVector::DotProduct(Body.Axis, Axis1)) < 0.9f)
            continue; // Not parallel.
        const FVector Delta = FMath::ClosestPointOnSegment(P, Body.EndA(), Body.EndB()) - P;
        const float Offset = FVector::DotProduct(Delta, Perp);
        if ((Delta - Perp * Offset).Size() > Reach || FMath::Abs(Offset) < MinGap || FMath::Abs(Offset) > MaxSpan)
            continue;
        const float Score = FMath::Abs(Offset) - (FVector::DotProduct(Perp * FMath::Sign(Offset), Intent.ViewDirection) > 0.0f ? 10000.0f : 0.0f);
        if (Score < BestScore)
        {
            BestScore = Score;
            Best = Other;
            BestBody = Body;
        }
    }
    if (!Best)
    {
        Candidate.Reason = FString::Printf(TEXT("Needs a second parallel beam within %.0f cm (the board's length)."), MaxSpan);
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    const bool bStairs = GetParam(Profile, Intent, TEXT("stairs"), 0.0f) > 0.5f;
    const float Pitch = bStairs ? GetParam(Profile, Intent, TEXT("pitchCm"), 40.0f) : 2.0f * Board.Radius + 1.0f;
    FPlacementPose Pose;
    float Span = 0.0f;
    if (!PlacementBodyMath::SolveDeck(SupportBody.EndA(), SupportBody.EndB(), SupportBody.Radius, BestBody.EndA(), BestBody.EndB(),
        BestBody.Radius, Board, Intent.AimPoint, Pitch, bStairs, Pose, Span))
    {
        Candidate.Reason = TEXT("The supports are on top of each other.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    if (IsDamaged(Support) || IsDamaged(Best))
    {
        Candidate.Reason = DamagedReason();
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    if (!IsSpaceFree(Pose.Centre, Board.Radius * GetParam(Profile, Intent, TEXT("slotRadiusShare"), 0.7f),
        { Intent.Held, Support, Best }, false))
    {
        Candidate.Reason = TEXT("This spot already has a board.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    Candidate.bValid = true;
    Candidate.ProfileId = Profile.Id;
    Candidate.Score = FVector::Dist(Pose.Centre, Intent.AimPoint);
    Candidate.PieceTransform = HeldProvider->MakeBodyTransform(Intent.Held, Pose);
    Candidate.SpanCm = Span;
    Candidate.Links.Add(MakeBodyLink(Intent.Held, Support, EPlacementLinkRole::Primary));
    Candidate.Links.Add(MakeBodyLink(Intent.Held, Best, EPlacementLinkRole::Secondary));
    Offer(MoveTemp(Candidate), InOutBest);
}

void UPlacementKernelSubsystem::SolveStep(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FPlacementSolution& InOutBest) const
{
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    AActor* Edge = Intent.AimActor;
    FPlacementBody EdgeBody, Step;
    if (!Edge || !FamilyAllowed(Profile.Target, Edge) || !FPlacementProviderRegistry::IsPlaced(Edge) ||
        !FPlacementProviderRegistry::GetBody(Edge, EdgeBody) || !EdgeBody.IsLying() || !HeldProvider->GetBody(Intent.Held, Step))
        return;
    if (IsDamaged(Edge))
        return;
    FPlacementPose Pose;
    if (!PlacementBodyMath::SolveStep(EdgeBody, Step, Intent.AimPoint, Intent.PlacerLocation,
        GetParam(Profile, Intent, TEXT("gapCm"), 25.0f), GetParam(Profile, Intent, TEXT("dropCm"), 25.0f), Pose))
        return;
    FPlacementSolution Candidate;
    Candidate.bValid = true;
    Candidate.ProfileId = Profile.Id;
    Candidate.Score = 0.0f;
    Candidate.RequiredLengthCm = GetParam(Profile, Intent, TEXT("lengthCm"), 150.0f);
    Candidate.PieceTransform = HeldProvider->MakeBodyTransform(Intent.Held, Pose);
    Candidate.Links.Add(MakeBodyLink(Intent.Held, Edge, EPlacementLinkRole::Primary));
    Offer(MoveTemp(Candidate), InOutBest);
}

void UPlacementKernelSubsystem::SolveGable(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FPlacementSolution& InOutBest) const
{
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    AActor* SlopedBeam = Intent.AimActor;
    FPlacementSolution Candidate;
    FPlacementBody Beam, Held;
    if (!SlopedBeam || !FPlacementProviderRegistry::IsPlaced(SlopedBeam) || !FPlacementProviderRegistry::GetBody(SlopedBeam, Beam) ||
        !HeldProvider->GetBody(Intent.Held, Held))
    {
        Candidate.Reason = TEXT("Carry a log and aim at a sloped beam to fill the gable under it.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    const float Slope = FMath::Abs(Beam.Axis.Z);
    if (Slope < GetParam(Profile, Intent, TEXT("minSlope"), 0.15f) || Slope > GetParam(Profile, Intent, TEXT("maxSlope"), 0.9f))
    {
        Candidate.Reason = TEXT("A gable is filled under a sloped beam.");
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    TArray<AActor*> Nearby;
    GatherPlacedBodies(Beam.Centre, Beam.HalfLength + 300.0f, Nearby);
    const int32 MaxRows = FMath::RoundToInt(GetParam(Profile, Intent, TEXT("maxRows"), 24.0f));
    const float MinLength = GetParam(Profile, Intent, TEXT("minLengthCm"), 40.0f);
    for (int32 Row = 0; Row < MaxRows; ++Row)
    {
        FPlacementPose Pose;
        float Length = 0.0f;
        if (!PlacementBodyMath::SolveGableRow(Beam, Held, Row, MinLength, Pose, Length))
            break;
        const bool bTaken = Nearby.ContainsByPredicate([&](const AActor* Other)
        {
            FPlacementBody Body;
            return Other != SlopedBeam && Other != Intent.Held && FPlacementProviderRegistry::GetBody(Other, Body) &&
                FVector::Dist(FMath::ClosestPointOnSegment(Pose.Centre, Body.EndA(), Body.EndB()), Pose.Centre) < Held.Radius;
        });
        if (bTaken)
            continue;
        if (Held.Length() < Length * 0.85f)
        {
            Candidate.Reason = FString::Printf(TEXT("Too short: this gable row needs about %.0f cm."), Length);
            Offer(MoveTemp(Candidate), InOutBest);
            return;
        }
        Candidate.bValid = true;
        Candidate.ProfileId = Profile.Id;
        Candidate.Score = Row; // The row number, for the status line.
        Candidate.RequiredLengthCm = Length;
        Candidate.PieceTransform = HeldProvider->MakeBodyTransform(Intent.Held, Pose);
        Candidate.Links.Add(MakeBodyLink(Intent.Held, SlopedBeam, EPlacementLinkRole::Primary));
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    Candidate.Reason = TEXT("The gable under this beam is full.");
    Offer(MoveTemp(Candidate), InOutBest);
}

void UPlacementKernelSubsystem::SolveStrut(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FPlacementSolution& InOutBest) const
{
    const IPlacementFeatureProvider* HeldProvider = ProviderOf(Intent.Held);
    AActor* Aimed = Intent.AimActor;
    FPlacementBody AimedBody;
    if (!Aimed || Aimed == Intent.Held || !FPlacementProviderRegistry::IsPlaced(Aimed) || !FPlacementProviderRegistry::GetBody(Aimed, AimedBody))
        return;
    // The post and the beam resting on it: aimed at the post, or at the beam over a post.
    AActor* Post = nullptr;
    AActor* Beam = nullptr;
    if (AimedBody.IsUpright())
    {
        TArray<AActor*> Beams;
        FindRestingOnTop(Aimed, Beams);
        AActor* const* Wanted = Intent.Candidates.Num() > 0 ? Beams.FindByKey(Intent.Candidates[0]) : nullptr;
        if (Beams.Num() > 0)
        {
            Post = Aimed;
            Beam = Wanted ? *Wanted : Beams[0];
        }
    }
    else if (AimedBody.IsLying(0.5f))
    {
        float BestDistance = Profile.SearchRadiusCm;
        TArray<AActor*> Nearby;
        GatherPlacedBodies(Intent.AimPoint, Profile.SearchRadiusCm + 600.0f, Nearby);
        for (AActor* Candidate : Nearby)
        {
            FPlacementBody Body;
            if (Candidate == Aimed || !FPlacementProviderRegistry::GetBody(Candidate, Body) || !Body.IsUpright())
                continue;
            TArray<AActor*> Beams;
            FindRestingOnTop(Candidate, Beams);
            const float Distance = FVector::Dist2D(Candidate->GetActorLocation(), Intent.AimPoint);
            if (Beams.Contains(Aimed) && Distance < BestDistance)
            {
                BestDistance = Distance;
                Post = Candidate;
            }
        }
        Beam = Post ? Aimed : nullptr;
    }
    if (!Post || !Beam || !FamilyAllowed(Profile.Target, Post) || !FamilyAllowed(Profile.Target, Beam))
        return;
    FPlacementSolution Candidate;
    if (IsDamaged(Post) || IsDamaged(Beam))
    {
        Candidate.Reason = DamagedReason();
        Offer(MoveTemp(Candidate), InOutBest);
        return;
    }
    FPlacementBody PostBody, BeamBody, Strut;
    FPlacementProviderRegistry::GetBody(Post, PostBody);
    FPlacementProviderRegistry::GetBody(Beam, BeamBody);
    if (!HeldProvider->GetBody(Intent.Held, Strut))
        return;
    FPlacementPose Pose;
    float Length = 0.0f;
    FVector BeamPoint;
    if (!PlacementBodyMath::SolveStrut(PostBody, BeamBody, Strut, Intent.AimPoint, GetParam(Profile, Intent, TEXT("legCm"), 75.0f),
        GetParam(Profile, Intent, TEXT("moduleCm"), 75.0f), GetParam(Profile, Intent, TEXT("minLengthCm"), 40.0f), Pose, Length, BeamPoint))
        return;
    Candidate.bValid = true;
    Candidate.ProfileId = Profile.Id;
    Candidate.Score = 0.0f;
    Candidate.RequiredLengthCm = Length;
    Candidate.MarkerPoint = BeamPoint;
    Candidate.PieceTransform = HeldProvider->MakeBodyTransform(Intent.Held, Pose);
    // Both ends in one solution: it stands against the post and the beam rests on it.
    Candidate.Links.Add(MakeBodyLink(Intent.Held, Post, EPlacementLinkRole::Primary));
    Candidate.Links.Add(MakeBodyLink(Intent.Held, Beam, EPlacementLinkRole::Carries));
    Offer(MoveTemp(Candidate), InOutBest);
}

void UPlacementKernelSubsystem::SolvePost(const FPlacementProfile& Profile, const FPlacementIntent& Intent,
    FPlacementSolution& InOutBest) const
{
    // A post near one log length from another post moves to exactly that span.
    TArray<AActor*> Nearby;
    const float StepCm = GetParam(Profile, Intent, TEXT("spanStepCm"), 150.0f);
    const int32 Count = FMath::RoundToInt(GetParam(Profile, Intent, TEXT("spanCount"), 4.0f));
    GatherPlacedBodies(Intent.AimPoint, StepCm * Count + 100.0f, Nearby);
    TArray<AActor*> Posts;
    TArray<FVector> Feet;
    for (AActor* Piece : Nearby)
    {
        FPlacementBody Body;
        if (Piece != Intent.Held && FamilyAllowed(Profile.Source, Piece) && FPlacementProviderRegistry::GetBody(Piece, Body) && Body.IsUpright())
        {
            Posts.Add(Piece);
            Feet.Add(Body.Bottom());
        }
    }
    TArray<float> Spans;
    for (int32 Index = 1; Index <= Count; ++Index)
        Spans.Add(StepCm * Index);
    FVector Base;
    int32 Partner = INDEX_NONE;
    float Span = 0.0f;
    if (!PlacementBodyMath::SolvePostSpacing(Intent.AimPoint, Feet, Spans, GetParam(Profile, Intent, TEXT("maxErrorCm"), 30.0f),
        Base, Partner, Span))
        return;
    FPlacementSolution Candidate;
    Candidate.bValid = true;
    Candidate.ProfileId = Profile.Id;
    Candidate.Score = 0.0f;
    Candidate.MarkerPoint = Base;
    Candidate.SpanCm = Span;
    Candidate.PieceTransform = Intent.Held ? Intent.Held->GetActorTransform() : FTransform::Identity;
    // The partner is a reference, not a support: a post stands on the ground.
    FPlacementLink Reference = MakeBodyLink(Intent.Held, Posts[Partner], EPlacementLinkRole::Secondary);
    Reference.Target.FeatureId = TEXT("Reference");
    Candidate.Links.Add(Reference);
    Offer(MoveTemp(Candidate), InOutBest);
}

// ---------------------------------------------------------------- commit

bool UPlacementKernelSubsystem::IsStillCurrent(const FPlacementSolution& Solution, FString& OutReason) const
{
    if (!Solution.bValid)
    {
        OutReason = Solution.Reason;
        return false;
    }
    for (const FPlacementLink& Link : Solution.Links)
    {
        if (Link.Target.FeatureId == TEXT("Reference"))
            continue;
        AActor* Source = Link.Source.Piece.Get();
        AActor* Target = Link.Target.Piece.Get();
        const IPlacementFeatureProvider* SourceProvider = ProviderOf(Source);
        const IPlacementFeatureProvider* TargetProvider = ProviderOf(Target);
        if (!SourceProvider || !TargetProvider)
        {
            OutReason = TEXT("A piece of the joint is gone.");
            return false;
        }
        if (SourceProvider->GetGeometryRevision(Source) != Link.Source.Revision ||
            TargetProvider->GetGeometryRevision(Target) != Link.Target.Revision)
        {
            OutReason = TEXT("A piece of the joint changed shape.");
            return false;
        }
        if (Link.Target.IsBody())
        {
            if (!IsStructurallyPlaced(Target))
            {
                OutReason = TEXT("A support is no longer built in.");
                return false;
            }
            continue;
        }
        FPlacementFeature SourceFeature;
        FPlacementFeature TargetFeature;
        if (!FPlacementProviderRegistry::FindFeature(Source, Link.Source.FeatureId, SourceFeature) ||
            !FPlacementProviderRegistry::FindFeature(Target, Link.Target.FeatureId, TargetFeature))
        {
            OutReason = TEXT("The joint point is gone.");
            return false;
        }
        if (TargetFeature.bOccupied || SourceFeature.bOccupied)
        {
            OutReason = TEXT("The joint point is taken.");
            return false;
        }
        if (SourceProvider->GetFamily(Source) == TargetProvider->GetFamily(Target) &&
            !TargetProvider->CanLink(Target, TargetFeature, Source, SourceFeature))
        {
            OutReason = TEXT("The joint point is taken.");
            return false;
        }
    }
    return true;
}

bool UPlacementKernelSubsystem::CommitLinks(const FPlacementSolution& Solution, FPlacementTransaction& Tx, FString& OutReason) const
{
    if (!IsStillCurrent(Solution, OutReason))
    {
        Tx.Fail(TEXT("Stale solution"));
        return false;
    }
    UMaterialConnectionSubsystem* Connections = GetWorld() ? GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr;
    for (const FPlacementLink& Link : Solution.Links)
    {
        if (Link.Target.IsBody() || Link.Target.FeatureId == TEXT("Reference"))
            continue;
        AActor* Source = Link.Source.Piece.Get();
        AActor* Target = Link.Target.Piece.Get();
        const IPlacementFeatureProvider* TargetProvider = ProviderOf(Target);
        FPlacementFeature SourceFeature;
        FPlacementFeature TargetFeature;
        FPlacementProviderRegistry::FindFeature(Source, Link.Source.FeatureId, SourceFeature);
        FPlacementProviderRegistry::FindFeature(Target, Link.Target.FeatureId, TargetFeature);
        bool bMade = false;
        if (FamilyOf(Source) == FamilyOf(Target))
        {
            // Same material: the material makes its own joint.
            bMade = Tx.Step(TEXT("Joint"),
                [&] { return TargetProvider->CommitLink(Target, TargetFeature, Source, SourceFeature); },
                [TargetProvider, Target, TargetFeature, Source, SourceFeature]
                {
                    TargetProvider->RevertLink(Target, TargetFeature, Source, SourceFeature);
                });
        }
        else
        {
            // Different materials meet through the shared connection rules.
            const FTransform At = Solution.PieceTransform;
            bMade = Tx.Step(TEXT("Joint"),
                [&]
                {
                    FString Reason;
                    return Connections && Connections->TryConnectAt(Target, TargetFeature.Id, Source, SourceFeature.Id, At, Reason);
                },
                [Connections, Source] { if (Connections) Connections->TryDisconnect(Source); });
        }
        if (!bMade)
        {
            OutReason = TEXT("The joint could not be made.");
            return false;
        }
    }
    return true;
}

bool UPlacementKernelSubsystem::CommitPlacement(AActor* Piece, const FPlacementSolution& Solution,
    const FPlacementCommitOptions& Options, FString& OutReason)
{
    const IPlacementFeatureProvider* Provider = ProviderOf(Piece);
    if (!Provider || !GetWorld() || Solution.PieceTransform.ContainsNaN())
    {
        OutReason = TEXT("Nothing to place.");
        return false;
    }
    if (!IsStillCurrent(Solution, OutReason))
        return false;
    if (const FPlacementProfile* Profile = Profiles.Find(Solution.ProfileId))
    {
        int32 Supports = 0;
        for (const FPlacementLink& Link : Solution.Links)
            Supports += Link.Target.FeatureId == TEXT("Reference") ? 0 : 1;
        if (Supports < Profile->RequiredLinks)
        {
            OutReason = TEXT("The joint is not complete.");
            return false;
        }
    }
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    const FGuid NodeId = NodeOf(Piece);

    FPlacementTransaction Tx;
    const FTransform Previous = Piece->GetActorTransform();
    const FTransform Wanted = Solution.PieceTransform;
    Tx.Step(TEXT("Transform"),
        [&] { Piece->SetActorTransform(Wanted, false, nullptr, ETeleportType::TeleportPhysics); return true; },
        [Piece, Previous] { Piece->SetActorTransform(Previous, false, nullptr, ETeleportType::TeleportPhysics); });

    // Joints at features (snap points, anchors): the material makes them and their graph edges.
    if (!CommitLinks(Solution, Tx, OutReason))
        return false;

    // Whole-body links: only the structural graph.
    AActor* PrimaryTarget = nullptr;
    int32 RestIndex = 0;
    for (const FPlacementLink& Link : Solution.Links)
    {
        AActor* Target = Link.Target.Piece.Get();
        if (Link.Role == EPlacementLinkRole::Primary)
            PrimaryTarget = Target;
        if (!Link.Target.IsBody() || !Graph)
            continue;
        const FGuid TargetId = NodeOf(Target);
        const FName Joint(*FString::Printf(TEXT("Joint_%s"), *NodeId.ToString()));
        switch (Link.Role)
        {
        case EPlacementLinkRole::Primary:
            Tx.Step(TEXT("Support"),
                [&] { return Graph->TryConnect(TargetId, Joint, NodeId, TEXT("Base")); },
                [Graph, NodeId] { Graph->TryDisconnect(NodeId); });
            break;
        case EPlacementLinkRole::Secondary:
        {
            const FName Own(*FString::Printf(TEXT("Rest%d"), ++RestIndex));
            Tx.Step(TEXT("Second support"),
                [&] { return Graph->TryAddSecondarySupport(NodeId, TargetId, Joint, Own); },
                [Graph, NodeId, TargetId] { Graph->TryRemoveSecondarySupport(NodeId, TargetId); });
            break;
        }
        case EPlacementLinkRole::Carries:
        {
            const FName Carry(*FString::Printf(TEXT("Carry_%s"), *TargetId.ToString()));
            const FName Own(*FString::Printf(TEXT("RestsOn_%s"), *NodeId.ToString()));
            Tx.Step(TEXT("Carried piece"),
                [&] { return Graph->TryAddSecondarySupport(TargetId, NodeId, Carry, Own); },
                [Graph, TargetId, NodeId] { Graph->TryRemoveSecondarySupport(TargetId, NodeId); });
            break;
        }
        }
    }

    if (UWorldObjectIdentityComponent* Identity = Piece->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        // Pieces whose material does not track lifecycle stay as they are.
        if (Identity->GetLifecycleState() == Options.FromLifecycle)
        {
            const EWorldObjectLifecycleState From = Options.FromLifecycle;
            Tx.Step(TEXT("Lifecycle"),
                [&] { return Identity->TryTransitionLifecycle(From, EWorldObjectLifecycleState::Placed); },
                [Identity, From] { Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Placed, From); });
        }
    }
    // Ownership goes last: it is the one step without an undo.
    if (Options.FromOwner != EWorldItemOwnerKind::World)
        Tx.Step(TEXT("Ownership"), [&] { return Provider->TransferToWorld(Piece, Options.FromOwner); });

    if (Tx.Failed())
    {
        if (OutReason.IsEmpty())
            OutReason = FString::Printf(TEXT("Cannot place here (%s)."), *Tx.GetFailedStep().ToLower());
        return false;
    }
    Tx.Commit();
    Provider->ApplyPlacedState(Piece, PrimaryTarget);
    AddContactSupports(Piece);
    OnPlacementCommitted.Broadcast(Piece, Solution);
    return true;
}

FPlacementSolution UPlacementKernelSubsystem::MakeJointSolution(AActor* Piece, const FTransform& Transform, FName ProfileId,
    AActor* Target, FName TargetFeatureId, FName OwnFeatureId) const
{
    FPlacementSolution Solution = MakeDirectSolution(Piece, Transform, ProfileId, nullptr);
    const IPlacementFeatureProvider* PieceProvider = ProviderOf(Piece);
    const IPlacementFeatureProvider* TargetProvider = ProviderOf(Target);
    if (!PieceProvider || !TargetProvider)
    {
        Solution.bValid = false;
        Solution.Reason = TEXT("Nothing to join.");
        return Solution;
    }
    FPlacementLink Link;
    Link.Role = EPlacementLinkRole::Primary;
    Link.Source.Piece = Piece;
    Link.Source.FeatureId = OwnFeatureId;
    Link.Source.Revision = PieceProvider->GetGeometryRevision(Piece);
    Link.Target.Piece = Target;
    Link.Target.FeatureId = TargetFeatureId;
    Link.Target.Revision = TargetProvider->GetGeometryRevision(Target);
    Solution.Links.Add(Link);
    return Solution;
}

bool UPlacementKernelSubsystem::CommitReseat(AActor* Piece, const FTransform& NewTransform, AActor* NewSupport, FString& OutReason)
{
    if (!ProviderOf(Piece) || NewTransform.ContainsNaN() || !GetWorld())
    {
        OutReason = TEXT("Nothing to move.");
        return false;
    }
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    const FGuid NodeId = NodeOf(Piece);
    FPlacementTransaction Tx;
    const FTransform Previous = Piece->GetActorTransform();
    Tx.Step(TEXT("Transform"),
        [&] { Piece->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics); return true; },
        [Piece, Previous] { Piece->SetActorTransform(Previous, false, nullptr, ETeleportType::TeleportPhysics); });
    FStructuralNodeRecord Node;
    if (Graph && NewSupport && NewSupport != Piece && IsStructurallyPlaced(NewSupport) && Graph->GetNode(NodeId, Node))
    {
        const FGuid SupportId = NodeOf(NewSupport);
        const bool bAlready = Node.ParentNodeId == SupportId ||
            Node.SecondarySupports.ContainsByPredicate([&](const FStructuralSupportLink& Link) { return Link.SupportNodeId == SupportId; });
        if (!bAlready && Node.ParentNodeId.IsValid())
        {
            const FName Joint(*FString::Printf(TEXT("Rest_%s"), *NodeId.ToString()));
            const FName Own(*FString::Printf(TEXT("RestOn_%s"), *SupportId.ToString()));
            Tx.Step(TEXT("New support"),
                [&] { return Graph->TryAddSecondarySupport(NodeId, SupportId, Joint, Own); },
                [Graph, NodeId, SupportId] { Graph->TryRemoveSecondarySupport(NodeId, SupportId); });
        }
        else if (!bAlready)
        {
            const FName Joint(*FString::Printf(TEXT("Joint_%s"), *NodeId.ToString()));
            Tx.Step(TEXT("New support"),
                [&] { return Graph->TryConnect(SupportId, Joint, NodeId, TEXT("Base")); },
                [Graph, NodeId] { Graph->TryDisconnect(NodeId); });
        }
    }
    if (Tx.Failed())
    {
        OutReason = FString::Printf(TEXT("Cannot rest it there (%s)."), *Tx.GetFailedStep().ToLower());
        return false;
    }
    Tx.Commit();
    return true;
}

int32 UPlacementKernelSubsystem::AddContactSupports(AActor* Piece)
{
    UWorldStructuralGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    FPlacementBody Body;
    FStructuralNodeRecord Node;
    const FGuid NodeId = NodeOf(Piece);
    if (!Graph || !FPlacementProviderRegistry::GetBody(Piece, Body) || !Graph->GetNode(NodeId, Node))
        return 0;
    int32 Added = 0;
    TSet<const AActor*> Seen;
    TArray<AActor*> Nearby;
    GatherPlacedBodies(Body.Centre, Body.HalfLength + 300.0f, Nearby);
    for (const FVector& End : { Body.EndA(), Body.EndB() })
    {
        for (AActor* Other : Nearby)
        {
            FPlacementBody OtherBody;
            if (Other == Piece || Seen.Contains(Other) || !IsStructurallyPlaced(Other) || !FPlacementProviderRegistry::GetBody(Other, OtherBody))
                continue;
            const FGuid OtherId = NodeOf(Other);
            if (OtherId == Node.ParentNodeId)
                continue;
            const FVector Closest = FMath::ClosestPointOnSegment(End, OtherBody.EndA(), OtherBody.EndB());
            if (FVector::Dist(Closest, End) > Body.Radius + OtherBody.Radius + 12.0f)
                continue;
            Seen.Add(Other);
            if (OtherBody.Centre.Z > End.Z + 5.0f)
            {
                // This end props up the piece above it (a brace or strut).
                Added += Graph->TryAddSecondarySupport(OtherId, NodeId,
                    FName(*FString::Printf(TEXT("Contact_%s"), *OtherId.ToString())),
                    FName(*FString::Printf(TEXT("BraceOn_%s"), *NodeId.ToString()))) ? 1 : 0;
            }
            else if (Node.ParentNodeId.IsValid())
            {
                // This end rests on the piece under it.
                Added += Graph->TryAddSecondarySupport(NodeId, OtherId,
                    FName(*FString::Printf(TEXT("Contact_%s"), *NodeId.ToString())),
                    FName(*FString::Printf(TEXT("EndOn_%s"), *OtherId.ToString()))) ? 1 : 0;
            }
        }
    }
    return Added;
}

bool UPlacementKernelSubsystem::GetFeatureFrame(const AActor* Piece, FName FeatureId, const FVector& Toward,
    FTransform& OutFrame, float StepDeg) const
{
    FPlacementFeature Feature;
    if (!FPlacementProviderRegistry::FindFeature(Piece, FeatureId, Feature))
        return false;
    OutFrame = ResolveFeatureFrame(Feature, Toward, StepDeg);
    return true;
}

bool UPlacementKernelSubsystem::CommitGroundPlacement(AActor* Piece, const FTransform& Transform,
    const FPlacementCommitOptions& Options, FString& OutReason)
{
    const IPlacementFeatureProvider* Provider = ProviderOf(Piece);
    if (!Provider || Transform.ContainsNaN())
    {
        OutReason = TEXT("Nothing to place.");
        return false;
    }
    if (!Provider->HasOwnGroundPlacement())
        return CommitPlacement(Piece, MakeDirectSolution(Piece, Transform, TEXT("piece.free_place"), nullptr), Options, OutReason);
    if (!Provider->CommitGroundPlacement(Piece, Transform))
    {
        OutReason = TEXT("Cannot place on the ground here.");
        return false;
    }
    if (Options.FromOwner != EWorldItemOwnerKind::World && !Provider->TransferToWorld(Piece, Options.FromOwner))
        UE_LOG(LogTemp, Warning, TEXT("Placement: %s is on the ground but its ledger owner did not change."), *GetNameSafe(Piece));
    FPlacementSolution Solution = MakeDirectSolution(Piece, Transform, TEXT("piece.free_place"), nullptr);
    OnPlacementCommitted.Broadcast(Piece, Solution);
    return true;
}
