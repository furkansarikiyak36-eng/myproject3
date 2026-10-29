# Roadmap — step-by-step tasks for any assistant

Design (what the game is): `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md` (Turkish).
This file (how we build it): ordered tasks, honest status, evidence. Any assistant with access to
this GitHub repo can continue from here: Claude Code, Claude web, another account, ChatGPT.

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

---

## Phase 0 — Preparation

**P0.1 · Decide the base branch** — `blocked-user`
Question for the user: "`main` is 697 commits behind `feature/simple-intent`. Merge
`feature/simple-intent` into `main` and work from `main` from now on?" Also: close PR #34
(second, conflicting kernel)? Done when: the user answered and the merge/close is made.

**P0.2 · Freeze note** — `todo`
Add a short "frozen until phase 10" note at the top of `Source/MyProject3/Placement/CLAUDE.md`
and `memory/modules/carry.md`, listing the open items moved there (mitre cuts, NotchCradle sockets,
crossing wall diagonals, deck path). Done when: committed.

**P0.3 · Game data files** — `todo` · Needs: —
Create `Config/GameData/` with JSON files, content copied from the design doc:
`materials.json` (§4.1), `items.json` (items produced by §4.3), `transformations.json` (38 rows:
id, inputs, tool/station, outputs, kind E/F, duration, fuel), `stations.json` (8),
`crops.json` (3 placeholders until P6 question), `calendar.json` (12 months: temperature range,
precipitation, possible events), `events.json` (§7.1: signal, effect, counters).
Done when: files exist, valid JSON, each row has a stable id. Status ceiling for web: `code-written`
is fine here (no build needed); mark `done` after the user reviews the lists.

**P0.4 · Data loader** — `todo` · Needs: P0.3
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

**P1.1 · Calendar** — `todo` · Needs: P0.4
Derive year, month (1–12), day of month, season from the clock. Days per month from
`calendar.json` (default 5, see open question Q1). Start date: end of March, year 1.
Tests: day rollover, month rollover, year rollover, save/load keeps the date.

**P1.2 · Climate by month** — `todo` · Needs: P1.1
Each day pick temperature and weather from the month's ranges in `calendar.json`
(deterministic seed saved with the world). Call the existing `SetWeather`. Snow depth and
ground wetness as world values that grow/melt. Tests: January never Heatwave; snow melts in March.

**P1.3 · Event director** — `todo` · Needs: P1.2
New `UWorldEventDirectorSubsystem`: events from `events.json`; each event has a **signal phase**
(hours/days before) and an **effect phase**; conditions read world state (month, snow depth,
dryness, wind). First three events only: snowstorm, heavy rain, flood (water level rises in
low cells). Chained events come in P8. Tests: signal always precedes effect; flood only after
melt + rain; state saved mid-event.

**P1.4 · Player sees it** — `todo` · Needs: P1.3
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

## Phase 10 — Physical assembly (unfrozen)

The 8 physical parts (log family, plank, stake, stone block, thatch bundle) with full geometry,
notches, mitre cuts, crossing diagonals; replaces simple placement everywhere (no old path left).

## Phase 11 — Story

Traces, ruins, old settlements in the places reserved in P7.2. No new mechanic for the story.

## Phase 12 — Polish and demo

Sound, UI, performance, full save/load, a 60–90 minute Steam demo.

**Milestone C — "Primitive era complete"**.

---

## Open questions for the user (blocking)

| Id | Question | Blocks |
|---|---|---|
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

- 2026-10-29 · design + roadmap written · — · commit after 85569c6
