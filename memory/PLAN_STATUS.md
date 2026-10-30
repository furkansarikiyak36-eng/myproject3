# Plan status: what exists, what is partial, what is missing

Checked 2026-10-30. Baseline changes remain on `game/p00-adopt-group-a`; P6/P7 source changes are ancestors of `game/p08-events-defense` through `game/p07-zone-entry-progression`. P8 regional event/raid/snow-load/repair changes are source-written on `game/p08-events-defense`. None of the P6/P7/P8 work-branch changes has been compiled or tested in Unreal. Task ids are from `ROADMAP.md`.
Update this file whenever a task changes state.

- **Var / exists**: code covers the task's behaviour (backend). Still not played in PIE.
- **Kısmi / partial**: a base exists; the "Missing" column is still to do.
- **Yok / missing**: no code.
- `[A]` = came from group-a, `[S]` = from our branch. Both exist on `game/p00-adopt-group-a`.
- **Player** = input and HUD so the player can use it. Almost everything is missing this.

## Summary

| Status | Count | Tasks |
|---|---:|---|
| Var | 9 | P0.3, P0.4, P1.1, P1.2, P2.4, P2.6, P4.2, P5.1, P5.3 |
| Kısmi | 25 | P0.5, P1.3, P2.1, P2.2, P2.3, P2.5, P3.1, P3.2, P4.1, P4.3, P4.4, P6.1, P6.2, P6.3, P6.4, P7.1, P7.2, P7.3, P7.4, P8.1, P8.2, P8.3, P9.2, P10, Save |
| Yok | 9 | P0.2, P1.4, P3.3, P3.4, P5.2, P9.1, P9.3, P11, P12 |

Player input/HUD: missing for most systems except carrying/cutting/building logs [S] and the dig tool on N [A]. The new P6 source contracts below are not wired to player input/HUD or proven in PIE.

## Per task

| Task | Status | Exists | Missing |
|---|---|---|---|
| P0.2 freeze note | Yok | — | note in Placement/CLAUDE.md, carry.md |
| P0.3 data files | Var | `Config/GameData/`: materials, items (90), transformations (59 rows = the 38 plan steps), stations (10), conditions, crops (3 tunable definitions), animals (3 tunable definitions), calendar, climate, layers | user review of tuning/lists; events.json contains the 12 P8 regional hazards; loaded by UWorldRegionalEventSubsystem |
| P0.4 data loader | Var | `UWorldGameDataSubsystem`: loads + checks references; applies production, survival, climate, calendar, terrain, 3 crop definitions and 3 animal definitions; inventories and fires pick up hand recipes / fuels at BeginPlay; bootstrap loads it | source-level checks exist; current P6 branch additions have not been built/run; VerticalSlice_C does not use the bootstrap yet (P1.4) |
| P0.5 recipes from data | Kısmi | all game recipes come from data | the old demo recipe in `WorldDemoScenarioActor.cpp` is still in C++ |
| P1.1 calendar | Var | `WorldCalendar` + clock `GetCalendarDate`: year, month, day, hour, season from saved time; 5 days/month, start March 5 08:00; 3 tests | config from data file (P0.3), HUD (P1.4) |
| P1.2 climate by month | Var | `UWorldClimateSubsystem`: 12-month table, daily weather from clock seed, 05:00/15:00 temperature curve, snow depth, ground wetness, pushes weather + air temperature to survival; in the coordinator step + rollback; save schema v21; 3 tests | VerticalSlice_C does not run the coordinator yet (own hours, own weather) -> P1.4 |
| P1.3 event director | Kısmi | P8 adds 12 deterministic, data-driven regional hazards (flood, frost, hail, drought, lightning, wildfire, ash/toxic rain, epidemic, snowstorm, avalanche, windstorm, fog); chains, per-edge route costs and fixed-step structural damage are source-wired [S] | survival-condition and water/crop contamination consumers, hazard start/end notifications, fire spread, HUD/warnings and scene bootstrap/tagging |
| P1.4 date/signals on screen | Yok | — | HUD date/season, visible signs |
| P2.1 water | Kısmi | thirst value [S]; consuming an item gives thirst relief [A]; dirty vs boiled water as consumable definitions, dirty causes illness (test) | drinking from the creek (interaction), water container item, boiling recipe (P4) |
| P2.2 sleep/fatigue | Kısmi | fatigue value [S]; rest on the shared clock [A]; rest quality from shelter, warmth, dry clothes (`GetRestQuality`, test) | fatigue limiting actions (slower work, no sprint) |
| P2.3 wetness | Kısmi | profile `Wetness`: rain/storm wets by severity x (1-shelter) x (1-clothing water resistance), dries faster with shelter and warmth; wet penalty in felt temperature [S/A] (was wrongly listed as missing) | wading in the creek, drying at a fire explicitly, long wet cold -> illness |
| P2.4 spoilage | Var | freshness duration, spoiled energy [A]; spoilage age accrues per owner: storage `SpoilageRateMultiplier` (cellar, smoke rack) x air-temperature factor from climate; schema v22; test. Smoked/dried food = other item definitions with longer freshness (recipes, P4) | the temperature of a segment is read when it closes (approximation); damp not modelled |
| P2.5 disease/injury | Kısmi | conditions Injury/Illness/Poisoning, duration, damage, treatment item [A]; physical health [S]; triggers: dirty water (`ConditionOnConsumeId`), spoiled food (`ConditionWhenSpoiledId`) | triggers: toxic rain (P8), falls, animal attacks, long wet cold (P2.3) |
| P2.6 clothing | Var [A] | slots Head/Body/Hands/Legs/Feet, insulation, water resistance, capabilities | (hide clothing recipe is P4) |
| P3.1 fire fuel | Kısmi | fuel items with burn time and receipts [A]; per-fuel `HeatOutputMultiplier` mixed by burn time (wet wood 0.6, charcoal 1.5), `IsBurningHot` for firing clay; saved and rolled back; test | firewood turning wet in rain (item state), player input on the fire |
| P3.2 shelter enclosure | Kısmi | roof/wall cover by line traces, counting only structurally placed pieces (loose logs no longer count) [S]; a closed shelter keeps fire heat (`ShelterHeatRetention`, test) | per-gap heat loss from the graph; plaster closing gaps (P3.3); the sensor change has no automated test (needs placed log actors) |
| P3.3 clay plaster | Yok | — | plaster item, wall state, closes gaps |
| P3.4 charcoal pit | Yok | generic production station [S] | the station and recipe |
| P4.1 gatherables | Kısmi | trees, logs, sticks, stones [S]; `AGatherableResourceActor`: any data item, tool rule, units that grow back on the world clock, food as ledger items (test) | placing the spots in VerticalSlice_C per zone; bone/hide/antler come from hunting (P6.4); player input |
| P4.2 item transformations | Var | every E transformation is data: hand ones become inventory crafting recipes, station ones production recipes (station tools go in and come back) | hand-crafted items are plain stacks, not ledger items (eating needs ledger items) |
| P4.3 stations | Kısmi | `AProductionStationActor` for every station in stations.json: deposit, start (fire / charcoal-hot fire rule), collect, take; `TryTakeStock` in production; kiln test | station meshes and placing them; the fire is checked only at the start of a job; player input |
| P4.4 tool requirements | Kısmi | hand recipes need their tools (not used up; CanCraft now checks them, it ignored them before); station recipes need the tool in the station | the log axe and the dig tool strength are not tied to tool items yet |
| P5.1 layers → items | Var | yield rules [A] filled from layers.json (soil, clay, sand, stone, flint, ochre; layers 0-3); clay digs out only in its layer (test) | which material a voxel cell holds is still chosen by the caller (no per-location geology map) |
| P5.2 cellar, trench, collapse | Yok | dig refused near grounded structures [A] | collapse of unsupported rooms; cellar/trench/cistern/embankment |
| P5.3 dig limits + save | Var [A] | bounded dig, soil classes, tool strength, receipts, density save | chunk seams, neighbour chunks (later) |
| P6.1 field cell state | Kısmi | `FWorldFieldCellState`: cleared, tilled, fertility, weed pressure, mulch coverage, moisture, live crop link, last-crop/repeat counter, revision and field-operation receipts; stored by agriculture and persisted in save schema v23 [code-written] | player inputs/tool gating and field visuals, fence cells, rainfall/moisture integration, yearly simulation and Unreal verification |
| P6.2 15 field steps | Kısmi | Clear/Till/Fertilize/Weed/Mulch/Water logical actions; planting requires prepared cell + permitted month + owned seed; harvest creates item outputs and records retry receipts [code-written] | full 15-step chain: fence, drying, threshing, winnowing, grinding, storing, physical tools, player input/HUD and Unreal verification |
| P6.3 seeds, exhaustion, 3 crops | Kısmi | Grain/Root/Legume definitions are registered from crops.json with validated plant months; harvest returns configured seed items; soil fertility decreases with a stronger repeated-crop penalty [code-written] | crop/season tuning is provisional, soil recovery/beneficial rotation and a full-year balance run remain; no Unreal verification |
| P6.4 animal chain | Kısmi | data-driven Rabbit/Bird/Goat definitions; unique-item or count-stack capture; 4-stake + 1-rope logical pen, gate/assignment, owned-item feeding, closed-pen products and slaughter yields; pen/capture state persisted in schema v24 [code-written] | integrate with WildlifeActor/stick-trap/player interactions, physical pen/animal visuals, pickup/UI and input; feeding currently requires a unique owned feed item; Unreal verification and whole-year simulation remain |
| P7.1 load, slope, surface | Kısmi | carry mass slows the player [S]; route costs include slope/surface/weather/load/infrastructure [A]; `ALogSnapCharacter::FindWalkingRoute` uses live carry mass + climate severity [code-written]; route nodes/edges are now loaded from routes.json and edge costs respond to infrastructure state | populate route anchors from actual scene actors, connect route choice to player input/HUD, apply slope/surface cost to actual walking speed/time, and run Unreal verification |
| P7.2 zones | Kısmi | zones.json validates zones 0–4, resources, barriers, permanent solutions, per-zone discovery IDs, prerequisite discoveries and seasonal/capability gates; routes.json adds camp/creek/ford/lookout anchors for existing VerticalSlice_C [code-written] | zone 3/4 physical placement, real zone volumes/actor membership and overlap/input wiring remain deferred to world/interaction integration |
| P7.3 rewards | Kısmi | discoveries.json loads prerequisites, required equipment and unlock IDs; loader registers definitions; `TryRecordZoneEntry` validates access and records the configured zone discovery at shared world time with an idempotent operation ID [code-written] | connect a real zone/interaction trigger to `TryRecordZoneEntry`; place discoveries in the world and enforce recipe/seed/action gating with `IsDefinitionUnlocked` |
| P7.4 infrastructure | Kısmi | route edges carry stable infrastructure IDs; SetInfrastructureBuilt updates matching directional edges; bridge/ford cost change has test source; route graph/infrastructure state is persisted in schema v25 [code-written] | connect actual construction/damage/repair events to route edges; trail markers, path, ladder, raft, forward camp, maintenance and weather damage remain |
| P8.1 natural events + chains | Kısmi | `events.json` + `UWorldRegionalEventSubsystem`: 12 validated hazards, seeded regional windows and flood→epidemic, drought→lightning→wildfire→toxic-rain, snowstorm→avalanche chains; region-tagged route edges consume live event cost multipliers [code-written] | player warnings/start-end notifications, survival-condition triggers, ash/water/crop contamination, fire spread and authored scene signals remain |
| P8.2 signal-driven raids | Kısmi | adapter scans region-tagged actors each committed fixed step for `RaidSignal.Storage/Smell/Light/Smoke:<0..1>` tags; aggregates/saturates values and submits per-region/tick idempotent samples; director applies threshold/cooldown/region target; snapshot v2 receipts; supported regional defenses reduce raid pulse damage [code-written] | connect tags to live storage/light/smoke/scent source values, author actor tags/targets in the level, then validate raid materialization/save-load in Unreal |
| P8.3 damage, repair, snow, avalanche, fire | Kısmi | structural graph derives live snow load and invalidates over-capacity support; the coordinator runs post-commit regional structural damage/collapse each fixed step; `TryRepairStructureWithMaterial` consumes an owned Resource/BuildingPart through ownership + operation ledgers; regional defense tags mitigate raid damage [code-written] | fire spread/ignition, item/material tuning, helper/player repair UI, scene tags and Unreal collapse/save-load verification remain |
| P9.1 time budget tuning | Yok | — | balance pass |
| P9.2 helper uses stations | Kısmi | helper follows, collects, piles, builds plans, maintains walls [S] | using stations and new materials |
| P9.3 one full year played | Yok | — | — |
| P10 physical assembly | Kısmi | log family: cuts, split, hew, plank, snap kernel [S]; stone pieces with anchors [S]; stake by sharpening [S]; incremental cut progress [A] | thatch bundle, cut stone block making, notches/sockets, mitre cuts, crossing diagonals |
| P11 story | Yok | — | — |
| P12 polish and demo | Yok | — | — |
| Save (all phases) | Kısmi | existing save manifest plus farm field cells (schema v23), logical animal pens/capture/slaughter (schema v24), route nodes/edges and built-infrastructure state (schema v25); v22→v23→v24→v25 migration path is source-written | batch Unreal serialization/build/test and all phase systems must be verified; legacy data is not inferred where absent |
