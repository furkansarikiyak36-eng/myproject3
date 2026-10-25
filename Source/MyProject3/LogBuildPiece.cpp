#include "LogBuildPiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Math/RotationMatrix.h"

ALogBuildPiece::ALogBuildPiece()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LogMesh"));
    SetRootComponent(Mesh);

    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(false);
}

void ALogBuildPiece::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (bAutoGenerateEndSnapPoints && SnapPoints.IsEmpty())
    {
        GenerateDefaultEndSnapPoints();
    }
}

void ALogBuildPiece::BeginPlay()
{
    Super::BeginPlay();

    bIsPlaced = bStartsPlaced;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsPlaced);
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

FVector ALogBuildPiece::GetLongitudinalAxisVector() const
{
    switch (LongitudinalAxis)
    {
    case ELogLongitudinalAxis::Y:
        return FVector::YAxisVector;
    case ELogLongitudinalAxis::Z:
        return FVector::UpVector;
    case ELogLongitudinalAxis::X:
    default:
        return FVector::XAxisVector;
    }
}

FRotator ALogBuildPiece::MakeOutwardRotation(const FVector& Direction) const
{
    return FRotationMatrix::MakeFromX(Direction.GetSafeNormal()).Rotator();
}

void ALogBuildPiece::GenerateDefaultEndSnapPoints()
{
    if (!Mesh || !Mesh->GetStaticMesh())
    {
        return;
    }

    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const FVector Axis = GetLongitudinalAxisVector();
    const float HalfLength = FVector::DotProduct(Bounds.BoxExtent, Axis.GetAbs());

    if (HalfLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    SnapPoints.Reset();

    const FVector PositiveLocation = Bounds.Origin + Axis * HalfLength;
    const FVector NegativeLocation = Bounds.Origin - Axis * HalfLength;

    FLogSnapPoint PositiveEnd;
    PositiveEnd.PointId = FName(TEXT("AutoEnd_Positive"));
    PositiveEnd.SnapTag = DefaultEndSnapTag;
    PositiveEnd.LocalTransform = FTransform(MakeOutwardRotation(Axis), PositiveLocation);
    PositiveEnd.bCanSource = true;
    PositiveEnd.bCanReceive = true;
    PositiveEnd.bAcceptAnyPieceType = true;
    PositiveEnd.bFlipIncomingOrientation = true;
    SnapPoints.Add(PositiveEnd);

    FLogSnapPoint NegativeEnd;
    NegativeEnd.PointId = FName(TEXT("AutoEnd_Negative"));
    NegativeEnd.SnapTag = DefaultEndSnapTag;
    NegativeEnd.LocalTransform = FTransform(MakeOutwardRotation(-Axis), NegativeLocation);
    NegativeEnd.bCanSource = true;
    NegativeEnd.bCanReceive = true;
    NegativeEnd.bAcceptAnyPieceType = true;
    NegativeEnd.bFlipIncomingOrientation = true;
    SnapPoints.Add(NegativeEnd);
}

FTransform ALogBuildPiece::GetSnapPointLocalTransform(int32 Index) const
{
    return SnapPoints.IsValidIndex(Index)
        ? SnapPoints[Index].LocalTransform
        : FTransform::Identity;
}

FTransform ALogBuildPiece::GetSnapPointWorldTransform(int32 Index) const
{
    if (!SnapPoints.IsValidIndex(Index))
    {
        return GetActorTransform();
    }

    // Local transform followed by actor world transform.
    return SnapPoints[Index].LocalTransform * GetActorTransform();
}

void ALogBuildPiece::SetPieceStaticMesh(UStaticMesh* NewMesh)
{
    if (!Mesh)
    {
        return;
    }

    Mesh->SetStaticMesh(NewMesh);
    if (bAutoGenerateEndSnapPoints)
    {
        SnapPoints.Reset();
        GenerateDefaultEndSnapPoints();
    }
}

void ALogBuildPiece::SetPieceTypeByName(FName TypeName)
{
    FString Key = TypeName.ToString().ToLower();
    Key.ReplaceInline(TEXT("_"), TEXT(""));
    Key.ReplaceInline(TEXT("-"), TEXT(""));
    Key.ReplaceInline(TEXT(" "), TEXT(""));

    if (Key == TEXT("whole") || Key == TEXT("wholelog") || Key == TEXT("rawlog"))
    {
        PieceType = ELogPieceType::WholeLog;
    }
    else if (Key.Contains(TEXT("threequarter")) || Key.Contains(TEXT("3quarter")))
    {
        PieceType = ELogPieceType::ThreeQuarterLog;
    }
    else if (Key.Contains(TEXT("halflength")) || Key == TEXT("halflog") || Key == TEXT("half"))
    {
        PieceType = ELogPieceType::HalfLengthLog;
    }
    else if (Key.Contains(TEXT("quarter")))
    {
        PieceType = ELogPieceType::QuarterLog;
    }
    else if (Key.Contains(TEXT("split")))
    {
        PieceType = ELogPieceType::SplitHalfLog;
    }
    else if (Key.Contains(TEXT("plank")) || Key.Contains(TEXT("kalas")))
    {
        PieceType = ELogPieceType::Plank;
    }
    else if (Key.Contains(TEXT("hewn")))
    {
        PieceType = ELogPieceType::HewnLog;
    }
    else if (Key.Contains(TEXT("gutter")) || Key.Contains(TEXT("trough")) || Key.Contains(TEXT("groove")))
    {
        PieceType = ELogPieceType::GutterLog;
    }
    else if (Key.Contains(TEXT("notch")) || Key.Contains(TEXT("crosslap")) ||
        Key.Contains(TEXT("saddle")) || Key.Contains(TEXT("fork")))
    {
        PieceType = ELogPieceType::NotchedLog;
    }
    else if (Key.Contains(TEXT("joint")))
    {
        PieceType = ELogPieceType::JointPiece;
    }
    else
    {
        PieceType = ELogPieceType::Other;
    }
}

void ALogBuildPiece::AddCustomSnapPoint(
    FName PointId,
    FName SnapTag,
    FTransform LocalTransform,
    bool bCanSource,
    bool bCanReceive,
    bool bAcceptAnyPieceType,
    bool bFlipIncomingOrientation)
{
    FLogSnapPoint Point;
    Point.PointId = PointId;
    Point.SnapTag = SnapTag;
    Point.LocalTransform = LocalTransform;
    Point.bCanSource = bCanSource;
    Point.bCanReceive = bCanReceive;
    Point.bAcceptAnyPieceType = bAcceptAnyPieceType;
    Point.bFlipIncomingOrientation = bFlipIncomingOrientation;
    Point.bOccupied = false;
    SnapPoints.Add(Point);
}

bool ALogBuildPiece::CanBePickedUp() const
{
    return bCanBeCarried && !bIsHeld && GetAttachedPieceCount() == 0;
}

void ALogBuildPiece::SetPlacedState(bool bNewPlaced)
{
    bIsPlaced = bNewPlaced;
    bStartsPlaced = bNewPlaced;

    if (!Mesh)
    {
        return;
    }

    Mesh->SetSimulatePhysics(!bNewPlaced && !bIsHeld);
    Mesh->SetCollisionProfileName(
        bNewPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
    Mesh->SetCollisionEnabled(bIsHeld
        ? ECollisionEnabled::NoCollision
        : ECollisionEnabled::QueryAndPhysics);
}

void ALogBuildPiece::SetHeldState(bool bNewHeld)
{
    bIsHeld = bNewHeld;

    if (!Mesh)
    {
        return;
    }

    if (bNewHeld)
    {
        Mesh->SetSimulatePhysics(false);
        SetActorEnableCollision(false);
    }
    else
    {
        SetActorEnableCollision(true);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

bool ALogBuildPiece::IsSnapPointFreeAndSource(int32 Index) const
{
    return SnapPoints.IsValidIndex(Index) &&
        SnapPoints[Index].bCanSource &&
        !SnapPoints[Index].bOccupied;
}

bool ALogBuildPiece::IsSnapPointFreeAndReceiver(int32 Index) const
{
    return SnapPoints.IsValidIndex(Index) &&
        SnapPoints[Index].bCanReceive &&
        !SnapPoints[Index].bOccupied;
}

bool ALogBuildPiece::CanSnapPointAccept(
    int32 TargetPointIndex,
    const ALogBuildPiece* SourcePiece,
    int32 SourcePointIndex) const
{
    if (!SourcePiece || SourcePiece == this ||
        !SnapPoints.IsValidIndex(TargetPointIndex) ||
        !SourcePiece->SnapPoints.IsValidIndex(SourcePointIndex))
    {
        return false;
    }

    const FLogSnapPoint& TargetPoint = SnapPoints[TargetPointIndex];
    const FLogSnapPoint& SourcePoint = SourcePiece->SnapPoints[SourcePointIndex];

    if (!TargetPoint.AcceptsPieceType(SourcePiece->PieceType) ||
        !SourcePoint.bCanSource ||
        SourcePoint.bOccupied)
    {
        return false;
    }

    if (!TargetPoint.AcceptsSnapTag(SourcePoint.SnapTag))
    {
        return false;
    }

    // Only an already placed piece can receive additional structure.
    return bIsPlaced && !bIsHeld;
}

bool ALogBuildPiece::TryReserveSnapPoints(
    int32 TargetPointIndex,
    ALogBuildPiece* SourcePiece,
    int32 SourcePointIndex)
{
    if (!CanSnapPointAccept(TargetPointIndex, SourcePiece, SourcePointIndex))
    {
        return false;
    }

    SnapPoints[TargetPointIndex].bOccupied = true;
    SourcePiece->SnapPoints[SourcePointIndex].bOccupied = true;
    SourcePiece->AttachToSnapParent(this, TargetPointIndex, SourcePointIndex);
    return true;
}

void ALogBuildPiece::ReleaseSnapPoint(int32 Index)
{
    if (SnapPoints.IsValidIndex(Index))
    {
        SnapPoints[Index].bOccupied = false;
    }
}

void ALogBuildPiece::AttachToSnapParent(
    ALogBuildPiece* Parent,
    int32 ParentPointIndex,
    int32 MyPointIndex)
{
    SnapParent = Parent;
    ParentSnapPointIndex = ParentPointIndex;
    OwnSnapPointIndexUsed = MyPointIndex;

    if (Parent)
    {
        Parent->RegisterAttachedPiece(this);
        AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
    }

    SetPlacedState(true);
}

void ALogBuildPiece::DetachFromSnapParent()
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    if (ALogBuildPiece* Parent = SnapParent.Get())
    {
        Parent->ReleaseSnapPoint(ParentSnapPointIndex);
        Parent->UnregisterAttachedPiece(this);
    }

    if (SnapPoints.IsValidIndex(OwnSnapPointIndexUsed))
    {
        SnapPoints[OwnSnapPointIndexUsed].bOccupied = false;
    }

    SnapParent.Reset();
    ParentSnapPointIndex = INDEX_NONE;
    OwnSnapPointIndexUsed = INDEX_NONE;
}

void ALogBuildPiece::RegisterAttachedPiece(ALogBuildPiece* Child)
{
    if (!Child)
    {
        return;
    }

    AttachedPieces.RemoveAll([](const TWeakObjectPtr<ALogBuildPiece>& Item)
    {
        return !Item.IsValid();
    });

    AttachedPieces.AddUnique(Child);
}

void ALogBuildPiece::UnregisterAttachedPiece(ALogBuildPiece* Child)
{
    AttachedPieces.RemoveAll([Child](const TWeakObjectPtr<ALogBuildPiece>& Item)
    {
        return !Item.IsValid() || Item.Get() == Child;
    });
}

int32 ALogBuildPiece::GetAttachedPieceCount() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<ALogBuildPiece>& Item : AttachedPieces)
    {
        if (Item.IsValid())
        {
            ++Count;
        }
    }
    return Count;
}
