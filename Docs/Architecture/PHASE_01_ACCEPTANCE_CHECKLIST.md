# P01 — Shared Data Contract Acceptance Checklist

**Status:** Draft checklist; no engine/build/runtime boxes are marked complete by this planning task.

## A. Scope and identity
- [x] Canonical registry files for M001–M119 are present in this planning branch.
- [x] Mechanic IDs are treated as separate from item/material/recipe/asset IDs.
- [x] Instance ID, definition ID, operation ID and anchor feature ID namespaces are documented.
- [x] Existing candidate C++ contracts and their branches are identified.
- [ ] Every M001–M119 row is checked against exact owning functions and caller chains; domain-level paths are not sufficient for closeout.
- [ ] Replace generic acceptance criteria with mechanic-specific preconditions, state changes, rejection cases, retry rules, persistence expectations and evidence steps.

## B. Data definition schemas
- [x] Item definition schema drafted.
- [x] Material definition schema drafted.
- [x] Recipe definition schema drafted.
- [x] Physical process-transition schema drafted.
- [x] Build-part definition schema drafted and placement-profile ID convention reconciled.
- [x] Region definition schema drafted.
- [x] Feedback event definition schema drafted.
- [ ] Validate all schemas with a JSON Schema Draft 2020-12 validator.
- [ ] Add representative sample entries and validate them against schemas.
- [ ] Check uniqueness of IDs and resolution of every cross-reference.
- [ ] Reconcile every field with the current candidate C++ types and config IDs.

## C. Operation and ownership invariants
- [x] No new identity registry or inventory ownership subsystem is defined by this branch.
- [x] Generic lifecycle is distinguished from item ownership and domain-specific processing state.
- [x] Operation ledger is not incorrectly described as a complete transaction coordinator.
- [x] Committed events are defined as post-commit notifications, not durable persistence.
- [ ] Test idempotent retries for the same operation ID in each domain.
- [ ] Test expected-revision conflicts and verify failure has no partial mutation.
- [ ] Test physical item transitions World ↔ Held ↔ Placed ↔ Inventory/Storage without duplication.
- [ ] Test wood split outputs, inventory ownership receipt and structural graph changes as one valid cross-system outcome.

## D. Placement and physical geometry
- [x] Existing shared placement kernel/profile architecture is recorded as the authority.
- [x] Build-part schema uses cm for dimensions, explicit local axis/pivot/collision and semantic anchor IDs.
- [ ] Validate every active profile against the compiled built-in catalogue and project JSON.
- [ ] Resolve mirrored diagonal intersections, missing mitre end cuts, authored joinery sockets and deck path acceptance.
- [ ] Confirm preview/commit parity using the same placement solution.

## E. Terrain, save/load and runtime
- [x] Logical terrain state and mesh prototype are described as separate components.
- [x] Current known save/load gaps are recorded.
- [ ] Validate terrain schema to chunk renderer/collision adapter mapping.
- [ ] Verify actor transforms, attachments, item ownership, structural graph and terrain edits in one save/load round trip.
- [ ] Compile/UHT/UBT against the selected source integration branch using Unreal Engine 5.8.
- [ ] Run Editor/PIE acceptance scenarios and store logs/captures/reproduction steps.
- [ ] Record performance for the bounded voxel region and snap solve before widening scope.

## Closeout rule

P01 can close as a **design/data contract phase** when schema validation, sample fixtures, unique ID/reference checks, and code-contract reconciliation are recorded. The larger source integration and player-visible functionality remain separate until the actual target engine build/runtime gate passes.
