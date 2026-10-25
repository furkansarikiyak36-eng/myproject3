# Phase 1 — Log Interaction Call-Chain Review

Status: **static review only; no Unreal build or runtime test**  
Baseline: `main` at `ff2fd51444463afad598b837e820dbc3df2d439b`  
Working branch: `phase-01-foundation-v2`

## Observed call paths

### Input → interaction

- `ALogSnapCharacter::SetupPlayerInputComponent` binds movement/look/jump keys and delegates log controls to `ULogCarryComponent::BindDefaultInput`.
- `ULogCarryComponent::Interact` branches on whether `HeldPiece` is valid: if held, it calls `PlaceHeldPiece`; otherwise it calls `TryPickupTarget`.
- `TryPickupTarget` traces within `PickupRange`, requires an `ALogBuildPiece` that passes `CanBePickedUp`, detaches it from its parent if attached, switches held/placed flags, and clears preview/snap state.
- `DropHeldPiece` moves the actor in front of the view, clears held state and keeps it unplaced. The actual physical settling behavior still needs runtime verification.

### Input → processing

- `CycleCutAction` selects one of the existing enum actions.
- `ProcessAimedPiece` refuses to process while carrying a piece, traces within `PickupRange`, requires an `ALogBuildPiece`, then calls `ApplyCutAction`.
- `ALogBuildPiece::CanApplyCutAction` requires the piece not be placed, held, or have attached children. It also enforces the current type transition and requires the corresponding variant mesh.
- The current transitions are:
  - `WholeLog → SplitHalfLog`
  - `WholeLog → HewnLog`
  - `WholeLog → SquaredBeam`
  - `SplitHalfLog → QuarterLog`
  - `SplitHalfLog → GutterLog`
  - `SplitHalfLog → TroughLog`
  - `SplitHalfLog → Plank`
- `ApplyCutAction` selects the configured mesh, updates the piece type and offsets the actor to preserve the old bottom Z. No physical tool, cut progress, hit location, or durable resource consumption is represented in this path yet.

### Held piece → snap/free placement

- `TickComponent` updates the held preview and periodically calls `FindBestSnapCandidate`.
- `FindBestSnapCandidate` requires the crosshair trace to hit an already placed, non-held `ALogBuildPiece`. It evaluates compatible/free source and receiving anchors and computes the held actor transform from the anchor transforms.
- `PlaceHeldPiece` first attempts `TryReserveSnapPoints` when a snap candidate exists. On success, both anchors become occupied and the child is attached to the target. Otherwise free placement is allowed only when enabled and, if configured, a valid non-build-piece surface is present.
- `CanSnapPointAccept` checks source/target validity, source capability/occupancy, tag/type compatibility, and that the receiving host is already placed and not held.
- `TryReserveSnapPoints` performs the occupancy mutation and calls `AttachToSnapParent`. `DetachFromSnapParent` releases the parent anchor and removes the child from the parent's attached-piece list.

## Confirmed compatibility constraints

- Keep `ELogPieceType`, `ELogCutAction`, `ELogPlacementMode` and `FLogSnapPoint` as the current public vocabulary until all callers and Blueprint usage have been reconciled.
- Preserve the distinction between split-half geometry and half-length geometry; the prototype comments explicitly treat them as different states.
- Keep the requirement that the receiving host be placed before snapped construction can attach.
- Do not infer construction integrity from mesh overlap. The current code tracks explicit parent/child links, but does not yet model load-bearing strength or structural failure.
- The current module declares `Core`, `CoreUObject`, `Engine`, `InputCore` and `EnhancedInput` as public dependencies. `MyProject3.uproject` targets engine association `5.8`; editor-only plugins are enabled for Modeling Tools, Python Script Plugin and Editor Scripting Utilities.

## Actionable issue

`ALogBuildPiece::SetPieceStaticMesh` calls `GenerateDefaultSnapPoints`, and that helper begins with `SnapPoints.Reset()`. Since `ApplyCutAction` calls `SetPieceStaticMesh`, processing a piece appears to discard custom editor-authored anchors as well as generated anchors. The existing `RegenerateAutomaticSnapPoints` helper is safer because it removes only IDs with `AutoEnd_` and `AutoSide_` prefixes. Recommended fix: route mesh changes through a regeneration function that preserves custom points, while checking whether their transforms remain valid for the replacement mesh. Do not silently preserve stale geometry-specific anchors without an explicit policy.

## Phase 1 exit requirements still open

- Obtain and reconcile the authoritative list of 119 mechanics; this repository review cannot invent that list.
- Decide the custom-anchor policy and implement it without breaking public APIs.
- Inventory the remaining source/config/build/editor-script files and their dependencies.
- Define shared operation result/event/persistence contracts against the existing classes rather than adding duplicate state owners.
- Record static review, automated checks, Unreal build, and runtime results independently.
