# Reference game research dossier

First pass, 2026-11-02. Written from general knowledge of the games, **not** from playing them in this
project or from sourced articles; claims about hidden rules are likely, not confirmed. A later pass can add
sources or playtest notes per game. Each game: the five questions of the plan, then what it means for us
(requirement ids from `SYSTEMS_REGISTER.md`; class: exists / partial / missing / demo / later / not for us).

## Death Stranding
1. Visible: carry stacked cargo across rough land; ladders, ropes, bridges, roads built by players.
2. Hidden: cargo weight and stack height change balance; terrain slope and surface (rocks, water depth) set stumble risk; cargo and boots wear.
3. Links: cargo ↔ terrain ↔ infrastructure ↔ weather (timefall wears cargo).
4. Edge cases: rivers too deep to wade; ladders too short; structures decay without maintenance.
5. Long term: the map gets a network of shortcuts that makes later trips fast.
- For us: V1 load/balance (partial), V2 surface cost (partial), V3/V4 ropes and bridges (partial), B3 wear (missing). Demo: one crossing. Not for us: delivery-job structure.

## 7 Days to Die
1. Visible: dig and build with blocks; fortify against horde nights.
2. Hidden: structural integrity per block (load passes down to ground); block health per material; zombies path to weak points.
3. Links: digging ↔ materials ↔ building ↔ AI pathing ↔ horde schedule.
4. Edge cases: long overhangs collapse; digging under a base drops it; AI finds the cheapest breach.
5. Long term: bases become engineered fortresses; the terrain is reshaped.
- For us: A7/A8 load and collapse (partial), T7–T10 digging and collapse (partial, frozen), M5–M7 raids and breaching (partial/designed). Not for us: fixed horde nights (we use signals), full-voxel world (cost, see WORLD_PLAN).

## Farming Simulator
1. Visible: fields worked step by step (cultivate, sow, fertilise, harvest), animals fed and kept.
2. Hidden: per-field state layers at sub-metre resolution (worked, sown, growth stage, fertiliser, weeds, lime); crop calendar.
3. Links: field state ↔ season ↔ yield ↔ storage ↔ animals (feed, manure).
4. Edge cases: wrong season, missing lime/fertiliser, weeds and stones lower yield.
5. Long term: a yearly rhythm and a farm economy.
- For us: G2 field cycle (partial), G3 soil (partial), G5 presets (partial), field state as data layers (we have field cells), dig areas as "field logic" (D8). Not for us: vehicle and machine economy.

## Enshrouded
1. Visible: voxel terrain shaping and digging; building with blocks; a corrupting fog.
2. Hidden: terrain is voxel with material types; edits are persisted; base area limits building; fog hazard has a timer and gear protection.
3. Links: terrain edits ↔ building ↔ hazard zones ↔ progression gear.
4. Edge cases: editing near world features is restricted; fog damage if protection runs out.
5. Long term: bases carved into the land; regions opened as protection grows.
- For us: T7/T12 voxel–world transitions (partial), X1–X7 hazard with protection gear (missing), T5 base areas/claims (D10).

## Sons of the Forest
1. Visible: fell trees, carry logs, cut them, place them with snap guides; free-form structures.
2. Hidden: logs as physical pieces with lengths and halves; snap points along logs; structural support; AI reacts to the base and the player's actions.
3. Links: trees ↔ logs ↔ building ↔ enemies ↔ companions.
4. Edge cases: unsupported pieces fall; cut halves stay halves; enemies damage walls.
5. Long term: forest is cleared around the base; large structures.
- For us: A2–A10 (built, the most advanced area), A11 joinery (later), helper companion (built), B6 defences (partial).

## Satisfactory
1. Visible: build production lines; balance machine counts.
2. Hidden: fixed rates per recipe (items per minute); buffers; power budget; bottlenecks show as idle machines.
3. Links: resources ↔ recipes ↔ logistics ↔ power.
4. Edge cases: one starved input stalls the line; overflow fills buffers.
5. Long term: the economy scales, the map fills with factories.
- For us: P3/P5 recipe rates and bottlenecks (partial/missing), I4 stations pulling from storage (partial). Not for us: modern automation (D16).

## Hydroneer
1. Visible: dig dirt, wash it in pans/sluices, sell or process the found ore.
2. Hidden: dirt has volume and hidden content; buckets hold a volume; machines process by volume and time.
3. Links: digging ↔ containers ↔ processing ↔ purity/value.
4. Edge cases: overfilled containers spill; wrong machine gives nothing.
5. Long term: bigger processing chains from the same dirt.
- For us: R4/R5 bulk materials by volume and containers (missing, M1), dig yield by ground id (T11, partial), I5 spill (missing).

## Valheim
1. Visible: build houses that need support, roofs and fire; comfort; biome progression.
2. Hidden: structural integrity colour-coded from ground; smoke rises and fills closed roofs; rain wears uncovered wood; comfort counts nearby items; biomes gate by gear.
3. Links: shelter ↔ fire ↔ smoke ↔ rest ↔ stamina; materials ↔ biomes.
4. Edge cases: smoke chokes the player in a sealed roof; wood decays in rain; too-tall builds collapse.
5. Long term: bases grow with comfort; progression through biomes.
- For us: H3 smoke and ventilation (missing), B3 rain wear (missing), H4 comfort (partial), A7 support (partial), T5 region gating (data).
