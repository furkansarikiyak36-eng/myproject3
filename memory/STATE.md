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
| `work/phase-3-4-closure` | ChatGPT, +179 on p00@`ea00757`, unbuilt. Not merged; conflicts with p00. |
| `game/p06-farming-seed-cycle`, `game/p07-zone-entry-progression` | ChatGPT, forked from `0d91300`; a second farming API. Not merged. |
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

## Recent progress — 2026-10-29

- On `game/p00-adopt-group-a`, committed P1.3 natural-event director/data and wired the game-data loader + coordinator transaction/rollback.
- Added P1.4 read-only HUD data aggregation + Canvas display and made VerticalSlice_C sun presentation use the shared simulation clock.
- Added save schema v28 with v27 migration, plus one focused Automation test source. Commit chain: `0c2f1e0` → `cd76c57` → `07dfb57` after checkpoint `f5c44c5`.
- These P1 changes are **code-written only**; Unreal 5.8 build, automation, PIE and save/load validation have not run.
- P5.1: added deterministic geology-by-region/depth/cell/shared seed, configured weighted material regions in `layers.json`, persisted resolved material in the cell ledger, and routed the N-key dig adapter through that resolver (`e48c3c4`, `ffcb364`). Automation source added, not executed.
- P2.2: direct structure dismantle calls now enforce shared survival-cost checks unless called by a multi-piece operation with an aggregate charge; sled tow movement charges Carry stamina/energy from shared logical-time elapsed seconds (`609f445`, `28755f3`). Automation source added, not executed.

## Open

- Checked by eye with `mp3.SnapGallery` (screenshots), not yet by the user in PIE.
- Two mirrored wall diagonals of full-length logs clip into each other where they cross.
- Mitre (45° end) cuts do not exist; cut ends stay square.
- `GatherPlacedBodies` scans the whole world on every solve (performance risk).
- 7 joinery meshes need hand-placed NotchCradle sockets.
- The deck path in contextual placement is untested.
- Code quality: `CommitCutAction` is ~450 lines; `LogCarryComponent.cpp` and `LogBuildPiece.cpp` are 3000+ lines.
