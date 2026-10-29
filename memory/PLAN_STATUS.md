# Plan status: what exists, what is partial, what is missing

Checked 2026-10-29 against `feature/simple-intent` (ours, builds, 68 tests) and
`group-a-code-closure` (ChatGPT). Both are now merged on `game/p00-adopt-group-a`: builds, 85/85 tests (2026-10-29). Task ids from `ROADMAP.md`.
Update this file whenever a task changes state.

- **Var / exists**: code covers the task's behaviour (backend). Still not played in PIE.
- **Kısmi / partial**: a base exists; the "Missing" column is still to do.
- **Yok / missing**: no code.
- `[A]` = came from group-a, `[S]` = from our branch. Both exist on `game/p00-adopt-group-a`.
- **Player** = input and HUD so the player can use it. Almost everything is missing this.

## Summary

| Status | Count | Tasks |
|---|---:|---|
| Var | 8 | P0.3, P0.4, P1.1, P1.2, P2.4, P2.6, P4.2, P5.3 |
| Kısmi | 23 | P0.5, P2.1, P2.2, P2.3, P2.5, P3.1, P3.2, P4.1, P4.3, P4.4, P5.1, P6.2, P6.3, P6.4, P7.1, P7.2, P7.3, P7.4, P8.2, P8.3, P9.2, P10, Save |
| Yok | 12 | P0.2, P1.3, P1.4, P3.3, P3.4, P5.2, P6.1, P8.1, P9.1, P9.3, P11, P12 |

Player input/HUD: missing for every system except carrying/cutting/building logs [S] and the dig tool on N [A].

## Per task

| Task | Status | Exists | Missing |
|---|---|---|---|
| P0.2 freeze note | Yok | — | note in Placement/CLAUDE.md, carry.md |
| P0.3 data files | Var | `Config/GameData/`: materials, items (88), transformations (59 rows = the 38 plan steps), stations (10), conditions, crops (3 placeholders), calendar, climate, layers | user review of the lists; events.json comes with P1.3 |
| P0.4 data loader | Var | `UWorldGameDataSubsystem`: loads + checks every reference, keeps good data on a bad load, applies to production, survival, climate, calendar, terrain; inventories and fires pick up hand recipes / fuels at BeginPlay; the bootstrap loads it; 3 tests | VerticalSlice_C does not use the bootstrap yet (P1.4) |
| P0.5 recipes from data | Kısmi | all game recipes come from data | the old demo recipe in `WorldDemoScenarioActor.cpp` is still in C++ |
| P1.1 calendar | Var | `WorldCalendar` + clock `GetCalendarDate`: year, month, day, hour, season from saved time; 5 days/month, start March 5 08:00; 3 tests | config from data file (P0.3), HUD (P1.4) |
| P1.2 climate by month | Var | `UWorldClimateSubsystem`: 12-month table, daily weather from clock seed, 05:00/15:00 temperature curve, snow depth, ground wetness, pushes weather + air temperature to survival; in the coordinator step + rollback; save schema v21; 3 tests | VerticalSlice_C does not run the coordinator yet (own hours, own weather) -> P1.4 |
| P1.3 event director | Yok | only raids have a warning phase [S] | natural events with signals: snowstorm, heavy rain, flood |
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
| P5.1 layers → items | Kısmi | yield rules material + depth layer → item, units per m³ [A]; voxel prototype [S] | our layers: soil, clay, sand, stone, flint, ochre |
| P5.2 cellar, trench, collapse | Yok | dig refused near grounded structures [A] | collapse of unsupported rooms; cellar/trench/cistern/embankment |
| P5.3 dig limits + save | Var [A] | bounded dig, soil classes, tool strength, receipts, density save | chunk seams, neighbour chunks (later) |
| P6.1 field cell state | Yok | crop moisture only [S] | cleared, tilled, fertility, weeds, mulch |
| P6.2 15 field steps | Kısmi | planting with a seed item, harvesting into items [A] (2 of 15) | clearing, tilling, fertilising, watering, weeding, mulch, fence, drying, threshing, winnowing, grinding, storing, seed saving |
| P6.3 seeds, exhaustion, 3 crops | Kısmi | crop definitions seed → harvest [A] | seed saving, soil exhaustion, the 3 crops |
| P6.4 animal chain | Kısmi | animal hunger/health/products [S]; feeding and collecting with items [A]; stick traps [S] | catching live animals, pen, slaughter outputs |
| P7.1 load, slope, surface | Kısmi | carry mass slows the player [S]; route cost with slope/surface/weather/load/infrastructure for cargo [A] | the same cost for the player's own walking |
| P7.2 zones | Kısmi | VerticalSlice_C: creek, far ford, old rope bridge, cliff terrace, lookout [S] | zones 0–4 with their resources |
| P7.3 rewards | Kısmi | discovery definitions with prerequisites, required equipment, unlocks [A] | discoveries placed in the world; recipes/seeds gated by them |
| P7.4 infrastructure | Kısmi | rope bridge, zipline, floor-mode bridges/ramps, stairs, sleds [S] | trail markers, path, ladder, raft, forward camp, maintenance and weather damage |
| P8.1 natural events + chains | Yok | — | flood, frost, hail, drought, wildfire, toxic rain, epidemic, storm, fog and chains |
| P8.2 signal-driven raids | Kısmi | threat director with warning phase, raid adapter, structural damage pulses [S]; pulse claimed once [A] | triggers from store size, smell, light, smoke |
| P8.3 damage, repair, snow, avalanche, fire | Kısmi | structural damage and dependents [S]; helper `MaintainWalls` [S] | snow load, avalanche, fire spread, repair with materials |
| P9.1 time budget tuning | Yok | — | balance pass |
| P9.2 helper uses stations | Kısmi | helper follows, collects, piles, builds plans, maintains walls [S] | using stations and new materials |
| P9.3 one full year played | Yok | — | — |
| P10 physical assembly | Kısmi | log family: cuts, split, hew, plank, snap kernel [S]; stone pieces with anchors [S]; stake by sharpening [S]; incremental cut progress [A] | thatch bundle, cut stone block making, notches/sockets, mitre cuts, crossing diagonals |
| P11 story | Yok | — | — |
| P12 polish and demo | Yok | — | — |
| Save (all phases) | Kısmi | saves every subsystem [S]; schema v20, voxel, component snapshots [A] | full actor transforms after physics, each new system |
