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

**Evidence:** `RegenerateAutomaticSnapPoints` removes points whose IDs start with `AutoEnd_` or `AutoSide_`. `AddCustomSnapPoint` accepts any ID without checking that reserved prefix or duplicate IDs.

**Impact:** A custom point authored with one of those prefixes can be removed on the next regeneration. Duplicate IDs can also make editor diagnostics ambiguous.

**Follow-up:** Before changing the public `FLogSnapPoint` contract, decide whether automatic/custom origin should be explicit metadata or whether the API should reject reserved prefixes and duplicate IDs. Preserve existing Blueprint/API compatibility during that decision. Add editor-visible validation and a focused test later.

### R2 — Snap reservation has no explicit rollback path

**Evidence:** `TryReserveSnapPoints` validates first, then marks target/source occupied and calls `AttachToSnapParent`. The current attachment helper has no result value and no rollback contract.

**Impact:** The current synchronous path has no obvious failure return after reservation, but future attachment rules, collision validation, or structural constraints could introduce a failure after mutation. That would violate the shared all-or-nothing operation contract.

**Follow-up:** Keep validation before mutation. When attachment can fail, make the operation return a result and either perform a guaranteed commit step or roll back both occupancy flags and any partially registered relationship. Do not add a speculative global transaction framework in this phase.

### R3 — Runtime placement mutates the startup-default property

**Evidence:** `SetPlacedState` writes both `bIsPlaced` and `bStartsPlaced`. `BeginPlay` initializes `bIsPlaced` from `bStartsPlaced`.

**Impact:** The property named as a startup default is also changed by gameplay placement. This may be intentional for the prototype, but conflates authored initialization with runtime state and could surprise construction/editor workflows or future save/load code.

**Follow-up:** Check all declarations, Blueprint reads/writes, and level-authored defaults before splitting this into an immutable startup setting and runtime placement state. Do not change it until those consumers are inventoried.

### R4 — Physics/collision state depends on call order

**Evidence:** `SetHeldState(false)` enables actor/component collision but does not itself enable physics. `SetPlacedState` determines simulation from `!bNewPlaced && !bIsHeld`. Current pickup/drop/place callers invoke both methods in specific orders.

**Impact:** A future caller that invokes only one setter can leave the piece in a mixed state (for example, loose but not simulating, or placed with unexpected collision settings).

**Follow-up:** Map all call sites and Blueprint-callable usage. Consider a single state-transition owner that applies flags, physics, and collision together, but preserve the current API until callers are reconciled. Runtime verify pickup, drop, free placement, snap placement, and child detachment.

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
