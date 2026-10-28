# Phase 3 Acceptance Checklist — Wood Processing

## API and compatibility
- [x] Add `TryApplyCutAction` returning `FWorldOperationResult`.
- [x] Keep the existing `ApplyCutAction(ELogCutAction) -> bool` signature.
- [x] Use shared operation result codes instead of UI text as the sole failure identifier.
- [x] Distinguish duplicate source-mesh mapping conflicts from missing mappings and unmet processing prerequisites.
- [x] Preserve `ALogBuildPiece` as the authoritative processing owner.

## Processing invariants
- [x] Split-half output requires a whole log and configured mesh.
- [x] Groove, trough, quartering and plank require a split-half log.
- [x] Processing remains blocked while held, placed, supporting attached pieces, or carrying any occupied snap anchor.
- [x] Verify the selected output mesh before committing state.
- [x] Preserve the previous bottom world height when swapping output geometry.

## Event semantics
- [x] Publish `ObjectProcessed` after the processing state is changed.
- [x] Include operation ID and, when available, the identity component's object ID.
- [x] Do not claim event notifications are durable.
- [x] Do not treat notification failure as if the committed processing operation failed.

## Pending
- [x] Scan all tracked C++ header/source files for processing call sites; the tracked input path is result-aware and the legacy boolean wrapper remains available.
- [ ] Audit local/untracked Blueprint consumers in the actual project checkout.
- [ ] Compile/UHT with Unreal Engine 5.8.
- [ ] Verify processing, invalid prerequisites, missing meshes, and geometry placement in Editor/PIE.
- [x] Add source-level exact-source-mesh output mapping with fail-closed duplicate/unmapped handling.
- [ ] Populate each actor/Blueprint's mapping sets and inspect FBX axes, pivots, bounds, collision and same-length output pairing in Unreal Editor.
- [ ] Integrate tool/inventory requirements and resource yields only after the canonical inventory/tool owner is identified.
- [ ] Integrated test evidence remains deferred to the final all-119 gate.

A checked source item means it was implemented in source, not that it has passed engine/runtime verification.
