#include "WorldStructuralGraphSubsystem.h"

#include "GameFramework/Actor.h"

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
    if (bRequireNoDependents && !Dependents.IsEmpty())
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

    // If a caller explicitly permits removal with dependents, detach those
    // graph edges and mark them unsupported rather than leaving dangling IDs.
    for (const FGuid& ChildId : Dependents)
    {
        if (FStructuralNodeRecord* Child = Nodes.Find(ChildId))
        {
            Child->ParentNodeId.Invalidate();
            Child->ParentAnchorId = NAME_None;
            Child->OwnAnchorId = NAME_None;
            Child->bGroundSupported = false;
            ++Child->Revision;
        }
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
    for (const TPair<FGuid, FStructuralNodeRecord>& Pair : Nodes)
    {
        if (Pair.Value.ParentNodeId == ParentNodeId &&
            Pair.Value.ParentAnchorId == ParentAnchorId)
        {
            return false;
        }
    }

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
    const FStructuralNodeRecord* Current = Nodes.Find(NodeId);
    if (!Current)
    {
        return false;
    }

    TSet<FGuid> Visited;
    while (Current)
    {
        if (Visited.Contains(Current->NodeId))
        {
            return false;
        }
        Visited.Add(Current->NodeId);

        if (!Current->Actor.IsValid() || Current->Damage >= Current->DamageThreshold)
        {
            return false;
        }
        if (Current->bGroundSupported)
        {
            return true;
        }
        if (!Current->ParentNodeId.IsValid())
        {
            return false;
        }
        Current = Nodes.Find(Current->ParentNodeId);
    }
    return false;
}

bool UWorldStructuralGraphSubsystem::CanSafelyRemove(FGuid NodeId) const
{
    const FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    return Node && GetDirectDependents(NodeId).IsEmpty();
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
        if (Pair.Value.ParentNodeId == NodeId)
        {
            Total += ComputeAggregateLoad(Pair.Key, Visiting, bOutValid);
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
    FGuid CurrentId = ParentNodeId;
    TSet<FGuid> Visited;
    while (CurrentId.IsValid())
    {
        if (CurrentId == ChildNodeId || Visited.Contains(CurrentId))
        {
            return true;
        }
        Visited.Add(CurrentId);
        const FStructuralNodeRecord* Current = Nodes.Find(CurrentId);
        if (!Current)
        {
            return true;
        }
        CurrentId = Current->ParentNodeId;
    }
    return false;
}
