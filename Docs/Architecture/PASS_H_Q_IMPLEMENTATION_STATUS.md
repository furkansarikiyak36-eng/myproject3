# Pass H–Q Implementation Status

**Branch:** `mechanic-detailing-pass-h-integrated-world-systems`  
**Base:** `mechanic-detailing-pass-g-reference-gap-plan`  
**Scope:** source-layer implementation; no Unreal build, UHT/UBT, Editor, PIE or gameplay validation was run.

This file distinguishes implemented source contracts from incomplete game-facing integration. The code is not engine-certified merely because it is committed.

## Pass status

### H — Fixed-step and operation boundary: source layer added
- Added pure clock preflight (`CanAdvanceSimulation`).
- Added threat-director target-time/tick preflight and commit API.
- Coordinator preflights clock, threat, agriculture, survival, production and cargo before stepping them from one shared logical time.
- Bootstrap validates raid tuning/time arithmetic before clock initialization.
- Added a pre-step snapshot of clock, threat, agriculture, survival, production, cargo and item ownership. If any commit unexpectedly fails, the coordinator restores the captured logical state and logs a fatal diagnostic if rollback itself fails.
- **Remaining:** rollback is best-effort because each subsystem restore can itself reject; no engine-level transaction or fault-injection run has validated the failure path.

### I — Authoritative item ownership: core registry added
- Added stable item records with definition, quantity, owner kind/ID, location and revision.
- Transfer validates expected owner/revision, fails closed on invalid data, and returns the original operation receipt on retry.
- Added consume transition and validated snapshot import/export.
- Cargo transport uses this ownership ledger and validates active cargo snapshots against item owner/revision.
- `ALogBuildPiece` registers a physical log piece under its identity ID; pickup, placement and drop transfer that record. Gameplay destruction transitions world/held items to consumed.
- `UInventoryCraftingComponent` can store a registered physical actor as one unique stack, move that item to a storage owner, and consume owned physical items without silently leaving them inventory-owned.
- **Remaining:** stone/stick actor adapters, actor respawn from inventory/storage, split/merge ownership policy and processing-output registration still need domain adapters.

### J — Structural support/removal: partial source contract
- Added deterministic leaf-first dependent-removal planning and revision preflight.
- Damage application rejects revision overflow.
- Log snap attachment already calls the structural graph; storing a log rejects pieces with dependents, and gameplay actor destruction reconciles its item ownership.
- **Remaining:** graph + actor destruction + salvage is not a fully atomic multi-actor transaction. Existing single-parent graph constraints remain.

### K — Bounded terrain: logical state layer added
- Added layer-gated cell excavation/fill/material edits, revision checks, operation receipts and snapshot import/export.
- **Remaining:** this is not a voxel renderer or collision adapter. It does not yet modify Unreal landscape/mesh collision, navigation, water, underground support or physical actors. Those must be connected before calling digging playable.

### L — Farming and animal lifecycle: logical simulation added
- Added crop stage/moisture/progress records, animal hunger/health/product records, bounded fixed-step updates and snapshot import/export.
- **Remaining:** no farm/animal actor visuals, feeding-item transaction, season/soil data model, animal habitat/pathing, or product transfer into item ownership is connected.

### M — Threat/raids: planning exists; real AI adapter remains
- Existing director schedule is now target-time preflighted.
- A configured raid pawn/structural-damage adapter exists on this branch; graph-collapse actor destruction now reconciles item ownership.
- **Remaining:** the adapter does not itself provide full target selection, navigation, pursuit/attack behavior or settlement-aware AI. Do not describe the current director as complete enemy AI.

### N — Logistics/production: logical queue and cargo state added
- Added station recipes, input consumption, output-capacity reservation, production progress, explicit blocked reasons, and bounded queue lengths.
- Added route-backed cargo that transfers item ownership to `InTransit`, progresses along a route-cost abstraction, and transfers to destination storage on delivery.
- Cargo, production, ownership and inventory snapshots are now included in the world save manifest. Active cargo restore checks the item ledger; historical delivered cargo remains valid after later item transfers.
- Inventory-owned physical items can move into a storage owner, from which the cargo subsystem can start transport using the same item ID.
- **Remaining:** route costs are not yet derived from roads, bridges, rope/zipline, terrain, cargo load, weather or threat. Cargo has no physical movement animation/pathing or destination actor-spawn adapter. Production is not yet linked to real machines/fuel/power/processing actors. Retry/stock-removal behavior and all queue edge cases need source review.

### O — Survival/weather/exploration: logical state added
- Added health, hunger, thirst, stamina, fatigue, clothing insulation, shelter protection, wetness, weather severity and persistent discovery records.
- **Remaining:** no food spoilage, fire/fuel/cooking, injury/medicine, sleep/rest action, equipment inventory binding, or region-specific temperature model is connected.

### P — Versioned logical save/load: partial coordinator added
- Added schema-versioned save payload for clock, threat schedule, ownership records, inventory stacks/quick access, terrain cells, crop/animal state, survival/discoveries, production stations/recipes and cargo. Schema is now version 2.
- Added ordered restore and best-effort rollback to a captured pre-load logical manifest if a domain rejects the snapshot; rollback failure is logged explicitly. Inventory restore validates physical-item IDs against ownership records.
- Saving is refused while an item is `Held`, because the carry-component state is not in the manifest; this avoids loading an orphaned Held owner.
- **Remaining:** save/load does not yet persist/respawn physical actors, actor transforms, structural graph links, world-partition state or terrain collision/mesh data. Schema migration from version 1 is not implemented. Cross-domain rollback is still best-effort.

### Q — 119-mechanic acceptance map: added, not executed
- Added `Docs/Testing/DEMO_ACCEPTANCE_MAP_M001_M119.md` with all 119 canonical IDs, seven designed zones and required positive/negative/retry/cross-system/save evidence.
- **Remaining:** the actual Unreal demo scene and all 119 positive/negative runs have not been built or executed. Registry statuses must remain `unreviewed` until evidence exists.

## Source-review risks to close before engine validation

1. The coordinator now attempts full logical rollback if a fixed-step commit fails; rollback failure and actor/engine side effects still need fault-injection validation.
2. Log actor ↔ item ledger ↔ inventory ↔ storage ↔ cargo logical ownership is bridged; stone/stick adapters, actor respawn, and production-output registration remain incomplete.
3. Threat scheduling still does not instantiate enemies or apply structural damage.
4. Terrain records do not update visual/collision/navigation representations.
5. Cargo↔item ownership and inventory↔item ownership are now validated on restore; production job↔recipe consistency still needs review.
6. Unreal Header Tool/Build Tool compatibility, Blueprint exposure, serialization behavior, and runtime order remain unverified.
7. The acceptance map is a plan/evidence contract, not a passed test report.

## Required final gate

Only after the remaining adapters and consistency checks are closed: run source audit, UHT/UBT build, Editor startup, PIE, and the 119-mechanic positive/negative acceptance pass; capture reproducible evidence; then revise registry statuses. No engine test result is implied here.
