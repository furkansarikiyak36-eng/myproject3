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
- The current status ledger describes P2.1 as owned dirty-water filling implemented but boiling and player drinking control/UI incomplete.
- P2.2 has fatigue-based movement/action gates but lacks explicit sleep/rest input and work-speed/tool-duration coupling.
- P2.3 has wetness and wet-cold condition triggers; visible drying feedback/threshold tuning remains.
- P2.5 has dirty-water/spoiled-food/wet-cold, fall and animal-bite triggers; toxic-rain and player-visible condition status remain.
- Recent source changes on this branch are code-written, not built or PIE-verified.
- `TryConsumeFoodItem` requires a unique item present in both inventory and ownership ledger; its condition and thirst effect are transactional. Hand-craft outputs are ordinary stacks today, so any boiling/drinking UI must not pretend ordinary stacks are ledger-owned.
- `TryBeginRest` uses `UWorldSimulationClockSubsystem`; the current VerticalSlice_C actor creates a profile and binds I/O/J/F2/F3/F4 but has no explicit drink/rest key. Verify the shared clock advances in this demo before wiring rest.

### Changes
- Checkpoint reset to the user's current Phase 2 task before source reads.

### Next step
Next: verify VerticalSlice_C clock stepping and inventory output semantics, then add clear player drinking/rest interactions that only consume valid ledger-owned consumables, with tests where safe.
