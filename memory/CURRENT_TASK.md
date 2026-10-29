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
- Current work branch contains shared logical clock, calendar, climate and coordinator; natural event director and event HUD are missing/incomplete.
- Save schema is v27. Any persisted field requires validation and a migration.
- Existing systems include item ownership/operation ledger, production, storage, terrain/voxel, agriculture/wildlife, survival, structural graph, logistics/cargo, raid director, helper and save coordinator.
- Current gap ledger overstates whole-task completion if source presence is read as gameplay acceptance; treat runtime integration as a separate gate.

## Next step
Inspect the remaining exact declarations and caller paths; implement cohesive changes in bounded batches, then update the status ledger and dated session log. Do not claim UE compile, Automation, PIE or save/load success.
