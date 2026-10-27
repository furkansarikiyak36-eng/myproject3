# P01 — Shared Data Contract Acceptance Checklist

**Status:** Design-time catalogue validation passed on GitHub Actions; engine/build/runtime gates remain open.

## A. Scope and identity
- [x] Canonical registry files for M001–M119 are present in this planning branch.
- [x] Mechanic IDs are treated as separate from item/material/recipe/asset IDs.
- [x] Instance ID, definition ID, operation ID and anchor feature ID namespaces are documented.
- [x] Existing candidate C++ contracts and their branches are identified.
- [x] Targeted static inspection of core result/request types, identity, ownership, inventory, log split, structural graph, terrain state and save coordinator recorded in [the reconciliation review](PHASE_01_SHARED_DATA_CONTRACTS_RECONCILIATION.md).
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
- [x] All 14 JSON Schema Draft 2020-12 schemas pass validator meta-schema checks in GitHub Actions run [40583305901](https://github.com/furkansarikiyak36-eng/myproject3/actions/runs/40583305901).
- [x] 45 representative sample records validate against their assigned schemas.
- [x] Registry validator confirms 119 unique mechanic IDs, valid entry records and resolved mechanic dependencies.
- [x] Sample definition IDs are unique within each schema family.
- [ ] Check every cross-reference across the complete production catalogues. The current validator deliberately tolerates references to definitions absent from the representative sample set.
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
- [ ] Decide where the global operation ledger must be adopted versus domain-owned receipts; inspected domain source does not consistently call TryBeginOperation/CompleteOperation.
- [ ] Normalize caller operation-ID propagation for helper transactions that currently generate their own ID, or explicitly document a narrower retry guarantee.

## D. Placement and physical geometry
- [x] Existing shared placement kernel/profile architecture is recorded as the authority.
- [x] Build-part schema uses cm for dimensions, explicit local axis/pivot/collision and semantic anchor IDs.
- [ ] Validate every active profile against the compiled built-in catalogue and project JSON.
- [ ] Resolve mirrored diagonal intersections, missing mitre end cuts, authored joinery sockets and deck path acceptance.
- [ ] Confirm preview/commit parity using the same placement solution.

## E. Terrain, save/load and runtime
- [x] Logical terrain state and mesh prototype are described as separate components.
- [x] Current known save/load gaps are recorded.
- [ ] Validate terrain schema to chunk renderer/collision adapter mapping; current logical cell state and voxel chunk density are separate APIs with no adapter found in the inspected implementation files.
- [ ] Verify actor transforms, attachments, item ownership, structural graph and terrain edits in one save/load round trip.
- [ ] Compile/UHT/UBT against the selected source integration branch using Unreal Engine 5.8.
- [ ] Run Editor/PIE acceptance scenarios and store logs/captures/reproduction steps.
- [ ] Record performance for the bounded voxel region and snap solve before widening scope.

## Closeout rule

P01 may close as a **design/data contract phase** when schema validation, sample fixtures, ID integrity, cross-reference validation, and code-contract reconciliation are recorded. The first four catalogue checks now have CI evidence. Full cross-reference validation, caller/API integration review, and Unreal build/runtime checks remain open. A catalogue CI pass is not proof that Unreal code compiles or that the game plays correctly.
