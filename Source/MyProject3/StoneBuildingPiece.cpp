#include "StoneBuildingPiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

AStoneBuildingPiece::AStoneBuildingPiece()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
    SetRootComponent(Mesh);
    StructuralIdentity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(false);
}

void AStoneBuildingPiece::BeginPlay()
{
    Super::BeginPlay();

    bIsPlaced = bStartsPlaced;
    if (!StructuralIdentity || !GetWorld())
        return;

    StructuralIdentity->EnsureIdentity();
    if (DefinitionId.IsNone())
        DefinitionId = FName(TEXT("Stone.BuildingPiece"));
    StructuralIdentity->SetDefinitionId(DefinitionId);

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    if (!Ownership)
    {
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        return;
    }

    FWorldOwnedItemRecord Item;
    if (Ownership->TryGetItem(StructuralIdentity->GetInstanceId(), Item))
    {
        if (Item.OwnerKind != EWorldItemOwnerKind::World)
        {
            if (Mesh) Mesh->SetSimulatePhysics(false);
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            SetActorTickEnabled(false);
            return;
        }
        if (!Item.DefinitionId.IsNone())
        {
            DefinitionId = Item.DefinitionId;
            StructuralIdentity->SetDefinitionId(Item.DefinitionId);
        }
    }
    else
    {
        Item.ItemId = StructuralIdentity->GetInstanceId();
        Item.DefinitionId = DefinitionId;
        Item.Quantity = 1;
        Item.OwnerKind = EWorldItemOwnerKind::World;
        Item.WorldLocation = GetActorLocation();
        if (!Ownership->RegisterItem(Item))
        {
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            UE_LOG(LogTemp, Error, TEXT("Stone piece '%s' could not register item ownership."), *GetName());
            return;
        }
    }

    UWorldStructuralGraphSubsystem* Graph =
        GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Graph || !Graph->RegisterNode(this, StructuralIdentity->GetInstanceId(),
        EStructuralNodeKind::Stone, bIsPlaced, 1.0f, 100000.0f, 100.0f))
    {
        FStructuralNodeRecord ExistingNode;
        if (!Graph || !Graph->GetNode(StructuralIdentity->GetInstanceId(), ExistingNode) ||
            ExistingNode.Actor.Get() != this)
        {
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            UE_LOG(LogTemp, Error, TEXT("Stone piece '%s' could not register structural graph node."), *GetName());
            return;
        }
    }

    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsPlaced && !bIsHeld);
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

bool AStoneBuildingPiece::ActivateAfterOwnershipTransfer()
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
        Item.OwnerKind != EWorldItemOwnerKind::World || Item.DefinitionId.IsNone())
        return false;

    FStructuralNodeRecord ExistingNode;
    if (Graph->GetNode(Item.ItemId, ExistingNode))
    {
        if (ExistingNode.Actor.Get() != this)
            return false;
    }
    else if (!Graph->RegisterNode(this, Item.ItemId, EStructuralNodeKind::Stone,
        false, 1.0f, 100000.0f, 100.0f))
    {
        return false;
    }

    DefinitionId = Item.DefinitionId;
    StructuralIdentity->SetDefinitionId(Item.DefinitionId);
    bIsPlaced = false;
    bStartsPlaced = false;
    bIsHeld = false;
    Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionProfileName(FName(TEXT("PhysicsActor")));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    return true;
}

void AStoneBuildingPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveConnection();

    for (const TWeakObjectPtr<AStoneBuildingPiece>& Child : AttachedPieces)
    {
        if (Child.IsValid())
        {
            Child->RemoveConnection();
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
            ParentPiece.IsValid() || AttachedPieces.Num() > 0)
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
    if (StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            Graph->SetGroundSupported(StructuralIdentity->GetInstanceId(), true);
        }
    }

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
        bIsPlaced || !bIsHeld || bIsHeld == false ||
        ParentPiece.IsValid() || !TargetPiece->HasValidSupportChain())
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
    if (!CanConnectToAnchor(TargetPiece, TargetAnchorId, OwnAnchorId) ||
        !GetWorld() || !StructuralIdentity || !TargetPiece->StructuralIdentity)
    {
        return false;
    }

    UWorldStructuralGraphSubsystem* Graph =
        GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Graph)
        return false;

    const int32 TargetIndex = TargetPiece->FindAnchorIndex(TargetAnchorId);
    const int32 OwnIndex = FindAnchorIndex(OwnAnchorId);
    const FTransform PreviousTransform = GetActorTransform();
    const FTransform TargetWorldTransform =
        TargetPiece->GetAnchorWorldTransform(TargetAnchorId);
    const FTransform DesiredActorTransform =
        Anchors[OwnIndex].LocalTransform.Inverse() * TargetWorldTransform;

    SetActorTransform(DesiredActorTransform, false, nullptr,
        ETeleportType::TeleportPhysics);

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
    if (!Graph->TryConnect(TargetPiece->StructuralIdentity->GetInstanceId(), TargetAnchorId,
        StructuralIdentity->GetInstanceId(), OwnAnchorId))
    {
        TargetPiece->ReleaseAnchorReservation(TargetAnchorId);
        TargetPiece->AttachedPieces.Remove(this);
        ReleaseAnchorReservation(OwnAnchorId);
        ParentPiece.Reset(); ParentAnchorId = NAME_None; OwnAnchorIdUsed = NAME_None;
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        SetActorTransform(PreviousTransform, false, nullptr, ETeleportType::TeleportPhysics);
        return false;
    }

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
    if (StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            Graph->TryDisconnect(StructuralIdentity->GetInstanceId());
        }
    }
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
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    }
    return true;
}
