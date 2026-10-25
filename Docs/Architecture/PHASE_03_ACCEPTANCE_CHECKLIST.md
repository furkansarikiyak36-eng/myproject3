# Phase 3 Acceptance Checklist — Wood Processing

## API and compatibility
- [x] Add `TryApplyCutAction` returning `FWorldOperationResult`.
- [x] Keep the existing `ApplyCutAction(ELogCutAction) -> bool` signature.
- [x] Use shared operation result codes instead of UI text as the sole failure identifier.
- [x] Preserve `ALogBuildPiece` as the authoritative processing owner.

## Processing invariants
- [x] Split-half output requires a whole log and configured mesh.
- [x] Groove, trough, quartering and plank require a split-half log.
- [x] Processing remains blocked while held, placed, or supporting attached pieces.
- [x] Verify the selected output mesh before committing state.
- [x] Preserve the previous bottom world height when swapping output geometry.

## Event semantics
- [x] Publish `ObjectProcessed` after the processing state is changed.
- [x] Include operation ID and, when available, the identity component's object ID.
- [x] Do not claim event notifications are durable.
- [x] Do not treat notification failure as if the committed processing operation failed.

## Pending
- [ ] Inspect all tracked call sites and Blueprint compatibility in the local project.
- [ ] Compile/UHT with Unreal Engine 5.8.
- [ ] Verify processing, invalid prerequisites, missing meshes, and geometry placement in Editor/PIE.
- [ ] Integrated test evidence remains deferred to the final all-119 gate.

A checked source item means it was implemented in source, not that it has passed engine/runtime verification.
