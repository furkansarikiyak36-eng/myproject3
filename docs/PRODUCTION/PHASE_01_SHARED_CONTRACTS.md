# Phase 01 — Shared Contracts and Data Models

**Status:** Design/compatibility contract for the planning branch  
**Target:** Unreal Engine 5.8, single existing MyProject3 module  
**Source policy:** Reuse and reconcile existing types on phase-02-core-contracts and later branches. Do not add a parallel type until it is proved necessary.

## 1. Purpose

This document sets the shared rules needed to add systems without duplicating state or breaking the existing log prototype. It is not permission to migrate every actor to a new subsystem. The project’s current native classes, local Blueprint dependencies, and the candidate phase branches must be reviewed before changing a public API.

## 2. Authoritative ownership rules

| Domain | Current/candidate owner | Must not become |
|---|---|---|
| Existing log geometry, processing prerequisites, snap anchors and attachment | ALogBuildPiece until an explicit reviewed migration | A duplicate copy of log state in an identity/graph subsystem |
| Player interaction, carry preview and input flow | ULogCarryComponent / character input | A global dispatcher that owns every mechanic |
| Stable instance identity + definition reference | Opt-in WorldObjectIdentityComponent; registry in WorldObjectIdentitySubsystem | A shadow copy of domain state |
| Shared operation result/request vocabulary | GameplayCoreTypes | A second enum/request envelope in each system |
| Transient committed-event notification | WorldCommittedEventSubsystem | A durable event log or a substitute for committing state |
| Operation ID deduplication | WorldOperationLedgerSubsystem where integrated | A guarantee of durable exactly-once behavior or atomic world mutation by itself |
| Individual durable item ownership | WorldItemOwnershipSubsystem candidate | A second independent owner for the same item held in inventory stacks |
| Abstract stack inventory/recipes | InventoryCraftingComponent/InventoryCraftingTypes candidate | A second representation of the same quantity without a reconciliation rule |
| Cross-material structural graph | WorldStructuralGraphSubsystem candidate, only after actor integration is explicit | A duplicate of all per-piece geometry and snap-point data |
| Bounded logical terrain edits | WorldTerrainStateSubsystem candidate | The voxel renderer/collision/navigation adapter; these are separate responsibilities |
| Clock and fixed-step scheduling | WorldSimulationClockSubsystem + explicit coordinator caller | An autonomous per-frame clock which races domain ticks |
| Crop/animal, survival, threat, logistics and production state | Existing candidate domain subsystem in the corresponding branch | Another subsystem implementing the same formula/state independently |
| Save orchestration | WorldSaveCoordinatorSubsystem candidate | An unversioned dump that bypasses owner validation or rollback policy |

## 3. Canonical identity and definition keys

- A persistent instance uses a stable FGuid; a definition uses a stable FName/string key such as ITEM_WOOD_BRANCH or BUILD_LOG_BEAM.
- Display/localized names are never persistent IDs.
- An instance record separates immutable/slow-changing identity and definition from mutable state and revision.
- A state-changing request carries the expected revision where concurrent/retried updates are possible. A stale revision fails before mutation.
- A quantity uses a named unit and is checked for finite, non-negative (or positive where required) values. Do not compare untyped quantities from different units.
- IDs are stable once used in save data. Renaming an in-game display name must not change the ID. Retired IDs remain reserved.

Suggested namespaces:
- ITEM_* for item definitions
- MAT_* for material definitions
- TOOL_* for tool definitions
- RECIPE_* for player recipes
- PROD_* for station/production-job definitions
- BUILD_* for construction-piece definitions
- ANCHOR_* for semantic anchor-feature IDs
- CROP_* and ANIMAL_* for biological definitions
- REGION_* and DISCOVERY_* for region/discovery definitions
- OP_* for operation kinds
- EVT_* for committed event types
- ANIM_*, VFX_*, SFX_*, MUS_* for animation and feedback events

The registry ID M001–M119 is separate from item/recipe/event IDs. Do not overload one namespace for all entities.

## 4. Operation transaction contract

Every world-changing operation follows these steps:

1. Validate the request ID/type and required target/definition IDs.
2. Resolve the authoritative domain owner and confirm the object is live.
3. Validate expected revisions, range, permitted context, tool/material/capacity requirements, and domain-specific state transitions.
4. Calculate the proposed change without mutating authoritative state.
5. Reserve any required destinations/anchors/outputs, with a clear rollback path.
6. Commit the state transition exactly once.
7. Return a structured result with a stable machine-readable code, operation ID, and affected object IDs only for a committed success.
8. Publish committed events after commit. A UI, animation, VFX or audio listener cannot be allowed to change whether the operation itself succeeded.
9. On failure, leave inputs, ownership, anchors, graph edges, terrain state, and output capacity unchanged unless the design specifically declares an explicit cost.

Retries with the same operation ID and same request may return the prior immutable result. Reusing that ID with different inputs must fail. The existing WorldOperationLedger is world-local/in-memory by its own header; durable retry guarantees remain a separate decision.

## 5. Shared result and event contracts

The source candidate in GameplayCoreTypes.h contains structured result codes including Success, InvalidTarget, OutOfRange, MissingTool, MissingMaterial, InvalidState, PrerequisiteNotMet, BlockedByPlacement, UnsupportedSurface, CapacityExceeded, NotPermitted, MissingConfiguration, InsufficientResources, Conflict, Unsupported and InternalError.

Rules:
- UI strings are explanatory text, not identifiers.
- Success must not be expressible as a failure.
- A failed result has no changed-object list.
- Events represent committed facts, not previews or intentions.
- Event delivery is notification, not persistence. A missing listener cannot roll back a committed operation.
- Add a new result/event code only when existing semantics cannot describe the case; keep codes stable once they enter save/test contracts.

## 6. Item ownership vs inventory stacks — integration blocker

Two models exist on candidate branches:
- WorldItemOwnershipSubsystem represents an item record with a GUID, definition ID, owner kind/owner ID, quantity, revision and transfer receipts.
- InventoryCraftingComponent represents abstract stacks with FName ItemId and Quantity.

These may coexist only if the unit-of-ownership distinction is explicit:
- Unique physical pieces (long logs, boulders, placed beams, tool instances if durability is instance-specific) need one stable instance identity and exactly one owner/location.
- Abstract stack quantities (sticks, fibers, seeds, gravel) may be represented as stacks, but every transfer must reconcile against the authoritative inventory/ownership owner.
- Do not count the same quantity both as a world-owned record and an inventory stack.
- A world actor picked up into a stack must have a documented conversion/split rule; dropping it must reconstruct the appropriate world representation.
- Held, in-transit, storage, inventory, consumed and transformed states must be exclusive, and transitions must be atomic.

**Required before implementation:** write the selected ownership model and mapping rules into a decision record, then test add/remove/transfer/drop/consume/craft failure paths. The save coordinator currently refuses a save when an item has Held ownership; this is an intentional loss-prevention guard until carry state is part of the persistence manifest.

## 7. Processing state and geometry

A processing action is a valid transition graph, not a cosmetic mesh swap:
- Whole log → longitudinal split/hew/square only where supported.
- Split half-round → quarter/groove/trough/plank only if that parent state allows it.
- Half-length log and split half-round are different concepts.
- A successful state change must select valid output geometry before commit and preserve the documented transform convention.
- Failed transitions do not change PieceType, mesh, occupancy, child links, ownership, or durability.
- Mesh absence is a clear MissingConfiguration failure, not a false success.
- Geometry and gameplay state must survive save/load together.

Initially use author-authored geometry variants. Procedural cutting is not a prerequisite for the common data contract.

## 8. Structural connection contract

- The shared graph owns cross-domain support/connection relationships only when its integration with log, stone and stick actors is explicit.
- Each connection uses stable semantic anchor IDs, not array offsets.
- Source and receiving anchors are both revalidated at commit; receiving anchors must be free.
- Preview and commit must use the same resolved transform/connection solution.
- Reparenting cycles, duplicate edges and stale node IDs are rejected.
- A failed physical attachment must not leave anchor occupancy or graph edges reserved.
- Removing/damaging a support must either resolve dependent structures in a deterministic order or reject without partial mutation.
- Structural graph integration is a source candidate on the broad H branch, not a verified live integration on main.

## 9. Terrain contract and voxel boundary

WorldTerrainStateSubsystem on the H branch models bounded cell edits with operation IDs, expected revisions, allowed depth layers, maximum cell count and snapshot export/restore. Its source comment explicitly leaves rendering/collision edits to an adapter.

Required contract:
- Terrain state stores the authoritative logical edit; the chosen terrain technology renders it and updates collision/navigation.
- Every edit is bounded by region/depth/material policy and maximum work.
- A dig action validates tool, hardness and terrain permission before applying the edit.
- Resource yield is derived from a committed edit and cannot be granted twice on retries.
- Support validation is connected to the structural graph through explicit requests/events.
- Save/load restores the logical terrain state before rebuilding dependent collision/navigation and structures.
- A state cell array is not, by itself, a voxel terrain implementation. The runtime renderer/collision adapter and UE 5.8 compatibility gate remain open.

## 10. Simulation time contract

- Clock stores schema version, logical simulation seconds, tick index, and random seed.
- Clock does not advance itself every render frame. An explicit fixed-step driver calls the coordinator.
- Domains preflight a step, then advance exactly once for a committed logical step.
- Pause, failed step and save/load must not duplicate crop growth, hunger/spoilage, production outputs, raid scheduling or damage.
- Daily weather, seasonal calendar, technical progression and long-term regional restoration remain separate time scales, even if they reference the same logical clock.
- The reviewed coordinator API lists multiple domain getters; inspect its actual implementation before claiming every domain is wired into AdvanceFixedStep.

## 11. Save/load and migration contract

- SaveGame schema version is explicit; current H-branch candidate uses schema version 4.
- Save/load is an orchestration boundary. Each owner validates its own snapshot and must reject invalid references/duplicate IDs/capacity overflow.
- Restore order is deterministic: logical clock/header, terrain/world state, identities/physical actors, structural links, items/inventories/storage, biological state, production/cargo, threat, survival/discoveries, then derived collision/navigation/UI.
- Exact ordering must be checked against Unreal actor lifecycle and actual serialization.
- A schema-version mismatch is rejected by the reviewed code; no completed migration chain was established. Older saves therefore must not be called compatible until explicit migrations exist.
- Save refusal is preferable to a lossy save; every refusal must explain what the player must do (for example, a held item).
- A failed partial restore must roll back to a captured pre-load snapshot or stop gameplay and report a diagnosable fatal consistency failure.

## 12. Data definitions and minimum fields

### ItemDefinition
ID, version, category, world representation, stack policy, mass/volume units, durability or consumable model, base/processed definitions, allowed actions, source definitions, recipe references, persistence policy, UI/feedback event IDs.

### RecipeDefinition
ID/version, inputs and quantities, preconditions, tools/stations, duration, interruptions, outputs/by-products, fuel/water/tool cost, noise/light/risk, rollback policy and completion feedback.

### BuildPartDefinition
ID/version, material family, piece state, dimensions/axis/pivot convention, collision, anchor profile, structural properties, repair/dismantle behavior and mesh-variant references.

### MaterialDefinition
ID, hardness, density/mass units where used, diggability, fracture stages, moisture/contamination responses, extraction yield and compatible process families.

### Crop/AnimalDefinition
ID/version, season windows, growth/care stages, time-step rules, resource costs, outputs/by-products, failure/dormancy, and save migration defaults.

### EventDefinition
Stable ID, owning gameplay event, trigger-after-commit rule, spatial/2D routing, priority/concurrency, parameters, fallback and accessibility controls.

## 13. Required implementation decisions still open

1. Which exact source branch becomes the integration head for the current project.
2. Single ownership model for unique physical items vs abstract item stacks.
3. How inventory component snapshots and WorldItemOwnership records reconcile without duplicate authority.
4. How current SaveGame schema v4 evolves and migrates older versions.
5. How the save coordinator treats held log actors/carry preview state.
6. How the structural graph registers existing pieces and owns connections without duplicating anchor state.
7. Which UE 5.8 runtime terrain renderer/collision adapter backs the logical terrain state.
8. Which domains actually advance inside WorldSimulationCoordinatorSubsystem::AdvanceFixedStep.
9. Which crop harvest, animal reproduction, weather/season and restoration behaviors are truly implemented versus only represented as fields.

## 14. Phase 01 acceptance gate

- [x] Existing phase-01 M001–M119 registry found and counted; no duplicate registry created.
- [x] Core source candidates and domain ownership boundaries identified.
- [x] Item/stack ownership conflict explicitly recorded as an integration blocker.
- [x] Operation, state transition, event, terrain, simulation and save/load contracts documented.
- [x] Runtime, rendering, and persistence gaps separated from mere source presence.
- [ ] Select a source integration head and reconcile branch diffs.
- [ ] Validate JSON/CSV registry consistency and schema.
- [ ] Make a detailed M001–M119 source-evidence mapping.
- [ ] Compile the selected integration branch with Unreal Engine 5.8.
- [ ] Execute relevant Editor/PIE tests and record the logs/screenshots.

Phase 01 design contract is drafted; it is not marked engine-verified or merge-ready.
