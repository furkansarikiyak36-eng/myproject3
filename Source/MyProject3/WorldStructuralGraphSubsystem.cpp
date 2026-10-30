#include "WorldStructuralGraphSubsystem.h"
#include "WorldClimateSubsystem.h"
#include "WorldRegionalEventSubsystem.h"

#include "GameFramework/Actor.h"
#include "WorldObjectIdentityComponent.h"
#include "LogBuildPiece.h"
#include "MaterialConnectionSubsystem.h"
#include "StructuralPieceInterface.h"
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
        RefreshEnvironmentMetadata(Node);
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

        ALogBuildPiece* Child = Cast<ALogBuildPiece>(ChildNode->Actor.Get());
        ALogBuildPiece* Parent = ParentNode
            ? Cast<ALogBuildPiece>(ParentNode->Actor.Get()) : nullptr;

        // Non-log structural adapters own their own local anchor bookkeeping.
        // Do not fabricate a log-only link for a cross-domain edge.
        if (!Child || !Parent)
            continue;

        if (!Child->RestoreActorLocalSnapLinkFromGraph(
            Parent, ChildNode->ParentAnchorId, ChildNode->OwnAnchorId, true))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Structural graph restored but log-local anchor reconstruction failed for node %s."),
                *NodeId.ToString());
            return false;
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
    RefreshEnvironmentMetadata(Record);
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
    if (!Child || Child->Revision == MAX_int64)
        return false;
    const int32 Removed = Child->SecondarySupports.RemoveAll(
        [&SupportNodeId](const FStructuralSupportLink& Link) { return Link.SupportNodeId == SupportNodeId; });
    if (Removed == 0)
        return false;
    ++Child->Revision;
    if (FStructuralNodeRecord* Support = Nodes.Find(SupportNodeId))
    {
        if (Support->Revision < MAX_int64)
            ++Support->Revision;
    }
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

    const float EffectiveAggregateLoad = GetAggregateLoad(NodeId);
    if (!FMath::IsFinite(EffectiveAggregateLoad) || EffectiveAggregateLoad < 0.0f ||
        EffectiveAggregateLoad > Current->MaxSupportedLoad)
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
    float Total = Node->LocalLoad + CalculateNodeSnowLoad(*Node);
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



void UWorldStructuralGraphSubsystem::RefreshEnvironmentMetadata(FStructuralNodeRecord& Node) const
{
    Node.RegionId = NAME_None;
    Node.bSnowExposed = false;
    Node.SnowCatchmentAreaM2 = 0.0f;
    Node.RaidDefenseRating = 0.0f;

    AActor* Actor = Node.Actor.Get();
    if (!IsValid(Actor))
        return;

    for (const FName& Tag : Actor->Tags)
    {
        const FString Value = Tag.ToString();
        if (Value.StartsWith(TEXT("Region:"), ESearchCase::CaseSensitive))
        {
            const FString Region = Value.RightChop(7);
            if (!Region.IsEmpty())
                Node.RegionId = FName(*Region);
        }
        else if (Value.Equals(TEXT("SnowExposed"), ESearchCase::CaseSensitive))
        {
            Node.bSnowExposed = true;
        }
        else if (Value.StartsWith(TEXT("SnowAreaM2:"), ESearchCase::CaseSensitive))
        {
            const FString Number = Value.RightChop(11);
            const float Parsed = FCString::Atof(*Number);
            if (FMath::IsFinite(Parsed) && Parsed >= 0.0f)
                Node.SnowCatchmentAreaM2 = Parsed;
        }
        else if (Value.StartsWith(TEXT("RaidDefense:"), ESearchCase::CaseSensitive))
        {
            const FString Number = Value.RightChop(12);
            const float Parsed = FCString::Atof(*Number);
            if (FMath::IsFinite(Parsed) && Parsed >= 0.0f)
                Node.RaidDefenseRating = FMath::Min(Parsed, 1000.0f);
        }
    }

    // Only roofs/surfaces with explicit area count. A bare tag cannot silently
    // invent area, and non-positive defense ratings never improve raid resistance.
    if (!Node.bSnowExposed || Node.SnowCatchmentAreaM2 <= 0.0f)
        Node.bSnowExposed = false;
}

TArray<FGuid> UWorldStructuralGraphSubsystem::GetNodeIds() const
{
    TArray<FGuid> Result;
    Nodes.GetKeys(Result);
    Result.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
    return Result;
}

float UWorldStructuralGraphSubsystem::GetNodeSnowLoad(FGuid NodeId) const
{
    const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    return Node ? CalculateNodeSnowLoad(*Node) : 0.0f;
}

float UWorldStructuralGraphSubsystem::CalculateNodeSnowLoad(const FStructuralNodeRecord& Node) const
{
    if (!Node.bSnowExposed || Node.SnowCatchmentAreaM2 <= 0.0f || Node.RegionId.IsNone())
        return 0.0f;

    const UWorld* World = GetWorld();
    const UWorldClimateSubsystem* Climate = World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    const UWorldRegionalEventSubsystem* Events = World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    if (!Climate || !Climate->IsClimateInitialized() || !Events || !Events->AreDefinitionsLoaded())
        return 0.0f;

    const float UnitsPerCmM2 = Events->GetSnowLoadUnitsPerCmM2(Node.RegionId);
    if (!FMath::IsFinite(UnitsPerCmM2) || UnitsPerCmM2 <= 0.0f)
        return 0.0f;

    float EventMultiplier = 1.0f;
    FWorldRegionalEventEvaluation Evaluation;
    if (Events->EvaluateRegion(Node.RegionId, Evaluation))
        EventMultiplier = Evaluation.SnowLoadMultiplier;

    const double Load = static_cast<double>(Climate->GetSnowDepthCm()) *
        static_cast<double>(UnitsPerCmM2) *
        static_cast<double>(Node.SnowCatchmentAreaM2) *
        static_cast<double>(EventMultiplier);
    if (!FMath::IsFinite(Load) || Load <= 0.0 || Load > static_cast<double>(MAX_flt))
        return 0.0f;
    return static_cast<float>(Load);
}

float UWorldStructuralGraphSubsystem::GetRegionDefenseRating(FName RegionId) const
{
    if (RegionId.IsNone())
        return 0.0f;

    double Total = 0.0;
    const TArray<FGuid> SortedIds = GetNodeIds();
    for (const FGuid& NodeId : SortedIds)
    {
        const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
        if (!Node || Node->RegionId != RegionId || Node->RaidDefenseRating <= 0.0f ||
            !Node->Actor.IsValid() || Node->Damage >= Node->DamageThreshold)
        {
            continue;
        }
        if (HasValidSupportPath(NodeId))
            Total += static_cast<double>(Node->RaidDefenseRating);
    }
    return FMath::Clamp(static_cast<float>(Total), 0.0f, 10000.0f);
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
