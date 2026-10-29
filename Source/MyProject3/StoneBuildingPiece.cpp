#include "StoneBuildingPiece.h"

#include "MaterialConnectionSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "WallCoatingComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

AStoneBuildingPiece::AStoneBuildingPiece()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
    WallCoating = CreateDefaultSubobject<UWallCoatingComponent>(TEXT("WallCoating"));
    SetRootComponent(Mesh);
    StructuralIdentity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(false);
}

void AStoneBuildingPiece::BeginPlay()
{
    Super::BeginPlay();
    // The placeholder kit is only a fallback for the newly authored archetypes;
    // any Blueprint-assigned physical mesh remains authoritative.
    BuildStructuralProxyIfNeeded();
    BuildMovingLeafIfNeeded();
    bIsOpen = bStartsOpen;
    ApplyMovingLeafState();
    // Its top and bottom joints exist from the start; placement queries never add them.
    EnsureAutomaticAnchors();

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
    float DamageThreshold = 100.0f;
    switch (PieceType)
    {
    case EStonePieceType::Gate: DamageThreshold = 70.0f; break;
    case EStonePieceType::DoorFrame: DamageThreshold = 65.0f; break;
    case EStonePieceType::FenceSegment: DamageThreshold = 45.0f; break;
    case EStonePieceType::RailingSegment: DamageThreshold = 35.0f; break;
    case EStonePieceType::WallSegment: DamageThreshold = 130.0f; break;
    case EStonePieceType::DefensiveWall: DamageThreshold = 180.0f; break;
    default: break;
    }
    if (!Graph || !Graph->RegisterNode(this, StructuralIdentity->GetInstanceId(),
        EStructuralNodeKind::Stone, bIsPlaced, 1.0f, 100000.0f, DamageThreshold))
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
        if (bUsesStructuralProxy)
        {
            // The empty root moves the whole kit; proxy members own its collision.
            Mesh->SetSimulatePhysics(false);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        else
        {
            Mesh->SetSimulatePhysics(!bIsPlaced && !bIsHeld);
            Mesh->SetCollisionProfileName(
                bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
            Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
    }
    ApplyStructuralProxyCollision();
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
    if (bUsesStructuralProxy)
        Mesh->SetSimulatePhysics(false);
    else
        Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionProfileName(FName(TEXT("PhysicsActor")));
    Mesh->SetCollisionEnabled(bUsesStructuralProxy ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    ApplyStructuralProxyCollision();
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    return true;
}

void AStoneBuildingPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    const TArray<TWeakObjectPtr<AStoneBuildingPiece>> RopeOwnersCopy = ConnectedRopeOwners;
    for (const TWeakObjectPtr<AStoneBuildingPiece>& Owner : RopeOwnersCopy)
        if (Owner.IsValid()) Owner->DisconnectRopeConnection();
    ConnectedRopeOwners.Reset();
    DisconnectRopeConnection();
    RemoveConnection(true);
    if (UMaterialConnectionSubsystem* Connections =
        GetWorld() ? GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr)
    {
        Connections->ReleaseCrossMaterialChildren(this);
    }

    // Children remove themselves from AttachedPieces, so walk a copy.
    const TArray<TWeakObjectPtr<AStoneBuildingPiece>> AttachedCopy = AttachedPieces;
    for (const TWeakObjectPtr<AStoneBuildingPiece>& Child : AttachedCopy)
    {
        if (Child.IsValid())
        {
            Child->RemoveConnection(true);
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
            ParentPiece.IsValid() || AttachedPieces.Num() > 0 ||
            CrossMaterialParent.IsValid() || HasStructuralDependents())
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
        Mesh->SetSimulatePhysics(bUsesStructuralProxy ? false : (!bIsHeld && !bIsPlaced));
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
        if (bUsesStructuralProxy)
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    ApplyStructuralProxyCollision();
    return true;
}

bool AStoneBuildingPiece::TryCommitGroundPlacement(
    const FVector& SurfaceLocation,
    const FRotator& SurfaceRotation)
{
    if (bIsPlaced || !bIsHeld || ParentPiece.IsValid() ||
        SurfaceLocation.ContainsNaN() || SurfaceRotation.ContainsNaN())
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
        Mesh->SetCollisionEnabled(bUsesStructuralProxy ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    }
    ApplyStructuralProxyCollision();
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
        Mesh->SetCollisionEnabled(bUsesStructuralProxy ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    }
    ApplyStructuralProxyCollision();
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

bool AStoneBuildingPiece::RemoveConnection(bool bAllowGraphFailure)
{
    if (CrossMaterialParent.IsValid() || (bAllowGraphFailure && !CrossMaterialParent.IsExplicitlyNull()))
    {
        UMaterialConnectionSubsystem* Connections =
            GetWorld() ? GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr;
        if (Connections && Connections->TryDisconnect(this))
            return true;
        if (!bAllowGraphFailure)
            return false;
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        ClearCrossMaterialAttachment();
        return true;
    }

    AStoneBuildingPiece* Parent = ParentPiece.Get();
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    bool bGraphDisconnected = false;

    if (Parent)
    {
        bool bGraphEdgeMatches = false;
        if (Graph && StructuralIdentity && Parent->StructuralIdentity)
        {
            FStructuralNodeRecord Node;
            bGraphEdgeMatches = Graph->GetNode(StructuralIdentity->GetInstanceId(), Node) &&
                Node.ParentNodeId == Parent->StructuralIdentity->GetInstanceId();
            if (bGraphEdgeMatches)
                bGraphDisconnected = Graph->TryDisconnect(StructuralIdentity->GetInstanceId());
        }

        if ((!bGraphEdgeMatches || !bGraphDisconnected) && !bAllowGraphFailure)
            return false;
    }
    else if (Graph && StructuralIdentity)
    {
        FStructuralNodeRecord Node;
        if (Graph->GetNode(StructuralIdentity->GetInstanceId(), Node) &&
            Node.ParentNodeId.IsValid())
        {
            bGraphDisconnected = Graph->TryDisconnect(StructuralIdentity->GetInstanceId());
            if (!bGraphDisconnected && !bAllowGraphFailure)
                return false;
        }
    }

    if (!Parent)
    {
        ParentPiece.Reset();
        ParentAnchorId = NAME_None;
        OwnAnchorIdUsed = NAME_None;
        if (bGraphDisconnected)
            ApplyStructuralProxyCollision();
        return bGraphDisconnected;
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
        if (bUsesStructuralProxy)
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    ApplyStructuralProxyCollision();
    return true;
}

bool AStoneBuildingPiece::HasStructuralDependents() const
{
    const UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    return Graph && StructuralIdentity &&
        !Graph->GetAllDependents(StructuralIdentity->GetInstanceId()).IsEmpty();
}

FGuid AStoneBuildingPiece::GetStructuralNodeId() const
{
    return StructuralIdentity ? StructuralIdentity->GetInstanceId() : FGuid();
}

bool AStoneBuildingPiece::GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const
{
    const int32 Index = FindAnchorIndex(AnchorId);
    if (!Anchors.IsValidIndex(Index))
        return false;
    const FStoneSnapAnchor* Match = &Anchors[Index];
    OutAnchor.AnchorId = Match->AnchorId;
    OutAnchor.Tag = Match->CompatibilityTag;
    OutAnchor.LocalTransform = Match->LocalTransform;
    OutAnchor.bCanReceive = Match->bCanReceive;
    OutAnchor.bCanConnect = Match->bCanConnect;
    OutAnchor.bOccupied = Match->bOccupied;
    return true;
}

bool AStoneBuildingPiece::SetStructuralAnchorOccupied(FName AnchorId, bool bOccupied)
{
    const int32 Index = FindAnchorIndex(AnchorId);
    if (!Anchors.IsValidIndex(Index))
        return false;
    Anchors[Index].bOccupied = bOccupied;
    return true;
}

bool AStoneBuildingPiece::CanAttachAsCrossMaterialChild() const
{
    return !bIsPlaced && !ParentPiece.IsValid() && AttachedPieces.IsEmpty() &&
        !CrossMaterialParent.IsValid() && !HasStructuralDependents();
}

void AStoneBuildingPiece::ApplyCrossMaterialAttachment(AActor* Parent)
{
    CrossMaterialParent = Parent;
    bIsHeld = false;
    bIsPlaced = true;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetCollisionEnabled(bUsesStructuralProxy ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    }
    ApplyStructuralProxyCollision();
}

void AStoneBuildingPiece::ClearCrossMaterialAttachment()
{
    CrossMaterialParent.Reset();
    bIsPlaced = false;
    if (Mesh && !bIsHeld)
    {
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(bUsesStructuralProxy ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(bUsesStructuralProxy ? false : true);
    }
    ApplyStructuralProxyCollision();
}

void AStoneBuildingPiece::GetStructuralAnchorIds(TArray<FName>& OutAnchorIds) const
{
    OutAnchorIds.Reset();
    for (const FStoneSnapAnchor& Anchor : Anchors)
        OutAnchorIds.Add(Anchor.AnchorId);
}

void AStoneBuildingPiece::BuildStructuralProxyIfNeeded()
{
    if (!Mesh || Mesh->GetStaticMesh() ||
        static_cast<uint8>(PieceType) <= static_cast<uint8>(EStonePieceType::Other))
        return;

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube)
        return;

    struct FProxyBox
    {
        FVector Center;
        FVector Size;
        FRotator Rotation;
    };
    TArray<FProxyBox> Boxes;
    auto Box = [&Boxes](const FVector& Center, const FVector& Size)
    {
        Boxes.Add({Center, Size, FRotator::ZeroRotator});
    };
    auto SlopedBox = [&Boxes](const FVector& Center, const FVector& Size, float Pitch)
    {
        Boxes.Add({Center, Size, FRotator(Pitch, 0.0f, 0.0f)});
    };

    switch (PieceType)
    {
    case EStonePieceType::WallSegment:
        Box(FVector(0.0, 0.0, 100.0), FVector(300.0, 35.0, 200.0));
        break;
    case EStonePieceType::DoorFrame:
    case EStonePieceType::WallOpening:
        Box(FVector(-120.0, 0.0, 100.0), FVector(60.0, 35.0, 200.0));
        Box(FVector(120.0, 0.0, 100.0), FVector(60.0, 35.0, 200.0));
        Box(FVector(0.0, 0.0, 170.0), FVector(300.0, 35.0, 60.0));
        break;
    case EStonePieceType::WindowFrame:
        Box(FVector(-120.0, 0.0, 100.0), FVector(50.0, 35.0, 200.0));
        Box(FVector(120.0, 0.0, 100.0), FVector(50.0, 35.0, 200.0));
        Box(FVector(0.0, 0.0, 160.0), FVector(300.0, 35.0, 40.0));
        Box(FVector(0.0, 0.0, 45.0), FVector(300.0, 35.0, 30.0));
        break;
    case EStonePieceType::FloorSlab:
    case EStonePieceType::RoofPanel:
        Box(FVector::ZeroVector, FVector(300.0, 300.0, 20.0));
        break;
    case EStonePieceType::Ramp:
        SlopedBox(FVector::ZeroVector, FVector(300.0, 140.0, 20.0), 18.0f);
        break;
    case EStonePieceType::Stair:
        Box(FVector(-105.0, 0.0, 11.0), FVector(80.0, 120.0, 22.0));
        Box(FVector(-35.0, 0.0, 33.0), FVector(80.0, 120.0, 22.0));
        Box(FVector(35.0, 0.0, 55.0), FVector(80.0, 120.0, 22.0));
        Box(FVector(105.0, 0.0, 77.0), FVector(80.0, 120.0, 22.0));
        break;
    case EStonePieceType::BridgeDeck:
        Box(FVector::ZeroVector, FVector(300.0, 120.0, 20.0));
        break;
    case EStonePieceType::ElevatedWalkway:
        Box(FVector::ZeroVector, FVector(300.0, 100.0, 20.0));
        for (const float X : {-120.0f, 0.0f, 120.0f})
        {
            Box(FVector(X, -45.0, 45.0), FVector(12.0, 12.0, 90.0));
            Box(FVector(X, 45.0, 45.0), FVector(12.0, 12.0, 90.0));
        }
        Box(FVector(0.0, -45.0, 90.0), FVector(300.0, 12.0, 12.0));
        Box(FVector(0.0, 45.0, 90.0), FVector(300.0, 12.0, 12.0));
        break;
    case EStonePieceType::RopeCrossingAnchor:
        Box(FVector(0.0, 0.0, 20.0), FVector(80.0, 80.0, 40.0));
        Box(FVector(0.0, 0.0, 105.0), FVector(30.0, 30.0, 170.0));
        Box(FVector(0.0, 0.0, 190.0), FVector(220.0, 25.0, 25.0));
        break;
    case EStonePieceType::Gate:
        // Fixed jambs/lintel only. The gate bars are the moving leaf built
        // by BuildMovingLeafIfNeeded; static proxy bars would block the passage
        // even after the leaf rotated open.
        Box(FVector(-125.0, 0.0, 100.0), FVector(35.0, 35.0, 200.0));
        Box(FVector(125.0, 0.0, 100.0), FVector(35.0, 35.0, 200.0));
        Box(FVector(0.0, 0.0, 185.0), FVector(280.0, 30.0, 30.0));
        break;
    case EStonePieceType::FenceSegment:
        Box(FVector(-130.0, 0.0, 60.0), FVector(25.0, 25.0, 120.0));
        Box(FVector(130.0, 0.0, 60.0), FVector(25.0, 25.0, 120.0));
        Box(FVector(0.0, 0.0, 35.0), FVector(280.0, 15.0, 15.0));
        Box(FVector(0.0, 0.0, 85.0), FVector(280.0, 15.0, 15.0));
        break;
    case EStonePieceType::RailingSegment:
        for (const float X : {-130.0f, -65.0f, 0.0f, 65.0f, 130.0f})
            Box(FVector(X, 0.0, 40.0), FVector(10.0, 12.0, 80.0));
        Box(FVector(0.0, 0.0, 80.0), FVector(300.0, 12.0, 12.0));
        break;
    case EStonePieceType::DefensiveWall:
        Box(FVector(0.0, 0.0, 110.0), FVector(300.0, 50.0, 220.0));
        for (const float X : {-100.0f, 0.0f, 100.0f})
            Box(FVector(X, 0.0, 235.0), FVector(45.0, 50.0, 30.0));
        break;
    default:
        return;
    }

    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetSimulatePhysics(false);
    bUsesStructuralProxy = true;
    StructuralProxyParts.Reset();
    StructuralProxyParts.Reserve(Boxes.Num());
    for (int32 Index = 0; Index < Boxes.Num(); ++Index)
    {
        const FProxyBox& Shape = Boxes[Index];
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(
            this, FName(*FString::Printf(TEXT("StructuralProxy_%02d"), Index)));
        if (!Part)
        {
            bUsesStructuralProxy = false;
            StructuralProxyParts.Reset();
            return;
        }
        AddInstanceComponent(Part);
        Part->SetupAttachment(Mesh);
        Part->SetStaticMesh(Cube);
        Part->SetRelativeLocation(Shape.Center);
        Part->SetRelativeRotation(Shape.Rotation);
        Part->SetRelativeScale3D(Shape.Size / 100.0f);
        Part->SetSimulatePhysics(false);
        Part->SetCollisionProfileName(TEXT("BlockAll"));
        Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Part->RegisterComponent();
        StructuralProxyParts.Add(Part);
    }
}

void AStoneBuildingPiece::ApplyStructuralProxyCollision()
{
    if (!bUsesStructuralProxy)
        return;

    for (UStaticMeshComponent* Part : StructuralProxyParts)
    {
        if (!IsValid(Part))
            continue;
        Part->SetSimulatePhysics(false);
        if (bIsHeld)
        {
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        else
        {
            Part->SetCollisionProfileName(
                bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
            Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
    }
}



void AStoneBuildingPiece::BuildMovingLeafIfNeeded()
{
    if (!bGenerateMovingLeaf || !Mesh ||
        (PieceType != EStonePieceType::DoorFrame &&
         PieceType != EStonePieceType::Gate &&
         PieceType != EStonePieceType::WindowFrame))
        return;

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube)
        return;

    MovingLeafPivot = NewObject<USceneComponent>(this, TEXT("MovingLeafPivot"));
    if (!MovingLeafPivot)
        return;
    AddInstanceComponent(MovingLeafPivot);
    MovingLeafPivot->SetupAttachment(Mesh);
    MovingLeafPivot->SetRelativeLocation(FVector(-120.0, 0.0, 100.0));
    MovingLeafPivot->RegisterComponent();

    auto AddLeafPart = [this, Cube](const TCHAR* Name, const FVector& PivotLocalCenter,
        const FVector& SizeCm)
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, FName(Name));
        if (!Part)
            return;
        AddInstanceComponent(Part);
        Part->SetupAttachment(MovingLeafPivot);
        Part->SetStaticMesh(Cube);
        Part->SetRelativeLocation(PivotLocalCenter);
        Part->SetRelativeScale3D(SizeCm / 100.0);
        Part->SetCollisionProfileName(TEXT("BlockAll"));
        Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Part->SetSimulatePhysics(false);
        Part->RegisterComponent();
        MovingLeafParts.Add(Part);
    };

    if (PieceType == EStonePieceType::DoorFrame)
    {
        AddLeafPart(TEXT("DoorLeaf"), FVector(120.0, 0.0, 0.0), FVector(240.0, 8.0, 180.0));
    }
    else if (PieceType == EStonePieceType::WindowFrame)
    {
        AddLeafPart(TEXT("WindowShutter"), FVector(120.0, 0.0, 0.0), FVector(240.0, 8.0, 100.0));
    }
    else
    {
        for (const float X : {-80.0f, -40.0f, 0.0f, 40.0f, 80.0f})
            AddLeafPart(*FString::Printf(TEXT("GateBar_%02d"), static_cast<int32>((X + 80.0f) / 40.0f)),
                FVector(X + 120.0, 0.0, -5.0), FVector(12.0, 15.0, 160.0));
        AddLeafPart(TEXT("GateLeafTopRail"), FVector(120.0, 0.0, 85.0), FVector(280.0, 18.0, 18.0));
    }
}

void AStoneBuildingPiece::ApplyMovingLeafState()
{
    if (MovingLeafPivot)
        MovingLeafPivot->SetRelativeRotation(FRotator(0.0, bIsOpen ? DoorOpenYawDegrees : 0.0, 0.0));
    for (UStaticMeshComponent* Part : MovingLeafParts)
    {
        if (IsValid(Part))
        {
            Part->SetCollisionProfileName(TEXT("BlockAll"));
            Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
    }
}

bool AStoneBuildingPiece::TrySetOpen(bool bOpen)
{
    if (PieceType != EStonePieceType::DoorFrame &&
        PieceType != EStonePieceType::WindowFrame &&
        PieceType != EStonePieceType::Gate)
        return false;
    if (!bGenerateMovingLeaf || !MovingLeafPivot || MovingLeafParts.IsEmpty())
        return false;
    if (bIsOpen == bOpen)
        return true;
    bIsOpen = bOpen;
    ApplyMovingLeafState();
    return true;
}

bool AStoneBuildingPiece::IsPassageClear() const
{
    switch (PieceType)
    {
    case EStonePieceType::WallOpening:
        return true;
    case EStonePieceType::DoorFrame:
    case EStonePieceType::Gate:
        return bIsOpen;
    default:
        return false;
    }
}

bool AStoneBuildingPiece::IsRaidBlocking() const
{
    switch (PieceType)
    {
    case EStonePieceType::WallOpening:
    case EStonePieceType::FloorSlab:
    case EStonePieceType::RoofPanel:
    case EStonePieceType::Ramp:
    case EStonePieceType::Stair:
    case EStonePieceType::BridgeDeck:
    case EStonePieceType::ElevatedWalkway:
    case EStonePieceType::RopeCrossingAnchor:
        return false;
    case EStonePieceType::DoorFrame:
    case EStonePieceType::Gate:
        return !bIsOpen;
    case EStonePieceType::WindowFrame:
        return true;
    case EStonePieceType::RawStone:
    case EStonePieceType::Block:
    case EStonePieceType::Slab:
    case EStonePieceType::Column:
    case EStonePieceType::Lintel:
    case EStonePieceType::Corner:
    case EStonePieceType::Other:
    case EStonePieceType::WallSegment:
    case EStonePieceType::FenceSegment:
    case EStonePieceType::RailingSegment:
    case EStonePieceType::DefensiveWall:
    default:
        return true;
    }
}

bool AStoneBuildingPiece::CanAcceptAdditionalSupport(FName OwnAnchorId) const
{
    if (PieceType != EStonePieceType::BridgeDeck &&
        PieceType != EStonePieceType::ElevatedWalkway)
        return true;
    if (OwnAnchorId != TEXT("SupportLeft") && OwnAnchorId != TEXT("SupportRight"))
        return false;

    const UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    FStructuralNodeRecord Node;
    if (!Graph || !StructuralIdentity ||
        !Graph->GetNode(StructuralIdentity->GetInstanceId(), Node) ||
        !Node.ParentNodeId.IsValid() || Node.OwnAnchorId == OwnAnchorId)
        return false;
    const bool bPrimaryIsSupportAnchor =
        Node.OwnAnchorId == TEXT("SupportLeft") || Node.OwnAnchorId == TEXT("SupportRight");
    if (!bPrimaryIsSupportAnchor)
        return false;
    return FVector::Dist(
        GetAnchorWorldTransform(TEXT("SupportLeft")).GetLocation(),
        GetAnchorWorldTransform(TEXT("SupportRight")).GetLocation()) <= MaxSupportSpanCm;
}


bool AStoneBuildingPiece::CanCarryTraversalLoad(float WeightKg) const
{
    if ((PieceType != EStonePieceType::BridgeDeck &&
         PieceType != EStonePieceType::ElevatedWalkway) ||
        !FMath::IsFinite(WeightKg) || WeightKg <= 0.0f ||
        WeightKg > MaxConcurrentTraversalWeightKg)
        return false;
    const UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    FStructuralNodeRecord Node;
    if (!Graph || !StructuralIdentity ||
        !Graph->GetNode(StructuralIdentity->GetInstanceId(), Node) ||
        !Node.ParentNodeId.IsValid() || Node.SecondarySupports.IsEmpty() ||
        !Graph->HasValidSupportPath(Node.NodeId) ||
        (Node.OwnAnchorId != TEXT("SupportLeft") && Node.OwnAnchorId != TEXT("SupportRight")) ||
        FVector::Dist(GetAnchorWorldTransform(TEXT("SupportLeft")).GetLocation(),
            GetAnchorWorldTransform(TEXT("SupportRight")).GetLocation()) > MaxSupportSpanCm)
        return false;
    const FName OtherEnd = Node.OwnAnchorId == TEXT("SupportLeft")
        ? FName(TEXT("SupportRight")) : FName(TEXT("SupportLeft"));
    const bool bHasOtherEndSupport = Node.SecondarySupports.ContainsByPredicate(
        [OtherEnd](const FStructuralSupportLink& Link) { return Link.OwnAnchorId == OtherEnd; });
    FStructuralAnchorView PrimaryAnchor;
    FStructuralAnchorView OtherAnchor;
    return bHasOtherEndSupport &&
        GetStructuralAnchor(Node.OwnAnchorId, PrimaryAnchor) && PrimaryAnchor.bOccupied &&
        GetStructuralAnchor(OtherEnd, OtherAnchor) && OtherAnchor.bOccupied;
}

bool AStoneBuildingPiece::TryReserveBridgeTraversal(FGuid TraversalId, float WeightKg)
{
    if (!TraversalId.IsValid() || ActiveBridgeTraversalWeights.Contains(TraversalId) ||
        !CanCarryTraversalLoad(WeightKg))
        return false;
    float CurrentLoad = 0.0f;
    for (const TPair<FGuid, float>& Pair : ActiveBridgeTraversalWeights)
        CurrentLoad += Pair.Value;
    if (!FMath::IsFinite(CurrentLoad) ||
        CurrentLoad + WeightKg > MaxConcurrentTraversalWeightKg)
        return false;
    ActiveBridgeTraversalWeights.Add(TraversalId, WeightKg);
    return true;
}

bool AStoneBuildingPiece::ReleaseBridgeTraversal(FGuid TraversalId)
{
    return TraversalId.IsValid() && ActiveBridgeTraversalWeights.Remove(TraversalId) > 0;
}

AStoneBuildingPiece* AStoneBuildingPiece::GetRopeConnectionOwner() const
{
    if (bOwnsRopeConnection)
        return const_cast<AStoneBuildingPiece*>(this);
    return RopeOtherEnd.Get();
}

bool AStoneBuildingPiece::TryConnectRopeTo(
    AStoneBuildingPiece* TargetPiece, FName OwnAnchorId, FName TargetAnchorId)
{
    if (!IsValid(TargetPiece) || TargetPiece == this ||
        PieceType != EStonePieceType::RopeCrossingAnchor ||
        TargetPiece->PieceType != EStonePieceType::RopeCrossingAnchor ||
        RopeOtherEnd.IsValid() || TargetPiece->RopeOtherEnd.IsValid() ||
        OwnAnchorId.IsNone() || TargetAnchorId.IsNone())
        return false;

    FStructuralAnchorView OwnAnchor;
    FStructuralAnchorView TargetAnchor;
    if (!GetStructuralAnchor(OwnAnchorId, OwnAnchor) ||
        !TargetPiece->GetStructuralAnchor(TargetAnchorId, TargetAnchor) ||
        !OwnAnchor.bCanConnect || !OwnAnchor.bCanReceive ||
        !TargetAnchor.bCanConnect || !TargetAnchor.bCanReceive ||
        OwnAnchor.bOccupied || TargetAnchor.bOccupied ||
        OwnAnchor.Tag != TargetAnchor.Tag)
        return false;

    const FVector Start = GetAnchorWorldTransform(OwnAnchorId).GetLocation();
    const FVector End = TargetPiece->GetAnchorWorldTransform(TargetAnchorId).GetLocation();
    const float Span = FVector::Distance(Start, End);
    const float AllowedSpan = FMath::Min(MaxRopeSpanCm, TargetPiece->MaxRopeSpanCm);
    if (!FMath::IsFinite(Span) || Span < 100.0f || Span > AllowedSpan)
        return false;

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube)
        return false;

    const FVector LocalStart = GetActorTransform().InverseTransformPosition(Start);
    const FVector LocalEnd = GetActorTransform().InverseTransformPosition(End);
    const FVector LocalDelta = LocalEnd - LocalStart;
    const float LocalLength = LocalDelta.Size();
    if (!FMath::IsFinite(LocalLength) || LocalLength < 100.0f)
        return false;

    TArray<TObjectPtr<UStaticMeshComponent>> NewSegments;
    const int32 SegmentCount = FMath::Clamp(FMath::CeilToInt(LocalLength / 100.0f), 1, 128);
    NewSegments.Reserve(SegmentCount);
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const float AlphaA = static_cast<float>(Index) / SegmentCount;
        const float AlphaB = static_cast<float>(Index + 1) / SegmentCount;
        const FVector A = FMath::Lerp(LocalStart, LocalEnd, AlphaA);
        const FVector B = FMath::Lerp(LocalStart, LocalEnd, AlphaB);
        const FVector Delta = B - A;
        UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(
            this, FName(*FString::Printf(TEXT("RopeSpan_%03d"), Index)));
        if (!Segment)
            break;
        AddInstanceComponent(Segment);
        Segment->SetupAttachment(Mesh);
        Segment->SetStaticMesh(Cube);
        Segment->SetRelativeLocation((A + B) * 0.5f);
        Segment->SetRelativeRotation(FQuat::FindBetweenVectors(FVector::ForwardVector, Delta.GetSafeNormal()).Rotator());
        Segment->SetRelativeScale3D(FVector(Delta.Size() / 100.0f, 0.035f, 0.035f));
        Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Segment->SetSimulatePhysics(false);
        Segment->RegisterComponent();
        NewSegments.Add(Segment);
    }
    if (NewSegments.Num() != SegmentCount)
    {
        for (UStaticMeshComponent* Segment : NewSegments)
            if (IsValid(Segment)) Segment->DestroyComponent();
        return false;
    }

    if (!SetStructuralAnchorOccupied(OwnAnchorId, true) ||
        !TargetPiece->SetStructuralAnchorOccupied(TargetAnchorId, true))
    {
        SetStructuralAnchorOccupied(OwnAnchorId, false);
        TargetPiece->SetStructuralAnchorOccupied(TargetAnchorId, false);
        for (UStaticMeshComponent* Segment : NewSegments)
            if (IsValid(Segment)) Segment->DestroyComponent();
        return false;
    }

    RopeVisualSegments = MoveTemp(NewSegments);
    RopeOtherEnd = TargetPiece;
    RopeLocalAnchorId = OwnAnchorId;
    RopeRemoteAnchorId = TargetAnchorId;
    bOwnsRopeConnection = true;
    TargetPiece->RopeOtherEnd = this;
    TargetPiece->RopeLocalAnchorId = TargetAnchorId;
    TargetPiece->RopeRemoteAnchorId = OwnAnchorId;
    TargetPiece->bOwnsRopeConnection = false;
    TargetPiece->ConnectedRopeOwners.AddUnique(this);
    return true;
}

void AStoneBuildingPiece::DisconnectRopeConnection()
{
    AStoneBuildingPiece* Owner = GetRopeConnectionOwner();
    if (Owner && Owner != this)
    {
        Owner->DisconnectRopeConnection();
        return;
    }

    AStoneBuildingPiece* Other = RopeOtherEnd.Get();
    for (UStaticMeshComponent* Segment : RopeVisualSegments)
        if (IsValid(Segment)) Segment->DestroyComponent();
    RopeVisualSegments.Reset();
    ActiveRopeTravelerWeights.Reset();
    ActiveRopeTravelers.Reset();
    ActiveRopeTraversalStartsAtOwner.Reset();

    if (IsValid(Other))
    {
        if (!RopeRemoteAnchorId.IsNone())
            Other->SetStructuralAnchorOccupied(RopeRemoteAnchorId, false);
        Other->ConnectedRopeOwners.RemoveAll(
            [this](const TWeakObjectPtr<AStoneBuildingPiece>& Ref) { return !Ref.IsValid() || Ref.Get() == this; });
        Other->RopeOtherEnd.Reset();
        Other->RopeLocalAnchorId = NAME_None;
        Other->RopeRemoteAnchorId = NAME_None;
        Other->bOwnsRopeConnection = false;
    }
    if (!RopeLocalAnchorId.IsNone())
        SetStructuralAnchorOccupied(RopeLocalAnchorId, false);
    RopeOtherEnd.Reset();
    RopeLocalAnchorId = NAME_None;
    RopeRemoteAnchorId = NAME_None;
    bOwnsRopeConnection = false;
}

bool AStoneBuildingPiece::CanBeginRopeTraversal(float WeightKg) const
{
    const AStoneBuildingPiece* Owner = bOwnsRopeConnection ? this : RopeOtherEnd.Get();
    if (!Owner || !FMath::IsFinite(WeightKg) || WeightKg <= 0.0f)
        return false;
    const AStoneBuildingPiece* Other = Owner->RopeOtherEnd.Get();
    if (!Other || Other == Owner || Owner->PieceType != EStonePieceType::RopeCrossingAnchor ||
        Other->PieceType != EStonePieceType::RopeCrossingAnchor)
        return false;
    const float Span = FVector::Distance(
        Owner->GetAnchorWorldTransform(Owner->RopeLocalAnchorId).GetLocation(),
        Other->GetAnchorWorldTransform(Owner->RopeRemoteAnchorId).GetLocation());
    if (!FMath::IsFinite(Span) || Span > FMath::Min(Owner->MaxRopeSpanCm, Other->MaxRopeSpanCm) ||
        WeightKg > FMath::Min(Owner->MaxRopeTravelerWeightKg, Other->MaxRopeTravelerWeightKg))
        return false;
    float CurrentLoad = 0.0f;
    for (const TPair<FGuid, float>& Pair : Owner->ActiveRopeTravelerWeights)
        CurrentLoad += Pair.Value;
    return FMath::IsFinite(CurrentLoad) &&
        CurrentLoad + WeightKg <= FMath::Min(Owner->MaxConcurrentTraversalWeightKg,
            Other->MaxConcurrentTraversalWeightKg);
}

bool AStoneBuildingPiece::TryBeginRopeTraversal(
    AActor* Traveler, float WeightKg, FGuid& OutTraversalId)
{
    OutTraversalId.Invalidate();
    AStoneBuildingPiece* Owner = GetRopeConnectionOwner();
    if (!Owner || !IsValid(Traveler))
        return false;

    // Dead/disappeared travelers must not retain the rope's shared capacity.
    TArray<FGuid> StaleTickets;
    for (const TPair<FGuid, TWeakObjectPtr<AActor>>& Pair : Owner->ActiveRopeTravelers)
        if (!Pair.Value.IsValid())
            StaleTickets.Add(Pair.Key);
    for (const FGuid& StaleTicket : StaleTickets)
        Owner->CancelRopeTraversal(StaleTicket);

    if (!CanBeginRopeTraversal(WeightKg))
        return false;
    const bool bStartsAtOwner = Owner == this;
    const FName StartAnchor = bStartsAtOwner ? Owner->RopeLocalAnchorId : Owner->RopeRemoteAnchorId;
    const FVector Start = GetAnchorWorldTransform(StartAnchor).GetLocation();
    const FGuid Ticket = FGuid::NewGuid();
    if (!Ticket.IsValid() || Owner->ActiveRopeTravelers.Contains(Ticket))
        return false;
    if (!Traveler->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics))
        return false;
    Owner->ActiveRopeTravelers.Add(Ticket, Traveler);
    Owner->ActiveRopeTravelerWeights.Add(Ticket, WeightKg);
    Owner->ActiveRopeTraversalStartsAtOwner.Add(Ticket, bStartsAtOwner);
    OutTraversalId = Ticket;
    return true;
}

bool AStoneBuildingPiece::AdvanceRopeTraversal(FGuid TraversalId, float Alpha)
{
    AStoneBuildingPiece* Owner = GetRopeConnectionOwner();
    if (!Owner)
        Owner = this;
    TWeakObjectPtr<AActor>* TravelerRef = Owner->ActiveRopeTravelers.Find(TraversalId);
    const bool* StartsAtOwner = Owner->ActiveRopeTraversalStartsAtOwner.Find(TraversalId);
    AStoneBuildingPiece* Other = Owner->RopeOtherEnd.Get();
    if (!TravelerRef || !TravelerRef->IsValid() || !StartsAtOwner || !Other ||
        !FMath::IsFinite(Alpha))
    {
        Owner->CancelRopeTraversal(TraversalId);
        return false;
    }
    const FVector Start = (*StartsAtOwner ? Owner : Other)->GetAnchorWorldTransform(
        *StartsAtOwner ? Owner->RopeLocalAnchorId : Owner->RopeRemoteAnchorId).GetLocation();
    const FVector End = (*StartsAtOwner ? Other : Owner)->GetAnchorWorldTransform(
        *StartsAtOwner ? Owner->RopeRemoteAnchorId : Owner->RopeLocalAnchorId).GetLocation();
    const float Progress = FMath::Clamp(Alpha, 0.0f, 1.0f);
    const FVector Position = FMath::Lerp(Start, End, Progress);
    // Sweep against the world so a blocked rope path cannot teleport a
    // traveler through a wall. The caller may cancel or retry the active ticket.
    if (!TravelerRef->Get()->SetActorLocation(Position, true, nullptr, ETeleportType::TeleportPhysics))
        return false;
    if (Progress >= 1.0f)
        return Owner->CancelRopeTraversal(TraversalId);
    return true;
}

bool AStoneBuildingPiece::CancelRopeTraversal(FGuid TraversalId)
{
    AStoneBuildingPiece* Owner = GetRopeConnectionOwner();
    if (!Owner)
        Owner = this;
    const bool bHadTicket = Owner->ActiveRopeTravelers.Remove(TraversalId) > 0;
    Owner->ActiveRopeTravelerWeights.Remove(TraversalId);
    Owner->ActiveRopeTraversalStartsAtOwner.Remove(TraversalId);
    return bHadTicket;
}

void AStoneBuildingPiece::ResetActorLocalAttachmentStateForGraphRestore(bool bPlaced)
{
    if (AStoneBuildingPiece* Parent = ParentPiece.Get())
    {
        Parent->ReleaseAnchorReservation(ParentAnchorId);
        Parent->AttachedPieces.Remove(this);
    }
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    ParentPiece.Reset();
    ParentAnchorId = NAME_None;
    OwnAnchorIdUsed = NAME_None;
    AttachedPieces.Reset();
    CrossMaterialParent.Reset();
    for (FStoneSnapAnchor& Anchor : Anchors)
        Anchor.bOccupied = false;
    bIsHeld = false;
    bIsPlaced = bPlaced;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(bUsesStructuralProxy ? false : !bIsPlaced);
        Mesh->SetCollisionProfileName(bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
        Mesh->SetCollisionEnabled(bUsesStructuralProxy ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    }
    ApplyStructuralProxyCollision();
}

bool AStoneBuildingPiece::RestoreSameMaterialAttachmentFromGraph(
    AStoneBuildingPiece* Parent, FName ParentAnchorIdIn, FName OwnAnchorIdIn)
{
    if (!IsValid(Parent) || Parent == this || ParentPiece.IsValid() ||
        ParentAnchorIdIn.IsNone() || OwnAnchorIdIn.IsNone())
        return false;
    FStructuralAnchorView ParentAnchor;
    FStructuralAnchorView OwnAnchor;
    if (!Parent->GetStructuralAnchor(ParentAnchorIdIn, ParentAnchor) ||
        !GetStructuralAnchor(OwnAnchorIdIn, OwnAnchor) ||
        !ParentAnchor.bCanReceive || !OwnAnchor.bCanConnect ||
        ParentAnchor.Tag != OwnAnchor.Tag || ParentAnchor.bOccupied || OwnAnchor.bOccupied)
        return false;
    if (!AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform))
        return false;
    if (!Parent->SetStructuralAnchorOccupied(ParentAnchorIdIn, true) ||
        !SetStructuralAnchorOccupied(OwnAnchorIdIn, true))
    {
        Parent->SetStructuralAnchorOccupied(ParentAnchorIdIn, false);
        SetStructuralAnchorOccupied(OwnAnchorIdIn, false);
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        return false;
    }
    ParentPiece = Parent;
    this->ParentAnchorId = ParentAnchorIdIn;
    OwnAnchorIdUsed = OwnAnchorIdIn;
    Parent->AttachedPieces.AddUnique(this);
    CrossMaterialParent.Reset();
    bIsHeld = false;
    bIsPlaced = true;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(FName(TEXT("BlockAll")));
        Mesh->SetCollisionEnabled(bUsesStructuralProxy ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    }
    ApplyStructuralProxyCollision();
    return true;
}

void AStoneBuildingPiece::EnsureAutomaticAnchors()
{
    if (!Mesh)
        return;

    FVector Top = FVector::ZeroVector;
    FVector Bottom = FVector::ZeroVector;
    if (bUsesStructuralProxy)
    {
        Top.Z = 200.0;
        if (PieceType == EStonePieceType::DefensiveWall)
            Top.Z = 250.0;
        else if (PieceType == EStonePieceType::FloorSlab ||
                 PieceType == EStonePieceType::RoofPanel ||
                 PieceType == EStonePieceType::Ramp ||
                 PieceType == EStonePieceType::BridgeDeck ||
                 PieceType == EStonePieceType::ElevatedWalkway)
        {
            Top.Z = 10.0;
            Bottom.Z = -10.0;
        }
        else if (PieceType == EStonePieceType::Stair)
            Top.Z = 88.0;
        else if (PieceType == EStonePieceType::FenceSegment)
            Top.Z = 120.0;
        else if (PieceType == EStonePieceType::RailingSegment)
            Top.Z = 86.0;
        else if (PieceType == EStonePieceType::RopeCrossingAnchor)
            Top.Z = 202.5;
    }
    else
    {
        if (!Mesh->GetStaticMesh())
            return;
        const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
        Top = Bounds.Origin + FVector(0.0, 0.0, Bounds.BoxExtent.Z);
        Bottom = Bounds.Origin - FVector(0.0, 0.0, Bounds.BoxExtent.Z);
    }

    if (FindAnchorIndex(TEXT("AutoTop")) == INDEX_NONE)
        AddCustomAnchor(TEXT("AutoTop"), TEXT("Stone"), FTransform(Top), true, false);
    if (FindAnchorIndex(TEXT("AutoBottom")) == INDEX_NONE)
        AddCustomAnchor(TEXT("AutoBottom"), TEXT("Stone"), FTransform(Bottom), false, true);

    // Secondary support points are required whether geometry comes from the
    // proxy kit or a Blueprint-supplied physical mesh. For authored meshes,
    // anchor positions follow the real mesh bounds instead of fixed proxy sizes.
    if (PieceType == EStonePieceType::BridgeDeck ||
        PieceType == EStonePieceType::ElevatedWalkway)
    {
        FVector Left(-120.0, 0.0, -10.0);
        FVector Right(120.0, 0.0, -10.0);
        if (!bUsesStructuralProxy && Mesh->GetStaticMesh())
        {
            const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
            const double ZBottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;
            Left = FVector(Bounds.Origin.X - Bounds.BoxExtent.X, Bounds.Origin.Y, ZBottom);
            Right = FVector(Bounds.Origin.X + Bounds.BoxExtent.X, Bounds.Origin.Y, ZBottom);
        }
        if (FindAnchorIndex(TEXT("SupportLeft")) == INDEX_NONE)
            AddCustomAnchor(TEXT("SupportLeft"), TEXT("Stone"), FTransform(Left), false, true);
        if (FindAnchorIndex(TEXT("SupportRight")) == INDEX_NONE)
            AddCustomAnchor(TEXT("SupportRight"), TEXT("Stone"), FTransform(Right), false, true);
    }
    if (PieceType == EStonePieceType::RopeCrossingAnchor &&
        FindAnchorIndex(TEXT("RopeSocket")) == INDEX_NONE)
        AddCustomAnchor(TEXT("RopeSocket"), TEXT("Rope"), FTransform(Top), true, true);
}
