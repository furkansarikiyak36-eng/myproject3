#include "TerrainWorldStateSubsystem.h"

FWorldOperationResult UTerrainWorldStateSubsystem::Failure(EWorldOperationResultCode Code,
    const FGuid& OperationId, const TCHAR* Message) const
{
    return FWorldOperationResult::Failure(Code, OperationId, FText::FromString(Message));
}

FString UTerrainWorldStateSubsystem::MakeCellKey(const FTerrainChunkKey& Chunk,
    const FTerrainChunkKey& Cell) const
{
    return FString::Printf(TEXT("%d:%d:%d/%d:%d:%d"), Chunk.X, Chunk.Y, Chunk.Z, Cell.X, Cell.Y, Cell.Z);
}

FWorldOperationResult UTerrainWorldStateSubsystem::ConfigurePolicy(
    const FTerrainEditPolicy& NewPolicy, FGuid OperationId)
{
    if (!FMath::IsFinite(NewPolicy.MaxDepthCm) || NewPolicy.MaxDepthCm <= 0.0f ||
        !FMath::IsFinite(NewPolicy.MaxExcavationVolumeM3) || NewPolicy.MaxExcavationVolumeM3 <= 0.0f ||
        !FMath::IsFinite(NewPolicy.CellSizeCm) || NewPolicy.CellSizeCm <= 0.0f ||
        NewPolicy.DiggableLayers.Num() == 0)
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId,
            TEXT("Digging policy needs positive finite limits and at least one diggable layer."));
    if (NewPolicy.DiggableLayers.Contains(ETerrainEditLayer::Protected))
        return Failure(EWorldOperationResultCode::NotPermitted, OperationId,
            TEXT("Protected terrain cannot be configured as diggable."));
    if (ExcavatedVolumeM3 > NewPolicy.MaxExcavationVolumeM3)
        return Failure(EWorldOperationResultCode::CapacityExceeded, OperationId,
            TEXT("New excavation limit is below the volume already excavated."));
    Policy = NewPolicy;
    bPolicyConfigured = true;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Terrain edit policy configured.")), {});
}

FWorldOperationResult UTerrainWorldStateSubsystem::RegisterCell(const FTerrainChunkKey& Chunk,
    const FTerrainChunkKey& Cell, ETerrainEditLayer Layer, FGuid OperationId)
{
    if (!bPolicyConfigured)
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId, TEXT("Terrain edit policy is not configured."));
    const FString Key = MakeCellKey(Chunk, Cell);
    if (Cells.Contains(Key))
        return Failure(EWorldOperationResultCode::Conflict, OperationId, TEXT("Terrain cell is already registered."));
    FTerrainCellState State;
    State.Layer = Layer;
    Cells.Add(Key, State);
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Terrain cell registered.")), {});
}

FWorldOperationResult UTerrainWorldStateSubsystem::TryExcavateCell(const FTerrainChunkKey& Chunk,
    const FTerrainChunkKey& Cell, float DepthCm, float CellVolumeM3, FGuid OperationId)
{
    if (!bPolicyConfigured)
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId, TEXT("Terrain edit policy is not configured."));
    if (!FMath::IsFinite(DepthCm) || !FMath::IsFinite(CellVolumeM3) || DepthCm <= 0.0f || CellVolumeM3 <= 0.0f)
        return Failure(EWorldOperationResultCode::InvalidState, OperationId, TEXT("Depth and cell volume must be finite and positive."));
    const FString Key = MakeCellKey(Chunk, Cell);
    FTerrainCellState* State = Cells.Find(Key);
    if (!State)
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Terrain cell is not registered."));
    if (State->Layer == ETerrainEditLayer::Protected || !Policy.DiggableLayers.Contains(State->Layer))
        return Failure(EWorldOperationResultCode::NotPermitted, OperationId, TEXT("Terrain layer is protected or not configured for excavation."));
    if (DepthCm > Policy.MaxDepthCm)
        return Failure(EWorldOperationResultCode::OutOfRange, OperationId, TEXT("Requested excavation exceeds maximum depth."));
    if (State->bExcavated)
        return Failure(EWorldOperationResultCode::Conflict, OperationId, TEXT("Terrain cell has already been excavated."));
    if (ExcavatedVolumeM3 + CellVolumeM3 > Policy.MaxExcavationVolumeM3)
        return Failure(EWorldOperationResultCode::CapacityExceeded, OperationId, TEXT("Excavation would exceed the configured volume limit."));

    State->bExcavated = true;
    ++State->Revision;
    ExcavatedVolumeM3 += CellVolumeM3;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Terrain state marked excavated.")), {});
}

bool UTerrainWorldStateSubsystem::GetCellState(const FTerrainChunkKey& Chunk,
    const FTerrainChunkKey& Cell, FTerrainCellState& OutState) const
{
    const FTerrainCellState* State = Cells.Find(MakeCellKey(Chunk, Cell));
    if (!State)
        return false;
    OutState = *State;
    return true;
}
