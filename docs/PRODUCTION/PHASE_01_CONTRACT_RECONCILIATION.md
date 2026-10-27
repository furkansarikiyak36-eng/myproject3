# Phase 01 — Shared Contract Reconciliation

**Status:** design/reconciliation baseline created; source integration and UE validation remain open  
**Repository:** furkansarikiyak36-eng/myproject3  
**Planning branch:** planning/gdd-inventory-production-roadmap  
**Engine target:** Unreal Engine 5.8

## Decision summary

Do not create a second identity, operation result, event dispatcher, inventory authority or construction graph. Reuse the candidate source contracts already present on the stacked phase branches, but only after source ownership and callers are reconciled. The presence of these files on a branch does not mean they are integrated into main or verified by UE 5.8.

## Existing candidate contracts found

| Contract | Candidate source | Current boundary | Integration status |
|---|---|---|---|
| Shared operation/result types | `GameplayCoreTypes.h/.cpp` on phase-02-core-contracts and later stacked branches | Stable result codes, operation request/result records, unitized resource amount | Not on current main; compile/integration unverified |
| Physical actor identity | `WorldObjectIdentityComponent.h/.cpp`, `WorldObjectIdentitySubsystem.h/.cpp` | Actor instance GUID, definition key, lifecycle metadata, world-local uniqueness | Opt-in; existing actors not consistently migrated |
| Committed event broadcast | `WorldCommittedEventSubsystem.h/.cpp` | Publish a committed fact after mutation | Transient only; no durable event history |
| Operation deduplication | `WorldOperationLedgerSubsystem.h/.cpp` on later stacked branch | Reserve/complete/cancel an operation ID in memory | Does not perform rollback; not durable |
| Item ownership | `WorldItemOwnershipSubsystem.h/.cpp` on later stacked branch | World/Held/Inventory/Storage/InTransit/Consumed/Transformed ownership records | Conflicts in authority with current inventory stack array until reconciled |
| Inventory/crafting prototype | `InventoryCraftingTypes.h`, `InventoryCraftingComponent.h/.cpp` | Stack definitions/quantities, slot/weight checks, recipe transaction prototype | Not full world pickup/UI/save integration |
| Physical log state | `LogBuildPiece.h/.cpp`, `LogBuildingTypes.h` | Log processing state, mesh variants, snap anchors, parent/child and held/placed state | Existing current-main authority for log prototype |
| Cross-material structural graph | `WorldStructuralGraphSubsystem.h/.cpp` on later stacked branch | Nodes, edges, support checks, removal order and damage fields | Duplicate relation risk with actors' local attachment fields |
| Logical terrain state | `WorldTerrainStateSubsystem.h/.cpp` on later stacked branch | Bounded cell edit records, depth limits, revisions and operation receipts | Not a voxel renderer/collision/navigation implementation |
| Logical clock | `WorldSimulationClockSubsystem.h/.cpp` and coordinator | Logical time, tick index, seed, versioned clock snapshot | Requires an owning bootstrap/fixed-step driver; coordinator atomicity issue identified |
| Save snapshot | `WorldSimulationSaveGame.h`, `WorldSaveCoordinatorSubsystem.h/.cpp` | Versioned snapshot of several logical subsystems | Does not yet prove lossless placed-structure/graph/held-object round-trip |

## Contract A — IDs and definitions

1. Persistent physical world instances use a stable `FGuid InstanceId`; definitions use stable `FName DefinitionId`. Display names and asset paths are not identifiers.
2. A resource type such as wood is a definition, not a unique world instance. A physical log actor has an instance ID and a processing state.
3. A stackable inventory resource has a definition ID and a quantity. If a stack has its own persistent record, that record also needs a separate stable stack-instance ID. Do not confuse item-definition IDs and physical-instance IDs.
4. IDs are never renumbered after release. Renames change display/localization metadata, not IDs.
5. Save records store stable IDs, revisions, schema versions and domain state—not transient actor names, array positions, or localized text.

## Contract B — authoritative ownership

**One item quantity may have one authoritative owner at a time.**

The candidate code currently has two overlapping representations: `InventoryCraftingComponent.Items` stores `FInventoryStack` records with definition IDs and quantities; `WorldItemOwnershipSubsystem` stores owned-item records that also have a definition ID, quantity and owner kind. They must not both be independently mutated for the same stack.

Required integration decision:
- Define whether an item is a unique physical instance or an abstract stack.
- For a unique physical object (log, stone piece, tool instance), use a stable instance/ownership record and explicit World → Held → World/Storage/Inventory/Consumed/Transformed transitions.
- For a stackable abstract resource, one system must own the canonical quantity. Other components may present slots/UI or provide transaction facades, but may not keep an independently mutable duplicate count.
- Pickup, drop, stack, split, storage transfer, craft, consume and transform must use one transaction boundary and a revision/operation ID where retry duplication is possible.
- Migration from the current inventory array must define how saved stacks are converted; until then, do not activate both ownership authorities together.

## Contract C — domain state versus common lifecycle

The generic lifecycle state is not a substitute for domain-specific state.

- Log processing owns whole/split/quarter/hewn/groove/trough/plank state and its valid transition table.
- Placement/structural state owns anchor occupancy, parent/child/support relation, damage and safely-removable rules.
- Inventory/storage owns stack slots and quantities only after ownership authority is selected.
- Terrain owns the authoritative edited-cell/material/depth state; a rendering/collision adapter reflects it.
- Crops/animals own their lifecycle values; visuals are projections of state.
- Survival owns exposure/needs calculations; UI, animation and audio report the result.

A mesh swap or animation does not itself commit a gameplay state transition.

## Contract D — operation request and result

All cross-domain operations should use the common request/result vocabulary defined by the candidate `GameplayCoreTypes` types. Preserve the current bool/Blueprint APIs as adapters until caller/Blueprint compatibility is reviewed; do not change public signatures casually.

Every operation must define:
- operation ID and operation type;
- target instance IDs and expected revisions;
- requester/owner when relevant;
- required definition/tool/recipe keys and context;
- read-only preflight;
- one commit point;
- result code and IDs/revisions actually changed;
- explicit failure reason with no partial mutation;
- idempotent retry behavior, where retries are possible.

The operation ledger may prevent duplicate requests, but it is not a transaction manager. State mutation and rollback still belong to the domain operation. Never publish success before the owner commits.

## Contract E — event and persistence semantics

- `WorldCommittedEventSubsystem` is notification fan-out, not a persistent event store.
- Events report committed facts only, carry a stable operation ID, event type and schema version, and must not be emitted on a failed preflight.
- Persistent world state is captured by explicit versioned save contracts. Current candidate `WorldSimulationSaveGame` uses a schema version, but each contained domain still needs round-trip coverage.
- A save must either capture all supported state or fail explicitly rather than silently dropping held items/structural relationships.
- Restore ordering must establish stable identities, logical clock and world/terrain state before dependent actors, structural graph, inventory/storage, production/threat and player state. Derived collision/navigation/visual data is rebuilt and cross-references are validated before gameplay resumes.
- Schema versioning is not the same as migration. Every supported old schema needs an explicit migration or safe rejection policy.

## Contract F — construction relationship ownership

Current `ALogBuildPiece` stores local snap-anchor occupancy and parent/child pointers. The candidate `WorldStructuralGraphSubsystem` also stores graph edges and support/removal information.

Required integration model:
1. The piece/mesh provider owns geometry, local anchors and compatibility metadata.
2. A single accepted attach/detach operation validates geometry and graph constraints, then commits anchor reservation, actor attachment and graph relation transactionally.
3. The cross-material structural graph is the intended shared authority for support/removal analysis, but it must not be activated as a second independently mutable parent/child store.
4. Until migration is complete, existing actor-local data is the compatibility authority for the log prototype; graph state is synchronized through one adapter and checked for equality.
5. A failed engine attachment must roll back the graph edge, anchor occupancy and actor transform. A successful preview must be the exact placement solution committed.

## Contract G — bounded terrain

The candidate `WorldTerrainStateSubsystem` is a logical edit-state service. Before calling it a functional voxel system, an engine adapter must demonstrate:
- actual visible terrain geometry changes;
- collision and, where relevant, navigation update;
- configured diggable materials/depth/range;
- bounded per-operation work and edited-cell count;
- excavation resource outputs without one inventory object per voxel;
- safe interaction with support structures;
- save/load and deterministic restore of edited geometry.

Tunnels and small rooms are in target scope; unlimited depth, every-enemy digging and fully simulated voxel/structure physics are not.

## Phase 01 acceptance gates

- [x] Existing candidate core contract files located and their ownership boundaries read.
- [x] Duplicated-authority risks identified explicitly.
- [x] Stable IDs, operation semantics, domain ownership and save/event expectations documented.
- [ ] Choose the implementation head that contains desired snap work and the accepted foundation without conflicting kernels.
- [ ] Resolve inventory quantity ownership and structural relation ownership in source contracts.
- [ ] Validate schema examples and add automated schema checks.
- [ ] Review all modified C++ call sites and any Blueprint consumers.
- [ ] Run the integrated UE 5.8 build/UHT and the appropriate editor/runtime acceptance cases.

**Phase 01 is not code-complete or engine-verified.** This document closes the initial contract-reconciliation draft only.
