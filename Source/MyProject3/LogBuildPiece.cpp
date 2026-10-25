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
    const TCHAR* LengthFractionToken(ELogLengthFraction Value)
    {
        switch (Value)
        {
        case ELogLengthFraction::ThreeQuarterLength: return TEXT("ThreeQuarter");
        case ELogLengthFraction::HalfLength: return TEXT("Half");
        case ELogLengthFraction::QuarterLength: return TEXT("Quarter");
        case ELogLengthFraction::Other: return TEXT("Other");
        case ELogLengthFraction::FullLength:
        default: return TEXT("Full");
        }
    }

    bool TryParseLengthFractionToken(const FString& Token, ELogLengthFraction& OutValue)
    {
        if (Token.Equals(TEXT("Full"), ESearchCase::IgnoreCase))
            OutValue = ELogLengthFraction::FullLength;
        else if (Token.Equals(TEXT("ThreeQuarter"), ESearchCase::IgnoreCase))
            OutValue = ELogLengthFraction::ThreeQuarterLength;
        else if (Token.Equals(TEXT("Half"), ESearchCase::IgnoreCase))
            OutValue = ELogLengthFraction::HalfLength;
        else if (Token.Equals(TEXT("Quarter"), ESearchCase::IgnoreCase))
            OutValue = ELogLengthFraction::QuarterLength;
        else if (Token.Equals(TEXT("Other"), ESearchCase::IgnoreCase))
            OutValue = ELogLengthFraction::Other;
        else
            return false;
        return true;
    }

    const TCHAR* CrossSectionToken(ELogCrossSectionProfile Value)
    {
        switch (Value)
        {
        case ELogCrossSectionProfile::HalfRound: return TEXT("HalfRound");
        case ELogCrossSectionProfile::QuarterRound: return TEXT("QuarterRound");
        case ELogCrossSectionProfile::Hewn: return TEXT("Hewn");
        case ELogCrossSectionProfile::SquareBeam: return TEXT("SquareBeam");
        case ELogCrossSectionProfile::GroovedHalfRound: return TEXT("GroovedHalfRound");
        case ELogCrossSectionProfile::Trough: return TEXT("Trough");
        case ELogCrossSectionProfile::Plank: return TEXT("Plank");
        case ELogCrossSectionProfile::Notched: return TEXT("Notched");
        case ELogCrossSectionProfile::Joint: return TEXT("Joint");
        case ELogCrossSectionProfile::Other: return TEXT("Other");
        case ELogCrossSectionProfile::Round:
        default: return TEXT("Round");
        }
    }

    bool TryParseCrossSectionToken(const FString& Token, ELogCrossSectionProfile& OutValue)
    {
        if (Token.Equals(TEXT("Round"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::Round;
        else if (Token.Equals(TEXT("HalfRound"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::HalfRound;
        else if (Token.Equals(TEXT("QuarterRound"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::QuarterRound;
        else if (Token.Equals(TEXT("Hewn"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::Hewn;
        else if (Token.Equals(TEXT("SquareBeam"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::SquareBeam;
        else if (Token.Equals(TEXT("GroovedHalfRound"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::GroovedHalfRound;
        else if (Token.Equals(TEXT("Trough"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::Trough;
        else if (Token.Equals(TEXT("Plank"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::Plank;
        else if (Token.Equals(TEXT("Notched"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::Notched;
        else if (Token.Equals(TEXT("Joint"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::Joint;
        else if (Token.Equals(TEXT("Other"), ESearchCase::IgnoreCase))
            OutValue = ELogCrossSectionProfile::Other;
        else
            return false;
        return true;
    }

    FName MakeCanonicalGeometryDefinitionId(
        FName BaseDefinitionId, ELogLengthFraction Length, ELogCrossSectionProfile Profile)
    {
        if (BaseDefinitionId.IsNone())
            return NAME_None;
        return FName(*FString::Printf(TEXT("%s.Geom.%s.%s"),
            *BaseDefinitionId.ToString(), LengthFractionToken(Length), CrossSectionToken(Profile)));
    }

    bool TryParseCanonicalGeometrySuffix(
        const FString& Suffix, ELogLengthFraction& OutLength, ELogCrossSectionProfile& OutProfile)
    {
        TArray<FString> Parts;
        Suffix.ParseIntoArray(Parts, TEXT("."), false);
        return Parts.Num() == 3 &&
            Parts[0].Equals(TEXT("Geom"), ESearchCase::IgnoreCase) &&
            TryParseLengthFractionToken(Parts[1], OutLength) &&
            TryParseCrossSectionToken(Parts[2], OutProfile);
    }

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

void ALogBuildPiece::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    SyncCanonicalGeometryFromLegacyPieceType();
    if (!IsValid(BaseLogVariantMesh) && Mesh && Mesh->GetStaticMesh() &&
        GetCrossSectionProfile() == ELogCrossSectionProfile::Round)
    {
        BaseLogVariantMesh = Mesh->GetStaticMesh();
        BaseLogVariantLengthFraction = GetLengthFraction();
    }

    // Preserve hand-authored anchors (for example, test-map top sockets) but
    // rebuild only auto-generated sockets when mesh/spacing/axis properties change.
    RegenerateAutomaticSnapPoints();
}

void ALogBuildPiece::BeginPlay()
{
    Super::BeginPlay();

    SyncCanonicalGeometryFromLegacyPieceType();
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

        if (Item.BaseDefinitionId.IsNone())
            Item.BaseDefinitionId = Item.DefinitionId;
        BaseDefinitionId = Item.BaseDefinitionId;
        StructuralIdentity->SetDefinitionId(Item.DefinitionId);
    }
    else
    {
        Item.ItemId = StructuralIdentity->GetInstanceId();
        Item.DefinitionId = StructuralIdentity->GetDefinitionId().IsNone()
            ? BaseDefinitionId : StructuralIdentity->GetDefinitionId();
        Item.BaseDefinitionId = BaseDefinitionId;
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

ELogLengthFraction ALogBuildPiece::LengthFractionFromLegacyType(ELogPieceType Type)
{
    switch (Type)
    {
    case ELogPieceType::ThreeQuarterLog: return ELogLengthFraction::ThreeQuarterLength;
    case ELogPieceType::HalfLengthLog: return ELogLengthFraction::HalfLength;
    case ELogPieceType::QuarterLog: return ELogLengthFraction::QuarterLength;
    default: return ELogLengthFraction::FullLength;
    }
}

ELogCrossSectionProfile ALogBuildPiece::CrossSectionFromLegacyType(ELogPieceType Type)
{
    switch (Type)
    {
    case ELogPieceType::WholeLog:
    case ELogPieceType::ThreeQuarterLog:
    case ELogPieceType::HalfLengthLog:
    case ELogPieceType::QuarterLog:
        return ELogCrossSectionProfile::Round;
    case ELogPieceType::SplitHalfLog:
        return ELogCrossSectionProfile::HalfRound;
    case ELogPieceType::QuarterSectionLog:
        return ELogCrossSectionProfile::QuarterRound;
    case ELogPieceType::HewnLog:
        return ELogCrossSectionProfile::Hewn;
    case ELogPieceType::SquaredBeam:
        return ELogCrossSectionProfile::SquareBeam;
    case ELogPieceType::GutterLog:
        return ELogCrossSectionProfile::GroovedHalfRound;
    case ELogPieceType::TroughLog:
        return ELogCrossSectionProfile::Trough;
    case ELogPieceType::Plank:
        return ELogCrossSectionProfile::Plank;
    case ELogPieceType::NotchedLog:
        return ELogCrossSectionProfile::Notched;
    case ELogPieceType::JointPiece:
        return ELogCrossSectionProfile::Joint;
    case ELogPieceType::Other:
    default:
        return ELogCrossSectionProfile::Other;
    }
}

ELogLengthFraction ALogBuildPiece::GetLengthFraction() const
{
    return bUseCanonicalGeometryDimensions
        ? LengthFraction
        : LengthFractionFromLegacyType(PieceType);
}

ELogCrossSectionProfile ALogBuildPiece::GetCrossSectionProfile() const
{
    return bUseCanonicalGeometryDimensions
        ? CrossSectionProfile
        : CrossSectionFromLegacyType(PieceType);
}

void ALogBuildPiece::SyncCanonicalGeometryFromLegacyPieceType()
{
    if (bUseCanonicalGeometryDimensions)
        return;

    LengthFraction = LengthFractionFromLegacyType(PieceType);
    CrossSectionProfile = CrossSectionFromLegacyType(PieceType);
}

void ALogBuildPiece::SetCanonicalCrossSectionFromPieceType(ELogPieceType Type)
{
    CrossSectionProfile = CrossSectionFromLegacyType(Type);
    bUseCanonicalGeometryDimensions = true;
}

ELogPieceType ALogBuildPiece::LegacyTypeFromCanonicalDimensions(
    ELogLengthFraction InLength, ELogCrossSectionProfile InProfile)
{
    if (InProfile == ELogCrossSectionProfile::Round)
    {
        switch (InLength)
        {
        case ELogLengthFraction::ThreeQuarterLength: return ELogPieceType::ThreeQuarterLog;
        case ELogLengthFraction::HalfLength: return ELogPieceType::HalfLengthLog;
        case ELogLengthFraction::QuarterLength: return ELogPieceType::QuarterLog;
        case ELogLengthFraction::FullLength: return ELogPieceType::WholeLog;
        case ELogLengthFraction::Other:
        default: return ELogPieceType::Other;
        }
    }

    switch (InProfile)
    {
    case ELogCrossSectionProfile::HalfRound: return ELogPieceType::SplitHalfLog;
    case ELogCrossSectionProfile::QuarterRound: return ELogPieceType::QuarterSectionLog;
    case ELogCrossSectionProfile::Hewn: return ELogPieceType::HewnLog;
    case ELogCrossSectionProfile::SquareBeam: return ELogPieceType::SquaredBeam;
    case ELogCrossSectionProfile::GroovedHalfRound: return ELogPieceType::GutterLog;
    case ELogCrossSectionProfile::Trough: return ELogPieceType::TroughLog;
    case ELogCrossSectionProfile::Plank: return ELogPieceType::Plank;
    case ELogCrossSectionProfile::Notched: return ELogPieceType::NotchedLog;
    case ELogCrossSectionProfile::Joint: return ELogPieceType::JointPiece;
    case ELogCrossSectionProfile::Other:
    case ELogCrossSectionProfile::Round:
    default: return ELogPieceType::Other;
    }
}

bool ALogBuildPiece::TryResolveGeometryVariant(
    ELogLengthFraction InLength,
    ELogCrossSectionProfile InProfile,
    UStaticMesh*& OutMesh,
    FVector* OutSecondPieceCenterOffset,
    FRotator* OutSecondPieceRotationOffset) const
{
    OutMesh = nullptr;
    if (OutSecondPieceCenterOffset)
        *OutSecondPieceCenterOffset = FVector::ZeroVector;
    if (OutSecondPieceRotationOffset)
        *OutSecondPieceRotationOffset = FRotator::ZeroRotator;

    const FLogGeometryVariantMesh* Match = nullptr;
    for (const FLogGeometryVariantMesh& Variant : GeometryVariantMeshes)
    {
        if (Variant.LengthFraction != InLength || Variant.CrossSectionProfile != InProfile)
            continue;

        // Two equally specific entries would make output selection non-deterministic.
        if (Match)
            return false;
        Match = &Variant;
    }

    if (Match)
    {
        if (!IsValid(Match->Mesh))
            return false;
        OutMesh = Match->Mesh;
        if (OutSecondPieceCenterOffset)
            *OutSecondPieceCenterOffset = Match->SecondPieceCenterOffsetLocal;
        if (OutSecondPieceRotationOffset)
            *OutSecondPieceRotationOffset = Match->SecondPieceRotationOffsetLocal;
        return true;
    }

    // A raw/base mesh can only stand in for the exact authored length it represents.
    if (InProfile == ELogCrossSectionProfile::Round &&
        InLength == BaseLogVariantLengthFraction && IsValid(BaseLogVariantMesh))
    {
        OutMesh = BaseLogVariantMesh;
        return true;
    }

    // The legacy per-profile properties only describe the original full-length
    // content path. Never silently reuse those assets for a shorter log.
    if (InLength != ELogLengthFraction::FullLength)
        return false;

    switch (InProfile)
    {
    case ELogCrossSectionProfile::Round:
        if (IsValid(BaseLogVariantMesh))
            OutMesh = BaseLogVariantMesh;
        else if (GetCrossSectionProfile() == ELogCrossSectionProfile::Round &&
            Mesh && IsValid(Mesh->GetStaticMesh()))
            OutMesh = Mesh->GetStaticMesh();
        break;
    case ELogCrossSectionProfile::HalfRound:
        OutMesh = SplitHalfVariantMesh;
        if (OutSecondPieceCenterOffset)
            *OutSecondPieceCenterOffset = SplitHalfPieceCenterOffsetLocal;
        if (OutSecondPieceRotationOffset)
            *OutSecondPieceRotationOffset = SplitHalfChildRotationOffsetLocal;
        break;
    case ELogCrossSectionProfile::QuarterRound:
        OutMesh = QuarterVariantMesh;
        if (OutSecondPieceCenterOffset)
            *OutSecondPieceCenterOffset = QuarterPieceCenterOffsetLocal;
        if (OutSecondPieceRotationOffset)
            *OutSecondPieceRotationOffset = QuarterChildRotationOffsetLocal;
        break;
    case ELogCrossSectionProfile::Hewn:
        OutMesh = HewnVariantMesh;
        break;
    case ELogCrossSectionProfile::SquareBeam:
        OutMesh = KalasVariantMesh;
        break;
    case ELogCrossSectionProfile::GroovedHalfRound:
        OutMesh = GutterVariantMesh;
        break;
    case ELogCrossSectionProfile::Trough:
        OutMesh = TroughVariantMesh;
        break;
    case ELogCrossSectionProfile::Plank:
        OutMesh = PlankVariantMesh;
        break;
    case ELogCrossSectionProfile::Notched:
    case ELogCrossSectionProfile::Joint:
    case ELogCrossSectionProfile::Other:
    default:
        return false;
    }

    return IsValid(OutMesh);
}

bool ALogBuildPiece::IsLengthCutAction(ELogCutAction Action) const
{
    return Action == ELogCutAction::SplitLengthHalf ||
        Action == ELogCutAction::SplitLengthThreeQuarter ||
        Action == ELogCutAction::SplitHalfLengthIntoQuarters;
}

bool ALogBuildPiece::IsValidLengthCutLayout(const FLogLengthCutVariant& Variant) const
{
    const FVector& First = Variant.FirstOutputCenterOffsetLocal;
    const FVector& Second = Variant.SecondOutputCenterOffsetLocal;
    const FRotator& FirstRotation = Variant.FirstOutputRotationOffsetLocal;
    const FRotator& SecondRotation = Variant.SecondOutputRotationOffsetLocal;
    if (!IsFiniteVector(First) || !IsFiniteVector(Second) ||
        !FMath::IsFinite(FirstRotation.Pitch) || !FMath::IsFinite(FirstRotation.Yaw) ||
        !FMath::IsFinite(FirstRotation.Roll) || !FMath::IsFinite(SecondRotation.Pitch) ||
        !FMath::IsFinite(SecondRotation.Yaw) || !FMath::IsFinite(SecondRotation.Roll))
    {
        return false;
    }

    const FVector Separation = Second - First;
    if (Separation.SizeSquared() <= 1.0f)
        return false;

    const FVector LongAxis = GetLocalLongitudinalAxis(LongitudinalAxis);
    const float LongitudinalAlignment =
        FMath::Abs(FVector::DotProduct(Separation.GetSafeNormal(), LongAxis));
    return LongitudinalAlignment >= 0.95f;
}

bool ALogBuildPiece::TryResolveLengthCutVariant(
    ELogCutAction Action,
    FLogLengthCutVariant& OutVariant) const
{
    if (!IsLengthCutAction(Action))
        return false;

    const ELogLengthFraction CurrentLength = GetLengthFraction();
    const ELogCrossSectionProfile CurrentProfile = GetCrossSectionProfile();
    const FLogLengthCutVariant* Match = nullptr;
    for (const FLogLengthCutVariant& Variant : LengthCutVariants)
    {
        if (Variant.Action != Action ||
            Variant.SourceLengthFraction != CurrentLength ||
            Variant.CrossSectionProfile != CurrentProfile)
        {
            continue;
        }
        // Duplicate exact recipes are ambiguous. Reject rather than picking the
        // first row and producing content-order-dependent geometry.
        if (Match)
            return false;
        Match = &Variant;
    }
    if (!Match)
        return false;

    bool bOutputLengthsValid = false;
    switch (Action)
    {
    case ELogCutAction::SplitLengthHalf:
        bOutputLengthsValid =
            CurrentLength == ELogLengthFraction::FullLength &&
            Match->FirstOutputLengthFraction == ELogLengthFraction::HalfLength &&
            Match->SecondOutputLengthFraction == ELogLengthFraction::HalfLength;
        break;
    case ELogCutAction::SplitLengthThreeQuarter:
        bOutputLengthsValid =
            CurrentLength == ELogLengthFraction::FullLength &&
            Match->FirstOutputLengthFraction == ELogLengthFraction::ThreeQuarterLength &&
            Match->SecondOutputLengthFraction == ELogLengthFraction::QuarterLength;
        break;
    case ELogCutAction::SplitHalfLengthIntoQuarters:
        bOutputLengthsValid =
            CurrentLength == ELogLengthFraction::HalfLength &&
            Match->FirstOutputLengthFraction == ELogLengthFraction::QuarterLength &&
            Match->SecondOutputLengthFraction == ELogLengthFraction::QuarterLength;
        break;
    default:
        return false;
    }
    if (!bOutputLengthsValid || !IsValidLengthCutLayout(*Match))
        return false;

    UStaticMesh* FirstMesh = nullptr;
    UStaticMesh* SecondMesh = nullptr;
    if (!TryResolveGeometryVariant(Match->FirstOutputLengthFraction, CurrentProfile, FirstMesh) ||
        !TryResolveGeometryVariant(Match->SecondOutputLengthFraction, CurrentProfile, SecondMesh) ||
        !IsValid(FirstMesh) || !IsValid(SecondMesh))
    {
        return false;
    }

    OutVariant = *Match;
    return true;
}

bool ALogBuildPiece::IsLengthCutAction(ELogCutAction Action) const
{
    return Action == ELogCutAction::SplitLengthHalf ||
        Action == ELogCutAction::SplitLengthThreeQuarter ||
        Action == ELogCutAction::SplitHalfLengthIntoQuarters;
}

bool ALogBuildPiece::IsValidLengthCutLayout(const FLogLengthCutVariant& Variant) const
{
    const FVector& First = Variant.FirstOutputCenterOffsetLocal;
    const FVector& Second = Variant.SecondOutputCenterOffsetLocal;
    const FRotator& FirstRotation = Variant.FirstOutputRotationOffsetLocal;
    const FRotator& SecondRotation = Variant.SecondOutputRotationOffsetLocal;
    if (!IsFiniteVector(First) || !IsFiniteVector(Second) ||
        !FMath::IsFinite(FirstRotation.Pitch) || !FMath::IsFinite(FirstRotation.Yaw) ||
        !FMath::IsFinite(FirstRotation.Roll) || !FMath::IsFinite(SecondRotation.Pitch) ||
        !FMath::IsFinite(SecondRotation.Yaw) || !FMath::IsFinite(SecondRotation.Roll))
    {
        return false;
    }

    const FVector Separation = Second - First;
    if (Separation.SizeSquared() <= 1.0f)
        return false;

    // The two output centers must follow the original log axis, not be guessed
    // side-by-side like two cross-section pieces.
    const FVector LongAxis = GetLocalLongitudinalAxis(LongitudinalAxis);
    const float LongitudinalAlignment =
        FMath::Abs(FVector::DotProduct(Separation.GetSafeNormal(), LongAxis));
    return LongitudinalAlignment >= 0.95f;
}

bool ALogBuildPiece::TryResolveLengthCutVariant(
    ELogCutAction Action,
    FLogLengthCutVariant& OutVariant) const
{
    if (!IsLengthCutAction(Action))
        return false;

    const ELogLengthFraction CurrentLength = GetLengthFraction();
    const ELogCrossSectionProfile CurrentProfile = GetCrossSectionProfile();
    const FLogLengthCutVariant* Match = nullptr;
    for (const FLogLengthCutVariant& Variant : LengthCutVariants)
    {
        if (Variant.Action != Action ||
            Variant.SourceLengthFraction != CurrentLength ||
            Variant.CrossSectionProfile != CurrentProfile)
        {
            continue;
        }

        // Duplicate exact recipes are ambiguous. Fail closed rather than make
        // content array order decide the physical result.
        if (Match)
            return false;
        Match = &Variant;
    }
    if (!Match)
        return false;

    bool bOutputLengthsValid = false;
    switch (Action)
    {
    case ELogCutAction::SplitLengthHalf:
        bOutputLengthsValid =
            CurrentLength == ELogLengthFraction::FullLength &&
            Match->FirstOutputLengthFraction == ELogLengthFraction::HalfLength &&
            Match->SecondOutputLengthFraction == ELogLengthFraction::HalfLength;
        break;
    case ELogCutAction::SplitLengthThreeQuarter:
        bOutputLengthsValid =
            CurrentLength == ELogLengthFraction::FullLength &&
            Match->FirstOutputLengthFraction == ELogLengthFraction::ThreeQuarterLength &&
            Match->SecondOutputLengthFraction == ELogLengthFraction::QuarterLength;
        break;
    case ELogCutAction::SplitHalfLengthIntoQuarters:
        bOutputLengthsValid =
            CurrentLength == ELogLengthFraction::HalfLength &&
            Match->FirstOutputLengthFraction == ELogLengthFraction::QuarterLength &&
            Match->SecondOutputLengthFraction == ELogLengthFraction::QuarterLength;
        break;
    default:
        return false;
    }
    if (!bOutputLengthsValid || !IsValidLengthCutLayout(*Match))
        return false;

    UStaticMesh* FirstMesh = nullptr;
    UStaticMesh* SecondMesh = nullptr;
    if (!TryResolveGeometryVariant(Match->FirstOutputLengthFraction, CurrentProfile, FirstMesh) ||
        !TryResolveGeometryVariant(Match->SecondOutputLengthFraction, CurrentProfile, SecondMesh) ||
        !IsValid(FirstMesh) || !IsValid(SecondMesh))
    {
        return false;
    }

    OutVariant = *Match;
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
        return false;

    if (IsLengthCutAction(Action))
    {
        FLogLengthCutVariant LengthCut;
        return TryResolveLengthCutVariant(Action, LengthCut);
    }

    UStaticMesh* OutputMesh = nullptr;
    FVector SplitOffset = FVector::ZeroVector;
    FRotator ChildRotation = FRotator::ZeroRotator;
    ELogCrossSectionProfile RequiredProfile = ELogCrossSectionProfile::Other;
    bool bNeedsSplitConfiguration = false;

    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::Round)
            return false;
        RequiredProfile = ELogCrossSectionProfile::HalfRound;
        bNeedsSplitConfiguration = true;
        break;
    case ELogCutAction::Quarter:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return false;
        RequiredProfile = ELogCrossSectionProfile::QuarterRound;
        bNeedsSplitConfiguration = true;
        break;
    case ELogCutAction::Hewn:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::Round)
            return false;
        RequiredProfile = ELogCrossSectionProfile::Hewn;
        break;
    case ELogCutAction::Gutter:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return false;
        RequiredProfile = ELogCrossSectionProfile::GroovedHalfRound;
        break;
    case ELogCutAction::Trough:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return false;
        RequiredProfile = ELogCrossSectionProfile::Trough;
        break;
    case ELogCutAction::Kalas:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::Round)
            return false;
        RequiredProfile = ELogCrossSectionProfile::SquareBeam;
        break;
    case ELogCutAction::Plank:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return false;
        RequiredProfile = ELogCrossSectionProfile::Plank;
        break;
    default:
        return false;
    }

    if (!TryResolveGeometryVariant(GetLengthFraction(), RequiredProfile,
        OutputMesh, &SplitOffset, &ChildRotation) || !IsValid(OutputMesh))
        return false;

    return !bNeedsSplitConfiguration ||
        IsValidCrossSectionCenterOffset(SplitOffset, LongitudinalAxis);
}

FString ALogBuildPiece::GetCutActionBlockReason(ELogCutAction Action) const
{
    if (bIsPlaced)
        return TEXT("Remove the log from the structure before processing it.");
    if (bIsHeld)
        return TEXT("Drop the log before processing it.");
    if (GetAttachedPieceCount() > 0)
        return TEXT("Remove attached pieces before processing the support log.");

    if (IsLengthCutAction(Action))
    {
        switch (Action)
        {
        case ELogCutAction::SplitLengthHalf:
        case ELogCutAction::SplitLengthThreeQuarter:
            if (GetLengthFraction() != ELogLengthFraction::FullLength)
                return TEXT("This length-cut action requires a full-length log.");
            break;
        case ELogCutAction::SplitHalfLengthIntoQuarters:
            if (GetLengthFraction() != ELogLengthFraction::HalfLength)
                return TEXT("Quarter-length cutting requires a half-length log first.");
            break;
        default:
            break;
        }

        FLogLengthCutVariant LengthCut;
        if (!TryResolveLengthCutVariant(Action, LengthCut))
        {
            return TEXT("No unique valid length-cut recipe and exact output meshes/positions are configured for this length and cross-section.");
        }
        return TEXT("The requested length cut is currently blocked.");
    }

    ELogCrossSectionProfile RequiredProfile = ELogCrossSectionProfile::Other;
    bool bNeedsSplitConfiguration = false;
    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::Round)
            return TEXT("Splitting into half-rounds requires a round cross-section.");
        RequiredProfile = ELogCrossSectionProfile::HalfRound;
        bNeedsSplitConfiguration = true;
        break;
    case ELogCutAction::Quarter:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return TEXT("Quarter-round shaping requires a half-round cross-section first.");
        RequiredProfile = ELogCrossSectionProfile::QuarterRound;
        bNeedsSplitConfiguration = true;
        break;
    case ELogCutAction::Gutter:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return TEXT("A groove can only be cut into a half-round cross-section.");
        RequiredProfile = ELogCrossSectionProfile::GroovedHalfRound;
        break;
    case ELogCutAction::Trough:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return TEXT("A trough requires a half-round cross-section first.");
        RequiredProfile = ELogCrossSectionProfile::Trough;
        break;
    case ELogCutAction::Hewn:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::Round)
            return TEXT("Hewing requires a round cross-section in this prototype.");
        RequiredProfile = ELogCrossSectionProfile::Hewn;
        break;
    case ELogCutAction::Kalas:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::Round)
            return TEXT("Squared-beam shaping requires a round cross-section.");
        RequiredProfile = ELogCrossSectionProfile::SquareBeam;
        break;
    case ELogCutAction::Plank:
        if (GetCrossSectionProfile() != ELogCrossSectionProfile::HalfRound)
            return TEXT("Plank shaping requires a half-round cross-section first.");
        RequiredProfile = ELogCrossSectionProfile::Plank;
        break;
    default:
        return TEXT("Unsupported processing action.");
    }

    UStaticMesh* OutputMesh = nullptr;
    FVector Offset = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    if (!TryResolveGeometryVariant(GetLengthFraction(), RequiredProfile,
        OutputMesh, &Offset, &Rotation) || !IsValid(OutputMesh))
    {
        return FString::Printf(
            TEXT("No mesh is configured for length '%s' and cross-section '%s'. Add an exact GeometryVariantMeshes entry; legacy mesh slots only cover full-length pieces."),
            *UEnum::GetValueAsString(GetLengthFraction()),
            *UEnum::GetValueAsString(RequiredProfile));
    }

    if (bNeedsSplitConfiguration &&
        !IsValidCrossSectionCenterOffset(Offset, LongitudinalAxis))
    {
        return TEXT("Configure a valid, non-zero cross-section center offset for this exact length/profile mesh variant.");
    }

    return TEXT("The requested processing action is currently blocked.");
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
    const ELogLengthFraction OutputLength = GetLengthFraction();
    const ELogCrossSectionProfile OutputProfile = CrossSectionFromLegacyType(OutputType);
    UStaticMesh* ResolvedOutputMesh = nullptr;
    FVector LocalCenterOffset = FVector::ZeroVector;
    FRotator ChildRotationOffset = FRotator::ZeroRotator;
    if (!TryResolveGeometryVariant(OutputLength, OutputProfile,
            ResolvedOutputMesh, &LocalCenterOffset, &ChildRotationOffset) ||
        ResolvedOutputMesh != OutputMesh ||
        !IsValidCrossSectionCenterOffset(LocalCenterOffset, LongitudinalAxis))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("Physical cross-section splitting requires an exact output mesh and a valid authored center offset.")));
    }

    const FTransform OriginalTransform = GetActorTransform();
    const FVector WorldCenterOffset = OriginalTransform.TransformVector(LocalCenterOffset);
    if (!IsFiniteVector(WorldCenterOffset) || WorldCenterOffset.SizeSquared() <= 1.0f)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("Configured cross-section offset collapses after applying actor scale.")));
    }

    const FVector FirstLocation = OriginalTransform.GetLocation() - WorldCenterOffset * 0.5f;
    const FVector SecondLocation = OriginalTransform.GetLocation() + WorldCenterOffset * 0.5f;
    const FQuat FirstRotation = OriginalTransform.GetRotation();
    const FQuat SecondRotation =
        OriginalTransform.GetRotation() * ChildRotationOffset.Quaternion();

    TArray<FName> OutputDefinitionIds;
    OutputDefinitionIds.Add(OutputDefinitionId);
    OutputDefinitionIds.Add(OutputDefinitionId);
    TArray<ELogLengthFraction> OutputLengths;
    OutputLengths.Add(OutputLength);
    OutputLengths.Add(OutputLength);
    TArray<ELogCrossSectionProfile> OutputProfiles;
    OutputProfiles.Add(OutputProfile);
    OutputProfiles.Add(OutputProfile);
    TArray<ELogPieceType> OutputLegacyTypes;
    OutputLegacyTypes.Add(LegacyTypeFromCanonicalDimensions(OutputLength, OutputProfile));
    OutputLegacyTypes.Add(LegacyTypeFromCanonicalDimensions(OutputLength, OutputProfile));
    TArray<UStaticMesh*> OutputMeshes;
    OutputMeshes.Add(OutputMesh);
    OutputMeshes.Add(OutputMesh);
    TArray<FTransform> OutputTransforms;
    OutputTransforms.Emplace(FirstRotation, FirstLocation, OriginalTransform.GetScale3D());
    OutputTransforms.Emplace(SecondRotation, SecondLocation, OriginalTransform.GetScale3D());

    const FText SuccessMessage = FText::FromString(
        Action == ELogCutAction::SplitHalf
            ? TEXT("Log split into two half-round physical pieces.")
            : TEXT("Split half-round into two quarter-section physical pieces."));
    return TryCommitPhysicalSplitOutputs(
        Action, OperationId, CurrentItem, OutputDefinitionIds, OutputLengths,
        OutputProfiles, OutputLegacyTypes, OutputMeshes, OutputTransforms, SuccessMessage);
}

FWorldOperationResult ALogBuildPiece::TrySplitLengthOutputPieces(
    ELogCutAction Action,
    const FGuid& OperationId,
    const FWorldOwnedItemRecord& CurrentItem)
{
    FLogLengthCutVariant Variant;
    if (!TryResolveLengthCutVariant(Action, Variant))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("No unique, valid longitudinal length-cut recipe is configured for this source length and cross-section.")));
    }

    UStaticMesh* FirstMesh = nullptr;
    UStaticMesh* SecondMesh = nullptr;
    if (!TryResolveGeometryVariant(Variant.FirstOutputLengthFraction,
            Variant.CrossSectionProfile, FirstMesh) ||
        !TryResolveGeometryVariant(Variant.SecondOutputLengthFraction,
            Variant.CrossSectionProfile, SecondMesh) ||
        !IsValid(FirstMesh) || !IsValid(SecondMesh))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("Both length-cut output meshes must be authored for the exact output lengths and current cross-section.")));
    }

    const FTransform OriginalTransform = GetActorTransform();
    const FVector FirstLocation =
        OriginalTransform.TransformPosition(Variant.FirstOutputCenterOffsetLocal);
    const FVector SecondLocation =
        OriginalTransform.TransformPosition(Variant.SecondOutputCenterOffsetLocal);
    const FQuat FirstRotation =
        OriginalTransform.GetRotation() * Variant.FirstOutputRotationOffsetLocal.Quaternion();
    const FQuat SecondRotation =
        OriginalTransform.GetRotation() * Variant.SecondOutputRotationOffsetLocal.Quaternion();

    const FName BaseDefinition = !CurrentItem.BaseDefinitionId.IsNone()
        ? CurrentItem.BaseDefinitionId
        : (!BaseDefinitionId.IsNone() ? BaseDefinitionId : CurrentItem.DefinitionId);
    const FName FirstDefinition = MakeCanonicalGeometryDefinitionId(
        BaseDefinition, Variant.FirstOutputLengthFraction, Variant.CrossSectionProfile);
    const FName SecondDefinition = MakeCanonicalGeometryDefinitionId(
        BaseDefinition, Variant.SecondOutputLengthFraction, Variant.CrossSectionProfile);
    if (FirstDefinition.IsNone() || SecondDefinition.IsNone())
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("The length-cut recipe cannot produce canonical output definitions.")));
    }

    TArray<FName> OutputDefinitionIds;
    OutputDefinitionIds.Add(FirstDefinition);
    OutputDefinitionIds.Add(SecondDefinition);
    TArray<ELogLengthFraction> OutputLengths;
    OutputLengths.Add(Variant.FirstOutputLengthFraction);
    OutputLengths.Add(Variant.SecondOutputLengthFraction);
    TArray<ELogCrossSectionProfile> OutputProfiles;
    OutputProfiles.Add(Variant.CrossSectionProfile);
    OutputProfiles.Add(Variant.CrossSectionProfile);
    TArray<ELogPieceType> OutputLegacyTypes;
    OutputLegacyTypes.Add(LegacyTypeFromCanonicalDimensions(
        Variant.FirstOutputLengthFraction, Variant.CrossSectionProfile));
    OutputLegacyTypes.Add(LegacyTypeFromCanonicalDimensions(
        Variant.SecondOutputLengthFraction, Variant.CrossSectionProfile));
    TArray<UStaticMesh*> OutputMeshes;
    OutputMeshes.Add(FirstMesh);
    OutputMeshes.Add(SecondMesh);
    TArray<FTransform> OutputTransforms;
    OutputTransforms.Emplace(FirstRotation, FirstLocation, OriginalTransform.GetScale3D());
    OutputTransforms.Emplace(SecondRotation, SecondLocation, OriginalTransform.GetScale3D());

    FText SuccessMessage;
    switch (Action)
    {
    case ELogCutAction::SplitLengthHalf:
        SuccessMessage = FText::FromString(TEXT("Full-length log cut into two half-length pieces."));
        break;
    case ELogCutAction::SplitLengthThreeQuarter:
        SuccessMessage = FText::FromString(TEXT("Full-length log cut into three-quarter and quarter-length pieces."));
        break;
    case ELogCutAction::SplitHalfLengthIntoQuarters:
        SuccessMessage = FText::FromString(TEXT("Half-length log cut into two quarter-length pieces."));
        break;
    default:
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Unsupported,
            OperationId,
            FText::FromString(TEXT("Unsupported longitudinal length-cut action.")));
    }

    return TryCommitPhysicalSplitOutputs(
        Action, OperationId, CurrentItem, OutputDefinitionIds, OutputLengths,
        OutputProfiles, OutputLegacyTypes, OutputMeshes, OutputTransforms, SuccessMessage);
}

FWorldOperationResult ALogBuildPiece::TryCommitPhysicalSplitOutputs(
    ELogCutAction Action,
    const FGuid& OperationId,
    const FWorldOwnedItemRecord& CurrentItem,
    const TArray<FName>& OutputDefinitionIds,
    const TArray<ELogLengthFraction>& OutputLengths,
    const TArray<ELogCrossSectionProfile>& OutputProfiles,
    const TArray<ELogPieceType>& OutputLegacyTypes,
    const TArray<UStaticMesh*>& OutputMeshes,
    const TArray<FTransform>& OutputTransforms,
    const FText& SuccessMessage)
{
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph =
        World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldObjectIdentityComponent* Identity =
        FindComponentByClass<UWorldObjectIdentityComponent>();

    const int32 NumOutputs = OutputDefinitionIds.Num();
    if (!World || !Ownership || !Graph || !Identity || !Mesh ||
        !CurrentItem.ItemId.IsValid() || CurrentItem.OwnerKind != EWorldItemOwnerKind::World ||
        CurrentItem.Quantity != 1 || CurrentItem.Revision <= 0 ||
        OutputDefinitionIds.Num() != 2 || OutputLengths.Num() != NumOutputs ||
        OutputProfiles.Num() != NumOutputs || OutputLegacyTypes.Num() != NumOutputs ||
        OutputMeshes.Num() != NumOutputs || OutputTransforms.Num() != NumOutputs ||
        OutputDefinitionIds[0].IsNone() || OutputDefinitionIds[1].IsNone() ||
        OutputDefinitionIds[0] == CurrentItem.DefinitionId ||
        !IsValid(OutputMeshes[0]) || !IsValid(OutputMeshes[1]))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("Physical split output configuration is incomplete or inconsistent.")));
    }

    for (int32 Index = 0; Index < NumOutputs; ++Index)
    {
        const FTransform& Transform = OutputTransforms[Index];
        const FVector Location = Transform.GetLocation();
        const FQuat Rotation = Transform.GetRotation();
        const FVector Scale = Transform.GetScale3D();
        if (OutputDefinitionIds[Index].IsNone() ||
            !IsFiniteVector(Location) ||
            !FMath::IsFinite(Rotation.X) || !FMath::IsFinite(Rotation.Y) ||
            !FMath::IsFinite(Rotation.Z) || !FMath::IsFinite(Rotation.W) ||
            !IsFiniteVector(Scale))
        {
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::MissingConfiguration,
                OperationId,
                FText::FromString(TEXT("A physical split output has an invalid definition or transform.")));
        }
    }
    if (OutputTransforms[0].GetLocation().Equals(OutputTransforms[1].GetLocation(), 0.1f))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("The two physical output centers must be distinct.")));
    }

    // Construct the second actor in deferred state. Its authoritative item ID
    // is assigned before BeginPlay can register it with ownership or the graph.
    ALogBuildPiece* Child = World->SpawnActorDeferred<ALogBuildPiece>(
        GetClass(), OutputTransforms[1], nullptr, nullptr,
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

    Child->PieceType = OutputLegacyTypes[1];
    Child->BaseLogVariantMesh = BaseLogVariantMesh;
    Child->BaseLogVariantLengthFraction = BaseLogVariantLengthFraction;
    Child->GeometryVariantMeshes = GeometryVariantMeshes;
    Child->LengthCutVariants = LengthCutVariants;
    Child->LengthFraction = OutputLengths[1];
    Child->CrossSectionProfile = OutputProfiles[1];
    Child->bUseCanonicalGeometryDimensions = true;
    Child->BaseDefinitionId = CurrentItem.BaseDefinitionId.IsNone()
        ? CurrentItem.DefinitionId : CurrentItem.BaseDefinitionId;
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
    Child->SetPieceStaticMesh(OutputMeshes[1]);
    Child->SetActorEnableCollision(false);
    if (Child->Mesh)
    {
        Child->Mesh->SetSimulatePhysics(false);
        Child->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    TArray<FWorldOwnedItemRecord> BeforeItems;
    TArray<FWorldItemTransferReceipt> BeforeTransferReceipts;
    TArray<FWorldItemSplitReceipt> BeforeSplitReceipts;
    Ownership->ExportSnapshot(BeforeItems, BeforeTransferReceipts, BeforeSplitReceipts);

    TArray<FVector> OutputLocations;
    OutputLocations.Reserve(NumOutputs);
    for (const FTransform& Transform : OutputTransforms)
        OutputLocations.Add(Transform.GetLocation());

    TArray<FWorldOwnedItemRecord> OutputRecords;
    const bool bSplitCommitted = Ownership->TrySplitItem(OperationId, CurrentItem.ItemId,
        CurrentItem.Revision, EWorldItemOwnerKind::World,
        OutputDefinitionIds, OutputLocations, OutputRecords);
    if (!bSplitCommitted)
    {
        Child->Destroy();
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Conflict,
            OperationId,
            FText::FromString(TEXT("Ownership ledger rejected the atomic multi-output split request.")));
    }
    if (OutputRecords.Num() != NumOutputs)
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
    const ELogLengthFraction OldLengthFraction = LengthFraction;
    const ELogCrossSectionProfile OldCrossSectionProfile = CrossSectionProfile;
    const bool bOldUseCanonicalGeometryDimensions = bUseCanonicalGeometryDimensions;
    const FName OldBaseDefinition = BaseDefinitionId;
    const FName OldDefinition = Identity->GetDefinitionId();
    const TArray<FLogSnapPoint> OldSnapPoints = SnapPoints;
    const bool bOldSimulatesPhysics = Mesh->IsSimulatingPhysics();
    const ECollisionEnabled::Type OldCollisionEnabled = Mesh->GetCollisionEnabled();
    const FName OldCollisionProfile = Mesh->GetCollisionProfileName();
    const bool bOldActorCollisionEnabled = GetActorEnableCollision();
    const bool bOldHidden = IsHidden();
    const FTransform OldTransform = GetActorTransform();

    auto RollbackPhysicalSplit = [&]()
    {
        if (IsValid(Child))
            Child->Destroy();

        const bool bOwnershipRestored =
            Ownership->RestoreSnapshot(BeforeItems, BeforeTransferReceipts, BeforeSplitReceipts);
        SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
        SetPieceStaticMesh(const_cast<UStaticMesh*>(OldMesh));
        PieceType = OldType;
        LengthFraction = OldLengthFraction;
        CrossSectionProfile = OldCrossSectionProfile;
        bUseCanonicalGeometryDimensions = bOldUseCanonicalGeometryDimensions;
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
                TEXT("Physical split rollback failed for operation %s; stop world simulation and recover the world."),
                *OperationId.ToString());
        }
    };

    SetActorTransform(OutputTransforms[0], false, nullptr, ETeleportType::TeleportPhysics);
    SetPieceStaticMesh(OutputMeshes[0]);
    PieceType = OutputLegacyTypes[0];
    LengthFraction = OutputLengths[0];
    CrossSectionProfile = OutputProfiles[0];
    bUseCanonicalGeometryDimensions = true;
    BaseDefinitionId = CurrentItem.BaseDefinitionId.IsNone()
        ? CurrentItem.DefinitionId : CurrentItem.BaseDefinitionId;
    Identity->SetDefinitionId(OutputDefinitionIds[0]);
    if (Mesh)
    {
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
        Mesh->UpdateBounds();
    }

    Child->StructuralIdentity->InstanceId = OutputRecords[1].ItemId;
    Child->StructuralIdentity->SetDefinitionId(OutputRecords[1].DefinitionId);
    Child->FinishSpawning(OutputTransforms[1]);

    FWorldOwnedItemRecord ChildItem;
    FStructuralNodeRecord ChildNode;
    FWorldOwnedItemRecord SourceAfterSplit;
    FStructuralNodeRecord SourceNode;
    const bool bChildRegistered =
        Ownership->TryGetItem(OutputRecords[1].ItemId, ChildItem) &&
        ChildItem.OwnerKind == EWorldItemOwnerKind::World &&
        ChildItem.DefinitionId == OutputDefinitionIds[1] &&
        Graph->GetNode(OutputRecords[1].ItemId, ChildNode) &&
        ChildNode.Actor.Get() == Child;
    const bool bSourceCommitted =
        Ownership->TryGetItem(CurrentItem.ItemId, SourceAfterSplit) &&
        SourceAfterSplit.OwnerKind == EWorldItemOwnerKind::World &&
        SourceAfterSplit.DefinitionId == OutputDefinitionIds[0] &&
        SourceAfterSplit.Revision == CurrentItem.Revision + 1 &&
        Graph->GetNode(CurrentItem.ItemId, SourceNode) &&
        SourceNode.Actor.Get() == this;
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
    if (UWorldCommittedEventSubsystem* Events =
        World->GetSubsystem<UWorldCommittedEventSubsystem>())
    {
        FWorldCommittedEvent Event;
        Event.Type = EWorldCommittedEventType::ObjectProcessed;
        Event.OperationId = OperationId;
        Event.AffectedObjectIds = AffectedIds;
        Event.Message = SuccessMessage;
        if (!Events->PublishCommittedEvent(Event))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Physical split committed for operation %s but its event was not published."),
                *OperationId.ToString());
        }
    }

    return FWorldOperationResult::Success(OperationId, SuccessMessage, AffectedIds);
}

FWorldOperationResult ALogBuildPiece::TryApplyCutAction(
    ELogCutAction Action,
    FGuid OperationId)
{
    SyncCanonicalGeometryFromLegacyPieceType();
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }

    // Split receipts cover both cross-section splits and longitudinal length cuts.
    // Handle them before checking the now-changed source prerequisites so a retry
    // returns the original two IDs and can never spawn another child actor.
    UWorldObjectIdentityComponent* SplitRetryIdentity =
        FindComponentByClass<UWorldObjectIdentityComponent>();
    UWorldItemOwnershipSubsystem* SplitRetryOwnership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    TArray<FWorldOwnedItemRecord> ExistingSplitRecords;
    if (SplitRetryOwnership &&
        SplitRetryOwnership->TryGetSplitResult(OperationId, ExistingSplitRecords))
    {
        FWorldOwnedItemRecord CurrentSplitSource;
        const bool bCurrentSourceMatches =
            SplitRetryIdentity &&
            SplitRetryOwnership->TryGetItem(SplitRetryIdentity->GetInstanceId(), CurrentSplitSource) &&
            ExistingSplitRecords.Num() == 2 &&
            CurrentSplitSource.ItemId == ExistingSplitRecords[0].ItemId &&
            CurrentSplitSource.OwnerKind == EWorldItemOwnerKind::World &&
            CurrentSplitSource.Revision == ExistingSplitRecords[0].Revision &&
            CurrentSplitSource.DefinitionId == ExistingSplitRecords[0].DefinitionId &&
            CurrentSplitSource.Revision > 1;
        if (!bCurrentSourceMatches)
        {
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::Conflict,
                OperationId,
                FText::FromString(TEXT("This operation ID already belongs to a different split result.")));
        }

        const FName RetryBase = !CurrentSplitSource.BaseDefinitionId.IsNone()
            ? CurrentSplitSource.BaseDefinitionId
            : (!BaseDefinitionId.IsNone() ? BaseDefinitionId : CurrentSplitSource.DefinitionId);
        ELogLengthFraction FirstLength = ELogLengthFraction::Other;
        ELogLengthFraction SecondLength = ELogLengthFraction::Other;
        ELogCrossSectionProfile FirstProfile = ELogCrossSectionProfile::Other;
        ELogCrossSectionProfile SecondProfile = ELogCrossSectionProfile::Other;
        bool bKnownSplitAction = true;

        switch (Action)
        {
        case ELogCutAction::SplitHalf:
            FirstLength = GetLengthFraction();
            SecondLength = GetLengthFraction();
            FirstProfile = ELogCrossSectionProfile::HalfRound;
            SecondProfile = ELogCrossSectionProfile::HalfRound;
            break;
        case ELogCutAction::Quarter:
            FirstLength = GetLengthFraction();
            SecondLength = GetLengthFraction();
            FirstProfile = ELogCrossSectionProfile::QuarterRound;
            SecondProfile = ELogCrossSectionProfile::QuarterRound;
            break;
        case ELogCutAction::SplitLengthHalf:
            FirstLength = ELogLengthFraction::HalfLength;
            SecondLength = ELogLengthFraction::HalfLength;
            FirstProfile = GetCrossSectionProfile();
            SecondProfile = GetCrossSectionProfile();
            break;
        case ELogCutAction::SplitLengthThreeQuarter:
            FirstLength = ELogLengthFraction::ThreeQuarterLength;
            SecondLength = ELogLengthFraction::QuarterLength;
            FirstProfile = GetCrossSectionProfile();
            SecondProfile = GetCrossSectionProfile();
            break;
        case ELogCutAction::SplitHalfLengthIntoQuarters:
            FirstLength = ELogLengthFraction::QuarterLength;
            SecondLength = ELogLengthFraction::QuarterLength;
            FirstProfile = GetCrossSectionProfile();
            SecondProfile = GetCrossSectionProfile();
            break;
        default:
            bKnownSplitAction = false;
            break;
        }

        const FName FirstDefinition = bKnownSplitAction
            ? MakeCanonicalGeometryDefinitionId(RetryBase, FirstLength, FirstProfile)
            : NAME_None;
        const FName SecondDefinition = bKnownSplitAction
            ? MakeCanonicalGeometryDefinitionId(RetryBase, SecondLength, SecondProfile)
            : NAME_None;
        if (!bKnownSplitAction || FirstDefinition.IsNone() || SecondDefinition.IsNone() ||
            GetLengthFraction() != FirstLength || GetCrossSectionProfile() != FirstProfile ||
            ExistingSplitRecords[0].DefinitionId != FirstDefinition ||
            ExistingSplitRecords[1].DefinitionId != SecondDefinition)
        {
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::Conflict,
                OperationId,
                FText::FromString(TEXT("This operation ID was already committed with a different cut action or output signature.")));
        }

        TArray<FName> RetryDefinitions;
        RetryDefinitions.Add(FirstDefinition);
        RetryDefinitions.Add(SecondDefinition);
        TArray<FVector> RetryLocations;
        RetryLocations.Add(ExistingSplitRecords[0].WorldLocation);
        RetryLocations.Add(ExistingSplitRecords[1].WorldLocation);
        TArray<FWorldOwnedItemRecord> RetriedRecords;
        if (!SplitRetryOwnership->TrySplitItem(OperationId, CurrentSplitSource.ItemId,
            CurrentSplitSource.Revision - 1, EWorldItemOwnerKind::World,
            RetryDefinitions, RetryLocations, RetriedRecords) || RetriedRecords.Num() != 2)
        {
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::Conflict,
                OperationId,
                FText::FromString(TEXT("The saved split receipt did not match the retried cut request.")));
        }

        TArray<FGuid> RetriedIds;
        RetriedIds.Add(RetriedRecords[0].ItemId);
        RetriedIds.Add(RetriedRecords[1].ItemId);
        return FWorldOperationResult::Success(
            OperationId,
            FText::FromString(TEXT("The previous physical split result was returned.")),
            RetriedIds);
    }

    // A retry of a legacy single-piece processing operation must return its
    // committed result even though the first attempt changed the prerequisites.
    // though the first attempt has already changed PieceType and the prerequisites.
    ELogPieceType RetryTargetType = ELogPieceType::Other;
    const TCHAR* RetrySuffix = TEXT("Other");
    bool bRetryTargetSupported = true;
    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        RetryTargetType = ELogPieceType::SplitHalfLog; RetrySuffix = TEXT("SplitHalfLog"); break;
    case ELogCutAction::Quarter:
        RetryTargetType = ELogPieceType::QuarterSectionLog;
        RetrySuffix = TEXT("QuarterSectionLog");
        break;
    case ELogCutAction::Hewn:
        RetryTargetType = ELogPieceType::HewnLog; RetrySuffix = TEXT("HewnLog"); break;
    case ELogCutAction::Gutter:
        RetryTargetType = ELogPieceType::GutterLog; RetrySuffix = TEXT("GutterLog"); break;
    case ELogCutAction::Trough:
        RetryTargetType = ELogPieceType::TroughLog; RetrySuffix = TEXT("TroughLog"); break;
    case ELogCutAction::Kalas:
        RetryTargetType = ELogPieceType::SquaredBeam; RetrySuffix = TEXT("SquaredBeam"); break;
    case ELogCutAction::Plank:
        RetryTargetType = ELogPieceType::Plank; RetrySuffix = TEXT("Plank"); break;
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
        const ELogCrossSectionProfile RetryProfile = CrossSectionFromLegacyType(RetryTargetType);
        const FName RetryDefinition = MakeCanonicalGeometryDefinitionId(
            RetryBase, GetLengthFraction(), RetryProfile);
        const FName LegacyActionDefinition(*FString::Printf(
            TEXT("%s.%s"), *RetryBase.ToString(), RetrySuffix));
        const FName LegacyQuarterDefinition(*FString::Printf(
            TEXT("%s.QuarterLog"), *RetryBase.ToString()));
        const bool bCanonicalRetryMatch =
            RetryCurrent.DefinitionId == RetryDefinition && PieceType == RetryTargetType;
        const bool bLegacyActionRetryMatch =
            RetryCurrent.DefinitionId == LegacyActionDefinition && PieceType == RetryTargetType;
        // Pre-migration cut operations stored ".QuarterLog". Accept that receipt
        // only when the loaded actor has already resolved the legacy suffix to
        // the explicit QuarterSectionLog type.
        const bool bLegacyQuarterRetryMatch =
            Action == ELogCutAction::Quarter &&
            RetryCurrent.DefinitionId == LegacyQuarterDefinition &&
            PieceType == ELogPieceType::QuarterSectionLog;
        const FName RequestedRetryDefinition =
            bLegacyQuarterRetryMatch ? LegacyQuarterDefinition :
            (bLegacyActionRetryMatch ? LegacyActionDefinition : RetryDefinition);
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

        if (bCanonicalRetryMatch || bLegacyActionRetryMatch || bLegacyQuarterRetryMatch)
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
                bPrerequisiteMet = GetCrossSectionProfile() == ELogCrossSectionProfile::Round;
                break;

            case ELogCutAction::Quarter:
            case ELogCutAction::Gutter:
            case ELogCutAction::Trough:
            case ELogCutAction::Plank:
                bPrerequisiteMet = GetCrossSectionProfile() == ELogCrossSectionProfile::HalfRound;
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

    // Resolve the exact authored output for this length/profile pair. A legacy
    // single-slot mesh is never silently reused for a shorter log.
    FVector OutputSplitOffset = FVector::ZeroVector;
    FRotator OutputChildRotation = FRotator::ZeroRotator;
    if (!TryResolveGeometryVariant(GetLengthFraction(),
            CrossSectionFromLegacyType(NewType), NewMesh,
            &OutputSplitOffset, &OutputChildRotation) ||
        !IsValid(NewMesh) || !Mesh)
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
    const FName NewDefinitionId = MakeCanonicalGeometryDefinitionId(
        EffectiveBaseDefinition, GetLengthFraction(), CrossSectionFromLegacyType(NewType));

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
    SetCanonicalCrossSectionFromPieceType(NewType);

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

    bUseCanonicalGeometryDimensions = false;
    SyncCanonicalGeometryFromLegacyPieceType();
}

bool ALogBuildPiece::ApplyDefinitionVariant(FName CurrentDefinitionId)
{
    SyncCanonicalGeometryFromLegacyPieceType();
    if (CurrentDefinitionId.IsNone() || BaseDefinitionId.IsNone() || !Mesh)
        return false;

    const FTransform OriginalTransform = GetActorTransform();
    FVector OldOrigin;
    FVector OldExtent;
    GetActorBounds(false, OldOrigin, OldExtent);
    const float OldBottomZ = OldOrigin.Z - OldExtent.Z;

    auto ApplyResolvedVariant = [this, &OriginalTransform, OldBottomZ, &CurrentDefinitionId](
        ELogLengthFraction InLength, ELogCrossSectionProfile InProfile,
        UStaticMesh* InMesh) -> bool
    {
        if (!IsValid(InMesh))
            return false;

        SetActorTransform(OriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);
        SetPieceStaticMesh(InMesh);
        LengthFraction = InLength;
        CrossSectionProfile = InProfile;
        bUseCanonicalGeometryDimensions = true;
        PieceType = LegacyTypeFromCanonicalDimensions(InLength, InProfile);

        FVector NewOrigin;
        FVector NewExtent;
        GetActorBounds(false, NewOrigin, NewExtent);
        AddActorWorldOffset(FVector(0.0f, 0.0f,
            OldBottomZ - (NewOrigin.Z - NewExtent.Z)), false);
        if (Mesh)
            Mesh->UpdateBounds();

        if (UWorldObjectIdentityComponent* Identity =
            FindComponentByClass<UWorldObjectIdentityComponent>())
        {
            Identity->SetDefinitionId(CurrentDefinitionId);
        }
        return true;
    };

    // An unprocessed item restores the authored base mesh for this actor's length.
    if (CurrentDefinitionId == BaseDefinitionId)
    {
        UStaticMesh* BaseMesh = nullptr;
        FVector IgnoredOffset = FVector::ZeroVector;
        FRotator IgnoredRotation = FRotator::ZeroRotator;
        if (!TryResolveGeometryVariant(GetLengthFraction(),
            ELogCrossSectionProfile::Round, BaseMesh, &IgnoredOffset, &IgnoredRotation))
        {
            return false;
        }
        return ApplyResolvedVariant(GetLengthFraction(), ELogCrossSectionProfile::Round, BaseMesh);
    }

    const FString BaseText = BaseDefinitionId.ToString();
    const FString CurrentText = CurrentDefinitionId.ToString();
    const FString Prefix = BaseText + TEXT(".");
    if (!CurrentText.StartsWith(Prefix, ESearchCase::IgnoreCase))
        return false;

    const FString Suffix = CurrentText.RightChop(Prefix.Len());

    // Canonical definitions carry both orthogonal dimensions, e.g.
    // Base.Geom.Half.HalfRound. Resolve the exact mesh before mutating the actor.
    ELogLengthFraction ParsedLength = ELogLengthFraction::FullLength;
    ELogCrossSectionProfile ParsedProfile = ELogCrossSectionProfile::Other;
    if (TryParseCanonicalGeometrySuffix(Suffix, ParsedLength, ParsedProfile))
    {
        UStaticMesh* CanonicalMesh = nullptr;
        FVector IgnoredOffset = FVector::ZeroVector;
        FRotator IgnoredRotation = FRotator::ZeroRotator;
        if (!TryResolveGeometryVariant(ParsedLength, ParsedProfile,
            CanonicalMesh, &IgnoredOffset, &IgnoredRotation))
        {
            return false;
        }
        return ApplyResolvedVariant(ParsedLength, ParsedProfile, CanonicalMesh);
    }

    // Backwards compatibility for pre-canonical single-suffix item definitions.
    // Shortened legacy pieces require an exact GeometryVariantMeshes entry; old
    // full-length slots are deliberately not applied to shorter pieces.
    ELogCrossSectionProfile LegacyProfile = ELogCrossSectionProfile::Other;
    if (Suffix.Equals(TEXT("SplitHalfLog"), ESearchCase::IgnoreCase))
        LegacyProfile = ELogCrossSectionProfile::HalfRound;
    else if (Suffix.Equals(TEXT("QuarterSectionLog"), ESearchCase::IgnoreCase) ||
        Suffix.Equals(TEXT("QuarterLog"), ESearchCase::IgnoreCase))
        LegacyProfile = ELogCrossSectionProfile::QuarterRound;
    else if (Suffix.Equals(TEXT("HewnLog"), ESearchCase::IgnoreCase))
        LegacyProfile = ELogCrossSectionProfile::Hewn;
    else if (Suffix.Equals(TEXT("GutterLog"), ESearchCase::IgnoreCase))
        LegacyProfile = ELogCrossSectionProfile::GroovedHalfRound;
    else if (Suffix.Equals(TEXT("TroughLog"), ESearchCase::IgnoreCase))
        LegacyProfile = ELogCrossSectionProfile::Trough;
    else if (Suffix.Equals(TEXT("SquaredBeam"), ESearchCase::IgnoreCase))
        LegacyProfile = ELogCrossSectionProfile::SquareBeam;
    else if (Suffix.Equals(TEXT("Plank"), ESearchCase::IgnoreCase))
        LegacyProfile = ELogCrossSectionProfile::Plank;
    else
        return false;

    UStaticMesh* LegacyMesh = nullptr;
    FVector IgnoredOffset = FVector::ZeroVector;
    FRotator IgnoredRotation = FRotator::ZeroRotator;
    const ELogLengthFraction LegacyLength = GetLengthFraction();
    if (!TryResolveGeometryVariant(LegacyLength, LegacyProfile,
        LegacyMesh, &IgnoredOffset, &IgnoredRotation))
    {
        return false;
    }
    return ApplyResolvedVariant(LegacyLength, LegacyProfile, LegacyMesh);
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
