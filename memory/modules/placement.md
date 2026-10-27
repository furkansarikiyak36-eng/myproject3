# Placement kernel

Detailed guide: `Source/MyProject3/Placement/CLAUDE.md`. Plan: `Docs/Architecture/GLOBAL_SNAP_SYSTEM_PLAN.md`.

## Files

- `PlacementKernelSubsystem.*`: `Solve` (pure), `CommitPlacement` (transaction), body queries.
- `PlacementBodyMath.*`: pure geometry (stack, diagonal stack, cross on top, strut, deck, step, gable, bent end).
- `PlacementSolvers.*`: angle grid with hysteresis, AlignOne, station offsets.
- `PlacementProfiles.*` + `Config/Placement/PlacementProfiles.json` (run `UnrealEditorScripts/SyncPlacementCatalogue.py` after editing).
- Providers: `LogFeatureProvider.*`, `MaterialFeatureProviders.cpp` (stone, stick), `RopeFeatureProvider.cpp`.

## Profiles (main ones)

| Id | What |
|---|---|
| `log.end_to_end` | End on end; bends 45/90/135 toward the carry direction (`bendStepDeg`, `maxBendDeg`). |
| `log.end_to_station` | T joint on a station's side. |
| `log.station_to_end` | Across a post top (`crossTop`): centred, 45° grid, or spans to a second free post. |
| `log.parallel_stack` | Next wall row, corner crossing, post on a station, diagonals (`diagonalBandDeg`). |
| `log.strut` | Strut from post side to beam; mirrors an existing strut's leg. |
| `log.deck`, `log.stairs`, `log.step`, `log.gable` | Floors, stairs, steps, gable rows. |
| `log.notch_pair`, `log.station_in_notch` | Notched joinery meshes. |

## Rules

- No material names in kernel files (test `Placement.KernelNamesNoMaterial`).
- Joints use feature ids. New behaviour: a new param on an existing solver first.
- Every behaviour change gets a test in `Source/MyProject3/Tests/PlacementKernelTests.cpp`.
