#include "StoneBuildingPiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AStoneBuildingPiece::AStoneBuildingPiece()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(false);
}

void AStoneBuildingPiece::BeginPlay()
{
    Super::BeginPlay();

    bIsPlaced = bStartsPlaced;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsPlaced && !bIsHeld);
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

void AStoneBuildingPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveConnection();

    // Child::RemoveConnection mutates this actor's AttachedPieces array.
    // Iterate a snapshot so teardown cannot invalidate the active iterator.
    const TArray<TWeakObjectPtr<AStoneBuildingPiece>> ChildrenToDetach = AttachedPieces;
    AttachedPieces.Reset();
    for (const TWeakObjectPtr<AStoneBuildingPiece>& Child : ChildrenToDetach)
    {
        if (Child.IsValid())
        {
            Child->RemoveConnection();
        }
    }

    Super::EndPlay(EndPlayReason);
}

int32 AStoneBuildingPiece::FindAnchorIndex(FName AnchorId) const
{
    if (AnchorId.IsNone())
    {
        return INDEX_NONE;
    }

    return Anchors.IndexOfByPredicate([AnchorId](const FStoneSnapAnchor& Anchor)
    {
        return Anchor.AnchorId == AnchorId;
    });
}

bool AStoneBuildingPiece::HasOccupiedAnchors() const
{
    return Anchors.ContainsByPredicate(
        [](const FStoneSnapAnchor& Anchor)
        {
            return Anchor.bOccupied;
        });
}

bool AStoneBuildingPiece::HasWellFormedAnchors() const
{
    TSet<FName> SeenIds;
    for (const FStoneSnapAnchor& Anchor : Anchors)
    {
        if (Anchor.AnchorId.IsNone() || Anchor.CompatibilityTag.IsNone() ||
            !Anchor.LocalTransform.IsValid() || SeenIds.Contains(Anchor.AnchorId))
        {
            return false;
        }
        SeenIds.Add(Anchor.AnchorId);
    }
    return true;
}

FTransform AStoneBuildingPiece::GetAnchorWorldTransform(FName AnchorId) const
{
    const int32 Index = FindAnchorIndex(AnchorId);
    return Anchors.IsValidIndex(Index)
        ? Anchors[Index].LocalTransform * GetActorTransform()
        : GetActorTransform();
}

bool AStoneBuildingPiece::AddCustomAnchor(
    FName AnchorId,
    FName CompatibilityTag,
    FTransform LocalTransform,
    bool bCanReceive,
    bool bCanConnect)
{
    if (AnchorId.IsNone() || CompatibilityTag.IsNone() ||
        FindAnchorIndex(AnchorId) != INDEX_NONE ||
        !LocalTransform.IsValid())
    {
        return false;
    }

    FStoneSnapAnchor Anchor;
    Anchor.AnchorId = AnchorId;
    Anchor.CompatibilityTag = CompatibilityTag;
    Anchor.LocalTransform = LocalTransform;
    Anchor.bCanReceive = bCanReceive;
    Anchor.bCanConnect = bCanConnect;
    Anchor.bOccupied = false;
    Anchors.Add(Anchor);
    return true;
}

bool AStoneBuildingPiece::SetHeldState(bool bNewHeld)
{
    if (bNewHeld)
    {
        if (!bCanBeCarried || bIsHeld || bIsPlaced ||
            ParentPiece.IsValid() || AttachedPieces.Num() > 0 || HasOccupiedAnchors())
        {
            return false;
        }

        bIsHeld = true;
    }
    else
    {
        if (!bIsHeld)
        {
            return true;
        }
        bIsHeld = false;
    }

    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsHeld && !bIsPlaced);
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
    }
    return true;
}

FWorldOperationResult AStoneBuildingPiece::TryThrowStone(
    const FVector& ThrowImpulse,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }

    if (!bIsHeld || bIsPlaced || ParentPiece.IsValid() || !Mesh)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidState,
            OperationId,
            FText::FromString(TEXT("Only a held, loose stone can be thrown.")));
    }

    if (!ThrowImpulse.IsFinite() || ThrowImpulse.IsNearlyZero())
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidTarget,
            OperationId,
            FText::FromString(TEXT("Throw impulse must be finite and non-zero.")));
    }

    bIsHeld = false;
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetSimulatePhysics(true);
    Mesh->AddImpulse(ThrowImpulse, NAME_None, true);

    return FWorldOperationResult::Success(
        OperationId,
        FText::FromString(TEXT("Stone thrown.")),
        TArray<FGuid>());
}

UStaticMesh* AStoneBuildingPiece::ResolveShapeOutputMesh(
    EStoneShapeAction Action,
    bool& bOutAmbiguousMapping) const
{
    bOutAmbiguousMapping = false;
    UStaticMesh* SourceMesh = Mesh ? Mesh->GetStaticMesh() : nullptr;

    if (ProcessingMeshSets.Num() == 0 || !IsValid(SourceMesh))
    {
        return nullptr;
    }

    const FStoneProcessingMeshSet* MatchedSet = nullptr;
    for (const FStoneProcessingMeshSet& Set : ProcessingMeshSets)
    {
        if (!IsValid(Set.SourceMesh) || Set.SourceMesh != SourceMesh)
        {
            continue;
        }

        if (MatchedSet)
        {
            bOutAmbiguousMapping = true;
            return nullptr;
        }
        MatchedSet = &Set;
    }

    if (!MatchedSet)
    {
        return nullptr;
    }

    switch (Action)
    {
    case EStoneShapeAction::Block: return MatchedSet->BlockMesh;
    case EStoneShapeAction::Slab: return MatchedSet->SlabMesh;
    case EStoneShapeAction::Column: return MatchedSet->ColumnMesh;
    case EStoneShapeAction::Lintel: return MatchedSet->LintelMesh;
    case EStoneShapeAction::Corner: return MatchedSet->CornerMesh;
    default: return nullptr;
    }
}

bool AStoneBuildingPiece::CanShapeStone(EStoneShapeAction Action) const
{
    if (!Mesh || bIsHeld || bIsPlaced || ParentPiece.IsValid() ||
        AttachedPieces.Num() > 0 || HasOccupiedAnchors() ||
        PieceType != EStonePieceType::RawStone)
    {
        return false;
    }

    bool bAmbiguousMapping = false;
    return IsValid(ResolveShapeOutputMesh(Action, bAmbiguousMapping)) &&
        !bAmbiguousMapping;
}

FWorldOperationResult AStoneBuildingPiece::TryShapeStone(
    EStoneShapeAction Action,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }

    if (!Mesh || !IsValid(Mesh->GetStaticMesh()))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("A source stone mesh must be configured before shaping.")));
    }

    if (bIsHeld || bIsPlaced || ParentPiece.IsValid() ||
        AttachedPieces.Num() > 0 || HasOccupiedAnchors())
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidState,
            OperationId,
            FText::FromString(TEXT("Release, unplace and disconnect the stone before shaping it.")));
    }

    if (PieceType != EStonePieceType::RawStone)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::PrerequisiteNotMet,
            OperationId,
            FText::FromString(TEXT("Only raw stone can be shaped by this operation.")));
    }

    EStonePieceType NewType = EStonePieceType::Other;
    switch (Action)
    {
    case EStoneShapeAction::Block: NewType = EStonePieceType::Block; break;
    case EStoneShapeAction::Slab: NewType = EStonePieceType::Slab; break;
    case EStoneShapeAction::Column: NewType = EStonePieceType::Column; break;
    case EStoneShapeAction::Lintel: NewType = EStonePieceType::Lintel; break;
    case EStoneShapeAction::Corner: NewType = EStonePieceType::Corner; break;
    default:
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Unsupported,
            OperationId,
            FText::FromString(TEXT("Unsupported stone shaping action.")));
    }

    bool bAmbiguousMapping = false;
    UStaticMesh* NewMesh = ResolveShapeOutputMesh(Action, bAmbiguousMapping);
    if (bAmbiguousMapping)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Conflict,
            OperationId,
            FText::FromString(TEXT("Multiple stone processing sets match this source mesh.")));
    }

    if (!IsValid(NewMesh))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration,
            OperationId,
            FText::FromString(TEXT("No output mesh is configured for this stone source and shape action.")));
    }

    UStaticMesh* PreviousMesh = Mesh->GetStaticMesh();
    const FBox OldBounds = Mesh->Bounds.GetBox();
    const double OldBottomZ = OldBounds.Min.Z;
    Mesh->SetStaticMesh(NewMesh);
    Mesh->UpdateBounds();
    const FBox NewBounds = Mesh->Bounds.GetBox();
    if (OldBounds.IsValid && NewBounds.IsValid)
    {
        FVector NewLocation = GetActorLocation();
        NewLocation.Z += OldBottomZ - NewBounds.Min.Z;
        if (!SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics))
        {
            Mesh->SetStaticMesh(PreviousMesh);
            Mesh->UpdateBounds();
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::InternalError,
                OperationId,
                FText::FromString(TEXT("Stone shaping was rolled back because bottom alignment failed.")));
        }
    }
    PieceType = NewType;

    return FWorldOperationResult::Success(
        OperationId,
        FText::FromString(TEXT("Stone shaping completed.")),
        TArray<FGuid>());
}

bool AStoneBuildingPiece::TryCommitGroundPlacement(
    const FVector& SurfaceLocation,
    const FRotator& SurfaceRotation)
{
    if (bIsPlaced || !bIsHeld || ParentPiece.IsValid() ||
        !SurfaceLocation.IsFinite() || SurfaceRotation.ContainsNaN())
    {
        return false;
    }

    SetActorLocationAndRotation(SurfaceLocation, SurfaceRotation, false, nullptr,
        ETeleportType::TeleportPhysics);
    bIsHeld = false;
    bIsPlaced = true;

    if (Mesh)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    return true;
}

bool AStoneBuildingPiece::HasValidSupportChain() const
{
    if (bIsPlaced)
    {
        return true;
    }

    const AStoneBuildingPiece* Current = this;
    TSet<const AStoneBuildingPiece*> Visited;

    while (Current)
    {
        if (Visited.Contains(Current))
        {
            return false;
        }
        Visited.Add(Current);

        if (Current->bIsPlaced)
        {
            return true;
        }

        Current = Current->ParentPiece.Get();
    }

    return false;
}

bool AStoneBuildingPiece::CanConnectToAnchor(
    const AStoneBuildingPiece* TargetPiece,
    FName TargetAnchorId,
    FName OwnAnchorId) const
{
    if (!IsValid(TargetPiece) || TargetPiece == this ||
        bIsPlaced || !bIsHeld ||
        ParentPiece.IsValid() || !HasWellFormedAnchors() ||
        !TargetPiece->HasWellFormedAnchors() ||
        !TargetPiece->HasValidSupportChain())
    {
        return false;
    }

    const int32 TargetIndex = TargetPiece->FindAnchorIndex(TargetAnchorId);
    const int32 OwnIndex = FindAnchorIndex(OwnAnchorId);
    if (!TargetPiece->Anchors.IsValidIndex(TargetIndex) ||
        !Anchors.IsValidIndex(OwnIndex))
    {
        return false;
    }

    const FStoneSnapAnchor& TargetAnchor = TargetPiece->Anchors[TargetIndex];
    const FStoneSnapAnchor& OwnAnchor = Anchors[OwnIndex];

    return TargetAnchor.bCanReceive && !TargetAnchor.bOccupied &&
        OwnAnchor.bCanConnect && !OwnAnchor.bOccupied &&
        TargetAnchor.CompatibilityTag == OwnAnchor.CompatibilityTag;
}

bool AStoneBuildingPiece::TryConnectToAnchor(
    AStoneBuildingPiece* TargetPiece,
    FName TargetAnchorId,
    FName OwnAnchorId)
{
    if (!CanConnectToAnchor(TargetPiece, TargetAnchorId, OwnAnchorId))
    {
        return false;
    }

    const int32 TargetIndex = TargetPiece->FindAnchorIndex(TargetAnchorId);
    const int32 OwnIndex = FindAnchorIndex(OwnAnchorId);
    const FTransform PreviousTransform = GetActorTransform();
    const FTransform TargetWorldTransform =
        TargetPiece->GetAnchorWorldTransform(TargetAnchorId);
    const FTransform DesiredActorTransform =
        Anchors[OwnIndex].LocalTransform.Inverse() * TargetWorldTransform;

    if (!SetActorTransform(DesiredActorTransform, false, nullptr,
        ETeleportType::TeleportPhysics))
    {
        return false;
    }

    if (!AttachToActor(TargetPiece, FAttachmentTransformRules::KeepWorldTransform))
    {
        SetActorTransform(PreviousTransform, false, nullptr,
            ETeleportType::TeleportPhysics);
        return false;
    }

    TargetPiece->Anchors[TargetIndex].bOccupied = true;
    Anchors[OwnIndex].bOccupied = true;
    ParentPiece = TargetPiece;
    ParentAnchorId = TargetAnchorId;
    OwnAnchorIdUsed = OwnAnchorId;
    TargetPiece->AttachedPieces.AddUnique(this);

    bIsHeld = false;
    bIsPlaced = true;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    return true;
}

void AStoneBuildingPiece::ReleaseAnchorReservation(FName AnchorId)
{
    const int32 Index = FindAnchorIndex(AnchorId);
    if (Anchors.IsValidIndex(Index))
    {
        Anchors[Index].bOccupied = false;
    }
}

bool AStoneBuildingPiece::RemoveConnection()
{
    AStoneBuildingPiece* Parent = ParentPiece.Get();
    if (!Parent)
    {
        ParentPiece.Reset();
        ParentAnchorId = NAME_None;
        OwnAnchorIdUsed = NAME_None;
        return false;
    }

    Parent->ReleaseAnchorReservation(ParentAnchorId);
    Parent->AttachedPieces.Remove(this);
    ReleaseAnchorReservation(OwnAnchorIdUsed);

    ParentPiece.Reset();
    ParentAnchorId = NAME_None;
    OwnAnchorIdUsed = NAME_None;
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    bIsPlaced = false;

    if (Mesh)
    {
        // A detached piece becomes a loose physical object again.
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
    }
    return true;
}
