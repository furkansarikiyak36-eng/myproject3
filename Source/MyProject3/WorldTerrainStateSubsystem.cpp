#include "WorldTerrainStateSubsystem.h"

bool UWorldTerrainStateSubsystem::TryApplyCellEdit(
    const FGuid& OperationId, const FIntVector& Cell, int32 DepthLayer,
    ETerrainCellEdit Edit, FName MaterialId, int64 ExpectedRevision,
    FWorldTerrainCellState& OutState)
{
    if (!OperationId.IsValid() || DepthLayer < 0 ||
        static_cast<uint8>(Edit) > static_cast<uint8>(ETerrainCellEdit::ReplaceMaterial) ||
        DepthLayer >= MaxEditableDepthLayers || DepthLayer >= MaxDepthPerCell ||
        MaxEditedCells <= 0 || MaxDepthPerCell <= 0 ||
        (PermittedDepthLayers.Num() > 0 && !PermittedDepthLayers.Contains(DepthLayer)))
    {
        return false;
    }

    if (const FWorldTerrainOperationReceipt* Receipt = Receipts.Find(OperationId))
    {
        if (Receipt->Cell != Cell || Receipt->RequestedDepthLayer != DepthLayer ||
            Receipt->RequestedEdit != Edit || Receipt->RequestedMaterialId != MaterialId ||
            Receipt->ExpectedRevision != ExpectedRevision)
        {
            return false;
        }
        OutState = Receipt->ResultState;
        return true;
    }

    FWorldTerrainCellState* Existing = Cells.Find(Cell);
    const int64 CurrentRevision = Existing ? Existing->Revision : 0;
    if (ExpectedRevision != CurrentRevision || (Existing && Existing->Revision == MAX_int64))
    {
        return false;
    }
    if (!Existing && Cells.Num() >= MaxEditedCells) return false;
    if ((Edit == ETerrainCellEdit::Fill || Edit == ETerrainCellEdit::ReplaceMaterial) &&
        MaterialId.IsNone())
    {
        return false;
    }
    if (Edit == ETerrainCellEdit::Excavate && Existing && Existing->bExcavated)
    {
        return false;
    }
    if (Edit == ETerrainCellEdit::Fill && (!Existing || !Existing->bExcavated))
    {
        return false;
    }
    if (Edit == ETerrainCellEdit::ReplaceMaterial && !Existing)
    {
        return false;
    }

    FWorldTerrainCellState Updated = Existing ? *Existing : FWorldTerrainCellState();
    Updated.Cell = Cell;
    Updated.DepthLayer = DepthLayer;
    if (Edit == ETerrainCellEdit::Excavate)
    {
        Updated.bExcavated = true;
    }
    else if (Edit == ETerrainCellEdit::Fill)
    {
        Updated.bExcavated = false;
        Updated.MaterialId = MaterialId;
    }
    else
    {
        Updated.MaterialId = MaterialId;
    }
    Updated.Revision = CurrentRevision + 1;
    Cells.Add(Cell, Updated);

    FWorldTerrainOperationReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.Cell = Cell;
    Receipt.ResultingRevision = Updated.Revision;
    Receipt.ResultState = Updated;
    Receipt.RequestedDepthLayer = DepthLayer;
    Receipt.RequestedEdit = Edit;
    Receipt.RequestedMaterialId = MaterialId;
    Receipt.ExpectedRevision = ExpectedRevision;
    Receipts.Add(OperationId, Receipt);
    OutState = Updated;
    return true;
}

bool UWorldTerrainStateSubsystem::TryGetCell(
    const FIntVector& Cell, FWorldTerrainCellState& OutState) const
{
    const FWorldTerrainCellState* Found = Cells.Find(Cell);
    if (!Found) return false;
    OutState = *Found;
    return true;
}

void UWorldTerrainStateSubsystem::ExportSnapshot(TArray<FWorldTerrainCellState>& OutCells) const
{
    OutCells.Reset();
    Cells.GenerateValueArray(OutCells);
    OutCells.Sort([](const FWorldTerrainCellState& A, const FWorldTerrainCellState& B)
    {
        if (A.Cell.X != B.Cell.X) return A.Cell.X < B.Cell.X;
        if (A.Cell.Y != B.Cell.Y) return A.Cell.Y < B.Cell.Y;
        return A.Cell.Z < B.Cell.Z;
    });
}

bool UWorldTerrainStateSubsystem::RestoreSnapshot(const TArray<FWorldTerrainCellState>& Snapshot)
{
    if (Snapshot.Num() > MaxEditedCells || MaxEditedCells <= 0) return false;
    TMap<FIntVector, FWorldTerrainCellState> Proposed;
    for (const FWorldTerrainCellState& Cell : Snapshot)
    {
        if (Cell.DepthLayer < 0 || Cell.DepthLayer >= MaxEditableDepthLayers ||
            Cell.DepthLayer >= MaxDepthPerCell || Cell.Revision <= 0 ||
            Proposed.Contains(Cell.Cell) ||
            (PermittedDepthLayers.Num() > 0 && !PermittedDepthLayers.Contains(Cell.DepthLayer)))
            return false;
        Proposed.Add(Cell.Cell, Cell);
    }
    Cells = MoveTemp(Proposed);
    Receipts.Reset();
    return true;
}
