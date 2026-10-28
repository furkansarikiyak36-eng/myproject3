#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayCoreTypes.h"
#include "TerrainWorldStateTypes.h"
#include "TerrainWorldStateSubsystem.generated.h"

/** Bounded terrain-state ledger. It does not edit Landscape meshes or collision by itself. */
UCLASS()
class MYPROJECT3_API UTerrainWorldStateSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Terrain|State")
    FWorldOperationResult ConfigurePolicy(const FTerrainEditPolicy& NewPolicy, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Terrain|State")
    FWorldOperationResult RegisterCell(const FTerrainChunkKey& Chunk, const FTerrainChunkKey& Cell,
        ETerrainEditLayer Layer, FGuid OperationId);

    UFUNCTION(BlueprintCallable, Category = "Terrain|Digging")
    FWorldOperationResult TryExcavateCell(const FTerrainChunkKey& Chunk, const FTerrainChunkKey& Cell,
        float DepthCm, float CellVolumeM3, FGuid OperationId);

    UFUNCTION(BlueprintPure, Category = "Terrain|State")
    bool GetCellState(const FTerrainChunkKey& Chunk, const FTerrainChunkKey& Cell,
        FTerrainCellState& OutState) const;

    UFUNCTION(BlueprintPure, Category = "Terrain|State")
    float GetExcavatedVolumeM3() const { return ExcavatedVolumeM3; }

private:
    UPROPERTY(Transient)
    FTerrainEditPolicy Policy;
    UPROPERTY(Transient)
    bool bPolicyConfigured = false;
    UPROPERTY(Transient)
    TMap<FString, FTerrainCellState> Cells;
    UPROPERTY(Transient)
    float ExcavatedVolumeM3 = 0.0f;

    FString MakeCellKey(const FTerrainChunkKey& Chunk, const FTerrainChunkKey& Cell) const;
    FWorldOperationResult Failure(EWorldOperationResultCode Code, const FGuid& OperationId,
        const TCHAR* Message) const;
};
