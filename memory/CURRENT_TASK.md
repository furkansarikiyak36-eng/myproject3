# Current task (checkpoint)

Status: open
Overall goal: Finish partial tasks in memory/PLAN_STATUS.md in dependency order, including player input/HUD where required.
Current work package: P6.2–P6.3 farming/seed lifecycle, then P6.4 animal lifecycle.
Branch: game/p06-farming-seed-cycle (based on game/p00-adopt-group-a).

## Fixed constraints
- Backend first; no per-task Unreal build. Add automation tests as source and perform one batch build/test/fix pass at the end.
- New Valley scene last; add a headless whole-world year test before the scene.
- Physical assembly P10 is frozen/skipped. Preserve M001–M119 IDs and data-driven definitions.
- No status above code-written until actual build/test evidence exists.

## Plan
0. [x] Create isolated task branch.
1. [x] Read shared project memory and codebase map.
2. [x] Inspect agriculture, game-data, calendar, ownership, save and world-test code.
3. [x] Identify crop/field integration gaps and persistence call sites.
4. [x] Add field-cell state/work contracts, revisioned operations, seasonal planting, seed-output records, soil exhaustion and crop/field consistency rules.
5. [x] Extend crops.json and data-loader parsing/registration for seed-return, season and soil rules; add schema v23 field-state persistence/migration.
6. [ ] Add automation-test source for positive, negative, seasonal, retry and snapshot cases.
7. [ ] Continue P6.4 animal capture/pen/slaughter lifecycle.
8. [ ] Update PLAN_STATUS/CODEBASE/checkpoint honestly; keep P7/P8/P9.2 and full-year test queued.
9. [ ] Batch Unreal build/tests only after the code batch; do not claim success before execution.
10. [ ] New Valley scene last.

## Read
- memory/README.md, memory/STATE.md, memory/CODEBASE.md, memory/sessions/2026-10-29_claude-code_game-plan.md
- Source/MyProject3/WorldAgricultureSubsystem.h/.cpp
- Source/MyProject3/WorldGameDataSubsystem.h/.cpp; Config/GameData/crops.json
- Config/GameData/items.json, Config/GameData/transformations.json
- Source/MyProject3/WorldCalendar.h, Source/MyProject3/WorldSimulationClockSubsystem.h/.cpp
- Source/MyProject3/WorldItemOwnershipSubsystem.h, Source/MyProject3/InventoryCraftingComponent.h/.cpp
- Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp, Source/MyProject3/WorldSimulationSaveGame.h
- Source/MyProject3/Tests/GameWorldTests.cpp, memory/CODE_INDEX.md

## Findings
- Field state now tracks clearing, tilling, fertility, weed pressure, mulch coverage, moisture, planted crop, last crop/cycle and stable field action receipts. Field operations are revision-guarded and retry-safe.
- Planting now requires a prepared field, permitted plant month, fertility/weed/moisture conditions and a unique seed item owned by the expected inventory. Legacy TryPlantCrop delegates to the guarded contract.
- Harvest materializes harvest outputs plus configured seed-return items in one output registration call, clears the field planting link, and depletes fertility with a larger penalty for consecutive repeat crops.
- crops.json now carries seed-return/soil/weed values; parser validates crop IDs and planting month ranges/duplicates; ApplyToWorld registers all three crop definitions.
- Save schema v23 persists field cells. v22 migration deliberately clears absent field history and disables invented seed returns for legacy crop records.
- These are source edits only. No UHT/UBT build, automation test or PIE check has been run on this branch; parent branch's earlier 102/102 result is not evidence for these edits.

## Changes
- Created game/p06-farming-seed-cycle from game/p00-adopt-group-a.
- Updated WorldAgricultureSubsystem.h/.cpp, WorldGameDataSubsystem.h/.cpp, Config/GameData/crops.json, WorldSimulationSaveGame.h and WorldSaveCoordinatorSubsystem.cpp.

## Next step
Fix remaining static consistency issues (crop field links on wither, snapshot receipt/legacy validation, data-loader assertions), then add GameWorldTests.cpp coverage for field work retries, seasonal rejection/acceptance, exact seed ownership, returned seeds, soil exhaustion and field snapshot round-trip. No build.
