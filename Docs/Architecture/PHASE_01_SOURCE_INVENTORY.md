# Phase 1 — Existing Source Inventory and Compatibility Notes

Status: **initial static source inventory; not a build or runtime verification**  
Repository: `furkansarikiyak36-eng/myproject3`  
Baseline: `main` at `ff2fd51444463afad598b837e820dbc3df2d439b` when this inventory was prepared  
Working branch: `phase-01-foundation-v2`

## Current source files relevant to the first playable mechanic loop

| File | Current responsibility observed in source |
|---|---|
| `LogBuildPiece.h/.cpp` | `ALogBuildPiece`: log/piece type, mesh variants, cut-action prerequisites, snap-point generation, occupied anchors, parent/child attachment, placed/held flags and physics/collision transitions. |
| `LogBuildingTypes.h` | Shared enums and `FLogSnapPoint` data for piece types, axis, placement mode, cut actions, snap tags, accepted piece types and occupancy. |
| `LogCarryComponent.h/.cpp` | `ULogCarryComponent`: view trace, pickup/drop, held preview, free placement, snap candidate search, placement mode/rotation and input bindings for processing/placement. |
| `LogSnapCharacter.h/.cpp` | Character/input integration for the log interaction prototype. |
| `LogSnapGameMode.h/.cpp` | Minimal game-mode class for the prototype. |
| `LOG_SNAP_PROTOTYPE.md` | Existing prototype usage notes; read before changing input/setup assumptions. |

The current project also contains `MyProject3.Build.cs`, target files, `Config/DefaultInput.ini`, `Config/DefaultEngine.ini`, editor setup script(s), and project-owned FBX log variants under `AssetPackages/UnrealExport`. This inventory is limited to files visible in the repository tree; it is not a claim that all editor-only assets or local files are represented in Git.

## Existing behavior that must be preserved

- A whole log can be split only when the split-half mesh variant is configured and the piece is not held, placed, or supporting attached pieces.
- Quartering, groove/gutter, trough and plank actions require a split-half log. This matches the key prerequisite that a groove cannot be cut into an unprocessed whole log.
- Processing swaps to an explicitly configured geometry mesh; it is not only a material change.
- The replacement mesh attempts to preserve the old world-space bottom height.
- Snap points include stable editor-facing IDs, tags, source/receiver capability, accepted piece types, compatible tags and occupancy.
- A receiving snap point accepts a new piece only when its host is already placed and not held.
- The carry component supports both snap placement and free placement; the final demo must test these as distinct paths.

## Static-review findings and risks to resolve in Phase 1

1. **Custom snap-anchor preservation risk:** `OnConstruction` calls `RegenerateAutomaticSnapPoints`, which removes/rebuilds only IDs prefixed `AutoEnd_` or `AutoSide_`. However, `SetPieceStaticMesh` calls `GenerateDefaultSnapPoints`, which starts with `SnapPoints.Reset()`. Processing a piece through `ApplyCutAction` therefore appears to remove hand-authored snap points. Decide whether custom anchors should survive mesh replacement, and implement one consistent regeneration path.
2. **No durable world-state/persistence owner is present in the reviewed log files:** held/placed/occupied/attachment state is stored on actors/components. Phase 1 should define how durable state is eventually captured, but must not invent a parallel persistence system without reviewing the rest of the project.
3. **Processing and construction state are coupled in one actor:** `ALogBuildPiece` owns both geometry processing and attachment/placement state. Do not split or replace this class in Phase 1 without first mapping every caller and Blueprint/editor dependency. A later refactor may introduce interfaces/adapters, but must preserve the current public API until integration is planned.
4. **Mesh configuration is an implicit prerequisite:** each processing action depends on its corresponding variant mesh being assigned. The demo should report a clear blocked reason when a required variant is absent; this is not equivalent to a geometry operation succeeding.
5. **Verification is incomplete:** this is a source-level observation only. Unreal Engine 5.8 compilation, Blueprint compatibility, collision behavior, attachment transforms, save/load and runtime interaction have not been tested here.

## Phase 1 ownership map (current prototype, not the final architecture)

- Piece geometry and processing prerequisites: `ALogBuildPiece`.
- Snap-point data contract: `FLogSnapPoint` / `LogBuildingTypes.h`; snap validation and occupancy mutation currently occur in `ALogBuildPiece`.
- Player-facing pickup/placement/input flow: `ULogCarryComponent` plus `ALogSnapCharacter`.
- Project/module setup: `MyProject3.Build.cs`, target files and project configuration.
- Persistent world identity, world events, shared operation result codes, terrain, survival, farming, animals, weather and raid state: **not established by the reviewed files; must be mapped before implementing those domains**.

## Next concrete actions

1. Reconcile the custom-anchor reset behavior with intended asset/editor workflow.
2. Trace `ProcessAimedPiece`, `PlaceHeldPiece`, `TryPickupTarget`, `TryReserveSnapPoints` and detach/drop paths end-to-end.
3. Record the authoritative 119-mechanic list; do not fabricate missing mechanics to fill the registry.
4. Add shared C++ types only after the existing types and callers are reconciled.
5. Keep code review, automated checks, Unreal build, and in-editor/runtime evidence as separate status fields.
