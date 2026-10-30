#include "WorldLogisticsRouteSubsystem.h"
#include "Algo/Reverse.h"
#include "Engine/World.h"
#include "WorldRegionalEventSubsystem.h"
#include "WorldGameDataSubsystem.h"

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
        !FMath::IsFinite(Edge.SlopeRatio) || Edge.SlopeRatio < 0.0f || Edge.SlopeRatio > 10.0f ||
        !FMath::IsFinite(Edge.SurfaceCostMultiplier) ||
        Edge.SurfaceCostMultiplier < 0.01f || Edge.SurfaceCostMultiplier > 100.0f ||
        !FMath::IsFinite(Edge.WeatherSensitivity) || Edge.WeatherSensitivity < 0.0f || Edge.WeatherSensitivity > 100.0f ||
        !FMath::IsFinite(Edge.LoadSensitivity) || Edge.LoadSensitivity < 0.0f || Edge.LoadSensitivity > 100.0f ||
        !FMath::IsFinite(Edge.MissingInfrastructurePenalty) ||
        Edge.MissingInfrastructurePenalty < 0.0f || Edge.MissingInfrastructurePenalty > 100.0f ||
        !FMath::IsFinite(Edge.InfrastructureCondition) || Edge.InfrastructureCondition < 0.0f ||
        !FMath::IsFinite(Edge.InfrastructureMaxCondition) || Edge.InfrastructureMaxCondition <= 0.0f ||
        Edge.InfrastructureCondition > Edge.InfrastructureMaxCondition ||
        !FMath::IsFinite(Edge.InfrastructureDegradationPenalty) || Edge.InfrastructureDegradationPenalty < 0.0f || Edge.InfrastructureDegradationPenalty > 100.0f ||
        !FMath::IsFinite(Edge.InfrastructureBuiltCostMultiplier) || Edge.InfrastructureBuiltCostMultiplier < 0.01f || Edge.InfrastructureBuiltCostMultiplier > 100.0f ||
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

bool UWorldLogisticsRouteSubsystem::SetInfrastructureBuilt(FName InfrastructureId, bool bBuilt)
{
    if (InfrastructureId.IsNone())
        return false;

    bool bMatched = false;
    for (FLogisticsRouteEdge& Edge : Edges)
    {
        if (Edge.InfrastructureId != InfrastructureId)
            continue;
        Edge.bHasBuiltInfrastructure = bBuilt && Edge.InfrastructureCondition > 0.0f;
        if (bBuilt && Edge.InfrastructureCondition <= 0.0f)
            Edge.InfrastructureCondition = Edge.InfrastructureMaxCondition;
        Edge.bHasBuiltInfrastructure = bBuilt;
        bMatched = true;
    }
    return bMatched;
}

bool UWorldLogisticsRouteSubsystem::SetInfrastructureBuiltCostMultiplier(
    FName InfrastructureId, float CostMultiplier)
{
    if (InfrastructureId.IsNone() || !FMath::IsFinite(CostMultiplier) ||
        CostMultiplier < 0.01f || CostMultiplier > 100.0f)
        return false;
    bool bMatched = false;
    for (FLogisticsRouteEdge& Edge : Edges)
        if (Edge.InfrastructureId == InfrastructureId)
        {
            Edge.InfrastructureBuiltCostMultiplier = CostMultiplier;
            bMatched = true;
        }
    return bMatched;
}

bool UWorldLogisticsRouteSubsystem::ApplyInfrastructureWear(
    FName InfrastructureId, float WearAmount)
{
    if (InfrastructureId.IsNone() || !FMath::IsFinite(WearAmount) ||
        WearAmount <= 0.0f || WearAmount > 1000000.0f)
        return false;
    bool bMatched = false;
    for (FLogisticsRouteEdge& Edge : Edges)
    {
        if (Edge.InfrastructureId != InfrastructureId || !Edge.bHasBuiltInfrastructure)
            continue;
        Edge.InfrastructureCondition = FMath::Clamp(
            Edge.InfrastructureCondition - WearAmount, 0.0f, Edge.InfrastructureMaxCondition);
        if (Edge.InfrastructureCondition <= 0.0f)
            Edge.bHasBuiltInfrastructure = false;
        bMatched = true;
    }
    return bMatched;
}

bool UWorldLogisticsRouteSubsystem::RepairInfrastructure(
    FName InfrastructureId, float RepairAmount)
{
    if (InfrastructureId.IsNone() || !FMath::IsFinite(RepairAmount) ||
        RepairAmount <= 0.0f || RepairAmount > 1000000.0f)
        return false;
    bool bMatched = false;
    for (FLogisticsRouteEdge& Edge : Edges)
    {
        if (Edge.InfrastructureId != InfrastructureId)
            continue;
        const float Before = Edge.InfrastructureCondition;
        Edge.InfrastructureCondition = FMath::Clamp(
            Edge.InfrastructureCondition + RepairAmount, 0.0f, Edge.InfrastructureMaxCondition);
        if (Edge.InfrastructureCondition > 0.0f)
            Edge.bHasBuiltInfrastructure = true;
        bMatched |= !FMath::IsNearlyEqual(Before, Edge.InfrastructureCondition);
    }
    return bMatched;
}

bool UWorldLogisticsRouteSubsystem::GetInfrastructureState(
    FName InfrastructureId, float& OutCondition, float& OutMaxCondition, bool& bOutBuilt) const
{
    OutCondition = 0.0f;
    OutMaxCondition = 0.0f;
    bOutBuilt = false;
    if (InfrastructureId.IsNone())
        return false;
    bool bFound = false;
    for (const FLogisticsRouteEdge& Edge : Edges)
    {
        if (Edge.InfrastructureId != InfrastructureId)
            continue;
        if (!bFound)
        {
            OutCondition = Edge.InfrastructureCondition;
            OutMaxCondition = Edge.InfrastructureMaxCondition;
            bOutBuilt = Edge.bHasBuiltInfrastructure;
            bFound = true;
        }
        else if (!FMath::IsNearlyEqual(OutCondition, Edge.InfrastructureCondition, 0.01f) ||
            bOutBuilt != Edge.bHasBuiltInfrastructure)
            return false; // Directional records for one physical link must agree.
    }
    return bFound;
}

bool UWorldLogisticsRouteSubsystem::AdvanceRegionalInfrastructureWear(double DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0)
        return false;
    TArray<FLogisticsRouteNode> NodeSnapshot;
    TArray<FLogisticsRouteEdge> EdgeSnapshot;
    ExportSnapshot(NodeSnapshot, EdgeSnapshot);
    const bool bHasBuiltInfrastructure = EdgeSnapshot.ContainsByPredicate(
        [](const FLogisticsRouteEdge& Edge)
        { return Edge.bHasBuiltInfrastructure && !Edge.InfrastructureId.IsNone(); });
    if (!bHasBuiltInfrastructure)
        return true;

    UWorld* World = GetWorld();
    UWorldGameDataSubsystem* GameData = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldRegionalEventSubsystem* Events = World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    if (!GameData || !GameData->IsLoaded() || !Events || !Events->AreDefinitionsLoaded())
        return false;

    TMap<FName, float> WearByInfrastructure;
    for (const FLogisticsRouteEdge& Edge : EdgeSnapshot)
    {
        if (Edge.InfrastructureId.IsNone() || !Edge.bHasBuiltInfrastructure || Edge.RegionId.IsNone())
            continue;
        const FGameDataInfrastructure* Definition = GameData->FindInfrastructure(Edge.InfrastructureId);
        if (!Definition || Definition->WearRules.IsEmpty())
            continue;
        FWorldRegionalEventEvaluation Evaluation;
        if (!Events->EvaluateRegion(Edge.RegionId, Evaluation))
            return false;
        float HourlyWear = 0.0f;
        for (const FGameDataInfrastructureWearRule& Rule : Definition->WearRules)
            if (Evaluation.ActiveEventIds.Contains(Rule.EventId))
                HourlyWear += Rule.WearPerHour;
        if (!FMath::IsFinite(HourlyWear) || HourlyWear < 0.0f)
            return false;
        if (HourlyWear > 0.0f)
        {
            float& Total = WearByInfrastructure.FindOrAdd(Edge.InfrastructureId);
            Total = FMath::Max(Total, HourlyWear * static_cast<float>(DeltaSeconds / 3600.0));
        }
    }
    TArray<FName> InfrastructureIds;
    WearByInfrastructure.GetKeys(InfrastructureIds);
    InfrastructureIds.Sort([](const FName& A, const FName& B)
        { return A.ToString() < B.ToString(); });
    for (const FName Id : InfrastructureIds)
        if (!ApplyInfrastructureWear(Id, WearByInfrastructure.FindRef(Id)))
            return false;
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

bool UWorldLogisticsRouteSubsystem::ComputeEdgeCost(
    const FLogisticsRouteEdge& Edge,
    const FLogisticsRouteCostContext& Context,
    float& OutCost) const
{
    OutCost = 0.0f;
    if (!FMath::IsFinite(Edge.Cost) || Edge.Cost < 0.0f ||
        !FMath::IsFinite(Edge.SlopeRatio) || Edge.SlopeRatio < 0.0f || Edge.SlopeRatio > 10.0f ||
        !FMath::IsFinite(Edge.SurfaceCostMultiplier) ||
        Edge.SurfaceCostMultiplier < 0.01f || Edge.SurfaceCostMultiplier > 100.0f ||
        !FMath::IsFinite(Edge.WeatherSensitivity) || Edge.WeatherSensitivity < 0.0f ||
        Edge.WeatherSensitivity > 100.0f ||
        !FMath::IsFinite(Edge.LoadSensitivity) || Edge.LoadSensitivity < 0.0f ||
        Edge.LoadSensitivity > 100.0f ||
        !FMath::IsFinite(Edge.MissingInfrastructurePenalty) ||
        Edge.MissingInfrastructurePenalty < 0.0f || Edge.MissingInfrastructurePenalty > 100.0f ||
        !FMath::IsFinite(Context.CarriedMassKg) || Context.CarriedMassKg < 0.0f ||
        !FMath::IsFinite(Context.MaxCarryMassKg) || Context.MaxCarryMassKg <= 0.0f ||
        Context.CarriedMassKg > Context.MaxCarryMassKg ||
        !FMath::IsFinite(Context.WeatherSeverity) ||
        Context.WeatherSeverity < 0.0f || Context.WeatherSeverity > 1.0f)
    {
        return false;
    }
    if (Edge.bRequiresInfrastructure && !Edge.bHasBuiltInfrastructure)
    {
        return false;
    }

    const double LoadRatio = static_cast<double>(Context.CarriedMassKg) /
        static_cast<double>(Context.MaxCarryMassKg);
    const double SlopeFactor = 1.0 + static_cast<double>(Edge.SlopeRatio) * 0.75;
    const double SurfaceFactor = Edge.SurfaceCostMultiplier;
    const double WeatherFactor = 1.0 +
        static_cast<double>(Edge.WeatherSensitivity) * Context.WeatherSeverity;
    const double LoadFactor = 1.0 +
        static_cast<double>(Edge.LoadSensitivity) * LoadRatio;
    if (Edge.bHasBuiltInfrastructure && Edge.InfrastructureCondition <= 0.0f)
        return false;
    const double ConditionRatio = FMath::Clamp(
        static_cast<double>(Edge.InfrastructureCondition) /
            static_cast<double>(Edge.InfrastructureMaxCondition), 0.0, 1.0);
    const double DegradationFactor = 1.0 +
        (1.0 - ConditionRatio) * static_cast<double>(Edge.InfrastructureDegradationPenalty);
    const double InfrastructureFactor = Edge.bHasBuiltInfrastructure
        ? static_cast<double>(Edge.InfrastructureBuiltCostMultiplier) * DegradationFactor
        : 1.0 + static_cast<double>(Edge.MissingInfrastructurePenalty);
    double RegionalHazardFactor = 1.0;
    if (!Edge.RegionId.IsNone())
    {
        const UWorldRegionalEventSubsystem* Events =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
        FWorldRegionalEventEvaluation Evaluation;
        if (Events && Events->AreDefinitionsLoaded() &&
            Events->EvaluateRegion(Edge.RegionId, Evaluation))
        {
            RegionalHazardFactor = Evaluation.RouteCostMultiplier;
        }
    }
    if (!FMath::IsFinite(RegionalHazardFactor) || RegionalHazardFactor <= 0.0 ||
        RegionalHazardFactor > 100000.0)
    {
        return false;
    }

    const double Cost = static_cast<double>(Edge.Cost) * SlopeFactor *
        SurfaceFactor * WeatherFactor * LoadFactor * InfrastructureFactor *
        RegionalHazardFactor;
    if (!FMath::IsFinite(Cost) || Cost < 0.0 ||
        Cost > static_cast<double>(TNumericLimits<float>::Max()))
    {
        return false;
    }
    OutCost = static_cast<float>(Cost);
    return true;
}

bool UWorldLogisticsRouteSubsystem::FindRoute(
    FName StartNode, FName GoalNode, TArray<FName>& OutPath, float& OutTotalCost) const
{
    return FindRouteWithContext(StartNode, GoalNode,
        FLogisticsRouteCostContext(), OutPath, OutTotalCost);
}

bool UWorldLogisticsRouteSubsystem::FindRouteWithContext(
    FName StartNode, FName GoalNode,
    const FLogisticsRouteCostContext& Context,
    TArray<FName>& OutPath, float& OutTotalCost) const
{
    OutPath.Reset();
    OutTotalCost = 0.0f;

    // Validate once at the boundary; all edge costs then use the exact same context.
    if (!FMath::IsFinite(Context.CarriedMassKg) || Context.CarriedMassKg < 0.0f ||
        !FMath::IsFinite(Context.MaxCarryMassKg) || Context.MaxCarryMassKg <= 0.0f ||
        Context.CarriedMassKg > Context.MaxCarryMassKg ||
        !FMath::IsFinite(Context.WeatherSeverity) ||
        Context.WeatherSeverity < 0.0f || Context.WeatherSeverity > 1.0f)
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

    // Sort tie cases explicitly: route choice must not depend on TMap/TSet iteration order.
    TArray<FLogisticsRouteEdge> SortedEdges = Edges;
    SortedEdges.Sort([](const FLogisticsRouteEdge& A, const FLogisticsRouteEdge& B)
    {
        if (A.FromNode != B.FromNode)
            return A.FromNode.ToString() < B.FromNode.ToString();
        return A.ToNode.ToString() < B.ToNode.ToString();
    });

    // O(V^2 + E) bounded Dijkstra is sufficient for this deliberately small graph.
    while (!Unvisited.IsEmpty())
    {
        FName Current = NAME_None;
        float BestDistance = TNumericLimits<float>::Max();
        TArray<FName> SortedUnvisited = Unvisited.Array();
        SortedUnvisited.Sort([](const FName& A, const FName& B)
            { return A.ToString() < B.ToString(); });
        for (const FName Candidate : SortedUnvisited)
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

        for (const FLogisticsRouteEdge& Edge : SortedEdges)
        {
            if (!Edge.bEnabled || Edge.FromNode != Current ||
                !Unvisited.Contains(Edge.ToNode))
            {
                continue;
            }
            float EdgeCost = 0.0f;
            if (!ComputeEdgeCost(Edge, Context, EdgeCost))
            {
                // Invalid or infrastructure-blocked edges are unavailable, not zero-cost.
                continue;
            }
            const float* CurrentDistance = Distances.Find(Current);
            const float* NeighborDistance = Distances.Find(Edge.ToNode);
            if (!CurrentDistance || !NeighborDistance)
            {
                continue;
            }
            const double CandidateCost = static_cast<double>(*CurrentDistance) + EdgeCost;
            if (!FMath::IsFinite(CandidateCost) || CandidateCost > TNumericLimits<float>::Max())
            {
                continue;
            }
            // Strictly-lower updates plus sorted traversal make equal-cost route choice stable.
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

bool UWorldLogisticsRouteSubsystem::CalculatePathCost(
    const TArray<FName>& Path,
    const FLogisticsRouteCostContext& Context,
    float& OutTotalCost) const
{
    OutTotalCost = 0.0f;
    if (Path.Num() < 2 ||
        !FMath::IsFinite(Context.CarriedMassKg) || Context.CarriedMassKg < 0.0f ||
        !FMath::IsFinite(Context.MaxCarryMassKg) || Context.MaxCarryMassKg <= 0.0f ||
        Context.CarriedMassKg > Context.MaxCarryMassKg ||
        !FMath::IsFinite(Context.WeatherSeverity) ||
        Context.WeatherSeverity < 0.0f || Context.WeatherSeverity > 1.0f)
    {
        return false;
    }

    double TotalCost = 0.0;
    for (int32 Index = 0; Index < Path.Num(); ++Index)
    {
        const FLogisticsRouteNode* Node = Nodes.Find(Path[Index]);
        if (!Node || !Node->bEnabled)
            return false;
        if (Index == 0)
            continue;

        const int32 EdgeIndex = FindEdgeIndex(Path[Index - 1], Path[Index]);
        if (!Edges.IsValidIndex(EdgeIndex))
            return false;
        const FLogisticsRouteEdge& Edge = Edges[EdgeIndex];
        if (!Edge.bEnabled)
            return false;

        float EdgeCost = 0.0f;
        if (!ComputeEdgeCost(Edge, Context, EdgeCost))
            return false;
        TotalCost += EdgeCost;
        if (!FMath::IsFinite(TotalCost) ||
            TotalCost > static_cast<double>(TNumericLimits<float>::Max()))
            return false;
    }

    OutTotalCost = static_cast<float>(TotalCost);
    return true;
}

void UWorldLogisticsRouteSubsystem::ExportSnapshot(
    TArray<FLogisticsRouteNode>& OutNodes, TArray<FLogisticsRouteEdge>& OutEdges) const
{
    OutNodes.Reset();
    OutEdges.Reset();
    Nodes.GenerateValueArray(OutNodes);
    OutEdges = Edges;
    OutNodes.Sort([](const FLogisticsRouteNode& A, const FLogisticsRouteNode& B)
    {
        return A.NodeId.ToString() < B.NodeId.ToString();
    });
    OutEdges.Sort([](const FLogisticsRouteEdge& A, const FLogisticsRouteEdge& B)
    {
        if (A.FromNode != B.FromNode)
            return A.FromNode.ToString() < B.FromNode.ToString();
        return A.ToNode.ToString() < B.ToNode.ToString();
    });
}

bool UWorldLogisticsRouteSubsystem::RestoreSnapshot(
    const TArray<FLogisticsRouteNode>& NodeSnapshot,
    const TArray<FLogisticsRouteEdge>& EdgeSnapshot)
{
    if (NodeSnapshot.Num() > MaxNodes || EdgeSnapshot.Num() > MaxEdges ||
        MaxNodes < 1 || MaxNodes > 4096 || MaxEdges < 1 || MaxEdges > 16384)
        return false;

    TMap<FName, FLogisticsRouteNode> NewNodes;
    for (const FLogisticsRouteNode& Node : NodeSnapshot)
    {
        if (Node.NodeId.IsNone() || !FMath::IsFinite(Node.Location.X) ||
            !FMath::IsFinite(Node.Location.Y) || !FMath::IsFinite(Node.Location.Z) ||
            NewNodes.Contains(Node.NodeId))
            return false;
        NewNodes.Add(Node.NodeId, Node);
    }

    TArray<FLogisticsRouteEdge> NewEdges;
    TSet<FString> SeenPairs;
    for (const FLogisticsRouteEdge& Edge : EdgeSnapshot)
    {
        const FString PairKey = Edge.FromNode.ToString() + TEXT("->") + Edge.ToNode.ToString();
        if (Edge.FromNode.IsNone() || Edge.ToNode.IsNone() || Edge.FromNode == Edge.ToNode ||
            !NewNodes.Contains(Edge.FromNode) || !NewNodes.Contains(Edge.ToNode) ||
            SeenPairs.Contains(PairKey) ||
            !FMath::IsFinite(Edge.Cost) || Edge.Cost < 0.0f ||
            !FMath::IsFinite(Edge.SlopeRatio) || Edge.SlopeRatio < 0.0f || Edge.SlopeRatio > 10.0f ||
            !FMath::IsFinite(Edge.SurfaceCostMultiplier) ||
            Edge.SurfaceCostMultiplier < 0.01f || Edge.SurfaceCostMultiplier > 100.0f ||
            !FMath::IsFinite(Edge.WeatherSensitivity) ||
            Edge.WeatherSensitivity < 0.0f || Edge.WeatherSensitivity > 100.0f ||
            !FMath::IsFinite(Edge.LoadSensitivity) ||
            Edge.LoadSensitivity < 0.0f || Edge.LoadSensitivity > 100.0f ||
            !FMath::IsFinite(Edge.MissingInfrastructurePenalty) ||
            Edge.MissingInfrastructurePenalty < 0.0f || Edge.MissingInfrastructurePenalty > 100.0f ||
            !FMath::IsFinite(Edge.InfrastructureCondition) || Edge.InfrastructureCondition < 0.0f ||
            !FMath::IsFinite(Edge.InfrastructureMaxCondition) || Edge.InfrastructureMaxCondition <= 0.0f ||
            Edge.InfrastructureCondition > Edge.InfrastructureMaxCondition ||
            !FMath::IsFinite(Edge.InfrastructureDegradationPenalty) || Edge.InfrastructureDegradationPenalty < 0.0f || Edge.InfrastructureDegradationPenalty > 100.0f ||
            !FMath::IsFinite(Edge.InfrastructureBuiltCostMultiplier) || Edge.InfrastructureBuiltCostMultiplier < 0.01f || Edge.InfrastructureBuiltCostMultiplier > 100.0f ||
            (Edge.bRequiresInfrastructure && Edge.InfrastructureId.IsNone()))
            return false;
        SeenPairs.Add(PairKey);
        NewEdges.Add(Edge);
    }

    NewEdges.Sort([](const FLogisticsRouteEdge& A, const FLogisticsRouteEdge& B)
    {
        if (A.FromNode != B.FromNode)
            return A.FromNode.ToString() < B.FromNode.ToString();
        return A.ToNode.ToString() < B.ToNode.ToString();
    });
    Nodes = MoveTemp(NewNodes);
    Edges = MoveTemp(NewEdges);
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
