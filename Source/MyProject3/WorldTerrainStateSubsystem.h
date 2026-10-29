#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldTerrainStateSubsystem.generated.h"

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

    /** Deposit contents persist through fill/material changes to prevent repeated excavation farming. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Terrain|Resources")
    FName ResourceDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Terrain|Resources", meta=(ClampMin="0", ClampMax="1000000"))
    int32 RemainingResourceUnits = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Terrain|Resources")
    bool bResourceDepositInitialized = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Terrain")
    int64 Revision = 1;
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

    UPROPERTY(SaveGame)
    bool bYieldAware = false;

    UPROPERTY(SaveGame)
    FName YieldedResourceDefinitionId = NAME_None;

    UPROPERTY(SaveGame)
    int32 YieldedResourceUnits = 0;

    UPROPERTY(SaveGame)
    FGuid OutputWorldItemId;
};

USTRUCT(BlueprintType)
struct FWorldTerrainResourceYieldRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources")
    FName SourceMaterialId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="0", ClampMax="63"))
    int32 MinDepthLayer = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="0", ClampMax="63"))
    int32 MaxDepthLayer = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources")
    FName ResourceDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="1", ClampMax="1000000"))
    int32 InitialDepositUnitsPerCell = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources", meta=(ClampMin="1", ClampMax="1000000"))
    int32 YieldUnitsPerExcavation = 1;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Limits", meta=(ClampMin="1", ClampMax="2000000"))
    int32 MaxOperationReceipts = 100000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Limits", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxDepthPerCell = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain")
    TSet<int32> PermittedDepthLayers;

    /** Deterministic material/depth to resource-yield contracts; no runtime mesh is assumed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Terrain|Resources")
    TArray<FWorldTerrainResourceYieldRule> ResourceYieldRules;

    UFUNCTION(BlueprintCallable, Category="World|Terrain")
    bool TryApplyCellEdit(const FGuid& OperationId, const FIntVector& Cell,
        int32 DepthLayer, ETerrainCellEdit Edit, FName MaterialId,
        int64 ExpectedRevision, FWorldTerrainCellState& OutState);

    /** Excavates a registered resource-bearing cell and registers its yield as one ledger-owned stack. */
    UFUNCTION(BlueprintCallable, Category="World|Terrain|Resources")
    bool TryExcavateCellWithYield(const FGuid& OperationId, const FIntVector& Cell,
        int32 DepthLayer, int64 ExpectedRevision, FVector ResourceWorldLocation,
        FWorldTerrainCellState& OutState, FWorldOwnedItemRecord& OutResourceItem);

    UFUNCTION(BlueprintPure, Category="World|Terrain")
    bool TryGetCell(const FIntVector& Cell, FWorldTerrainCellState& OutState) const;

    UFUNCTION(BlueprintPure, Category="World|Terrain")
    int32 GetEditedCellCount() const { return Cells.Num(); }
    void ExportSnapshot(TArray<FWorldTerrainCellState>& OutCells) const;
    void ExportSnapshot(TArray<FWorldTerrainCellState>& OutCells,
        TArray<FWorldTerrainOperationReceipt>& OutReceipts) const;
    bool RestoreSnapshot(const TArray<FWorldTerrainCellState>& Snapshot);
    bool RestoreSnapshot(const TArray<FWorldTerrainCellState>& CellSnapshot,
        const TArray<FWorldTerrainOperationReceipt>& ReceiptSnapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Terrain")
    TMap<FIntVector, FWorldTerrainCellState> Cells;

    UPROPERTY(Transient)
    TMap<FGuid, FWorldTerrainOperationReceipt> Receipts;

    bool TryApplyCellEditInternal(const FGuid& OperationId, const FIntVector& Cell,
        int32 DepthLayer, ETerrainCellEdit Edit, FName MaterialId,
        int64 ExpectedRevision, bool bYieldAware, FWorldTerrainCellState& OutState);
    const FWorldTerrainResourceYieldRule* FindResourceYieldRule(
        const FWorldTerrainCellState& Cell) const;
};
