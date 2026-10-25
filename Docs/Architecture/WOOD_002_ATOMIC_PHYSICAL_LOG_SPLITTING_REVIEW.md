# WOOD-002 — Atomic Physical Log Splitting

**Branch:** `fix/atomic-log-piece-splitting`  
**Base:** `fix/log-quarter-cross-section-semantics`  
**Pull request:** [#23 — Atomically split logs into uniquely owned physical pieces](https://github.com/furkansarikiyak36-eng/myproject3/pull/23)  
**Scope:** Create two physical outputs for the cross-section split/quarter actions while preserving source identity, creating a unique second identity, and keeping ownership receipts consistent through rollback/save snapshots.  
**Evidence level:** Static source review and structural checks only. UE 5.8 UHT/UBT, Editor, PIE, failure injection, asset-pivot validation, and save/load runtime tests were not run.

## Source finding

Before this change, `TryApplyCutAction` changed a single log actor's mesh/type and one ownership definition. The `SplitHalf` and `Quarter` actions did not create a second uniquely identified physical resource. A naive extra `SpawnActor` after changing the source record could duplicate or lose resources if spawn/registration failed or the same operation was retried.

## Ownership ledger changes

- Added `TrySplitItem`, which validates the full request before mutation.
- The first output retains the source item GUID and increments its revision; each additional output gets a new GUID and its own world-owned record.
- A split receipt records the operation ID, source ID, expected revision/owner, requested output definitions/locations and immutable result records.
- Repeating the same operation ID with the identical request returns the original output records. Reusing it with a different request is rejected.
- Split receipt IDs cannot collide with the existing transfer/definition-change receipt map.
- Snapshot export/restore now includes split receipts; validation occurs before the live item/receipt maps are swapped.
- Fixed-step rollback includes split receipts.
- SaveGame schema is now v7 with a v6→v7 migration that initializes the new split-receipt array as empty. Existing v4→v5→v6 migrations are retained.

## Actor integration

- `SplitHalf` and `Quarter` now route into a physical multi-output operation instead of the one-actor definition-change path.
- A second actor is created with deferred spawning so the unique ownership ID and definition are assigned before BeginPlay.
- The source actor is moved to the first output location and its mesh/type are updated; the second actor is finished at the second output location.
- Success requires the second item to exist as world-owned and its structural graph node to point to the child actor.
- If child registration fails, the implementation destroys the child, restores the ownership snapshot and attempts to restore the source actor's transform/mesh/type/collision state.

## Authored geometry requirement

The split offset is **not guessed**. Two editable local-space center-to-center vectors are provided:
- `SplitHalfPieceCenterOffsetLocal`
- `QuarterPieceCenterOffsetLocal`

A zero, non-finite, or longitudinal offset is rejected. Configure the vectors per mesh/Blueprint so the output meshes meet correctly. Because these properties default to zero, splitting remains intentionally blocked until the required geometry is authored. Visual overlap, pivot alignment and collision still require Unreal validation.

## Static acceptance

- [x] Ownership split API, split receipt and IDempotent retry lookup exist.
- [x] Transfer receipts and split receipts are distinct and mutually exclusive for the same operation ID.
- [x] Snapshot/restore handles items, transfer receipts and split receipts together.
- [x] Coordinator rollback snapshots/restores split receipts.
- [x] SaveGame v7 contains split receipts and a v4→v7 migration chain.
- [x] Actor creates a deferred second output, assigns its unique ID before BeginPlay and verifies graph/ownership registration.
- [x] Failure path attempts to restore source actor and ownership snapshot.
- [x] Targeted structural checks found matching API calls and balanced braces.
- [ ] UE 5.8 UHT/UBT compile.
- [ ] Blueprint mesh asset and center-offset configuration.
- [ ] Runtime test: each cut yields two visible, separately owned actors.
- [ ] Runtime test: same operation ID does not create a third actor and returns both original output IDs.
- [ ] Failure-injection test: child/graph registration failure restores world ownership/actor state.
- [ ] Save/load test: both physical actors/transforms/graph nodes and split receipt survive round trip.

## Remaining limits

- General world persistence still does not fully respawn/save all actor transforms and structural graph relationships; persisting split receipts does not solve that broader gap alone.
- Other log actions (hewn, gutter, trough, squared beam, plank) keep their existing single-actor transformation behavior for now.
- Exact mesh asset/pivot/offset and collision configuration is a content-authoring/UE validation step, not proven by source presence.

## Next item

WOOD-003 — define the canonical output geometry table for every log action, separate length fraction from cross-section profile, document output counts/byproducts, and map each Blueprint asset to the table before adding additional cut modes.
