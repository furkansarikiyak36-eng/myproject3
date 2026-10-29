#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldTerrainStateSubsystem.generated.h"

class UVoxelTerrainChunkComponent;

UENUM(BlueprintType)
enum class ETerrainCellEdit : uint8
{
    Excavate,
    Fill,
    ReplaceMaterial
};

USTRUCT(BlueprintType)
struct FWorldTerrainCellState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Terrain")
    FIntVector Cell = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Terrain", meta=(ClampMin="0"))
    int32 DepthLayer = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Terrain")
    FName MaterialId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Terrain")
    bool bExcavated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Terrain")
    int64 Revision = 1;
};

/** Data-driven extraction yield for one bounded terrain material/depth band. */
USTRUCT(BlueprintType)
struct FWorldTerrainResourceYieldRule
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources") FName SourceMaterialId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources") FName OutputDefinitionId = NAME_None;
    /** Empty means all regions; otherwise this material is valid only in these geology regions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources") TArray<FName> GeologyRegions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="1", ClampMax="10000")) int32 GeologyWeight = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="0", ClampMax="63")) int32 MinimumDepthLayer = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="0", ClampMax="63")) int32 MaximumDepthLayer = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="0.01", ClampMax="100000")) float UnitsPerCubicMetre = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="1", ClampMax="64")) int32 MaxUnitsPerOperation = 16;
};

USTRUCT()
struct FWorldTerrainOperationReceipt
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid OperationId;

    UPROPERTY(SaveGame)
    FIntVector Cell = FIntVector::ZeroValue;

    UPROPERTY(SaveGame)
    int64 ResultingRevision = 0;

    // Immutable committed state returned by idempotent retries.
    UPROPERTY(SaveGame)
    FWorldTerrainCellState ResultState;

    UPROPERTY(SaveGame)
    int32 RequestedDepthLayer = 0;

    UPROPERTY(SaveGame)
    ETerrainCellEdit RequestedEdit = ETerrainCellEdit::Excavate;

    UPROPERTY(SaveGame)
    FName RequestedMaterialId = NAME_None;

    UPROPERTY(SaveGame)
    int64 ExpectedRevision = 0;

    /** Non-empty only for a successful resource-extraction transaction. */
    UPROPERTY(SaveGame)
    FGuid VoxelChunkActorIdentityId;

    UPROPERTY(SaveGame)
    int64 ExpectedDensityRevision = 0;

    UPROPERTY(SaveGame)
    int64 ResultingDensityRevision = 0;

    UPROPERTY(SaveGame)
    float RemovedVolumeCubicMetres = 0.0f;

    UPROPERTY(SaveGame)
    FName YieldSourceMaterialId = NAME_None;

    UPROPERTY(SaveGame)
    FName YieldOutputDefinitionId = NAME_None;

    UPROPERTY(SaveGame)
    float YieldUnitsPerCubicMetre = 0.0f;

    UPROPERTY(SaveGame)
    int32 YieldOutputUnits = 0;

    UPROPERTY(SaveGame)
    TArray<FGuid> YieldOutputItemIds;
};

/** Bounded, layer-gated terrain state. Rendering/collision edits belong to a terrain adapter. */
UCLASS()
class MYPROJECT3_API UWorldTerrainStateSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Limits", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxEditableDepthLayers = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Limits", meta=(ClampMin="1", ClampMax="1000000"))
    int32 MaxEditedCells = 50000;

    /** Receipt retention is bounded; once full, new edits fail closed rather than lose retry safety. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Limits", meta=(ClampMin="1", ClampMax="1000000"))
    int32 MaxOperationReceipts = 100000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources|Limits", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxResourceYieldRules = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources")
    TArray<FWorldTerrainResourceYieldRule> ResourceYieldRules;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Limits", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxDepthPerCell = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain")
    TSet<int32> PermittedDepthLayers;

    UFUNCTION(BlueprintCallable, Category="World|Terrain")
    bool TryApplyCellEdit(const FGuid& OperationId, const FIntVector& Cell,
        int32 DepthLayer, ETerrainCellEdit Edit, FName MaterialId,
        int64 ExpectedRevision, FWorldTerrainCellState& OutState);

    UFUNCTION(BlueprintCallable, Category="World|Terrain|Voxel")
    bool TryApplyVoxelExcavation(const FGuid& OperationId,
        UVoxelTerrainChunkComponent* VoxelChunk, FVector LocalCenter, float RadiusCm,
        float ToolStrength, int64 ExpectedDensityRevision,
        const FIntVector& LogicalCell, int32 DepthLayer, int64 ExpectedCellRevision,
        FName MaterialId, FWorldTerrainCellState& OutState);

    /** Resolve geology by region, depth and stable cell/world seed. */
    UFUNCTION(BlueprintPure, Category="World|Terrain|Geology")
    bool ResolveGeologyMaterial(const FIntVector& LogicalCell, FName RegionId, int32 DepthLayer,
        int32 WorldSeed, FName& OutMaterialId) const;

    /** Excavate using saved cell material, or resolve geology on first dig. */
    UFUNCTION(BlueprintCallable, Category="World|Terrain|Geology")
    bool TryExcavateForGeology(const FGuid& OperationId, UVoxelTerrainChunkComponent* VoxelChunk,
        FVector LocalCenter, float RadiusCm, float ToolStrength, int64 ExpectedDensityRevision,
        const FIntVector& LogicalCell, int32 DepthLayer, int64 ExpectedCellRevision,
        FName RegionId, int32 WorldSeed, FWorldTerrainCellState& OutState,
        TArray<FGuid>& OutResourceItemIds);

    /** Atomic bounded excavation that also registers deterministically-yielded resources in ownership. */
    UFUNCTION(BlueprintCallable, Category="World|Terrain|Resources")
    bool TryExcavateForResource(const FGuid& OperationId,
        UVoxelTerrainChunkComponent* VoxelChunk, FVector LocalCenter, float RadiusCm,
        float ToolStrength, int64 ExpectedDensityRevision,
        const FIntVector& LogicalCell, int32 DepthLayer, int64 ExpectedCellRevision,
        FName SourceMaterialId, FWorldTerrainCellState& OutState,
        TArray<FGuid>& OutResourceItemIds);

    UFUNCTION(BlueprintPure, Category="World|Terrain")
    bool TryGetCell(const FIntVector& Cell, FWorldTerrainCellState& OutState) const;

    UFUNCTION(BlueprintPure, Category="World|Terrain")
    int32 GetEditedCellCount() const { return Cells.Num(); }
    void ExportSnapshot(TArray<FWorldTerrainCellState>& OutCells) const;
    void ExportSnapshot(TArray<FWorldTerrainCellState>& OutCells,
        TArray<FWorldTerrainOperationReceipt>& OutReceipts) const;
    bool RestoreSnapshot(const TArray<FWorldTerrainCellState>& Snapshot);
    bool RestoreSnapshot(const TArray<FWorldTerrainCellState>& Snapshot,
        const TArray<FWorldTerrainOperationReceipt>& ReceiptSnapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Terrain")
    TMap<FIntVector, FWorldTerrainCellState> Cells;

    UPROPERTY(Transient)
    TMap<FGuid, FWorldTerrainOperationReceipt> Receipts;
};
