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
- **C**: cycle the selected processing action (split half, quarter, hewn, gutter, trough, kalas, plank)
- **F**: apply the selected processing action to a loose, unplaced log in interaction range

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
- The selected processing actions now swap to same-length pre-authored meshes, and transitions are guarded: a whole log may split or be hewn/squared; a split half-round may be quartered, grooved, hollowed, or made into a plank. Invalid transitions are rejected instead of silently reverting to a straight whole log.
- The current processing action is a prototype interaction, not a physically animated axe/saw operation. Tool requirements, hit location, cut progress, cut animation, chips/sparks, and multiplayer authority are not implemented yet.
- A split half-round's groove operation is accepted only when the piece is in the split-half state. The prototype does not yet generate new geometry procedurally; it selects the matching imported variant mesh.
- The green anchor marker is development feedback, not a final ghost mesh or production HUD.
- The code has been added to GitHub but has not been compiled or run in the user's Unreal Editor. The first editor build is still needed before the test-map script can execute.

## Assets used by the test

Imported from `AssetPackages/UnrealExport`:
- whole 600 and 300 length logs
- split half-round log meshes (distinct from half-length logs)
- gutter / trough log
- hewn log
- plank / kalas
- saddle-notch joint sample

The height-map ZIP is not automatically assigned as a material in this prototype. Material and end-grain work remain a separate pass.
