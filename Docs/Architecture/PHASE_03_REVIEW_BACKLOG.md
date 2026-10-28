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

- [x] Audit all tracked `Source/MyProject3/*.{h,cpp}` files for processing call sites: the compatibility wrapper delegates to `TryApplyCutAction`, and the only tracked gameplay input caller (`ULogCarryComponent::ProcessAimedPiece`) consumes the result and displays its message.
- [ ] Audit local/untracked Blueprint assets in the actual project checkout/editor; they are not present in this repository tree.
- [x] Source-reviewed the action-to-result-code mapping for all seven actions: invalid state, unmet prerequisite, missing configuration, unsupported action, and duplicate source-mesh mapping conflict are distinguished; prerequisite messages are prioritized before configuration messages. Engine/runtime behavior remains unverified.
- [x] Document operation-ID semantics in `LogBuildPiece.h`: the ID is correlation/diagnostic metadata only, not an idempotency key. A repeated request is revalidated against the new piece state and may return a different result; callers must not assume deduplication.
- [ ] Review whether bottom-height preservation is valid for all configured output meshes and actor rotations; current logic preserves world AABB bottom, but actual pivot/orientation/collision fit needs real asset inspection.
- [x] Source guard: processing rejects any occupied snap anchor as well as attached children, so an occupied auto-anchor cannot be silently recreated as free during the mesh swap; custom anchors remain outside automatic regeneration. Runtime/editor confirmation remains open.

### P1 — Domain completeness (not silently included in this phase)

- [ ] Inventory/tool requirement and tool durability. The tracked C++ source tree currently contains no inventory/tool durability owner to integrate with; do not add a duplicate inventory authority inside `ALogBuildPiece`.
- [ ] Resource yield/consumption for each processing action. A shared unit/resource amount type exists, but no wood-processing yield/consumption owner is present in the tracked source tree.
- [ ] Physical cutting progress and intermediate geometry rather than a single mesh swap.
- [x] Add source-level exact-source-mesh output sets with duplicate/unmapped-source fail-closed behavior and legacy fallback only when no sets are configured.
- [ ] Populate the output sets for each imported source mesh and validate real mesh bounds/pivots/collision in Unreal. `AssetPackages/UnrealExport` has named families for 150/300/600 plus 75/short/long variants; filenames are not dimension evidence.
- [ ] Persistence of processed piece type and stable identity.
- [ ] Structural graph integration for pieces that become attached or support other pieces.

These are separate integration tasks; do not mark them implemented merely because the result-bearing API exists.

### P2 — Verification

- [ ] Run Unreal Engine 5.8 UHT/build.
- [ ] Exercise every processing action, each invalid prerequisite, missing output meshes, held/placed/supporting-piece rejection, and repeated operation IDs in Editor/PIE.
- [ ] Audit local/untracked Blueprint consumers in the actual project checkout.
- [ ] Inspect imported FBX dimensions, local axes, pivots, collision and per-action same-length variants inside Unreal Editor; repository filenames alone are not sufficient evidence.
- [ ] Attach reproducible evidence before marking any runtime gate passed.

## Phase boundary

Phase 2 voxel excavation, inventory-authoritative dig strength, terrain seam stitching, terrain save/load restoration, and terrain rollback gaps are deferred until after the current phase sequence, as requested. This does not imply that Phase 2 passed its exit gate.

No Unreal build, UHT, Editor, PIE, or gameplay test is claimed by this document.
