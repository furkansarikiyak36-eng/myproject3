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

namespace
{
    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    FVector GetLocalLongitudinalAxis(ELogLongitudinalAxis Axis)
    {
        switch (Axis)
        {
        case ELogLongitudinalAxis::Y: return FVector::YAxisVector;
        case ELogLongitudinalAxis::Z: return FVector::UpVector;
        case ELogLongitudinalAxis::X:
        default: return FVector::XAxisVector;
        }
    }

    ELogCrossSectionShape GetCrossSectionForPieceType(
        ELogPieceType Type, ELogCrossSectionShape Fallback = ELogCrossSectionShape::Other)
    {
        switch (Type)
        {
        case ELogPieceType::WholeLog:
        case ELogPieceType::ThreeQuarterLog:
        case ELogPieceType::HalfLengthLog:
        case ELogPieceType::QuarterLog:
            return ELogCrossSectionShape::FullRound;
        case ELogPieceType::SplitHalfLog: return ELogCrossSectionShape::HalfRound;
        case ELogPieceType::QuarterSectionLog: return ELogCrossSectionShape::QuarterRound;
        case ELogPieceType::HewnLog: return ELogCrossSectionShape::HewnFlat;
        case ELogPieceType::SquaredBeam: return ELogCrossSectionShape::SquareBeam;
        case ELogPieceType::GutterLog: return ELogCrossSectionShape::Grooved;
        case ELogPieceType::TroughLog: return ELogCrossSectionShape::Trough;
        case ELogPieceType::Plank: return ELogCrossSectionShape::Plank;
        case ELogPieceType::NotchedLog: return ELogCrossSectionShape::Notched;
        case ELogPieceType::JointPiece: return ELogCrossSectionShape::Joint;
        case ELogPieceType::Other:
        default: return Fallback;
        }
    }

    const TCHAR* GetLengthFractionToken(ELogLengthFraction Fraction)
    {
        switch (Fraction)
        {
        case ELogLengthFraction::ThreeQuarterLength: return TEXT("LengthThreeQuarter");
        case ELogLengthFraction::HalfLength: return TEXT("LengthHalf");
        case ELogLengthFraction::QuarterLength: return TEXT("LengthQuarter");
        case ELogLengthFraction::FullLength:
        default: return TEXT("");
        }
    }

    const TCHAR* GetCrossSectionToken(ELogCrossSectionShape Shape)
    {
        switch (Shape)
        {
        case ELogCrossSectionShape::FullRound: return TEXT("FullRound");
        case ELogCrossSectionShape::HalfRound: return TEXT("SplitHalfLog");
        case ELogCrossSectionShape::QuarterRound: return TEXT("QuarterSectionLog");
        case ELogCrossSectionShape::HewnFlat: return TEXT("HewnLog");
        case ELogCrossSectionShape::SquareBeam: return TEXT("SquaredBeam");
        case ELogCrossSectionShape::Grooved: return TEXT("GutterLog");
        case ELogCrossSectionShape::Trough: return TEXT("TroughLog");
        case ELogCrossSectionShape::Plank: return TEXT("Plank");
        case ELogCrossSectionShape::Notched: return TEXT("NotchedLog");
        case ELogCrossSectionShape::Joint: return TEXT("JointPiece");
        case ELogCrossSectionShape::Other:
        default: return TEXT("Other");
        }
    }

    FName BuildGeometryDefinitionId(
        FName BaseDefinitionId, ELogLengthFraction LengthFraction,
        ELogCrossSectionShape CrossSectionShape)
    {
        if (BaseDefinitionId.IsNone())
            return NAME_None;

        if (LengthFraction == ELogLengthFraction::FullLength &&
            CrossSectionShape == ELogCrossSectionShape::FullRound)
        {
            return BaseDefinitionId;
        }

        const FString LengthToken = GetLengthFractionToken(LengthFraction);
        const FString ShapeToken = GetCrossSectionToken(CrossSectionShape);
        const FString Suffix = LengthToken.IsEmpty()
            ? ShapeToken
            : LengthToken + TEXT(".") + ShapeToken;
        return FName(*FString::Printf(TEXT("%s.%s"),
            *BaseDefinitionId.ToString(), *Suffix));
    }

    bool IsValidCrossSectionCenterOffset(
        const FVector& Offset, ELogLongitudinalAxis LongitudinalAxis)
    {
        if (!IsFiniteVector(Offset) || Offset.SizeSquared() <= 1.0f)
            return false;

        // The two output centers must separate across the cross-section, not
        // down the log's length. Mesh-specific offset magnitude is authored.
        const float LongitudinalComponent =
            FMath::Abs(FVector::DotProduct(Offset.GetSafeNormal(),
                GetLocalLongitudinalAxis(LongitudinalAxis)));
        return LongitudinalComponent <= 0.05f;
    }
}


ALogBuildPiece::ALogBuildPiece()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LogMesh"));
    SetRootComponent(Mesh);
    StructuralIdentity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));

    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(false);
}

void ALogBuildPiece::UpdateGeometryStateFromPieceType(bool bPreserveLengthFraction)
{
    const ELogLengthFraction PreviousLengthFraction = GeometryState.LengthFraction;
    if (!bPreserveLengthFraction)
    {
        GeometryState.LengthFraction = ELogLengthFraction::FullLength;
    }

    switch (PieceType)
    {
    case ELogPieceType::WholeLog:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::ThreeQuarterLog:
        GeometryState.LengthFraction = ELogLengthFraction::ThreeQuarterLength;
        GeometryState.CrossSectionShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::HalfLengthLog:
        GeometryState.LengthFraction = ELogLengthFraction::HalfLength;
        GeometryState.CrossSectionShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::QuarterLog:
        GeometryState.LengthFraction = ELogLengthFraction::QuarterLength;
        GeometryState.CrossSectionShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::SplitHalfLog:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::HalfRound;
        break;
    case ELogPieceType::QuarterSectionLog:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::QuarterRound;
        break;
    case ELogPieceType::HewnLog:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::HewnFlat;
        break;
    case ELogPieceType::SquaredBeam:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::SquareBeam;
        break;
    case ELogPieceType::GutterLog:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::Grooved;
        break;
    case ELogPieceType::TroughLog:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::Trough;
        break;
    case ELogPieceType::Plank:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::Plank;
        break;
    case ELogPieceType::NotchedLog:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::Notched;
        break;
    case ELogPieceType::JointPiece:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::Joint;
        break;
    case ELogPieceType::Other:
    default:
        GeometryState.CrossSectionShape = ELogCrossSectionShape::Other;
        break;
    }

    if (bPreserveLengthFraction)
    {
        GeometryState.LengthFraction = PreviousLengthFraction;
    }
}

bool ALogBuildPiece::SelectProcessingMeshSetFromCurrentMesh()
{
    if (ProcessingMeshSets.IsEmpty())
    {
        // A persisted/authored set ID without its corresponding table is invalid;
        // do not silently fall back to a possibly different nominal mesh family.
        return SelectedProcessingMeshSetId.IsNone();
    }

    if (!SelectedProcessingMeshSetId.IsNone())
    {
        return GetActiveProcessingMeshSet() != nullptr;
    }

    const UStaticMesh* CurrentMesh = Mesh ? Mesh->GetStaticMesh() : nullptr;
    if (!CurrentMesh)
        return false;

    const FLogProcessingMeshSet* Match = nullptr;
    for (const FLogProcessingMeshSet& Candidate : ProcessingMeshSets)
    {
        bool bCandidateUsesCurrentMesh = Candidate.BaseRoundMesh.Get() == CurrentMesh;
        if (!bCandidateUsesCurrentMesh)
        {
            for (const FLogPieceMeshVariant& Variant : Candidate.MeshVariants)
            {
                if (Variant.Mesh.Get() == CurrentMesh)
                {
                    bCandidateUsesCurrentMesh = true;
                    break;
                }
            }
        }
        if (!bCandidateUsesCurrentMesh)
            continue;

        if (Match)
            return false; // Ambiguous base/variant mesh assignment must not select arbitrarily.
        Match = &Candidate;
    }

    if (!Match || Match->VariantSetId.IsNone())
        return false;

    int32 SameIdCount = 0;
    for (const FLogProcessingMeshSet& Candidate : ProcessingMeshSets)
    {
        if (Candidate.VariantSetId == Match->VariantSetId)
            ++SameIdCount;
    }
    if (SameIdCount != 1)
        return false;

    SelectedProcessingMeshSetId = Match->VariantSetId;
    return true;
}

const FLogProcessingMeshSet* ALogBuildPiece::GetActiveProcessingMeshSet() const
{
    if (ProcessingMeshSets.IsEmpty() || SelectedProcessingMeshSetId.IsNone())
        return nullptr;

    const FLogProcessingMeshSet* Match = nullptr;
    for (const FLogProcessingMeshSet& Candidate : ProcessingMeshSets)
    {
        if (Candidate.VariantSetId != SelectedProcessingMeshSetId)
            continue;

        if (Match)
            return nullptr; // Duplicate IDs are invalid; never choose by array order.
        Match = &Candidate;
    }
    if (!Match || Match->VariantSetId.IsNone() || !Match->BaseRoundMesh)
        return nullptr;

    for (int32 Index = 0; Index < Match->MeshVariants.Num(); ++Index)
    {
        const FLogPieceMeshVariant& Variant = Match->MeshVariants[Index];
        if (!Variant.Mesh ||
            (Variant.LengthFraction == ELogLengthFraction::FullLength &&
             Variant.CrossSectionShape == ELogCrossSectionShape::FullRound))
        {
            // The base-round slot is canonical for full-length/full-round.
            // Null variant rows and duplicate base-round rows are configuration errors.
            return nullptr;
        }
        for (int32 Other = Index + 1; Other < Match->MeshVariants.Num(); ++Other)
        {
            if (Match->MeshVariants[Other].LengthFraction == Variant.LengthFraction &&
                Match->MeshVariants[Other].CrossSectionShape == Variant.CrossSectionShape)
            {
                return nullptr;
            }
        }
    }
    return Match;
}

UStaticMesh* ALogBuildPiece::GetConfiguredMeshForPieceType(ELogPieceType Type) const
{
    ELogLengthFraction TargetLength = GeometryState.LengthFraction;
    ELogCrossSectionShape TargetShape = GeometryState.CrossSectionShape;

    switch (Type)
    {
    case ELogPieceType::WholeLog:
        // "WholeLog" now means full-round cross-section; the independent
        // length fraction remains authoritative when restoring shortened pieces.
        TargetShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::ThreeQuarterLog:
        TargetLength = ELogLengthFraction::ThreeQuarterLength;
        TargetShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::HalfLengthLog:
        TargetLength = ELogLengthFraction::HalfLength;
        TargetShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::QuarterLog:
        TargetLength = ELogLengthFraction::QuarterLength;
        TargetShape = ELogCrossSectionShape::FullRound;
        break;
    case ELogPieceType::SplitHalfLog:
        TargetShape = ELogCrossSectionShape::HalfRound;
        break;
    case ELogPieceType::QuarterSectionLog:
        TargetShape = ELogCrossSectionShape::QuarterRound;
        break;
    case ELogPieceType::HewnLog:
        TargetShape = ELogCrossSectionShape::HewnFlat;
        break;
    case ELogPieceType::SquaredBeam:
        TargetShape = ELogCrossSectionShape::SquareBeam;
        break;
    case ELogPieceType::GutterLog:
        TargetShape = ELogCrossSectionShape::Grooved;
        break;
    case ELogPieceType::TroughLog:
        TargetShape = ELogCrossSectionShape::Trough;
        break;
    case ELogPieceType::Plank:
        TargetShape = ELogCrossSectionShape::Plank;
        break;
    case ELogPieceType::NotchedLog:
        TargetShape = ELogCrossSectionShape::Notched;
        break;
    case ELogPieceType::JointPiece:
        TargetShape = ELogCrossSectionShape::Joint;
        break;
    case ELogPieceType::Other:
    default:
        TargetShape = ELogCrossSectionShape::Other;
        break;
    }

    if (!ProcessingMeshSets.IsEmpty())
    {
        const FLogProcessingMeshSet* Set = GetActiveProcessingMeshSet();
        if (!Set)
            return nullptr;

        if (TargetLength == ELogLengthFraction::FullLength &&
            TargetShape == ELogCrossSectionShape::FullRound)
        {
            return Set->BaseRoundMesh.Get();
        }

        const FLogPieceMeshVariant* Match = nullptr;
        for (const FLogPieceMeshVariant& Variant : Set->MeshVariants)
        {
            if (Variant.LengthFraction != TargetLength ||
                Variant.CrossSectionShape != TargetShape)
                continue;

            if (Match)
                return nullptr; // Duplicate state mappings are ambiguous.
            Match = &Variant;
        }
        return Match ? Match->Mesh.Get() : nullptr;
    }

    // Legacy compatibility path. A per-length mesh table, when configured,
    // takes precedence and deliberately fails closed if its mapping is absent.
    if (!SelectedProcessingMeshSetId.IsNone())
        return nullptr;

    switch (TargetShape)
    {
    case ELogCrossSectionShape::FullRound:
        return Mesh ? Mesh->GetStaticMesh() : nullptr;
    case ELogCrossSectionShape::HalfRound:
        return SplitHalfVariantMesh;
    case ELogCrossSectionShape::QuarterRound:
        return QuarterVariantMesh;
    case ELogCrossSectionShape::HewnFlat:
        return HewnVariantMesh;
    case ELogCrossSectionShape::SquareBeam:
        return KalasVariantMesh;
    case ELogCrossSectionShape::Grooved:
        return GutterVariantMesh;
    case ELogCrossSectionShape::Trough:
        return TroughVariantMesh;
    case ELogCrossSectionShape::Plank:
        return PlankVariantMesh;
    case ELogCrossSectionShape::Notched:
    case ELogCrossSectionShape::Joint:
    case ELogCrossSectionShape::Other:
    default:
        return nullptr;
    }
}

FVector ALogBuildPiece::GetConfiguredSplitCenterOffset(ELogCutAction Action) const
{
    if (!ProcessingMeshSets.IsEmpty())
    {
        const FLogProcessingMeshSet* Set = GetActiveProcessingMeshSet();
        if (!Set)
            return FVector::ZeroVector;
        return Action == ELogCutAction::SplitHalf
            ? Set->SplitHalfCenterOffsetLocal
            : Set->QuarterCenterOffsetLocal;
    }

    if (!SelectedProcessingMeshSetId.IsNone())
        return FVector::ZeroVector;

    return Action == ELogCutAction::SplitHalf
        ? SplitHalfPieceCenterOffsetLocal
        : QuarterPieceCenterOffsetLocal;
}

FRotator ALogBuildPiece::GetConfiguredSplitChildRotation(ELogCutAction Action) const
{
    if (!ProcessingMeshSets.IsEmpty())
    {
        const FLogProcessingMeshSet* Set = GetActiveProcessingMeshSet();
        if (!Set)
            return FRotator::ZeroRotator;
        return Action == ELogCutAction::SplitHalf
            ? Set->SplitHalfChildRotationOffsetLocal
            : Set->QuarterChildRotationOffsetLocal;
    }

    if (!SelectedProcessingMeshSetId.IsNone())
        return FRotator::ZeroRotator;

    return Action == ELogCutAction::SplitHalf
        ? SplitHalfChildRotationOffsetLocal
        : QuarterChildRotationOffsetLocal;
}

void ALogBuildPiece::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!SelectProcessingMeshSetFromCurrentMesh() && !ProcessingMeshSets.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Log '%s' has a processing-mesh table but no unique matching base-mesh set; processing actions will be blocked."),
            *GetName());
    }

    // Keep the inspector's legacy piece label and the orthogonal geometry fields aligned.
    UpdateGeometryStateFromPieceType(false);

    // Preserve hand-authored anchors (for example, test-map top sockets) but
    // rebuild only auto-generated sockets when mesh/spacing/axis properties change.
    RegenerateAutomaticSnapPoints();
}

void ALogBuildPiece::BeginPlay()
{
    Super::BeginPlay();

    bIsPlaced = bStartsPlaced;
    if (!StructuralIdentity || !GetWorld())
    {
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        return;
    }

    StructuralIdentity->EnsureIdentity();
    if (BaseDefinitionId.IsNone())
    {
        BaseDefinitionId = StructuralIdentity->GetDefinitionId();
        if (BaseDefinitionId.IsNone())
            BaseDefinitionId = FName(TEXT("Wood.LogPiece"));
    }

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    if (!Ownership)
    {
        UE_LOG(LogTemp, Error, TEXT("Log piece '%s' has no world-item ownership subsystem."), *GetName());
        if (Mesh) Mesh->SetSimulatePhysics(false);
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        return;
    }

    FWorldOwnedItemRecord Item;
    if (Ownership->TryGetItem(StructuralIdentity->GetInstanceId(), Item))
    {
        // One stable ID must never produce a second visible world representation.
        if (Item.OwnerKind != EWorldItemOwnerKind::World)
        {
            if (Mesh) Mesh->SetSimulatePhysics(false);
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            SetActorTickEnabled(false);
            return;
        }

        if (!Item.MeshVariantSetId.IsNone())
            SelectedProcessingMeshSetId = Item.MeshVariantSetId;

        if ((!ProcessingMeshSets.IsEmpty() && !GetActiveProcessingMeshSet()) ||
            (!SelectedProcessingMeshSetId.IsNone() && ProcessingMeshSets.IsEmpty()))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Log '%s' requires missing/ambiguous mesh variant set '%s'; actor was quarantined."),
                *GetName(), *SelectedProcessingMeshSetId.ToString());
            if (Mesh)
            {
                Mesh->SetSimulatePhysics(false);
                Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            SetActorTickEnabled(false);
            return;
        }

        if (Item.BaseDefinitionId.IsNone())
            Item.BaseDefinitionId = Item.DefinitionId;
        BaseDefinitionId = Item.BaseDefinitionId;
        StructuralIdentity->SetDefinitionId(Item.DefinitionId);
    }
    else
    {
        Item.ItemId = StructuralIdentity->GetInstanceId();
        const FName AuthoredDefinitionId = StructuralIdentity->GetDefinitionId();
        Item.DefinitionId = AuthoredDefinitionId.IsNone() ||
            AuthoredDefinitionId == BaseDefinitionId
            ? BuildGeometryDefinitionId(BaseDefinitionId, GeometryState.LengthFraction,
                GeometryState.CrossSectionShape)
            : AuthoredDefinitionId;
        if (Item.DefinitionId.IsNone())
            Item.DefinitionId = BaseDefinitionId;
        Item.BaseDefinitionId = BaseDefinitionId;
        Item.MeshVariantSetId = SelectedProcessingMeshSetId;
        Item.Quantity = 1;
        Item.OwnerKind = EWorldItemOwnerKind::World;
        Item.WorldLocation = GetActorLocation();
        if (!Ownership->RegisterItem(Item))
        {
            UE_LOG(LogTemp, Error, TEXT("Log piece '%s' could not register item ownership."), *GetName());
            if (Mesh) Mesh->SetSimulatePhysics(false);
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            return;
        }
        StructuralIdentity->SetDefinitionId(Item.DefinitionId);
    }

    UWorldStructuralGraphSubsystem* Graph =
        GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    FStructuralNodeRecord ExistingNode;
    const FGuid NodeId = StructuralIdentity->GetInstanceId();
    const bool bRegistered = Graph &&
        (Graph->GetNode(NodeId, ExistingNode)
            ? ExistingNode.Actor.Get() == this
            : Graph->RegisterNode(this, NodeId, EStructuralNodeKind::Wood,
                bIsPlaced, 1.0f, 100000.0f, 100.0f));
    if (!bRegistered)
    {
        UE_LOG(LogTemp, Error, TEXT("Log piece '%s' could not register structural graph node."), *GetName());
        if (Mesh) Mesh->SetSimulatePhysics(false);
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        return;
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
    const FGuid& NewOwnerId, const FVector& NewWorldLocation, FWorldOwnedItemRecord& OutRecord)
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
        ExpectedOwner, NewOwner, NewOwnerId, NewWorldLocation, OutRecord);
}

bool ALogBuildPiece::ActivateAfterOwnershipTransfer()
{
    if (!StructuralIdentity || !Mesh || !GetWorld() ||
        !StructuralIdentity->HasValidIdentity())
        return false;

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Ownership || !Graph)
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(StructuralIdentity->GetInstanceId(), Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::World ||
        Item.DefinitionId.IsNone())
        return false;

    if (!Item.MeshVariantSetId.IsNone())
        SelectedProcessingMeshSetId = Item.MeshVariantSetId;
    if ((!ProcessingMeshSets.IsEmpty() && !GetActiveProcessingMeshSet()) ||
        (!SelectedProcessingMeshSetId.IsNone() && ProcessingMeshSets.IsEmpty()))
        return false;

    FStructuralNodeRecord ExistingNode;
    if (Graph->GetNode(Item.ItemId, ExistingNode))
    {
        // A second actor with the same canonical ID must never be made visible.
        if (ExistingNode.Actor.Get() != this)
            return false;
    }
    else if (!Graph->RegisterNode(this, Item.ItemId, EStructuralNodeKind::Wood,
        false, 1.0f, 100000.0f, 100.0f))
    {
        return false;
    }

    StructuralIdentity->SetDefinitionId(Item.DefinitionId);
    BaseDefinitionId = Item.BaseDefinitionId.IsNone() ? Item.DefinitionId : Item.BaseDefinitionId;
    bIsPlaced = false;
    bStartsPlaced = false;
    SetHeldState(false);
    SetPlacedState(false);
    Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionProfileName(FName(TEXT("PhysicsActor")));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    return true;
}

void ALogBuildPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // A gameplay-destroyed physical piece must not remain as a world-owned
    // ledger item. Inventory handoff is excluded because its owner has already
    // changed before Destroy() is requested.
    if (EndPlayReason == EEndPlayReason::Destroyed && StructuralIdentity &&
        StructuralIdentity->HasValidIdentity() && GetWorld())
    {
        if (UWorldItemOwnershipSubsystem* Ownership =
            GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>())
        {
            FWorldOwnedItemRecord Item;
            if (Ownership->TryGetItem(StructuralIdentity->GetInstanceId(), Item) &&
                (Item.OwnerKind == EWorldItemOwnerKind::World ||
                 Item.OwnerKind == EWorldItemOwnerKind::Held) &&
                Item.Revision < MAX_int64)
            {
                FWorldOwnedItemRecord Consumed;
                if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
                    Item.OwnerKind, EWorldItemOwnerKind::Consumed, FGuid(),
                    GetActorLocation(), Consumed))
                {
                    UE_LOG(LogTemp, Error, TEXT("Destroyed log '%s' left an unconsumed ownership record."),
                        *GetName());
                }
            }
        }
    }

    DetachFromSnapParent(true);
    for (const TWeakObjectPtr<ALogBuildPiece>& Child : AttachedPieces)
    {
        if (Child.IsValid())
        {
            Child->DetachFromSnapParent(true);
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

    ELogPieceType OutputType = ELogPieceType::Other;
    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::FullRound)
            return false;
        OutputType = ELogPieceType::SplitHalfLog;
        return IsValid(GetConfiguredMeshForPieceType(OutputType)) &&
            IsValidCrossSectionCenterOffset(GetConfiguredSplitCenterOffset(Action), LongitudinalAxis);
    case ELogCutAction::Quarter:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return false;
        OutputType = ELogPieceType::QuarterSectionLog;
        return IsValid(GetConfiguredMeshForPieceType(OutputType)) &&
            IsValidCrossSectionCenterOffset(GetConfiguredSplitCenterOffset(Action), LongitudinalAxis);
    case ELogCutAction::Hewn:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::FullRound)
            return false;
        OutputType = ELogPieceType::HewnLog;
        break;
    case ELogCutAction::Gutter:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return false;
        OutputType = ELogPieceType::GutterLog;
        break;
    case ELogCutAction::Trough:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return false;
        OutputType = ELogPieceType::TroughLog;
        break;
    case ELogCutAction::Kalas:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::FullRound)
            return false;
        OutputType = ELogPieceType::SquaredBeam;
        break;
    case ELogCutAction::Plank:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return false;
        OutputType = ELogPieceType::Plank;
        break;
    default:
        return false;
    }

    return IsValid(GetConfiguredMeshForPieceType(OutputType));
}

FString ALogBuildPiece::GetCutActionBlockReason(ELogCutAction Action) const
{
    if (bIsPlaced)
        return TEXT("Remove the log from the structure before processing it.");
    if (bIsHeld)
        return TEXT("Drop the log before processing it.");
    if (GetAttachedPieceCount() > 0)
        return TEXT("Remove attached pieces before processing the support log.");

    if (!ProcessingMeshSets.IsEmpty() && !GetActiveProcessingMeshSet())
        return TEXT("No unique processing mesh set is selected. Select a set whose base-round mesh matches this actor; mesh length is not inferred from its filename.");

    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::FullRound)
            return TEXT("Only a full-round cross-section can be split into half-rounds.");
        if (!IsValid(GetConfiguredMeshForPieceType(ELogPieceType::SplitHalfLog)))
            return TEXT("No split-half output mesh is configured for this processing mesh set.");
        return TEXT("Configure a non-zero cross-section center offset for the two half-round outputs in the active mesh set.");
    case ELogCutAction::Quarter:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return TEXT("Quarter-round shaping requires a half-round cross-section first.");
        if (!IsValid(GetConfiguredMeshForPieceType(ELogPieceType::QuarterSectionLog)))
            return TEXT("No quarter cross-section mesh is configured for this processing mesh set.");
        return TEXT("Configure a non-zero cross-section center offset for the two quarter-section outputs in the active mesh set.");
    case ELogCutAction::Gutter:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return TEXT("A groove can only be cut into a half-round cross-section.");
        return TEXT("No gutter mesh is configured for this processing mesh set.");
    case ELogCutAction::Trough:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return TEXT("A trough can only be carved into a half-round cross-section.");
        return TEXT("No trough mesh is configured for this processing mesh set.");
    case ELogCutAction::Hewn:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::FullRound)
            return TEXT("Hewing is available only on a full-round cross-section in this prototype.");
        return TEXT("No hewn mesh is configured for this processing mesh set.");
    case ELogCutAction::Kalas:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::FullRound)
            return TEXT("Kalas shaping requires a full-round cross-section in this prototype.");
        return TEXT("No squared-beam mesh is configured for this processing mesh set.");
    case ELogCutAction::Plank:
        if (GeometryState.CrossSectionShape != ELogCrossSectionShape::HalfRound)
            return TEXT("Plank shaping requires a half-round cross-section first.");
        return TEXT("No plank mesh is configured for this processing mesh set.");
    default:
        return TEXT("Unsupported processing action.");
    }
}

bool ALogBuildPiece::ApplyCutAction(ELogCutAction Action)
{
    return TryApplyCutAction(Action, FGuid::NewGuid()).IsSuccess();
}

FWorldOperationResult ALogBuildPiece::TrySplitOutputPieces(
    ELogCutAction Action,
    const FGuid& OperationId,
    UStaticMesh* OutputMesh,
    ELogPieceType OutputType,
    FName OutputDefinitionId,
    const FWorldOwnedItemRecord& CurrentItem)
{
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph =
        World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldObjectIdentityComponent* Identity =
        FindComponentByClass<UWorldObjectIdentityComponent>();

    const FVector LocalOffset = GetConfiguredSplitCenterOffset(Action);
    if (!World || !Ownership || !Graph || !Identity || !Mesh ||
        !IsValid(OutputMesh) || OutputDefinitionId.IsNone() ||
        CurrentItem.OwnerKind != EWorldItemOwnerKind::World ||
        CurrentItem.Quantity != 1 ||
        !IsValidCrossSectionCenterOffset(LocalOffset, LongitudinalAxis))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("Physical split is missing a valid ownership, mesh, or cross-section offset configuration.")));
    }

    const FTransform OriginalTransform = GetActorTransform();
    const FVector WorldCenterOffset = OriginalTransform.TransformVector(LocalOffset);
    if (!IsFiniteVector(WorldCenterOffset) || WorldCenterOffset.SizeSquared() <= 1.0f)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("Configured split offset collapses after applying actor scale.")));
    }

    const FVector SourceOutputLocation = OriginalTransform.GetLocation() - WorldCenterOffset * 0.5f;
    const FVector ChildOutputLocation = OriginalTransform.GetLocation() + WorldCenterOffset * 0.5f;
    // The additional piece must be oriented as the complementary cut
    // section, not merely translated beside an identical half mesh. These
    // authored local rotations are per-mesh because imported pivots/cut planes
    // can differ. The defaults assume a log axis of X and may be overridden
    // in the asset Blueprint.
    const FRotator ChildRotationOffset = GetConfiguredSplitChildRotation(Action);
    const FQuat ChildOutputRotation =
        OriginalTransform.GetRotation() * ChildRotationOffset.Quaternion();

    const FTransform SourceOutputTransform(OriginalTransform.GetRotation(),
        SourceOutputLocation, OriginalTransform.GetScale3D());
    const FTransform ChildOutputTransform(ChildOutputRotation,
        ChildOutputLocation, OriginalTransform.GetScale3D());

    // Construct the child in deferred state so its stable ID and mesh are
    // configured before BeginPlay attempts ownership/graph registration.
    ALogBuildPiece* Child = World->SpawnActorDeferred<ALogBuildPiece>(
        GetClass(), ChildOutputTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Child)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InternalError,
            OperationId,
            FText::FromString(TEXT("Could not allocate the second physical output actor.")));
    }

    if (!Child->StructuralIdentity || !Child->Mesh)
    {
        Child->Destroy();
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InternalError,
            OperationId,
            FText::FromString(TEXT("The second output actor is missing its identity or mesh component.")));
    }

    Child->PieceType = OutputType;
    Child->GeometryState = GeometryState;
    Child->UpdateGeometryStateFromPieceType(true);
    Child->BaseDefinitionId = CurrentItem.BaseDefinitionId.IsNone()
        ? CurrentItem.DefinitionId : CurrentItem.BaseDefinitionId;
    Child->ProcessingMeshSets = ProcessingMeshSets;
    Child->SelectedProcessingMeshSetId = SelectedProcessingMeshSetId;
    Child->LongitudinalAxis = LongitudinalAxis;
    Child->bCanBeCarried = bCanBeCarried;
    Child->bStartsPlaced = false;
    Child->bIsPlaced = false;
    Child->bIsHeld = false;
    Child->bAutoGenerateEndSnapPoints = bAutoGenerateEndSnapPoints;
    Child->DefaultEndSnapTag = DefaultEndSnapTag;
    Child->bAutoGenerateSideSnapPoints = bAutoGenerateSideSnapPoints;
    Child->SideSnapSpacing = SideSnapSpacing;
    Child->SideSnapTag = SideSnapTag;
    Child->SplitHalfVariantMesh = SplitHalfVariantMesh;
    Child->SplitHalfPieceCenterOffsetLocal = SplitHalfPieceCenterOffsetLocal;
    Child->SplitHalfChildRotationOffsetLocal = SplitHalfChildRotationOffsetLocal;
    Child->QuarterVariantMesh = QuarterVariantMesh;
    Child->QuarterPieceCenterOffsetLocal = QuarterPieceCenterOffsetLocal;
    Child->QuarterChildRotationOffsetLocal = QuarterChildRotationOffsetLocal;
    Child->HewnVariantMesh = HewnVariantMesh;
    Child->GutterVariantMesh = GutterVariantMesh;
    Child->TroughVariantMesh = TroughVariantMesh;
    Child->KalasVariantMesh = KalasVariantMesh;
    Child->PlankVariantMesh = PlankVariantMesh;
    Child->SnapPoints.Reset();
    Child->SetPieceStaticMesh(OutputMesh);
    if (Child->Mesh)
    {
        Child->Mesh->SetSimulatePhysics(false);
        Child->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    TArray<FWorldOwnedItemRecord> BeforeItems;
    TArray<FWorldItemTransferReceipt> BeforeTransferReceipts;
    TArray<FWorldItemSplitReceipt> BeforeSplitReceipts;
    Ownership->ExportSnapshot(BeforeItems, BeforeTransferReceipts, BeforeSplitReceipts);

    TArray<FName> OutputDefinitions;
    OutputDefinitions.Add(OutputDefinitionId);
    OutputDefinitions.Add(OutputDefinitionId);
    TArray<FVector> OutputLocations;
    OutputLocations.Add(SourceOutputLocation);
    OutputLocations.Add(ChildOutputLocation);

    TArray<FWorldOwnedItemRecord> OutputRecords;
    const bool bSplitCommitted = Ownership->TrySplitItem(OperationId, CurrentItem.ItemId,
        CurrentItem.Revision, EWorldItemOwnerKind::World,
        OutputDefinitions, OutputLocations, OutputRecords);
    if (!bSplitCommitted)
    {
        Child->Destroy();
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Conflict,
            OperationId,
            FText::FromString(TEXT("Ownership ledger rejected the atomic split request.")));
    }
    if (OutputRecords.Num() != 2)
    {
        Ownership->RestoreSnapshot(BeforeItems, BeforeTransferReceipts, BeforeSplitReceipts);
        Child->Destroy();
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InternalError,
            OperationId,
            FText::FromString(TEXT("The ownership ledger returned an invalid output count; split state was restored.")));
    }

    const UStaticMesh* OldMesh = Mesh->GetStaticMesh();
    const ELogPieceType OldType = PieceType;
    const FLogPieceGeometryState OldGeometryState = GeometryState;
    const FName OldBaseDefinition = BaseDefinitionId;
    const FName OldDefinition = Identity->GetDefinitionId();
    const TArray<FLogSnapPoint> OldSnapPoints = SnapPoints;
    const bool bOldSimulatesPhysics = Mesh->IsSimulatingPhysics();
    const ECollisionEnabled::Type OldCollisionEnabled = Mesh->GetCollisionEnabled();
    const FName OldCollisionProfile = Mesh->GetCollisionProfileName();
    const bool bOldActorCollisionEnabled = GetActorEnableCollision();
    const bool bOldHidden = IsHidden();
    const FTransform OldTransform = OriginalTransform;

    auto RollbackPhysicalSplit = [&]()
    {
        if (IsValid(Child))
        {
            Child->Destroy();
        }

        const bool bOwnershipRestored =
            Ownership->RestoreSnapshot(BeforeItems, BeforeTransferReceipts, BeforeSplitReceipts);
        SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
        SetPieceStaticMesh(const_cast<UStaticMesh*>(OldMesh));
        PieceType = OldType;
        GeometryState = OldGeometryState;
        BaseDefinitionId = OldBaseDefinition;
        SnapPoints = OldSnapPoints;
        Identity->SetDefinitionId(OldDefinition);
        SetActorHiddenInGame(bOldHidden);
        SetActorEnableCollision(bOldActorCollisionEnabled);
        if (Mesh)
        {
            Mesh->SetSimulatePhysics(bOldSimulatesPhysics);
            Mesh->SetCollisionProfileName(OldCollisionProfile);
            Mesh->SetCollisionEnabled(OldCollisionEnabled);
        }
        if (!bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error,
                TEXT("Physical log split rollback failed for operation %s; stop world simulation and recover the world."),
                *OperationId.ToString());
        }
    };

    SetActorTransform(SourceOutputTransform, false, nullptr, ETeleportType::TeleportPhysics);
    SetPieceStaticMesh(OutputMesh);
    PieceType = OutputType;
    UpdateGeometryStateFromPieceType(true);
    BaseDefinitionId = CurrentItem.BaseDefinitionId.IsNone()
        ? CurrentItem.DefinitionId : CurrentItem.BaseDefinitionId;
    Identity->SetDefinitionId(OutputDefinitionId);
    if (Mesh)
    {
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
        Mesh->UpdateBounds();
    }

    Child->StructuralIdentity->InstanceId = OutputRecords[1].ItemId;
    Child->StructuralIdentity->SetDefinitionId(OutputRecords[1].DefinitionId);
    Child->FinishSpawning(ChildOutputTransform);
    // Construction scripts synchronize legacy PieceType labels. Reapply the
    // source's orthogonal length fraction after construction so splitting a
    // shortened log does not silently turn it back into a full-length piece.
    Child->GeometryState = GeometryState;

    FWorldOwnedItemRecord ChildItem;
    FStructuralNodeRecord ChildNode;
    const bool bChildRegistered =
        Ownership->TryGetItem(OutputRecords[1].ItemId, ChildItem) &&
        ChildItem.OwnerKind == EWorldItemOwnerKind::World &&
        ChildItem.DefinitionId == OutputDefinitionId &&
        Graph->GetNode(OutputRecords[1].ItemId, ChildNode) &&
        ChildNode.Actor.Get() == Child;
    FWorldOwnedItemRecord SourceAfterSplit;
    const bool bSourceCommitted =
        Ownership->TryGetItem(CurrentItem.ItemId, SourceAfterSplit) &&
        SourceAfterSplit.OwnerKind == EWorldItemOwnerKind::World &&
        SourceAfterSplit.DefinitionId == OutputDefinitionId &&
        SourceAfterSplit.Revision == CurrentItem.Revision + 1;
    if (!bChildRegistered || !bSourceCommitted)
    {
        RollbackPhysicalSplit();
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InternalError,
            OperationId,
            FText::FromString(TEXT("Physical split output registration failed; the transaction was rolled back.")));
    }

    TArray<FGuid> AffectedIds;
    AffectedIds.Add(OutputRecords[0].ItemId);
    AffectedIds.Add(OutputRecords[1].ItemId);
    const FText Message = FText::FromString(
        Action == ELogCutAction::SplitHalf
            ? TEXT("Log split into two half-round physical pieces.")
            : TEXT("Split half-round into two quarter-section physical pieces."));

    if (UWorldCommittedEventSubsystem* Events =
        World->GetSubsystem<UWorldCommittedEventSubsystem>())
    {
        FWorldCommittedEvent Event;
        Event.Type = EWorldCommittedEventType::ObjectProcessed;
        Event.OperationId = OperationId;
        Event.AffectedObjectIds = AffectedIds;
        Event.Message = Message;
        if (!Events->PublishCommittedEvent(Event))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Physical split committed for operation %s but its event was not published."),
                *OperationId.ToString());
        }
    }

    return FWorldOperationResult::Success(OperationId, Message, AffectedIds);
}

FWorldOperationResult ALogBuildPiece::TryApplyCutAction(
    ELogCutAction Action,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }

    // A retry of the same cut operation must return its committed result even
    // though the first attempt has already changed PieceType and the prerequisites.
    ELogPieceType RetryTargetType = ELogPieceType::Other;
    bool bRetryTargetSupported = true;
    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        RetryTargetType = ELogPieceType::SplitHalfLog; break;
    case ELogCutAction::Quarter:
        RetryTargetType = ELogPieceType::QuarterSectionLog; break;
    case ELogCutAction::Hewn:
        RetryTargetType = ELogPieceType::HewnLog; break;
    case ELogCutAction::Gutter:
        RetryTargetType = ELogPieceType::GutterLog; break;
    case ELogCutAction::Trough:
        RetryTargetType = ELogPieceType::TroughLog; break;
    case ELogCutAction::Kalas:
        RetryTargetType = ELogPieceType::SquaredBeam; break;
    case ELogCutAction::Plank:
        RetryTargetType = ELogPieceType::Plank; break;
    default:
        bRetryTargetSupported = false; break;
    }

    UWorldObjectIdentityComponent* RetryIdentity =
        FindComponentByClass<UWorldObjectIdentityComponent>();
    UWorldItemOwnershipSubsystem* RetryOwnership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    FWorldOwnedItemRecord RetryCurrent;
    if (bRetryTargetSupported && RetryIdentity && RetryOwnership &&
        RetryOwnership->TryGetItem(RetryIdentity->GetInstanceId(), RetryCurrent) &&
        RetryCurrent.OwnerKind == EWorldItemOwnerKind::World &&
        RetryCurrent.Revision > 1)
    {
        const FName RetryBase = !RetryCurrent.BaseDefinitionId.IsNone()
            ? RetryCurrent.BaseDefinitionId
            : (!BaseDefinitionId.IsNone() ? BaseDefinitionId : RetryCurrent.DefinitionId);
        const FName RetryDefinition = BuildGeometryDefinitionId(
            RetryBase, GeometryState.LengthFraction,
            GetCrossSectionForPieceType(RetryTargetType));
        const FName LegacyQuarterDefinition(*FString::Printf(
            TEXT("%s.QuarterLog"), *RetryBase.ToString()));
        const bool bCanonicalRetryMatch =
            RetryCurrent.DefinitionId == RetryDefinition && PieceType == RetryTargetType;
        // Pre-migration cut operations stored ".QuarterLog". Accept that receipt
        // only when the loaded actor has already resolved the legacy suffix to
        // the explicit QuarterSectionLog type.
        const bool bLegacyQuarterRetryMatch =
            Action == ELogCutAction::Quarter &&
            GeometryState.LengthFraction == ELogLengthFraction::FullLength &&
            RetryCurrent.DefinitionId == LegacyQuarterDefinition &&
            PieceType == ELogPieceType::QuarterSectionLog;
        const FName RequestedRetryDefinition =
            bLegacyQuarterRetryMatch ? LegacyQuarterDefinition : RetryDefinition;
        if (bCanonicalRetryMatch && (Action == ELogCutAction::SplitHalf ||
            Action == ELogCutAction::Quarter))
        {
            TArray<FWorldOwnedItemRecord> PreviousSplitRecords;
            if (RetryOwnership->TryGetSplitResult(OperationId, PreviousSplitRecords) &&
                PreviousSplitRecords.Num() == 2 &&
                PreviousSplitRecords[0].ItemId == RetryCurrent.ItemId &&
                PreviousSplitRecords[0].Revision == RetryCurrent.Revision &&
                PreviousSplitRecords[0].DefinitionId == RetryDefinition &&
                PreviousSplitRecords[1].DefinitionId == RetryDefinition)
            {
                TArray<FName> OutputDefinitions;
                OutputDefinitions.Add(RetryDefinition);
                OutputDefinitions.Add(RetryDefinition);
                TArray<FVector> OutputLocations;
                OutputLocations.Add(PreviousSplitRecords[0].WorldLocation);
                OutputLocations.Add(PreviousSplitRecords[1].WorldLocation);
                TArray<FWorldOwnedItemRecord> RetriedRecords;
                if (RetryOwnership->TrySplitItem(OperationId, RetryCurrent.ItemId,
                    RetryCurrent.Revision - 1, EWorldItemOwnerKind::World,
                    OutputDefinitions, OutputLocations, RetriedRecords))
                {
                    TArray<FGuid> RetriedIds;
                    for (const FWorldOwnedItemRecord& Record : RetriedRecords)
                        RetriedIds.Add(Record.ItemId);
                    return FWorldOperationResult::Success(
                        OperationId,
                        FText::FromString(TEXT("The previous multi-piece processing result was returned.")),
                        RetriedIds);
                }
            }
        }

        if (bCanonicalRetryMatch || bLegacyQuarterRetryMatch)
        {
            FWorldOwnedItemRecord PreviousResult;
            if (RetryOwnership->TryUpdateItemDefinition(OperationId, RetryCurrent.ItemId,
                RetryCurrent.Revision - 1, EWorldItemOwnerKind::World,
                RequestedRetryDefinition, PreviousResult))
            {
                TArray<FGuid> RetriedIds;
                RetriedIds.Add(RetryCurrent.ItemId);
                return FWorldOperationResult::Success(
                    OperationId,
                    FText::FromString(TEXT("The previous log-processing result was returned.")),
                    RetriedIds);
            }
        }
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
                bPrerequisiteMet =
                    GeometryState.CrossSectionShape == ELogCrossSectionShape::FullRound;
                break;

            case ELogCutAction::Quarter:
            case ELogCutAction::Gutter:
            case ELogCutAction::Trough:
            case ELogCutAction::Plank:
                bPrerequisiteMet =
                    GeometryState.CrossSectionShape == ELogCrossSectionShape::HalfRound;
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
        NewType = ELogPieceType::SplitHalfLog;
        break;
    case ELogCutAction::Quarter:
        NewType = ELogPieceType::QuarterSectionLog;
        break;
    case ELogCutAction::Hewn:
        NewType = ELogPieceType::HewnLog;
        break;
    case ELogCutAction::Gutter:
        NewType = ELogPieceType::GutterLog;
        break;
    case ELogCutAction::Trough:
        NewType = ELogPieceType::TroughLog;
        break;
    case ELogCutAction::Kalas:
        NewType = ELogPieceType::SquaredBeam;
        break;
    case ELogCutAction::Plank:
        NewType = ELogPieceType::Plank;
        break;
    default:
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Unsupported,
            OperationId,
            FText::FromString(TEXT("Unsupported processing action.")));
    }

    NewMesh = GetConfiguredMeshForPieceType(NewType);

    // Recheck the selected output before mutating any authoritative state.
    if (!IsValid(NewMesh) || !Mesh)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("The output mesh for this processing action is not configured.")));
    }

    // Transform the authoritative ledger definition before changing geometry.
    // The stable base ID allows repeated processing to replace the current suffix,
    // rather than building a nested chain of item names.
    UWorldObjectIdentityComponent* ItemIdentity =
        FindComponentByClass<UWorldObjectIdentityComponent>();
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    FWorldOwnedItemRecord CurrentItem;
    if (!ItemIdentity || !Ownership ||
        !Ownership->TryGetItem(ItemIdentity->GetInstanceId(), CurrentItem) ||
        CurrentItem.OwnerKind != EWorldItemOwnerKind::World)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidState,
            OperationId,
            FText::FromString(TEXT("The log does not currently have world ownership.")));
    }

    const FName EffectiveBaseDefinition = !CurrentItem.BaseDefinitionId.IsNone()
        ? CurrentItem.BaseDefinitionId
        : (!BaseDefinitionId.IsNone() ? BaseDefinitionId : CurrentItem.DefinitionId);
    const FName NewDefinitionId = BuildGeometryDefinitionId(
        EffectiveBaseDefinition, GeometryState.LengthFraction,
        GetCrossSectionForPieceType(NewType));
    if (NewDefinitionId.IsNone())
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("Could not create a stable definition ID for the requested geometry state.")));
    }

    if (Action == ELogCutAction::SplitHalf || Action == ELogCutAction::Quarter)
    {
        return TrySplitOutputPieces(
            Action, OperationId, NewMesh, NewType, NewDefinitionId, CurrentItem);
    }

    FWorldOwnedItemRecord TransformedItem;
    if (!Ownership->TryUpdateItemDefinition(OperationId, CurrentItem.ItemId,
        CurrentItem.Revision, EWorldItemOwnerKind::World, NewDefinitionId, TransformedItem))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Conflict,
            OperationId,
            FText::FromString(TEXT("The world-item definition changed or could not be committed.")));
    }

    BaseDefinitionId = EffectiveBaseDefinition;
    ItemIdentity->SetDefinitionId(NewDefinitionId);

    // Preserve the mesh's bottom world height when replacing the geometry.
    FVector OldOrigin;
    FVector OldExtent;
    GetActorBounds(false, OldOrigin, OldExtent);
    const float OldBottomZ = OldOrigin.Z - OldExtent.Z;

    SetPieceStaticMesh(NewMesh);
    PieceType = NewType;
    UpdateGeometryStateFromPieceType(true);

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

    // Explicit cross-section names must be resolved before generic "quarter".
    // "QuarterLog" remains a legacy length-piece label; the cut action now emits
    // the unambiguous QuarterSectionLog type.
    if (Key.Contains(TEXT("quartersection")) || Key.Contains(TEXT("quarterround")))
    {
        PieceType = ELogPieceType::QuarterSectionLog;
    }
    // Specific length states must be detected before the generic raw-log name.
    else if (Key.Contains(TEXT("threequarter")) || Key.Contains(TEXT("3quarter")))
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
    // The current imported FBX names use RawLog + Quarter for a quarter-round
    // cross-section, whereas a standalone QuarterLog label is a legacy length
    // fraction. Keep these two interpretations distinct.
    else if (Key.Contains(TEXT("rawlog")) && Key.Contains(TEXT("quarter")))
    {
        PieceType = ELogPieceType::QuarterSectionLog;
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

    UpdateGeometryStateFromPieceType(false);
}

bool ALogBuildPiece::ApplyDefinitionVariant(FName CurrentDefinitionId)
{
    if (CurrentDefinitionId.IsNone() || BaseDefinitionId.IsNone() || !Mesh)
        return false;

    // The unprocessed/base definition is configured by the actor Blueprint.
    if (CurrentDefinitionId == BaseDefinitionId)
    {
        if (UWorldObjectIdentityComponent* Identity =
            FindComponentByClass<UWorldObjectIdentityComponent>())
        {
            Identity->SetDefinitionId(CurrentDefinitionId);
        }
        UpdateGeometryStateFromPieceType(false);
        return true;
    }

    const FString BaseText = BaseDefinitionId.ToString();
    const FString CurrentText = CurrentDefinitionId.ToString();
    const FString Prefix = BaseText + TEXT(".");
    if (!CurrentText.StartsWith(Prefix, ESearchCase::IgnoreCase))
        return false;

    FString ShapeSuffix = CurrentText.RightChop(Prefix.Len());
    ELogLengthFraction RequestedLengthFraction = GeometryState.LengthFraction;
    bool bHasExplicitLengthFraction = false;

    FString LengthToken;
    FString ParsedShapeSuffix;
    if (ShapeSuffix.Split(TEXT("."), &LengthToken, &ParsedShapeSuffix, ESearchCase::IgnoreCase))
    {
        if (LengthToken.Equals(TEXT("LengthFull"), ESearchCase::IgnoreCase))
            RequestedLengthFraction = ELogLengthFraction::FullLength;
        else if (LengthToken.Equals(TEXT("LengthThreeQuarter"), ESearchCase::IgnoreCase))
            RequestedLengthFraction = ELogLengthFraction::ThreeQuarterLength;
        else if (LengthToken.Equals(TEXT("LengthHalf"), ESearchCase::IgnoreCase))
            RequestedLengthFraction = ELogLengthFraction::HalfLength;
        else if (LengthToken.Equals(TEXT("LengthQuarter"), ESearchCase::IgnoreCase))
            RequestedLengthFraction = ELogLengthFraction::QuarterLength;
        else
            return false;

        ShapeSuffix = ParsedShapeSuffix;
        bHasExplicitLengthFraction = true;
    }

    UStaticMesh* VariantMesh = nullptr;
    ELogPieceType VariantType = ELogPieceType::Other;
    if (ShapeSuffix.Equals(TEXT("FullRound"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::WholeLog;
    }
    else if (ShapeSuffix.Equals(TEXT("SplitHalfLog"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::SplitHalfLog;
    }
    else if (ShapeSuffix.Equals(TEXT("QuarterSectionLog"), ESearchCase::IgnoreCase) ||
        ShapeSuffix.Equals(TEXT("QuarterLog"), ESearchCase::IgnoreCase))
    {
        // ".QuarterLog" is a backwards-compatibility alias emitted by earlier
        // versions for a quarter-round cross-section. New commits use the
        // explicit ".QuarterSectionLog" suffix.
        VariantType = ELogPieceType::QuarterSectionLog;
    }
    else if (ShapeSuffix.Equals(TEXT("HewnLog"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::HewnLog;
    }
    else if (ShapeSuffix.Equals(TEXT("GutterLog"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::GutterLog;
    }
    else if (ShapeSuffix.Equals(TEXT("TroughLog"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::TroughLog;
    }
    else if (ShapeSuffix.Equals(TEXT("SquaredBeam"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::SquaredBeam;
    }
    else if (ShapeSuffix.Equals(TEXT("Plank"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::Plank;
    }
    else if (ShapeSuffix.Equals(TEXT("NotchedLog"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::NotchedLog;
    }
    else if (ShapeSuffix.Equals(TEXT("JointPiece"), ESearchCase::IgnoreCase))
    {
        VariantType = ELogPieceType::JointPiece;
    }
    else
    {
        return false;
    }

    const ELogLengthFraction PreviousLengthFraction = GeometryState.LengthFraction;
    if (bHasExplicitLengthFraction)
        GeometryState.LengthFraction = RequestedLengthFraction;

    VariantMesh = GetConfiguredMeshForPieceType(VariantType);
    if (!IsValid(VariantMesh))
    {
        GeometryState.LengthFraction = PreviousLengthFraction;
        return false;
    }

    // Preserve the lower contact plane as geometry dimensions change.
    FVector OldOrigin;
    FVector OldExtent;
    GetActorBounds(false, OldOrigin, OldExtent);
    const float OldBottomZ = OldOrigin.Z - OldExtent.Z;

    SetPieceStaticMesh(VariantMesh);
    PieceType = VariantType;
    UpdateGeometryStateFromPieceType(true);
    if (Mesh)
        Mesh->UpdateBounds();

    FVector NewOrigin;
    FVector NewExtent;
    GetActorBounds(false, NewOrigin, NewExtent);
    const float NewBottomZ = NewOrigin.Z - NewExtent.Z;
    AddActorWorldOffset(FVector(0.0f, 0.0f, OldBottomZ - NewBottomZ), false);

    if (UWorldObjectIdentityComponent* Identity =
        FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        Identity->SetDefinitionId(CurrentDefinitionId);
    }
    return true;
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
    if (!IsValid(Parent) || Parent == this ||
        !Parent->SnapPoints.IsValidIndex(ParentPointIndex) ||
        !SnapPoints.IsValidIndex(MyPointIndex) ||
        SnapParent.IsValid() || ParentSnapPointIndex != INDEX_NONE ||
        OwnSnapPointIndexUsed != INDEX_NONE ||
        !StructuralIdentity || !Parent->StructuralIdentity || !GetWorld())
    {
        return false;
    }

    UWorldStructuralGraphSubsystem* Graph =
        GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Graph)
        return false;

    // Do not publish parent/child bookkeeping until Unreal attachment and the
    // canonical graph edge have both committed successfully.
    if (!AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform))
        return false;

    SnapParent = Parent;
    ParentSnapPointIndex = ParentPointIndex;
    OwnSnapPointIndexUsed = MyPointIndex;
    Parent->RegisterAttachedPiece(this);

    if (!Graph->TryConnect(Parent->StructuralIdentity->GetInstanceId(),
        Parent->SnapPoints[ParentPointIndex].PointId,
        StructuralIdentity->GetInstanceId(), SnapPoints[MyPointIndex].PointId))
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

    SetPlacedState(true);
    return true;
}

bool ALogBuildPiece::DetachFromSnapParent(bool bAllowGraphFailure)
{
    ALogBuildPiece* Parent = SnapParent.Get();
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;

    if (Parent && (!StructuralIdentity || !Parent->StructuralIdentity || !Graph))
    {
        if (!bAllowGraphFailure)
            return false;
    }

    if (StructuralIdentity && Graph)
    {
        FStructuralNodeRecord Node;
        if (Graph->GetNode(StructuralIdentity->GetInstanceId(), Node) &&
            Node.ParentNodeId.IsValid())
        {
            const FGuid ExpectedParentId = Parent && Parent->StructuralIdentity
                ? Parent->StructuralIdentity->GetInstanceId() : FGuid();
            if ((!ExpectedParentId.IsValid() || Node.ParentNodeId != ExpectedParentId ||
                 !Graph->TryDisconnect(StructuralIdentity->GetInstanceId())) &&
                !bAllowGraphFailure)
            {
                return false;
            }
        }
    }
    else if (Parent && !bAllowGraphFailure)
    {
        return false;
    }

    // Graph disconnect has either committed, or this is lifecycle cleanup while
    // the actor is already being destroyed. Only now mutate actor/anchor state.
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    if (Parent)
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
    return true;
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
