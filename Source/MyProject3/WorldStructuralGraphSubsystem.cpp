#include "WorldStructuralGraphSubsystem.h"

#include "GameFramework/Actor.h"
#include "WorldObjectIdentityComponent.h"
#include "LogBuildPiece.h"
#include "StoneBuildingPiece.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "StickPrimitivePiece.h"
#include "MaterialConnectionSubsystem.h"
#include "StructuralPieceInterface.h"
#include "EngineUtils.h"
#include "InventoryCraftingComponent.h"
#include "InventoryCraftingTypes.h"
#include "WorldGameDataSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

bool UWorldStructuralGraphSubsystem::ExportSnapshot(
    TArray<FWorldStructuralNodeSnapshot>& OutSnapshot) const
{
    OutSnapshot.Reset();
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        const FStructuralNodeRecord& Node = Pair.Value;
        AActor* Actor = Node.Actor.Get();
        UWorldObjectIdentityComponent* Identity =
            IsValid(Actor) ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        if (!Node.NodeId.IsValid() || !IsValid(Actor) || !IsValid(Identity) ||
            !Identity->InstanceId.IsValid())
        {
            OutSnapshot.Reset();
            return false;
        }

        FWorldStructuralNodeSnapshot& State = OutSnapshot.AddDefaulted_GetRef();
        State.NodeId = Node.NodeId;
        State.ActorIdentityId = Identity->InstanceId;
        State.Kind = Node.Kind;
        State.ParentNodeId = Node.ParentNodeId;
        State.ParentAnchorId = Node.ParentAnchorId;
        State.OwnAnchorId = Node.OwnAnchorId;
        State.SecondarySupports = Node.SecondarySupports;
        State.LocalLoad = Node.LocalLoad;
        State.MaxSupportedLoad = Node.MaxSupportedLoad;
        State.bGroundSupported = Node.bGroundSupported;
        State.Damage = Node.Damage;
        State.DamageThreshold = Node.DamageThreshold;
        State.Revision = Node.Revision;
    }
    OutSnapshot.Sort([](const FWorldStructuralNodeSnapshot& A, const FWorldStructuralNodeSnapshot& B)
    {
        return A.NodeId.ToString() < B.NodeId.ToString();
    });
    return true;
}

bool UWorldStructuralGraphSubsystem::RestoreSnapshot(
    const TArray<FWorldStructuralNodeSnapshot>& Snapshot)
{
    // A legacy save without graph data must not erase an already registered structure.
    if (Snapshot.IsEmpty() && !Nodes.IsEmpty())
        return false;

    TMap<FGuid, AActor*> ActorsByIdentity;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            UWorldObjectIdentityComponent* Identity =
                IsValid(Actor) ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
            if (IsValid(Identity) && Identity->InstanceId.IsValid())
            {
                if (ActorsByIdentity.Contains(Identity->InstanceId))
                    return false;
                ActorsByIdentity.Add(Identity->InstanceId, Actor);
            }
        }
    }
    if (ActorsByIdentity.IsEmpty() && !Snapshot.IsEmpty())
        return false;

    TMap<FGuid, FStructuralNodeRecord> Proposed;
    TSet<FGuid> UsedActorIds;
    TSet<FString> OccupiedParentAnchors;
    for (const FWorldStructuralNodeSnapshot& State : Snapshot)
    {
        AActor* const* Actor = ActorsByIdentity.Find(State.ActorIdentityId);
        if (!State.NodeId.IsValid() || Proposed.Contains(State.NodeId) ||
            !State.ActorIdentityId.IsValid() || UsedActorIds.Contains(State.ActorIdentityId) ||
            !Actor || !IsValid(*Actor) ||
            static_cast<uint8>(State.Kind) == static_cast<uint8>(EStructuralNodeKind::Unknown) ||
            static_cast<uint8>(State.Kind) > static_cast<uint8>(EStructuralNodeKind::Other) ||
            !FMath::IsFinite(State.LocalLoad) || State.LocalLoad < 0.0f ||
            !FMath::IsFinite(State.MaxSupportedLoad) || State.MaxSupportedLoad <= 0.0f ||
            !FMath::IsFinite(State.Damage) || State.Damage < 0.0f ||
            !FMath::IsFinite(State.DamageThreshold) || State.DamageThreshold <= 0.0f ||
            State.Revision < 0 ||
            // A child is supported through its parent and must not also claim
            // ground contact; a parentless root may be ground-supported or loose.
            (State.ParentNodeId.IsValid() &&
                (State.ParentAnchorId.IsNone() || State.OwnAnchorId.IsNone() || State.bGroundSupported)) ||
            (!State.ParentNodeId.IsValid() &&
                (!State.ParentAnchorId.IsNone() || !State.OwnAnchorId.IsNone())))
        {
            return false;
        }

        if (State.ParentNodeId.IsValid())
        {
            const FString AnchorKey = State.ParentNodeId.ToString() + TEXT(":") + State.ParentAnchorId.ToString();
            if (OccupiedParentAnchors.Contains(AnchorKey))
                return false;
            OccupiedParentAnchors.Add(AnchorKey);
        }

        // Secondary supports only exist next to a primary edge, and every
        // contact still occupies exactly one physical anchor on each side.
        if (!State.SecondarySupports.IsEmpty() && !State.ParentNodeId.IsValid())
            return false;
        TSet<FName> OwnAnchors;
        OwnAnchors.Add(State.OwnAnchorId);
        TSet<FGuid> SupportIds;
        SupportIds.Add(State.ParentNodeId);
        for (const FStructuralSupportLink& Link : State.SecondarySupports)
        {
            const FString AnchorKey = Link.SupportNodeId.ToString() + TEXT(":") + Link.SupportAnchorId.ToString();
            if (!Link.SupportNodeId.IsValid() || Link.SupportNodeId == State.NodeId ||
                Link.SupportAnchorId.IsNone() || Link.OwnAnchorId.IsNone() ||
                SupportIds.Contains(Link.SupportNodeId) || OwnAnchors.Contains(Link.OwnAnchorId) ||
                OccupiedParentAnchors.Contains(AnchorKey))
            {
                return false;
            }
            SupportIds.Add(Link.SupportNodeId);
            OwnAnchors.Add(Link.OwnAnchorId);
            OccupiedParentAnchors.Add(AnchorKey);
        }

        FStructuralNodeRecord Node;
        Node.NodeId = State.NodeId;
        Node.Actor = *Actor;
        Node.Kind = State.Kind;
        Node.ParentNodeId = State.ParentNodeId;
        Node.ParentAnchorId = State.ParentAnchorId;
        Node.OwnAnchorId = State.OwnAnchorId;
        Node.SecondarySupports = State.SecondarySupports;
        Node.LocalLoad = State.LocalLoad;
        Node.MaxSupportedLoad = State.MaxSupportedLoad;
        Node.bGroundSupported = State.bGroundSupported;
        Node.Damage = State.Damage;
        Node.DamageThreshold = State.DamageThreshold;
        Node.Revision = State.Revision;
        Proposed.Add(Node.NodeId, Node);
        UsedActorIds.Add(State.ActorIdentityId);
    }

    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Proposed)
    {
        if (Pair.Value.ParentNodeId.IsValid() && !Proposed.Contains(Pair.Value.ParentNodeId))
            return false;
        for (const FStructuralSupportLink& Link : Pair.Value.SecondarySupports)
        {
            if (!Proposed.Contains(Link.SupportNodeId))
                return false;
        }
    }

    TMap<FGuid, FStructuralNodeRecord> Previous = MoveTemp(Nodes);
    Nodes = MoveTemp(Proposed);
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        // Loose, unplaced pieces may be valid graph roots without ground support.
        // Any node that claims a support must reach ground through some support.
        if (Pair.Value.ParentNodeId.IsValid() && !HasValidSupportPath(Pair.Key))
        {
            Nodes = MoveTemp(Previous);
            return false;
        }
    }
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        TSet<FGuid> Visiting;
        bool bLoadValid = true;
        const float AggregateLoad = ComputeAggregateLoad(Pair.Key, Visiting, bLoadValid);
        if (!bLoadValid || !FMath::IsFinite(AggregateLoad))
        {
            Nodes = MoveTemp(Previous);
            return false;
        }
        const FStructuralNodeRecord* Node = Nodes.Find(Pair.Key);
        if (!Node || AggregateLoad > Node->MaxSupportedLoad)
        {
            Nodes = MoveTemp(Previous);
            return false;
        }
    }
    return true;
}


bool UWorldStructuralGraphSubsystem::RebuildLogActorAttachmentsFromSnapshot()
{
    // Preflight all edges before mutating any local attachment. Cross-domain
    // edges need their own actor adapter; do not detach a log from a stone or
    // foundation and then silently leave the canonical edge without a local link.
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        const FStructuralNodeRecord& ChildNode = Pair.Value;
        if (!ChildNode.ParentNodeId.IsValid())
            continue;
        const FStructuralNodeRecord* ParentNode = Nodes.Find(ChildNode.ParentNodeId);
        if (!ParentNode)
            return false;
        const bool bChildIsLog = Cast<ALogBuildPiece>(ChildNode.Actor.Get()) != nullptr;
        const bool bParentIsLog = Cast<ALogBuildPiece>(ParentNode->Actor.Get()) != nullptr;
        const bool bMixedMaterial = ChildNode.Kind != ParentNode->Kind;
        if (bMixedMaterial &&
            (!Cast<IStructuralPieceInterface>(ChildNode.Actor.Get()) ||
             !Cast<IStructuralPieceInterface>(ParentNode->Actor.Get())))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Cannot restore mixed-material edge without structural adapters on both actors (child %s)."),
                *Pair.Key.ToString());
            return false;
        }
        if (!bMixedMaterial && bChildIsLog != bParentIsLog)
            return false;
    }

    UMaterialConnectionSubsystem* Connections =
        GetWorld() ? GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr;

    // First clear all domain-local references and anchor reservations. The
    // canonical graph remains authoritative and is restored in topological order below.
    for (TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        AActor* Actor = Pair.Value.Actor.Get();
        const bool bPlaced = Pair.Value.bGroundSupported || Pair.Value.ParentNodeId.IsValid();
        if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Actor))
            Stone->ResetActorLocalAttachmentStateForGraphRestore(bPlaced);
        else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Actor))
            Stick->ResetActorLocalAttachmentStateForGraphRestore(bPlaced);
        else if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Actor))
            Log->ResetActorLocalSnapStateForGraphRestore();
        else if (IStructuralPieceInterface* Piece = Cast<IStructuralPieceInterface>(Actor))
        {
            if (Piece->GetCrossMaterialParent())
            {
                Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                Piece->ClearCrossMaterialAttachment();
            }
        }
    }

    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        const FStructuralNodeRecord* ParentNode = Pair.Value.ParentNodeId.IsValid()
            ? Nodes.Find(Pair.Value.ParentNodeId) : nullptr;
        if (ParentNode && ParentNode->Kind != Pair.Value.Kind)
        {
            if (IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(Pair.Value.Actor.Get()))
                ChildPiece->SetStructuralAnchorOccupied(Pair.Value.OwnAnchorId, false);
            if (IStructuralPieceInterface* ParentPiece = Cast<IStructuralPieceInterface>(ParentNode->Actor.Get()))
                ParentPiece->SetStructuralAnchorOccupied(Pair.Value.ParentAnchorId, false);
        }
    }

    TArray<FGuid> OrderedIds;
    Nodes.GetKeys(OrderedIds);
    OrderedIds.Sort([](const FGuid& A, const FGuid& B)
    {
        return A.ToString() < B.ToString();
    });

    for (const FGuid& NodeId : OrderedIds)
    {
        const FStructuralNodeRecord* ChildNode = Nodes.Find(NodeId);
        if (!ChildNode || !ChildNode->ParentNodeId.IsValid())
            continue;

        const FStructuralNodeRecord* ParentNode = Nodes.Find(ChildNode->ParentNodeId);
        if (ParentNode && ParentNode->Kind != ChildNode->Kind)
        {
            // Cross-material joints are rebuilt by the material connection adapter.
            if (!Connections || !Connections->RestoreLinkFromGraph(ParentNode->Actor.Get(),
                ChildNode->ParentAnchorId, ChildNode->Actor.Get(), ChildNode->OwnAnchorId))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Structural graph restored but cross-material anchor reconstruction failed for node %s."),
                    *NodeId.ToString());
                return false;
            }
            continue;
        }

        AActor* ChildActor = ChildNode->Actor.Get();
        AActor* ParentActor = ParentNode ? ParentNode->Actor.Get() : nullptr;
        if (!ChildActor || !ParentActor)
            return false;

        bool bRestored = true;
        if (ALogBuildPiece* ChildLog = Cast<ALogBuildPiece>(ChildActor))
        {
            if (ALogBuildPiece* ParentLog = Cast<ALogBuildPiece>(ParentActor))
                bRestored = ChildLog->RestoreActorLocalSnapLinkFromGraph(
                    ParentLog, ChildNode->ParentAnchorId, ChildNode->OwnAnchorId, true);
            else
                continue;
        }
        else if (AStoneBuildingPiece* ChildStone = Cast<AStoneBuildingPiece>(ChildActor))
        {
            if (AStoneBuildingPiece* ParentStone = Cast<AStoneBuildingPiece>(ParentActor))
                bRestored = ChildStone->RestoreSameMaterialAttachmentFromGraph(
                    ParentStone, ChildNode->ParentAnchorId, ChildNode->OwnAnchorId);
            else
                continue;
        }
        else if (AStickPrimitivePiece* ChildStick = Cast<AStickPrimitivePiece>(ChildActor))
        {
            if (AStickPrimitivePiece* ParentStick = Cast<AStickPrimitivePiece>(ParentActor))
                bRestored = ChildStick->RestoreSameMaterialAttachmentFromGraph(
                    ParentStick, ChildNode->ParentAnchorId, ChildNode->OwnAnchorId);
            else
                continue;
        }
        else
            continue;

        if (!bRestored)
        {
            UE_LOG(LogTemp, Error,
                TEXT("Structural graph restored but same-material actor link reconstruction failed for node %s."),
                *NodeId.ToString());
            return false;
        }
    }

    // Re-reserve each secondary contact after primary actor links are rebuilt.
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        AActor* ChildActor = Pair.Value.Actor.Get();
        IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(ChildActor);
        for (const FStructuralSupportLink& Link : Pair.Value.SecondarySupports)
        {
            const FStructuralNodeRecord* SupportNode = Nodes.Find(Link.SupportNodeId);
            AActor* SupportActor = SupportNode ? SupportNode->Actor.Get() : nullptr;
            IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(SupportActor);
            if (!ChildPiece && !SupportPiece)
                continue;
            FStructuralAnchorView ChildAnchor;
            FStructuralAnchorView SupportAnchor;
            if (!ChildPiece || !SupportPiece ||
                !ChildPiece->GetStructuralAnchor(Link.OwnAnchorId, ChildAnchor) ||
                !SupportPiece->GetStructuralAnchor(Link.SupportAnchorId, SupportAnchor) ||
                !ChildAnchor.bCanConnect || !SupportAnchor.bCanReceive ||
                ChildAnchor.bOccupied || SupportAnchor.bOccupied ||
                ChildAnchor.Tag != SupportAnchor.Tag ||
                FVector::Dist(ChildActor->GetActorTransform().TransformPosition(ChildAnchor.LocalTransform.GetLocation()),
                    SupportActor->GetActorTransform().TransformPosition(SupportAnchor.LocalTransform.GetLocation())) > 50.0f)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Structural graph restored but secondary support anchors failed reconstruction for node %s."),
                    *Pair.Key.ToString());
                return false;
            }
            if (!ChildPiece->SetStructuralAnchorOccupied(Link.OwnAnchorId, true) ||
                !SupportPiece->SetStructuralAnchorOccupied(Link.SupportAnchorId, true))
            {
                ChildPiece->SetStructuralAnchorOccupied(Link.OwnAnchorId, false);
                SupportPiece->SetStructuralAnchorOccupied(Link.SupportAnchorId, false);
                return false;
            }
        }
    }

    // A root log is considered placed only if the canonical graph says it is
    // ground-supported; loose logs remain available for physics/carry actions.
    for (TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Pair.Value.Actor.Get()))
        {
            if (!Pair.Value.ParentNodeId.IsValid())
                Log->SetPlacedState(Pair.Value.bGroundSupported);
        }
    }
    return true;
}

bool UWorldStructuralGraphSubsystem::RegisterNode(
    AActor* Actor, FGuid NodeId, EStructuralNodeKind Kind,
    bool bGroundSupported, float LocalLoad, float MaxSupportedLoad,
    float DamageThreshold)
{
    if (!IsValid(Actor) || !NodeId.IsValid() || Kind == EStructuralNodeKind::Unknown ||
        Nodes.Contains(NodeId) || !FMath::IsFinite(LocalLoad) || LocalLoad < 0.0f ||
        !FMath::IsFinite(MaxSupportedLoad) || MaxSupportedLoad <= 0.0f ||
        !FMath::IsFinite(DamageThreshold) || DamageThreshold <= 0.0f)
    {
        return false;
    }

    FStructuralNodeRecord Record;
    Record.NodeId = NodeId;
    Record.Actor = Actor;
    Record.Kind = Kind;
    Record.bGroundSupported = bGroundSupported;
    Record.LocalLoad = LocalLoad;
    Record.MaxSupportedLoad = MaxSupportedLoad;
    Record.DamageThreshold = DamageThreshold;
    Nodes.Add(NodeId, Record);
    return true;
}

bool UWorldStructuralGraphSubsystem::UnregisterNode(
    FGuid NodeId, bool bRequireNoDependents)
{
    FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node)
        return false;

    const TArray<FGuid> Dependents = GetDirectDependents(NodeId);
    if (bRequireNoDependents && !GetAllDependents(NodeId).IsEmpty())
        return false;

    for (const FGuid& ChildId : Dependents)
    {
        const FStructuralNodeRecord* Child = Nodes.Find(ChildId);
        if (!Child || Child->Revision == MAX_int64)
            return false;
    }

    // Preflight the ancestor chain before mutating any edge/revision.
    TArray<FGuid> AncestorIds;
    TSet<FGuid> Visited;
    FGuid CurrentId = Node->ParentNodeId;
    while (CurrentId.IsValid())
    {
        if (Visited.Contains(CurrentId))
            return false;
        Visited.Add(CurrentId);
        const FStructuralNodeRecord* Current = Nodes.Find(CurrentId);
        if (!Current || Current->Revision == MAX_int64)
            return false;
        AncestorIds.Add(CurrentId);
        CurrentId = Current->ParentNodeId;
    }

    // If a caller explicitly permits removal with dependents, a child with
    // another contact keeps standing on it; otherwise its edge is detached and
    // it becomes unsupported rather than keeping a dangling ID.
    for (const FGuid& ChildId : Dependents)
    {
        if (FStructuralNodeRecord* Child = Nodes.Find(ChildId))
        {
            Child->SecondarySupports.RemoveAll([&NodeId](const FStructuralSupportLink& Link)
            {
                return Link.SupportNodeId == NodeId;
            });
            if (!Child->SecondarySupports.IsEmpty())
            {
                const FStructuralSupportLink Promoted = Child->SecondarySupports[0];
                Child->SecondarySupports.RemoveAt(0);
                Child->ParentNodeId = Promoted.SupportNodeId;
                Child->ParentAnchorId = Promoted.SupportAnchorId;
                Child->OwnAnchorId = Promoted.OwnAnchorId;
            }
            else
            {
                Child->ParentNodeId.Invalidate();
                Child->ParentAnchorId = NAME_None;
                Child->OwnAnchorId = NAME_None;
            }
            Child->bGroundSupported = false;
            ++Child->Revision;
        }
    }
    for (TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        const int32 Removed = Pair.Value.SecondarySupports.RemoveAll(
            [&NodeId](const FStructuralSupportLink& Link) { return Link.SupportNodeId == NodeId; });
        if (Removed > 0 && Pair.Value.Revision < MAX_int64)
            ++Pair.Value.Revision;
    }

    for (const FGuid& AncestorId : AncestorIds)
    {
        if (FStructuralNodeRecord* Ancestor = Nodes.Find(AncestorId))
            ++Ancestor->Revision;
    }

    Nodes.Remove(NodeId);
    return true;
}

bool UWorldStructuralGraphSubsystem::TryConnect(
    FGuid ParentNodeId, FName ParentAnchorId,
    FGuid ChildNodeId, FName OwnAnchorId)
{
    FStructuralNodeRecord* Parent = Nodes.Find(ParentNodeId);
    FStructuralNodeRecord* Child = Nodes.Find(ChildNodeId);
    if (!Parent || !Child || ParentNodeId == ChildNodeId ||
        ParentAnchorId.IsNone() || OwnAnchorId.IsNone() ||
        Child->ParentNodeId.IsValid() || !GetAllDependents(ChildNodeId).IsEmpty() ||
        !Parent->Actor.IsValid() || !Child->Actor.IsValid() ||
        WouldCreateCycle(ParentNodeId, ChildNodeId) ||
        !HasValidSupportPath(ParentNodeId) ||
        Child->Revision == MAX_int64)
    {
        return false;
    }

    // Enforce one edge per physical anchor even when a caller bypasses the
    // actor-level snap-point reservation API.
    if (IsAnchorInUse(ParentNodeId, ParentAnchorId) || IsAnchorInUse(ChildNodeId, OwnAnchorId))
        return false;

    TArray<FGuid> AncestorIds;
    TSet<FGuid> AncestorsVisited;
    FGuid AncestorId = ParentNodeId;
    while (AncestorId.IsValid())
    {
        if (AncestorsVisited.Contains(AncestorId)) return false;
        AncestorsVisited.Add(AncestorId);
        const FStructuralNodeRecord* Ancestor = Nodes.Find(AncestorId);
        if (!Ancestor || !Ancestor->Actor.IsValid() || Ancestor->Revision == MAX_int64)
            return false;
        AncestorIds.Add(AncestorId);
        AncestorId = Ancestor->ParentNodeId;
    }

    const bool bPreviousGroundSupported = Child->bGroundSupported;
    Child->ParentNodeId = ParentNodeId;
    Child->ParentAnchorId = ParentAnchorId;
    Child->OwnAnchorId = OwnAnchorId;
    Child->bGroundSupported = false;

    // Validate the full aggregate load chain before advancing any revision.
    for (const FGuid& Id : AncestorIds)
    {
        const FStructuralNodeRecord* Ancestor = Nodes.Find(Id);
        if (!Ancestor)
        {
            Child->ParentNodeId.Invalidate();
            Child->ParentAnchorId = NAME_None;
            Child->OwnAnchorId = NAME_None;
            Child->bGroundSupported = bPreviousGroundSupported;
            return false;
        }
        TSet<FGuid> Visiting;
        bool bLoadValid = true;
        const float Aggregate = ComputeAggregateLoad(Id, Visiting, bLoadValid);
        if (!bLoadValid || Aggregate > Ancestor->MaxSupportedLoad)
        {
            Child->ParentNodeId.Invalidate();
            Child->ParentAnchorId = NAME_None;
            Child->OwnAnchorId = NAME_None;
            Child->bGroundSupported = bPreviousGroundSupported;
            return false;
        }
    }

    // Every ancestor's aggregate load changed, so its revision advances too.
    for (const FGuid& Id : AncestorIds)
    {
        if (FStructuralNodeRecord* Ancestor = Nodes.Find(Id))
            ++Ancestor->Revision;
    }
    ++Child->Revision;
    return true;
}

bool UWorldStructuralGraphSubsystem::TryDisconnect(FGuid ChildNodeId)
{
    FStructuralNodeRecord* Child = Nodes.Find(ChildNodeId);
    if (!Child || !Child->ParentNodeId.IsValid() || Child->Revision == MAX_int64)
        return false;

    // The child is being detached from every support link. Reject first if a
    // dependent would lose its only healthy path to the ground.
    for (const FGuid& DependentId : GetAllDependents(ChildNodeId))
    {
        TSet<FGuid> SupportVisiting;
        if (!HasValidSupportPathInternal(DependentId, ChildNodeId, SupportVisiting))
            return false;
    }

    const FGuid OldParentId = Child->ParentNodeId;
    TArray<FGuid> AncestorIds;
    TSet<FGuid> Visited;
    FGuid CurrentId = OldParentId;
    while (CurrentId.IsValid())
    {
        if (Visited.Contains(CurrentId)) return false;
        Visited.Add(CurrentId);
        const FStructuralNodeRecord* Current = Nodes.Find(CurrentId);
        if (!Current || Current->Revision == MAX_int64) return false;
        AncestorIds.Add(CurrentId);
        CurrentId = Current->ParentNodeId;
    }

    // Physical secondary-support anchor reservations must be released with
    // the canonical edges. Logical-only nodes are allowed, but half-adapted
    // links are rejected before any mutation.
    IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(Child->Actor.Get());
    TArray<FStructuralSupportLink> ReservationsToRelease;
    for (const FStructuralSupportLink& Link : Child->SecondarySupports)
    {
        const FStructuralNodeRecord* Support = Nodes.Find(Link.SupportNodeId);
        IStructuralPieceInterface* SupportPiece = Support
            ? Cast<IStructuralPieceInterface>(Support->Actor.Get()) : nullptr;
        FStructuralAnchorView ChildAnchor;
        FStructuralAnchorView SupportAnchor;
        const bool bHasPhysicalAdapters = ChildPiece || SupportPiece;
        if (!Support || Support->Revision == MAX_int64 ||
            (bHasPhysicalAdapters &&
             (!ChildPiece || !SupportPiece ||
              !ChildPiece->GetStructuralAnchor(Link.OwnAnchorId, ChildAnchor) ||
              !SupportPiece->GetStructuralAnchor(Link.SupportAnchorId, SupportAnchor))))
            return false;
        if (bHasPhysicalAdapters)
            ReservationsToRelease.Add(Link);
    }

    TArray<FStructuralSupportLink> ReleasedReservations;
    for (const FStructuralSupportLink& Link : ReservationsToRelease)
    {
        const FStructuralNodeRecord* Support = Nodes.Find(Link.SupportNodeId);
        IStructuralPieceInterface* SupportPiece = Support
            ? Cast<IStructuralPieceInterface>(Support->Actor.Get()) : nullptr;
        if (!ChildPiece->SetStructuralAnchorOccupied(Link.OwnAnchorId, false))
        {
            for (const FStructuralSupportLink& Restore : ReleasedReservations)
            {
                const FStructuralNodeRecord* RestoreSupport = Nodes.Find(Restore.SupportNodeId);
                if (IStructuralPieceInterface* RestorePiece = RestoreSupport
                    ? Cast<IStructuralPieceInterface>(RestoreSupport->Actor.Get()) : nullptr)
                {
                    ChildPiece->SetStructuralAnchorOccupied(Restore.OwnAnchorId, true);
                    RestorePiece->SetStructuralAnchorOccupied(Restore.SupportAnchorId, true);
                }
            }
            return false;
        }
        if (!SupportPiece->SetStructuralAnchorOccupied(Link.SupportAnchorId, false))
        {
            ChildPiece->SetStructuralAnchorOccupied(Link.OwnAnchorId, true);
            for (const FStructuralSupportLink& Restore : ReleasedReservations)
            {
                const FStructuralNodeRecord* RestoreSupport = Nodes.Find(Restore.SupportNodeId);
                if (IStructuralPieceInterface* RestorePiece = RestoreSupport
                    ? Cast<IStructuralPieceInterface>(RestoreSupport->Actor.Get()) : nullptr)
                {
                    ChildPiece->SetStructuralAnchorOccupied(Restore.OwnAnchorId, true);
                    RestorePiece->SetStructuralAnchorOccupied(Restore.SupportAnchorId, true);
                }
            }
            return false;
        }
        ReleasedReservations.Add(Link);
    }

    for (const FStructuralSupportLink& Link : Child->SecondarySupports)
    {
        if (FStructuralNodeRecord* Support = Nodes.Find(Link.SupportNodeId))
            ++Support->Revision;
    }
    Child->SecondarySupports.Reset();
    Child->ParentNodeId.Invalidate();
    Child->ParentAnchorId = NAME_None;
    Child->OwnAnchorId = NAME_None;
    Child->bGroundSupported = false;
    ++Child->Revision;
    for (const FGuid& Id : AncestorIds)
    {
        if (FStructuralNodeRecord* Ancestor = Nodes.Find(Id))
            ++Ancestor->Revision;
    }
    return true;
}

bool UWorldStructuralGraphSubsystem::IsAnchorInUse(FGuid SupportNodeId, FName SupportAnchorId) const
{
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        if (Pair.Value.ParentNodeId == SupportNodeId && Pair.Value.ParentAnchorId == SupportAnchorId)
            return true;
        for (const FStructuralSupportLink& Link : Pair.Value.SecondarySupports)
        {
            if (Link.SupportNodeId == SupportNodeId && Link.SupportAnchorId == SupportAnchorId)
                return true;
        }
    }
    return false;
}

bool UWorldStructuralGraphSubsystem::TryAddSecondarySupport(
    FGuid ChildNodeId, FGuid SupportNodeId, FName SupportAnchorId, FName OwnAnchorId)
{
    FStructuralNodeRecord* Child = Nodes.Find(ChildNodeId);
    FStructuralNodeRecord* Support = Nodes.Find(SupportNodeId);
    if (!Child || !Support || ChildNodeId == SupportNodeId ||
        !Child->ParentNodeId.IsValid() || Child->ParentNodeId == SupportNodeId ||
        SupportAnchorId.IsNone() || OwnAnchorId.IsNone() || OwnAnchorId == Child->OwnAnchorId ||
        !Child->Actor.IsValid() || !Support->Actor.IsValid() ||
        Child->Revision == MAX_int64 || Support->Revision == MAX_int64 ||
        IsAnchorInUse(SupportNodeId, SupportAnchorId) ||
        IsAnchorInUse(ChildNodeId, OwnAnchorId) ||
        WouldCreateCycle(SupportNodeId, ChildNodeId) ||
        !HasValidSupportPath(SupportNodeId))
    {
        return false;
    }
    for (const FStructuralSupportLink& ExistingLink : Child->SecondarySupports)
    {
        if (ExistingLink.SupportNodeId == SupportNodeId || ExistingLink.OwnAnchorId == OwnAnchorId)
            return false;
    }

    AActor* ChildActor = Child->Actor.Get();
    AActor* SupportActor = Support->Actor.Get();
    IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(ChildActor);
    IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(SupportActor);
    FStructuralAnchorView ChildAnchor;
    FStructuralAnchorView SupportAnchor;
    const bool bHasPhysicalAdapters = ChildPiece || SupportPiece;
    if (AStoneBuildingPiece* Bridge = Cast<AStoneBuildingPiece>(ChildActor))
    {
        if (!Bridge->CanAcceptAdditionalSupport(OwnAnchorId))
            return false;
    }
    if (bHasPhysicalAdapters &&
        (!ChildPiece || !SupportPiece ||
         !ChildPiece->GetStructuralAnchor(OwnAnchorId, ChildAnchor) ||
         !SupportPiece->GetStructuralAnchor(SupportAnchorId, SupportAnchor) ||
         !ChildAnchor.bCanConnect || !SupportAnchor.bCanReceive ||
         ChildAnchor.bOccupied || SupportAnchor.bOccupied ||
         ChildAnchor.Tag != SupportAnchor.Tag ||
         FVector::Dist(ChildActor->GetActorTransform().TransformPosition(ChildAnchor.LocalTransform.GetLocation()),
             SupportActor->GetActorTransform().TransformPosition(SupportAnchor.LocalTransform.GetLocation())) > 50.0f))
        return false;

    FStructuralSupportLink Link;
    Link.SupportNodeId = SupportNodeId;
    Link.SupportAnchorId = SupportAnchorId;
    Link.OwnAnchorId = OwnAnchorId;
    Child->SecondarySupports.Add(Link);

    // Sharing the load changes every support's aggregate; reject overloads.
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        TSet<FGuid> Visiting;
        bool bLoadValid = true;
        const float Aggregate = ComputeAggregateLoad(Pair.Key, Visiting, bLoadValid);
        if (!bLoadValid || Aggregate > Pair.Value.MaxSupportedLoad)
        {
            Child = Nodes.Find(ChildNodeId);
            if (Child)
                Child->SecondarySupports.Pop();
            return false;
        }
    }

    // The canonical link and both physical anchor reservations must agree.
    // Logical-only test nodes may omit both adapters, but a half-adapted edge is invalid.
    if (bHasPhysicalAdapters)
    {
        if (!ChildPiece->SetStructuralAnchorOccupied(OwnAnchorId, true))
        {
            Child = Nodes.Find(ChildNodeId);
            if (Child)
                Child->SecondarySupports.Pop();
            return false;
        }
        if (!SupportPiece->SetStructuralAnchorOccupied(SupportAnchorId, true))
        {
            ChildPiece->SetStructuralAnchorOccupied(OwnAnchorId, false);
            Child = Nodes.Find(ChildNodeId);
            if (Child)
                Child->SecondarySupports.Pop();
            return false;
        }
    }

    ++Child->Revision;
    ++Support->Revision;
    return true;
}

bool UWorldStructuralGraphSubsystem::TryRemoveSecondarySupport(FGuid ChildNodeId, FGuid SupportNodeId)
{
    FStructuralNodeRecord* Child = Nodes.Find(ChildNodeId);
    FStructuralNodeRecord* Support = Nodes.Find(SupportNodeId);
    if (!Child || !Support || Child->Revision == MAX_int64 ||
        Support->Revision == MAX_int64)
        return false;

    const FStructuralSupportLink* ExistingLink = Child->SecondarySupports.FindByPredicate(
        [&SupportNodeId](const FStructuralSupportLink& Link) { return Link.SupportNodeId == SupportNodeId; });
    if (!ExistingLink)
        return false;
    const FStructuralSupportLink RemovedLink = *ExistingLink;
    const TArray<FStructuralSupportLink> PreviousSupports = Child->SecondarySupports;
    Child->SecondarySupports.RemoveAll(
        [&SupportNodeId](const FStructuralSupportLink& Link) { return Link.SupportNodeId == SupportNodeId; });

    // Removing a brace is not allowed to leave a still-registered piece
    // unsupported or overload any remaining support.
    if (!HasValidSupportPath(ChildNodeId))
    {
        Child->SecondarySupports = PreviousSupports;
        return false;
    }

    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        TSet<FGuid> Visiting;
        bool bLoadValid = true;
        const float Aggregate = ComputeAggregateLoad(Pair.Key, Visiting, bLoadValid);
        if (!bLoadValid || Aggregate > Pair.Value.MaxSupportedLoad)
        {
            Child = Nodes.Find(ChildNodeId);
            if (Child)
                Child->SecondarySupports = PreviousSupports;
            return false;
        }
    }

    IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(Child->Actor.Get());
    IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(Support->Actor.Get());
    FStructuralAnchorView ChildAnchor;
    FStructuralAnchorView SupportAnchor;
    const bool bHasPhysicalAdapters = ChildPiece || SupportPiece;
    if (bHasPhysicalAdapters &&
        (!ChildPiece || !SupportPiece ||
         !ChildPiece->GetStructuralAnchor(RemovedLink.OwnAnchorId, ChildAnchor) ||
         !SupportPiece->GetStructuralAnchor(RemovedLink.SupportAnchorId, SupportAnchor)))
    {
        Child->SecondarySupports = PreviousSupports;
        return false;
    }

    if (bHasPhysicalAdapters)
    {
        if (!ChildPiece->SetStructuralAnchorOccupied(RemovedLink.OwnAnchorId, false))
        {
            Child->SecondarySupports = PreviousSupports;
            return false;
        }
        if (!SupportPiece->SetStructuralAnchorOccupied(RemovedLink.SupportAnchorId, false))
        {
            ChildPiece->SetStructuralAnchorOccupied(RemovedLink.OwnAnchorId, true);
            Child->SecondarySupports = PreviousSupports;
            return false;
        }
    }

    ++Child->Revision;
    ++Support->Revision;
    return true;
}

TArray<FGuid> UWorldStructuralGraphSubsystem::GetAllDependents(FGuid NodeId) const
{
    TArray<FGuid> Result;
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        bool bDepends = Pair.Value.ParentNodeId == NodeId;
        for (const FStructuralSupportLink& Link : Pair.Value.SecondarySupports)
            bDepends |= Link.SupportNodeId == NodeId;
        if (bDepends)
            Result.Add(Pair.Key);
    }
    return Result;
}

bool UWorldStructuralGraphSubsystem::SetGroundSupported(FGuid NodeId, bool bGroundSupported)
{
    FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node || Node->Revision == MAX_int64 || (bGroundSupported && Node->ParentNodeId.IsValid()))
    {
        return false;
    }
    if (Node->bGroundSupported != bGroundSupported)
    {
        Node->bGroundSupported = bGroundSupported;
        ++Node->Revision;
    }
    return true;
}

bool UWorldStructuralGraphSubsystem::HasValidSupportPath(FGuid NodeId) const
{
    TSet<FGuid> Visiting;
    return HasValidSupportPathInternal(NodeId, FGuid(), Visiting);
}

bool UWorldStructuralGraphSubsystem::HasValidSupportPathInternal(
    FGuid NodeId, FGuid ExcludedNodeId, TSet<FGuid>& Visiting) const
{
    const FStructuralNodeRecord* Current = Nodes.Find(NodeId);
    if (!Current || NodeId == ExcludedNodeId || Visiting.Contains(NodeId) ||
        !Current->Actor.IsValid() || Current->Damage >= Current->DamageThreshold)
    {
        return false;
    }
    if (Current->bGroundSupported)
        return true;

    Visiting.Add(NodeId);
    bool bSupported = Current->ParentNodeId.IsValid() &&
        HasValidSupportPathInternal(Current->ParentNodeId, ExcludedNodeId, Visiting);
    for (int32 Index = 0; !bSupported && Index < Current->SecondarySupports.Num(); ++Index)
    {
        bSupported = HasValidSupportPathInternal(
            Current->SecondarySupports[Index].SupportNodeId, ExcludedNodeId, Visiting);
    }
    Visiting.Remove(NodeId);
    return bSupported;
}

bool UWorldStructuralGraphSubsystem::CanSafelyRemove(FGuid NodeId) const
{
    if (!Nodes.Contains(NodeId))
        return false;
    // Removal is safe when every piece resting on this node still reaches
    // ground through another contact.
    for (const FGuid& DependentId : GetAllDependents(NodeId))
    {
        TSet<FGuid> Visiting;
        if (!HasValidSupportPathInternal(DependentId, NodeId, Visiting))
            return false;
    }
    return true;
}

bool UWorldStructuralGraphSubsystem::BuildDependentRemovalOrder(
    FGuid RootNodeId, TArray<FGuid>& OutLeafFirstOrder) const
{
    OutLeafFirstOrder.Reset();
    if (!Nodes.Contains(RootNodeId)) return false;

    TSet<FGuid> Visited;
    if (!CollectDependentsDepthFirst(RootNodeId, Visited, OutLeafFirstOrder))
    {
        OutLeafFirstOrder.Reset();
        return false;
    }

    // No partial graph/actor mutation is performed here. The caller receives
    // a stable leaf-first plan and must commit actor + graph removal together.
    for (const FGuid& Id : OutLeafFirstOrder)
    {
        const FStructuralNodeRecord* Node = Nodes.Find(Id);
        if (!Node || Node->Revision == MAX_int64)
        {
            OutLeafFirstOrder.Reset();
            return false;
        }
    }
    return true;
}

bool UWorldStructuralGraphSubsystem::CollectDependentsDepthFirst(
    FGuid NodeId, TSet<FGuid>& Visited, TArray<FGuid>& OutOrder) const
{
    const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node || Visited.Contains(NodeId)) return false;
    Visited.Add(NodeId);

    TArray<FGuid> Children = GetAllDependents(NodeId);
    Children.Sort([](const FGuid& A, const FGuid& B)
    {
        return A.ToString() < B.ToString();
    });
    for (const FGuid& ChildId : Children)
    {
        // A child that still reaches ground through another contact does not
        // collapse with this node.
        TSet<FGuid> SupportVisiting;
        if (HasValidSupportPathInternal(ChildId, NodeId, SupportVisiting))
            continue;
        if (!CollectDependentsDepthFirst(ChildId, Visited, OutOrder)) return false;
    }
    OutOrder.Add(NodeId);
    return true;
}

float UWorldStructuralGraphSubsystem::GetAggregateLoad(FGuid NodeId) const
{
    TSet<FGuid> Visiting;
    bool bValid = true;
    const float Result = ComputeAggregateLoad(NodeId, Visiting, bValid);
    return bValid ? Result : -1.0f;
}

float UWorldStructuralGraphSubsystem::ComputeAggregateLoad(
    FGuid NodeId, TSet<FGuid>& Visiting, bool& bOutValid) const
{
    const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node || Visiting.Contains(NodeId))
    {
        bOutValid = false;
        return 0.0f;
    }

    Visiting.Add(NodeId);
    float Total = Node->LocalLoad;
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        bool bSupportsChild = Pair.Value.ParentNodeId == NodeId;
        for (const FStructuralSupportLink& Link : Pair.Value.SecondarySupports)
            bSupportsChild |= Link.SupportNodeId == NodeId;
        if (bSupportsChild)
        {
            // A piece resting on several supports shares its load equally.
            const float ChildLoad = ComputeAggregateLoad(Pair.Key, Visiting, bOutValid);
            Total += ChildLoad / static_cast<float>(1 + Pair.Value.SecondarySupports.Num());
            if (!bOutValid)
            {
                Visiting.Remove(NodeId);
                return 0.0f;
            }
        }
    }
    Visiting.Remove(NodeId);
    if (!FMath::IsFinite(Total))
    {
        bOutValid = false;
        return 0.0f;
    }
    return Total;
}

bool UWorldStructuralGraphSubsystem::RepairDamage(FGuid NodeId, float Amount)
{
    FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node || Node->Revision == MAX_int64 || !FMath::IsFinite(Amount) || Amount <= 0.0f || Node->Damage <= 0.0f)
        return false;
    Node->Damage = FMath::Max(0.0f, Node->Damage - Amount);
    ++Node->Revision;
    return true;
}

bool UWorldStructuralGraphSubsystem::TryRepairDamageWithInventory(
    FGuid NodeId, FName RepairId, int64 ExpectedNodeRevision,
    UInventoryCraftingComponent* Inventory)
{
    if (!NodeId.IsValid() || RepairId.IsNone() || ExpectedNodeRevision <= 0 ||
        !IsValid(Inventory) || !GetWorld())
        return false;

    const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    const UWorldGameDataSubsystem* GameData =
        GetWorld()->GetSubsystem<UWorldGameDataSubsystem>();
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    const FGameDataStructuralRepair* Repair =
        GameData ? GameData->FindStructuralRepair(RepairId) : nullptr;
    if (!Node || !Node->Actor.IsValid() || Node->Revision != ExpectedNodeRevision ||
        Node->Revision == MAX_int64 || Node->Damage <= 0.0f ||
        !GameData || !GameData->IsLoaded() || !Ownership || !Repair)
        return false;

    FName TargetKind = NAME_None;
    switch (Node->Kind)
    {
    case EStructuralNodeKind::Wood: TargetKind = TEXT("Wood"); break;
    case EStructuralNodeKind::Stone: TargetKind = TEXT("Stone"); break;
    case EStructuralNodeKind::Stick: TargetKind = TEXT("Stick"); break;
    default: return false;
    }
    const FGameDataItem* Material = GameData->GetItems().Find(Repair->MaterialItemId);
    const FGameDataItem* Tool = GameData->GetItems().Find(Repair->ToolItemId);
    if (Repair->TargetKind != TargetKind || !Material || !Tool ||
        Tool->Category != TEXT("Tool") || Repair->MaterialUnits <= 0 ||
        Repair->RepairAmount <= 0.0f ||
        Inventory->GetItemCount(Repair->MaterialItemId) < Repair->MaterialUnits ||
        Inventory->GetItemCount(Repair->ToolItemId) < 1)
        return false;

    FGuid InventoryActorId;
    FGuid InventoryOwnerId;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(InventoryActorId, InventoryOwnerId,
        InventoryItemsBefore, QuickAccessBefore);
    if (!InventoryOwnerId.IsValid() || InventoryOwnerId != Inventory->GetInventoryOwnerId())
        return false;
    (void)InventoryActorId;

    TArray<FWorldOwnedItemRecord> OwnershipItemsBefore;
    TArray<FWorldItemTransferReceipt> OwnershipTransfersBefore;
    TArray<FWorldItemSplitReceipt> OwnershipSplitsBefore;
    Ownership->ExportSnapshot(OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);

    if (!Inventory->TryRemoveItem(Repair->MaterialItemId, Repair->MaterialUnits))
        return false;

    if (RepairDamage(NodeId, Repair->RepairAmount))
        return true;

    // Restore the authoritative ledger before inventory rows so unique consumed
    // IDs can be validated against their original owner when the snapshot returns.
    const bool bOwnershipRestored = Ownership->RestoreSnapshot(
        OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);
    const bool bInventoryRestored = Inventory->RestoreSnapshot(
        InventoryOwnerId, InventoryItemsBefore, QuickAccessBefore);
    if (!bOwnershipRestored || !bInventoryRestored)
        UE_LOG(LogTemp, Error, TEXT("Structural repair rollback failed; item ownership and inventory require recovery."));
    return false;
}

bool UWorldStructuralGraphSubsystem::TryDismantleStandaloneNodeToInventory(
    FGuid NodeId, int64 ExpectedNodeRevision, UInventoryCraftingComponent* Inventory)
{
    if (!NodeId.IsValid() || ExpectedNodeRevision <= 0 ||
        !IsValid(Inventory) || !GetWorld())
        return false;

    FStructuralNodeRecord Node;
    if (!GetNode(NodeId, Node) || !Node.Actor.IsValid() ||
        Node.Revision != ExpectedNodeRevision || Node.Revision == MAX_int64 ||
        Node.ParentNodeId.IsValid() || !Node.SecondarySupports.IsEmpty() ||
        !GetAllDependents(NodeId).IsEmpty())
        return false;

    AActor* Actor = Node.Actor.Get();
    UWorldObjectIdentityComponent* Identity =
        Actor ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldGameDataSubsystem* GameData =
        GetWorld()->GetSubsystem<UWorldGameDataSubsystem>();
    const FName KindName = Node.Kind == EStructuralNodeKind::Wood ? FName(TEXT("Wood")) :
        Node.Kind == EStructuralNodeKind::Stone ? FName(TEXT("Stone")) :
        Node.Kind == EStructuralNodeKind::Stick ? FName(TEXT("Stick")) : NAME_None;
    const FGameDataStructuralRepair* Recovery = GameData
        ? GameData->GetStructuralRepairs().FindByPredicate(
            [&KindName](const FGameDataStructuralRepair& Candidate)
            { return Candidate.TargetKind == KindName; })
        : nullptr;
    if (!Identity || !Identity->HasValidIdentity() || Identity->GetInstanceId() != NodeId ||
        !Ownership || !GameData || !GameData->IsLoaded() || !Recovery ||
        Recovery->RecoveryItemId.IsNone() || Recovery->RecoveryUnits <= 0 ||
        (Identity->GetLifecycleState() != EWorldObjectLifecycleState::World &&
         Identity->GetLifecycleState() != EWorldObjectLifecycleState::Placed))
        return false;

    FWorldOwnedItemRecord SourceItem;
    if (!Ownership->TryGetItem(NodeId, SourceItem) ||
        SourceItem.ItemId != NodeId || SourceItem.OwnerKind != EWorldItemOwnerKind::World ||
        SourceItem.Quantity != 1 || SourceItem.Revision == MAX_int64)
        return false;

    const FGameDataItem* RecoveryDefinition = GameData->GetItems().Find(Recovery->RecoveryItemId);
    if (!RecoveryDefinition || RecoveryDefinition->bConsumable ||
        !GameData->ApplyToInventory(Inventory))
        return false;

    FInventoryStack Recovered;
    Recovered.ItemId = Recovery->RecoveryItemId;
    Recovered.Quantity = Recovery->RecoveryUnits;
    Recovered.Category = RecoveryDefinition->Category == TEXT("Tool")
        ? EInventoryItemCategory::Tool
        : RecoveryDefinition->Category == TEXT("BuildingPart")
            ? EInventoryItemCategory::BuildingPart
            : RecoveryDefinition->Category == TEXT("Weapon")
                ? EInventoryItemCategory::Weapon : EInventoryItemCategory::Resource;
    Recovered.bIsTool = Recovered.Category == EInventoryItemCategory::Tool;
    Recovered.MaxStack = Recovered.bIsTool ? 1 : 99;
    if (Recovered.Quantity > Recovered.MaxStack || !Inventory->CanAddItems({Recovered}))
        return false;

    const EWorldObjectLifecycleState PreviousLifecycle = Identity->GetLifecycleState();
    FGuid InventoryActorId;
    FGuid InventoryOwnerId;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(InventoryActorId, InventoryOwnerId,
        InventoryItemsBefore, QuickAccessBefore);
    if (!InventoryOwnerId.IsValid() || InventoryOwnerId != Inventory->GetInventoryOwnerId())
        return false;
    (void)InventoryActorId;

    TArray<FWorldOwnedItemRecord> OwnershipItemsBefore;
    TArray<FWorldItemTransferReceipt> OwnershipTransfersBefore;
    TArray<FWorldItemSplitReceipt> OwnershipSplitsBefore;
    Ownership->ExportSnapshot(OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);
    TArray<FWorldStructuralNodeSnapshot> GraphBefore;
    if (!ExportSnapshot(GraphBefore))
        return false;

    auto Rollback = [this, Ownership, Inventory, &OwnershipItemsBefore,
        &OwnershipTransfersBefore, &OwnershipSplitsBefore, &InventoryOwnerId,
        &InventoryItemsBefore, &QuickAccessBefore, &GraphBefore]()
    {
        const bool bItemsRestored = Ownership->RestoreSnapshot(
            OwnershipItemsBefore, OwnershipTransfersBefore, OwnershipSplitsBefore);
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            InventoryOwnerId, InventoryItemsBefore, QuickAccessBefore);
        const bool bGraphRestored = RestoreSnapshot(GraphBefore);
        if (!bItemsRestored || !bInventoryRestored || !bGraphRestored)
            UE_LOG(LogTemp, Error, TEXT("Structural dismantle rollback failed; world state requires recovery."));
        return false;
    };

    FWorldOwnedItemRecord ConsumedItem;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), NodeId, SourceItem.Revision,
        EWorldItemOwnerKind::World, EWorldItemOwnerKind::Consumed,
        FGuid(), SourceItem.WorldLocation, ConsumedItem))
        return false;

    if (!UnregisterNode(NodeId, true))
        return Rollback();

    if (!Inventory->TryAddItem(Recovered))
        return Rollback();

    if (!Identity->TryTransitionLifecycle(PreviousLifecycle, EWorldObjectLifecycleState::Destroyed))
        return Rollback();

    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);
    Actor->SetActorTickEnabled(false);
    if (!Actor->Destroy())
        UE_LOG(LogTemp, Warning, TEXT("Dismantled actor %s stayed inert after its world ownership was consumed."), *GetNameSafe(Actor));
    return true;
}

bool UWorldStructuralGraphSubsystem::TryDismantleConnectedStructureToInventory(
    FGuid RootNodeId, int64 ExpectedRootRevision, UInventoryCraftingComponent* Inventory)
{
    return TryRemoveConnectedStructure(RootNodeId, ExpectedRootRevision, Inventory, true);
}

bool UWorldStructuralGraphSubsystem::TryCollapseConnectedStructure(
    FGuid RootNodeId, int64 ExpectedRootRevision)
{
    return TryRemoveConnectedStructure(RootNodeId, ExpectedRootRevision, nullptr, false);
}

bool UWorldStructuralGraphSubsystem::TryRemoveConnectedStructure(
    FGuid RootNodeId, int64 ExpectedRootRevision, UInventoryCraftingComponent* Inventory,
    bool bReturnSalvage)
{
    if (!RootNodeId.IsValid() || ExpectedRootRevision <= 0 || !GetWorld() ||
        (bReturnSalvage && !IsValid(Inventory)) || (!bReturnSalvage && Inventory))
        return false;

    FStructuralNodeRecord Root;
    if (!GetNode(RootNodeId, Root) || Root.Revision != ExpectedRootRevision ||
        Root.Revision == MAX_int64 || !Root.Actor.IsValid())
        return false;

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldGameDataSubsystem* GameData =
        bReturnSalvage ? GetWorld()->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!Ownership || (bReturnSalvage && (!GameData || !GameData->IsLoaded())))
        return false;
    if (bReturnSalvage && !GameData->ApplyToInventory(Inventory))
        return false;

    TArray<FGuid> LeafFirstOrder;
    if (!BuildDependentRemovalOrder(RootNodeId, LeafFirstOrder) ||
        LeafFirstOrder.IsEmpty() || LeafFirstOrder.Last() != RootNodeId)
        return false;

    TSet<FGuid> RemovalSet;
    for (const FGuid& NodeId : LeafFirstOrder)
    {
        if (!NodeId.IsValid() || RemovalSet.Contains(NodeId))
            return false;
        RemovalSet.Add(NodeId);
    }

    for (const FGuid& NodeId : LeafFirstOrder)
    {
        const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
        if (!Node || !Node->Actor.IsValid() || Node->Revision >= MAX_int64 - LeafFirstOrder.Num() - 2)
            return false;
    }

    TSet<FGuid> AffectedSurvivorIds;
    for (const FGuid& NodeId : LeafFirstOrder)
        for (const FGuid& DependentId : GetAllDependents(NodeId))
            if (!RemovalSet.Contains(DependentId))
                AffectedSurvivorIds.Add(DependentId);

    TFunction<bool(FGuid, TSet<FGuid>&)> HasPathWithoutRemoval;
    HasPathWithoutRemoval = [this, &RemovalSet, &HasPathWithoutRemoval](
        FGuid CurrentId, TSet<FGuid>& Visiting) -> bool
    {
        if (!CurrentId.IsValid() || RemovalSet.Contains(CurrentId) || Visiting.Contains(CurrentId))
            return false;
        const FStructuralNodeRecord* Current = Nodes.Find(CurrentId);
        if (!Current || !Current->Actor.IsValid() || Current->Damage >= Current->DamageThreshold)
            return false;
        if (Current->bGroundSupported)
            return true;
        Visiting.Add(CurrentId);
        bool bSupported = Current->ParentNodeId.IsValid() &&
            HasPathWithoutRemoval(Current->ParentNodeId, Visiting);
        for (const FStructuralSupportLink& Link : Current->SecondarySupports)
            if (!bSupported && !RemovalSet.Contains(Link.SupportNodeId))
                bSupported = HasPathWithoutRemoval(Link.SupportNodeId, Visiting);
        Visiting.Remove(CurrentId);
        return bSupported;
    };

    TMap<FGuid, FStructuralNodeRecord> ProposedSurvivors;
    for (const FGuid& SurvivorId : AffectedSurvivorIds)
    {
        const FStructuralNodeRecord* Existing = Nodes.Find(SurvivorId);
        if (!Existing || Existing->Revision >= MAX_int64 - LeafFirstOrder.Num() - 2)
            return false;
        FStructuralNodeRecord Updated = *Existing;

        if (RemovalSet.Contains(Updated.ParentNodeId))
        {
            TArray<FStructuralSupportLink> Candidates = Updated.SecondarySupports;
            Candidates.Sort([](const FStructuralSupportLink& A, const FStructuralSupportLink& B)
            {
                return A.SupportNodeId.ToString() < B.SupportNodeId.ToString();
            });
            const FStructuralSupportLink* Promoted = nullptr;
            for (const FStructuralSupportLink& Candidate : Candidates)
            {
                if (RemovalSet.Contains(Candidate.SupportNodeId))
                    continue;
                TSet<FGuid> SupportVisiting;
                if (HasPathWithoutRemoval(Candidate.SupportNodeId, SupportVisiting))
                {
                    Promoted = &Candidate;
                    break;
                }
            }
            if (!Promoted)
                return false;
            const FGuid PromotedSupportId = Promoted->SupportNodeId;
            Updated.ParentNodeId = PromotedSupportId;
            Updated.ParentAnchorId = Promoted->SupportAnchorId;
            Updated.OwnAnchorId = Promoted->OwnAnchorId;
            Updated.SecondarySupports.RemoveAll(
                [&PromotedSupportId](const FStructuralSupportLink& Link)
                { return Link.SupportNodeId == PromotedSupportId; });
        }

        Updated.SecondarySupports.RemoveAll(
            [&RemovalSet](const FStructuralSupportLink& Link)
            { return RemovalSet.Contains(Link.SupportNodeId); });
        TSet<FGuid> SurvivorVisiting;
        if (!HasPathWithoutRemoval(SurvivorId, SurvivorVisiting))
            return false;
        ++Updated.Revision;
        ProposedSurvivors.Add(SurvivorId, Updated);
    }

    struct FRemovalEntry
    {
        FGuid NodeId;
        FStructuralNodeRecord Node;
        TWeakObjectPtr<AActor> Actor;
        TWeakObjectPtr<UWorldObjectIdentityComponent> Identity;
        EWorldObjectLifecycleState Lifecycle = EWorldObjectLifecycleState::World;
        FWorldOwnedItemRecord Item;
    };
    TArray<FRemovalEntry> Entries;
    Entries.Reserve(LeafFirstOrder.Num());
    TMap<FName, int32> SalvageQuantities;

    // Full preflight: identity/lifecycle, unique world ownership, repair profile,
    // actor adapter and both physical endpoints for every current graph edge.
    for (const FGuid& NodeId : LeafFirstOrder)
    {
        FRemovalEntry& Entry = Entries.AddDefaulted_GetRef();
        Entry.NodeId = NodeId;
        if (!GetNode(NodeId, Entry.Node))
            return false;
        AActor* Actor = Entry.Node.Actor.Get();
        Entry.Actor = Actor;
        UWorldObjectIdentityComponent* Identity =
            IsValid(Actor) ? Actor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        if (!Identity || !Identity->HasValidIdentity() || Identity->GetInstanceId() != NodeId)
            return false;
        Entry.Identity = Identity;
        Entry.Lifecycle = Identity->GetLifecycleState();
        if (Entry.Lifecycle != EWorldObjectLifecycleState::World &&
            Entry.Lifecycle != EWorldObjectLifecycleState::Placed)
            return false;

        if (!Ownership->TryGetItem(NodeId, Entry.Item) ||
            Entry.Item.ItemId != NodeId || Entry.Item.OwnerKind != EWorldItemOwnerKind::World ||
            Entry.Item.OwnerId.IsValid() || Entry.Item.Quantity != 1 ||
            Entry.Item.Revision <= 0 || Entry.Item.Revision == MAX_int64)
            return false;

        if (Entry.Node.ParentNodeId.IsValid())
        {
            const FStructuralNodeRecord* ParentNode = Nodes.Find(Entry.Node.ParentNodeId);
            if (!ParentNode || ParentNode->Revision >= MAX_int64 - LeafFirstOrder.Num() - 2)
                return false;
            IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(Actor);
            IStructuralPieceInterface* ParentPiece = Cast<IStructuralPieceInterface>(ParentNode->Actor.Get());
            if (ChildPiece || ParentPiece)
            {
                FStructuralAnchorView ChildAnchor;
                FStructuralAnchorView ParentAnchor;
                if (!ChildPiece || !ParentPiece ||
                    !ChildPiece->GetStructuralAnchor(Entry.Node.OwnAnchorId, ChildAnchor) ||
                    !ParentPiece->GetStructuralAnchor(Entry.Node.ParentAnchorId, ParentAnchor))
                    return false;
            }
        }

        for (const FStructuralSupportLink& Link : Entry.Node.SecondarySupports)
        {
            const FStructuralNodeRecord* SupportNode = Nodes.Find(Link.SupportNodeId);
            if (!SupportNode || SupportNode->Revision >= MAX_int64 - LeafFirstOrder.Num() - 2)
                return false;
            IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(Actor);
            IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(SupportNode->Actor.Get());
            if (ChildPiece || SupportPiece)
            {
                FStructuralAnchorView ChildAnchor;
                FStructuralAnchorView SupportAnchor;
                if (!ChildPiece || !SupportPiece ||
                    !ChildPiece->GetStructuralAnchor(Link.OwnAnchorId, ChildAnchor) ||
                    !SupportPiece->GetStructuralAnchor(Link.SupportAnchorId, SupportAnchor))
                    return false;
            }
        }

        if (bReturnSalvage)
        {
            const FName KindName = Entry.Node.Kind == EStructuralNodeKind::Wood ? FName(TEXT("Wood")) :
                Entry.Node.Kind == EStructuralNodeKind::Stone ? FName(TEXT("Stone")) :
                Entry.Node.Kind == EStructuralNodeKind::Stick ? FName(TEXT("Stick")) : NAME_None;
            const FGameDataStructuralRepair* Recovery = KindName.IsNone() ? nullptr :
                GameData->GetStructuralRepairs().FindByPredicate(
                    [&KindName](const FGameDataStructuralRepair& Candidate)
                    { return Candidate.TargetKind == KindName; });
            if (!Recovery || Recovery->RecoveryItemId.IsNone() ||
                Recovery->RecoveryUnits <= 0 || !GameData->GetItems().Contains(Recovery->RecoveryItemId))
                return false;
            int32& Total = SalvageQuantities.FindOrAdd(Recovery->RecoveryItemId);
            if (Total > MAX_int32 - Recovery->RecoveryUnits)
                return false;
            Total += Recovery->RecoveryUnits;
        }
    }

    TArray<FInventoryStack> SalvageStacks;
    if (bReturnSalvage)
    {
        for (const TPair<FName, int32>& Pair : SalvageQuantities)
        {
            const FGameDataItem* Definition = GameData->GetItems().Find(Pair.Key);
            if (!Definition || Definition->bConsumable || Pair.Value <= 0)
                return false;
            EInventoryItemCategory Category = EInventoryItemCategory::Resource;
            if (Definition->Category == TEXT("Tool")) Category = EInventoryItemCategory::Tool;
            else if (Definition->Category == TEXT("BuildingPart")) Category = EInventoryItemCategory::BuildingPart;
            else if (Definition->Category == TEXT("Weapon")) Category = EInventoryItemCategory::Weapon;
            int32 Remaining = Pair.Value;
            while (Remaining > 0)
            {
                FInventoryStack Stack;
                Stack.ItemId = Pair.Key;
                Stack.Category = Category;
                Stack.bIsTool = Category == EInventoryItemCategory::Tool;
                Stack.MaxStack = Stack.bIsTool ? 1 : 99;
                Stack.Quantity = FMath::Min(Remaining, Stack.MaxStack);
                if (Stack.Quantity <= 0)
                    return false;
                SalvageStacks.Add(Stack);
                Remaining -= Stack.Quantity;
            }
        }
        if (SalvageStacks.IsEmpty() || !Inventory->CanAddItems(SalvageStacks))
            return false;
    }

    TArray<FWorldStructuralNodeSnapshot> GraphBefore;
    if (!ExportSnapshot(GraphBefore))
        return false;
    TArray<FWorldOwnedItemRecord> OwnershipBefore;
    TArray<FWorldItemTransferReceipt> TransferReceiptsBefore;
    TArray<FWorldItemSplitReceipt> SplitReceiptsBefore;
    Ownership->ExportSnapshot(OwnershipBefore, TransferReceiptsBefore, SplitReceiptsBefore);

    FGuid InventoryActorId;
    FGuid InventoryOwnerId;
    TArray<FInventoryStack> InventoryBefore;
    FName QuickAccessBefore = NAME_None;
    if (bReturnSalvage)
    {
        Inventory->ExportSnapshot(InventoryActorId, InventoryOwnerId, InventoryBefore, QuickAccessBefore);
        if (!InventoryOwnerId.IsValid() || InventoryOwnerId != Inventory->GetInventoryOwnerId())
            return false;
    }

    auto RollbackData = [this, Ownership, Inventory, bReturnSalvage, &GraphBefore,
        &OwnershipBefore, &TransferReceiptsBefore, &SplitReceiptsBefore,
        &InventoryOwnerId, &InventoryBefore, &QuickAccessBefore]()
    {
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            OwnershipBefore, TransferReceiptsBefore, SplitReceiptsBefore);
        const bool bInventoryRestored = !bReturnSalvage ||
            Inventory->RestoreSnapshot(InventoryOwnerId, InventoryBefore, QuickAccessBefore);
        const bool bGraphRestored = RestoreSnapshot(GraphBefore);
        const bool bActorLinksRestored = bGraphRestored && RebuildLogActorAttachmentsFromSnapshot();
        if (!bOwnershipRestored || !bInventoryRestored || !bGraphRestored || !bActorLinksRestored)
            UE_LOG(LogTemp, Error, TEXT("Connected structural removal rollback failed; actor/graph/ownership recovery is required."));
        return false;
    };

    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : ProposedSurvivors)
        Nodes.Add(Pair.Key, Pair.Value);

    for (const FRemovalEntry& Entry : Entries)
    {
        FWorldOwnedItemRecord Consumed;
        if (!Ownership->TryTransferItem(FGuid::NewGuid(), Entry.NodeId, Entry.Item.Revision,
            EWorldItemOwnerKind::World, EWorldItemOwnerKind::Consumed, FGuid(),
            Entry.Item.WorldLocation, Consumed))
        {
            return RollbackData();
        }
    }

    if (bReturnSalvage)
    {
        for (const FInventoryStack& Stack : SalvageStacks)
        {
            if (!Inventory->TryAddItem(Stack))
                return RollbackData();
        }
    }

    // Remove local links while all actors still exist. If a domain adapter
    // refuses disconnect, roll the whole transaction back before any actor dies.
    for (const FRemovalEntry& Entry : Entries)
    {
        AActor* Actor = Entry.Actor.Get();
        if (!IsValid(Actor))
            return RollbackData();

        if (Entry.Node.ParentNodeId.IsValid())
        {
            bool bDetached = false;
            if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Actor))
                bDetached = Log->DetachFromSnapParent(false);
            else if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Actor))
                bDetached = Stone->RemoveConnection(false);
            else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Actor))
                bDetached = Stick->RemoveConnection(false);
            else
                bDetached = TryDisconnect(Entry.NodeId);
            if (!bDetached)
                return RollbackData();

            FStructuralNodeRecord DetachedNode;
            if (!GetNode(Entry.NodeId, DetachedNode) ||
                DetachedNode.ParentNodeId.IsValid() || !DetachedNode.SecondarySupports.IsEmpty())
                return RollbackData();
        }
    }

    // Actor-local attachments are reconstructed from the changed graph before
    // the removed subset is committed. Promoted alternate supports therefore
    // become real parent links and regain their anchor reservations.
    if (!RebuildLogActorAttachmentsFromSnapshot())
        return RollbackData();

    if (!RemoveNodesAtomically(LeafFirstOrder))
        return RollbackData();

    // Lifecycle transitions are preflighted to World/Placed only and happen
    // after the authoritative owner and graph changes have succeeded.
    for (FRemovalEntry& Entry : Entries)
    {
        AActor* Actor = Entry.Actor.Get();
        UWorldObjectIdentityComponent* Identity = Entry.Identity.Get();
        if (!IsValid(Actor) || !IsValid(Identity) ||
            !Identity->TryTransitionLifecycle(Entry.Lifecycle, EWorldObjectLifecycleState::Destroyed))
        {
            UE_LOG(LogTemp, Error, TEXT("Connected structure was committed but an actor lifecycle transition failed for node %s."),
                *Entry.NodeId.ToString());
            if (IsValid(Actor))
            {
                Actor->SetActorHiddenInGame(true);
                Actor->SetActorEnableCollision(false);
                Actor->SetActorTickEnabled(false);
            }
            continue;
        }
    }

    for (const FRemovalEntry& Entry : Entries)
    {
        if (AActor* Actor = Entry.Actor.Get())
        {
            Actor->SetActorHiddenInGame(true);
            Actor->SetActorEnableCollision(false);
            Actor->SetActorTickEnabled(false);
            if (!Actor->Destroy())
                UE_LOG(LogTemp, Warning, TEXT("Committed structural removal left inert actor %s pending destruction."), *GetNameSafe(Actor));
        }
    }
    return true;
}

bool UWorldStructuralGraphSubsystem::RemoveNodesAtomically(const TArray<FGuid>& NodeIds)
{
    if (NodeIds.IsEmpty())
        return false;
    TSet<FGuid> RemovalSet;
    for (const FGuid& NodeId : NodeIds)
    {
        if (!NodeId.IsValid() || RemovalSet.Contains(NodeId) || !Nodes.Contains(NodeId))
            return false;
        RemovalSet.Add(NodeId);
    }
    for (const FGuid& NodeId : NodeIds)
        for (const FGuid& DependentId : GetAllDependents(NodeId))
            if (!RemovalSet.Contains(DependentId))
                return false;

    TMap<FGuid, FStructuralNodeRecord> Proposed = Nodes;
    for (const FGuid& NodeId : NodeIds)
        Proposed.Remove(NodeId);
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Proposed)
    {
        if (Pair.Value.ParentNodeId.IsValid() && !Proposed.Contains(Pair.Value.ParentNodeId))
            return false;
        for (const FStructuralSupportLink& Link : Pair.Value.SecondarySupports)
            if (!Proposed.Contains(Link.SupportNodeId))
                return false;
    }

    TMap<FGuid, FStructuralNodeRecord> Previous = MoveTemp(Nodes);
    Nodes = MoveTemp(Proposed);
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        if (Pair.Value.ParentNodeId.IsValid() && !HasValidSupportPath(Pair.Key))
        {
            Nodes = MoveTemp(Previous);
            return false;
        }
        TSet<FGuid> Visiting;
        bool bLoadValid = true;
        const float Aggregate = ComputeAggregateLoad(Pair.Key, Visiting, bLoadValid);
        const FStructuralNodeRecord* LiveNode = Nodes.Find(Pair.Key);
        if (!bLoadValid || !LiveNode || Aggregate > LiveNode->MaxSupportedLoad)
        {
            Nodes = MoveTemp(Previous);
            return false;
        }
    }
    return true;
}

bool UWorldStructuralGraphSubsystem::ApplyDamage(
    FGuid NodeId, float DamageAmount, bool& bOutThresholdExceeded)
{
    bOutThresholdExceeded = false;
    FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node || Node->Revision == MAX_int64 || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
    {
        return false;
    }

    Node->Damage = FMath::Min(Node->DamageThreshold, Node->Damage + DamageAmount);
    ++Node->Revision;
    bOutThresholdExceeded = Node->Damage >= Node->DamageThreshold;
    return true;
}

bool UWorldStructuralGraphSubsystem::GetNode(
    FGuid NodeId, FStructuralNodeRecord& OutNode) const
{
    const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node)
    {
        return false;
    }
    OutNode = *Node;
    return true;
}

TArray<FGuid> UWorldStructuralGraphSubsystem::GetDirectDependents(FGuid NodeId) const
{
    TArray<FGuid> Result;
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        if (Pair.Value.ParentNodeId == NodeId)
        {
            Result.Add(Pair.Key);
        }
    }
    return Result;
}

bool UWorldStructuralGraphSubsystem::WouldCreateCycle(
    FGuid ParentNodeId, FGuid ChildNodeId) const
{
    // The child must not already be below the proposed support through any link.
    TArray<FGuid> Pending;
    Pending.Add(ParentNodeId);
    TSet<FGuid> Visited;
    while (!Pending.IsEmpty())
    {
        const FGuid CurrentId = Pending.Pop();
        if (CurrentId == ChildNodeId)
            return true;
        if (Visited.Contains(CurrentId))
            continue;
        Visited.Add(CurrentId);
        const FStructuralNodeRecord* Current = Nodes.Find(CurrentId);
        if (!Current)
            return true;
        if (Current->ParentNodeId.IsValid())
            Pending.Add(Current->ParentNodeId);
        for (const FStructuralSupportLink& Link : Current->SecondarySupports)
            Pending.Add(Link.SupportNodeId);
    }
    return false;
}
