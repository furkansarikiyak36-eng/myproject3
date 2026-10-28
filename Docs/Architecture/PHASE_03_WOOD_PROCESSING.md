# Phase 3 — Wood Processing Domain Integration

**Status:** Source implementation drafted; static review and Unreal validation remain open.  
**Branch:** `phase-03-wood-processing`  
**Base:** `phase-02-core-contracts`  
**Target:** Unreal Engine 5.8

## Goal

Connect the existing log-processing owner to the Phase 2 operation-result and committed-event contracts without replacing the log prototype or changing the existing Blueprint-facing boolean API.

## Implemented in source

- `ALogBuildPiece::TryApplyCutAction` is the result-bearing entry point. It returns a stable `FWorldOperationResult` code and diagnostic message.
- Existing `ApplyCutAction(ELogCutAction)` remains available and delegates to the new entry point, returning the result's success state.
- Existing processing prerequisites remain in `CanApplyCutAction`: groove, trough, quartering and plank output require a split-half log; each operation requires its configured output mesh; held/placed/supporting pieces are rejected.
- The output mesh is checked before mutation. The existing geometry swap, piece-type update and bottom-height preservation remain in the domain owner.
- A successful processing commit attempts to publish `ObjectProcessed` through `UWorldCommittedEventSubsystem` after mesh, type and transform changes.
- If an identity component is present, its stable instance ID is included in the result/event. The component remains opt-in; existing actors are not silently modified.

## Ownership and compatibility

- `ALogBuildPiece` remains authoritative for log processing state and geometry.
- `ULogCarryComponent` remains the input/preview owner.
- The event subsystem is notification-only. A notification failure does not roll back an already committed processing operation.
- No inventory consumption, tool durability, physical cutting simulation, new meshes, or persistence implementation is claimed by this phase.
- Existing boolean callers remain source-compatible; the new result-bearing function is an additional API.

## Acceptance checklist

- [x] Add result-bearing processing API while retaining the legacy boolean function.
- [x] Return stable failure codes and an explanation for rejected processing.
- [x] Preserve split-before-groove/trough/plank/quarter prerequisites.
- [x] Validate the output mesh before mutating piece type.
- [x] Publish the committed processing event only after state mutation.
- [x] Keep event notification failure separate from processing success.
- [x] Keep identity integration optional.
- [ ] Review all callers and local/untracked Blueprint consumers.
- [ ] Run Unreal Engine 5.8 UHT/build and Editor/PIE checks at the agreed final gate.
- [ ] Add integrated all-119 mechanic evidence and final demo at the end of the phase sequence.

## Follow-on review

Track remaining source-level correctness questions, explicitly deferred domain work, and engine verification in [`PHASE_03_REVIEW_BACKLOG.md`](PHASE_03_REVIEW_BACKLOG.md). Phase 3 remains a source draft until those review items and the final Unreal validation gate are resolved.

## Validation boundary

This is a source-level implementation pass only. No Unreal Engine build, UHT, editor, PIE, or gameplay test was run. Registry verification remains `unreviewed` / `not_run` until implementation and evidence are reviewed.
