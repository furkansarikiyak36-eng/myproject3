# Phase 1 — Shared Operation, Event and Persistence Contracts

Status: **design contract proposal; not yet implemented as C++ API**  
Repository: `furkansarikiyak36-eng/myproject3`  
Working branch: `phase-01-foundation-v2`

## Purpose

Give all 16 phases one predictable way to request a world-changing operation, report why it succeeded or failed, publish changes, and mark durable state for saving. This document does not replace existing log types or introduce a second authoritative world-state store.

## Ownership rules

1. **Domain owner is authoritative.** The existing `ALogBuildPiece` remains the authority for the current prototype's piece type, cut eligibility, snap points, occupied anchors, and parent/child attachment until a later refactor is explicitly approved.
2. **Callers request; owners validate and mutate.** Character input, UI, AI, crafting, raids, and demo scripts must not directly edit another domain's internal state as a shortcut.
3. **One operation, one commit point.** Validate all preconditions and resolve required assets/targets before mutating state. A failed request must not consume resources, reserve anchors, change piece type, or publish a success event.
4. **No generic state shadow.** A registry, event log, or save record may describe authoritative state, but must not become a competing live owner of it.
5. **Cross-domain changes use explicit requests.** Example: a trap asks the terrain owner to validate anchoring; the trap does not write voxel data directly.

## Proposed stable result vocabulary

These are conceptual names for a future shared result type; do not add a duplicate enum if the project already has an equivalent public contract.

| Result | Meaning |
|---|---|
| `Success` | Operation committed; result includes the affected stable object IDs. |
| `InvalidTarget` | Target missing, destroyed, out of range, or wrong domain type. |
| `PreconditionFailed` | Required state/order is not satisfied (for example, groove before split). |
| `MissingConfiguration` | Required mesh, recipe, tool definition, or authored anchor is absent. |
| `Blocked` | World/physics/occupancy rule prevents the operation. |
| `InsufficientResources` | Required materials or durability are unavailable. |
| `Conflict` | Target state changed between validation and commit; re-resolve instead of applying stale results. |
| `Unsupported` | This implementation intentionally does not support the requested operation. |
| `InternalError` | Unexpected failure; must not be reported as success. |

Every failed result should have a stable code plus a user-facing explanation. UI text must not be the sole machine-readable code.

## Operation request/commit envelope

A future common request should carry only the fields needed for deterministic validation:

- operation ID (unique per request, for duplicate detection and diagnostics)
- operation type and requesting actor/system
- target stable ID(s), not only transient pointers where persistence matters
- optional source/recipe/tool IDs
- relevant hit/anchor/location context
- expected target revision/state when stale-request detection is required

The owner resolves and validates the request, computes the intended changes, then commits once. On success it returns affected IDs, changed state/version, and emitted domain events. This is a contract shape, not a demand to route every low-level function through a global dispatcher in Phase 1.

## Event rules

- Events describe a committed fact (`LogProcessed`, `PieceAttached`, `PieceDetached`, later `TerrainChanged`, `CropHarvested`, `RaidStarted`); they do not themselves own the state.
- Publish success events only after the owner has committed the change.
- Include operation ID, event type, stable affected IDs, world time/tick where available, and a schema version when event persistence/replay is introduced.
- Consumers should tolerate duplicate delivery or use operation/event IDs to deduplicate. Do not assume event delivery alone is durable persistence.
- A failed operation may emit a diagnostic result, but never a domain-success event.

## Persistence boundary

Phase 1 does **not** implement a world save system. Before a persistence implementation is selected, map each domain's authoritative state and stable identity.

- Save durable facts: piece identity/type, world transform, placement/attachment relation, processed geometry definition, terrain edits, resource quantities, crop/animal lifecycle, survival progression, raid/world progression.
- Do not treat transient presentation as durable by default: held preview, candidate snap marker, crosshair hit, debug labels, temporary UI state.
- Persist attachment relationships by stable piece/anchor IDs rather than only actor pointers or array indices.
- Version save data and define migrations before a save schema ships. Missing optional data must have an explicit fallback; unknown incompatible versions must fail safely rather than partially applying.
- A committed durable mutation marks the owning domain/chunk dirty. Saving should snapshot a consistent state; it must not observe half of a multi-object attachment or terrain operation.

## Mapping to the existing log prototype

| Concern | Current owner | Phase 1 rule |
|---|---|---|
| Piece type and cut prerequisites | `ALogBuildPiece` | Keep existing API and transition checks; do not duplicate in carry component. |
| Mesh variant selection | `ALogBuildPiece::ApplyCutAction` | Resolve valid configured mesh before changing piece type. |
| Snap compatibility and occupancy | `ALogBuildPiece` | Keep reservation in the owner; validate both anchors before changing either. |
| Held preview / aim trace / input | `ULogCarryComponent` | May request actions; must not become authoritative owner of piece geometry. |
| Parent-child attachment | `ALogBuildPiece` | Treat the relation as explicit construction state, not mere mesh overlap. |
| Durable identity/save format | Not established in reviewed prototype files | Do not invent a parallel save store; design this in the persistence phase after broader inventory. |

## Non-negotiable invariants

- Groove/ trough/ plank operations are blocked until the split-half prerequisite is met.
- Missing mesh variants fail before piece type, transform, anchor occupancy, or resources change.
- Failed placement reserves neither the receiver nor source anchor.
- A receiving anchor requires a valid placed host and must be free and compatible.
- A piece with attached children cannot be processed or picked up until those children are detached.
- World-changing operations are atomic from the player's perspective: either all required changes commit or none do.

## Deliberately deferred

- C++ shared result/event types until the existing public API and callers are fully mapped.
- Durable world save/load, migration, replay, and cross-session identity implementation.
- Multiplayer authority and network replication.
- Final automated test suite, Unreal 5.8 build, editor checks, and integrated demo validation, which remain part of the later end-of-phase integration/test plan.
