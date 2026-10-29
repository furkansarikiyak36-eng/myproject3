# Current task (checkpoint)

**Every assistant reads this file FIRST.** If the status below is `open`, continue from
"Next step". Do **not** analyse again what "Findings" already says.

## Rules (the limit can end the session at any moment)

1. New task from the user: replace the section below (status `open`), write the goal and plan,
   save the file **before** reading any code.
2. After **every** step (one file read, one search, one edit): add 1–3 lines to "Findings" or
   "Changes", update "Next step", save. Small and often; never keep results only in the chat.
3. Read only what `memory/CODEBASE.md` points to. Write down in "Read" each file you opened, so
   nobody opens it again for the same question.
4. Finished: status `done`; move lasting facts to `CODEBASE.md` (Findings) and the session file;
   clear this file back to the empty template at the next new task.
5. Cannot write to the repo (read-only GitHub connector)? End **every** answer with the updated
   block of this file so the user can paste or commit it.

---

## Task

Status: `open`
Goal: Close the remaining partial tasks listed in `memory/PLAN_STATUS.md` on `game/p00-adopt-group-a`, prioritizing backend completeness and necessary player-facing interactions, then record exact status. This plan has 22 partial rows; P10 is frozen and must be skipped.
Asked by the user: "bu kısmileri bitirelim"; continued after "devam" (2026-10-29)
Started: 2026-10-29 · Last saved: 2026-10-29 · Branch: `game/p00-adopt-group-a`

### Plan (write code and tests; do not build per small task; status ceiling is code-written)

0. [x] Update checkpoint before source reads; scope from current `PLAN_STATUS.md`, not stale count of 27.
1. [ ] P0.5 + P2.1–P2.5: remove hard-coded demo recipe; complete water, fatigue action effects, wetness/cold triggers, illness/injury triggers; check ledger item ownership.
2. [ ] P3.1–P3.2 + P4.1/P4.3/P4.4: fuel wetness and fire interaction, enclosure heat-loss gaps, resource placement hooks, station interaction, tool-item strength links.
3. [ ] P6.1–P6.4: field-cell states, calendar-gated planting, crop steps, soil exhaustion/seed return, animal capture/pen/slaughter chain; register definitions from data.
4. [ ] P7.1–P7.4: player route cost, populated zones, physical discovery placement and unlock gates, missing primitive infrastructure and weather maintenance.
5. [ ] P8.2–P8.3: raid signals, snow load/avalanche/fire spread, material-based repair.
6. [ ] P9.2 + Save: helper station usage and persistence of actor transforms/system state.
7. [ ] P10 frozen: skip; do not extend physical snap kernel.
8. [ ] Add/update targeted automation tests and headless whole-world year/save-load test; no build after each step.
9. [ ] Update `PLAN_STATUS.md`, `CODEBASE.md`, this checkpoint and roadmap truthfully; do not claim compile/PIE verified.
10. [ ] When edits are done, prepare batch build/fix, then scene work last.

### User constraints
- No build after each task; write code/tests and batch build/fix at the end (`memory/ROADMAP.md`).
- Backend first; scene last; include a headless whole-world year test.
- P10 is frozen; skip it.
- Never guess Unreal API names; use headers read in this session.
- After every repository step, update Findings/Changes and Next step here.

### Read
- `Source/MyProject3/HeatSourceComponent.h/.cpp` (fuel definitions, BeginPlay identity, TryAddFuelItem, Ignite/GetHeatAt reviewed).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` (existing-source fuel application in ApplyToWorld added; `ApplyToHeatSource` materializes item fuel records).
- `Source/MyProject3/WorldItemOwnershipSubsystem.cpp` (`TryUpdateItemDefinition` supports world-owner wet/dry transitions with receipt and stable base definition).
- `Source/MyProject3/WorldSurvivalSubsystem.cpp` (world-owned fuel transitions added to shared survival step).
- `Source/MyProject3/VerticalSliceCWorldActor.h/.cpp` (I creek-fill and O refuel/ignite inputs present).
- `Source/MyProject3/Tests/MyProject3CoreTests.cpp` (current blob refreshed after a wet-fuel test conflict; wet/dry test still absent).
- `Source/MyProject3/WorldSurvivalSubsystem.h` (P2 API declarations: profile/location/activity/owned-item consume/rest/condition/weather/snapshots).
- `Source/MyProject3/VerticalSliceCWorldActor.h` (scene has `IsInCreek`, `PlayerSurvivalId`, `SetupPlayer`, `ChopInput`, `HelperOrderInput`, and its own day/night timer).
- `Source/MyProject3/VerticalSliceCWorldActor.cpp` (Tick drives separate time/weather and updates survival location; only LMB chop and J helper inputs are bound; `IsInCreek` currently only classifies water for log drift).
- `Source/MyProject3/LogSnapCharacter.cpp` (WASD movement is polled in Tick; no survival action-cost or fatigue speed integration exists).
- `Source/MyProject3/ShelterSensorComponent.h` (`SurvivalEntityId` is public and can bridge the player character to the survival profile).
- `Source/MyProject3/LogSnapCharacter.cpp` (constructor sets base MaxWalkSpeed 420 cm/s; Tick polls WASD and has no existing survival activity call).
- `Source/MyProject3/LogSnapCharacter.h` (added base movement speed and survival activity accumulator for non-compounding fatigue modifiers).
- `Source/MyProject3/WorldSurvivalSubsystem.cpp` (`RecordActivity` adds calorie burn/stamina cost, now also gates exhausted actions; survival step models rain/storm wetness and warmth/shelter drying; rest uses shared clock).
- `Config/GameData/conditions.json` (existing `Illness.Cold`, `Illness.DirtyWater`, `Injury.Fall`, and `Injury.AnimalBite` definitions are available for environment triggers).
- `Source/MyProject3/WorldSurvivalSubsystem.h` (declared public environmental `ApplyWetnessExposure`).
- `Source/MyProject3/WorldSurvivalSubsystem.cpp` (bounded additive environmental wetness; wet-cold illness requires Wetness≥0.8, felt temperature sufficiently cold, and core temperature≤36°C).
- `Source/MyProject3/Tests/MyProject3CoreTests.cpp` (added fall-impact and hostile-bite automation tests; both are authored/unrun).
- `memory/PLAN_STATUS.md` (P2.5 now lists code-written hard-fall and hostile-bite triggers/tests; P0.4 inventory backfill and I/O/J bindings noted).
- `memory/ROADMAP.md` (added a P2.1–P2.5 code-written progress entry for player inventory/data backfill, water fill/schema v26, fatigue/chop, creek wet-cold and fall/bite triggers; listed test commits explicitly as unbuilt/unrun).
- `Source/MyProject3/VerticalSliceCWorldActor.cpp` (player location update now detects `IsInCreek` and adds 0.12 wetness each survival update while wading).
- `memory/CODE_INDEX.md` (current symbol map read for player and survival entry-point discovery; index is stale for newly added agriculture symbols).
- `Source/MyProject3/LogSnapCharacter.h` (input bindings/movement actions: MoveForward/Backward/Right/Left, Tick, Jump and actor components).
- `Source/MyProject3/WorldSurvivalSubsystem.h` (current action, profile, condition, consumable, weather, rest and snapshot API declarations).
- `memory/ROADMAP.md` (current task dependencies, phase descriptions and progress log read for next P2 batch).
- `memory/CURRENT_TASK.md` (checkpoint only).
- `memory/CODEBASE.md` (map; older portions describe `feature/simple-intent`, so verify the current branch status file before trusting those rows).
- `memory/CODE_INDEX.md` (raw unit index; used to identify code file names).
- `Source/MyProject3/WorldSurvivalSubsystem.h` (current declarations for survival, consumables, conditions, equipment, actions, and profile).
- `Source/MyProject3/WorldDemoScenarioActor.cpp` (scenario initialization validates and registers a `ConfiguredRecipe` property).
- `Source/MyProject3/WorldDemoScenarioActor.h` (recipe is an editor-editable `FWorldProductionRecipe` property, not hard-coded ingredient literals).
- `Source/MyProject3/WorldGameDataSubsystem.h` (data schemas: transformations, stations, crops; APIs `ApplyToWorld`, `ApplyToInventory`, `FindTransformation`).
- `Source/MyProject3/InventoryCraftingComponent.h` (inventory ownership, recipe registration, item/tool checks).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` (JSON validation and data→system registration paths inspected).
- `Source/MyProject3/WorldSurvivalSubsystem.cpp` (survival item transactions, conditions, exposure/rest, and simulation-step functions inspected by symbol).
- `Source/MyProject3/Tests/MyProject3CoreTests.cpp` (added `MyProject3.Survival.WorldFirewoodGetsWetInRainAndDriesNearLitFire`: rain wets World-owner fuel only; clear weather + lit nearby heat dries only in-range fuel; inventory fuel stays dry; base definition + revisions asserted. Source-only/unrun).
- `memory/PLAN_STATUS.md` (P3.1 records data-driven fuel rates, O-key refuel, world-only rain wetting, lit-fire drying and authored unrun test; roof coverage/visual verification remain gaps).
- `memory/ROADMAP.md` (added P3.1 code-written evidence line, explicit test name/commit and unbuilt/unrun caveat).
- `memory/CODEBASE.md` (updated weather row to climate subsystem backend and Fire/Shelter row to reflect owned refuel receipts, rain wetting/drying and remaining cover/visual gaps).
- `Source/MyProject3/VerticalSliceCWorldActor.h/.cpp` (I=FillCreekWater and O=RefuelFire are already bound; fill requires `IsInCreek`, a pawn inventory, and a unique ledger-owned `Pot.Fired`).
- `Source/MyProject3/LogSnapCharacter.cpp` (now creates one canonical `UInventoryCraftingComponent` default subobject adjacent to carry/build components).
- `Source/MyProject3/WorldDemoBootstrapActor.cpp` (bootstrap calls `LoadDefault` + `ApplyToWorld` before coordinator initialization, but actor BeginPlay order can leave player inventory already initialized).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` (`ApplyToWorld` does not backfill existing inventories).
- `Source/MyProject3/InventoryCraftingComponent.cpp` (`RegisterRecipe` rejects duplicate IDs; `HasCapacityFor` stacks only non-owned items, while a uniquely owned water item always needs a new slot).
- `Source/MyProject3/InventoryCraftingTypes.h` (recipe contract is `RecipeId`, `Ingredients` with `ItemId/Quantity/bConsume`, `Output`, `Byproducts`, `bEnabled`; stacks have item/quantity/category/max-stack/tool/owned-identity fields).
- GitHub code search found no canonical player inventory attachment.
- `Source/MyProject3/LogSnapCharacter.h` now forward-declares `UInventoryCraftingComponent` and exposes the intended canonical `Inventory` default-subobject property.
- `Source/MyProject3/WorldProductionSubsystem.h` (current recipe/station record shapes and production APIs).
- `Source/MyProject3/WorldDemoScenarioActor.cpp` (edited: scenario recipe selection resolves from game data).
- `Source/MyProject3/WorldDemoScenarioActor.h` (edited: replaced editable recipe struct with a `FName` ID).
- `Source/MyProject3/WorldDemoScenarioActor.cpp` (edited: all production-recipe lookups use `ProductionRecipeId`; recipe payload comes from transformations data).
- `memory/PLAN_STATUS.md` (P0.5 updated to Var; aggregate counts corrected from 9/22/12 to 10/21/12).
- `Source/MyProject3/WorldAgricultureSubsystem.h` (current crop/animal/action receipts, API, and save-state definitions).
- `Config/GameData/crops.json` (authoritative crop IDs, seed/harvest item IDs, stage durations, planting months and discovery zones).
- `Source/MyProject3/WorldAgricultureSubsystem.cpp` (current planting, harvesting, growth, animal transactions, snapshots inspected by symbol).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` lines 360–470 (world application path; no agriculture/crop registration exists).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` lines 1–45 (include/dependency layout checked before adding agriculture integration).
- `Source/MyProject3/WorldAgricultureSubsystem.h` (edited: crop definitions carry plant-month restrictions and zone metadata).
- `Source/MyProject3/WorldAgricultureSubsystem.cpp` (edited: validates unique months and calendar-gates planting).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` (edited: registers data-defined crop definitions with planting months and zone IDs).
- `Source/MyProject3/WorldAgricultureSubsystem.h` (added persisted field cells/actions and snapshot overloads).
- `Source/MyProject3/WorldAgricultureSubsystem.cpp` (implemented idempotent field actions, prepared planting, calendar gates, growth modifiers, soil exhaustion, and snapshot overloads; fixed harvest revision preflight).
- `Source/MyProject3/WorldSimulationSaveGame.h` (edited: `CurrentSchemaVersion` advanced to 24; crop arrays serialize new seed fields with default legacy values).
- `Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp` (v23→v24 crop seed defaults are explicit).
- `Source/MyProject3/Tests/MyProject3CoreTests.cpp` (added crop seed-return assertions and switched farming snapshot test to three arrays).
- Re-read `Source/MyProject3/WorldAgricultureSubsystem.cpp` seed-related save/restore invariants by symbol after adding the regression test.
- `Source/MyProject3/Tests/MyProject3CoreTests.cpp` (edited farming test preps plot and added field action/snapshot regression test).
- `Source/MyProject3/WorldGameDataSubsystem.h` (added `FGameDataCrop.SeedReturnUnits`).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` (parses and bounds seed-return units).
- `Config/GameData/crops.json` (all three placeholder crops declare `seed_return_units: 1`).
- `Source/MyProject3/WorldAgricultureSubsystem.h` (crop definition has configurable seed return and crop state persists its fixed count).
- `Source/MyProject3/WorldGameDataSubsystem.cpp` (passes configured seed return into crop definitions).
- `Source/MyProject3/WorldAgricultureSubsystem.cpp` lines 350–442 (planting and feed paths re-read).
- `Source/MyProject3/WorldAgricultureSubsystem.cpp` lines 443–530 (harvest output construction and crop state read).
- `Source/MyProject3/WorldAgricultureSubsystem.h` (crop state stores seed definition and seed-return count).
- `Source/MyProject3/WorldAgricultureSubsystem.cpp` (captures seed definition/count at planting; harvest emits produce+seeds as one batch; validators use total outputs).
- `Source/MyProject3/WildlifeActor.h` (in-world wildlife enum, movable actor and private death handler reviewed).
- `Source/MyProject3/WildlifeActor.cpp` (current spawning/tick/death behavior inspected).
- `Source/MyProject3/WorldItemOwnershipSubsystem.h` (confirmed exact owned-item fields and owner kinds).
- `Source/MyProject3/WorldItemOwnershipSubsystem.cpp` (`RegisterItem` requires an inventory owner ID, finite timestamps/location and capacity; fills `BaseDefinitionId`).
- `Source/MyProject3/LogSnapCharacter.h` (fall tracking fields and `ApplyFallImpact` entry point).
- `Source/MyProject3/LogSnapCharacter.cpp` (tracks peak downward velocity while falling; hard impacts ≥850 cm/s deal bounded physical damage and attempt receipt-backed `Injury.Fall` through the shelter sensor profile).
- `Source/MyProject3/WildlifeActor.h` (added bite cooldown timer).
- `Source/MyProject3/WildlifeActor.cpp` (Enemy now delegates near-range Tick to shared `TryBiteTarget`; the method applies 8 physical damage, attempts `Injury.AnimalBite`, and sets a 3-second cooldown; physical damage still occurs if the profile is unavailable).
- `Source/MyProject3/PhysicalHealthComponent.h/.cpp` (physical hits reduce local health, launch characters and broadcast damage/death; no bridge to survival conditions exists).
- `Source/MyProject3/WildlifeActor.h/.cpp` (Enemy already approaches the player and stops at ~150 cm, but there is no bite cooldown or attack/damage action; SmallAnimal capture state is present).
- Re-read current agriculture implementation and coordinator lines 635–650 to check field validation/capture wiring after edits.
- `Source/MyProject3/WorldCalendar.h` (calendar configuration/date struct and season enum).
- `Source/MyProject3/WorldSimulationClockSubsystem.h` (current shared clock and `GetCalendarDate` query contract).
- `Source/MyProject3/InventoryCraftingComponent.cpp` (recipe registration/crafting and item ownership implementations inspected by symbol).

### Findings
- Current status source lists 22 partial rows (not 27); P10 is explicitly frozen.
- P0.5 is now data-authoritative, and P6 field prep/calendar/growth/seed return/schema v24 plus animal capture/move/slaughter APIs have been committed as code. These edits are not compiled or PIE verified.
- Agriculture CPP implements capture, pen registration/move and slaughter; snapshot and coordinator wiring remains. Save schema advanced to v25 and now declares the `AnimalPens` array.
- Snapshot APIs in the header now advertise four arrays, while CPP has not yet implemented the four-array overload. This interface/implementation mismatch must be corrected before build.
- P0.5 is changed to use data transformation IDs; agriculture now has crop-data registration, calendar gates, field-cell actions/simulation, field save schema v23, and crop seed-return outputs. None of these recent changes have been built or run in Unreal.
- The migration chain now normalizes v22→v23 empty field cells and v23→v24 crop seed fields. Existing item-backed agriculture test already exercises seed consumption, crop progression, animal feeding and collection; it is the right place to add a harvest seed-return assertion.
- Field snapshot state is wired through save coordinator; the old two-array agriculture snapshot overload remains for source/API compatibility while coordinator uses the three-array overload.
- `WorldDemoScenarioActor.h` confirms recipe values are editor-authored `ConfiguredRecipe`, so P0.5 should be resolved by making the game-data loader authoritative and not silently registering a parallel demo recipe.
- `WorldGameDataSubsystem.cpp` already converts `transformations.json` to production and hand recipes. Scenario actor no longer authors a second production recipe. `PLAN_STATUS.md` now reflects this source correction; compile/PIE remain unverified.
- `InventoryCraftingComponent.cpp` confirms unique physical items are consumed through the ownership ledger and non-consuming tools remain in inventory. Existing survival methods already implement owned-item consumption, conditions/treatment, activity stamina cost, wetness, rest quality, and temperature exposure; remaining gaps include environment/player triggers and UI/world wiring, not those bases.
- Current production API can be checked against `FWorldProductionRecipe` and `FWorldProductionStation`; `WorldGameDataSubsystem` converts transformation JSON to these types, while the demo actor has an optional second editable recipe path. Use the exact fields from this header if validating it against data.
- `CODEBASE.md` is a legacy map of `feature/simple-intent`; current branch contains subsequent adoption/fixes. Current roadmap reports 102/102 tests at `bbc57b3`, while older status/memory references 85 tests. Do not conflate these snapshots.
- Working branch is `game/p00-adopt-group-a`; roadmap status ceiling is `code-written` until batch validation.

### Changes
- Replaced stale checkpoint for the current request before gameplay source reads.
- Recorded current codebase map limitations and newer roadmap test-count reference.
- P0.5 source-of-truth change committed; status ledger corrected.
- Seed return is JSON-configured, captured at planting, emitted as ledger-owned world items at harvest, and included in receipt/restore validation. Schema v24 migration avoids inventing seeds for legacy saves.
- `AWildlifeActor` is a lightweight wandering actor with `SmallAnimal/Enemy/Plant`; death is private and currently only changes actor presentation/physics. It has no capture or pen state.
- The item ledger is item-centric; live animals should remain domain entities, not be converted to generic item records. A stable wildlife actor identity can bind capture receipts to a particular source actor.
- The item-backed farming test now drives a crop to maturity and asserts produce+seed identity/count, single-commit retry, and three-array agriculture snapshot restoration. Not built/run yet.
- P6 work added data-defined month/zone/seed return, field operations and simulation, SaveGame field persistence, migration v22→v23, plus field preparation/snapshot automation coverage authored.

### Next step
Inspect existing `UWorldGameDataSubsystem` automation fixture and `FScopedTestWorld` setup. Add `AProductionStationActor` regression using data-loaded `workbench_log` + `make_plank`: generic inventory stacks for HalfLog/Wedge/Chisel, deposit required quantities, start job, advance production, collect, take plank and verify returned tool categories. Then update P4.3 status and docs.
