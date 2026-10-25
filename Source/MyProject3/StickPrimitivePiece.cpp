#include "StickPrimitivePiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

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
    if (StructuralIdentity)
    {
        StructuralIdentity->EnsureIdentity();
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            Graph->RegisterNode(this, StructuralIdentity->GetInstanceId(), EStructuralNodeKind::Stick,
                bIsPlaced, 1.0f, 100000.0f, 100.0f);
        }
    }
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsHeld && !bIsPlaced);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

void AStickPrimitivePiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveConnection();
    for (const TWeakObjectPtr<AStickPrimitivePiece>& Child : AttachedPieces)
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
        Mesh->SetCollisionProfileName(bIsPlaced ? TEXT("BlockAll") : TEXT("PhysicsActor"));
    }
    return true;
}

bool AStickPrimitivePiece::TryCommitGroundPlacement(
    const FVector& SurfaceLocation, const FRotator& SurfaceRotation)
{
    if (!bIsHeld || bIsPlaced || ParentPiece.IsValid() ||
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
        ParentPiece.IsValid() || !IsValid(TargetPiece) ||
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
    if (StructuralIdentity && TargetPiece->StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
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
        }
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

bool AStickPrimitivePiece::RemoveConnection()
{
    if (StructuralIdentity)
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            Graph->TryDisconnect(StructuralIdentity->GetInstanceId());
        }
    }
    AStickPrimitivePiece* Parent = ParentPiece.Get();
    if (!Parent)
    {
        return false;
    }

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
