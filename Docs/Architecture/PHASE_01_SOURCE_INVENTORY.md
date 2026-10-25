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

1. **Custom snap-anchor preservation — addressed in this branch:** `SetPieceStaticMesh` now calls `RegenerateAutomaticSnapPoints`, matching `OnConstruction`. That helper removes/rebuilds only IDs prefixed `AutoEnd_` or `AutoSide_`, preserving hand-authored points. Their semantic suitability for every replacement geometry still needs asset/editor review; preserving them does not guarantee that a geometry-specific anchor remains correctly positioned.
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

1. **Completed at source level:** trace `ProcessAimedPiece`, `PlaceHeldPiece`, `TryPickupTarget`, `TryReserveSnapPoints`, attach/detach, and drop paths; see `PHASE_01_CALL_CHAIN_REVIEW.md` and `PHASE_01_STATE_TRANSITION_CALLER_MAP.md`.
2. **Completed at source level:** preserve custom anchors across mesh replacement, validate custom IDs, revalidate both snap endpoints, reject unsafe reparent/cycle cases, and make snap reservation conditional on successful actor attachment. The unused private `GenerateDefaultSnapPoints` helper—which cleared all anchors and could erase custom points—was removed so the destructive reset path is no longer available. Runtime/editor checks remain pending.
3. **Resolved for project scope:** `MECHANIC_REGISTRY_119.json/.md/.csv` now define the canonical 119-item scope from this commit onward. The historical 119-pass report was a count, not an exact name list; the larger user-authored catalog remains source context. All new registry entries remain `unreviewed` until code/test evidence is attached.
4. Add shared C++ types only after the authoritative mechanics and all existing/local callers are reconciled; no duplicate state owner has been added.
5. Keep code review, automated checks, Unreal build, and in-editor/runtime evidence as separate status fields. Phase 1 foundation/scope is closed; the integrated validation gate remains deferred to the end of the phases.

## Build, configuration and editor automation

- `MyProject3.Build.cs` declares `Core`, `CoreUObject`, `Engine`, `InputCore` and `EnhancedInput`. The current prototype binds keys directly through `UInputComponent`; it does not yet require a new Enhanced Input mapping asset for its documented controls.
- `MyProject3.uproject` declares engine association `5.8`; Modeling Tools Editor Mode, Python Script Plugin and Editor Scripting Utilities are editor-only plugins.
- `UnrealEditorScripts/BuildLogSnapPrototype.ps1` builds the `MyProject3Editor Win64 Development` target and intentionally does not launch the editor or run gameplay tests. It checks for an editor process tied to this project before building.
- `UnrealEditorScripts/CreateLogSnapTestLevel.py` creates a separate `/Game/Maps/LogSnap_Playground` map, imports selected project FBXs when absent, and uses `LOGSNAP_DEMO_` actor labels for generated scene actors. Its safety behavior and generated-map cleanup should be preserved during demo improvements.
- `UnrealEditorScripts/ImportLogsToScene.py`, `CreateCubeTestLevel.py` and `FixSceneView.py` are additional editor utilities. Their interactions with current levels and generated actors need a separate pass before changing them.
- `Config/DefaultInput.ini` includes Enhanced Input player/component classes, while the prototype uses direct key bindings. Keep this mixed setup intentional until a dedicated input migration phase; do not casually switch one side and strand the prototype controls.
- `LOG_SNAP_PROTOTYPE.md` documents the setup flow and states that the native classes have not yet been compiled/run in the user's editor. The build helper's success message is not a gameplay test result.

## Scope note

This is a repository-tree and source-level inventory, not a complete audit of editor-generated assets, local-only content, or all Blueprint references. The authoritative mechanic list and final runtime evidence remain separate inputs.
