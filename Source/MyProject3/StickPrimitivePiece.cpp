#include "StickPrimitivePiece.h"

#include "MaterialConnectionSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

AStickPrimitivePiece::AStickPrimitivePiece()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StickMesh"));
    SetRootComponent(Mesh);
    StructuralIdentity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(true);
}

void AStickPrimitivePiece::BeginPlay()
{
    Super::BeginPlay();

    if (!StructuralIdentity || !GetWorld())
        return;

    StructuralIdentity->EnsureIdentity();
    if (DefinitionId.IsNone())
        DefinitionId = FName(TEXT("Stick.PrimitivePiece"));
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
            UE_LOG(LogTemp, Error, TEXT("Stick piece '%s' could not register item ownership."), *GetName());
            return;
        }
    }

    UWorldStructuralGraphSubsystem* Graph =
        GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Graph || !Graph->RegisterNode(this, StructuralIdentity->GetInstanceId(),
        EStructuralNodeKind::Stick, bIsPlaced, 1.0f, 100000.0f, 100.0f))
    {
        FStructuralNodeRecord ExistingNode;
        if (!Graph || !Graph->GetNode(StructuralIdentity->GetInstanceId(), ExistingNode) ||
            ExistingNode.Actor.Get() != this)
        {
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            UE_LOG(LogTemp, Error, TEXT("Stick piece '%s' could not register structural graph node."), *GetName());
            return;
        }
    }

    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsHeld && !bIsPlaced);
        Mesh->SetCollisionProfileName(bIsPlaced ? TEXT("BlockAll") : TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

bool AStickPrimitivePiece::ActivateAfterOwnershipTransfer()
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
    else if (!Graph->RegisterNode(this, Item.ItemId, EStructuralNodeKind::Stick,
        false, 1.0f, 100000.0f, 100.0f))
    {
        return false;
    }

    DefinitionId = Item.DefinitionId;
    StructuralIdentity->SetDefinitionId(Item.DefinitionId);
    bIsPlaced = false;
    bIsHeld = false;
    TrapState = EPrimitiveTrapState::Incomplete;
    Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionProfileName(FName(TEXT("PhysicsActor")));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    return true;
}

void AStickPrimitivePiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveConnection(true);
    if (UMaterialConnectionSubsystem* Connections =
        GetWorld() ? GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr)
    {
        Connections->ReleaseCrossMaterialChildren(this);
    }
    // Children remove themselves from AttachedPieces, so walk a copy.
    const TArray<TWeakObjectPtr<AStickPrimitivePiece>> AttachedCopy = AttachedPieces;
    for (const TWeakObjectPtr<AStickPrimitivePiece>& Child : AttachedCopy)
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

int32 AStickPrimitivePiece::FindAnchorIndex(FName AnchorId) const
{
    return AnchorId.IsNone() ? INDEX_NONE :
        Anchors.IndexOfByPredicate([AnchorId](const FPrimitiveAnchor& A)
        {
            return A.AnchorId == AnchorId;
        });
}

FTransform AStickPrimitivePiece::GetAnchorWorldTransform(FName AnchorId) const
{
    const int32 Index = FindAnchorIndex(AnchorId);
    return Anchors.IsValidIndex(Index)
        ? Anchors[Index].LocalTransform * GetActorTransform()
        : GetActorTransform();
}

bool AStickPrimitivePiece::AddAnchor(
    FName AnchorId, FName CompatibilityTag, FTransform LocalTransform)
{
    if (AnchorId.IsNone() || CompatibilityTag.IsNone() ||
        FindAnchorIndex(AnchorId) != INDEX_NONE || !LocalTransform.IsValid())
    {
        return false;
    }

    FPrimitiveAnchor Anchor;
    Anchor.AnchorId = AnchorId;
    Anchor.CompatibilityTag = CompatibilityTag;
    Anchor.LocalTransform = LocalTransform;
    Anchors.Add(Anchor);
    return true;
}

bool AStickPrimitivePiece::SetHeldState(bool bNewHeld)
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
        Mesh->SetSimulatePhysics(!bIsHeld && !bIsPlaced);
        Mesh->SetCollisionProfileName(bIsPlaced ? TEXT("BlockAll") : TEXT("PhysicsActor"));
    }
    return true;
}

void AStickPrimitivePiece::ResetLocalConnectionStateForGraphRestore()
{
    if (ParentPiece.IsValid() || CrossMaterialParent.IsValid() ||
        Cast<IStructuralPieceInterface>(GetAttachParentActor()))
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    ParentPiece.Reset();
    ParentAnchorId = NAME_None;
    OwnAnchorIdUsed = NAME_None;
    CrossMaterialParent.Reset();
    AttachedPieces.Reset();
    for (FPrimitiveAnchor& Anchor : Anchors)
        Anchor.bOccupied = false;
    bIsHeld = false;
    bIsPlaced = false;
}

bool AStickPrimitivePiece::RestoreConnectionFromGraph(
    AStickPrimitivePiece* Parent, FName InParentAnchorId, FName InOwnAnchorId)
{
    if (!IsValid(Parent) || Parent == this || !StructuralIdentity ||
        !Parent->StructuralIdentity || InParentAnchorId.IsNone() || InOwnAnchorId.IsNone() ||
        ParentPiece.IsValid() || CrossMaterialParent.IsValid())
        return false;

    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    FStructuralNodeRecord ChildNode;
    FStructuralNodeRecord ParentNode;
    if (!Graph || !Graph->GetNode(StructuralIdentity->GetInstanceId(), ChildNode) ||
        !Graph->GetNode(Parent->StructuralIdentity->GetInstanceId(), ParentNode) ||
        ChildNode.ParentNodeId != ParentNode.NodeId ||
        ChildNode.ParentAnchorId != InParentAnchorId || ChildNode.OwnAnchorId != InOwnAnchorId)
        return false;

    int32 ParentIndex = INDEX_NONE;
    int32 OwnIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Parent->Anchors.Num(); ++Index)
    {
        if (Parent->Anchors[Index].AnchorId == InParentAnchorId)
        {
            if (ParentIndex != INDEX_NONE) return false;
            ParentIndex = Index;
        }
    }
    for (int32 Index = 0; Index < Anchors.Num(); ++Index)
    {
        if (Anchors[Index].AnchorId == InOwnAnchorId)
        {
            if (OwnIndex != INDEX_NONE) return false;
            OwnIndex = Index;
        }
    }
    if (!Parent->Anchors.IsValidIndex(ParentIndex) || !Anchors.IsValidIndex(OwnIndex) ||
        Parent->Anchors[ParentIndex].bOccupied || Anchors[OwnIndex].bOccupied ||
        Parent->Anchors[ParentIndex].CompatibilityTag != Anchors[OwnIndex].CompatibilityTag)
        return false;

    if (!AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform))
        return false;
    Parent->Anchors[ParentIndex].bOccupied = true;
    Anchors[OwnIndex].bOccupied = true;
    ParentPiece = Parent;
    ParentAnchorId = InParentAnchorId;
    OwnAnchorIdUsed = InOwnAnchorId;
    Parent->AttachedPieces.AddUnique(this);
    bIsHeld = false;
    bIsPlaced = true;
    bStartsPlaced = true;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    SetActorEnableCollision(true);
    return true;
}

void AStickPrimitivePiece::RestoreRootStateFromGraph(bool bGroundSupported, bool bStorageOwned)
{
    bIsHeld = false;
    bIsPlaced = bGroundSupported;
    bStartsPlaced = bGroundSupported;
    if (PieceState == EStickPrimitiveState::TrapComponent)
        TrapState = bGroundSupported ? EPrimitiveTrapState::Grounded : EPrimitiveTrapState::Unplaced;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bGroundSupported && !bStorageOwned);
        Mesh->SetCollisionProfileName(bGroundSupported ? TEXT("BlockAll") : TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(bStorageOwned ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
    }
    SetActorEnableCollision(true);
}

bool AStickPrimitivePiece::TryCommitGroundPlacement(
    const FVector& SurfaceLocation, const FRotator& SurfaceRotation)
{
    if (!bIsHeld || bIsPlaced || ParentPiece.IsValid() ||
        !FMath::IsFinite(SurfaceLocation.X) || !FMath::IsFinite(SurfaceLocation.Y) ||
        !FMath::IsFinite(SurfaceLocation.Z) ||
        !FMath::IsFinite(SurfaceRotation.Pitch) || !FMath::IsFinite(SurfaceRotation.Yaw) ||
        !FMath::IsFinite(SurfaceRotation.Roll) || !StructuralIdentity || !GetWorld())
    {
        return false;
    }

    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Graph)
        return false;
    const FTransform PreviousTransform = GetActorTransform();
    SetActorLocationAndRotation(SurfaceLocation, SurfaceRotation, false, nullptr,
        ETeleportType::TeleportPhysics);
    if (!Graph->SetGroundSupported(StructuralIdentity->GetInstanceId(), true))
    {
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
    if (PieceState == EStickPrimitiveState::TrapComponent)
    {
        TrapState = EPrimitiveTrapState::Grounded;
    }
    return true;
}

bool AStickPrimitivePiece::TryProcessTo(EStickPrimitiveState NewState)
{
    if (NewState == PieceState || bIsPlaced || ParentPiece.IsValid())
    {
        return false;
    }

    bool bAllowed = false;
    switch (NewState)
    {
    case EStickPrimitiveState::Broken:
        bAllowed = PieceState == EStickPrimitiveState::Raw;
        break;
    case EStickPrimitiveState::Sharpened:
    case EStickPrimitiveState::DoubleEndedStake:
        bAllowed = PieceState == EStickPrimitiveState::Raw ||
            PieceState == EStickPrimitiveState::Broken;
        break;
    case EStickPrimitiveState::PrimitiveTool:
    case EStickPrimitiveState::Spear:
    case EStickPrimitiveState::Bow:
    case EStickPrimitiveState::Arrow:
    case EStickPrimitiveState::Torch:
    case EStickPrimitiveState::BarrierPart:
    case EStickPrimitiveState::StoragePart:
    case EStickPrimitiveState::TrapComponent:
        bAllowed = PieceState == EStickPrimitiveState::Raw ||
            PieceState == EStickPrimitiveState::Broken ||
            PieceState == EStickPrimitiveState::Sharpened;
        break;
    default:
        break;
    }

    if (!bAllowed)
    {
        return false;
    }

    PieceState = NewState;
    if (NewState == EStickPrimitiveState::TrapComponent)
    {
        TrapState = EPrimitiveTrapState::Incomplete;
    }
    return true;
}

bool AStickPrimitivePiece::TryArmTrap()
{
    if (PieceState != EStickPrimitiveState::TrapComponent &&
        PieceState != EStickPrimitiveState::AssembledStructure)
    {
        return false;
    }
    if (!bTrapHasAllComponents || !bTrapHasValidTrigger ||
        (bTrapRequiresGrounding && !bIsPlaced))
    {
        TrapState = bIsPlaced ? EPrimitiveTrapState::Incomplete : EPrimitiveTrapState::Unplaced;
        return false;
    }
    TrapState = EPrimitiveTrapState::Armed;
    return true;
}

bool AStickPrimitivePiece::TryTriggerTrap()
{
    if (TrapState != EPrimitiveTrapState::Armed || !bIsPlaced ||
        !bTrapHasAllComponents || !bTrapHasValidTrigger)
    {
        return false;
    }
    TrapState = EPrimitiveTrapState::Triggered;
    return true;
}

bool AStickPrimitivePiece::DisarmTrap()
{
    if (TrapState != EPrimitiveTrapState::Armed &&
        TrapState != EPrimitiveTrapState::Triggered)
    {
        return false;
    }
    TrapState = EPrimitiveTrapState::Disarmed;
    return true;
}

bool AStickPrimitivePiece::HasValidSupportChain() const
{
    const AStickPrimitivePiece* Current = this;
    TSet<const AStickPrimitivePiece*> Visited;
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

bool AStickPrimitivePiece::TryConnectToAnchor(
    AStickPrimitivePiece* TargetPiece, FName TargetAnchorId, FName OwnAnchorId)
{
    if (!IsValid(TargetPiece) || TargetPiece == this || bIsHeld || bIsPlaced ||
        ParentPiece.IsValid() || !TargetPiece->HasValidSupportChain() ||
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
    if (!TargetPiece->Anchors.IsValidIndex(TargetIndex) ||
        !Anchors.IsValidIndex(OwnIndex))
    {
        return false;
    }

    FPrimitiveAnchor& TargetAnchor = TargetPiece->Anchors[TargetIndex];
    FPrimitiveAnchor& OwnAnchor = Anchors[OwnIndex];
    if (TargetAnchor.bOccupied || OwnAnchor.bOccupied ||
        TargetAnchor.CompatibilityTag != OwnAnchor.CompatibilityTag)
    {
        return false;
    }

    const FTransform PreviousTransform = GetActorTransform();
    const FTransform TargetWorld = TargetPiece->GetAnchorWorldTransform(TargetAnchorId);
    const FTransform DesiredWorld = OwnAnchor.LocalTransform.Inverse() * TargetWorld;
    SetActorTransform(DesiredWorld, false, nullptr, ETeleportType::TeleportPhysics);

    if (!AttachToActor(TargetPiece, FAttachmentTransformRules::KeepWorldTransform))
    {
        SetActorTransform(PreviousTransform, false, nullptr, ETeleportType::TeleportPhysics);
        return false;
    }

    TargetAnchor.bOccupied = true;
    OwnAnchor.bOccupied = true;
    ParentPiece = TargetPiece;
    ParentAnchorId = TargetAnchorId;
    OwnAnchorIdUsed = OwnAnchorId;
    TargetPiece->AttachedPieces.AddUnique(this);
    if (!Graph->TryConnect(TargetPiece->StructuralIdentity->GetInstanceId(), TargetAnchorId,
        StructuralIdentity->GetInstanceId(), OwnAnchorId))
    {
        TargetPiece->ReleaseAnchor(TargetAnchorId);
        TargetPiece->AttachedPieces.Remove(this);
        ReleaseAnchor(OwnAnchorId);
        ParentPiece.Reset(); ParentAnchorId = NAME_None; OwnAnchorIdUsed = NAME_None;
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        SetActorTransform(PreviousTransform, false, nullptr, ETeleportType::TeleportPhysics);
        return false;
    }
    bIsPlaced = true;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    }
    return true;
}

void AStickPrimitivePiece::ReleaseAnchor(FName AnchorId)
{
    const int32 Index = FindAnchorIndex(AnchorId);
    if (Anchors.IsValidIndex(Index))
    {
        Anchors[Index].bOccupied = false;
    }
}

bool AStickPrimitivePiece::RemoveConnection(bool bAllowGraphFailure)
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

    AStickPrimitivePiece* Parent = ParentPiece.Get();
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
        return bGraphDisconnected;

    Parent->ReleaseAnchor(ParentAnchorId);
    Parent->AttachedPieces.Remove(this);
    ReleaseAnchor(OwnAnchorIdUsed);
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    ParentPiece.Reset();
    ParentAnchorId = NAME_None;
    OwnAnchorIdUsed = NAME_None;
    bIsPlaced = false;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(true);
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    }
    return true;
}

bool AStickPrimitivePiece::HasStructuralDependents() const
{
    const UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    return Graph && StructuralIdentity &&
        !Graph->GetAllDependents(StructuralIdentity->GetInstanceId()).IsEmpty();
}

FGuid AStickPrimitivePiece::GetStructuralNodeId() const
{
    return StructuralIdentity ? StructuralIdentity->GetInstanceId() : FGuid();
}

bool AStickPrimitivePiece::GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const
{
    const int32 Index = FindAnchorIndex(AnchorId);
    if (!Anchors.IsValidIndex(Index))
        return false;
    const FPrimitiveAnchor* Match = &Anchors[Index];
    OutAnchor.AnchorId = Match->AnchorId;
    OutAnchor.Tag = Match->CompatibilityTag;
    OutAnchor.LocalTransform = Match->LocalTransform;
    OutAnchor.bCanReceive = true;
    OutAnchor.bCanConnect = true;
    OutAnchor.bOccupied = Match->bOccupied;
    return true;
}

bool AStickPrimitivePiece::SetStructuralAnchorOccupied(FName AnchorId, bool bOccupied)
{
    const int32 Index = FindAnchorIndex(AnchorId);
    if (!Anchors.IsValidIndex(Index))
        return false;
    Anchors[Index].bOccupied = bOccupied;
    return true;
}

bool AStickPrimitivePiece::CanAttachAsCrossMaterialChild() const
{
    return !bIsPlaced && !ParentPiece.IsValid() && AttachedPieces.IsEmpty() &&
        !CrossMaterialParent.IsValid() && !HasStructuralDependents();
}

void AStickPrimitivePiece::ApplyCrossMaterialAttachment(AActor* Parent)
{
    CrossMaterialParent = Parent;
    bIsHeld = false;
    bIsPlaced = true;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

void AStickPrimitivePiece::ClearCrossMaterialAttachment()
{
    CrossMaterialParent.Reset();
    bIsPlaced = false;
    if (Mesh && !bIsHeld)
    {
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
    }
}

void AStickPrimitivePiece::GetStructuralAnchorIds(TArray<FName>& OutAnchorIds) const
{
    OutAnchorIds.Reset();
    for (const FPrimitiveAnchor& Anchor : Anchors)
        OutAnchorIds.Add(Anchor.AnchorId);
}

void AStickPrimitivePiece::EnsureAutomaticAnchors()
{
    if (!Mesh || !Mesh->GetStaticMesh() || FindAnchorIndex(TEXT("AutoTop")) != INDEX_NONE)
        return;
    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const FVector Top = Bounds.Origin + FVector(0.0, 0.0, Bounds.BoxExtent.Z);
    const FVector Bottom = Bounds.Origin - FVector(0.0, 0.0, Bounds.BoxExtent.Z);
    AddAnchor(TEXT("AutoTop"), TEXT("Stick"), FTransform(Top));
    AddAnchor(TEXT("AutoBottom"), TEXT("Stick"), FTransform(Bottom));
}
