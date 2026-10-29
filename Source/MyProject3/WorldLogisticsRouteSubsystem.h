#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldLogisticsRouteSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FLogisticsRouteNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    FName NodeId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    bool bEnabled = true;
};

USTRUCT(BlueprintType)
struct FLogisticsRouteEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    FName FromNode = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    FName ToNode = NAME_None;

    /** Authored base cost before terrain, load, weather and infrastructure modifiers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.0"))
    float Cost = 1.0f;

    /** Non-negative rise/run ratio; 0 is flat. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost", meta=(ClampMin="0.0", ClampMax="10.0"))
    float SlopeRatio = 0.0f;

    /** Multiplier for the edge's authored surface (e.g. mud, snow, rock, road). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost", meta=(ClampMin="0.01", ClampMax="100.0"))
    float SurfaceCostMultiplier = 1.0f;

    /** Additional cost at full weather severity, scaled by the route context. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost", meta=(ClampMin="0.0", ClampMax="100.0"))
    float WeatherSensitivity = 0.0f;

    /** Additional cost at maximum configured carried mass. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost", meta=(ClampMin="0.0", ClampMax="100.0"))
    float LoadSensitivity = 0.0f;

    /** Additional cost when an edge has no built road/bridge/rope infrastructure. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost", meta=(ClampMin="0.0", ClampMax="100.0"))
    float MissingInfrastructurePenalty = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost")
    bool bHasBuiltInfrastructure = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost")
    bool bRequiresInfrastructure = false;

    /** Stable data ID used by construction/infrastructure adapters to update this edge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics|Cost")
    FName InfrastructureId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    bool bEnabled = true;
};

/** Inputs affecting every edge's deterministic route cost; values are data, not world queries. */
USTRUCT(BlueprintType)
struct FLogisticsRouteCostContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics|Cost", meta=(ClampMin="0.0", ClampMax="100000.0"))
    float CarriedMassKg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics|Cost", meta=(ClampMin="0.01", ClampMax="100000.0"))
    float MaxCarryMassKg = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics|Cost", meta=(ClampMin="0.0", ClampMax="1.0"))
    float WeatherSeverity = 0.0f;
};

/** Explicit, bounded route-cost graph; physical movement is handled by a separate adapter. */
UCLASS()
class MYPROJECT3_API UWorldLogisticsRouteSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics|Limits", meta=(ClampMin="1", ClampMax="4096"))
    int32 MaxNodes = 512;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics|Limits", meta=(ClampMin="1", ClampMax="16384"))
    int32 MaxEdges = 2048;

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool UpsertNode(const FLogisticsRouteNode& Node);

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool UpsertEdge(const FLogisticsRouteEdge& Edge);

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool SetNodeEnabled(FName NodeId, bool bEnabled);

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool SetEdgeEnabled(FName FromNode, FName ToNode, bool bEnabled);

    /** Update matching authored edges after an infrastructure item is built, repaired or lost. */
    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool SetInfrastructureBuilt(FName InfrastructureId, bool bBuilt);

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool RemoveNode(FName NodeId);

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool FindRoute(FName StartNode, FName GoalNode, TArray<FName>& OutPath, float& OutTotalCost) const;

    /** Deterministic route solve using the same terrain/load/weather context for each edge. */
    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool FindRouteWithContext(FName StartNode, FName GoalNode,
        const FLogisticsRouteCostContext& Context, TArray<FName>& OutPath,
        float& OutTotalCost) const;

    /** Pure edge-cost calculation, exposed for route previews and contract tests. */
    UFUNCTION(BlueprintPure, Category="Logistics")
    bool ComputeEdgeCost(const FLogisticsRouteEdge& Edge,
        const FLogisticsRouteCostContext& Context, float& OutCost) const;
    UFUNCTION(BlueprintPure, Category="Logistics")
    bool CalculatePathCost(const TArray<FName>& Path,
        const FLogisticsRouteCostContext& Context, float& OutTotalCost) const;

    UFUNCTION(BlueprintPure, Category="Logistics")
    int32 GetNodeCount() const { return Nodes.Num(); }

    UFUNCTION(BlueprintPure, Category="Logistics")
    int32 GetEdgeCount() const { return Edges.Num(); }

    /** Deterministic route-graph persistence for infrastructure state and route revisions. */
    void ExportSnapshot(TArray<FLogisticsRouteNode>& OutNodes, TArray<FLogisticsRouteEdge>& OutEdges) const;
    bool RestoreSnapshot(const TArray<FLogisticsRouteNode>& NodeSnapshot,
        const TArray<FLogisticsRouteEdge>& EdgeSnapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="Logistics")
    TMap<FName, FLogisticsRouteNode> Nodes;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="Logistics")
    TArray<FLogisticsRouteEdge> Edges;

    int32 FindEdgeIndex(FName FromNode, FName ToNode) const;
};
