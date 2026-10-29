# Current task (checkpoint)

Status: open — P6 source-written; Unreal verification pending
Overall goal: Finish partial tasks in memory/PLAN_STATUS.md in dependency order, including player input/HUD where required.
Current work package completed as code: P6.1 field contracts, P6.2–P6.3 crop/seed lifecycle, P6.4 logical animal capture/pen/feeding/products/slaughter.
Branch: game/p06-farming-seed-cycle (based on game/p00-adopt-group-a).

## Fixed constraints
- Backend first; no per-task Unreal build. Add automation tests as source and perform one batch build/test/fix pass at the end.
- New Valley scene last; add a headless whole-world year test before the scene.
- Physical assembly P10 is frozen/skipped. Preserve M001–M119 IDs and data-driven definitions.
- No task is marked built/tested/played without actual evidence. Source-only automation tests are not pass evidence.

## Plan
0. [x] Create isolated task branch.
1. [x] Read shared project memory and codebase map.
2. [x] Inspect agriculture, game-data, calendar, ownership, save and test code.
3. [x] Identify crop/field integration and persistence gaps.
4. [x] Add field-cell state/work contracts; revision-guarded/idempotent clear/till/fertilize/weed/mulch/water; prepared-field and month-gated planting; seed return and repeated-crop fertility cost.
5. [x] Validate and load crops.json tuning; register the three crop definitions; persist field cells; add schema v23 migration.
6. [x] Add automation-test source for field work, seasonal rejection/acceptance, seed ownership/returns, repeat receipts and field snapshot.
7. [x] Add data-driven animal definitions, unique-item and count-stack capture, logical 4-stake/1-rope pen, gate/assignment, owned-item feed, closed-pen products, data-driven slaughter outputs; save pens in schema v24 and update simulation rollback.
8. [x] Update memory/PLAN_STATUS.md and CODEBASE.md to reflect source-written status honestly. CODE_INDEX.md is generated and is still stale; it should be regenerated with tools/memory_code_index.py when the repo is locally available.
9. [ ] Batch Unreal UHT/UBT build, automation tests, then fix all errors. No build/test has run on this branch.
10. [ ] Add headless whole-world year simulation and any P7/P8/P9.2 tasks in dependency order.
11. [ ] Integrate WildlifeActor/stick-trap interactions, player input/HUD, field tools, physical pen/crop/animal visuals; complete remaining field chain.
12. [ ] New Valley scene and PIE play acceptance last.

## Files read
- memory/README.md, memory/STATE.md, memory/CODEBASE.md, memory/sessions/2026-10-29_claude-code_game-plan.md
- Source/MyProject3/WorldAgricultureSubsystem.h/.cpp
- Source/MyProject3/WorldGameDataSubsystem.h/.cpp
- Config/GameData/items.json, crops.json, animals.json, transformations.json
- Source/MyProject3/WorldCalendar.h, Source/MyProject3/WorldSimulationClockSubsystem.h/.cpp
- Source/MyProject3/WorldItemOwnershipSubsystem.h, Source/MyProject3/InventoryCraftingComponent.h/.cpp
- Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp, Source/MyProject3/WorldSimulationSaveGame.h, Source/MyProject3/WorldSimulationCoordinatorSubsystem.cpp
- Source/MyProject3/Tests/GameWorldTests.cpp, memory/CODE_INDEX.md, memory/PLAN_STATUS.md

## Findings
- Field state now tracks clear/till, fertility, weeds, mulch, moisture, planted crop, last-crop/repeat count and revision-bound receipts. These are logical contracts; field visuals/tools/player controls are not wired.
- Eking requires a prepared field, current permitted month, field-owned moisture threshold, fertility/weed limits and a unique seed item whose ownership/revision is valid. Harvest creates harvest items plus configured seed-return items and applies a stronger fertility cost for repeated crop cycles.
- crops.json defines Crop.Grain, Crop.Root and Crop.Legume; plant months, seed return and soil parameters are validated and registered in agriculture. These tuning values are placeholders, not balance-approved values.
- animals.json defines Animal.Rabbit, Animal.Bird and Animal.Goat; item IDs/intervals/tools/yields are checked and registered. The logical animal chain supports both unique-ledger and count-stack capture tools; pen construction consumes 4 Stake + 1 Rope; feed requires a unique ledger-owned feed item; products accrue for captured animals in a closed pen; slaughter yields come from animal data.
- Save schema v23 persists field cells and stops inventing seed history for legacy saves; schema v24 persists logical pens and migrates old animals as uncaptured. The fixed-step rollback now carries fields and pens.
- Tests in GameWorldTests.cpp now cover field-work retries, seasonal planting, item-backed seed consume/return, crop maturation, field snapshot, unique/count-stack capture, pen/material consumption, feed/product collection, slaughter idempotency and farm/pen snapshot. They are source-only and have not run.
- memory/PLAN_STATUS.md and CODEBASE.md now state that P6 is partial/source-written. Player input/HUD, actual WildlifeActor/stick-trap integration, field tools, full 15-step post-harvest chain, whole-year simulation and scene/PIE work remain open.

## Changes
- Branch: game/p06-farming-seed-cycle.
- Updated WorldAgricultureSubsystem.h/.cpp; WorldGameDataSubsystem.h/.cpp; Config/GameData/crops.json, animals.json, items.json; WorldSimulationSaveGame.h; WorldSaveCoordinatorSubsystem.cpp; WorldSimulationCoordinatorSubsystem.cpp; Tests/GameWorldTests.cpp; memory/PLAN_STATUS.md; memory/CODEBASE.md.
- No Unreal build, automation test or PIE run has been executed for this branch.

## Next step
Proceed with the next dependency-ordered backlog slice after P6: inspect P7.1–P7.4 source headers and contracts first, without building. Before the eventual batch build, re-audit all P6 receipt/save invariants and regenerate CODE_INDEX.md locally.
