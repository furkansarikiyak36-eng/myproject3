# Current task (checkpoint)

**Every assistant reads this file FIRST.** If the status below is `open`, continue from "Next step".

## Rules
1. For a new task, update this checkpoint before source reads.
2. After each meaningful repository step, record findings/changes and the next step.
3. Never claim build, automation, or PIE success unless run and evidenced.
4. Keep scene/mesh placement until the backend batch is complete.
5. P10 is frozen; do not work on it.

---

## Task

Status: `open`
Goal: Continue Phase 2 survival mechanics on `game/p00-adopt-group-a`: close P2.1 water consumption/boiling, P2.2 rest and fatigue effects, P2.3 wetness/drying feedback and P2.5 environmental condition integration where safely supported. Add focused automation coverage, sync status/docs, and do not build after each small change.
Asked by user: "faz 2 ile devam"
Started: 2026-10-29 · Branch: `game/p00-adopt-group-a`

### Plan
0. [x] Refresh this checkpoint before reviewing gameplay source.
1. [ ] Re-check exact current Phase 2 APIs and current status ledger.
2. [ ] Choose the highest-value safe gap, implement code + focused tests from verified interfaces.
3. [ ] Repeat for other P2 gaps where code can be completed without guessing.
4. [ ] Update `PLAN_STATUS.md`, `ROADMAP.md`, `CODEBASE.md`, and this checkpoint.
5. [ ] Mark all newly authored work `code-written / unbuilt-unrun`; batch validation comes later.

### User constraints
- Backend/mechanics first, scene last.
- No build/test run after each small task; tests may be authored but never described as passed unless executed.
- Don’t make disconnected mechanics; keep inventory ownership, world clock, weather, conditions and UI hooks coherent.
- Don’t expand P10 physical snap work.

### Read
- `memory/PLAN_STATUS.md` (current branch task state and P2 gaps).
- `memory/CURRENT_TASK.md` (prior checkpoint replaced for this request).
- Previously inspected in earlier work: `WorldSurvivalSubsystem.h/.cpp`, `VerticalSliceCWorldActor.h/.cpp`, `LogSnapCharacter.h/.cpp`, `Config/GameData/conditions.json`, `Tests/MyProject3CoreTests.cpp`. Refresh the exact portions needed before editing.

### Findings
- The current status ledger's P2.1 notes were stale: the branch now has authored creek filling, timed boiling, ledger-owned drinking, direct player controls and status feedback.
- P2.2 now has F6 rest on the shared clock; active rest blocks movement, jump is next to audit, and fatigue effectiveness scales tree chop, log processing and voxel digging.
- P2.3 has rain/creek wetness, shelter/warmth drying, wet-cold illness and F7 wetness status; a warm-shelter drying test was authored, with thresholds still requiring batch/PIE tuning.
- P2.5 has dirty-water/spoiled-food/wet-cold, fall and bite triggers; F7 lists condition IDs and survival health. Toxic rain remains part of P8.1; all new tests remain unrun.
- Recent source changes on this branch are code-written, not built or PIE-verified.
- Current branch includes a receipt-backed `TryBoilWaterAtFire`, M-key drinking, F5 boiling, F6 rest and F7 survival-status wiring, plus a water automation test. `items.json` now contains `Water.Boiling` as a non-consumable intermediate item (89 items; JSON parse checked). `TryBoilWaterAtFire` is defined once and uses the authored `boil_water` duration (1188 logical seconds).
- The single water-boiling implementation stores start time/duration in the survival action receipt, retains the same unique item ID, synchronizes inventory/ownership state, and uses the v27 save migration. Water automation now covers fill/retry, start/wait/finish, snapshot restore, and drinking. Source-only audit confirms one implementation; no build or test was run.
- Inventory item definition transitions are now accepted by the ownership ledger only when the item retains a valid Inventory owner; snapshot validation accepts this new same-owner transformation receipt. Save schema was advanced to v27 for boiling receipt fields.
- `TryConsumeFoodItem` requires a unique item present in both inventory and ownership ledger; its condition and thirst effect are transactional. Creek water and its processed state retain a unique ledger identity throughout the boil.
- `TryBeginRest` uses `UWorldSimulationClockSubsystem`; VerticalSlice_C binds I=fill, M=drink, F5=boil, F6=rest and F7=status. The bootstrap advances the shared clock. `RecordActivity` blocks activity during rest; `GetWorkEffectiveness` scales tree chop, log-cut work and digging. A rest/fatigue regression assertion and warm-shelter drying test are authored/unrun.

### Changes
- Implemented receipt-backed inventory-owned water processing transitions and added schema v27 migration.
- Bound player M/F5/F6/F7 interactions for drink, boil, rest and survival status; water tests now cover the complete source-level loop.
- Added fatigue work-effectiveness API, active-rest action gate, scaling for tree chop/log cut/dig/repair, plus tests for rest/work scaling and warm-shelter drying.
- Reworked axe-action accounting so misses and cut-marking do not automatically spend work stamina. Log cutting gates/records work when the actual cutting press happens.

### Next step
Update `PLAN_STATUS.md`, `ROADMAP.md` and `CODEBASE.md` to reflect the current Phase 2 code and schema v27. Keep P2 rows Kısmi until batch build/automation and PIE validation; do not call new tests passed. Then perform a source-only consistency audit (method counts, bindings, migration chain, JSON validity) and record any remaining Phase 2 gaps.
