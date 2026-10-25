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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Logistics", meta=(ClampMin="0.0"))
    float Cost = 1.0f;

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
};
