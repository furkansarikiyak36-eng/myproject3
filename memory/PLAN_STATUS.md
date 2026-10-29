# Plan status: what exists, what is partial, what is missing

Baseline checked 2026-10-29 against `feature/simple-intent` (builds, 68 tests) and
`group-a-code-closure` (85/85 tests at merge). Subsequent changes in this work session are committed to `game/p00-adopt-group-a` but have **not** been compiled or run in Unreal yet; the earlier 85/85 result does not cover them. Task ids from `ROADMAP.md`.
Update this file whenever a task changes state.

- **Var / exists**: code covers the task's behaviour (backend). Still not played in PIE.
- **Kısmi / partial**: a base exists; the "Missing" column is still to do.
- **Yok / missing**: no code.
- `[A]` = came from group-a, `[S]` = from our branch. Both exist on `game/p00-adopt-group-a`.
- **Player** = input and HUD so the player can use it. Almost everything is missing this.

## Summary

| Status | Count | Tasks |
|---|---:|---|
| Var | 12 | P0.3, P0.4, P0.5, P1.1, P1.2, P2.4, P2.6, P4.2, P5.1, P5.3, P6.1, P6.4 |
| Kısmi | 20 | P2.1, P2.2, P2.3, P2.5, P3.1, P3.2, P4.1, P4.3, P4.4, P6.2, P6.3, P7.1, P7.2, P7.3, P7.4, P8.2, P8.3, P9.2, P10, Save |
| Yok | 11 | P0.2, P1.3, P1.4, P3.3, P3.4, P5.2, P8.1, P9.1, P9.3, P11, P12 |

Player input/HUD: available prototype bindings include carry/cut/build tools, dig tool (N), creek fill (I), fire refuel (O), and helper order (J). A general survival/status HUD, drinking confirmation, rest/sleep control, and most system-specific feedback are still missing.

## Per task

| Task | Status | Exists | Missing |
|---|---|---|---|
| P0.2 freeze note | Yok | — | note in Placement/CLAUDE.md, carry.md |
| P0.3 data files | Var | `Config/GameData/`: materials, items (88), transformations (59 rows = the 38 plan steps), stations (10), conditions, crops (3 placeholders), calendar, climate, layers | user review of the lists; events.json comes with P1.3 |
| P0.4 data loader | Var | `UWorldGameDataSubsystem`: loads + checks every reference, applies data to production/survival/climate/calendar/terrain/agriculture; inventory item weights and hand recipes are backfilled for inventories that began play before data application; existing and newly spawned fires receive authored fuels; 3 baseline tests | VerticalSlice_C coordinator/bootstrap integration and PIE remain unverified |
| P0.5 recipes from data | Var | `UWorldGameDataSubsystem` builds hand/station recipes from `transformations.json`; demo scenario now selects only a data transformation ID and builds recipe payload from that record | batch build + legacy Blueprint/config compatibility and PIE still unverified |
| P1.1 calendar | Var | `WorldCalendar` + clock `GetCalendarDate`: year, month, day, hour, season from saved time; 5 days/month, start March 5 08:00; 3 tests | config from data file (P0.3), HUD (P1.4) |
| P1.2 climate by month | Var | `UWorldClimateSubsystem`: 12-month table, daily weather from clock seed, 05:00/15:00 temperature curve, snow depth, ground wetness, pushes weather + air temperature to survival; in the coordinator step + rollback; save schema v21; 3 tests | VerticalSlice_C does not run the coordinator yet (own hours, own weather) -> P1.4 |
| P1.3 event director | Yok | only raids have a warning phase [S] | natural events with signals: snowstorm, heavy rain, flood |
| P1.4 date/signals on screen | Yok | — | HUD date/season, visible signs |
| P2.1 water | Kısmi (code-written) | thirst relief when consuming owned water item; `Water.Dirty` applies `Illness.DirtyWater`; `Pot.Fired` is a reusable vessel; `TryFillWaterFromCreek` creates an owned `Water.Dirty` item transactionally, with receipt/retry and save schema v26; VerticalSlice_C binds I while player is in creek | boil recipe/heat-station workflow and player-facing drinking confirmation/UI still need wiring; tests authored but unrun |
| P2.2 sleep/fatigue | Kısmi (code-written) | fatigue scales player walking speed (down to 45% at max fatigue); LeftShift sprint requires stamina>20 and fatigue<85; `RecordActivity` gates Run and strenuous actions; VerticalSlice_C chop preflights stamina and displays exhaustion feedback | explicit player sleep/rest input and work-speed/tool-duration coupling remain; new tests unrun |
| P2.3 wetness | Kısmi (code-written) | rain/storm wetness and shelter/warmth drying already work; creek wading adds wetness through `ApplyWetnessExposure`; survival step applies configured `Illness.Cold` after high wetness + low felt/core temperature; test authored but unrun | verify visible drying feedback at fires and build/PIE tune exposure thresholds |
| P2.4 spoilage | Var | freshness duration, spoiled energy [A]; spoilage age accrues per owner: storage `SpoilageRateMultiplier` (cellar, smoke rack) x air-temperature factor from climate; schema v22; test. Smoked/dried food = other item definitions with longer freshness (recipes, P4) | the temperature of a segment is read when it closes (approximation); damp not modelled |
| P2.5 disease/injury | Kısmi (code-written) | condition definitions/duration/damage/treatment; dirty-water/spoiled-food and long wet-cold triggers; hard falls ≥850 cm/s cause physical damage + configured `Injury.Fall`; hostile Enemy bites within 190 cm cause physical damage + `Injury.AnimalBite` on a 3-second cooldown; regression tests authored | toxic-rain trigger (P8), player-visible condition/health status, and batch build/automation run remain |
| P2.6 clothing | Var [A] | slots Head/Body/Hands/Legs/Feet, insulation, water resistance, capabilities | (hide clothing recipe is P4) |
| P3.1 fire fuel | Kısmi (code-written) | data-defined `Firewood` (1800s/1.0), `Firewood.Wet` (900s/0.6), `Charcoal` (2400s/1.5); owned fuel-item refuel/receipts and O-key fire interaction; rain/storm extinguishes exposed sources; world-owned dry firewood becomes wet at severity≥0.2; wet fuel dries in non-rain/storm only within an active heat radius (≥12°C); regression test authored for world-vs-inventory ownership, near-vs-far drying, base definition and revisions | no roof/shelter coverage for loose wood; no visible wet/dry world-mesh swap verified; batch build/automation/PIE still pending |
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
| P6.1 field cell state | Var (code-written) | persistent field-cell map: cleared, tilled, fenced, fertility, weed pressure, mulch, moisture, harvest count, optimistic revisions and idempotent action receipts; seasonal crop registration; save schema v23+ | physical plot/input/HUD wiring and Unreal validation remain |
| P6.2 15 field steps | Kısmi | planting and harvest use owned items [A]; backend field actions now cover clear, till, fertilise, water, weed, mulch and fence; moisture, weeds and mulch affect growth; one configured seed is returned per harvest | material/tool costs and player input for field actions; drying, threshing, winnowing, grinding and storage steps |
| P6.3 seeds, exhaustion, 3 crops | Kısmi | crops load from `crops.json` into agriculture; planting months and discovery-zone metadata are registered; harvest emits configured seed-return units; each harvest reduces fertility | three crop records are still placeholders and need full authored distinctions/balancing; discovery placement/locks and field-item costs |
| P6.4 animal chain | Var (code-written) | stable wildlife identity; idempotent capture; capacity/species/revision-gated logical pens; saved captured/penned/slaughtered states; atomic meat/hide/bone outputs; slaughter retries and v25 save migration; regression test authored | capture definition data loader, trap/player interaction, physical pen/build-piece placement, HUD, build/PIE validation |
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
| Save (all phases) | Kısmi | coordinator persists subsystem state; schema v26 includes field cells, animal pens and saved survival fill-output IDs; v22→v23, v23→v24, v24→v25 and v25→v26 migrations are defined; actor/component SaveGame snapshots remain | batch build/load validation; verify new receipt roundtrips, physical actor transforms after physics, and the whole-year scenario |
