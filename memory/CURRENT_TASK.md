# Current task (checkpoint)

Status: open
Overall goal: Finish the partial tasks in memory/PLAN_STATUS.md in dependency order, including player input/HUD where required.
Current work package: P6.2–P6.3 farming/seed lifecycle, then P6.4 animal lifecycle.
Branch: game/p06-farming-seed-cycle (based on game/p00-adopt-group-a).

## Fixed constraints
- Backend first; no per-task Unreal build. Add automation tests as source and perform one batch build/test/fix pass at the end.
- New Valley scene last; add a headless whole-world year test before the scene.
- Physical assembly P10 is frozen/skipped. Preserve M001–M119 IDs and data-driven definitions.
- No status above code-written until actual build/test evidence exists.

## Plan
0. [x] Create isolated task branch.
1. [x] Read the shared project memory and codebase map.
2. [x] Inspect agriculture, game-data, calendar, ownership, save schema and world-test source.
3. [ ] Define crop month and field-state data contracts.
4. [ ] Implement field preparation actions, seasonal planting, soil exhaustion/rotation and seed return under idempotent/atomic operation rules.
5. [ ] Load crop months/seed-return/soil parameters from crops.json; validate all references and register definitions in ApplyToWorld.
6. [ ] Add automation-test source for positive, negative, seasonal, retry and snapshot cases.
7. [ ] Continue P6.4 animal capture/pen/slaughter lifecycle.
8. [ ] Update PLAN_STATUS/CODEBASE/checkpoint honestly; keep P7/P8/P9.2 and whole-year test queued.
9. [ ] Batch Unreal build/tests only after the code batch; do not claim success before execution.
10. [ ] New Valley scene last.

## Read
- memory/README.md, memory/STATE.md, memory/CODEBASE.md, memory/sessions/2026-10-29_claude-code_game-plan.md
- Source/MyProject3/WorldAgricultureSubsystem.h/.cpp
- Source/MyProject3/WorldGameDataSubsystem.h/.cpp
- Config/GameData/crops.json
- Source/MyProject3/WorldCalendar.h
- Source/MyProject3/WorldItemOwnershipSubsystem.h
- Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp, Source/MyProject3/WorldSimulationSaveGame.h
- Source/MyProject3/Tests/GameWorldTests.cpp, memory/CODE_INDEX.md

## Findings
- Agriculture currently supports item-backed Plant/Harvest/Feed/Collect actions and durable per-entity receipts, but field-cell state and seasonal crop constraints do not exist in the API.
- Crop definitions currently lack plant-months, seed-return quantity, fertility/exhaustion values; crops.json already has plant_months and zone data, but ApplyToWorld does not yet register the crops into agriculture.
- Existing world save already snapshots FWorldCropState and FWorldAnimalState, so field-cell state must be explicitly added to the agricultural snapshot/save coordinator to persist.
- Web-only work must remain code-written; the historical 102/102 result on the parent branch does not validate these edits.

## Changes
- Created game/p06-farming-seed-cycle from game/p00-adopt-group-a.
- Saved this scoped task checkpoint before examining source.

## Next step
Inspect exact crop-data struct/parser, crop/animal operation bodies, agriculture snapshot signatures, save coordinator capture/restore and existing agriculture test helpers. Then implement the smallest coherent field/seed contract without building.
