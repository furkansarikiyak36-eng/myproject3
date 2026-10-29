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

/** Caller-supplied context for a deterministic, asset-independent route-cost estimate. */
USTRUCT(BlueprintType)
struct FLogisticsRouteCostContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics")
    float CargoMassKg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics", meta=(ClampMin="0.0", ClampMax="1.0"))
    float WeatherSeverity = 0.0f;

    /** Extra cost added per unit of edge weather exposure at severity 1.0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Logistics", meta=(ClampMin="0.0", ClampMax="10.0"))
    float WeatherPenaltyPerSeverity = 0.5f;
};

USTRUCT(BlueprintType)
struct FLogisticsRouteEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    FName FromNode = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    FName ToNode = NAME_None;

    /** Authored base traversal cost; not a physical distance or a time value by itself. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.0"))
    float Cost = 1.0f;

    /** Surface and infrastructure remain authored data until world actors provide adapters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.25", ClampMax="10.0"))
    float SurfaceCostMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.1", ClampMax="4.0"))
    float InfrastructureCostMultiplier = 1.0f;

    /** Angle penalty applied to the absolute endpoint slope, in degrees. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.0", ClampMax="0.25"))
    float SlopePenaltyPerDegree = 0.01f;

    /** Load penalty added per 100 kg of carried mass. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.0", ClampMax="10.0"))
    float LoadPenaltyPer100Kg = 0.0f;

    /** 0 means unlimited. A configured limit makes an overloaded edge unavailable. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.0", ClampMax="1000000.0"))
    float MaxPayloadMassKg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.0", ClampMax="1.0"))
    float WeatherExposure = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics")
    bool bEnabled = true;
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

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool RemoveNode(FName NodeId);

    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool FindRoute(FName StartNode, FName GoalNode, TArray<FName>& OutPath, float& OutTotalCost) const;

    /** Same bounded Dijkstra solver with caller-supplied weather/load context. */
    UFUNCTION(BlueprintCallable, Category="Logistics")
    bool FindRouteWithContext(FName StartNode, FName GoalNode,
        const FLogisticsRouteCostContext& Context,
        TArray<FName>& OutPath, float& OutTotalCost) const;

    UFUNCTION(BlueprintPure, Category="Logistics")
    int32 GetNodeCount() const { return Nodes.Num(); }

    UFUNCTION(BlueprintPure, Category="Logistics")
    int32 GetEdgeCount() const { return Edges.Num(); }

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="Logistics")
    TMap<FName, FLogisticsRouteNode> Nodes;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="Logistics")
    TArray<FLogisticsRouteEdge> Edges;

    int32 FindEdgeIndex(FName FromNode, FName ToNode) const;
    bool ComputeEffectiveEdgeCost(const FLogisticsRouteEdge& Edge,
        const FLogisticsRouteNode& From, const FLogisticsRouteNode& To,
        const FLogisticsRouteCostContext& Context, float& OutCost) const;
};
