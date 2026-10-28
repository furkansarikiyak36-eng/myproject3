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
