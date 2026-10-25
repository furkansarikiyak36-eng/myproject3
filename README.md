# MyProject3 — Unreal Engine 5.8

This repository contains the Unreal project and a first playable prototype layer for physical log carrying, placement, and snap-based construction.

## Current focus: Log Carry + Snap

- `Source/MyProject3/LogBuildPiece.*`: log piece actor, snap anchors, parent/child structural attachment, and guarded mesh-state changes.
- `Source/MyProject3/LogCarryComponent.*`: pickup/drop, carry preview, crosshair-guided snapping, placement-mode rotation, processing-action selection, and development feedback.
- `Source/MyProject3/LogSnapCharacter.*`: simple first-person prototype pawn and input bindings.
- `Source/MyProject3/LogSnapGameMode.*`: test game mode.
- `UnrealEditorScripts/CreateLogSnapTestLevel.py`: imports the selected FBX variants and builds `/Game/Maps/LogSnap_Playground`.
- `UnrealEditorScripts/BuildLogSnapPrototype.ps1`: Windows helper to compile the editor target.
- `Source/MyProject3/LOG_SNAP_PROTOTYPE.md`: controls, workflow, current constraints, and next mechanics.

## Quick start (Windows)

1. Pull `main`.
2. Close Unreal Editor and compile:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\UnrealEditorScripts\BuildLogSnapPrototype.ps1
   ```
   Pass `-EngineRoot "D:\Path\To\UE_5.8"` if Engine 5.8 is installed outside a standard location.
3. Reopen the project. In Output Log → `Cmd`, run:
   ```text
   py "C:\Users\merts\Downloads\MyProject3\UnrealEditorScripts\CreateLogSnapTestLevel.py"
   ```
4. Open `/Game/Maps/LogSnap_Playground` and press **Play**.

Controls: WASD + mouse to move/look; E pickup/place; G drop; R rotate; right mouse cycles free-placement orientation; C selects a processing action; F applies it to a loose log in interaction range.

## Asset library

- `AssetPackages/UnrealExport/`: FBX static meshes for whole logs and prepared variants.
- `AssetPackages/UnrealExport/Archive/`: special notch and joint pieces.
- `AssetPackages/endgrainv2_height.zip`: supplied end-grain/height-map image archive.

The prototype uses pre-authored mesh variants for processing; it does not yet procedurally cut meshes, animate tool impacts, or implement the full Sons of the Forest construction rule set. This is an initial mechanics layer, not a claim that the project has been compiled or played successfully.
