# Phase 14 — M001–M119 Integration and Demo Coverage Matrix

**Purpose:** turn the locked 119-item registry into an auditable integration/demo checklist. This is a planning/evidence ledger, not a claim that the mechanics work.

**Rules**
- The canonical registry remains unchanged; every entry remains `unreviewed` until reviewed against implementation and validated at the agreed UE 5.8 gate.
- “Demo location” means the intended compact test area, not a level that has already been built.
- Source evidence, dependency evidence, save/load evidence and UE runtime evidence must be attached before any row is accepted.
- Do not mark an item complete from source-file presence, static inspection, or another mechanic's success.

| ID | Canonical mechanic | Owner | Dependencies | Intended demo area | Evidence required | Status |
|---|---|---|---|---|---|---|
| M001 | Stable identity for physical world objects | `CoreWorld` | — | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M002 | Exclusive object lifecycle state | `CoreWorld` | M001 | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M003 | Atomic world-changing operation | `CoreWorld` | M002 | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M004 | Structured operation result and failure reason | `CoreWorld` | M003 | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M005 | Committed domain events | `CoreWorld` | M003 | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M006 | Deterministic world clock and random seed | `CoreWorld` | — | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M007 | Versioned save/load and migration boundary | `CoreWorld` | M001, M006 | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M008 | Shared survival state: health, stamina, warmth and needs | `CoreWorld` | M006 | Core test bench | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M009 | Fell a tree with tool hits | `LogSystem` | — | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M010 | Generate logs from tree dimensions | `LogSystem` | M009 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M011 | Pick up and carry a loose log | `LogSystem` | M010 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M012 | Drop or quick-release a carried log | `LogSystem` | M011 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M013 | Roll, drag and push a loose log | `LogSystem` | M011 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M014 | Log collision and impact damage | `LogSystem` | M013 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M015 | Cross-cut log to a valid length | `LogSystem` | M010 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M016 | Split a whole log longitudinally | `LogSystem` | M015 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M017 | Quarter a split-half log | `LogSystem` | M016 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M018 | Hew or square a log | `LogSystem` | M015 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M019 | Cut a longitudinal groove only after splitting | `LogSystem` | M016 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M020 | Make a trough or channel | `LogSystem` | M016 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M021 | Produce a plank from a split log | `LogSystem` | M016 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M022 | Cut notches and interlocking joints | `LogSystem` | M018 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M023 | Shape a grounded spike | `LogSystem` | M015 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M024 | Generate and validate snap anchors | `LogSystem` | M001 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M025 | Place logs horizontally, vertically and diagonally | `LogSystem` | M024 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M026 | Maintain explicit parent-child construction links | `LogSystem` | M024 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M027 | Remove a support and resolve collapse | `LogSystem` | M026 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M028 | Repair, salvage and burn wood | `LogSystem` | M027 | Wood/log worksite | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M029 | Collect, hold and throw loose stone | `StoneSystem` | — | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M030 | Shape raw stone into building pieces | `StoneSystem` | M029 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M031 | Place and align individual stones | `StoneSystem` | M030 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M032 | Stack stones horizontally and vertically | `StoneSystem` | M031 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M033 | Build a stone wall and corner | `StoneSystem` | M032 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M034 | Build stone columns, beams and openings | `StoneSystem` | M033 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M035 | Resolve stone support, balance and collapse | `StoneSystem` | M034 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M036 | Damage, repair and salvage stone structures | `StoneSystem` | M035 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M037 | Store and haul stone in bulk | `StoneSystem` | M029 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M038 | Connect mixed wood-stone construction and stone defenses | `StoneSystem` | M033, M026 | Stone/masonry pad | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M039 | Gather and break sticks | `StickSystem` | — | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M040 | Sharpen a stick or make a double-ended stake | `StickSystem` | M039 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M041 | Craft primitive tools and melee weapons | `StickSystem` | M039 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M042 | Craft spear, bow and arrows | `StickSystem` | M041 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M043 | Make torches and simple light sources | `StickSystem` | M039 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M044 | Build stakes, fences and simple barriers | `StickSystem` | M040 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M045 | Build primitive traps and trigger lines | `StickSystem` | M044 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M046 | Build stick storage, racks and furniture | `StickSystem` | M039 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M047 | Build simple garden and animal-support structures | `StickSystem` | M046 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M048 | Dismantle stick structures and recover materials | `StickSystem` | M044 | Stick/tool camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M049 | Pick up a world item into inventory | `InventoryCrafting` | — | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M050 | Stack items and enforce carry limits | `InventoryCrafting` | M049 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M051 | Show item category, properties and quick access | `InventoryCrafting` | M049 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M052 | Place ingredients into a crafting area | `InventoryCrafting` | M049 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M053 | Resolve recipes and alternate valid recipes | `InventoryCrafting` | M052 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M054 | Show missing ingredients and required quantities | `InventoryCrafting` | M053 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M055 | Confirm crafting and consume materials atomically | `InventoryCrafting` | M053 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M056 | Preserve tools and return unused materials | `InventoryCrafting` | M055 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M057 | Cancel crafting and collect the result | `InventoryCrafting` | M055 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M058 | Craft repair kits, protection and trap components | `InventoryCrafting` | M055 | Inventory/crafting station | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M059 | Use one authoritative structural graph | `StructuralGraph` | — | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M060 | Validate support path from ground to load | `StructuralGraph` | M059 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M061 | Enforce span, overhang and load limits | `StructuralGraph` | M060 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M062 | Apply damage thresholds to structural links | `StructuralGraph` | M059 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M063 | Collapse dependent pieces in a stable order | `StructuralGraph` | M060 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M064 | Create and remove doors, windows and wall openings | `StructuralGraph` | M059 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M065 | Build floors, roofs, ramps and stairs | `StructuralGraph` | M060 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M066 | Build bridges, rope crossings and elevated walkways | `StructuralGraph` | M065 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M067 | Build gates, fences, railings and defensive walls | `StructuralGraph` | M059 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M068 | Damage and repair a structure without duplicating state | `StructuralGraph` | M062 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M069 | Dismantle structures and recover resources | `StructuralGraph` | M059 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M070 | Prevent unsafe pickup/removal of supporting pieces | `StructuralGraph` | M060 | Build-and-damage yard | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M071 | Divide world state into persistent chunks | `WorldStateTerrain` | — | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M072 | Edit only configured diggable terrain layers | `WorldStateTerrain` | M071 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M073 | Dig a pit with valid terrain geometry and collision | `WorldStateTerrain` | M072 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M074 | Dig a horizontal tunnel and small room | `WorldStateTerrain` | M073 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M075 | Prevent unsupported underground collapse | `WorldStateTerrain` | M074 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M076 | Preserve terrain edits across save/load | `WorldStateTerrain` | M071 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M077 | Simulate water level, flow and local flooding | `WorldStateTerrain` | M071 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M078 | Simulate soil moisture, nutrients and surface runoff | `WorldStateTerrain` | M077 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M079 | Track fire spread, rot and material exposure | `WorldStateTerrain` | M071 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M080 | Place and discover environmental ruins and clues | `WorldStateTerrain` | M071 | Terrain test cells | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M081 | Advance world time with deterministic simulation ticks | `FarmingSimulation` | — | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M082 | Plant seeds and validate soil/season requirements | `FarmingSimulation` | M078, M081 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M083 | Grow crops through defined stages | `FarmingSimulation` | M082 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M084 | Apply watering, nutrients and plant stress | `FarmingSimulation` | M083 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M085 | Harvest crops and calculate yield | `FarmingSimulation` | M083 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M086 | Keep and feed farm animals | `FarmingSimulation` | M081 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M087 | Advance animal growth and health | `FarmingSimulation` | M086 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M088 | Produce milk, eggs, wool or other configured goods | `FarmingSimulation` | M086 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M089 | Reproduce animals within habitat capacity | `FarmingSimulation` | M087 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M090 | Connect feed, manure and soil nutrient cycles | `FarmingSimulation` | M085, M086 | Farm/animal pen | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M091 | Schedule and gate threat events | `RaidDirector` | M006 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M092 | Evaluate difficulty from player/world context | `RaidDirector` | M091 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M093 | Plan enemy groups, waves and resource budget | `RaidDirector` | M092 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M094 | Enforce spawn capacity and queue limits | `RaidDirector` | M093 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M095 | Choose valid spawn locations | `RaidDirector` | M094 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M096 | Select targets and compute navigation paths | `RaidDirector` | M095 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M097 | Resolve attack, bypass and structure damage | `RaidDirector` | M096, M062 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M098 | Advance wave state machine | `RaidDirector` | M093 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M099 | Cancel, interrupt and recover a failed raid | `RaidDirector` | M098 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M100 | Reassess pressure from settlement noise, stock and defenses | `RaidDirector` | M091, M086 | Raid perimeter | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M101 | Load and unload carts, sleds and storage racks | `LogisticsProduction` | — | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M102 | Transport materials by rope line or zipline | `LogisticsProduction` | M101 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M103 | Create primitive saw/production chains | `LogisticsProduction` | M101 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M104 | Queue production and expose blocked reasons | `LogisticsProduction` | M103 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M105 | Manage fuel, heat and cooking stations | `LogisticsProduction` | M043 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M106 | Use roads and bridges to reduce route cost | `LogisticsProduction` | M066 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M107 | Manage shelter, temperature and daily survival choices | `LogisticsProduction` | M008 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M108 | Expose resource, durability and storage status to the player | `LogisticsProduction` | M051 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M109 | Demonstrate integrated save/load across world and production | `LogisticsProduction` | M076, M104, M090, M098 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M110 | Simulate weather and seasonal transitions | `LogisticsProduction` | M006 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M111 | Apply cold, heat and wetness exposure | `LogisticsProduction` | M110 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M112 | Track hunger and thirst consumption | `LogisticsProduction` | M006 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M113 | Recover stamina through rest and activity limits | `LogisticsProduction` | M008 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M114 | Apply sleep and fatigue effects | `LogisticsProduction` | M113 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M115 | Apply injury, illness and poisoning state | `LogisticsProduction` | M008 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M116 | Preserve food quality and spoilage over time | `LogisticsProduction` | M006 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M117 | Apply clothing insulation and protective equipment | `LogisticsProduction` | M111 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M118 | Make shelter quality affect environmental exposure | `LogisticsProduction` | M107, M111 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |
| M119 | Make exploration discoveries persist and unlock world context | `LogisticsProduction` | M080 | Survival/logistics camp | Source mapping + positive/negative runtime case + save/load round-trip where required | `unreviewed` |

## Coverage exit gate

- [ ] All 119 IDs have a source mapping and named responsible owner.
- [ ] Dependencies are exercised in valid order and failure paths are checked for partial mutation.
- [ ] Each `save_required` mechanic has a save → reload → state-equivalence case.
- [ ] Each mechanic has a compact demo placement and a reset/replay procedure.
- [ ] Logs, evidence bundle, engine version, commit SHA and exact test scope are recorded.
- [ ] Final demo remains readable: dedicated test stations/areas, not every debug object piled into one scene.
