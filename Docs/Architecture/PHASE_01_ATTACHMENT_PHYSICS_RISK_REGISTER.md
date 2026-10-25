# Phase 1 — Attachment, Placement and Physics Risk Register

Status: **static source review only; not a runtime validation report**  
Repository: `furkansarikiyak36-eng/myproject3`  
Branch: `phase-01-foundation-v2`

## Scope inspected

- `ALogBuildPiece::CanBePickedUp`
- `ALogBuildPiece::SetHeldState` and `SetPlacedState`
- `ALogBuildPiece::CanSnapPointAccept` and `TryReserveSnapPoints`
- `ALogBuildPiece::AttachToSnapParent`, `DetachFromSnapParent`, child registration
- `ULogCarryComponent::TryPickupTarget`, `UpdateHeldPreview`, `FindBestSnapCandidate`, `PlaceHeldPiece`, and `DropHeldPiece`

No Unreal build, editor session, physics simulation, or gameplay test was run.

## Expected state transitions observed in source

| Action | Intended state after call | Source-level observation |
|---|---|---|
| Pickup a loose piece | Held=true, placed=false, physics off, collision off | Carry component detaches from parent if any, then calls `SetHeldState(true)` and `SetPlacedState(false)`. |
| Drop held piece | Held=false, placed=false, physics on | Calls `SetHeldState(false)`, then `SetPlacedState(false)`; the latter enables simulation for a non-held loose piece. |
| Free-place on a valid surface | Held=false, placed=true, physics off | Requires free placement enabled and, when configured, a non-build-piece placement surface. |
| Snap to a placed host | Parent/child relation set, both anchors occupied, placed=true | Target validates compatibility before reserving both anchors and attaching the child. |
| Pick up an attached child | Parent anchor released and child unregistered | `DetachFromSnapParent` releases both sides of the relationship before the piece becomes held. |
| Pick up a support with attached children | Rejected | `CanBePickedUp` requires zero valid attached children. |

These are source-path observations only. Collision profiles, attachment transforms, settling, and frame-to-frame preview behavior remain unverified in Unreal.

## Risks and required follow-up

### R1 — Custom snap-point IDs can collide with the automatic namespace

**Evidence:** `RegenerateAutomaticSnapPoints` removes points whose IDs start with `AutoEnd_` or `AutoSide_`. The source now rejects `NAME_None`, either reserved prefix (case-insensitive), and duplicate IDs in `AddCustomSnapPoint`; rejected calls emit a `LogTemp` warning and do not append a point.

**Impact:** The known silent-removal and duplicate-identifier paths are guarded at the source API. Because `AddCustomSnapPoint` is `void`, callers do not receive a structured success/failure result; they must use the warning/log or inspect the resulting point list. Blueprint/editor behavior remains unverified.

**Follow-up:** Keep the public function signature compatible for now. Add a structured validation/result API only after caller review, and add focused checks for None, reserved prefixes, duplicates against both custom and generated IDs, and valid unique IDs. This source change is not runtime-tested.

### R2 — Snap reservation must remain atomic if actor attachment fails

**Evidence:** Previously, `TryReserveSnapPoints` marked both anchors occupied before calling a void attachment helper, and the helper ignored the boolean result of `AttachToActor`. A failed engine attachment could therefore leave the structural anchor flags inconsistent with the actual actor hierarchy.

**Impact:** The source path now calls a result-returning `AttachToSnapParent` before occupying either anchor. The helper validates its inputs, attempts `AttachToActor`, and only after success writes parent/index state, registers the child, and marks the piece placed. If attachment fails, `TryReserveSnapPoints` returns false without reserving either anchor. The target-chain guard also rejects already-corrupted cycles rather than traversing indefinitely.

**Follow-up:** This is a source-level transactional safeguard, not proof that every engine attachment scenario succeeds. Add focused Unreal checks for failed attachment, valid attachment, occupancy rollback, and cleanup after detach. No build or runtime test has been run.

**Evidence:** `TryReserveSnapPoints` validates first, then marks target/source occupied and calls `AttachToSnapParent`. The current attachment helper has no result value and no rollback contract.

**Impact:** The current synchronous path has no obvious failure return after reservation, but future attachment rules, collision validation, or structural constraints could introduce a failure after mutation. That would violate the shared all-or-nothing operation contract.

**Follow-up:** Keep validation before mutation. When attachment can fail, make the operation return a result and either perform a guaranteed commit step or roll back both occupancy flags and any partially registered relationship. Do not add a speculative global transaction framework in this phase.


### R8 — Direct snap calls could reparent an attached piece or create a cycle

**Evidence:** Before the new guard, `CanSnapPointAccept` did not reject a source piece that already had a parent, still had children, or would be attached beneath one of its own descendants. `AttachToSnapParent` overwrites the stored parent fields; reparenting without detaching can leave the former parent's anchor occupied and its child registration stale.

**Impact:** A direct/future caller of `TryReserveSnapPoints` could break the parent/child graph even though the carry pickup path normally detaches pieces first. A cycle could also make the structural graph invalid.

**Follow-up:** Source guard added: reject existing parent linkage/indices, reject a source with attached children, and reject targets whose parent chain contains the source piece. Add focused Unreal checks for each rejection and verify that valid detached-child placement still works. No build or runtime test has been run.

### R7 — Receiver anchor was not independently revalidated by the acceptance function

**Evidence:** `CanSnapPointAccept` now explicitly requires the target anchor to have `bCanReceive=true` and `bOccupied=false`. Before this guard, the carry component's candidate search checked receiver availability, but the acceptance/reservation API itself did not independently enforce both target conditions.

**Impact:** Direct or future callers of `TryReserveSnapPoints` could otherwise attempt to reserve an occupied or source-only target anchor if they bypassed the carry component's candidate search.

**Follow-up:** Source guard added. Add focused Unreal checks for occupied target, source-only target, occupied source, incompatible tag/type, and a valid receiver. No build or runtime test has been run.

### R3 — Runtime placement mutates the startup-default property

**Evidence:** `SetPlacedState` writes both `bIsPlaced` and `bStartsPlaced`. `BeginPlay` initializes `bIsPlaced` from `bStartsPlaced`.

**Impact:** The property named as a startup default is also changed by gameplay placement. This may be intentional for the prototype, but conflates authored initialization with runtime state and could surprise construction/editor workflows or future save/load code.

**Follow-up:** Recorded the source-derived semantics and compatibility gate in `PHASE_01_PLACEMENT_HELD_STATE_CONTRACT.md`. The tracked test-map script sets both `starts_placed` and `set_placed_state(True)` for supports. Do not remove the runtime write until local/untracked Blueprint and level consumers are inventoried.

### R4 — Physics/collision state depends on call order

**Evidence:** `SetHeldState(false)` enables actor/component collision but does not itself enable physics. `SetPlacedState` determines simulation from `!bNewPlaced && !bIsHeld`. Current pickup/drop/place callers invoke both methods in specific orders.

**Impact:** A future caller that invokes only one setter can leave the piece in a mixed state (for example, loose but not simulating, or placed with unexpected collision settings).

**Follow-up:** The source-derived expected states and current setter semantics are recorded in `PHASE_01_PLACEMENT_HELD_STATE_CONTRACT.md`. If compatibility permits after Blueprint review, introduce one internal state-application helper while retaining public Blueprint-callable methods as wrappers. Runtime verify pickup, drop, free placement, snap placement, child detachment, and BeginPlay initialization.

### R5 — Free-placement fallback after a stale snap candidate needs a targeted scenario

**Evidence:** The original `PlaceHeldPiece` path continued into free placement after `TryReserveSnapPoints` returned false. In this branch, the path now restores the pre-attempt actor transform, clears the snap candidate, and returns false after an explicit snap reservation failure.

**Impact:** The source-level fallback hole is guarded: a failed explicit snap attempt no longer becomes a free placement in the same call. The actual stale-candidate scenario and the preview's next-frame recovery remain unverified.

**Follow-up:** Keep the code change; add a runtime scenario where a target anchor becomes occupied or invalid between preview and placement. Confirm the held piece remains held, no anchors remain partially reserved, and the user can aim at a valid free surface afterward.

### R6 — Geometry changes and attachment anchors need per-variant validation

**Evidence:** Automatic anchors are rebuilt from replacement mesh bounds; custom anchors are preserved verbatim. `ApplyCutAction` currently rejects placed pieces and pieces with attached children, which limits but does not eliminate future variant/configuration mismatch.

**Impact:** A preserved custom transform may not match a new mesh's connection face; bounds-derived automatic anchors may also be unsuitable for irregular geometry.

**Follow-up:** Document which mesh variants support which anchors and verify geometry in the final Unreal demo. Do not claim the geometry is correct from source inspection alone.

## Current safety conclusions

- Explicit parent/child links, not visual overlap, govern the current construction relationship.
- A receiving host must already be placed and not held.
- A piece with attached children cannot currently be picked up or processed.
- The current cut-action path checks required variant assets before changing piece type.
- No evidence from this review establishes structural load capacity, terrain anchoring, durable persistence, or multiplayer safety.

## Exit evidence still required

- Tracked C++ and editor-script call-site map (`PHASE_01_STATE_TRANSITION_CALLER_MAP.md`); local/untracked Blueprint and level consumers still need inspection.
- Authoritative 119-mechanic list and ownership/dependency map.
- Focused automated checks where available.
- Unreal Engine 5.8 build and actual Editor/runtime scenarios at the planned final validation stage.
