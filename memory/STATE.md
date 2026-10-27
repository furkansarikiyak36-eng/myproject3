# State (updated 2026-10-28)

UE 5.8 C++ survival / log-building game in the style of Sons of the Forest. Solo
developer; replies in Turkish. Main test level: `/Game/DemoAssets/Maps/VerticalSlice_C`.

## Current integration pass (2026-10-28)

| Item | State |
|---|---|
| Active work branch | `integration/phase-02-14-closure` |
| Branch base | `feature/voxel-terrain-policy-spike`; do not confuse with `main` |
| Terrain decision | First playable slice uses macro Landscape + bounded smooth voxel pockets; engine Marching Cubes / ProceduralMeshComponent |
| Voxel source delta | Bounded soil-aware dig API, expected-revision check, idempotent receipts, grounded-support spatial guard, density snapshot API |
| Persistence delta | SaveGame schema v14 contains identity-bound voxel density snapshots; restore rebuilds mesh/collision for chunks already present in the level |
| Added source tests | Bounded-dig policy, grounded-structure guard, density-snapshot round trip |
| Engine acceptance | NOT RUN for this branch: UE 5.8 UHT/build, Automation tests, Editor/PIE, full save/load and performance |
| Open integration blockers | Logical terrain state vs physical density is not one atomic transaction; no live player/tool caller; no neighboring chunk seams, async mesh/collision, navmesh update, or missing-chunk recreation |

The phase map and remaining source/runtime gates are recorded in `Docs/Architecture/PHASE_02_14_INTEGRATION_CLOSEOUT.md` and the detailed terrain gate in `Docs/Architecture/PHASE_02_VOXEL_VALIDATION_AND_EXIT_GATE.md`. Do not mark any M-ID accepted merely because its source file exists.

## Branches

| Branch | State |
|---|---|
| `main` | Behind; the snap work is not merged yet. |
| `feature/global-snap-kernel` | Pushed. One placement kernel for all materials. |
| `feature/contextual-interaction-resolver` | Another assistant's work on top of the kernel (contextual targets, aim cuts). |
| `feature/simple-intent` | Newest. Contextual placement without modes, symmetric diagonals, bent end joints. |
| PR #34 `architecture/global-snap-variation-library` | Has a second, conflicting kernel. Recommended: close it. |

## Works (build + 68 automation tests)

- One placement kernel (`Source/MyProject3/Placement/`): solve is pure, preview equals commit, all-or-nothing commit.
- Logs: End.A, End.B, Station.NN every 75 cm (centres 37.5 + 75·i); joints by id.
- Contextual placement (no modes): post top → across, wall top → next row, side → T, end → continue,
  ground → lie, looking down → post, post side under a beam → strut. R switches to manual modes.
- Axe: aim decides the cut; the end face splits the log lengthwise.
- Symmetry: centred on a post top on a 45° grid, post-to-post spans, wall diagonals and their mirror,
  corner to corner, mirrored struts.
- End to end joints bend 45 / 90 / 135 toward the carry direction.

## Open

- Checked by eye with `mp3.SnapGallery` (screenshots), not yet by the user in PIE.
- Two mirrored wall diagonals of full-length logs clip into each other where they cross.
- Mitre (45° end) cuts do not exist; cut ends stay square.
- `GatherPlacedBodies` scans the whole world on every solve (performance risk).
- 7 joinery meshes need hand-placed NotchCradle sockets.
- The deck path in contextual placement is untested.
- Code quality: `CommitCutAction` is ~450 lines; `LogCarryComponent.cpp` and `LogBuildPiece.cpp` are 3000+ lines.
