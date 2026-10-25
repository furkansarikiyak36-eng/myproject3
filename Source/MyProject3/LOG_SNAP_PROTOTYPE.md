# Log Carry + Snap Prototype (Unreal Engine 5.8)

This is the first playable prototype layer for physical log handling. It is deliberately separate from the generated asset gallery map.

## What is included

- `ALogBuildPiece`: a mesh-backed log piece with a piece type, carry/placed state and configurable local snap points.
- `ULogCarryComponent`: pickup/drop, carry preview, placement, rotational adjustment, snap-target search and anchor occupancy.
- `ALogSnapCharacter`: a small first-person test pawn for quickly trying the controls.
- `ALogSnapGameMode`: selects that pawn for the test map.
- `UnrealEditorScripts/CreateLogSnapTestLevel.py`: imports a small representative subset of the FBXs and creates `/Game/Maps/LogSnap_Playground`.

## Controls

- **WASD**: movement
- **Mouse**: look
- **E**: pick up the log under the crosshair; when carrying a log, E attempts to place it
- **G**: drop the carried log
- **R**: rotate the carried piece by 90 degrees
- **Right mouse button**: cycle horizontal / vertical / diagonal free-placement mode

A green debug marker is shown at a candidate snap anchor while a valid snap is selected.

## Install / run

1. Pull the latest `main` branch.
2. Build the **MyProject3Editor / Win64 / Development** target with the project's Unreal 5.8 toolchain. These are new reflected C++ classes, so a full editor-target build is required before the Python level script can load them. A Python import does not compile C++.
3. Start/reopen the Unreal Editor for this project.
4. In Output Log, change the input mode to **Cmd** and run:

   ```text
   py "C:\Users\merts\Downloads\MyProject3\UnrealEditorScripts\CreateLogSnapTestLevel.py"
   ```

5. Open `/Game/Maps/LogSnap_Playground`, then use **Play**.

The setup script saves the currently open level before changing maps, only cleans actors it created in its own generated map, and does not change `Generated_Cube_Test`.

## Current behavior / known limitations

This iteration establishes the core interaction loop and anchor system. It is not yet a complete Sons of the Forest construction implementation.

- Auto-generated anchors are located at the two ends of a mesh whose long axis is set to local X (configurable per piece).
- Custom anchors can be added for top/side/slot connections. The test map adds two top anchors to a placed support beam so a carried log can be snapped into a vertical post position.
- A snapped piece is attached to its parent actor and reserves both connection anchors. A piece with attached children cannot be picked up until those children are removed.
- Free placement can be restricted through `bAllowFreePlacement` / `bRequireSurfaceForFreePlacement`.
- The current version does **not** yet inspect the physical cut state of a log, automatically generate a groove/notch from tool actions, compute realistic saw/chop cuts, or enforce every construction recipe. Those should be layered in next: cut-state data, target-specific snap tags, support/ground rules, and a no-overlap/placement validation pass.
- The green anchor marker is development feedback, not a final ghost mesh or production HUD.

## Assets used by the test

Imported from `AssetPackages/UnrealExport`:
- whole 600 and 300 length logs
- half-length logs
- gutter / trough log
- hewn log
- plank / kalas
- saddle-notch joint sample

The height-map ZIP is not automatically assigned as a material in this prototype. Material and end-grain work remain a separate pass.
