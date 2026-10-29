#include "WorldStructuralGraphSubsystem.h"

#include "GameFramework/Actor.h"
#include "WorldObjectIdentityComponent.h"
#include "LogBuildPiece.h"
#include "MaterialConnectionSubsystem.h"
#include "StructuralPieceInterface.h"
#include "StoneBuildingPiece.h"
#include "StickPrimitivePiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "EngineUtils.h"

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

    // First clear stale actor-local pointers/reservations. The canonical graph
    // remains untouched and is the only authority for parent/anchor relations.
    for (TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        AActor* Actor = Pair.Value.Actor.Get();
        if (IStructuralPieceInterface* Piece = Cast<IStructuralPieceInterface>(Actor))
        {
            if (Piece->GetCrossMaterialParent())
            {
                Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                Piece->ClearCrossMaterialAttachment();
            }
            // Release both anchors of a mixed-material edge so the rebuild can
            // reserve them again from the canonical record.
            const FStructuralNodeRecord* ParentNode = Pair.Value.ParentNodeId.IsValid()
                ? Nodes.Find(Pair.Value.ParentNodeId) : nullptr;
            if (ParentNode && ParentNode->Kind != Pair.Value.Kind)
            {
                Piece->SetStructuralAnchorOccupied(Pair.Value.OwnAnchorId, false);
                if (IStructuralPieceInterface* ParentPiece =
                    Cast<IStructuralPieceInterface>(ParentNode->Actor.Get()))
                {
                    ParentPiece->SetStructuralAnchorOccupied(Pair.Value.ParentAnchorId, false);
                }
            }
        }
        if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Actor))
            Log->ResetActorLocalSnapStateForGraphRestore();
        else if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Actor))
            Stone->ResetLocalConnectionStateForGraphRestore();
        else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Actor))
            Stick->ResetLocalConnectionStateForGraphRestore();
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

        ALogBuildPiece* ChildLog = Cast<ALogBuildPiece>(ChildNode->Actor.Get());
        ALogBuildPiece* ParentLog = ParentNode
            ? Cast<ALogBuildPiece>(ParentNode->Actor.Get()) : nullptr;
        if (ChildLog && ParentLog)
        {
            if (!ChildLog->RestoreActorLocalSnapLinkFromGraph(
                ParentLog, ChildNode->ParentAnchorId, ChildNode->OwnAnchorId, true))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Structural graph restored but log-local anchor reconstruction failed for node %s."),
                    *NodeId.ToString());
                return false;
            }
            continue;
        }

        if (AStoneBuildingPiece* ChildStone = Cast<AStoneBuildingPiece>(ChildNode->Actor.Get()))
        {
            AStoneBuildingPiece* ParentStone = ParentNode
                ? Cast<AStoneBuildingPiece>(ParentNode->Actor.Get()) : nullptr;
            if (!ParentStone || !ChildStone->RestoreConnectionFromGraph(
                ParentStone, ChildNode->ParentAnchorId, ChildNode->OwnAnchorId))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Structural graph restored but stone-local anchor reconstruction failed for node %s."),
                    *NodeId.ToString());
                return false;
            }
            continue;
        }

        if (AStickPrimitivePiece* ChildStick = Cast<AStickPrimitivePiece>(ChildNode->Actor.Get()))
        {
            AStickPrimitivePiece* ParentStick = ParentNode
                ? Cast<AStickPrimitivePiece>(ParentNode->Actor.Get()) : nullptr;
            if (!ParentStick || !ChildStick->RestoreConnectionFromGraph(
                ParentStick, ChildNode->ParentAnchorId, ChildNode->OwnAnchorId))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Structural graph restored but stick-local anchor reconstruction failed for node %s."),
                    *NodeId.ToString());
                return false;
            }
        }
    }

    // Root placement and storage physics come from the canonical graph/owner ledger.
    // This is intentionally done after rebuilding child links so occupied anchors remain intact.
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    for (TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        if (Pair.Value.ParentNodeId.IsValid())
            continue;

        FWorldOwnedItemRecord Item;
        const bool bStorageOwned = Ownership &&
            Ownership->TryGetItem(Pair.Key, Item) &&
            Item.OwnerKind == EWorldItemOwnerKind::Storage;

        if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Pair.Value.Actor.Get()))
        {
            Log->SetHeldState(false);
            Log->SetPlacedState(Pair.Value.bGroundSupported);
            if (bStorageOwned)
            {
                if (UStaticMeshComponent* Mesh = Log->GetLogMesh())
                {
                    Mesh->SetSimulatePhysics(false);
                    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
                    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                }
                Log->SetActorEnableCollision(true);
            }
        }
        else if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Pair.Value.Actor.Get()))
        {
            Stone->RestoreRootStateFromGraph(Pair.Value.bGroundSupported, bStorageOwned);
        }
        else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Pair.Value.Actor.Get()))
        {
            Stick->RestoreRootStateFromGraph(Pair.Value.bGroundSupported, bStorageOwned);
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

    // Work on a reversible graph copy. Reparenting children changes aggregate loads
    // on the promoted support chain; reject and restore the entire graph if the
    // resulting structure would overload or lose a required support path.
    const TMap<FGuid, FStructuralNodeRecord> PreviousNodes = Nodes;

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

    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        const FStructuralNodeRecord& Candidate = Pair.Value;
        if (Candidate.ParentNodeId.IsValid())
        {
            TSet<FGuid> SupportVisiting;
            if (!HasValidSupportPathInternal(Pair.Key, FGuid(), SupportVisiting))
            {
                Nodes = PreviousNodes;
                return false;
            }
        }

        TSet<FGuid> LoadVisiting;
        bool bLoadValid = true;
        const float Aggregate = ComputeAggregateLoad(Pair.Key, LoadVisiting, bLoadValid);
        if (!bLoadValid || Aggregate > Candidate.MaxSupportedLoad)
        {
            Nodes = PreviousNodes;
            return false;
        }
    }
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
        Child->ParentNodeId.IsValid() || !GetDirectDependents(ChildNodeId).IsEmpty() ||
        !Parent->Actor.IsValid() || !Child->Actor.IsValid() ||
        WouldCreateCycle(ParentNodeId, ChildNodeId) ||
        !HasValidSupportPath(ParentNodeId) ||
        Child->Revision == MAX_int64)
    {
        return false;
    }

    // Enforce one edge per physical anchor even when a caller bypasses the
    // actor-level snap-point reservation API.
    if (IsAnchorInUse(ParentNodeId, ParentAnchorId))
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

    // Disconnecting removes the piece from the structure entirely: its extra
    // contacts go too, so actor-local detachment and graph state stay aligned.
    for (const FStructuralSupportLink& Link : Child->SecondarySupports)
    {
        if (FStructuralNodeRecord* Support = Nodes.Find(Link.SupportNodeId))
        {
            if (Support->Revision < MAX_int64)
                ++Support->Revision;
        }
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
        WouldCreateCycle(SupportNodeId, ChildNodeId) ||
        !HasValidSupportPath(SupportNodeId))
    {
        return false;
    }
    for (const FStructuralSupportLink& Link : Child->SecondarySupports)
    {
        if (Link.SupportNodeId == SupportNodeId || Link.OwnAnchorId == OwnAnchorId)
            return false;
    }

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
    if (!Child || !Support || ChildNodeId == SupportNodeId ||
        Child->Revision == MAX_int64 || Support->Revision == MAX_int64)
        return false;

    const TArray<FStructuralSupportLink> PreviousSupports = Child->SecondarySupports;
    const int32 Removed = Child->SecondarySupports.RemoveAll(
        [&SupportNodeId](const FStructuralSupportLink& Link) { return Link.SupportNodeId == SupportNodeId; });
    if (Removed == 0)
        return false;

    // Removing a secondary support redirects more load to the remaining supports.
    // Preflight every resulting aggregate before advancing revisions/committing.
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        TSet<FGuid> Visiting;
        bool bLoadValid = true;
        const float Aggregate = ComputeAggregateLoad(Pair.Key, Visiting, bLoadValid);
        if (!bLoadValid || Aggregate > Pair.Value.MaxSupportedLoad)
        {
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

    TArray<FGuid> Children = GetDirectDependents(NodeId);
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
            const float ChildLoad = ComputeAggregateLoad(Pair.Key, Visiting, bOutValid);
            if (!bOutValid)
            {
                Visiting.Remove(NodeId);
                return 0.0f;
            }

            // Approximate the child as a point load. For exactly two supports,
            // simple beam statics gives each support a reaction proportional to
            // its distance from the opposite support. For three or more supports,
            // retain a deterministic inverse-distance heuristic (not a full solver).
            TArray<FGuid> SupportIds;
            if (Pair.Value.ParentNodeId.IsValid())
                SupportIds.Add(Pair.Value.ParentNodeId);
            for (const FStructuralSupportLink& Link : Pair.Value.SecondarySupports)
                if (Link.SupportNodeId.IsValid())
                    SupportIds.AddUnique(Link.SupportNodeId);
            SupportIds.Sort([](const FGuid& A, const FGuid& B)
            {
                return A.ToString() < B.ToString();
            });

            if (SupportIds.IsEmpty())
            {
                Visiting.Remove(NodeId);
                bOutValid = false;
                return 0.0f;
            }

            const AActor* ChildActor = Pair.Value.Actor.Get();
            const bool bChildLocationValid = IsValid(ChildActor) &&
                !ChildActor->GetActorLocation().ContainsNaN();
            bool bAllLocationsValid = bChildLocationValid;
            for (const FGuid& SupportId : SupportIds)
            {
                const FStructuralNodeRecord* SupportNode = Nodes.Find(SupportId);
                const AActor* SupportActor = SupportNode ? SupportNode->Actor.Get() : nullptr;
                bAllLocationsValid &= IsValid(SupportActor) &&
                    !SupportActor->GetActorLocation().ContainsNaN();
            }

            TArray<double> RawWeights;
            RawWeights.Reserve(SupportIds.Num());
            double WeightTotal = 0.0;
            int32 CoincidentSupports = 0;
            TArray<double> SupportDistances;
            SupportDistances.Reserve(SupportIds.Num());
            for (const FGuid& SupportId : SupportIds)
            {
                double Distance = 0.0;
                if (bAllLocationsValid)
                {
                    const FStructuralNodeRecord* SupportNode = Nodes.Find(SupportId);
                    const AActor* SupportActor = SupportNode ? SupportNode->Actor.Get() : nullptr;
                    if (!IsValid(SupportActor))
                    {
                        Visiting.Remove(NodeId);
                        bOutValid = false;
                        return 0.0f;
                    }
                    Distance = FVector::Distance(
                        ChildActor->GetActorLocation(), SupportActor->GetActorLocation());
                    if (!FMath::IsFinite(Distance))
                    {
                        Visiting.Remove(NodeId);
                        bOutValid = false;
                        return 0.0f;
                    }
                    if (Distance <= 1.0)
                        ++CoincidentSupports;
                }
                SupportDistances.Add(Distance);
            }

            if (CoincidentSupports > 0)
            {
                WeightTotal = static_cast<double>(CoincidentSupports);
                for (double Distance : SupportDistances)
                    RawWeights.Add(Distance <= 1.0 ? 1.0 : 0.0);
            }
            else if (SupportIds.Num() == 2 && bAllLocationsValid)
            {
                // For two supports A and B, R_A = P*d_B/(d_A+d_B) and
                // R_B = P*d_A/(d_A+d_B), where d_* is distance from the load
                // point to that support. This conserves the load and moment.
                RawWeights.Add(SupportDistances[1]);
                RawWeights.Add(SupportDistances[0]);
                WeightTotal = RawWeights[0] + RawWeights[1];
            }
            else
            {
                for (double Distance : SupportDistances)
                {
                    const double Weight = bAllLocationsValid && Distance > 1.0
                        ? 1.0 / Distance : 1.0;
                    RawWeights.Add(Weight);
                    WeightTotal += Weight;
                }
            }

            const int32 SupportIndex = SupportIds.IndexOfByKey(NodeId);
            if (SupportIndex == INDEX_NONE || !FMath::IsFinite(WeightTotal) ||
                WeightTotal <= 0.0 || !FMath::IsFinite(ChildLoad) || ChildLoad < 0.0f)
            {
                Visiting.Remove(NodeId);
                bOutValid = false;
                return 0.0f;
            }
            Total += ChildLoad * static_cast<float>(RawWeights[SupportIndex] / WeightTotal);
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

bool UWorldStructuralGraphSubsystem::ApplyDamageBatch(
    const TArray<FGuid>& NodeIds, float DamageAmount,
    TArray<FGuid>& OutAffectedNodeIds, TArray<FGuid>& OutThresholdExceededNodeIds)
{
    OutAffectedNodeIds.Reset();
    OutThresholdExceededNodeIds.Reset();
    if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
        return false;

    TArray<FGuid> OrderedIds = NodeIds;
    OrderedIds.Sort([](const FGuid& A, const FGuid& B)
    {
        return A.ToString() < B.ToString();
    });

    TSet<FGuid> Seen;
    TMap<FGuid, FStructuralNodeRecord> ProposedNodes = Nodes;
    TArray<FGuid> ProposedAffected;
    TArray<FGuid> ProposedThresholdExceeded;
    for (const FGuid& NodeId : OrderedIds)
    {
        if (!NodeId.IsValid() || Seen.Contains(NodeId))
            return false;
        Seen.Add(NodeId);

        FStructuralNodeRecord* Node = ProposedNodes.Find(NodeId);
        if (!Node || !Node->Actor.IsValid() || Node->Revision == MAX_int64 ||
            !FMath::IsFinite(Node->Damage) || Node->Damage < 0.0f ||
            !FMath::IsFinite(Node->DamageThreshold) || Node->DamageThreshold <= 0.0f ||
            Node->Damage > Node->DamageThreshold ||
            !FMath::IsFinite(Node->Damage + DamageAmount))
        {
            return false;
        }

        Node->Damage = FMath::Min(Node->DamageThreshold, Node->Damage + DamageAmount);
        ++Node->Revision;
        ProposedAffected.Add(NodeId);
        if (Node->Damage >= Node->DamageThreshold)
            ProposedThresholdExceeded.Add(NodeId);
    }

    // A single map swap means validation failure never leaves half a pulse's
    // structural damage applied to an arbitrary prefix of the target list.
    Nodes = MoveTemp(ProposedNodes);
    OutAffectedNodeIds = MoveTemp(ProposedAffected);
    OutThresholdExceededNodeIds = MoveTemp(ProposedThresholdExceeded);
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
