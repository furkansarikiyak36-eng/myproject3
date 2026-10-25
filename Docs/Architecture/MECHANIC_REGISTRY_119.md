# Authoritative 119-Mechanic Scope Registry

**Version:** 1.0  
**Status:** Scope locked; implementation and verification are not implied.  
**Canonical IDs:** `M001`–`M119`; never renumber or reuse.

## Provenance and scope decision

The user's detailed MyProject2 mechanics catalog and `MEKANIK_DERINLIK_PLANI.md` describe a much larger catalog (approximately 480–500 sub-mechanics). A historical report also states 119 checks passed, but the exact historical 119-name list was not found in the reviewed files. This registry therefore establishes an explicit 119-item scope from this commit onward; it does **not** claim to recover the missing historical test list.

All items start as `unreviewed`. Registry inclusion is not proof of implementation, successful compilation, or runtime validation.

## System allocation

| ID range | System | Primary owner | Count |
|---|---|---|---:|
| M001–M008 | Core / World Contracts | `CoreWorld` | 8 |
| M009–M028 | Log / Wood Processing | `LogSystem` | 20 |
| M029–M038 | Stone / Masonry | `StoneSystem` | 10 |
| M039–M048 | Stick / Primitive Tools | `StickSystem` | 10 |
| M049–M058 | Inventory / Crafting | `InventoryCrafting` | 10 |
| M059–M070 | Shared Structure / Damage | `StructuralGraph` | 12 |
| M071–M080 | Terrain / World State / Digging | `WorldStateTerrain` | 10 |
| M081–M090 | Farming / Animal Husbandry | `FarmingSimulation` | 10 |
| M091–M100 | Raid Director / Threat AI | `RaidDirector` | 10 |
| M101–M119 | Logistics / Production / Survival / Exploration | `LogisticsProduction` | 19 |

**Total: 119 mechanics.**

## Canonical list

| ID | Mechanic | Primary owner | Prerequisites / dependencies | Persistence | Acceptance criterion | Status |
|---|---|---|---|---|---|---|
| M001 | Stable identity for physical world objects | `CoreWorld` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M002 | Exclusive object lifecycle state | `CoreWorld` | M001 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M003 | Atomic world-changing operation | `CoreWorld` | M002 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M004 | Structured operation result and failure reason | `CoreWorld` | M003 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M005 | Committed domain events | `CoreWorld` | M003 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M006 | Deterministic world clock and random seed | `CoreWorld` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M007 | Versioned save/load and migration boundary | `CoreWorld` | M001, M006 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M008 | Shared survival state: health, stamina, warmth and needs | `CoreWorld` | M006 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M009 | Fell a tree with tool hits | `LogSystem` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M010 | Generate logs from tree dimensions | `LogSystem` | M009 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M011 | Pick up and carry a loose log | `LogSystem` | M010 | `save_required` | Reject a groove on a whole log; accept only after the required longitudinal split, and leave state unchanged on rejection. | `unreviewed` |
| M012 | Drop or quick-release a carried log | `LogSystem` | M010 | `save_required` | Reject a trough unless the configured split-half prerequisite and output mesh exist; successful output changes piece type and geometry together. | `unreviewed` |
| M013 | Roll, drag and push a loose log | `LogSystem` | M010 | `save_required` | Reject plank production from an unprocessed whole log; success produces the configured plank output without duplicating material. | `unreviewed` |
| M014 | Log collision and impact damage | `LogSystem` | M010 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M015 | Cross-cut log to a valid length | `LogSystem` | M010 | `save_required` | A spike is not an operational trap until its grounding/placement prerequisites pass. | `unreviewed` |
| M016 | Split a whole log longitudinally | `LogSystem` | M001 | `save_required` | Generated and authored anchor IDs remain unique; regeneration does not erase authored anchors. | `unreviewed` |
| M017 | Quarter a split-half log | `LogSystem` | M016 | `save_required` | Preview and committed transform agree; placement requires a valid support/surface for the selected mode. | `unreviewed` |
| M018 | Hew or square a log | `LogSystem` | M016 | `save_required` | A structural connection is an explicit graph relation; mesh overlap alone does not create a connection. | `unreviewed` |
| M019 | Cut a longitudinal groove only after splitting | `LogSystem` | M018 | `save_required` | Removing a support resolves dependents or rejects the operation without orphaning graph links. | `unreviewed` |
| M020 | Make a trough or channel | `LogSystem` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M021 | Produce a plank from a split log | `LogSystem` | M020 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M022 | Cut notches and interlocking joints | `LogSystem` | M021 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M023 | Shape a grounded spike | `LogSystem` | M022 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M024 | Generate and validate snap anchors | `LogSystem` | M023 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M025 | Place logs horizontally, vertically and diagonally | `LogSystem` | M024 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M026 | Maintain explicit parent-child construction links | `LogSystem` | M025 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M027 | Remove a support and resolve collapse | `LogSystem` | M026 | `save_required` | A trap cannot arm until all components, grounding and trigger conditions are valid. | `unreviewed` |
| M028 | Repair, salvage and burn wood | `LogSystem` | M027 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M029 | Collect, hold and throw loose stone | `StoneSystem` | M028 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M030 | Shape raw stone into building pieces | `StoneSystem` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M031 | Place and align individual stones | `StoneSystem` | M030 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M032 | Stack stones horizontally and vertically | `StoneSystem` | M031 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M033 | Build a stone wall and corner | `StoneSystem` | M032 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M034 | Build stone columns, beams and openings | `StoneSystem` | M033 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M035 | Resolve stone support, balance and collapse | `StoneSystem` | M034 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M036 | Damage, repair and salvage stone structures | `StoneSystem` | M033 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M037 | Store and haul stone in bulk | `StoneSystem` | M036 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M038 | Connect mixed wood-stone construction and stone defenses | `StoneSystem` | M033 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M039 | Gather and break sticks | `StickSystem` | M038 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M040 | Sharpen a stick or make a double-ended stake | `StickSystem` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M041 | Craft primitive tools and melee weapons | `StickSystem` | M040 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M042 | Craft spear, bow and arrows | `StickSystem` | M041 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M043 | Make torches and simple light sources | `StickSystem` | M041 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M044 | Build stakes, fences and simple barriers | `StickSystem` | M042 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M045 | Build primitive traps and trigger lines | `StickSystem` | M044 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M046 | Build stick storage, racks and furniture | `StickSystem` | M044 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M047 | Build simple garden and animal-support structures | `StickSystem` | M046 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M048 | Dismantle stick structures and recover materials | `StickSystem` | M047 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M049 | Pick up a world item into inventory | `InventoryCrafting` | M048 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M050 | Stack items and enforce carry limits | `InventoryCrafting` | M049 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M051 | Show item category, properties and quick access | `InventoryCrafting` | M001 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M052 | Place ingredients into a crafting area | `InventoryCrafting` | M051 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M053 | Resolve recipes and alternate valid recipes | `InventoryCrafting` | M052 | `save_required` | Reject excavation in protected layers; permitted layer edits change geometry and collision consistently. | `unreviewed` |
| M054 | Show missing ingredients and required quantities | `InventoryCrafting` | M053 | `save_required` | Tunnel/room excavation stays within configured depth/volume limits and yields valid connected geometry. | `unreviewed` |
| M055 | Confirm crafting and consume materials atomically | `InventoryCrafting` | M054 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M056 | Preserve tools and return unused materials | `InventoryCrafting` | M051 | `save_required` | Terrain edits and affected durable objects restore consistently after save/load. | `unreviewed` |
| M057 | Cancel crafting and collect the result | `InventoryCrafting` | M056 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M058 | Craft repair kits, protection and trap components | `InventoryCrafting` | M057 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M059 | Use one authoritative structural graph | `StructuralGraph` | M057 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M060 | Validate support path from ground to load | `StructuralGraph` | M051 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M061 | Enforce span, overhang and load limits | `StructuralGraph` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M062 | Apply damage thresholds to structural links | `StructuralGraph` | M061 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M063 | Collapse dependent pieces in a stable order | `StructuralGraph` | M062 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M064 | Create and remove doors, windows and wall openings | `StructuralGraph` | M063 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M065 | Build floors, roofs, ramps and stairs | `StructuralGraph` | M063 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M066 | Build bridges, rope crossings and elevated walkways | `StructuralGraph` | M061 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M067 | Build gates, fences, railings and defensive walls | `StructuralGraph` | M061 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M068 | Damage and repair a structure without duplicating state | `StructuralGraph` | M067 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M069 | Dismantle structures and recover resources | `StructuralGraph` | M061 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M070 | Prevent unsafe pickup/removal of supporting pieces | `StructuralGraph` | M061 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M071 | Divide world state into persistent chunks | `WorldStateTerrain` | — | `save_required` | Fixed seed and time steps reproduce identical simulation outputs. | `unreviewed` |
| M072 | Edit only configured diggable terrain layers | `WorldStateTerrain` | M006 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M073 | Dig a pit with valid terrain geometry and collision | `WorldStateTerrain` | M071, M072 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M074 | Dig a horizontal tunnel and small room | `WorldStateTerrain` | M073 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M075 | Prevent unsupported underground collapse | `WorldStateTerrain` | M074 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M076 | Preserve terrain edits across save/load | `WorldStateTerrain` | M074 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M077 | Simulate water level, flow and local flooding | `WorldStateTerrain` | M072 | `save_required` | Failed threat spawn/queue requests do not leak population counters or create phantom enemies. | `unreviewed` |
| M078 | Simulate soil moisture, nutrients and surface runoff | `WorldStateTerrain` | M077 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M079 | Track fire spread, rot and material exposure | `WorldStateTerrain` | M078 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M080 | Place and discover environmental ruins and clues | `WorldStateTerrain` | M077 | `save_required` | Threat damage must mutate the same authoritative structure integrity used by construction and persistence. | `unreviewed` |
| M081 | Advance world time with deterministic simulation ticks | `FarmingSimulation` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M082 | Plant seeds and validate soil/season requirements | `FarmingSimulation` | M081 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M083 | Grow crops through defined stages | `FarmingSimulation` | M082 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M084 | Apply watering, nutrients and plant stress | `FarmingSimulation` | M083 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M085 | Harvest crops and calculate yield | `FarmingSimulation` | M084 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M086 | Keep and feed farm animals | `FarmingSimulation` | M081 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M087 | Advance animal growth and health | `FarmingSimulation` | M086 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M088 | Produce milk, eggs, wool or other configured goods | `FarmingSimulation` | M086 | `save_required` | Feed consumption, animal outputs and manure/nutrient updates reconcile without duplicate stock. | `unreviewed` |
| M089 | Reproduce animals within habitat capacity | `FarmingSimulation` | M087 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M090 | Connect feed, manure and soil nutrient cycles | `FarmingSimulation` | M087 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M091 | Schedule and gate threat events | `RaidDirector` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M092 | Evaluate difficulty from player/world context | `RaidDirector` | M006 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M093 | Plan enemy groups, waves and resource budget | `RaidDirector` | M092 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M094 | Enforce spawn capacity and queue limits | `RaidDirector` | M093 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M095 | Choose valid spawn locations | `RaidDirector` | M094 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M096 | Select targets and compute navigation paths | `RaidDirector` | M095 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M097 | Resolve attack, bypass and structure damage | `RaidDirector` | M093 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M098 | Advance wave state machine | `RaidDirector` | M097 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M099 | Cancel, interrupt and recover a failed raid | `RaidDirector` | M097 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M100 | Reassess pressure from settlement noise, stock and defenses | `RaidDirector` | M098 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M101 | Load and unload carts, sleds and storage racks | `LogisticsProduction` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M102 | Transport materials by rope line or zipline | `LogisticsProduction` | M006 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M103 | Create primitive saw/production chains | `LogisticsProduction` | M102 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M104 | Queue production and expose blocked reasons | `LogisticsProduction` | M103 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M105 | Manage fuel, heat and cooking stations | `LogisticsProduction` | M104 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M106 | Use roads and bridges to reduce route cost | `LogisticsProduction` | M105 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M107 | Manage shelter, temperature and daily survival choices | `LogisticsProduction` | M105 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M108 | Expose resource, durability and storage status to the player | `LogisticsProduction` | M107 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M109 | Demonstrate integrated save/load across world and production | `LogisticsProduction` | M108 | `save_required` | Only one authoritative state may own a physical object during world/inventory/storage/production transfers. | `unreviewed` |
| M110 | Simulate weather and seasonal transitions | `LogisticsProduction` | M108 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M111 | Apply cold, heat and wetness exposure | `LogisticsProduction` | — | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M112 | Track hunger and thirst consumption | `LogisticsProduction` | M111 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M113 | Recover stamina through rest and activity limits | `LogisticsProduction` | M112 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M114 | Apply sleep and fatigue effects | `LogisticsProduction` | M112 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M115 | Apply injury, illness and poisoning state | `LogisticsProduction` | M114 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M116 | Preserve food quality and spoilage over time | `LogisticsProduction` | M114 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M117 | Apply clothing insulation and protective equipment | `LogisticsProduction` | M112 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M118 | Make shelter quality affect environmental exposure | `LogisticsProduction` | M117 | `save_required` | Valid context produces the documented result; invalid target, missing configuration or unmet prerequisites fail without partial mutation. | `unreviewed` |
| M119 | Make exploration discoveries persist and unlock world context | `LogisticsProduction` | M111 | `save_required` | One integrated save/load cycle restores terrain, structures, inventory, production, farming and threat progression consistently. | `unreviewed` |

## Non-negotiable invariants

1. A groove, trough or plank cannot be applied to an unprocessed whole log.
2. Spikes and traps do not become operational before placement, grounding and trigger prerequisites pass.
3. Physical assembly uses explicit anchors and structural relations; mesh overlap alone is not a connection.
4. A failed operation changes neither authoritative state nor material quantities.
5. Physical objects, inventory, storage and production have one authoritative owner at a time; transfers are atomic.
6. Digging is restricted to configured terrain layers and edits persist through save/load.
7. Farming, animal needs, production and raids use deterministic time and authoritative state/resource owners.
8. Historical test counts, source presence and static checks do not substitute for UE 5.8 build and editor/runtime evidence.

## Phase 1 closure checklist

- [x] Create an explicit, stable, numbered scope of exactly 119 mechanics.
- [x] Assign one intended primary owner system per entry.
- [x] Record dependencies, persistence expectations, failure cases and acceptance criteria.
- [x] Distinguish scope definition from implementation and verification status.
- [ ] Reconcile every entry against actual C++/Blueprint/editor call sites and attach source evidence.
- [ ] Run available automated tests; record exact scope and result.
- [ ] Run UE 5.8 build and editor/runtime validation at the agreed later integration gate.
- [ ] Build the final demo to cover all 119 mechanics in appropriate locations without clutter.
