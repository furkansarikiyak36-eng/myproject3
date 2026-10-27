# Phase 01 — Shared Data and Gameplay Contracts

**Branch:** planning/gdd-inventory-production-roadmap  
**Status:** Design/schema draft prepared; final mapping to the integrated C++ implementation remains open.  
**Target:** Unreal Engine 5.8.  
**Constraint:** This document does not introduce a parallel C++ subsystem.

## 1. Contract ownership

| Concern | Candidate implementation | Current-main status | Phase 01 rule |
|---|---|---|---|
| World object identity and generic lifecycle | GameplayCoreTypes.h, WorldObjectIdentityComponent.*, WorldObjectIdentitySubsystem.* in phase-02-core-contracts | Exact paths absent from main | Review and reuse only after branch-diff/integration review |
| Operation request/result | FWorldOperationRequest, FWorldOperationResult, EWorldOperationResultCode in Phase 02 | Exact header absent from main | One authoritative result vocabulary; no second enum/API |
| Unitized resource amount | FWorldResourceAmount candidate in Phase 02 | Exact header absent from main | Units explicit; values finite and non-negative |
| Committed events | WorldCommittedEventSubsystem.* in Phase 02 | Exact paths absent from main | Events are committed facts; current notifier is transient, not persistence |
| Wood processing and snap state | ALogBuildPiece, FLogSnapPoint, ULogCarryComponent in main; updated processing API in Phase 03 candidate | Legacy code exists on main; branch deltas differ | Preserve current owner until caller and Blueprint compatibility review |
| Placement kernel/profiles | Source/MyProject3/Placement and Config/Placement/PlacementProfiles.json on snap branches | Not part of main source surface reviewed | Keep material-neutral kernel and provider/profile boundary; compare snap branches |
| Inventory and recipes | InventoryCraftingTypes.h, InventoryCraftingComponent.* in Phase 06 candidate | Exact paths absent from main | Use candidate as baseline; do not create a second inventory authority |
| Terrain state | WorldTerrainStateSubsystem.* in integrated-world candidate | Exact paths absent from main | Treat as a candidate terrain state ledger; rendering/collision and persistence need verification |
| Save coordinator | WorldSaveCoordinatorSubsystem.*, WorldSimulationSaveGame.h in integrated-world candidate | Exact paths absent from main | Design restore order; do not claim persistence until round-trip tests pass |

## 2. Identifier rules

- Definition IDs are stable machine-readable IDs: uppercase ASCII with underscores, e.g. ITEM_WOOD_BRANCH, MAT_SOIL_CONTAMINATED, RECIPE_LOG_SPLIT, EVT_OBJECT_PROCESSED.
- Instance IDs identify one runtime world object and are different from definition IDs.
- Display text is localized presentation, never identity.
- Mechanic IDs M001–M119 are immutable and are not item/recipe IDs.
- Anchor IDs are semantic feature IDs, not mutable array indexes.
- Operation IDs identify a state-changing request and support diagnostics/idempotency where the owning subsystem implements it.
- Event IDs identify event types; event-instance IDs identify a single committed notification.

## 3. State-changing operation contract

Every request follows this order:

1. Resolve requester, target, operation, definition and required revision.
2. Validate identity, range, ownership, domain state, required tool/material and configuration.
3. Calculate a read-only proposed result.
4. Revalidate revision/anchor/ownership assumptions immediately before commit.
5. Apply all changes atomically at the domain owner.
6. Return a machine-readable result with operation ID and affected object IDs.
7. Emit a committed event only after the state change commits.
8. Mark durable state dirty through the authoritative persistence path.

Failure invariants:
- No ingredient/tool charge is consumed on failure unless a documented partial-progress rule applies.
- No snap reservation or parent-child mutation occurs before successful attachment.
- No success event is emitted before the authoritative commit.
- A retry must not duplicate item output or reapply an edit where idempotency is implemented.
- UI, audio, VFX and animation are feedback only; they do not decide whether an operation succeeded.

## 4. Instance lifecycle versus domain state

Candidate generic lifecycle values: World, Held, Placed, Consumed, Destroyed.

This is not a universal gameplay-state enum. Domain state remains separate:
- Wood processing: whole, split half-round, quarter, hewn/squared, groove, trough, plank and other explicitly supported mesh states.
- Construction: placement, anchor occupancy, parent-child graph, integrity and damage.
- Inventory: owner/container, stack quantity, equipment slot, durability/charge.
- Terrain: material, depth/edit layer, excavated/replaced state and revision.
- Crops: planted/growing/mature/harvested/dormant/dead where applicable.
- Region: contamination, soil/water recovery, vegetation stage and unlock flags.

A generic lifecycle transition can coordinate a domain owner but must not duplicate or replace its authoritative state.

## 5. Units and quantities

- Length: Unreal centimetres internally; convert external measurements at ingestion.
- Mass: integer grams for carried-item accounting; UI can display localized rounded values.
- Volume: cubic centimetres or a declared discrete capacity unit; never mix without a converter.
- Resource quantities: value plus stable unit ID, never an unlabeled float.
- Duration: seconds for real-time actions; crop/simulation time names its time basis.
- Coordinates: actor transforms for world actors; stable cell/region coordinates for terrain records. Actor names are not persistent references.
- Numeric configuration rejects negative, non-finite or out-of-range values before mutation.

## 6. Schema files

These JSON Schemas are data contracts, not runtime-loaded Unreal content yet:
- schemas/item-definition.schema.json
- schemas/material-definition.schema.json
- schemas/recipe-definition.schema.json
- schemas/world-object-instance.schema.json
- schemas/world-operation.schema.json
- schemas/feedback-event.schema.json

A later implementation task must choose an Unreal representation (for example Data Assets, Data Tables or validated JSON ingestion) and prove it in UE 5.8. The schemas do not assume Unreal enforces JSON Schema at runtime.

## 7. Candidate type mapping

| Candidate type/file | Contract responsibility | Required review |
|---|---|---|
| FWorldOperationResult | Operation outcome | Failure factories, affected IDs and caller compatibility |
| FWorldOperationRequest | Operation request | Required targets, expected revisions and idempotency boundary |
| FWorldResourceAmount | Unitized quantity | Unit registry and conversion ownership |
| UWorldObjectIdentityComponent | Instance identity and generic lifecycle | Duplicate identity, save stability and opt-in actor migration |
| UWorldObjectIdentitySubsystem | Active-world lookup | World lifecycle, duplicate resolution and teardown |
| UWorldCommittedEventSubsystem | Post-commit notification | Ordering and explicit non-persistent nature |
| FInventoryStack | Current stack representation | Definition lookup, weight, stack compatibility, ownership and save |
| FCraftingRecipe | Recipe representation | Output/byproduct rollback, operation identity and cancellation/session flow |
| ALogBuildPiece | Legacy log-piece authority | Public API/Blueprint calls, processing state, anchors and attachment links |
| UWorldTerrainStateSubsystem | Candidate bounded terrain-edit ledger | Rendering/collision adapter, bounded data size, persistence and duplicate edits |

## 8. Phase 01 exit criteria

- [ ] This contract is reviewed against the relevant branch stack.
- [ ] Stable IDs and their owning data files are assigned.
- [ ] Candidate C++ structures are mapped to schema fields; no duplicate type is added without a documented gap.
- [ ] Log and construction state remains domain-owned; generic lifecycle does not shadow it.
- [ ] Inventory ownership transitions (world → held/inventory → placed/consumed) are explicit and atomic.
- [ ] Operation-result and committed-event semantics are documented for success/failure.
- [ ] Persistence fields, versioning and restore dependencies are identified; incomplete work is marked.
- [ ] Schemas parse and sample records validate with a JSON Schema validator.
- [ ] Unreal/UHT/build/runtime checks are recorded separately; this document alone does not pass them.
