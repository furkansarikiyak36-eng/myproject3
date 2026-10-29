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
        !FMath::IsFinite(Edge.SurfaceCostMultiplier) ||
        Edge.SurfaceCostMultiplier < 0.25f || Edge.SurfaceCostMultiplier > 10.0f ||
        !FMath::IsFinite(Edge.InfrastructureCostMultiplier) ||
        Edge.InfrastructureCostMultiplier < 0.1f || Edge.InfrastructureCostMultiplier > 4.0f ||
        !FMath::IsFinite(Edge.SlopePenaltyPerDegree) ||
        Edge.SlopePenaltyPerDegree < 0.0f || Edge.SlopePenaltyPerDegree > 0.25f ||
        !FMath::IsFinite(Edge.LoadPenaltyPer100Kg) ||
        Edge.LoadPenaltyPer100Kg < 0.0f || Edge.LoadPenaltyPer100Kg > 10.0f ||
        !FMath::IsFinite(Edge.MaxPayloadMassKg) ||
        Edge.MaxPayloadMassKg < 0.0f || Edge.MaxPayloadMassKg > 1000000.0f ||
        !FMath::IsFinite(Edge.WeatherExposure) ||
        Edge.WeatherExposure < 0.0f || Edge.WeatherExposure > 1.0f ||
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
    const FLogisticsRouteCostContext DefaultContext;
    return FindRouteWithContext(StartNode, GoalNode, DefaultContext, OutPath, OutTotalCost);
}

bool UWorldLogisticsRouteSubsystem::FindRouteWithContext(
    FName StartNode, FName GoalNode, const FLogisticsRouteCostContext& Context,
    TArray<FName>& OutPath, float& OutTotalCost) const
{
    OutPath.Reset();
    OutTotalCost = 0.0f;
    if (!FMath::IsFinite(Context.CargoMassKg) ||
        Context.CargoMassKg < 0.0f || Context.CargoMassKg > 1000000.0f ||
        !FMath::IsFinite(Context.WeatherSeverity) ||
        Context.WeatherSeverity < 0.0f || Context.WeatherSeverity > 1.0f ||
        !FMath::IsFinite(Context.WeatherPenaltyPerSeverity) ||
        Context.WeatherPenaltyPerSeverity < 0.0f || Context.WeatherPenaltyPerSeverity > 10.0f)
    {
        return false;
    }

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
            if (Distance && (*Distance < BestDistance ||
                (*Distance == BestDistance &&
                    (Current.IsNone() || Candidate.ToString() < Current.ToString()))))
            {
                // Stable node-name tie-break makes equal-cost route selection independent
                // of TSet iteration order.
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
            const FLogisticsRouteNode* From = Nodes.Find(Edge.FromNode);
            const FLogisticsRouteNode* To = Nodes.Find(Edge.ToNode);
            float EffectiveEdgeCost = 0.0f;
            if (!From || !To ||
                !ComputeEffectiveEdgeCost(Edge, *From, *To, Context, EffectiveEdgeCost))
            {
                // An overloaded edge or invalid cost contract is unavailable; route
                // search continues through alternate paths rather than inventing a cost.
                continue;
            }
            const double CandidateCost =
                static_cast<double>(*CurrentDistance) + static_cast<double>(EffectiveEdgeCost);
            if (!FMath::IsFinite(CandidateCost) || CandidateCost > TNumericLimits<float>::Max())
            {
                continue;
            }
            const float CandidateCostFloat = static_cast<float>(CandidateCost);
            const FName* PreviousNode = Previous.Find(Edge.ToNode);
            const bool bBetterEqualCostPredecessor = CandidateCostFloat == *NeighborDistance &&
                (!PreviousNode || Current.ToString() < PreviousNode->ToString());
            if (CandidateCostFloat < *NeighborDistance || bBetterEqualCostPredecessor)
            {
                Distances[Edge.ToNode] = CandidateCostFloat;
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

bool UWorldLogisticsRouteSubsystem::ComputeEffectiveEdgeCost(
    const FLogisticsRouteEdge& Edge,
    const FLogisticsRouteNode& From,
    const FLogisticsRouteNode& To,
    const FLogisticsRouteCostContext& Context,
    float& OutCost) const
{
    OutCost = 0.0f;
    if (!FMath::IsFinite(Edge.Cost) || Edge.Cost < 0.0f ||
        !FMath::IsFinite(Edge.SurfaceCostMultiplier) ||
        Edge.SurfaceCostMultiplier < 0.25f || Edge.SurfaceCostMultiplier > 10.0f ||
        !FMath::IsFinite(Edge.InfrastructureCostMultiplier) ||
        Edge.InfrastructureCostMultiplier < 0.1f || Edge.InfrastructureCostMultiplier > 4.0f ||
        !FMath::IsFinite(Edge.SlopePenaltyPerDegree) ||
        Edge.SlopePenaltyPerDegree < 0.0f || Edge.SlopePenaltyPerDegree > 0.25f ||
        !FMath::IsFinite(Edge.LoadPenaltyPer100Kg) ||
        Edge.LoadPenaltyPer100Kg < 0.0f || Edge.LoadPenaltyPer100Kg > 10.0f ||
        !FMath::IsFinite(Edge.MaxPayloadMassKg) ||
        Edge.MaxPayloadMassKg < 0.0f || Edge.MaxPayloadMassKg > 1000000.0f ||
        !FMath::IsFinite(Edge.WeatherExposure) ||
        Edge.WeatherExposure < 0.0f || Edge.WeatherExposure > 1.0f ||
        !FMath::IsFinite(Context.CargoMassKg) ||
        Context.CargoMassKg < 0.0f || Context.CargoMassKg > 1000000.0f ||
        !FMath::IsFinite(Context.WeatherSeverity) ||
        Context.WeatherSeverity < 0.0f || Context.WeatherSeverity > 1.0f ||
        !FMath::IsFinite(Context.WeatherPenaltyPerSeverity) ||
        Context.WeatherPenaltyPerSeverity < 0.0f || Context.WeatherPenaltyPerSeverity > 10.0f ||
        !FMath::IsFinite(Edge.Cost) ||
        !FMath::IsFinite(From.Location.X) || !FMath::IsFinite(From.Location.Y) ||
        !FMath::IsFinite(From.Location.Z) || !FMath::IsFinite(To.Location.X) ||
        !FMath::IsFinite(To.Location.Y) || !FMath::IsFinite(To.Location.Z))
    {
        return false;
    }

    if (Edge.MaxPayloadMassKg > 0.0f && Context.CargoMassKg > Edge.MaxPayloadMassKg)
    {
        return false;
    }

    const double DeltaX = static_cast<double>(To.Location.X) - From.Location.X;
    const double DeltaY = static_cast<double>(To.Location.Y) - From.Location.Y;
    const double DeltaZ = FMath::Abs(static_cast<double>(To.Location.Z) - From.Location.Z);
    const double HorizontalDistance = FMath::Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
    if (!FMath::IsFinite(HorizontalDistance) || !FMath::IsFinite(DeltaZ))
    {
        return false;
    }

    // The edge stores an authored base cost. Geometry contributes an explicit
    // grade penalty; it does not silently replace authored travel distance/time.
    const double SlopeRadians = FMath::Atan2(DeltaZ, FMath::Max(1.0, HorizontalDistance));
    const double SlopeDegrees = FMath::RadiansToDegrees(SlopeRadians);
    const double LoadPenalty = (static_cast<double>(Context.CargoMassKg) / 100.0) *
        static_cast<double>(Edge.LoadPenaltyPer100Kg);
    const double WeatherPenalty = static_cast<double>(Edge.WeatherExposure) *
        static_cast<double>(Context.WeatherSeverity) *
        static_cast<double>(Context.WeatherPenaltyPerSeverity);
    const double AdditivePenalty = SlopeDegrees * static_cast<double>(Edge.SlopePenaltyPerDegree) +
        LoadPenalty + WeatherPenalty;
    const double EffectiveCost = static_cast<double>(Edge.Cost) *
        static_cast<double>(Edge.SurfaceCostMultiplier) *
        static_cast<double>(Edge.InfrastructureCostMultiplier) * (1.0 + AdditivePenalty);

    if (!FMath::IsFinite(SlopeDegrees) || SlopeDegrees < 0.0 ||
        !FMath::IsFinite(AdditivePenalty) || AdditivePenalty < 0.0 ||
        !FMath::IsFinite(EffectiveCost) || EffectiveCost < 0.0 ||
        EffectiveCost > static_cast<double>(TNumericLimits<float>::Max()))
    {
        return false;
    }

    OutCost = static_cast<float>(EffectiveCost);
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
