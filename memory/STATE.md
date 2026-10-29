# State (updated 2026-10-29)

UE 5.8 C++ survival / log-building game in the style of Sons of the Forest. Solo
developer; replies in Turkish. Main test level: `/Game/DemoAssets/Maps/VerticalSlice_C`.

## Now (2026-10-29)

Working branch: **`game/p00-adopt-group-a`** = `group-a-code-closure` (ChatGPT backend) + `feature/simple-intent`
(placement kernel, plan, memory). Builds; **85/85 automation tests pass**. Not played in PIE.
Plan: `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md`; tasks `ROADMAP.md`; per-task status `PLAN_STATUS.md`.
Current work: finishing the partial tasks (`CURRENT_TASK.md`). From 2026-10-29 code is written without
building (rules in `ROADMAP.md`); one batch build/test/fix at the end; the new scene last.
Last built and tested: `bbc57b3` = 102/102.

## Branches

| Branch | State |
|---|---|
| `main` | Behind; the snap work is not merged yet. |
| `feature/global-snap-kernel` | Pushed. One placement kernel for all materials. |
| `feature/contextual-interaction-resolver` | Another assistant's work on top of the kernel (contextual targets, aim cuts). |
| `feature/simple-intent` | Newest. Contextual placement without modes, symmetric diagonals, bent end joints. |
| `group-a-code-closure` | ChatGPT, +182 commits on simple-intent@321e55a: survival items, conditions, equipment, farming items, excavation yield, routes. Did not compile as pushed; fixed on `game/p00-adopt-group-a` (250ce13). |
| `fix/vertical-slice-b-core-loop`, `phase-03…14-*-sequential` | Older separate lines without the placement kernel. Not merged anywhere. |
| PR #34 `architecture/global-snap-variation-library` | Has a second, conflicting kernel. Recommended: close it. |

## Works (build + 85 automation tests on game/p00-adopt-group-a)

- One placement kernel (`Source/MyProject3/Placement/`): solve is pure, preview equals commit, all-or-nothing commit.
- Logs: End.A, End.B, Station.NN every 75 cm (centres 37.5 + 75·i); joints by id.
- Contextual placement (no modes): post top → across, wall top → next row, side → T, end → continue,
  ground → lie, looking down → post, post side under a beam → strut. R switches to manual modes.
- Axe: aim decides the cut; the end face splits the log lengthwise.
- Symmetry: centred on a post top on a 45° grid, post-to-post spans, wall diagonals and their mirror,
  corner to corner, mirrored struts.
- End to end joints bend 45 / 90 / 135 toward the carry direction.

## Plan

- Next work: `ROADMAP.md` (first open task P0.1 waits for the user: merge into `main`?).
- Game plan (draft): `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md`. Physical assembly frozen until phase 10.

## Open

- Checked by eye with `mp3.SnapGallery` (screenshots), not yet by the user in PIE.
- Two mirrored wall diagonals of full-length logs clip into each other where they cross.
- Mitre (45° end) cuts do not exist; cut ends stay square.
- `GatherPlacedBodies` scans the whole world on every solve (performance risk).
- 7 joinery meshes need hand-placed NotchCradle sockets.
- The deck path in contextual placement is untested.
- Code quality: `CommitCutAction` is ~450 lines; `LogCarryComponent.cpp` and `LogBuildPiece.cpp` are 3000+ lines.
