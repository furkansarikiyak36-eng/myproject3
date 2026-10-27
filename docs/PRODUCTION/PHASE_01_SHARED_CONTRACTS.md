# Phase 01 — Shared Gameplay Contracts

**Status:** Design baseline drafted; it does not certify the existing C++ source as compiled or integrated.  
**Authority:** Existing M001–M119 registry IDs remain canonical.  
**Implementation constraint:** Use current domain owners and existing Phase 02 contract types where appropriate; do not create a parallel owner on the planning branch.

## 1. Contract layers

### A. Definition identity vs runtime instance

A definition explains what a thing is; an instance records what happened to one particular thing.

- `DefinitionId`: stable ID such as `MAT_WOOD_LOG`, `ITEM_HAND_AXE`, `BUILD_WOOD_BEAM`, `RECIPE_SPLIT_LOG`.
- `InstanceId`: stable GUID/handle for one persistent physical object or uniquely tracked runtime entity.
- `DisplayNameKey`: localizable UI label; never a save identity.
- `Revision`: monotonically increasing revision for optimistic validation/idempotent operations where needed.
- `DomainState`: domain-owned state. A wood-processing enum is not also a placement, durability and inventory enum.

Do not silently assign new instance IDs every time an object is picked up, placed, split or saved. When processing transforms one piece into several pieces, define the parent-to-output identity policy explicitly and record it in the committed operation.

### B. Operation request and result

The source branch `phase-02-core-contracts` already contains candidate `FWorldOperationRequest`, `FWorldOperationResult`, and `EWorldOperationResultCode`. The plan is to review/reuse those types, not create a second global request system.

An operation request should be a description of intent and preconditions, not permission for the caller to mutate another domain's state. A result must distinguish machine-readable code from presentation text and list affected stable object IDs on success.

Minimum conceptual fields:

| Field | Meaning |
|---|---|
| OperationId | Stable operation/request ID for correlation and retry detection |
| OperationType | Stable name for the action being attempted |
| RequesterId | Optional actor/player/system making the request |
| TargetIds | Stable IDs of target objects |
| DefinitionId / ToolId / RecipeId | Optional source/tool/recipe definitions required by the domain |
| ExpectedRevision | Optional revision read by the caller to reject stale commits |
| Context | Optional placement/action parameters, validated by the domain owner |
| ResultCode | Stable enum; success and failure cannot be confused |
| Message | Human-readable diagnostic, not a machine key |
| AffectedObjectIds | Committed changes only; empty on failure |

Minimum stable failure meanings: invalid target, out of range, missing tool, missing material, invalid state, prerequisite not met, blocked by placement, unsupported surface, capacity exceeded, not permitted, missing configuration, insufficient resources, conflict/stale revision, unsupported operation, internal failure.

Domain-specific failure codes can be added after review; do not rename existing C++ enum values without migration/compatibility review.

### C. Domain ownership

- `ALogBuildPiece` remains current authority for the existing log prototype's piece type, processing prerequisites, snap points, occupancy and parent/child link behavior until callers and Blueprint references are audited.
- `ULogCarryComponent` remains current player-facing carry/preview/input owner.
- Stone, stick, inventory, terrain, agriculture, threat and storage domains own their respective states in the staged implementation.
- A shared identity registry, event dispatcher or ledger may identify/record state but must not become a duplicate live owner.
- Callers request a change; the owner validates preconditions and commits it.
- No direct write to another subsystem's internal arrays as a shortcut.

### D. Atomic state-changing operation

Every state-changing operation uses this sequence:

1. Resolve requester, target, operation and stable definitions.
2. Validate validity, range, permissions, revisions, available tool/material/capacity and domain-specific state prerequisites.
3. Calculate a proposed result without mutating authoritative state.
4. Preflight output/space/attachment/assets and all required cross-domain effects.
5. Commit once; update the owning domain and revisions.
6. Emit a committed event after state change succeeds.
7. Return a stable result. On failure, restore/no-op so no partial mutation remains.

This applies to pickup/drop, cutting/splitting, snap placement, inventory transfer, crafting, excavation, repair, production, harvesting and save restoration.

### E. Committed events

Events describe facts that have already happened, not previews or requests. Audio/VFX/UI subscribe to events or state changes and are not authority.

Every committed event should carry stable event/schema version, OperationId, event type, affected instance IDs, owner/domain, resulting revision(s), and the minimum payload required by listeners. Do not persist arbitrary actor pointers or display strings as identity.

The candidate `WorldCommittedEventSubsystem` is explicitly transient in Phase 02 documentation. A transient multicast event is not a durable event log and does not by itself implement save/load.

### F. Item/resource amounts and recipes

The Phase 02 candidate resource quantity contract requires a definition ID and unit ID, rejects negative/non-finite quantities, and keeps sufficiency checks read-only. The Phase 06 candidate inventory component already has stack/recipe structs, slot and weight capacity checks and rollback-oriented crafting logic. These are candidate implementations to review, not a reason to duplicate them in Phase 01.

Minimum item definition:
- DefinitionId and category.
- World representation: physical actor, stackable pickup, equipment-only, or inspectable record.
- Explicit quantity unit, unit mass and (where relevant) volume.
- Max stack, durability/charges/freshness/filter-life model.
- Domain-specific state model, if it affects gameplay.
- Source(s), permitted actions, recipe references and persistence policy.
- Feedback event IDs and current status.

Minimum recipe definition:
- RecipeId and version.
- Inputs and quantities; consumed vs returned/tool inputs.
- Preconditions: tool, station, terrain, piece state, season or exposure conditions.
- Duration and interruption behavior.
- Outputs and by-products.
- Costs: tool wear, fuel, water, time, noise/light or risk as applicable.
- Atomic failure/rollback behavior.
- UI/animation/VFX/SFX event references.
- Save policy for any in-progress operation.

Large logs and boulders remain physical actors unless a deliberate design decision documents otherwise. Small resources may use stacks; physical world object and inventory stack ownership cannot overlap.

### G. Construction and material state

- Use one placement kernel. Material-specific providers describe anchors/compatibility; profiles define dimensions/spacing.
- Preview and commit must consume the same solved placement, not independently recompute different results.
- Anchor IDs are semantic feature IDs, not unstable array indices.
- A successful connection commits parent/child links and anchor occupancy together. Failed attachment changes neither.
- Processing state determines valid output geometry. Whole log, half-length log and split half-round are distinct.
- Geometry, placement, durability/damage and ownership are related but separate state dimensions.
- Parent/support removal must have an explicit policy for children: reject, detach, or safely resolve load/collapse; never leave silent dangling relationships.

### H. Terrain, time, survival, and restoration

- Terrain state is bounded and data-driven. Terrain edits must define cell/material identity, depth/layer constraints, revision, persistence and resource-yield policy.
- Visual mesh/collision updates are derived representations and cannot replace the authoritative terrain state.
- The staged `WorldTerrainStateSubsystem` is not proof of a selected voxel rendering/meshing engine or of save/load integration.
- Use separate time scales for weather, seasonal growth, threat scheduling, and long-term regional restoration.
- Region restoration is coarse state (contamination/soil/water/vegetation/opportunities) rather than one global actor simulation per plant.
- Exposure warning and countermeasures must be communicated. Do not use planting as an instant solution to globally toxic air.

## 2. Save/load contract

The staged Phase 02 closeout explicitly states that its event dispatcher is transient and durable event-log/replay, cross-session identity and migration are not implemented there. The later integration plan proposes a save coordinator, but that also requires validation.

Required logical restore order, subject to Unreal serialization testing:

1. Save header: format version, world/project version, world seed and simulation clock.
2. Stable instance/definition identity records.
3. Terrain/world-state deltas and region/environment state.
4. Physical actors and each domain's authoritative local state.
5. Structural parent/child links, anchor occupancy and support dependencies.
6. Inventory, storage and exclusive item ownership.
7. Crop/animal, production queues, fuel/heat and threat schedule.
8. Player survival/equipment and exploration/discovery state.
9. Rebuild derived collision/navigation/visual state; validate references before enabling gameplay.

Every saved domain needs: schema version, stable IDs, explicit owner, validation on load, migration/fallback policy, corruption/error handling and a test fixture. Actor names, object-array indices and mesh paths are not sufficient as persistent identity.

## 3. Required ID namespaces

| Prefix | Use | Example |
|---|---|---|
| M | Canonical mechanics | M001 |
| SYS | Logical system | SYS_WOOD |
| DEF | General definition | DEF_OLD_RECORDER |
| MAT | Material/resource definition | MAT_WOOD_LOG |
| ITEM | Inventory item | ITEM_REPLACEABLE_FILTER |
| RECIPE | Transformation/recipe | RECIPE_SPLIT_LOG |
| BUILD | Build-piece definition | BUILD_WOOD_SUPPORT |
| PROC | Processing action/state transition | PROC_WOOD_GROOVE |
| REG | Region | REG_ASH_FOREST |
| CROP | Crop definition | CROP_ROOT_VEGETABLE_A |
| OP | Operation type | OP_PLACE_PIECE |
| EVT | Committed domain event | EVT_CONSTRUCTION_ATTACHED |
| VFX | Visual feedback event | VFX_STONE_CRACK |
| SFX | Sound effect event | SFX_RAID_WARNING |
| MUS | Music/adaptive audio cue | MUS_WORLD_ABANDONED |
| TEST | Reproducible acceptance case | TEST_M024_VALID_SNAP |

No IDs should be generated by runtime display strings. Namespaces are proposed for catalogue consistency; use them as definition keys only after the source/data format is approved.

## 4. Review blockers before source integration

- Confirm the candidate stacked branch head and resolve its 3-commit lag to current main without rewriting history.
- Review `GameplayCoreTypes.*` and existing public enum/struct contracts.
- Inspect whether the world identity subsystem has a stable policy for actors spawned before/after BeginPlay, duplicate IDs, destruction and re-registration.
- Review request validation and idempotency: request ID alone is not enough unless duplicate commit behavior is specified.
- Identify which event payload fields can be serialized and which events need durable receipts vs transient notification.
- Confirm that resource unit/mass/volume data has one owner rather than several maps with conflicting values.
- Confirm how inventory item IDs link to physical world actors and split outputs.
- Keep all current C++ API signatures and Blueprint-facing behavior stable until tracked and local Blueprint consumers have been reviewed.

## 5. Phase 01 exit gate

Phase 01 is ready for implementation only when the registry, source-owner map, definitions, operation/result contracts, event contract, ownership rules, state-transition tables, save plan and acceptance cases refer to the same IDs and owners.

Phase 01 is not fully runtime-verified until the candidate combined source branch compiles in Unreal Engine 5.8 and selected end-to-end paths pass Editor/PIE and save/load scenarios.
