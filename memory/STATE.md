# State (updated 2026-10-27)

UE 5.8 C++ survival / log-building game in the style of Sons of the Forest. Solo
developer; replies in Turkish. Main test level: `/Game/DemoAssets/Maps/VerticalSlice_C`.

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

## Plan

- Game plan (draft): `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md`. Physical assembly frozen until phase 10.

## Open

- Checked by eye with `mp3.SnapGallery` (screenshots), not yet by the user in PIE.
- Two mirrored wall diagonals of full-length logs clip into each other where they cross.
- Mitre (45° end) cuts do not exist; cut ends stay square.
- `GatherPlacedBodies` scans the whole world on every solve (performance risk).
- 7 joinery meshes need hand-placed NotchCradle sockets.
- The deck path in contextual placement is untested.
- Code quality: `CommitCutAction` is ~450 lines; `LogCarryComponent.cpp` and `LogBuildPiece.cpp` are 3000+ lines.
