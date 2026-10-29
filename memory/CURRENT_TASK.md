# Current task (checkpoint)

**Every assistant reads this file FIRST.** Continue the blind-coding closure below.

## Rules
1. Backend/mechanics first; do not spend time on scene polish or P10 physical snap geometry.
2. No per-task Unreal builds or PIE. Author focused automation tests with code, but never claim tests passed unless executed.
3. Use exact declarations from headers; do not guess Unreal API names.
4. New saved fields require restore validation and save-schema migration.
5. Keep each commit/task narrow and record it in `PLAN_STATUS.md`, `ROADMAP.md`, and a session note.
6. Mark source work only `code-written` until the user's batch build/test is run.

## Task

Status: `open`
Goal: Finish the previous response's A group: blind-codeable backend rules and data contracts, excluding anything frozen under P10 and anything that requires in-game tuning to call complete.
Branch: `work/phase-3-4-closure`
Started: 2026-10-29 · ChatGPT

### Scope
- P0.2 placement rules documentation.
- P1.3 natural-event state/definition layer and P1.4 calendar/event presentation contract where existing UI architecture allows.
- Close code/data gaps for P2.1/P2.2/P2.3/P2.5, P3.1/P3.2/P3.3/P3.4, P4.1/P4.3/P4.4, P5.2, P6.2/P6.3, P7.1-P7.4, P8.1-P8.3, P9.1/P9.2/P9.3, P11/P12 and save consistency.
- P10 remains frozen; no physical joinery geometry work.
- Do not call a task tested/played/done without actual local evidence.

### Plan
1. [x] Make the placement/carry contracts explicit; P0.2 documentation is committed.
2. [in-progress] Implement the natural-event runtime and connect it to existing shared-clock orchestration without bypassing rollback semantics. Event data loading/validation is now authored.
3. [in-progress] Close safe backend-only gaps in survival, shelter/plaster, gathering/stations, farming/regions/routes/raids/repair/helper. Plaster/charcoal data-contract test is authored.
4. [todo] Add focused automation tests for all changed contracts; author only, do not claim run.
5. [todo] Reconcile remaining status docs and keep the session note current; batch build/test remains for local Unreal session.

### Findings
- This branch contains station placement, gatherable spawning and charcoal recipe/fire-pause changes authored earlier in this work session.
- P0.2 is now documented in the placement guide and carry module note. Open geometry issues remain frozen until P10.
- Added `Config/GameData/events.json` with 9 natural-event contracts, a `FGameDataWorldEvent` data type, loader parsing/validation, and authored checks for the event data alongside plaster/charcoal contracts. Static JSON/reference sanity was checked in-tool; Unreal tests remain unrun.
- `mix_plaster` has no persistent wall-coating behavior yet; natural-event runtime and date/event UI are still open. Event definitions are data only until the scheduler/effect lifecycle is implemented.
- The status ledger was reconciled for P0.2/P3.3/P3.4, but broader task closure and one batch validation remain.
- Current known baseline evidence does not cover these recent unbuilt changes. All new work stays `code-written / unbuilt-unrun`.


## User scope update (2026-10-29)
User approved blind coding through Phases 1–10; Phases 11–12 are deferred until after demo. Author tests as mechanisms are coded, but batch Unreal build/automation/PIE validation is reserved for the user's later pass. P10 physical joinery remains last/frozen until upstream mechanics are closed.
