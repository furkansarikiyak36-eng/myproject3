# Current task (checkpoint)

Status: open — P8.1 regional hazards + route effects, P8.2 signal-driven region raids + defense, and P8.3 fixed-step structural damage/snow collapse/material repair source-written; Unreal verification and final gameplay consumers pending
Overall goal: Finish partial tasks in memory/PLAN_STATUS.md in dependency order, including player input/HUD where required.
Current source-written work: P6/P7 logical/data contracts from ancestor branches plus 12 P8 regional hazards/chains, per-edge hazard route costs, fixed-step structural damage and snow-overload collapse, material-backed repair, signal-triggered region raids with snapshot receipts, regional defense mitigation and five P8 automation-test cases. P6/P7/P8 are all unbuilt on this work branch.
Branch: game/p08-events-defense (follow-up from game/p07-zone-entry-progression; P6/P7/P8 remain unbuilt on this work branch).

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
14. [x] Add `TryRecordZoneEntry`: validate `CanEnterZone`, resolve the zone discovery ID, fetch the current profile revision and record the discovery at shared world time with an idempotent operation ID. Extend test source for zone progression and retry.
15. [ ] Wire a real overlap/interaction trigger to `TryRecordZoneEntry`; connect route graph to actual movement speed/time and player input/HUD; connect infrastructure build/repair/weather-damage events to route state. Keep Zones 3/4 physical placement for the final scene phase.
16. [ ] Finish remaining field/animal player inputs and the full 15-step farm processing chain; add headless whole-world year simulation.
17. [x] Add P8 regional event data/evaluator, region-specific signal raids and snapshot receipts, snow-load support checks, regional defense mitigation, and source automation tests.
18. [ ] Batch UHT/UBT build and run the full automation suite; collect/fix errors before marking any task built/tested.
19. [ ] Wire real world signal samplers, event-driven route/survival/structural damage consumers, material-based repair and HUD/scene tags; then PIE/save-load acceptance.

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
- `zones.json`, `routes.json` and `discoveries.json` load transactionally. Five zones and item references validate; route data contains six anchors/12 directed edges for existing camp/creek/ford/lookout. Zones 3/4 are catalogued but not physically placed. `CanEnterZone` gates by season, required equipped capabilities and prerequisite discoveries. New `TryRecordZoneEntry` looks up the per-zone discovery ID, resolves the current player profile revision and records discovery at current shared world time through `TryRecordConfiguredDiscovery`; retries can reuse the same operation ID. The API is not yet called by a real volume/overlap/player interaction, and unlocked items/recipes/seeds are not enforced at consuming actions. Infrastructure IDs update route edges and built-state persists in schema v25. Physical build/repair/weather-damage events, route→movement speed/time, player input/HUD, WildlifeActor/stick-trap integration, field tools, full 15-step post-harvest chain, whole-year simulation and scene/PIE remain open.

## Changes
- Branch: game/p07-zone-entry-progression; parent implementation branch: game/p06-farming-seed-cycle.
- Updated WorldAgricultureSubsystem.h/.cpp; WorldGameDataSubsystem.h/.cpp; WorldLogisticsRouteSubsystem.h/.cpp; Config/GameData/crops.json, animals.json, items.json, zones.json, routes.json, discoveries.json; WorldSimulationSaveGame.h (schema v25); WorldSaveCoordinatorSubsystem.cpp; WorldSimulationCoordinatorSubsystem.cpp; LogSnapCharacter.h/.cpp; Tests/GameWorldTests.cpp; Tests/MyProject3CoreTests.cpp; memory/PLAN_STATUS.md; memory/CODEBASE.md; then created `game/p07-zone-entry-progression` and added the `TryRecordZoneEntry` API, zone→discovery references and test-source coverage.
- No Unreal build, automation test or PIE run has been executed for this branch.

## Next step
Run the agreed batch Unreal UHT/UBT build and automation tests for P6/P7/P8 together; this branch is source-written only. After fixing source/build/test failures, wire survival-condition and water/crop contamination consumers, fire spread/ignition, real storage/smell/light/smoke samplers, HUD/transition warnings, repair player/helper interaction, and authored region/defense/snow tags. P6/P7 player interactions, route-to-movement integration, unlock enforcement and the P9.2 station helper remain separate follow-ups. Regenerate `memory/CODE_INDEX.md` locally before the batch gate.


## P8 source-written changes (2026-10-30)
- Branch: `game/p08-events-defense`, based on `game/p07-zone-entry-progression`.
- Added `Config/GameData/events.json` and `WorldRegionalEventSubsystem.h/.cpp`: five region profiles, twelve hazards (including lightning and windstorm), condition checks, deterministic event windows and chain validation/evaluation.
- Extended the structural graph with region/snow/defense actor-tag metadata, climate/event-driven snow load, load-capacity support checks, regional defense rating, and save-restore validation that excludes dynamic environmental load when validating persisted graph shape. Region-tagged route edges now consume event cost multipliers.
- Extended the threat director and snapshot: region-targeted scheduled raids; normalized store-value/smell/light/smoke signal API; weighted threshold/cooldown; idempotent operation receipts; snapshot v2 with v1 migration.
- Extended raid adapter with regional defense-based raid damage reduction, fixed-step regional structure damage/snow-overload collapse and item-backed idempotent repair; coordinator invokes the regional hazard pass after each committed fixed step. Added five P8 automation-test source cases. Added `Docs/Architecture/P8_EVENTS_DEFENSE_CLOSEOUT.md` and updated plan/status/contracts.
- Status ceiling: code-written. Test source was added but not executed; no UHT/UBT, automation run, save/load or PIE evidence exists.
- Remaining: connect actual signal producers, survival-condition and water/crop contamination effects, fire spread/ignition, HUD/transition notifications, repair UI/helper use, and author region/defense/snow tags on placed actors.
