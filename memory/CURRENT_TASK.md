# Current task (checkpoint)

**Read this file first.** Status: open. Continue from “Next step”.

## Rules
1. Keep work on `game/p00-adopt-group-a`; do not modify `main`.
2. Backend/mechanics first. Do not place scene actors or alter physical snap geometry.
3. P10 physical assembly is frozen.
4. Write focused automation-test source with behaviour changes; never claim it passed unless executed.
5. Keep the result at `code-written` if Unreal build/test is unavailable. No blind guesses about Unreal APIs; inspect headers first.
6. Update `memory/PLAN_STATUS.md`, `memory/ROADMAP.md`, `memory/STATE.md`, and a dated session note as implementation advances.

## Task

Status: `in-progress`
Goal: Close the user-selected blind-coding tasks for P1.1–P1.4, P2.1–P2.6, P3.1–P3.4, P4.1–P4.4, P5.1–P5.3, P6.1–P6.4, P7.1–P7.4 and P9.1–P9.3 on `game/p00-adopt-group-a`. P8 is a dependency only where event simulation is needed; P10 remains frozen.

Started: 2026-10-29
Branch base inspected: `ea00757dec03300f8214a3c31fb90b3f5efb6122`
Baseline: latest source changes are documented as unbuilt/unrun. Prior build/test count must not be attributed to unbuilt changes.

## Planned work
1. Reconcile current APIs with the gap ledger and use existing domain owners instead of adding duplicate systems.
2. Close the highest-leverage shared contracts first: data-driven calendar/events and HUD signals; transaction/ownership and common action costs; fuel/shelter/plaster/production; tool durability/resource geology/stability; field/crop/animal lifecycle; player movement/zone/discovery/infrastructure; time budget/helper station reservations/year simulation.
3. Add or update data files and focused automation test sources for each changed subsystem.
4. Update task statuses conservatively to `code-written` until local Unreal build/test/PIE evidence exists.
5. Batch compile/test remains deferred to the user's UE 5.8 environment.

## Findings
- Shared clock/calendar/climate have source-level P1.1/P1.2 integration: game data loads `calendar.json`/`climate.json`, solar presentation reads the shared clock.
- P1.3/P1.4 source code is committed: nine `events.json` definitions, deterministic event lifecycle/chains, coordinator transactional advance and deferred signal publishing, schema v28 snapshot/migration, read-only HUD data contract and Canvas readout.
- `EffectId` is a future domain-adapter contract, not yet a world-effect implementation; those mutations remain P8.1.
- All P1 integration/test source is code-written only. No Unreal build, automation test, PIE or save/load roundtrip was run in this session.
- Existing systems include item ownership/operation ledger, production, storage, terrain/voxel, agriculture/wildlife, survival, structural graph, logistics/cargo, raid director, helper and save coordinator.
- Current gap ledger overstates whole-task completion if source presence is read as gameplay acceptance; treat runtime integration as a separate gate.

## Changes in this checkpoint
- Added `WorldEventDirectorSubsystem.h/.cpp` and `events.json` (nine definitions; storm→flood/toxic rain, drought→wildfire, flood→epidemic chains).
- Wired definitions into `WorldGameDataSubsystem`, coordinator fixed-step preflight/advance/rollback, and post-commit transition dispatch.
- Added `WorldHudDataSubsystem`, HUD date/season/weather/event readout, shared-clock sun time, save schema v28 and v27 migration.
- Authored `MyProject3.World.Events.DeterministicLifecycleChainAndSnapshot` for warning/active/end/chaining/snapshot/HUD coverage.
- Three code commits are on `game/p00-adopt-group-a`; the P1 source changes are unbuilt/unrun.

## Next step
Continue with the requested Phase 2–7 and Phase 9 backend gaps in bounded, source-reviewed batches. Do not claim UE compile, Automation, PIE or save/load success.
