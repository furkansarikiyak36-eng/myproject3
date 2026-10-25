#include "LogBuildPiece.h"

#include "GameplayCoreTypes.h"
#include "WorldCommittedEventSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

ALogBuildPiece::ALogBuildPiece()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LogMesh"));
    SetRootComponent(Mesh);
    StructuralIdentity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));

    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(false);
}

void ALogBuildPiece::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Preserve hand-authored anchors (for example, test-map top sockets) but
    // rebuild only auto-generated sockets when mesh/spacing/axis properties change.
    RegenerateAutomaticSnapPoints();
}

void ALogBuildPiece::BeginPlay()
{
    Super::BeginPlay();

    bIsPlaced = bStartsPlaced;
    if (StructuralIdentity)
    {
        StructuralIdentity->EnsureIdentity();

        // The physical actor and the ownership ledger share one stable item ID.
        // A restored non-world owner must not leave a duplicate visible actor.
        if (UWorldItemOwnershipSubsystem* Ownership =
            GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>())
        {
            FWorldOwnedItemRecord Existing;
            if (Ownership->TryGetItem(StructuralIdentity->GetInstanceId(), Existing))
            {
                if (Existing.OwnerKind != EWorldItemOwnerKind::World)
                {
                    SetActorHiddenInGame(true);
                    SetActorEnableCollision(false);
                    SetActorTickEnabled(false);
                    return;
                }
            }
            else
            {
                FWorldOwnedItemRecord Record;
                Record.ItemId = StructuralIdentity->GetInstanceId();
                Record.DefinitionId = StructuralIdentity->GetDefinitionId().IsNone()
                    ? FName(TEXT("Wood.LogPiece")) : StructuralIdentity->GetDefinitionId();
                Record.Quantity = 1;
                Record.OwnerKind = EWorldItemOwnerKind::World;
                Record.WorldLocation = GetActorLocation();
                if (!Ownership->RegisterItem(Record))
                {
                    UE_LOG(LogTemp, Error, TEXT("Log piece '%s' could not register item ownership."), *GetName());
                    SetActorHiddenInGame(true);
                    SetActorEnableCollision(false);
                    return;
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Log piece '%s' has no world-item ownership subsystem."), *GetName());
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            return;
        }

        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            if (!Graph->RegisterNode(this, StructuralIdentity->GetInstanceId(), EStructuralNodeKind::Wood,
                bIsPlaced, 1.0f, 100000.0f, 100.0f))
            {
                UE_LOG(LogTemp, Error, TEXT("Log piece '%s' could not register structural graph node."), *GetName());
            }
        }
    }
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsPlaced);
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

bool ALogBuildPiece::TryTransferOwnership(
    EWorldItemOwnerKind ExpectedOwner, EWorldItemOwnerKind NewOwner,
    const FGuid& NewOwnerId, FWorldOwnedItemRecord& OutRecord)
{
    if (!StructuralIdentity || !StructuralIdentity->HasValidIdentity() || !GetWorld())
    {
        return false;
    }

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    if (!Ownership)
    {
        return false;
    }

    FWorldOwnedItemRecord Current;
    if (!Ownership->TryGetItem(StructuralIdentity->GetInstanceId(), Current) ||
        Current.OwnerKind != ExpectedOwner)
    {
        return false;
    }

    return Ownership->TryTransferItem(FGuid::NewGuid(), Current.ItemId, Current.Revision,
        ExpectedOwner, NewOwner, NewOwnerId, GetActorLocation(), OutRecord);
}

void ALogBuildPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DetachFromSnapParent();
    for (const TWeakObjectPtr<ALogBuildPiece>& Child : AttachedPieces)
    {
        if (Child.IsValid())
        {
            Child->DetachFromSnapParent();
        }
    }
    AttachedPieces.Reset();
    if (StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            Graph->UnregisterNode(StructuralIdentity->GetInstanceId(), false);
        }
    }
    Super::EndPlay(EndPlayReason);
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

void ALogBuildPiece::RegenerateAutomaticSnapPoints()
{
    SnapPoints.RemoveAll([](const FLogSnapPoint& Point)
    {
        const FString Id = Point.PointId.ToString();
        return Id.StartsWith(TEXT("AutoEnd_")) ||
            Id.StartsWith(TEXT("AutoSide_"));
    });

    if (bAutoGenerateEndSnapPoints)
    {
        GenerateDefaultEndSnapPoints();
    }
    if (bAutoGenerateSideSnapPoints)
    {
        GenerateDefaultSideSnapPoints();
    }
}

void ALogBuildPiece::GenerateDefaultSideSnapPoints()
{
    if (!Mesh || !Mesh->GetStaticMesh() || SideSnapSpacing <= KINDA_SMALL_NUMBER)
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

    FVector SideAxisA;
    FVector SideAxisB;
    float ExtentA = 0.0f;
    float ExtentB = 0.0f;

    if (LongitudinalAxis == ELogLongitudinalAxis::X)
    {
        SideAxisA = FVector::YAxisVector;
        SideAxisB = FVector::ZAxisVector;
        ExtentA = Bounds.BoxExtent.Y;
        ExtentB = Bounds.BoxExtent.Z;
    }
    else if (LongitudinalAxis == ELogLongitudinalAxis::Y)
    {
        SideAxisA = FVector::XAxisVector;
        SideAxisB = FVector::ZAxisVector;
        ExtentA = Bounds.BoxExtent.X;
        ExtentB = Bounds.BoxExtent.Z;
    }
    else
    {
        SideAxisA = FVector::XAxisVector;
        SideAxisB = FVector::YAxisVector;
        ExtentA = Bounds.BoxExtent.X;
        ExtentB = Bounds.BoxExtent.Y;
    }

    TArray<float> AlongOffsets;
    if (HalfLength * 2.0f <= SideSnapSpacing)
    {
        AlongOffsets.Add(0.0f);
    }
    else
    {
        const float Inset = SideSnapSpacing * 0.5f;
        for (float Offset = -HalfLength + Inset;
             Offset <= HalfLength - Inset + KINDA_SMALL_NUMBER;
             Offset += SideSnapSpacing)
        {
            AlongOffsets.Add(Offset);
        }
    }

    auto AddSideAnchors = [this, &Bounds, &Axis, &AlongOffsets](
        const FVector& SideAxis, float SideExtent, const TCHAR* AxisName)
    {
        if (SideExtent <= KINDA_SMALL_NUMBER)
        {
            return;
        }

        for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
        {
            const float Sign = SideIndex == 0 ? -1.0f : 1.0f;
            const FString SignName = Sign < 0.0f ? TEXT("Minus") : TEXT("Plus");

            for (int32 AlongIndex = 0; AlongIndex < AlongOffsets.Num(); ++AlongIndex)
            {
                FLogSnapPoint Point;
                Point.PointId = FName(*FString::Printf(
                    TEXT("AutoSide_%s_%s_%02d"),
                    AxisName, *SignName, AlongIndex));
                // A side socket receives a log end. Its outward vector controls
                // the incoming log's orientation so the beam projects away
                // from the post/host log instead of centering through it.
                Point.SnapTag = SideSnapTag;
                const FVector LocalPosition = Bounds.Origin +
                    Axis * AlongOffsets[AlongIndex] +
                    SideAxis * (SideExtent * Sign);
                Point.LocalTransform = FTransform(
                    MakeOutwardRotation(SideAxis * Sign), LocalPosition);
                Point.bCanSource = false;
                Point.bCanReceive = true;
                Point.bAcceptAnyPieceType = true;
                Point.bFlipIncomingOrientation = true;
                Point.bOccupied = false;
                SnapPoints.Add(Point);
            }
        }
    };

    AddSideAnchors(SideAxisA, ExtentA, TEXT("A"));
    AddSideAnchors(SideAxisB, ExtentB, TEXT("B"));
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
    // Preserve hand-authored anchors. Rebuild only the automatic anchors against
    // the replacement mesh bounds so custom sockets are not silently discarded.
    RegenerateAutomaticSnapPoints();
}

bool ALogBuildPiece::CanApplyCutAction(ELogCutAction Action) const
{
    if (bIsPlaced || bIsHeld || GetAttachedPieceCount() > 0)
    {
        return false;
    }

    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        return PieceType == ELogPieceType::WholeLog && IsValid(SplitHalfVariantMesh);
    case ELogCutAction::Quarter:
        return PieceType == ELogPieceType::SplitHalfLog && IsValid(QuarterVariantMesh);
    case ELogCutAction::Hewn:
        return PieceType == ELogPieceType::WholeLog && IsValid(HewnVariantMesh);
    case ELogCutAction::Gutter:
        // A groove/gutter operation is valid only after the log has been split.
        return PieceType == ELogPieceType::SplitHalfLog && IsValid(GutterVariantMesh);
    case ELogCutAction::Trough:
        return PieceType == ELogPieceType::SplitHalfLog && IsValid(TroughVariantMesh);
    case ELogCutAction::Kalas:
        return PieceType == ELogPieceType::WholeLog && IsValid(KalasVariantMesh);
    case ELogCutAction::Plank:
        return PieceType == ELogPieceType::SplitHalfLog && IsValid(PlankVariantMesh);
    default:
        return false;
    }
}

FString ALogBuildPiece::GetCutActionBlockReason(ELogCutAction Action) const
{
    if (bIsPlaced)
    {
        return TEXT("Remove the log from the structure before processing it.");
    }
    if (bIsHeld)
    {
        return TEXT("Drop the log before processing it.");
    }
    if (GetAttachedPieceCount() > 0)
    {
        return TEXT("Remove attached pieces before processing the support log.");
    }

    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        return PieceType != ELogPieceType::WholeLog
            ? TEXT("Only a whole log can be split into half-rounds.")
            : TEXT("No split-half mesh is configured for this log length.");
    case ELogCutAction::Quarter:
        return PieceType != ELogPieceType::SplitHalfLog
            ? TEXT("Quartering requires a split half-round log first.")
            : TEXT("No quarter mesh is configured for this log length.");
    case ELogCutAction::Gutter:
        return PieceType != ELogPieceType::SplitHalfLog
            ? TEXT("A groove can only be cut into a split half-round log.")
            : TEXT("No gutter mesh is configured for this log length.");
    case ELogCutAction::Trough:
        return PieceType != ELogPieceType::SplitHalfLog
            ? TEXT("A trough can only be carved into a split half-round log.")
            : TEXT("No trough mesh is configured for this log length.");
    case ELogCutAction::Hewn:
        return PieceType != ELogPieceType::WholeLog
            ? TEXT("Hewing is available only on a whole log in this prototype.")
            : TEXT("No hewn mesh is configured for this log length.");
    case ELogCutAction::Kalas:
        return PieceType != ELogPieceType::WholeLog
            ? TEXT("Kalas shaping requires a whole log in this prototype.")
            : TEXT("No kalas mesh is configured for this log length.");
    case ELogCutAction::Plank:
        return PieceType != ELogPieceType::SplitHalfLog
            ? TEXT("Plank shaping requires a split half-round log first.")
            : TEXT("No plank mesh is configured for this log length.");
    default:
        return TEXT("Unsupported processing action.");
    }
}

bool ALogBuildPiece::ApplyCutAction(ELogCutAction Action)
{
    return TryApplyCutAction(Action, FGuid::NewGuid()).IsSuccess();
}

FWorldOperationResult ALogBuildPiece::TryApplyCutAction(
    ELogCutAction Action,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }

    if (!CanApplyCutAction(Action))
    {
        EWorldOperationResultCode FailureCode =
            EWorldOperationResultCode::InvalidState;

        if (!bIsPlaced && !bIsHeld && GetAttachedPieceCount() == 0)
        {
            bool bPrerequisiteMet = false;
            bool bActionSupported = true;

            switch (Action)
            {
            case ELogCutAction::SplitHalf:
            case ELogCutAction::Hewn:
            case ELogCutAction::Kalas:
                bPrerequisiteMet = PieceType == ELogPieceType::WholeLog;
                break;

            case ELogCutAction::Quarter:
            case ELogCutAction::Gutter:
            case ELogCutAction::Trough:
            case ELogCutAction::Plank:
                bPrerequisiteMet = PieceType == ELogPieceType::SplitHalfLog;
                break;

            default:
                bActionSupported = false;
                break;
            }

            FailureCode = !bActionSupported
                ? EWorldOperationResultCode::Unsupported
                : (!bPrerequisiteMet
                    ? EWorldOperationResultCode::PrerequisiteNotMet
                    : EWorldOperationResultCode::MissingConfiguration);
        }

        return FWorldOperationResult::Failure(
            FailureCode,
            OperationId,
            FText::FromString(GetCutActionBlockReason(Action)));
    }

    UStaticMesh* NewMesh = nullptr;
    ELogPieceType NewType = ELogPieceType::Other;

    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        NewMesh = SplitHalfVariantMesh;
        NewType = ELogPieceType::SplitHalfLog;
        break;
    case ELogCutAction::Quarter:
        NewMesh = QuarterVariantMesh;
        NewType = ELogPieceType::QuarterLog;
        break;
    case ELogCutAction::Hewn:
        NewMesh = HewnVariantMesh;
        NewType = ELogPieceType::HewnLog;
        break;
    case ELogCutAction::Gutter:
        NewMesh = GutterVariantMesh;
        NewType = ELogPieceType::GutterLog;
        break;
    case ELogCutAction::Trough:
        NewMesh = TroughVariantMesh;
        NewType = ELogPieceType::TroughLog;
        break;
    case ELogCutAction::Kalas:
        NewMesh = KalasVariantMesh;
        NewType = ELogPieceType::SquaredBeam;
        break;
    case ELogCutAction::Plank:
        NewMesh = PlankVariantMesh;
        NewType = ELogPieceType::Plank;
        break;
    default:
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Unsupported,
            OperationId,
            FText::FromString(TEXT("Unsupported processing action.")));
    }

    // Recheck the selected output before mutating any authoritative state.
    if (!IsValid(NewMesh) || !Mesh)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("The output mesh for this processing action is not configured.")));
    }

    // Preserve the mesh's bottom world height when replacing the geometry.
    FVector OldOrigin;
    FVector OldExtent;
    GetActorBounds(false, OldOrigin, OldExtent);
    const float OldBottomZ = OldOrigin.Z - OldExtent.Z;

    SetPieceStaticMesh(NewMesh);
    PieceType = NewType;

    FVector NewOrigin;
    FVector NewExtent;
    GetActorBounds(false, NewOrigin, NewExtent);
    const float NewBottomZ = NewOrigin.Z - NewExtent.Z;
    AddActorWorldOffset(FVector(0.0f, 0.0f, OldBottomZ - NewBottomZ), false);

    if (Mesh)
    {
        Mesh->UpdateBounds();
        if (Mesh->IsSimulatingPhysics())
        {
            Mesh->WakeAllRigidBodies();
        }
    }

    TArray<FGuid> AffectedObjectIds;
    if (UWorldObjectIdentityComponent* Identity =
        FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (Identity->EnsureIdentity())
        {
            AffectedObjectIds.Add(Identity->GetInstanceId());
        }
    }

    const FText SuccessMessage = FText::FromString(
        FString::Printf(TEXT("Log processed successfully. New piece type: %s."),
            *UEnum::GetValueAsString(NewType)));

    FWorldOperationResult Result = FWorldOperationResult::Success(
        OperationId, SuccessMessage, AffectedObjectIds);

    // Notify listeners only after mesh, type and transform have been committed.
    // Event delivery is transient; it does not determine operation success.
    if (UWorld* World = GetWorld())
    {
        if (UWorldCommittedEventSubsystem* Events =
            World->GetSubsystem<UWorldCommittedEventSubsystem>())
        {
            FWorldCommittedEvent Event;
            Event.Type = EWorldCommittedEventType::ObjectProcessed;
            Event.OperationId = OperationId;
            Event.AffectedObjectIds = AffectedObjectIds;
            Event.Message = SuccessMessage;
            if (!Events->PublishCommittedEvent(Event))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("Log processing committed for operation %s, but its notification was not published."),
                    *OperationId.ToString());
            }
        }
    }

    return Result;
}

void ALogBuildPiece::SetPieceTypeByName(FName TypeName)
{
    FString Key = TypeName.ToString().ToLower();
    Key.ReplaceInline(TEXT("_"), TEXT(""));
    Key.ReplaceInline(TEXT("-"), TEXT(""));
    Key.ReplaceInline(TEXT(" "), TEXT(""));

    // Specific geometry states must be detected before the generic raw-log name.
    if (Key.Contains(TEXT("threequarter")) || Key.Contains(TEXT("3quarter")))
    {
        PieceType = ELogPieceType::ThreeQuarterLog;
    }
    else if (Key.Contains(TEXT("halflength")) || Key.Contains(TEXT("lengthhalf")))
    {
        PieceType = ELogPieceType::HalfLengthLog;
    }
    // In this asset set "_Half" is a split half-round, not a shorter log.
    else if (Key.Contains(TEXT("rawlog")) && Key.Contains(TEXT("half")))
    {
        PieceType = ELogPieceType::SplitHalfLog;
    }
    else if (Key.Contains(TEXT("quarter")))
    {
        PieceType = ELogPieceType::QuarterLog;
    }
    else if (Key.Contains(TEXT("trough")))
    {
        PieceType = ELogPieceType::TroughLog;
    }
    else if (Key.Contains(TEXT("gutter")) || Key.Contains(TEXT("groove")))
    {
        PieceType = ELogPieceType::GutterLog;
    }
    else if (Key.Contains(TEXT("hewn")))
    {
        PieceType = ELogPieceType::HewnLog;
    }
    else if (Key.Contains(TEXT("kalas")))
    {
        PieceType = ELogPieceType::SquaredBeam;
    }
    else if (Key.Contains(TEXT("plank")))
    {
        PieceType = ELogPieceType::Plank;
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
    else if (Key.Contains(TEXT("rawlog")) || Key.Contains(TEXT("wholelog")) ||
        Key == TEXT("whole"))
    {
        PieceType = ELogPieceType::WholeLog;
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
    // Automatic anchor IDs are owned by RegenerateAutomaticSnapPoints().
    // Reject them here so a custom anchor cannot be silently removed during a
    // later construction/mesh refresh.
    const FString IdString = PointId.ToString();
    if (PointId.IsNone() ||
        IdString.StartsWith(TEXT("AutoEnd_"), ESearchCase::IgnoreCase) ||
        IdString.StartsWith(TEXT("AutoSide_"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AddCustomSnapPoint rejected invalid/reserved PointId '%s' on '%s'."),
            *IdString, *GetName());
        return;
    }

    // Point IDs are used as stable editor-facing identifiers. Duplicates make
    // selection/diagnostics ambiguous, so keep the first existing definition.
    const bool bDuplicateId = SnapPoints.ContainsByPredicate(
        [&PointId](const FLogSnapPoint& ExistingPoint)
        {
            return ExistingPoint.PointId == PointId;
        });
    if (bDuplicateId)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AddCustomSnapPoint rejected duplicate PointId '%s' on '%s'."),
            *IdString, *GetName());
        return;
    }

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
    if (StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            Graph->SetGroundSupported(StructuralIdentity->GetInstanceId(), bNewPlaced && !SnapParent.IsValid());
        }
    }

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

    // A piece may have only one structural parent. Reparenting without first
    // detaching would strand the old parent's occupied anchor/child entry.
    // Also reject moving a support with children through this low-level API.
    if (SourcePiece->SnapParent.IsValid() ||
        SourcePiece->ParentSnapPointIndex != INDEX_NONE ||
        SourcePiece->OwnSnapPointIndexUsed != INDEX_NONE ||
        SourcePiece->GetAttachedPieceCount() > 0)
    {
        return false;
    }

    // Reject a cycle: the target cannot already be inside the source piece's
    // descendant chain, otherwise attachment would create a structural loop.
    TSet<const ALogBuildPiece*> VisitedAncestors;
    for (const ALogBuildPiece* Ancestor = this;
         Ancestor;
         Ancestor = Ancestor->SnapParent.Get())
    {
        // Reject both the proposed cycle and a pre-existing corrupted cycle
        // so validation itself cannot loop forever on malformed graph state.
        if (Ancestor == SourcePiece || VisitedAncestors.Contains(Ancestor))
        {
            return false;
        }
        VisitedAncestors.Add(Ancestor);
    }

    const FLogSnapPoint& TargetPoint = SnapPoints[TargetPointIndex];
    const FLogSnapPoint& SourcePoint = SourcePiece->SnapPoints[SourcePointIndex];

    // Revalidate both ends here, not only in the carry component's candidate
    // search. TryReserveSnapPoints is callable independently and must not
    // reserve an occupied or source-only target anchor.
    if (!TargetPoint.bCanReceive ||
        TargetPoint.bOccupied ||
        !TargetPoint.AcceptsPieceType(SourcePiece->PieceType) ||
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

    // Actor attachment is the only fallible commit step. Perform it before
    // reserving either anchor so a rejected attachment leaves both endpoints
    // untouched.
    if (!SourcePiece->AttachToSnapParent(this, TargetPointIndex, SourcePointIndex))
    {
        return false;
    }

    SnapPoints[TargetPointIndex].bOccupied = true;
    SourcePiece->SnapPoints[SourcePointIndex].bOccupied = true;
    return true;
}

void ALogBuildPiece::ReleaseSnapPoint(int32 Index)
{
    if (SnapPoints.IsValidIndex(Index))
    {
        SnapPoints[Index].bOccupied = false;
    }
}

bool ALogBuildPiece::AttachToSnapParent(
    ALogBuildPiece* Parent,
    int32 ParentPointIndex,
    int32 MyPointIndex)
{
    if (!IsValid(Parent) ||
        Parent == this ||
        !Parent->SnapPoints.IsValidIndex(ParentPointIndex) ||
        !SnapPoints.IsValidIndex(MyPointIndex) ||
        SnapParent.IsValid() ||
        ParentSnapPointIndex != INDEX_NONE ||
        OwnSnapPointIndexUsed != INDEX_NONE)
    {
        return false;
    }

    // Do not publish parent/child bookkeeping until Unreal confirms the actor
    // attachment. A failed attach must not leave stale weak-child entries or
    // occupied anchors.
    if (!AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform))
    {
        return false;
    }

    SnapParent = Parent;
    ParentSnapPointIndex = ParentPointIndex;
    OwnSnapPointIndexUsed = MyPointIndex;
    Parent->RegisterAttachedPiece(this);
    if (StructuralIdentity && Parent->StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            if (!Graph->TryConnect(Parent->StructuralIdentity->GetInstanceId(),
                Parent->SnapPoints[ParentPointIndex].PointId, StructuralIdentity->GetInstanceId(),
                SnapPoints[MyPointIndex].PointId))
            {
                Parent->ReleaseSnapPoint(ParentPointIndex);
                ReleaseSnapPoint(MyPointIndex);
                Parent->UnregisterAttachedPiece(this);
                SnapParent.Reset();
                ParentSnapPointIndex = INDEX_NONE;
                OwnSnapPointIndexUsed = INDEX_NONE;
                DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                return false;
            }
        }
    }
    SetPlacedState(true);
    return true;
}

void ALogBuildPiece::DetachFromSnapParent()
{
    if (StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            Graph->TryDisconnect(StructuralIdentity->GetInstanceId());
        }
    }
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
    SetPlacedState(false);
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
