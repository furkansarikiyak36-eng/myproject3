# Placement kernel — guide for Claude

One kernel decides where any piece goes and commits it all or nothing. Plan: `Docs/Architecture/GLOBAL_SNAP_SYSTEM_PLAN.md`.

## Files

| File | Role |
|---|---|
| `PlacementTypes.h` | Feature, body, pose, intent, solution, link (roles Primary / Secondary / Carries). |
| `PlacementFeatureProvider.*` | Interface a material implements, and the provider registry. |
| `PlacementProfiles.*` | Loads and validates `Config/Placement/PlacementProfiles.json` (solver, mode, params). |
| `PlacementKernelSubsystem.*` | `Solve` (pure), `CommitPlacement` (transaction), `OnPlacementCommitted`, body queries. |
| `PlacementBodyMath.*` | Pure geometry on bodies: stack, deck, step, gable, strut, post spacing, stations. |
| `PlacementSolvers.*` | Angle grid with hysteresis, AlignOne, station offsets. |
| `PlacementTransaction.h` | Step + undo; rolls back on failure. |
| `LogFeatureProvider.*`, `MaterialFeatureProviders.cpp`, `RopeFeatureProvider.cpp` | Logs, stones, sticks, ropes. |

## Rules

- The kernel files never name a material (no `ALogBuildPiece`, stone, stick, rope). The test `Placement.KernelNamesNoMaterial` enforces it. Material rules go in providers; numbers go in profile params.
- Joints use feature ids (`End.A`, `End.B`, `Station.NN`, authored ids), never array indexes.
- Log stations: one per complete 75 cm module of the real length, centre at 37.5 + 75·i from End.A; the side a joint uses is solved from the aim (`radialStepDeg`).
- New behaviour: first try an existing solver/mode with new params; add a mode only if none fits.
- After editing the JSON run `python UnrealEditorScripts/SyncPlacementCatalogue.py` (compiled copy for packaged builds; a test checks they match).

## Adding a material

1. A provider class (features, body, can/commit/revert link) registered with `REGISTER_PLACEMENT_PROVIDER`.
2. Profiles for it in the JSON, then the sync script.
3. A test in `Tests/PlacementKernelTests.cpp`. No kernel file should change.
