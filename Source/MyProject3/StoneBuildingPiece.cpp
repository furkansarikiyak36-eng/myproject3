#include "StoneBuildingPiece.h"

#include "MaterialConnectionSubsystem.h"

#include "Components/StaticMeshComponent.h"
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
        Box(FVector(-125.0, 0.0, 100.0), FVector(35.0, 35.0, 200.0));
        Box(FVector(125.0, 0.0, 100.0), FVector(35.0, 35.0, 200.0));
        Box(FVector(0.0, 0.0, 185.0), FVector(280.0, 30.0, 30.0));
        for (const float X : {-80.0f, -40.0f, 0.0f, 40.0f, 80.0f})
            Box(FVector(X, 0.0, 95.0), FVector(12.0, 15.0, 160.0));
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

void AStoneBuildingPiece::EnsureAutomaticAnchors()
{
    if (!Mesh)
        return;
    if (bUsesStructuralProxy)
    {
        FVector Top(0.0, 0.0, 200.0);
        FVector Bottom = FVector::ZeroVector;
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

        if (FindAnchorIndex(TEXT("AutoTop")) == INDEX_NONE)
            AddCustomAnchor(TEXT("AutoTop"), TEXT("Stone"), FTransform(Top), true, false);
        if (FindAnchorIndex(TEXT("AutoBottom")) == INDEX_NONE)
            AddCustomAnchor(TEXT("AutoBottom"), TEXT("Stone"), FTransform(Bottom), false, true);

        if (PieceType == EStonePieceType::BridgeDeck ||
            PieceType == EStonePieceType::ElevatedWalkway)
        {
            if (FindAnchorIndex(TEXT("SupportLeft")) == INDEX_NONE)
                AddCustomAnchor(TEXT("SupportLeft"), TEXT("Stone"),
                    FTransform(FVector(-120.0, 0.0, -10.0)), false, true);
            if (FindAnchorIndex(TEXT("SupportRight")) == INDEX_NONE)
                AddCustomAnchor(TEXT("SupportRight"), TEXT("Stone"),
                    FTransform(FVector(120.0, 0.0, -10.0)), false, true);
        }
        return;
    }

    if (!Mesh->GetStaticMesh() || FindAnchorIndex(TEXT("AutoTop")) != INDEX_NONE)
        return;
    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const FVector Top = Bounds.Origin + FVector(0.0, 0.0, Bounds.BoxExtent.Z);
    const FVector Bottom = Bounds.Origin - FVector(0.0, 0.0, Bounds.BoxExtent.Z);
    AddCustomAnchor(TEXT("AutoTop"), TEXT("Stone"), FTransform(Top), true, false);
    AddCustomAnchor(TEXT("AutoBottom"), TEXT("Stone"), FTransform(Bottom), false, true);
}
