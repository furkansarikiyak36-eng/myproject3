# Phase 3 — Wood Processing Review Backlog

**Branch:** `phase-03-wood-processing`  
**Base:** `phase-02-core-contracts`  
**Engine target:** Unreal Engine 5.8  
**Status:** Initial source pass exists; Phase 3 is not validated or closed.

This review deliberately continues Phase 3 without pulling the deferred Phase 2 voxel-terrain gaps into its scope.

## Current source baseline

- `ALogBuildPiece` remains the authoritative owner of log processing state.
- `TryApplyCutAction` returns `FWorldOperationResult`; `ApplyCutAction` remains as a compatibility wrapper.
- Whole-log and split-half prerequisites and configured output-mesh checks are retained.
- Processing is rejected while a piece is held, placed, or supporting attached pieces.
- The output mesh is checked before state mutation; the replacement geometry is bottom-height aligned.
- `ObjectProcessed` is a best-effort notification after the state change, not a durable event or part of the transaction.
- Identity is optional; no existing Blueprint/actor is silently migrated.

## Phase 3 work remaining

### P0 — Source-level correctness

- [ ] Audit all tracked C++ call sites of `ApplyCutAction` and `TryApplyCutAction`; verify callers do not discard useful failure codes where user feedback is needed.
- [x] Source-reviewed the action-to-result-code mapping for all seven declared actions: invalid state, unmet prerequisite, missing configuration, and unsupported action are distinguished. Engine/runtime behavior remains unverified.
- [x] Document operation-ID semantics in `LogBuildPiece.h`: the ID is correlation/diagnostic metadata only, not an idempotency key. A repeated request is revalidated against the new piece state and may return a different result; callers must not assume deduplication.
- [ ] Review whether bottom-height preservation is valid for all configured output meshes and actor rotations; current logic uses world-axis bounds and should be checked with real assets.
- [ ] Confirm snap-anchor regeneration after processing does not invalidate occupied/custom anchors in any allowed processing state.

### P1 — Domain completeness (not silently included in this phase)

- [ ] Inventory/tool requirement and tool durability.
- [ ] Resource yield/consumption for each processing action.
- [ ] Physical cutting progress and intermediate geometry rather than a single mesh swap.
- [ ] Length/section-specific output contracts and validation of real mesh bounds/pivots/collision.
- [ ] Persistence of processed piece type and stable identity.
- [ ] Structural graph integration for pieces that become attached or support other pieces.

These are separate integration tasks; do not mark them implemented merely because the result-bearing API exists.

### P2 — Verification

- [ ] Run Unreal Engine 5.8 UHT/build.
- [ ] Exercise every processing action, each invalid prerequisite, missing output meshes, held/placed/supporting-piece rejection, and repeated operation IDs in Editor/PIE.
- [ ] Audit local/untracked Blueprint consumers in the actual project checkout.
- [ ] Attach reproducible evidence before marking any runtime gate passed.

## Phase boundary

Phase 2 voxel excavation, inventory-authoritative dig strength, terrain seam stitching, terrain save/load restoration, and terrain rollback gaps are deferred until after the current phase sequence, as requested. This does not imply that Phase 2 passed its exit gate.

No Unreal build, UHT, Editor, PIE, or gameplay test is claimed by this document.
