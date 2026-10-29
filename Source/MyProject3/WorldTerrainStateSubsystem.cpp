#include "WorldTerrainStateSubsystem.h"

#include "VoxelTerrainChunkComponent.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

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

    if (MaxOperationReceipts <= 0 || Receipts.Num() >= MaxOperationReceipts)
    {
        // Never evict an old operation receipt: eviction would make a replay capable
        // of applying the same edit twice. Capacity exhaustion is fail-closed.
        return false;
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
    // Excavation may be repeated in an already-open logical cell: each new
    // operation still passes revision/receipt checks and advances the cell revision.
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


bool UWorldTerrainStateSubsystem::TryApplyVoxelExcavation(
    const FGuid& OperationId, UVoxelTerrainChunkComponent* VoxelChunk,
    FVector LocalCenter, float RadiusCm, float ToolStrength, int64 ExpectedDensityRevision,
    const FIntVector& LogicalCell, int32 DepthLayer, int64 ExpectedCellRevision,
    FName MaterialId, FWorldTerrainCellState& OutState)
{
    if (!IsValid(VoxelChunk) || !OperationId.IsValid())
        return false;

    // A retry after a successful two-domain commit must return the committed
    // result before comparing against the now-advanced physical revision.
    if (const FWorldTerrainOperationReceipt* ExistingReceipt = Receipts.Find(OperationId))
    {
        if (ExistingReceipt->Cell != LogicalCell ||
            ExistingReceipt->RequestedDepthLayer != DepthLayer ||
            ExistingReceipt->RequestedEdit != ETerrainCellEdit::Excavate ||
            ExistingReceipt->RequestedMaterialId != MaterialId ||
            ExistingReceipt->ExpectedRevision != ExpectedCellRevision)
            return false;

        FVoxelTerrainDigReceipt PhysicalReceipt;
        if (!VoxelChunk->TryGetDigReceipt(OperationId, PhysicalReceipt) ||
            !PhysicalReceipt.LocalCenter.Equals(LocalCenter, 0.01f) ||
            !FMath::IsNearlyEqual(PhysicalReceipt.RadiusCm, RadiusCm, 0.01f) ||
            !FMath::IsNearlyEqual(PhysicalReceipt.ToolStrength, ToolStrength, 0.01f) ||
            PhysicalReceipt.ExpectedRevision != ExpectedDensityRevision)
            return false;

        OutState = ExistingReceipt->ResultState;
        return true;
    }

    if (MaxOperationReceipts <= 0 || Receipts.Num() >= MaxOperationReceipts)
        return false;

    // Capture a rollback point before touching either authority. The voxel
    // component validates its own revision, bounds, material and structure guard.
    TArray<float> BeforeDensity;
    TArray<FVoxelTerrainDigReceipt> BeforeReceipts;
    int64 BeforeDensityRevision = 0;
    if (!VoxelChunk->ExportDensitySnapshot(BeforeDensity, BeforeDensityRevision, BeforeReceipts) ||
        BeforeDensityRevision != ExpectedDensityRevision)
        return false;

    const int32 CellsPerAxis = VoxelChunk->CellsPerAxis;
    const float VoxelSizeCm = VoxelChunk->VoxelSizeCm;
    if (CellsPerAxis < 4 || !FMath::IsFinite(VoxelSizeCm) || VoxelSizeCm < 5.0f)
        return false;

    float RemovedVolume = 0.0f;
    int64 ResultDensityRevision = 0;
    if (!VoxelChunk->TryApplyBoundedDig(OperationId, LocalCenter, RadiusCm, ToolStrength,
        ExpectedDensityRevision, RemovedVolume, ResultDensityRevision))
        return false;

    FWorldTerrainCellState UpdatedCell;
    if (TryApplyCellEdit(OperationId, LogicalCell, DepthLayer, ETerrainCellEdit::Excavate,
        MaterialId, ExpectedCellRevision, UpdatedCell))
    {
        OutState = UpdatedCell;
        return true;
    }

    // Logical commit rejected (e.g. stale cell revision, duplicate-cell
    // conflict, depth gate). Restore physical density and receipt state so the
    // operation is all-or-nothing from the caller's perspective.
    if (!VoxelChunk->RestoreDensitySnapshot(CellsPerAxis, VoxelSizeCm,
        BeforeDensityRevision, BeforeDensity, BeforeReceipts))
    {
        UE_LOG(LogTemp, Error, TEXT("Terrain transaction rollback failed for operation %s"),
            *OperationId.ToString());
    }
    return false;
}

bool UWorldTerrainStateSubsystem::TryExcavateForResource(
    const FGuid& OperationId, UVoxelTerrainChunkComponent* VoxelChunk,
    FVector LocalCenter, float RadiusCm, float ToolStrength, int64 ExpectedDensityRevision,
    const FIntVector& LogicalCell, int32 DepthLayer, int64 ExpectedCellRevision,
    FName SourceMaterialId, FWorldTerrainCellState& OutState, TArray<FGuid>& OutResourceItemIds)
{
    OutResourceItemIds.Reset();
    if (!OperationId.IsValid() || !IsValid(VoxelChunk) || SourceMaterialId.IsNone() ||
        !FMath::IsFinite(LocalCenter.X) || !FMath::IsFinite(LocalCenter.Y) ||
        !FMath::IsFinite(LocalCenter.Z) || !FMath::IsFinite(RadiusCm) || RadiusCm <= 0.0f ||
        !FMath::IsFinite(ToolStrength) || ToolStrength <= 0.0f ||
        ExpectedDensityRevision <= 0 || ExpectedCellRevision < 0 ||
        DepthLayer < 0 || DepthLayer >= MaxEditableDepthLayers ||
        MaxResourceYieldRules <= 0 || MaxResourceYieldRules > 256 ||
        ResourceYieldRules.Num() > MaxResourceYieldRules)
        return false;

    const FWorldTerrainResourceYieldRule* MatchedRule = nullptr;
    for (const FWorldTerrainResourceYieldRule& Candidate : ResourceYieldRules)
    {
        if (Candidate.SourceMaterialId != SourceMaterialId ||
            DepthLayer < Candidate.MinimumDepthLayer || DepthLayer > Candidate.MaximumDepthLayer)
            continue;
        if (MatchedRule)
            return false; // Ambiguous yield policy must not multiply resources.
        if (Candidate.SourceMaterialId.IsNone() || Candidate.OutputDefinitionId.IsNone() ||
            Candidate.MinimumDepthLayer < 0 || Candidate.MaximumDepthLayer < Candidate.MinimumDepthLayer ||
            Candidate.MaximumDepthLayer >= MaxDepthPerCell ||
            !FMath::IsFinite(Candidate.UnitsPerCubicMetre) ||
            Candidate.UnitsPerCubicMetre <= 0.0f || Candidate.UnitsPerCubicMetre > 100000.0f ||
            Candidate.MaxUnitsPerOperation <= 0 || Candidate.MaxUnitsPerOperation > 64)
            return false;
        MatchedRule = &Candidate;
    }
    if (!MatchedRule)
        return false;

    AActor* ChunkActor = VoxelChunk->GetOwner();
    UWorldObjectIdentityComponent* ChunkIdentity =
        IsValid(ChunkActor) ? ChunkActor->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
    if (!IsValid(ChunkIdentity) || !ChunkIdentity->HasValidIdentity())
        return false;
    const FGuid ChunkActorIdentityId = ChunkIdentity->GetInstanceId();
    if (!ChunkActorIdentityId.IsValid())
        return false;

    // Resource retries use the terrain receipt as the authoritative output list.
    // Validate the full request before returning the recorded output identities.
    if (const FWorldTerrainOperationReceipt* Existing = Receipts.Find(OperationId))
    {
        if (Existing->Cell != LogicalCell ||
            Existing->RequestedDepthLayer != DepthLayer ||
            Existing->RequestedEdit != ETerrainCellEdit::Excavate ||
            Existing->RequestedMaterialId != SourceMaterialId ||
            Existing->ExpectedRevision != ExpectedCellRevision ||
            Existing->VoxelChunkActorIdentityId != ChunkActorIdentityId ||
            Existing->ExpectedDensityRevision != ExpectedDensityRevision ||
            Existing->YieldSourceMaterialId != MatchedRule->SourceMaterialId ||
            Existing->YieldOutputDefinitionId != MatchedRule->OutputDefinitionId ||
            !FMath::IsNearlyEqual(Existing->YieldUnitsPerCubicMetre,
                MatchedRule->UnitsPerCubicMetre, 0.0001f))
            return false;

        FWorldTerrainCellState RetryState;
        if (!TryApplyVoxelExcavation(OperationId, VoxelChunk, LocalCenter, RadiusCm,
            ToolStrength, ExpectedDensityRevision, LogicalCell, DepthLayer,
            ExpectedCellRevision, SourceMaterialId, RetryState))
            return false;

        UWorldItemOwnershipSubsystem* Ownership =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
        if (!Ownership || Existing->YieldOutputUnits < 0 ||
            Existing->YieldOutputUnits > MatchedRule->MaxUnitsPerOperation ||
            Existing->YieldOutputItemIds.Num() != Existing->YieldOutputUnits)
            return false;
        for (const FGuid& ItemId : Existing->YieldOutputItemIds)
        {
            FWorldOwnedItemRecord Item;
            if (!Ownership->TryGetItem(ItemId, Item))
                return false;
        }
        OutState = RetryState;
        OutResourceItemIds = Existing->YieldOutputItemIds;
        return true;
    }

    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!World || !Ownership || MaxOperationReceipts <= 0 ||
        Receipts.Num() >= MaxOperationReceipts ||
        Ownership->MaxTrackedItems <= 0 ||
        Ownership->GetTrackedItemCount() >= Ownership->MaxTrackedItems)
        return false;

    TArray<FWorldTerrainCellState> CellsBefore;
    TArray<FWorldTerrainOperationReceipt> TerrainReceiptsBefore;
    ExportSnapshot(CellsBefore, TerrainReceiptsBefore);

    TArray<float> DensityBefore;
    TArray<FVoxelTerrainDigReceipt> DigReceiptsBefore;
    int64 DensityRevisionBefore = 0;
    if (!VoxelChunk->ExportDensitySnapshot(DensityBefore, DensityRevisionBefore, DigReceiptsBefore) ||
        DensityRevisionBefore != ExpectedDensityRevision)
        return false;

    TArray<FWorldOwnedItemRecord> ItemsBefore;
    TArray<FWorldItemTransferReceipt> TransferReceiptsBefore;
    TArray<FWorldItemSplitReceipt> SplitReceiptsBefore;
    Ownership->ExportSnapshot(ItemsBefore, TransferReceiptsBefore, SplitReceiptsBefore);

    const int32 CellsPerAxisBefore = VoxelChunk->CellsPerAxis;
    const float VoxelSizeBefore = VoxelChunk->VoxelSizeCm;
    auto RollbackAll = [&]() -> bool
    {
        const bool bTerrainRestored = RestoreSnapshot(CellsBefore, TerrainReceiptsBefore);
        const bool bDensityRestored = VoxelChunk->RestoreDensitySnapshot(
            CellsPerAxisBefore, VoxelSizeBefore, DensityRevisionBefore,
            DensityBefore, DigReceiptsBefore);
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            ItemsBefore, TransferReceiptsBefore, SplitReceiptsBefore);
        if (!bTerrainRestored || !bDensityRestored || !bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Resource excavation rollback failed for operation %s."),
                *OperationId.ToString());
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Resource excavation rollback failed; stop simulation and recover/reset the world."));
            }
            return false;
        }
        return true;
    };

    FWorldTerrainCellState State;
    if (!TryApplyVoxelExcavation(OperationId, VoxelChunk, LocalCenter, RadiusCm,
        ToolStrength, ExpectedDensityRevision, LogicalCell, DepthLayer,
        ExpectedCellRevision, SourceMaterialId, State))
        return false;

    FVoxelTerrainDigReceipt DigReceipt;
    if (!VoxelChunk->TryGetDigReceipt(OperationId, DigReceipt) ||
        DigReceipt.ResultingRevision <= DigReceipt.ExpectedRevision ||
        !FMath::IsFinite(DigReceipt.RemovedVolumeCubicMetres) ||
        DigReceipt.RemovedVolumeCubicMetres <= 0.0f)
    {
        RollbackAll();
        return false;
    }

    const double RawUnits = FMath::FloorToDouble(
        static_cast<double>(DigReceipt.RemovedVolumeCubicMetres) *
        static_cast<double>(MatchedRule->UnitsPerCubicMetre) + 1.e-6);
    if (!FMath::IsFinite(RawUnits) || RawUnits < 0.0 ||
        RawUnits > static_cast<double>(MatchedRule->MaxUnitsPerOperation))
    {
        RollbackAll();
        return false;
    }
    const int32 OutputUnits = static_cast<int32>(RawUnits);

    TArray<FGuid> CreatedItemIds;
    CreatedItemIds.Reserve(OutputUnits);
    const FTransform ChunkTransform = VoxelChunk->GetComponentTransform();
    const double CenteringOffset = (static_cast<double>(OutputUnits) - 1.0) * 0.5;
    for (int32 Index = 0; Index < OutputUnits; ++Index)
    {
        FWorldOwnedItemRecord Output;
        Output.ItemId = FGuid::NewGuid();
        Output.DefinitionId = MatchedRule->OutputDefinitionId;
        Output.BaseDefinitionId = MatchedRule->OutputDefinitionId;
        Output.Quantity = 1;
        Output.OwnerKind = EWorldItemOwnerKind::World;
        const FVector LocalOutputLocation = LocalCenter +
            FVector((static_cast<double>(Index) - CenteringOffset) * 15.0, 0.0, RadiusCm * 0.5);
        Output.WorldLocation = ChunkTransform.TransformPosition(LocalOutputLocation);
        if (!Output.ItemId.IsValid() || Output.WorldLocation.ContainsNaN() ||
            !Ownership->RegisterItem(Output))
        {
            RollbackAll();
            OutState = FWorldTerrainCellState();
            OutResourceItemIds.Reset();
            return false;
        }
        CreatedItemIds.Add(Output.ItemId);
    }

    FWorldTerrainOperationReceipt* CommittedReceipt = Receipts.Find(OperationId);
    if (!CommittedReceipt)
    {
        RollbackAll();
        OutState = FWorldTerrainCellState();
        OutResourceItemIds.Reset();
        return false;
    }
    CommittedReceipt->VoxelChunkActorIdentityId = ChunkActorIdentityId;
    CommittedReceipt->ExpectedDensityRevision = DigReceipt.ExpectedRevision;
    CommittedReceipt->ResultingDensityRevision = DigReceipt.ResultingRevision;
    CommittedReceipt->RemovedVolumeCubicMetres = DigReceipt.RemovedVolumeCubicMetres;
    CommittedReceipt->YieldSourceMaterialId = MatchedRule->SourceMaterialId;
    CommittedReceipt->YieldOutputDefinitionId = MatchedRule->OutputDefinitionId;
    CommittedReceipt->YieldUnitsPerCubicMetre = MatchedRule->UnitsPerCubicMetre;
    CommittedReceipt->YieldOutputUnits = OutputUnits;
    CommittedReceipt->YieldOutputItemIds = CreatedItemIds;
    OutState = State;
    OutResourceItemIds = MoveTemp(CreatedItemIds);
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

void UWorldTerrainStateSubsystem::ExportSnapshot(
    TArray<FWorldTerrainCellState>& OutCells) const
{
    TArray<FWorldTerrainOperationReceipt> IgnoredReceipts;
    ExportSnapshot(OutCells, IgnoredReceipts);
}

void UWorldTerrainStateSubsystem::ExportSnapshot(
    TArray<FWorldTerrainCellState>& OutCells,
    TArray<FWorldTerrainOperationReceipt>& OutReceipts) const
{
    OutCells.Reset();
    Cells.GenerateValueArray(OutCells);
    OutCells.Sort([](const FWorldTerrainCellState& A, const FWorldTerrainCellState& B)
    {
        if (A.Cell.X != B.Cell.X) return A.Cell.X < B.Cell.X;
        if (A.Cell.Y != B.Cell.Y) return A.Cell.Y < B.Cell.Y;
        return A.Cell.Z < B.Cell.Z;
    });

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
    const TArray<FWorldTerrainCellState>& Snapshot,
    const TArray<FWorldTerrainOperationReceipt>& ReceiptSnapshot)
{
    if (Snapshot.Num() > MaxEditedCells || MaxEditedCells <= 0 ||
        ReceiptSnapshot.Num() > MaxOperationReceipts || MaxOperationReceipts <= 0)
        return false;

    TMap<FIntVector, FWorldTerrainCellState> ProposedCells;
    for (const FWorldTerrainCellState& Cell : Snapshot)
    {
        if (Cell.DepthLayer < 0 || Cell.DepthLayer >= MaxEditableDepthLayers ||
            Cell.DepthLayer >= MaxDepthPerCell || Cell.Revision <= 0 ||
            ProposedCells.Contains(Cell.Cell) ||
            (PermittedDepthLayers.Num() > 0 && !PermittedDepthLayers.Contains(Cell.DepthLayer)))
            return false;
        ProposedCells.Add(Cell.Cell, Cell);
    }

    TMap<FGuid, FWorldTerrainOperationReceipt> ProposedReceipts;
    for (const FWorldTerrainOperationReceipt& Receipt : ReceiptSnapshot)
    {
        const uint8 EditValue = static_cast<uint8>(Receipt.RequestedEdit);
        const FWorldTerrainCellState* CurrentCell = ProposedCells.Find(Receipt.Cell);
        if (!Receipt.OperationId.IsValid() || ProposedReceipts.Contains(Receipt.OperationId) ||
            !CurrentCell || Receipt.ExpectedRevision < 0 || Receipt.ExpectedRevision >= MAX_int64 ||
            Receipt.ResultingRevision != Receipt.ExpectedRevision + 1 ||
            Receipt.ResultState.Cell != Receipt.Cell ||
            Receipt.ResultState.Revision != Receipt.ResultingRevision ||
            Receipt.ResultState.DepthLayer != Receipt.RequestedDepthLayer ||
            Receipt.RequestedDepthLayer < 0 ||
            Receipt.RequestedDepthLayer >= MaxEditableDepthLayers ||
            Receipt.RequestedDepthLayer >= MaxDepthPerCell ||
            EditValue > static_cast<uint8>(ETerrainCellEdit::ReplaceMaterial) ||
            CurrentCell->Revision < Receipt.ResultingRevision ||
            ((Receipt.RequestedEdit == ETerrainCellEdit::Fill ||
              Receipt.RequestedEdit == ETerrainCellEdit::ReplaceMaterial) &&
             Receipt.RequestedMaterialId.IsNone()))
        {
            return false;
        }

        if (Receipt.RequestedEdit == ETerrainCellEdit::Excavate && !Receipt.ResultState.bExcavated)
            return false;
        if (Receipt.RequestedEdit == ETerrainCellEdit::Fill &&
            (Receipt.ResultState.bExcavated ||
             Receipt.ResultState.MaterialId != Receipt.RequestedMaterialId))
            return false;
        if (Receipt.RequestedEdit == ETerrainCellEdit::ReplaceMaterial &&
            Receipt.ResultState.MaterialId != Receipt.RequestedMaterialId)
            return false;

        ProposedReceipts.Add(Receipt.OperationId, Receipt);
    }

    // Validate everything before swapping either map; a malformed save cannot
    // partially replace live terrain state or silently discard retry receipts.
    Cells = MoveTemp(ProposedCells);
    Receipts = MoveTemp(ProposedReceipts);
    return true;
}
