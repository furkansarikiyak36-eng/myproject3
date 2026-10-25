# Phase 1 — State Transition Caller Map

Status: **static repository-source inventory; not runtime validation**  
Repository: `furkansarikiyak36-eng/myproject3`  
Branch: `phase-01-foundation-v2`

## Scope and repository limitations

The branch's tracked tree was enumerated. It contains the native log prototype, its editor scripts, configuration and documentation, but no tracked `.uasset` or `.umap` files in the tree response. Therefore this map covers the tracked C++ and Python/PowerShell callers; it cannot establish whether external/local Blueprint assets call these methods.

No Unreal Engine build, Editor session, physics simulation, or gameplay test was run.

## Native state-transition call sites

| Caller | Call sequence | Intended result / note |
|---|---|---|
| `ALogBuildPiece::BeginPlay` | Initializes `bIsPlaced = bStartsPlaced`; sets simulation from `!bIsPlaced`; sets mesh collision profile/enabled state | Authored startup state is applied once when play begins. |
| `ULogCarryComponent::TryPickupTarget` | `DetachFromSnapParent()` → `SetHeldState(true)` → `SetPlacedState(false)` | Detaches a child first, then disables physics/collision for carrying. `SetPlacedState(false)` also writes `bStartsPlaced=false`. |
| `ULogCarryComponent::PlaceHeldPiece` — successful snap | `TryReserveSnapPoints()` (which calls `AttachToSnapParent()`, which calls `SetPlacedState(true)`) → `SetHeldState(false)` → `SetPlacedState(true)` | Placement state is applied more than once. It is currently synchronous but setter responsibilities overlap. |
| `ULogCarryComponent::PlaceHeldPiece` — free placement | `SetHeldState(false)` → `SetPlacedState(true)` | Enables collision, then marks the piece placed and disables simulation. |
| `ULogCarryComponent::DropHeldPiece` | `SetHeldState(false)` → `SetPlacedState(false)` | Re-enables collision and then enables physics for a loose piece. |
| `ALogBuildPiece::AttachToSnapParent` | Sets parent/anchor indices; registers child; attaches actor; calls `SetPlacedState(true)` | Structural relation is explicit. No separate structural-load validation is present in this path. |
| `ALogBuildPiece::DetachFromSnapParent` | Detaches actor; releases parent anchor; unregisters child; releases own anchor; clears parent indices | Source-level cleanup path for a child being picked up. |
| `ALogBuildPiece::TryReserveSnapPoints` | Validates; marks both anchors occupied; calls `AttachToSnapParent` | Validation occurs before mutation. There is no explicit rollback if the attachment helper later becomes fallible. |

## Editor-script call sites

| Script | State/configuration interaction |
|---|---|
| `UnrealEditorScripts/CreateLogSnapTestLevel.py` | Sets `starts_placed` on spawned native pieces; calls `set_placed_state(True)` for initially placed support pieces; calls `add_custom_snap_point` for support-top anchors. The C++ API now rejects None/reserved-prefix/duplicate IDs with a log warning, but the script's runtime result has not been verified. |
| `UnrealEditorScripts/BuildLogSnapPrototype.ps1` | Builds `MyProject3Editor Win64 Development`; checks for an editor process using this project path. It does not run gameplay tests. |
| `UnrealEditorScripts/ImportLogsToScene.py` | Creates/repairs a separate mesh-gallery level and imports assets; no direct calls to the native held/placed state API found in the tracked script. |
| `UnrealEditorScripts/CreateCubeTestLevel.py` | Creates/repairs a separate cube test level; no direct calls to the native held/placed state API found in the tracked script. |
| `UnrealEditorScripts/FixSceneView.py` | Changes editor viewport camera only; no actor-state transition calls found. |

## Design implications (not automatic permission to refactor)

1. **Startup default and runtime state are conflated.** `SetPlacedState` mutates `bStartsPlaced`, despite the latter being described as the level/startup default. Do not split the properties until Blueprint/editor consumers are fully inventoried.
2. **State application is distributed.** Carry code calls two setters in a prescribed order, while attachment also calls `SetPlacedState`. A future single transition owner may reduce order sensitivity, but introducing it now would change a public Blueprint-callable API.
3. **Snap reservation is synchronous but not transactional by contract.** All compatibility checks currently precede anchor mutation. If attachment acquires fallible steps, it will need a result/rollback path.
4. **Blueprint coverage remains unknown.** No tracked binary Blueprint/level assets were present in the enumerated tree. Check the user's local project/content before concluding these are all callers.
5. **The 119-mechanic authoritative list is not present in this tracked tree scan.** Do not infer a complete mechanic registry from this prototype.

## Recent source safeguard

`ULogCarryComponent::PlaceHeldPiece` now restores the pre-attempt transform, clears the stale snap candidate, and returns failure when a valid snap candidate's anchor reservation fails. This prevents that explicit failed snap attempt from falling through into the free-placement path. This is a source-level safeguard; the race/stale-candidate scenario still needs the planned Unreal runtime test.

## Custom snap-point ID safeguard

`ALogBuildPiece::AddCustomSnapPoint` now rejects `NAME_None`, IDs beginning with `AutoEnd_` or `AutoSide_` (case-insensitive), and any ID already present in `SnapPoints`. Rejected calls emit a `LogTemp` warning and leave the array unchanged. The Blueprint-callable signature remains `void` for compatibility; this means callers do not receive a structured failure result. This is a source-level change only, pending Unreal build and focused runtime/editor checks.
