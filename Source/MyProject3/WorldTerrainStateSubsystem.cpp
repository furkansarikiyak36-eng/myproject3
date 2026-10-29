#include "WorldTerrainStateSubsystem.h"

bool UWorldTerrainStateSubsystem::TryApplyCellEdit(
    const FGuid& OperationId, const FIntVector& Cell, int32 DepthLayer,
    ETerrainCellEdit Edit, FName MaterialId, int64 ExpectedRevision,
    FWorldTerrainCellState& OutState)
{
    return TryApplyCellEditInternal(OperationId, Cell, DepthLayer, Edit, MaterialId,
        ExpectedRevision, false, OutState);
}

const FWorldTerrainResourceYieldRule* UWorldTerrainStateSubsystem::FindResourceYieldRule(
    const FWorldTerrainCellState& Cell) const
{
    const FWorldTerrainResourceYieldRule* Found = nullptr;
    for (const FWorldTerrainResourceYieldRule& Rule : ResourceYieldRules)
    {
        if (Rule.ResourceDefinitionId.IsNone() ||
            Rule.MinDepthLayer < 0 || Rule.MaxDepthLayer < Rule.MinDepthLayer ||
            Rule.InitialDepositUnitsPerCell <= 0 || Rule.YieldUnitsPerExcavation <= 0 ||
            Rule.InitialDepositUnitsPerCell > 1000000 || Rule.YieldUnitsPerExcavation > 1000000)
            continue;

        const bool bMatches = Cell.bResourceDepositInitialized
            ? Rule.ResourceDefinitionId == Cell.ResourceDefinitionId
            : (!Rule.SourceMaterialId.IsNone() && Rule.SourceMaterialId == Cell.MaterialId &&
               Cell.DepthLayer >= Rule.MinDepthLayer && Cell.DepthLayer <= Rule.MaxDepthLayer);
        if (!bMatches) continue;
        if (Found) return nullptr; // duplicate matching rules are ambiguous: fail closed
        Found = &Rule;
    }
    return Found;
}

bool UWorldTerrainStateSubsystem::TryApplyCellEditInternal(
    const FGuid& OperationId, const FIntVector& Cell, int32 DepthLayer,
    ETerrainCellEdit Edit, FName MaterialId, int64 ExpectedRevision,
    bool bYieldAware, FWorldTerrainCellState& OutState)
{
    if (!OperationId.IsValid() ||
        static_cast<uint8>(Edit) > static_cast<uint8>(ETerrainCellEdit::ReplaceMaterial))
        return false;

    // An exact retry returns the immutable committed result before consulting
    // mutable current limits/permissions. Tightening configuration must not turn
    // an already-committed operation into a second, divergent request.
    if (const FWorldTerrainOperationReceipt* Receipt = Receipts.Find(OperationId))
    {
        if (Receipt->Cell != Cell || Receipt->RequestedDepthLayer != DepthLayer ||
            Receipt->RequestedEdit != Edit || Receipt->RequestedMaterialId != MaterialId ||
            Receipt->ExpectedRevision != ExpectedRevision || Receipt->bYieldAware != bYieldAware)
            return false;
        OutState = Receipt->ResultState;
        return true;
    }

    if (DepthLayer < 0 || DepthLayer >= MaxEditableDepthLayers ||
        DepthLayer >= MaxDepthPerCell || MaxEditedCells <= 0 || MaxDepthPerCell <= 0 ||
        (PermittedDepthLayers.Num() > 0 && !PermittedDepthLayers.Contains(DepthLayer)) ||
        MaxOperationReceipts <= 0 || Receipts.Num() >= MaxOperationReceipts)
        return false;

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
    // Resource-bearing cells may only be excavated through the yield-aware
    // API so a logical terrain edit cannot silently delete a deposit without
    // producing its authoritative item-stack result.
    if (Edit == ETerrainCellEdit::Excavate && Existing && !bYieldAware &&
        ((Existing->bResourceDepositInitialized && Existing->RemainingResourceUnits > 0) ||
         FindResourceYieldRule(*Existing) != nullptr))
        return false;
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
    Receipt.bYieldAware = bYieldAware;
    Receipts.Add(OperationId, Receipt);
    OutState = Updated;
    return true;
}

bool UWorldTerrainStateSubsystem::TryExcavateCellWithYield(
    const FGuid& OperationId, const FIntVector& Cell, int32 DepthLayer,
    int64 ExpectedRevision, FVector ResourceWorldLocation,
    FWorldTerrainCellState& OutState, FWorldOwnedItemRecord& OutResourceItem)
{
    OutState = FWorldTerrainCellState();
    OutResourceItem = FWorldOwnedItemRecord();
    if (!OperationId.IsValid() || ExpectedRevision <= 0 || ExpectedRevision == MAX_int64 ||
        !FMath::IsFinite(ResourceWorldLocation.X) ||
        !FMath::IsFinite(ResourceWorldLocation.Y) || !FMath::IsFinite(ResourceWorldLocation.Z))
        return false;

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!Ownership) return false;

    if (const FWorldTerrainOperationReceipt* ExistingReceipt = Receipts.Find(OperationId))
    {
        if (!ExistingReceipt->bYieldAware || ExistingReceipt->Cell != Cell ||
            ExistingReceipt->RequestedDepthLayer != DepthLayer ||
            ExistingReceipt->RequestedEdit != ETerrainCellEdit::Excavate ||
            !ExistingReceipt->RequestedMaterialId.IsNone() ||
            ExistingReceipt->ExpectedRevision != ExpectedRevision)
            return false;
        OutState = ExistingReceipt->ResultState;
        if (ExistingReceipt->YieldedResourceUnits == 0)
            return !ExistingReceipt->OutputWorldItemId.IsValid();
        FWorldOwnedItemRecord ExistingOutput;
        if (!ExistingReceipt->OutputWorldItemId.IsValid() ||
            !Ownership->TryGetItem(ExistingReceipt->OutputWorldItemId, ExistingOutput) ||
            ExistingOutput.OwnerKind != EWorldItemOwnerKind::World ||
            ExistingOutput.DefinitionId != ExistingReceipt->YieldedResourceDefinitionId ||
            ExistingOutput.Quantity != ExistingReceipt->YieldedResourceUnits)
            return false;
        OutResourceItem = ExistingOutput;
        return true;
    }

    const FWorldTerrainCellState* Existing = Cells.Find(Cell);
    if (!Existing || Existing->bExcavated || Existing->Revision != ExpectedRevision ||
        Existing->Revision == MAX_int64 ||
        DepthLayer < 0 || DepthLayer >= MaxEditableDepthLayers ||
        DepthLayer >= MaxDepthPerCell ||
        (PermittedDepthLayers.Num() > 0 && !PermittedDepthLayers.Contains(DepthLayer)) ||
        Existing->DepthLayer != DepthLayer)
        return false;

    const FWorldTerrainResourceYieldRule* Rule = FindResourceYieldRule(*Existing);
    if (!Rule || Rule->SourceMaterialId.IsNone() ||
        Rule->ResourceDefinitionId.IsNone() ||
        Rule->MinDepthLayer < 0 || Rule->MaxDepthLayer < Rule->MinDepthLayer ||
        Rule->InitialDepositUnitsPerCell <= 0 || Rule->InitialDepositUnitsPerCell > 1000000 ||
        Rule->YieldUnitsPerExcavation <= 0 || Rule->YieldUnitsPerExcavation > 1000000 ||
        (!Existing->bResourceDepositInitialized &&
         (Existing->MaterialId != Rule->SourceMaterialId ||
          DepthLayer < Rule->MinDepthLayer || DepthLayer > Rule->MaxDepthLayer)) ||
        (Existing->bResourceDepositInitialized &&
         (Existing->ResourceDefinitionId != Rule->ResourceDefinitionId ||
          Existing->RemainingResourceUnits < 0)))
        return false;

    const TMap<FIntVector, FWorldTerrainCellState> BeforeCells = Cells;
    const TMap<FGuid, FWorldTerrainOperationReceipt> BeforeReceipts = Receipts;
    FWorldTerrainCellState ResourceState = *Existing;
    if (!ResourceState.bResourceDepositInitialized)
    {
        ResourceState.bResourceDepositInitialized = true;
        ResourceState.ResourceDefinitionId = Rule->ResourceDefinitionId;
        ResourceState.RemainingResourceUnits = Rule->InitialDepositUnitsPerCell;
    }

    const int32 YieldUnits = FMath::Min(
        Rule->YieldUnitsPerExcavation, ResourceState.RemainingResourceUnits);
    ResourceState.RemainingResourceUnits -= YieldUnits;

    FWorldTerrainCellState EditedState;
    if (!TryApplyCellEditInternal(OperationId, Cell, DepthLayer, ETerrainCellEdit::Excavate,
        NAME_None, ExpectedRevision, true, EditedState))
        return false;

    FWorldTerrainCellState* CommittedCell = Cells.Find(Cell);
    FWorldTerrainOperationReceipt* CommittedReceipt = Receipts.Find(OperationId);
    if (!CommittedCell || !CommittedReceipt)
    {
        Cells = BeforeCells;
        Receipts = BeforeReceipts;
        return false;
    }
    ResourceState.bExcavated = true;
    ResourceState.Cell = Cell;
    ResourceState.DepthLayer = DepthLayer;
    ResourceState.Revision = EditedState.Revision;
    *CommittedCell = ResourceState;
    CommittedReceipt->ResultState = ResourceState;
    CommittedReceipt->YieldedResourceDefinitionId = Rule->ResourceDefinitionId;
    CommittedReceipt->YieldedResourceUnits = YieldUnits;

    if (YieldUnits > 0)
    {
        FWorldOwnedItemRecord Output;
        bool bUniqueId = false;
        for (int32 Attempt = 0; Attempt < 8 && !bUniqueId; ++Attempt)
        {
            const FGuid CandidateId = FGuid::NewGuid();
            FWorldOwnedItemRecord ExistingItem;
            if (CandidateId.IsValid() && !Ownership->TryGetItem(CandidateId, ExistingItem))
            {
                Output.ItemId = CandidateId;
                bUniqueId = true;
            }
        }
        if (!bUniqueId)
        {
            Cells = BeforeCells;
            Receipts = BeforeReceipts;
            return false;
        }
        Output.DefinitionId = Rule->ResourceDefinitionId;
        Output.BaseDefinitionId = Rule->ResourceDefinitionId;
        Output.Quantity = YieldUnits;
        Output.OwnerKind = EWorldItemOwnerKind::World;
        Output.WorldLocation = ResourceWorldLocation;
        Output.Revision = 1;
        if (!Ownership->RegisterItem(Output))
        {
            Cells = BeforeCells;
            Receipts = BeforeReceipts;
            return false;
        }
        CommittedReceipt->OutputWorldItemId = Output.ItemId;
        OutResourceItem = Output;
    }
    OutState = ResourceState;
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

void UWorldTerrainStateSubsystem::ExportSnapshot(
    TArray<FWorldTerrainCellState>& OutCells,
    TArray<FWorldTerrainOperationReceipt>& OutReceipts) const
{
    ExportSnapshot(OutCells);
    OutReceipts.Reset();
    Receipts.GenerateValueArray(OutReceipts);
    OutReceipts.Sort([](const FWorldTerrainOperationReceipt& A, const FWorldTerrainOperationReceipt& B)
    {
        return A.OperationId.ToString() < B.OperationId.ToString();
    });
}

bool UWorldTerrainStateSubsystem::RestoreSnapshot(
    const TArray<FWorldTerrainCellState>& Snapshot)
{
    return RestoreSnapshot(Snapshot, TArray<FWorldTerrainOperationReceipt>());
}

bool UWorldTerrainStateSubsystem::RestoreSnapshot(
    const TArray<FWorldTerrainCellState>& CellSnapshot,
    const TArray<FWorldTerrainOperationReceipt>& ReceiptSnapshot)
{
    if (CellSnapshot.Num() > MaxEditedCells || MaxEditedCells <= 0 ||
        MaxEditableDepthLayers <= 0 || MaxDepthPerCell <= 0 ||
        ReceiptSnapshot.Num() > MaxOperationReceipts || MaxOperationReceipts <= 0)
        return false;

    TMap<FIntVector, FWorldTerrainCellState> ProposedCells;
    for (const FWorldTerrainCellState& Cell : CellSnapshot)
    {
        if (Cell.DepthLayer < 0 || Cell.DepthLayer >= MaxEditableDepthLayers ||
            Cell.DepthLayer >= MaxDepthPerCell || Cell.Revision <= 0 ||
            (!Cell.bExcavated && Cell.MaterialId.IsNone()) ||
            (PermittedDepthLayers.Num() > 0 && !PermittedDepthLayers.Contains(Cell.DepthLayer)))
            return false;
        ProposedCells.Add(Cell.Cell, Cell);
    }

    TMap<FGuid, FWorldTerrainOperationReceipt> ProposedReceipts;
    for (const FWorldTerrainOperationReceipt& Receipt : ReceiptSnapshot)
    {
        const FWorldTerrainCellState* CurrentCell = ProposedCells.Find(Receipt.Cell);
        if (!Receipt.OperationId.IsValid() || ProposedReceipts.Contains(Receipt.OperationId) ||
            !CurrentCell || Receipt.ExpectedRevision < 0 || Receipt.ExpectedRevision >= MAX_int64 ||
            (Receipt.ExpectedRevision == 0 && Receipt.RequestedEdit != ETerrainCellEdit::Excavate) ||
            Receipt.ResultingRevision != Receipt.ExpectedRevision + 1 ||
            Receipt.ResultState.Cell != Receipt.Cell ||
            Receipt.ResultState.Revision != Receipt.ResultingRevision ||
            Receipt.RequestedDepthLayer != Receipt.ResultState.DepthLayer ||
            Receipt.RequestedDepthLayer < 0 ||
            Receipt.RequestedDepthLayer >= MaxEditableDepthLayers ||
            Receipt.RequestedDepthLayer >= MaxDepthPerCell ||
            static_cast<uint8>(Receipt.RequestedEdit) > static_cast<uint8>(ETerrainCellEdit::ReplaceMaterial) ||
            CurrentCell->Revision < Receipt.ResultingRevision ||
            (PermittedDepthLayers.Num() > 0 && !PermittedDepthLayers.Contains(Receipt.RequestedDepthLayer)) ||
            ((Receipt.RequestedEdit == ETerrainCellEdit::Fill ||
              Receipt.RequestedEdit == ETerrainCellEdit::ReplaceMaterial) &&
              Receipt.RequestedMaterialId.IsNone()) ||
            (Receipt.RequestedEdit == ETerrainCellEdit::Excavate && !Receipt.ResultState.bExcavated) ||
            (Receipt.RequestedEdit == ETerrainCellEdit::Fill &&
              (Receipt.ResultState.bExcavated ||
               Receipt.ResultState.MaterialId != Receipt.RequestedMaterialId)) ||
            (Receipt.RequestedEdit == ETerrainCellEdit::ReplaceMaterial &&
              Receipt.ResultState.MaterialId != Receipt.RequestedMaterialId))
            return false;

        ProposedReceipts.Add(Receipt.OperationId, Receipt);
    }

    Cells = MoveTemp(ProposedCells);
    Receipts = MoveTemp(ProposedReceipts);
    return true;
}
