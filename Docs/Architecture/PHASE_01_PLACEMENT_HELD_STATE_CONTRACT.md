# Phase 1 — Placement and Held-State Contract

Status: **source-derived contract; not Unreal runtime validation**  
Repository: `furkansarikiyak36-eng/myproject3`  
Branch: `phase-01-foundation-v2`

## Purpose

Record the current lifecycle semantics before changing the public Blueprint-callable state API. This is deliberately a compatibility document, not permission to refactor callers whose Blueprint usage is unavailable in the tracked tree.

## State meanings

| Field / API | Current meaning observed in source | Compatibility note |
|---|---|---|
| `bStartsPlaced` | Authored startup default read by `BeginPlay` to initialize `bIsPlaced` | Despite its name, it is also mutated by `SetPlacedState`. Do not assume it is startup-only in existing projects until Blueprint and local level consumers are checked. |
| `bIsPlaced` / `IsPlaced()` | Transient runtime placed flag | Initialized from `bStartsPlaced` in `BeginPlay`; changed by `SetPlacedState`. |
| `bIsHeld` / `IsHeld()` | Transient runtime held flag | Changed by `SetHeldState`. |
| `SetHeldState(true)` | Disables simulation and actor collision | Does not itself change `bIsPlaced`. Callers must currently coordinate both state setters. |
| `SetHeldState(false)` | Re-enables actor collision and component query/physics collision | Does not itself decide whether simulation should be active. |
| `SetPlacedState(true)` | Marks placed, disables simulation unless held, applies `BlockAll`, enables collision unless held | Also sets `bStartsPlaced=true`. |
| `SetPlacedState(false)` | Marks unplaced, enables simulation unless held, applies `PhysicsActor`, enables collision unless held | Also sets `bStartsPlaced=false`. |

## Observed caller sequences

1. **Begin play:** `bIsPlaced = bStartsPlaced`; mesh simulation and collision profile are initialized from the resulting placed flag.
2. **Pickup:** detach from snap parent → `SetHeldState(true)` → `SetPlacedState(false)`.
3. **Drop:** `SetHeldState(false)` → `SetPlacedState(false)`.
4. **Free placement:** `SetHeldState(false)` → `SetPlacedState(true)`.
5. **Successful snap:** reserve anchors → attach to parent (which calls `SetPlacedState(true)`) → `SetHeldState(false)` → `SetPlacedState(true)`.

These call sequences mean that placement is applied more than once on the successful snap path, and the order of held/placed calls currently matters for physics and collision.

## Safe invariants for future implementation

- Startup configuration and runtime state should have distinct documented meanings.
- A carried piece should have held=true, simulation disabled, and collision disabled.
- A loose dropped piece should have held=false, placed=false, simulation enabled, and collision enabled.
- A free-placed or snapped piece should have held=false, placed=true, simulation disabled, and collision enabled.
- A snapped child must also have a valid explicit parent/anchor relationship; transform overlap alone is not attachment.
- A failed placement operation must leave the held piece and anchor reservations in a coherent pre-operation state.
- Public Blueprint-callable methods must remain compatible until their consumers are inventoried.

## Why the apparent startup/runtime bug is not changed here

The tracked test-map script sets `starts_placed` on spawned pieces and also calls `set_placed_state(True)` for placed supports. The current implementation therefore keeps both values aligned. Removing the assignment to `bStartsPlaced` from `SetPlacedState` could be conceptually cleaner, but may change editor-time or Blueprint workflows that rely on that behavior. No tracked `.uasset` or `.umap` was present in the branch tree scan, so those consumers are not fully auditable here.

## Required follow-up before refactor

1. Inspect the actual local project's Blueprint graphs and level-authored defaults for `SetPlacedState`, `SetHeldState`, `starts_placed`, and `IsPlaced`.
2. Decide whether `SetPlacedState` is a runtime-only API or intentionally also edits the startup default.
3. If compatibility permits, introduce one internal function that applies physics/collision from the combined held/placed state, while retaining the existing public methods as compatible wrappers.
4. Add focused automated/runtime coverage for pickup, drop, free placement, snap placement, child detachment, and BeginPlay initialization.
5. Run Unreal Engine 5.8 build/editor checks only at the planned validation stage.

## Evidence boundary

This contract is derived from tracked `LogBuildPiece.h/.cpp`, `LogCarryComponent.cpp`, and `UnrealEditorScripts/CreateLogSnapTestLevel.py`. It does not establish Blueprint call coverage, successful compilation, physics correctness, or gameplay behavior.
