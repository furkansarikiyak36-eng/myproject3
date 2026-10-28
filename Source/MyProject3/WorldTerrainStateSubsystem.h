#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
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

    UFUNCTION(BlueprintCallable, Category="World|Terrain")
    bool TryApplyCellEdit(const FGuid& OperationId, const FIntVector& Cell,
        int32 DepthLayer, ETerrainCellEdit Edit, FName MaterialId,
        int64 ExpectedRevision, FWorldTerrainCellState& OutState);

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
};
