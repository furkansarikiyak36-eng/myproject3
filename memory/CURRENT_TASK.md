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
4. [ ] Implement field-cell state/actions, seasonal planting, soil exhaustion/rotation and seed return under idempotent operation rules.
5. [ ] Load crop months/seed-return/soil parameters from crops.json; validate all references and register definitions in ApplyToWorld.
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
- Agriculture header now defines per-crop plant months/seed returns/soil costs and a SaveGame field-cell state model with clear/till/fertilize/weed/mulch/water action contracts.
- Existing two-array crop/animal snapshot APIs are retained as compatibility overloads; the persistence path will use a new field-aware overload.
- Implementation still needs to enforce field prerequisites, seasonal month checks, item output receipts, field/crop consistency and v23 migration. This header-only change is code-written, not compiled.

## Changes
- Created game/p06-farming-seed-cycle from game/p00-adopt-group-a.
- Updated WorldAgricultureSubsystem.h with crop growth constraints, field-cell types, work kinds, revision-guarded planting and field-aware snapshot APIs.

## Next step
Implement WorldAgricultureSubsystem.cpp against the new declarations, preserving the existing ownership/rollback behavior and validating all receipt/snapshot references. No build.
