#include "WorldLogisticsRouteSubsystem.h"
#include "Algo/Reverse.h"

bool UWorldLogisticsRouteSubsystem::UpsertNode(const FLogisticsRouteNode& Node)
{
    if (Node.NodeId.IsNone() || !FMath::IsFinite(Node.Location.X) ||
        !FMath::IsFinite(Node.Location.Y) || !FMath::IsFinite(Node.Location.Z) ||
        MaxNodes < 1 || MaxNodes > 4096)
    {
        return false;
    }

    if (!Nodes.Contains(Node.NodeId) && Nodes.Num() >= MaxNodes)
    {
        return false;
    }
    Nodes.Add(Node.NodeId, Node);
    return true;
}

bool UWorldLogisticsRouteSubsystem::UpsertEdge(const FLogisticsRouteEdge& Edge)
{
    if (Edge.FromNode.IsNone() || Edge.ToNode.IsNone() || Edge.FromNode == Edge.ToNode ||
        !Nodes.Contains(Edge.FromNode) || !Nodes.Contains(Edge.ToNode) ||
        !FMath::IsFinite(Edge.Cost) || Edge.Cost < 0.0f ||
        MaxEdges < 1 || MaxEdges > 16384)
    {
        return false;
    }

    const int32 ExistingIndex = FindEdgeIndex(Edge.FromNode, Edge.ToNode);
    if (ExistingIndex == INDEX_NONE && Edges.Num() >= MaxEdges)
    {
        return false;
    }
    if (ExistingIndex == INDEX_NONE)
    {
        Edges.Add(Edge);
    }
    else
    {
        Edges[ExistingIndex] = Edge;
    }
    return true;
}

bool UWorldLogisticsRouteSubsystem::SetNodeEnabled(FName NodeId, bool bEnabled)
{
    FLogisticsRouteNode* Node = Nodes.Find(NodeId);
    if (!Node)
    {
        return false;
    }
    Node->bEnabled = bEnabled;
    return true;
}

bool UWorldLogisticsRouteSubsystem::SetEdgeEnabled(FName FromNode, FName ToNode, bool bEnabled)
{
    const int32 Index = FindEdgeIndex(FromNode, ToNode);
    if (Index == INDEX_NONE)
    {
        return false;
    }
    Edges[Index].bEnabled = bEnabled;
    return true;
}

bool UWorldLogisticsRouteSubsystem::RemoveNode(FName NodeId)
{
    if (!Nodes.Remove(NodeId))
    {
        return false;
    }
    Edges.RemoveAll([NodeId](const FLogisticsRouteEdge& Edge)
    {
        return Edge.FromNode == NodeId || Edge.ToNode == NodeId;
    });
    return true;
}

bool UWorldLogisticsRouteSubsystem::FindRoute(
    FName StartNode, FName GoalNode, TArray<FName>& OutPath, float& OutTotalCost) const
{
    OutPath.Reset();
    OutTotalCost = 0.0f;

    const FLogisticsRouteNode* Start = Nodes.Find(StartNode);
    const FLogisticsRouteNode* Goal = Nodes.Find(GoalNode);
    if (!Start || !Goal || !Start->bEnabled || !Goal->bEnabled)
    {
        return false;
    }
    if (StartNode == GoalNode)
    {
        OutPath.Add(StartNode);
        return true;
    }

    TMap<FName, float> Distances;
    TMap<FName, FName> Previous;
    TSet<FName> Unvisited;
    for (const TPair<FName, FLogisticsRouteNode>& Pair : Nodes)
    {
        if (Pair.Value.bEnabled)
        {
            Distances.Add(Pair.Key, TNumericLimits<float>::Max());
            Unvisited.Add(Pair.Key);
        }
    }
    if (!Distances.Contains(StartNode) || !Distances.Contains(GoalNode))
    {
        return false;
    }
    Distances[StartNode] = 0.0f;

    // O(V^2 + E) bounded Dijkstra is sufficient for this deliberately small
    // authored route graph and avoids hidden heap/lifetime complexity.
    while (!Unvisited.IsEmpty())
    {
        FName Current = NAME_None;
        float BestDistance = TNumericLimits<float>::Max();
        for (const FName Candidate : Unvisited)
        {
            const float* Distance = Distances.Find(Candidate);
            if (Distance && *Distance < BestDistance)
            {
                BestDistance = *Distance;
                Current = Candidate;
            }
        }
        if (Current.IsNone() || BestDistance == TNumericLimits<float>::Max())
        {
            break;
        }
        if (Current == GoalNode)
        {
            break;
        }
        Unvisited.Remove(Current);

        for (const FLogisticsRouteEdge& Edge : Edges)
        {
            if (!Edge.bEnabled || Edge.FromNode != Current ||
                !Unvisited.Contains(Edge.ToNode))
            {
                continue;
            }
            const float* CurrentDistance = Distances.Find(Current);
            const float* NeighborDistance = Distances.Find(Edge.ToNode);
            if (!CurrentDistance || !NeighborDistance)
            {
                continue;
            }
            const double CandidateCost = static_cast<double>(*CurrentDistance) + Edge.Cost;
            if (!FMath::IsFinite(CandidateCost) || CandidateCost > TNumericLimits<float>::Max())
            {
                continue;
            }
            if (static_cast<float>(CandidateCost) < *NeighborDistance)
            {
                Distances[Edge.ToNode] = static_cast<float>(CandidateCost);
                Previous.Add(Edge.ToNode, Current);
            }
        }
    }

    const float* GoalDistance = Distances.Find(GoalNode);
    if (!GoalDistance || *GoalDistance == TNumericLimits<float>::Max())
    {
        return false;
    }

    TArray<FName> ReversePath;
    FName Cursor = GoalNode;
    ReversePath.Add(Cursor);
    int32 Guard = 0;
    while (Cursor != StartNode && Guard++ <= Nodes.Num())
    {
        const FName* Parent = Previous.Find(Cursor);
        if (!Parent)
        {
            OutPath.Reset();
            return false;
        }
        Cursor = *Parent;
        ReversePath.Add(Cursor);
    }
    if (Cursor != StartNode)
    {
        OutPath.Reset();
        return false;
    }

    Algo::Reverse(ReversePath);
    OutPath = MoveTemp(ReversePath);
    OutTotalCost = *GoalDistance;
    return true;
}

int32 UWorldLogisticsRouteSubsystem::FindEdgeIndex(FName FromNode, FName ToNode) const
{
    for (int32 Index = 0; Index < Edges.Num(); ++Index)
    {
        if (Edges[Index].FromNode == FromNode && Edges[Index].ToNode == ToNode)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}
