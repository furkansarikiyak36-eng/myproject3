# Group A — Asset-Independent Mechanics Closure Pass

**Project:** MyProject3  
**Branch:** `group-a-code-closure`  
**Base:** `integration/phase-02-14-closure`  
**Date:** 2026-10-29  
**Evidence status:** Source changes and automation-test cases authored; Unreal 5.8 UHT/UBT, editor automation, PIE and end-to-end gameplay acceptance have not been run in this pass.

## Decision

This pass closes a set of **asset-independent source contracts**. It does not declare the nine reference groups fully gameplay-complete. Physical assets, actor/Blueprint wiring, UI, navigation and runtime acceptance remain separate gates. In particular, actor-backed operations must not be marked complete just because a logical subsystem or receipt type exists.

## Work completed in this branch

### P0 — Sons of the Forest: canonical log processing

- Keeps log length fraction and cross-section profile as independent canonical dimensions.
- Keeps cut eligibility dependent on explicit source state, action rules, configured output mesh variants and split offsets.
- Adds bounded, SaveGame-backed operation receipts for incremental cut progress. Replaying the same operation/action/work returns the previous result; reusing the operation ID with different work is rejected; receipt-capacity exhaustion fails closed before progress is changed.
- Existing split receipts continue to bind physical split output IDs to the ownership ledger.

**Remaining acceptance:** UE build, exact mesh/offset/collision checks for each authored family, fault-injection on second-actor materialization, and preview-versus-commit checks in the engine.

### P0 — 7 Days to Die: raid pulse idempotency and constrained terrain

- Raid state now stores the last claimed damage-pulse index. The adapter claims a pulse before structural damage and sorts/de-duplicates target node IDs so retry cannot apply that pulse twice.
- Snapshot validation rejects malformed empty raid records, invalid state values and invalid pulse indices. An active raid can rematerialize its pawn after adapter reset and reconstruct its next pulse deadline from persisted state.
- Terrain edits have bounded, durable operation receipts with deterministic export order. Receipt history is restored atomically with logical terrain state; a resource-yield receipt cannot be replayed through a weaker logical-only API.
- The voxel dig path retains bounds, soil strength, expected revision and grounded-support clearance guards.

**Remaining acceptance:** run raid damage/failure-injection tests in UE; connect full targeting, pursuit, navigation, attack selection and collapse/salvage behavior; verify dig impacts on actual foundations and navigation.

### P0 — Hydroneer: excavation → owned resource → production stock

- Defines explicit material/depth-band yield rules with deterministic volume-to-unit conversion and a per-operation yield cap.
- A bounded excavation transaction snapshots logical terrain, voxel density/receipts and the item ledger; it rolls them back together if physical dig, logical commit or resource registration fails. Rollback failure faults the coordinator rather than silently continuing.
- Successful yields have unique item IDs under the canonical ownership ledger and a terrain receipt containing the voxel identity/revisions, removed volume, yield rule and output IDs.
- Generic ledger-only world resources can be ingested into recipe-addressable station stock. The source item transitions from World to Transformed once; stock capacity and already reserved recipe outputs are preflighted; retries return the same stock key. Physical log actors use their domain-specific intake instead.
- Save schema v17 adds terrain resource receipts; schema v18 adds generic production intake receipts.

**Remaining acceptance:** verify the bounded dig and resource yield in UE, verify collision/seams and physical item presentation, and execute the save → dig → save → load → retry path.

### P1 — Death Stranding: route context and cargo recovery

- Routes can be solved with explicit slope, surface multiplier, weather severity, carried-mass and infrastructure context. Invalid numeric inputs fail closed; a required but absent infrastructure edge is unavailable.
- Stable sorted traversal makes equal-cost route selection deterministic.
- Cargo records retain the route cost context. When the saved path no longer matches current edge costs, the shipment becomes Blocked with its item still owned InTransit and its destination reservation retained.
- Replanning uses that same cargo/item/reservation identity; it does not spawn or deliver a second item.

**Remaining acceptance:** bind costs to live terrain/road/bridge/rope actors and transforms; validate route invalidation, physical movement, destination reservation rollback and delivery in UE.

### P1 — Farming Simulator: item-backed crop and animal actions

- Adds data-driven crop definitions and explicit action receipts.
- Planting consumes an exact inventory-owned seed item and commits one crop identity.
- Harvesting requires a mature crop and creates bounded unique owned output items once. Harvested is terminal and cannot re-enter growth.
- Feeding consumes the exact configured feed definition from inventory. Animal product collection materializes its configured outputs once.
- Restore validates receipt identity and references to existing ownership-ledger items.

**Remaining acceptance:** physical plot/soil actors, season/rain/nutrient inputs, habitat/pathing/reproduction, and full save/load/run tests.

### P1 — Satisfactory: production/resource intake contracts

- Adds durable receipt support for generic world-item intake into station stock, in addition to the physical-log intake, job enqueue/collection and output materialization receipts already present.
- Cross-command operation IDs are checked across the production intake/enqueue/collect/materialize paths.
- Input stock is consumed and output capacity reserved before a job is accepted; completed jobs are terminal and retries replay recorded results.

**Remaining acceptance:** validate all recipe definitions against runtime data, physical station/actor wiring, and multi-domain atomicity under injected failures.

### P1 — The Long Dark: consumables, freshness and rest

- Adds data-driven item consumable definitions.
- Food consumption verifies exact inventory/ownership identity, applies its registered calorie/thirst effects and uses the common logical world clock for freshness.
- A stale food item follows its configured remaining-energy fraction; a fully spoiled item with a zero fraction is rejected without consumption.
- Consumption and rest requests carry durable operation receipts. Rest uses an interval on the shared world timeline; recovery uses the overlap of that rest interval with the simulation step.

**Remaining acceptance:** injury/illness/poisoning/medicine progression, equipment and clothing inventory binding, spatial exposure, player-facing needs and engine round-trip validation.

### P1 — Frostpunk: fuel as a real owned item

- Heat sources accept data-driven fuel definitions and expose an item-backed refuel path.
- Refuelling requires a uniquely owned inventory item with a matching definition and stack. Capacity is checked first; the item is consumed through ownership and inventory snapshots; a durable receipt prevents duplicate refuelling and cross-fire operation-ID reuse.
- Failed rollback marks the world coordinator faulted.

**Remaining acceptance:** prove the complete fuel stock → fire heat output → shelter/clothing/wetness/region exposure → health chain in UE and confirm component receipt persistence through actor/world save restore.

### P1 — Subnautica: durable discovery base

- The existing survival subsystem has discovery records and snapshot persistence, while the shared structural graph provides a cross-material construction base.

**Still open:** discovery actor/interaction prerequisites, visible unlock/context, actor reconstruction/reconciliation, and equipment-gated regional access. These need authored interactions but should use the already-defined identity and receipt contracts rather than introduce another ledger.

## Cross-system safety invariants

- No unknown item definition or unknown item weight should be treated as free/weightless.
- Unique physical items are identified by stable GUIDs; stack quantities do not substitute for physical-item identity.
- The same operation ID must replay the same committed result and must not create a second output or consume another input.
- Capacity exhaustion is an explicit rejection; old idempotency receipts are not silently evicted.
- Logical world-time advances through the single clock/coordinator path; rest, freshness, farming, production, weather and raid timing do not create independent clocks.
- Persistence accepts only internally consistent receipts and fails closed rather than inventing missing history.
- If rollback cannot restore the authoritative state, the simulation is faulted and must not continue as though the operation were cleanly rejected.

## Added automation-test cases (authored, not executed)

- `MyProject3.Logistics.RouteCost.IsDeterministicAndHonorsTraversalContext`
- `MyProject3.Terrain.LogicalEditReceiptSurvivesSnapshotRestore`
- `MyProject3.Terrain.ExcavationCreatesRetrySafeOwnedResourceOutputs`
- `MyProject3.Farming.HarvestCreatesOwnedOutputsOnceAndPersistsReceipt`
- `MyProject3.Farming.InputOwnershipAndProductReceiptsAreIdempotent`
- `MyProject3.Survival.RestUsesSharedClockAndIdempotentReceipt`
- `MyProject3.Survival.FoodConsumesOwnedItemAndUsesSharedClockFreshness`
- `MyProject3.Survival.HeatSourceConsumesFuelItemOnce`
- `MyProject3.Production.ResourceIntakeFeedsRecipeAndOutputDoesNotDuplicate`
- `MyProject3.Threat.DamagePulseClaimIsIdempotentAcrossRestore`
- `MyProject3.Wood.CutProgressOperationIDDoesNotAdvanceTwice`

## Not claimed by this pass

- No Unreal Header Tool or Unreal Build Tool compile was run.
- No Unreal automation test, editor session, PIE run or fault-injection run was performed.
- No reference group is declared end-to-end complete solely from source-layer changes.
- No final landscape/map, new meshes, textures, UI or authored Blueprint actor configuration is part of this pass.
- Group A should only be finally marked closed after the listed engine/runtime acceptance gates and the remaining Subnautica / survival integration requirements are resolved or explicitly re-scoped.
