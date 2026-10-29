# Current task (checkpoint)

**Every assistant reads this file FIRST.** If the status below is open, continue from
"Next step". Do not treat source presence as runtime completion.

## Rules

1. Work on a task branch; do not commit directly to main or the shared baseline.
2. The user's requested workflow is backend first, physical assembly frozen until Phase 10, and no per-task Unreal builds. Add automation-test source with behavior changes; perform one batch build/test/fix pass at the end.
3. Never claim a task is built/tested/played unless that exact evidence exists. Web-only code changes stop at code-written.
4. After each focused work unit, update Findings/Changes/Next step and commit the checkpoint when appropriate.
5. Do not mark PLAN_STATUS.md tasks complete just because code/types are present; keep backend, player input, integration and runtime acceptance distinct.

---

## Task

Status: open
Overall goal: Finish the partial tasks in memory/PLAN_STATUS.md in dependency order, including player input/HUD where required. The larger remaining list continues after this workstream.
Current work package: P6.2–P6.3 farming and seeds; then P6.4 animal lifecycle.
Started: 2026-10-29 · Branch: game/p06-farming-seed-cycle (from game/p00-adopt-group-a).

### Plan

0. [x] Create isolated work branch from the current integrated code line.
1. [ ] Review codebase map and crop/data headers before using any APIs.
2. [ ] Add authoritative field-cell state: cleared, tilled, fertility, weeds, mulch, moisture.
3. [ ] Gate planting by crop season/month and validated seed-item ownership.
4. [ ] Add soil exhaustion/rotation effects and seed-return rules, with stable operation receipts and save/restore.
5. [ ] Register/validate crop definitions through the game-data loader and Config/GameData/crops.json.
6. [ ] Add focused automation-test source (positive, negative, retry, seasonal, save/restore).
7. [ ] Continue P6.4 animal capture/pen/slaughter only after crop contracts are coherent.
8. [ ] Update the task/status records honestly; keep P7/P8/P9.2 and the full-year world test queued.
9. [ ] Batch Unreal build and automation test after the intended code-only batch; do not claim pass before local Unreal evidence.
10. [ ] New Valley scene last, after backend/integration gates.

### Fixed decisions from the user

- Do not compile/test after each task; code first, batch build/fix at the end.
- Build the scene last; backend and a headless whole-world year test come first.
- P10 physical assembly is frozen/skip for now (no new thatch/notch/mitre geometry in this package).
- Preserve the 119 mechanic IDs and data-driven item/recipe lists.

### Read

- No source files read yet for this work package.

### Findings

- The prior checkpoint claims game/p00-adopt-group-a reached 102/102 tests at bbc57b3; this is historical local evidence and is not proof that this new branch or later source changes pass.
- The project plan identifies the next slice as field-cell state, plant_months, soil exhaustion, seed saving, crop-data registration and tests.

### Changes

- Created isolated branch game/p06-farming-seed-cycle from game/p00-adopt-group-a.
- This checkpoint records the P6 crop/seed work before inspecting source code.

### Next step

Read memory/README.md, memory/STATE.md, the latest session note and memory/CODEBASE.md to identify the authoritative crop, ownership, save and test modules. Then inspect only the named headers/data files and implement the crop/seed contracts without building.
