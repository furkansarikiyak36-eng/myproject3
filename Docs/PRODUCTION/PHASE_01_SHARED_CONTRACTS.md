# Phase 01 — Shared Data and Operation Contracts

**Status:** contract consolidation/specification in the planning branch; no gameplay code copied or changed by this document.  
**Target:** Unreal Engine 5.8  
**Canonical mechanic scope:** M001–M119 in this folder  
**Rule:** Reuse compatible existing source contracts after branch integration review. Never introduce a second identity, item-ownership, placement-kernel or save-state authority.

## 1. Contract ownership matrix

| Contract | Current source candidate | Authority rule | Gap before adoption |
|---|---|---|---|
| Operation request/result | `GameplayCoreTypes.h/.cpp` (Phase 2+) | Caller requests; domain owner validates and commits; result reports stable code and operation ID | Review all call sites and UHT exposure in the integration branch |
| World-instance identity | `WorldObjectIdentityComponent` and `WorldObjectIdentitySubsystem` (Phase 2+) | Optional metadata; domain actor remains owner of processing/structure-specific state | Decide migration actor-by-actor; validate identity through load/duplicate actor lifecycle |
| Committed event | `WorldCommittedEventSubsystem` (Phase 2+) | Reports a fact only after authoritative commit; not a state owner | No durable event journal/replay; consumers must tolerate transient notification |
| Item definition/stack | `InventoryCraftingTypes.h` (Phase 6+) | `FName` item key in current stack type is a definition/catalog key, not a physical instance GUID | Current stack type lacks a normalized full definition schema, volume and per-instance durability/state |
| Unique item ownership | `WorldItemOwnershipSubsystem` (H+) | `FGuid ItemId` identifies one owned item; `FName DefinitionId` identifies its definition | Integrate world actor/inventory/storage/production adapters; resolve split/merge/transform ownership semantics |
| Wood processing | `ALogBuildPiece` plus wood types | Piece state and geometry must agree; actor is the existing domain authority until migration is approved | Reconcile the legacy and contextual snap paths; define new transitions without breaking Blueprint callers |
| Structural anchors | placement kernel/provider profiles in latest candidate; legacy snap points in main | One material-neutral placement kernel; providers own material-specific anchor rules | Verify exact branch revision and authored anchor coverage; preview must equal commit |
| Terrain state | `WorldTerrainStateSubsystem` (H+) | Logical cell edit state/revision is separate from terrain rendering/collision | Runtime voxel backend, collision/nav adapter and actor/building interaction not present in the logical state model |
| World clock/simulation | `WorldSimulationClockSubsystem` / coordinator (H+) | One authoritative fixed-step time source; domains preflight then commit | Restore and rollback behavior requires fault-injection and engine validation |
| Save manifest | `WorldSimulationSaveGame` / `WorldSaveCoordinatorSubsystem` (H+) | One versioned manifest; restore order and cross-reference validation are centralized | Current candidate does not persist all physical transforms/structural links/terrain collision data; held-item save was refused in the reviewed version |
| UI/animation/VFX/audio | catalogues in `Docs/GDD` | Presentation consumes committed domain outcomes; no feedback system owns gameplay state | Event IDs, payloads, fallback and accessibility mix need wiring after action state is stable |

## 2. Stable identity model — critical distinction

Three different IDs must not be conflated:

1. **Definition ID** — a stable catalog key, currently often represented as `FName` (e.g. `MAT_WOOD_WHOLE_LOG`). It answers “what kind of thing is this?”
2. **World object / unique owned item ID** — a `FGuid` for a physical instance/resource record. It answers “which exact thing is this?”
3. **Operation ID** — a `FGuid` identifying one attempted/committed state-changing action. It supports diagnostics and retry/idempotency.

The existing `FInventoryStack.ItemId` is a `FName` catalog key, while `FWorldOwnedItemRecord.ItemId` is a `FGuid` unique item identifier and its `DefinitionId` is a `FName`. This name collision is dangerous if both types are described generically as “item ID”. Preserve compatibility at existing APIs while using explicit naming in schemas/docs and adapters: `DefinitionId`, `InstanceId` or `OwnedItemId`, and `OperationId`.

A physical log or boulder must remain one uniquely owned world object; it must not become a quantity stack of multiple physical objects. Small resource quantities can be stackable. A split, transform, or crafting output must explicitly consume/transform its input ownership and register distinct outputs atomically.

## 3. Shared operation contract

Every state-changing request must define:
- `OperationId` (GUID) and a stable operation type/key.
- Requester identity when relevant.
- Target instance IDs and any expected revisions.
- Definition IDs for tool, recipe, material or action configuration where applicable.
- A validated context (range/trace/anchor/terrain/crop state) owned by the relevant domain.
- A deterministic result code and explanatory message.
- Affected object IDs only after a successful commit.

Required execution order:

1. Resolve the owner, target and requested action.
2. Validate identity, target type, range, tool/material, state transition, capacity, compatibility and expected revisions.
3. Calculate the proposed result without mutating durable state.
4. Recheck relevant revisions/ownership before commit.
5. Commit exactly once or leave authoritative state unchanged.
6. Publish committed events after the domain state commits.
7. Record changed state for persistence; return the stable result.

A failure must not consume resources, reserve snap anchors, update processing state, move item ownership or publish a success event. Retries using the same operation ID may return the prior outcome only when the entire request matches; reusing an operation ID for a different request is a conflict.

## 4. Instance lifecycle vs domain state

Do not create one universal enum for every state.

- **Generic instance lifecycle** describes ownership/existence, such as World, Held, Placed, Consumed or Destroyed.
- **Ownership ledger** uses World, Held, Inventory, Storage, InTransit, Consumed or Transformed, with owner ID required for owner-bound states.
- **Wood processing** describes geometry/state, such as WholeLog, SplitHalfLog, QuarterLog, Hewn, Grooved, Trough or Plank.
- **Construction** describes attachment, anchor occupancy, support relationships, damage and repair.
- **Terrain** describes cell/material/depth/edit/revision.
- **Agriculture** describes crop and animal state.
- **Survival/environment** describes exposure, needs, weather and region state.

A generic lifecycle transition must not silently change the domain-specific state. For example, making a log Placed cannot turn a split-half log back into a whole log; ownership transfer to Inventory cannot duplicate an existing World actor.

## 5. Resource and item-definition contracts

- Resource amount must reference a definition ID and explicit unit ID; quantity must be finite and non-negative.
- Item definitions must specify category, representation (physical actor vs stackable pickup vs equipment/story record), mass, volume, stack limit, durability/state model, acquisition sources, allowed actions, recipes and persistence policy.
- Large or awkward physical pieces use world actors and explicit carry/drag/pickup semantics. Small resources use data stacks.
- Durability, filter life, freshness, contamination, moisture, processing state and crop state are present only where they affect gameplay.
- Each recipe defines inputs, preconditions, output/by-products, duration, interruption, tool wear/fuel/water/noise costs and failure rollback.
- No active recipe may reference a missing item definition; no definition may be added to the active economy without a source and useful outcome.

## 6. Structural placement contract

- One shared, material-neutral placement kernel.
- Each material provider owns permitted anchor types, source/receiver roles, accepted piece families, orientation and joint-specific rules.
- Anchor identity is a stable semantic ID (e.g. end/socket/feature ID), not a transient array index.
- A target anchor must be placed, receiving-capable, compatible and unoccupied; the source anchor must be compatible and free.
- Preview and commit consume the same resolved placement solution; do not solve a different pose on commit.
- Attachment, anchor reservation and parent/child graph bookkeeping commit atomically. Failed engine attachment must leave both anchors and the graph unchanged.
- Parent removal/support changes have explicit child behavior. The first scope does not require full structural physics for every arbitrary mesh.

## 7. Terrain contract

- Logical terrain edit uses operation ID, cell/chunk coordinate, allowed depth layer, edit kind, material ID and expected revision.
- Each edit validates region/depth/material/operation limits before changing the logical state.
- Logical state success is not equivalent to a playable voxel edit: the engine adapter must update mesh, collision, navigation where needed and attached structure/support behavior.
- Terrain changes and generated materials share one resource-yield policy; do not create one item per changed voxel.
- Saves store authoritative terrain deltas and schema version, not transient particles or derived mesh data. Runtime reconstruction must rebuild mesh/collision and validate cross references.
- The approved target is medium-depth digging and small underground spaces. Unlimited depth, every enemy digging, and full voxel-to-every-building physics are out of scope.

## 8. Event and feedback contract

Committed events are facts, not intents or previews. Each event includes stable event type, schema version, operation ID, affected instance IDs and the state data needed by intended listeners. Listeners must not mutate the original operation as a side effect.

Animation, UI, sound and VFX respond to committed results:
- valid/invalid snap;
- accepted/rejected processing;
- staged stone fracture;
- terrain edit accepted/rejected;
- item transfer/recipe completion/failure;
- exposure/weather and raid warning;
- crop stage and restoration milestones.

Audio/VFX failure never changes gameplay success. Important warnings cannot depend on music alone.

## 9. Persistence contract

- Save manifest has a schema version, world ID/seed/time, definitions/version references, physical instance records, domain state, item ownership, structural links, inventory/storage/production/terrain/crop/animal/survival/discovery state and any required operation receipts.
- Physical actors need stable instance ID, definition ID, transform, domain state and graph links; restore must not spawn duplicates for items already represented in ownership.
- Restore in dependency order: header/time/identity → terrain/material state → physical instances/domain state → structural links/anchors → item ownership/inventory/storage → crop/animal/production/raid/survival/discoveries → derived mesh/collision/navigation rebuild → consistency audit.
- Unknown save versions must be rejected or migrated explicitly. A version number alone is not migration support.
- Until Held carry state is part of the save contract, saving while holding an item must fail explicitly rather than produce an orphaned item. This is a known limitation of the reviewed candidate, not the final intended behavior.
- A logical snapshot save is not a complete world save until physical actors, transforms, structural links and terrain render/collision state are restored correctly.

## 10. Phase 01 deliverables and acceptance

- [x] Import the existing canonical M001–M119 CSV/JSON/Markdown registry and schema without changing IDs or verification statuses.
- [x] Inventory existing candidate contracts by source branch and identify who owns state today.
- [x] Record definition vs unique-instance vs operation ID distinction.
- [x] Record contract seams between stack inventory and unique item-ownership ledger.
- [x] Record terrain-state vs actual voxel renderer/collision distinction.
- [x] Separate code/source existence from Engine/UHT/PIE verification.
- [ ] Add/review JSON schemas for operation, item definition, resource amount, ownership, event, processing, anchors, terrain edits and save manifest.
- [ ] Validate JSON schema examples mechanically.
- [ ] Choose the exact code integration head and reconcile public/Blueprint caller compatibility.
- [ ] Run Unreal 5.8 UHT/UBT, Editor and PIE gates after authorization.

Phase 01 is ready for schema validation and branch integration review. It is not a claim that the C++ contracts have been integrated into main or successfully compiled.
