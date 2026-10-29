# Current task (checkpoint)

**Every assistant reads this file FIRST.** If the status below is `open`, continue from
"Next step". Do **not** analyse again what "Findings" already says.

## Rules (the limit can end the session at any moment)

1. New task from the user: replace the section below (status `open`), write the goal and plan,
   save the file **before** reading any code.
2. After **every** step (one file read, one search, one edit): add 1–3 lines to "Findings" or
   "Changes", update "Next step", save. Small and often; never keep results only in the chat.
3. Read only what `memory/CODEBASE.md` points to. Write down in "Read" each file you opened, so
   nobody opens it again for the same question.
4. Finished: status `done`; move lasting facts to `CODEBASE.md` (Findings) and the session file;
   clear this file back to the empty template at the next new task.
5. Cannot write to the repo (read-only GitHub connector)? End **every** answer with the updated
   block of this file so the user can paste or commit it.

---

## Task

Status: `open`
Goal: Close the remaining partial tasks listed in `memory/PLAN_STATUS.md` on `game/p00-adopt-group-a`, prioritizing backend completeness and necessary player-facing interactions, then record exact status. This plan has 22 partial rows; P10 is frozen and must be skipped.
Asked by the user: "bu kısmileri bitirelim" (2026-10-29)
Started: 2026-10-29 · Last saved: 2026-10-29 · Branch: `game/p00-adopt-group-a`

### Plan (write code and tests; do not build per small task; status ceiling is code-written until batch build)

0. [x] Update this checkpoint before reading source code; scope is current `PLAN_STATUS.md`, not the stale count of 27 in older notes.
1. [ ] P0.5 + P2.1–P2.5: remove hard-coded demo recipe; complete water, fatigue action effects, wetness/cold triggers, illness/injury triggers; review ledger item ownership.
2. [ ] P3.1–P3.2 + P4.1/P4.3/P4.4: fuel wetness and fire interaction, enclosure heat-loss gaps, resource placement hooks, station interaction, tool-item strength links.
3. [ ] P6.1–P6.4: field-cell states, calendar-gated planting, all crop steps supported by current architecture, soil exhaustion/seed return, animal capture/pen/slaughter chain; register definitions from data.
4. [ ] P7.1–P7.4: player route cost, populated zones, physical discovery placement and unlock gates, missing primitive infrastructure and weather maintenance.
5. [ ] P8.2–P8.3: raid signals, snow load/avalanche/fire spread, material-based repair.
6. [ ] P9.2 + Save: helper station usage and persistence of all relevant actor transforms/system state.
7. [ ] Do not implement P10 now (explicitly frozen); do not invent work for completed tasks. Keep existing physical snap kernel unchanged.
8. [ ] Add/update targeted automation tests for the changed logic and a headless whole-world year/save-load test, without invoking a build at each step.
9. [ ] Update `PLAN_STATUS.md`, `CODEBASE.md`, this checkpoint and roadmap truthfully. Mark code only as `code-written`; do not claim compile/PIE verified until batch validation is run.
10. [ ] Once all intended edits are in, prepare one batch build/fix session and then wire/inspect the scene; scene remains last.

### Rules inherited from user
- No build after each task: write code and tests, batch build/fix at the end (`memory/ROADMAP.md`).
- Backend first; scene last; include a headless full-year simulation test.
- P10 physical assembly is frozen: skip it rather than extending the scope now.
- Use names/signatures from headers read in this session; do not guess Unreal reflection/API names.
- After every repository step, update this file with Findings/Changes and Next step.

### Read
- `memory/CURRENT_TASK.md` (this checkpoint only; no gameplay source read yet).

### Findings
- Current status source lists 22 partial tasks (not 27); P10 is one row but explicitly frozen.
- Working branch is `game/p00-adopt-group-a`; `memory/ROADMAP.md` says code status must remain `code-written` until batch validation.

### Changes
- Replaced the previous stale checkpoint with this user's current task and plan before opening gameplay source.

### Next step
Read `memory/CODEBASE.md` and `memory/CODE_INDEX.md` only to identify the exact source/header paths for P0.5/P2, then fetch and inspect the relevant headers before writing code. Update this checkpoint after each read/edit.
