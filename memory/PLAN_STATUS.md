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
| Var | 4 | P1.1, P1.2, P2.6, P5.3 |
| Kısmi | 25 | P2.1, P2.2, P2.4, P2.5, P3.1, P3.2, P4.1–P4.4, P5.1, P6.2–P6.4, P7.1–P7.4, P8.2, P8.3, P9.2, P10, and save (all phases) |
| Yok | 16 | P0.2–P0.5, P1.3, P1.4, P2.3, P3.3, P3.4, P5.2, P6.1, P8.1, P9.1, P9.3, P11, P12 |

Player input/HUD: missing for every system except carrying/cutting/building logs [S] and the dig tool on N [A].

## Per task

| Task | Status | Exists | Missing |
|---|---|---|---|
| P0.2 freeze note | Yok | — | note in Placement/CLAUDE.md, carry.md |
| P0.3 data files | Yok | only `Config/Placement/PlacementProfiles.json` [S]; [A] definitions are editor arrays (UPROPERTY), not files | all game data JSON |
| P0.4 data loader | Yok | — | loader + validation |
| P0.5 recipes from data | Yok | recipe hard-coded in `WorldDemoScenarioActor.cpp` [S] | data-driven recipes |
| P1.1 calendar | Var | `WorldCalendar` + clock `GetCalendarDate`: year, month, day, hour, season from saved time; 5 days/month, start March 5 08:00; 3 tests | config from data file (P0.3), HUD (P1.4) |
| P1.2 climate by month | Var | `UWorldClimateSubsystem`: 12-month table, daily weather from clock seed, 05:00/15:00 temperature curve, snow depth, ground wetness, pushes weather + air temperature to survival; in the coordinator step + rollback; save schema v21; 3 tests | VerticalSlice_C does not run the coordinator yet (own hours, own weather) -> P1.4 |
| P1.3 event director | Yok | only raids have a warning phase [S] | natural events with signals: snowstorm, heavy rain, flood |
| P1.4 date/signals on screen | Yok | — | HUD date/season, visible signs |
| P2.1 water | Kısmi | thirst value [S]; consuming an item gives thirst relief [A] | dirty/boiled water items, drinking from the creek, container |
| P2.2 sleep/fatigue | Kısmi | fatigue value [S]; rest on the shared clock recovers fatigue/stamina [A] | sleep place quality, fatigue limiting actions |
| P2.3 wetness | Yok | clothing water-resistance number [A] | wet state from rain/creek, drying at fire, cold from wet |
| P2.4 spoilage | Kısmi | item creation time, freshness duration, spoiled energy fraction [A] | cellar, smoking, drying, heat and damp changing the speed |
| P2.5 disease/injury | Kısmi | conditions Injury/Illness/Poisoning, duration, damage, treatment item [A]; physical health [S] | triggers: dirty water, spoiled food, toxic rain, falls, animals |
| P2.6 clothing | Var [A] | slots Head/Body/Hands/Legs/Feet, insulation, water resistance, capabilities | (hide clothing recipe is P4) |
| P3.1 fire fuel | Kısmi | fuel = seconds, a log thrown on adds seconds [S]; fuel items with burn time and receipts [A] | wet wood, charcoal item, player input |
| P3.2 shelter enclosure | Kısmi | roof/wall cover by line traces [S]; shelter protection separate from clothing [A] | heat loss from the structural graph |
| P3.3 clay plaster | Yok | — | plaster item, wall state, closes gaps |
| P3.4 charcoal pit | Yok | generic production station [S] | the station and recipe |
| P4.1 gatherables | Kısmi | trees, logs, branches/sticks, stones [S] | flint, fibre, bone/antler, hide, reed, clay, resin, ochre, shell/tooth/feather |
| P4.2 item transformations | Kısmi | crafting + production frameworks, log cuts [S]; generic item intake into stations [A] | the 30 kind-E recipes |
| P4.3 stations | Kısmi | production station records and jobs [S] | the 6 station actors |
| P4.4 tool requirements | Kısmi | `MissingTool` result code, axe needed for log cuts [S]; dig tool strength setting [A] | tool items deciding what can be done |
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
