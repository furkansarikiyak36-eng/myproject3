# Roadmap — step-by-step tasks for any assistant

Design (what the game is): `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md` (Turkish).
This file (how we build it): ordered tasks, honest status, evidence. Any assistant with access to
this GitHub repo can continue from here: Claude Code, Claude web, another account, ChatGPT.

## Writing code without a compiler (all assistants, from 2026-10-29)

Work branch: `game/p00-adopt-group-a`; baseline build/test evidence is 102/102 at `bbc57b3`. The current partial-task edits after that commit are **not compiled or tested**. Status ceiling: `code-written`.
Most errors in unbuilt code are names that do not exist (group-a had 16). So:

1. **Read the header before you use anything.** Copy field, function and enum names from the `.h` file you read
   in this session (search `memory/CODE_INDEX.md`, then open the header). Never guess a name or a signature.
2. Unreal rules that broke builds before: `BlueprintReadWrite` is not allowed on `private:` members;
   `FBox::IsValid` is a field (no `()`); a `USTRUCT` with `GENERATED_BODY()` cannot be brace-initialised
   (`{a, b}`): set fields one by one; a loop variable must not reuse a name from an outer scope (C4459);
   every new `.h` needs `#include "X.generated.h"` as its last include.
3. Survival steps take the step's **end** time (`AdvanceSurvivalStep(Delta, EndTime)`).
4. A new saved field: add it to the struct, to the restore validation, and a save schema migration step
   (`WorldSaveCoordinatorSubsystem.cpp`, current schema 27 on `game/p00-adopt-group-a`; retain migrations from v22 through v26).
5. Keep a change small and in one place; do not refactor files you are not changing.
6. **Write the test with the code** (in `Tests/GameWorldTests.cpp`), even though you cannot run it.
   Name: `MyProject3.World.<Area>.<What>`.
7. Lists live in `Config/GameData/*.json`; new items/recipes go there, not into C++. Keep ids unique and
   every reference existing (the loader refuses the whole load otherwise).
8. Commit per task with the message `<Area>: <what> (P#.#) [unbuilt]`. Log the task in `CURRENT_TASK.md` and
   the progress log with status `code-written`.

## Batch build and fix (one Claude Code session at the end)

Editor closed. Then, in order, without fixing anything in between:
1. Build. Collect **all** errors into `memory/sessions/<date>_batch-fix.md`, grouped by file and by cause.
2. Fix compile errors by cause (a wrong name used in 5 places is one fix). Rebuild until it builds.
3. Run all tests once. List every failure with its first `Error: Expected` line.
4. Group failures by root cause before touching code: same system, same data, same contract.
   Fix a whole group together; decide whether the code or the test is wrong (say which).
5. Run all tests again. Repeat 4-5 only for what still fails.
6. Update `PLAN_STATUS.md` (`code-written` -> `tested`), commit, push.

## How to continue (read this first)

1. Read `memory/README.md`, `memory/STATE.md`, the newest file in `memory/sessions/`, then this file.
2. Take the **first task whose status is `todo` and whose "Needs" are all `done`**.
   If a task is `blocked-user`, ask the user the question in the task; do not guess.
3. Claim it: set status `in-progress`, write `who/date/branch` in the task, commit `memory: claim <ID>`.
4. Do the steps. Keep the task's "Done when" literally.
5. Set the status you can **prove** (see vocabulary), add the evidence line, commit.
6. Write a session file (`memory/sessions/`) before you stop. If you run out of time/tokens in the
   middle, set status `in-progress` with a `Left off:` line saying exactly what remains.

## Status vocabulary (honest levels)

| Status | Meaning | Who can set it |
|---|---|---|
| `todo` | Not started | — |
| `blocked-user` | Needs a user decision (question in the task) | anyone |
| `in-progress` | Someone is on it; see `Left off:` | anyone |
| `code-written` | Code exists, **not compiled** | web assistants (no Unreal) |
| `built` | UBT build passed | needs local Unreal (Claude Code on the user's PC) |
| `tested` | Build + automation tests passed (count them) | local |
| `played` | User checked it in PIE and said it works | only after the user confirms |
| `done` | `played` + save/load checked | only after the user confirms |

Never write a higher status than you proved. A web assistant that cannot build must stop at
`code-written` and say so. Build/test commands are in the root `CLAUDE.md`.

## Branches

- Until P0.1 is decided, base new work on `feature/simple-intent`.
- One branch per phase: `game/p<NN>-<name>` (e.g. `game/p01-calendar`). Commit per working step.
- Push only when the user asks (web assistants: open a PR instead of pushing to a shared branch).

## Fixed rules (from the design)

- World first, story last. Every mechanic must feed another one.
- Era 1 = primitive materials only (design §4). Max 2–3 items per material group.
- Physical assembly (log/stone/plank geometry, notches, mitres) is **frozen until phase 10**.
  Until then buildings use the existing snap kernel as-is. Do not add new cut geometry.
- Lists (materials, items, recipes, stations, crops, events, calendar) live in data files,
  not in C++ (after P0.3).
- Add an automation test with every behaviour change.

Exact per-task status (exists / partial / missing): `PLAN_STATUS.md`.

## What `group-a-code-closure` gives each task (if adopted, Q00)

Checked 2026-10-29 from its headers. "Fits" = matches the plan; "partly" = a base, plan needs more.
Everything below is backend only: no input or HUD for the player unless stated. Builds and passes tests on `game/p00-adopt-group-a` (2026-10-29).

| group-a piece | Task | Fit | Build on top (our work) |
|---|---|---|---|
| Item freshness (`CreatedAtWorldTimeSeconds`), consumable `FreshnessDurationSeconds`, `SpoiledEnergyFraction` | P2.4 | partly: time only | Storage modifiers: cellar, smoke rack, drying, heat, damp |
| `TryConsumeFoodItem` (kcal + thirst relief from an item) + `TryFillWaterFromCreek` | P2.1 | code-written partial: dirty-water consume/illness + fired-pot creek fill into owned inventory item, idempotent output receipt; I key in VerticalSlice_C | boil via heat/production station, player consumption input/UI, build/PIE test |
| `TryBeginRest` (fatigue/stamina recovery on the shared clock) + character movement adapter | P2.2 | code-written partial: fatigue reduces walk speed; sprint requires stamina/fatigue thresholds; Run/Chop/Dig/Build/Carry gates; VerticalSlice_C chop preflight | player sleep/rest input, work-speed/tool-duration coupling and in-game tuning |
| Conditions `Injury, Illness, Poisoning` with duration, damage, treatment item | P2.5 | code-written partial: dirty water, spoiled food and prolonged wet-cold trigger configured illness; `Injury.Fall`/`Injury.AnimalBite` definitions exist | fall/animal-attack and toxic-rain trigger wiring; player HUD feedback |
| Equipment slots `Head, Body, Hands, Legs, Feet`, clothing insulation, water resistance, capabilities | P2.6 | fits | Hide clothing recipe (#15); wetness (P2.3) uses water resistance |
| Shelter protection separate from clothing | P3.2 | partly: still line traces | Enclosure from the structural graph, plaster closes gaps |
| Fuel item definitions, `TryAddFuelItem` (item → burn seconds, receipts) | P3.1 | fits | Wet wood burns worse, charcoal item, player input on the fire |
| Generic production intake `TryIngestWorldOwnedItem` | P4.2–P4.3 | fits | The 38 transformations and 8 stations as data |
| Incremental cut progress receipts (work over time) | P4 / P10 | fits ("work takes time") | Same for other tools (knapping, scraping) |
| Bounded voxel dig, soil classes soft/hard/rock with tool strength, support clearance, dig receipts | P5.1, P5.3 | fits | Collapse of unsupported rooms (it only refuses near structures), cellar/trench/cistern |
| Resource yield rules: material + depth layer → item, units per m³ | P5.1 | fits exactly | Our layers: soil, clay, sand, stone, flint, ochre |
| `TerrainDigToolComponent` (N key, crosshair dig) | P5 player wiring | partly: strength is a setting | Strength from the equipped tool (P4.4) |
| Voxel density save, generic component SaveGame snapshot, schema v20 | every phase | fits | — |
| Crop definition seed item → harvest item; Plant/Harvest/Feed/CollectProducts with items | P6.2–P6.4 | partly: 2 of 15 field steps + animal feeding/products | Field cell state, 13 other steps, tools, fertility, seed saving, calendar |
| Discovery definitions: prerequisites, required equipment, unlocked definitions; `IsDefinitionUnlocked` | P7.3 | fits | Discoveries placed in the world; recipes/seeds gated by unlocks |
| Region access by equipped capability (`HasEquippedCapability`) | P7.2 | partly | Use only for climate gates (cold pass needs warm clothes). Other zones are opened by infrastructure, not by item keys |
| Route cost context: slope, surface, weather, load, required/built infrastructure; cargo replan | P7.1, P7.4 | fits the "bridge makes the next trip cheaper" idea | Apply the same cost to player movement; built bridge/path marks the edge |
| Raid damage pulse claimed once per raid index | P8.2–P8.3 | fits (small) | Signal-driven raids |
| Exact unique-item removal, `Equipped` ownership state | shared state | fits | — |

**Not in group-a (still all ours):** P0 data files, all of P1 (calendar, seasons, climate, events), wetness, water item types,
the 38 transformations as data, field chain, zones content, natural events and chains, player input/HUD for every system.
**Nothing in group-a contradicts the plan**, except item-key region gates (keep for climate only).

---

## Phase 0 — Preparation

**P0.0 · Decide what to do with `group-a-code-closure`** — `tested` (adopted on `game/p00-adopt-group-a`, 250ce13, 85/85; user said "kısmileri bitirelim")
ChatGPT wrote +182 commits (~6.6k lines) on top of our base: fuel items, freshness, rest, conditions,
equipment, discovery rewards, excavation → items → production, item-backed farming, route costs.
It does not compile as pushed: 10 UHT errors + 6 C++ errors in 2 files (see `CODEBASE.md` Findings). Options: (a) fix its build, run the
tests, then use it as the base for P1–P7; (b) leave it and rebuild those parts task by task.
Question for the user: "group-a dalını düzeltip temel alalım mı?" Done when: answered and,
for (a), the branch builds and its tests are counted.

**P0.1 · Decide the base branch** — `blocked-user`
Question for the user: "`main` is 697 commits behind `feature/simple-intent`. Merge
`feature/simple-intent` into `main` and work from `main` from now on?" Also: close PR #34
(second, conflicting kernel)? Done when: the user answered and the merge/close is made.

**P0.2 · Freeze note** — `todo`
Add a short "frozen until phase 10" note at the top of `Source/MyProject3/Placement/CLAUDE.md`
and `memory/modules/carry.md`, listing the open items moved there (mitre cuts, NotchCradle sockets,
crossing wall diagonals, deck path). Done when: committed.

**P0.3 · Game data files** — `tested` (needs user review of the lists) · Needs: —
Create `Config/GameData/` with JSON files, content copied from the design doc:
`materials.json` (§4.1), `items.json` (items produced by §4.3), `transformations.json` (38 rows:
id, inputs, tool/station, outputs, kind E/F, duration, fuel), `stations.json` (8),
`crops.json` (3 placeholders until P6 question), `calendar.json` (12 months: temperature range,
precipitation, possible events), `events.json` (§7.1: signal, effect, counters).
Done when: files exist, valid JSON, each row has a stable id. Status ceiling for web: `code-written`
is fine here (no build needed); mark `done` after the user reviews the lists.

**P0.4 · Data loader** — `tested` (99/99) · Needs: P0.3
New `UGameDataSubsystem` (GameInstance subsystem) that loads `Config/GameData/*.json`, validates
ids/references (every input/output item exists, every station exists) and exposes lookups.
Tests: loads all files; a broken reference fails validation. Done when: `tested`.

**P0.5 · Recipes from data** — `todo` · Needs: P0.4
`WorldDemoScenarioActor.cpp` registers recipes in code (`Production->RegisterRecipe`). Feed
`UWorldProductionSubsystem` from `transformations.json` instead; remove the hard-coded recipe.
Done when: `tested`, no recipe defined in C++.

## Phase 1 — Calendar and weather

Existing: `UWorldSimulationClockSubsystem` (time snapshot, saved). `UWorldSurvivalSubsystem` has
`EWorldWeatherState { Clear, Rain, Wind, Snow, Storm, Heatwave }` and `SetWeather`.
Missing: day/month/season/year, weather driven by the calendar, events with signals.

**P1.1 · Calendar** — `tested` (88/88; config is a UPROPERTY until P0.4 loads data) · Needs: P0.4
Derive year, month (1–12), day of month, season from the clock. Days per month from
`calendar.json` (default 5, see open question Q1). Start date: end of March, year 1.
Tests: day rollover, month rollover, year rollover, save/load keeps the date.

**P1.2 · Climate by month** — `tested` (91/91; month table is a UPROPERTY until P0.4) · Needs: P1.1
Each day pick temperature and weather from the month's ranges in `calendar.json`
(deterministic seed saved with the world). Call the existing `SetWeather`. Snow depth and
ground wetness as world values that grow/melt. Tests: January never Heatwave; snow melts in March.

**P1.3 · Event director** — `code-written` (2026-10-29; unbuilt/unrun) · `WorldEventDirectorSubsystem` + `events.json`: deterministic seeded schedule, month/chance gates, warning/active/end lifecycle, predecessor chains, bounded history, transactional coordinator integration and saved snapshot. `EffectId` world mutations are deferred to P8.1. · Needs: P1.2
New `UWorldEventDirectorSubsystem`: events from `events.json`; each event has a **signal phase**
(hours/days before) and an **effect phase**; conditions read world state (month, snow depth,
dryness, wind). First three events only: snowstorm, heavy rain, flood (water level rises in
low cells). Chained events come in P8. Tests: signal always precedes effect; flood only after
melt + rain; state saved mid-event.

**P1.4 · Player sees it** — `code-written` (2026-10-29; unbuilt/unrun), scene remains deferred: `UWorldHudDataSubsystem` exposes date/season/weather/temperature/active event notifications; `LogSnapHUD` draws a simple readout; VerticalSlice_C sun reads the authoritative clock. Remaining: PIE layout, message lifecycle/UX and visual-to-logical weather match. New "Valley" scene stays last by user decision.
HUD shows date and season; signals are visible in the world (sky/clouds, falling pressure line
on HUD, animals hiding later). No placement rules in HUD. Done when: `played` (user checks a
fast-forwarded year in PIE).

## Phase 2 — Individual survival

Existing in `UWorldSurvivalSubsystem`: Thirst, Fatigue, kcal, felt temperature, activities,
`ConsumeFood`. Missing: sleep, wetness, disease, injury, spoilage, clothing.

- **P2.1** Water as items: dirty/boiled water, drinking from creek (dirty), container needed. Tests.
- **P2.2** Sleep and fatigue: fatigue limits actions; sleeping where (cold ground vs bed) matters.
- **P2.3** Wetness: rain/creek make you wet; wet raises cold; fire/shelter dries.
- **P2.4** Spoilage: every food item has freshness; heat/moisture speed it, smoking/drying/cellar slow it.
- **P2.5** Disease and injury: dirty water/spoiled food → sickness; falls/animals → wounds; slow recovery.
- **P2.6** Clothing: hide clothes add protection (existing `UpdateProtection`).
Done for phase: `played` — "rain makes me wet and cold; dirty water makes me sick".

## Phase 3 — Fire, fuel, shelter

Existing: `HeatSourceComponent`, `ShelterSensorComponent`.
- **P3.1** Fire burns real fuel items (wood, charcoal); wet wood burns badly; fire goes out when empty.
- **P3.2** Shelter enclosure from the structural graph; heat loss per gap.
- **P3.3** Clay plaster (transformation 20) applied to a wall closes gaps (wall state, no new geometry).
- **P3.4** Charcoal pit station (transformation 25).
Done: `played` — "plastered hut stays warm; fire dies without fuel".

## Phase 4 — Gathering and transformations

- **P4.1** Gatherables in the world for every material group available in zone 0.
- **P4.2** Item transformations 1–2, 4, 11–16, 18–30 (kind E) through stations/hands, data-driven.
- **P4.3** The 6 non-farm stations as placeable actors using `UWorldProductionSubsystem`.
- **P4.4** Tool requirements (no axe → no log cut).
Done: `played` — "flint → axe and clay → fired pot chains work in game".

**Milestone A — "First month"** = phases 1–4 `played` together, one month survivable from nothing.

## Phase 5 — Terrain and digging

Existing: `VoxelTerrainChunkComponent`, `UWorldTerrainStateSubsystem`, decision in
`Docs/Architecture/TERRAIN_ARCHITECTURE_DECISION.md` (still waiting for user approval — ask).
- **P5.1** Layers (soil, clay, sand, stone, flint, ochre) by depth/zone; dug volume → items.
- **P5.2** Cellar, trench, embankment, cistern; unsupported rooms collapse.
- **P5.3** Limits (depth, volume per chunk) and save/load of all edits.

## Phase 6 — Farming and animals

Existing: `UWorldAgricultureSubsystem` (stage by time + moisture only).
- **P6.1** Field cell state: cleared, tilled, fertility, weeds, mulch, moisture.
- **P6.2** The 15 steps of design §6.1, each with its hand tool; transformations 31–38.
- **P6.3** Seeds as items, seed saving, soil exhaustion, 3 crops (question Q2).
- **P6.4** Animal chain: trap, pen (stakes + rope), feeding, products, slaughter.

## Phase 7 — Movement, exploration, infrastructure

- **P7.1** Load, slope and surface (mud, snow, ice) change speed and energy (`CarryLoadComponent`).
- **P7.2** Zones 0–4 in the level with their resources and barriers.
- **P7.3** Rewards: deposits, seeds, knowledge unlocks (`RecordDiscovery` exists), places.
- **P7.4** Infrastructure with simple placement: trail markers, path, bridge, rope line, ladder,
  raft, forward camp. Maintenance and weather damage.
Done: measured trip time to the creek drops after the bridge.

## Phase 8 — Events and threats

- **P8.1** All events of design §7.1 and the chains of §7.2.
- **P8.2** Raids triggered by signals (store size, smell, light, smoke) via the existing threat director.
- **P8.3** Structural damage and repair; snow load; avalanche; wildfire spread.

## Phase 9 — Balance and helpers

- **P9.1** Time budget tuning; stations and plans save time.
- **P9.2** Helper NPC uses stations and plans (question Q6).
- **P9.3** One full year played by the user.

**Milestone B — "First year"**.

## Phase 10 — Physical assembly (FROZEN)

Frozen by the user's current decision on 2026-10-29. Do not implement or expand this phase until explicitly reopened. The 8 advanced physical parts (log family, plank, stake, stone block, thatch bundle), notches, mitre cuts and crossing diagonals remain deferred; preserve the current carry/cut/place/snap kernel.

## Phase 11 — Story

Traces, ruins, old settlements in the places reserved in P7.2. No new mechanic for the story.

## Phase 12 — Polish and demo

Sound, UI, performance, full save/load, a 60–90 minute Steam demo.

**Milestone C — "Primitive era complete"**.

---

## Open questions for the user (blocking)

| Id | Question | Blocks |
|---|---|---|
| Q00 | Fix and adopt `group-a-code-closure` as the base? | P0.0, P1–P7 |
| Q0 | Merge `feature/simple-intent` into `main`? Close PR #34? | P0.1 |
| Q1 | Days per month (proposal 5 → 60-day year)? | P1.1 (can start with 5) |
| Q2 | Which 3 crops (grain, root, legume)? | P6.3 |
| Q3 | Map size: all 5 zones in one valley? | P7.2 |
| Q4 | What happens on death? | P2 end |
| Q5 | Single player only? | — (plan assumes yes) |
| Q6 | Helper NPCs: how many, how gained? | P9.2 |
| Q7 | Central building level (like a town hall) or progress only by exploration/transformations? | P4, P7 |
| Q8 | Approve the terrain decision (`TERRAIN_ARCHITECTURE_DECISION.md`)? | P5 |

## Progress log

Newest first, one line each: `date · task · status · evidence (commit, test count, user words)`.
- 2026-10-29 · P2.1/P2.2/P2.3/P2.5 · code-written · water loop supports I=creek fill, F5=timed boil from `boil_water` (1188 shared-clock seconds), M=drink (boiled preferred), F6=rest, F7=status; same ledger-owned item transitions `Water.Dirty`→`Water.Boiling`→`Water.Boiled`, with the inventory category updated Resource→Consumable and retry-safe owner receipts; schema v27/migration v26→v27. VerticalSlice_C now respects initialized shared climate weather/severity rather than overwriting rain/storm with local wind/snow. `CanRecordActivity` is a pure shared rest/exhaustion preflight used by `RecordActivity`; LogCarry now preflights and charges Carry for log/material pickup and Build for central log/material placement, while fatigue effectiveness scales tree chop, actual log-cut work, voxel digging and repair. Active rest blocks movement/jump/work. F7 shows body HP and survival HP separately plus conditions. Water-category, rest/work-effectiveness, Build/Carry preflight and warm-shelter-drying assertions plus near-vs-far fire drying test `MyProject3.Survival.FireDistanceControlsWetnessDrying` authored. `LogBuildToolsComponent` now shares rest/exhaustion gates for successful sharpening, fire feeding, door/window/gate cutting, rope tying, dismantle hotkey and throwing (`1f0293f`) (`7169395`, climate wiring `785b21b`/`3dc9834`, v27 `2e4410e`/`d803ff7`, work scaling `3c976b5`/`27adaa7`/`072bf6e`/`07484ed`, tests `d1665a5`/`2ad7ad8`/`e9e982a`/`bc3b02a`) · **unbuilt/unrun; no PIE validation**.
- 2026-10-29 · P4.3 stations · code-written · VerticalSlice_C now binds F2=start/collect, F3=deposit missing generic recipe inputs/tools, F4=take one stock unit at the nearest registered station within 300 cm; station registration waits for applied game data; withdrawals preserve item category/tool singleton metadata and roll back stock if inventory insertion fails; integration test `MyProject3.Production.StationDepositStartCollectTakePreservesToolType` authored (`2454b9b`, test `917ba36`, key fix `44d29d8`, status `efde8bd`) · **unbuilt/unrun**. No station actors/meshes have been placed in VerticalSlice_C, so this input has no nearby target until scene work.
- 2026-10-29 · P3.2 shelter enclosure · code-written · `ShelterSensorComponent::HitsBuiltPiece` now skips empty/open door/gate frames and retraces through them to placed pieces; only structurally placed pieces or closed built opening panels count; `FShelterCoverTest` adds empty door + wall-behind-opening assertions (`ce10b8c`, test `91acd2c`) · **unbuilt/unrun**. Per-gap thermal-loss calculation and plaster closure remain separate work.
- 2026-10-29 · P3.1 fire fuel · code-written · confirmed and regression-tested in source: rain/storm severity≥0.2 converts World-owned `Firewood`→`Firewood.Wet`; non-rain/storm + a lit heat source returning ≥12°C converts only nearby wet fuel back to dry; inventory-owned fuel is unaffected; fuel data rates and O-key refuel are already present; test `MyProject3.Survival.WorldFirewoodGetsWetInRainAndDriesNearLitFire` authored in `ceeb8a8` · **unbuilt/unrun**
- 2026-10-29 · P2.1–P2.5 · code-written · canonical player inventory + game-data weight/recipe backfill; `Pot.Fired`→unique `Water.Dirty` transaction with schema v26, exact capacity preflight and idempotent receipt; I=creek fill/O=fire refuel; fatigue/sprint/chop gates; creek wetness→`Illness.Cold`; hard-fall `Injury.Fall`; hostile bite `Injury.AnimalBite` with 3s cooldown; tests authored for fill capacity/retry, wet-cold, fall and bite (`46a6da3`, `3ed10e7`, `d3962be`, `e8e98c9`, `fe27cd0`, `ca0d23c`, `81f1c7f`, `408992c`) · **unbuilt/unrun**
- 2026-10-29 · P2.1/P2.2/P2.3/P2.5 (partial) · code-written · `TryFillWaterFromCreek` with Pot.Fired, dirty-water output/receipt, v26 save migration, fatigue walk/sprint/action gates, creek wetness and wet-cold illness; tests authored (`605ee55`, `f1c41a0`, scene wiring `fd706f7`) · **unbuilt/unrun**
- 2026-10-29 · P2.1/P2.2/P2.3/P2.5 continuation · code-written · receipt-backed timed water transformation (`Water.Dirty`→`Water.Boiling`→`Water.Boiled`) keeps one ledger ID and reusable pot, schema v27 migration; M/F5/F6/F7 player controls; shared rest/exhaustion preflight, fatigue-scaled work and carry/build costs; F7 status; newly added dry/damp/wet/soaked transition feedback in VerticalSlice_C (`53744cf`, `d3237df`, `2e4410e`, `d803ff7`, `9581c30`, `9c25d83`) · **unbuilt/unrun; no PIE validation**
- 2026-10-29 · P2.2 interaction-cost continuation · code-written · door-bar removal/unlocking and starting tow already charge on success; direct `DismantlePiece` calls now own their default Build preflight/charge, while gate/window aggregate paths suppress duplicate charges; `ALogContainerActor` charges ongoing Carry activity using logical-clock elapsed time (`609f445`, `28755f3`) · **unbuilt/unrun; no UE/PIE validation**
- 2026-10-29 · P6.4 · code-written · wildlife identity, capture receipt, capacity/species-gated pen, atomic meat/hide/bone outputs, pen save/migration v25, lifecycle test authored (`40e2ba3` and `3546219`) · **unbuilt**
- 2026-10-29 · P6.1/P6.2/P6.3 (part) · code-written · field-cell actions/snapshot, planting month and crop-data wiring, fertility/weed/mulch growth, seed-return output, schema v23/v24 and automation coverage authored (`2ca2ede`, `e258f59`) · **unbuilt**
- 2026-10-29 · P0.5 · code-written · demo scenario selects game-data transformation ID instead of separate editable recipe payload; status updated (`7ef5c47`) · **unbuilt**

- 2026-10-29 · P5.1 · baseline layer-yield test existed in the last verified build (102/102); new region/depth/cell-seed geology resolver and N-key wiring are `code-written`, unbuilt/unrun.
- 2026-10-29 · P4.1/P4.3 (part) · tested · gatherable spots, station actor with fire rules, 101/101
- 2026-10-29 · P0.3/P0.4/P4.2 · tested · Config/GameData + UWorldGameDataSubsystem, CanCraft checks tools, 99/99
- 2026-10-29 · P3.2 (part) · tested · shelter keeps fire heat, sensor counts only placed pieces, 96/96
- 2026-10-29 · P3.1 (part) · tested · fuel heat multiplier, wet wood / charcoal, IsBurningHot, 95/95
- 2026-10-29 · P2.4 · tested · spoilage by storage + temperature, schema v22, 94/94
- 2026-10-29 · P2.2 (part) · tested · rest quality from shelter/warmth/dryness, 93/93
- 2026-10-29 · P2.1/P2.5 (part) · tested · consumables cause conditions (dirty water, spoiled food), 92/92
- 2026-10-29 · P1.2 · tested · UWorldClimateSubsystem, coordinator + save v21, 91/91
- 2026-10-29 · P1.1 · tested · WorldCalendar + clock date, GameWorldTests.cpp, 88/88
- 2026-10-29 · P0.0 · tested · group-a adopted on game/p00-adopt-group-a, build fixed (16 errors), 85/85 tests, 250ce13
- 2026-10-29 · design + roadmap written · — · commit after 85569c6

- 2026-10-29 · P4.4 · code-written · `FGameDataItem::ToolStrength` and `GetToolStrength` are data-backed; Tool.Axe=1.5, Tool.DiggingStick=1.0; legacy Tool rows default to 1.0, non-tools to 0, Tool values validate within 0.1–10; VerticalSlice_C grants missing prototype tools after data application; axe strength scales tree chop and log-cut work; N digging requires Tool.DiggingStick and multiplies the configured dig strength; `MyProject3.GameData.ToolStrengthIsAuthoredAndResolved` authored. No durability/wear model. **Unbuilt/unrun; no PIE validation.**
- 2026-10-29 · P4.3 · code-written hardening · `AProductionStationActor::HasRequiredFire` now fails closed if world/game data is not ready or `StationDataId` is missing/unknown; interaction test covers valid no-fire workbench and unknown station rejection. Fire is still checked at recipe start only; pause/resume when fire goes out remains open. **Unbuilt/unrun.**
