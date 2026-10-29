# Current task (checkpoint)

Status: open — P6 and P7.1–P7.4 logical/data contracts source-written; Unreal verification pending
Overall goal: Finish partial tasks in memory/PLAN_STATUS.md in dependency order, including player input/HUD where required.
Current source-written work: P6.1 field contracts, P6.2–P6.3 crop/seed lifecycle, P6.4 logical animal capture/pen/feeding/products/slaughter, and P7.1–P7.4 route context, five-zone catalog, discovery definitions, infrastructure edge updates and route-state persistence.
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
9. [x] Add P7.1 `ALogSnapCharacter::FindWalkingRoute`: query the existing graph with live `CarryLoad` mass limit/mass and initialized climate severity; add a test-source case for deterministic storm/slope/surface/infrastructure cost. This is a route query only; it does not drive movement.
10. [x] Add `zones.json` with five zone IDs/resources/barriers/solution IDs/seasonal months; add `routes.json` for the current camp/creek/ford/lookout anchors; validate route node/edge references and apply the directed graph from game data.
11. [x] Add `discoveries.json`, validate prerequisite order, required equipment and unlock IDs, then register definitions into survival; add source tests for discovery prerequisite/unlock and bridge route-cost change.
12. [x] Add stable infrastructure IDs to route edges, `SetInfrastructureBuilt`, deterministic route graph snapshot/restore and save schema v25 migration.
13. [ ] Batch Unreal UHT/UBT build and automation tests, then fix all errors. No build/test has run on this branch.
14. [ ] Connect graph nodes to actual world actors, route query to movement speed/time and player input/HUD; connect zone entry and infrastructure build/repair/weather-damage events to discovery and route state. Keep Zones 3/4 physical placement for the final scene phase.
15. [ ] Finish remaining field/animal player inputs and the full 15-step farm processing chain; add headless whole-world year simulation.
16. [ ] Continue P8/P9.2 in dependency order; New Valley scene and PIE acceptance remain last.

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
- Ekim requires a prepared field, current permitted month, field-owned moisture threshold, fertility/weed limits and a unique seed item whose ownership/revision is valid. Harvest creates harvest items plus configured seed-return items and applies a stronger fertility cost for repeated crop cycles.
- crops.json defines Crop.Grain, Crop.Root and Crop.Legume; plant months, seed return and soil parameters are validated and registered in agriculture. These tuning values are placeholders, not balance-approved values.
- animals.json defines Animal.Rabbit, Animal.Bird and Animal.Goat; item IDs/intervals/tools/yields are checked and registered. The logical animal chain supports both unique-ledger and count-stack capture tools; pen construction consumes 4 Stake + 1 Rope; feed requires a unique ledger-owned feed item; products accrue for captured animals in a closed pen; slaughter yields come from animal data.
- Save schema v23 persists field cells, v24 persists logical animal pens/capture/slaughter state, and v25 persists route nodes/edges with built-infrastructure state. v22→v23→v24→v25 migration is source-written; v24→v25 retains the current authored route graph as the baseline rather than inventing old infrastructure history. Fixed-step rollback includes fields and pens; route graph is not mutated by normal fixed steps.
- Tests in GameWorldTests.cpp now cover field-work retries, seasonal planting, item-backed seed consume/return, crop maturation, field snapshot, unique/count-stack capture, pen/material consumption, feed/product collection, slaughter idempotency and farm/pen snapshot. They are source-only and have not run.
- `zones.json`, `routes.json` and `discoveries.json` are now loaded transactionally. Five zones and all item references validate; route data contains six anchors/12 directed edges for the currently existing camp/creek/ford/lookout. Zones 3/4 are catalogued but not physically placed. Discovery definitions are loaded and registered with ordered prerequisites/equipment gates/unlocks, but they are not yet triggered by real player zone entry and the unlocked items/recipes/seeds are not enforced at their consuming actions. Infrastructure IDs can update route-cost edges and this state is saved in schema v25. Physical build/repair/weather-damage events, route→movement speed/time, player input/HUD, actual WildlifeActor/stick-trap integration, field tools, full 15-step post-harvest chain, whole-year simulation and scene/PIE work remain open.

## Changes
- Branch: game/p06-farming-seed-cycle.
- Updated WorldAgricultureSubsystem.h/.cpp; WorldGameDataSubsystem.h/.cpp; WorldLogisticsRouteSubsystem.h/.cpp; Config/GameData/crops.json, animals.json, items.json, zones.json, routes.json, discoveries.json; WorldSimulationSaveGame.h (schema v25); WorldSaveCoordinatorSubsystem.cpp; WorldSimulationCoordinatorSubsystem.cpp; LogSnapCharacter.h/.cpp; Tests/GameWorldTests.cpp; Tests/MyProject3CoreTests.cpp; memory/PLAN_STATUS.md; memory/CODEBASE.md.
- No Unreal build, automation test or PIE run has been executed for this branch.

## Next step
Next, implement the missing P7 interactions without scene placement: route graph ↔ actual walking speed/time and authored edge state ↔ build/repair/damage events; then connect zone entry to `TryRecordConfiguredDiscovery` and enforce `IsDefinitionUnlocked` at recipe/seed/action execution. Continue to avoid builds until the batch gate. Before the eventual batch build, re-audit P6/P7 snapshot invariants and regenerate CODE_INDEX.md locally. Before the eventual batch build, re-audit all P6 receipt/save invariants and regenerate CODE_INDEX.md locally.
