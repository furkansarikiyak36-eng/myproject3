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
    {
        return false;
    }

    if (bRequireNoDependents && !GetDirectDependents(NodeId).IsEmpty())
    {
        return false;
    }

    // Removing a parent edge leaves the child detached; it does not silently
    // claim the child remains supported. Caller must resolve the consequence.
    if (Node->ParentNodeId.IsValid())
    {
        if (FStructuralNodeRecord* Parent = Nodes.Find(Node->ParentNodeId))
        {
            ++Parent->Revision;
        }
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
        Child->ParentNodeId.IsValid() ||
        !Parent->Actor.IsValid() || !Child->Actor.IsValid() ||
        WouldCreateCycle(ParentNodeId, ChildNodeId) ||
        !HasValidSupportPath(ParentNodeId))
    {
        return false;
    }

    Child->ParentNodeId = ParentNodeId;
    Child->ParentAnchorId = ParentAnchorId;
    Child->OwnAnchorId = OwnAnchorId;
    Child->bGroundSupported = false;

    TSet<FGuid> Visiting;
    bool bLoadValid = true;
    const float Aggregate = ComputeAggregateLoad(ParentNodeId, Visiting, bLoadValid);
    if (!bLoadValid || Aggregate > Parent->MaxSupportedLoad)
    {
        Child->ParentNodeId.Invalidate();
        Child->ParentAnchorId = NAME_None;
        Child->OwnAnchorId = NAME_None;
        return false;
    }

    ++Parent->Revision;
    ++Child->Revision;
    return true;
}

bool UWorldStructuralGraphSubsystem::TryDisconnect(FGuid ChildNodeId)
{
    FStructuralNodeRecord* Child = Nodes.Find(ChildNodeId);
    if (!Child || !Child->ParentNodeId.IsValid())
    {
        return false;
    }

    if (FStructuralNodeRecord* Parent = Nodes.Find(Child->ParentNodeId))
    {
        ++Parent->Revision;
    }
    Child->ParentNodeId.Invalidate();
    Child->ParentAnchorId = NAME_None;
    Child->OwnAnchorId = NAME_None;
    Child->bGroundSupported = false;
    ++Child->Revision;
    return true;
}

bool UWorldStructuralGraphSubsystem::SetGroundSupported(FGuid NodeId, bool bGroundSupported)
{
    FStructuralNodeRecord* Node = Nodes.Find(NodeId);
    if (!Node || (bGroundSupported && Node->ParentNodeId.IsValid()))
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
    if (!Node || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
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
