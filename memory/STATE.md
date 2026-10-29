# State (updated 2026-10-29)

UE 5.8 C++ survival / log-building game in the style of Sons of the Forest. Solo
developer; replies in Turkish. Main test level: `/Game/DemoAssets/Maps/VerticalSlice_C`.

## Now (2026-10-29)

Active branch: **`work/phase-3-4-closure`**, based on the historical `game/p00-adopt-group-a` + placement-kernel work.
Historical test evidence: `bbc57b3` = 102/102 on the parent baseline; **it does not cover the current branch edits**.
Plan: `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md`; tasks `ROADMAP.md`; per-task status `PLAN_STATUS.md`.
Current work: blind-code closure across Phases 1–10 (`CURRENT_TASK.md`). Since 2026-10-29, code is authored without
building; focused automation tests are authored but not run. One batch build/test/fix is deferred until the user can
run it locally; PIE and the demo scene follow after backend closure. P11/P12 are deferred until after the demo foundation.

## Branches

| Branch | State |
|---|---|
| `main` | Behind; the snap work is not merged yet. |
| `feature/global-snap-kernel` | Pushed. One placement kernel for all materials. |
| `feature/contextual-interaction-resolver` | Another assistant's work on top of the kernel (contextual targets, aim cuts). |
| `feature/simple-intent` | Newest. Contextual placement without modes, symmetric diagonals, bent end joints. |
| `group-a-code-closure` | ChatGPT, +182 commits on simple-intent@321e55a: survival items, conditions, equipment, farming items, excavation yield, routes. Did not compile as pushed; fixed on `game/p00-adopt-group-a` (250ce13). |
| `work/phase-3-4-closure` | Active blind-code closure for Phase 1–10 backend gaps. Changes are unbuilt-unrun; see `CURRENT_TASK.md` and `PLAN_STATUS.md`. |
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

- Next work: `CURRENT_TASK.md` and `ROADMAP.md`; continue blind-code closure on `work/phase-3-4-closure`. Do not build or PIE-test until the later batch pass.
- Game plan (draft): `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md`. Physical assembly frozen until phase 10.

## Open

- Checked by eye with `mp3.SnapGallery` (screenshots), not yet by the user in PIE.
- Two mirrored wall diagonals of full-length logs clip into each other where they cross.
- Mitre (45° end) cuts do not exist; cut ends stay square.
- `GatherPlacedBodies` scans the whole world on every solve (performance risk).
- 7 joinery meshes need hand-placed NotchCradle sockets.
- The deck path in contextual placement is untested.
- Code quality: `CommitCutAction` is ~450 lines; `LogCarryComponent.cpp` and `LogBuildPiece.cpp` are 3000+ lines.
