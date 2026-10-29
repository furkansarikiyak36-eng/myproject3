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

### Plan (write code and tests; do not build per small task; status ceiling is code-written)

0. [x] Update checkpoint before source reads; scope from current `PLAN_STATUS.md`, not stale count of 27.
1. [ ] P0.5 + P2.1–P2.5: remove hard-coded demo recipe; complete water, fatigue action effects, wetness/cold triggers, illness/injury triggers; check ledger item ownership.
2. [ ] P3.1–P3.2 + P4.1/P4.3/P4.4: fuel wetness and fire interaction, enclosure heat-loss gaps, resource placement hooks, station interaction, tool-item strength links.
3. [ ] P6.1–P6.4: field-cell states, calendar-gated planting, crop steps, soil exhaustion/seed return, animal capture/pen/slaughter chain; register definitions from data.
4. [ ] P7.1–P7.4: player route cost, populated zones, physical discovery placement and unlock gates, missing primitive infrastructure and weather maintenance.
5. [ ] P8.2–P8.3: raid signals, snow load/avalanche/fire spread, material-based repair.
6. [ ] P9.2 + Save: helper station usage and persistence of actor transforms/system state.
7. [ ] P10 frozen: skip; do not extend physical snap kernel.
8. [ ] Add/update targeted automation tests and headless whole-world year/save-load test; no build after each step.
9. [ ] Update `PLAN_STATUS.md`, `CODEBASE.md`, this checkpoint and roadmap truthfully; do not claim compile/PIE verified.
10. [ ] When edits are done, prepare batch build/fix, then scene work last.

### User constraints
- No build after each task; write code/tests and batch build/fix at the end (`memory/ROADMAP.md`).
- Backend first; scene last; include a headless whole-world year test.
- P10 is frozen; skip it.
- Never guess Unreal API names; use headers read in this session.
- After every repository step, update Findings/Changes and Next step here.

### Read
- `memory/CURRENT_TASK.md` (checkpoint only).
- `memory/CODEBASE.md` (map; older portions describe `feature/simple-intent`, so verify the current branch status file before trusting those rows).
- `memory/CODE_INDEX.md` (raw unit index; used to identify code file names).
- `Source/MyProject3/WorldSurvivalSubsystem.h` (current declarations for survival, consumables, conditions, equipment, actions, and profile).
- `Source/MyProject3/WorldDemoScenarioActor.cpp` (scenario initialization validates and registers a `ConfiguredRecipe` property).
- `Source/MyProject3/WorldDemoScenarioActor.h` (recipe is an editor-editable `FWorldProductionRecipe` property, not hard-coded ingredient literals).
- `Source/MyProject3/WorldGameDataSubsystem.h` (data schemas: transformations, stations, crops; APIs `ApplyToWorld`, `ApplyToInventory`, `FindTransformation`).
- `Source/MyProject3/InventoryCraftingComponent.h` (inventory ownership, recipe registration, item/tool checks).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` (JSON validation and data→system registration paths inspected).
- `Source/MyProject3/WorldSurvivalSubsystem.cpp` (survival item transactions, conditions, exposure/rest, and simulation-step functions inspected by symbol).
- `Source/MyProject3/Tests/MyProject3CoreTests.cpp` (existing automation-test names/fixtures scanned for game-data, survival, agriculture, production, save, route, threat, and structure coverage).
- `Source/MyProject3/InventoryCraftingComponent.cpp` (recipe registration/crafting and item ownership implementations inspected by symbol).

### Findings
- Current status source lists 22 partial rows (not 27); P10 is explicitly frozen.
- `WorldDemoScenarioActor.h` confirms recipe values are editor-authored `ConfiguredRecipe`, so P0.5 should be resolved by making the game-data loader authoritative and not silently registering a parallel demo recipe.
- `WorldGameDataSubsystem.cpp` already converts `transformations.json` to production and hand recipes. The remaining P0.5 gap is the scenario actor's optional parallel recipe path, not absence of the data pipeline.
- `InventoryCraftingComponent.cpp` confirms unique physical items are consumed through the ownership ledger and non-consuming tools remain in inventory. Existing survival methods already implement owned-item consumption, conditions/treatment, activity stamina cost, wetness, rest quality, and temperature exposure; remaining gaps include environment/player triggers and UI/world wiring, not those bases.
- `CODE_INDEX.md` is the authoritative filename/type map; survival API names are recorded. `WorldDemoScenarioActor.cpp` does not contain literal ingredient/output values; it registers a `ConfiguredRecipe` property. Inspect its header before deciding whether P0.5 is still open or the status note is stale.
- `CODEBASE.md` is a legacy map of `feature/simple-intent`; current branch contains subsequent adoption/fixes. Current roadmap reports 102/102 tests at `bbc57b3`, while older status/memory references 85 tests. Do not conflate these snapshots.
- Working branch is `game/p00-adopt-group-a`; roadmap status ceiling is `code-written` until batch validation.

### Changes
- Replaced stale checkpoint for the current request before gameplay source reads.
- After reading `CODEBASE.md`, recorded its stale branch scope and the current roadmap's newer test-count reference.

### Next step
Fetch the current branch versions of the exact headers identified in `CODE_INDEX.md` for P0.5/P2.1–P2.5; update the checkpoint after every header read before opening the next.
