# MyProject3 — Inventory and Resource Catalogue

**Status:** Design catalogue. Entries are planned logical content unless separately marked in implementation evidence.  
**No vendor asset packs or textures are specified.**

## 1. Rules

- Large pieces (full logs, large boulders, long beams) remain physical world actors; they are not silently converted into abstract stack items.
- Small resources may stack by item definition and stack limit.
- Weight and volume are separate concepts. A stone can be heavy; a long log can be awkward and obstructive even when its weight is manageable.
- Every item has a stable ID independent of its localized name or mesh file name.
- Processing state, durability, wetness, contamination, freshness, and crop phase are explicit only when they affect gameplay.
- Inventory, carry, world pickup, drop, crafting, construction, and save/load must agree about ownership. One item cannot exist simultaneously in two owners.
- Failed crafting/placement must be atomic: no consumed inputs and no partial output on failure.
- Critical state must survive save/load, including partially processed logs and terrain edits.

## 2. Required item schema

| Field | Purpose |
|---|---|
| ItemId | Stable identifier, e.g. MAT_WOOD_BRANCH |
| DisplayNameKey | Localization key, never the persistent identity |
| Category | RawMaterial, ProcessedMaterial, Tool, Equipment, Consumable, Seed, BuildPart, Story, TechPart |
| WorldRepresentation | PhysicalActor, StackablePickup, EquipmentOnly, StoryRecord |
| UnitMass / UnitVolume | Carry calculations |
| MaxStack | Stack limit; 1 for non-stackable items |
| DurabilityModel | None, Durability, ConsumableCharges, Freshness, FilterLife |
| StateModel | None, ProcessingState, CropState, ContaminationState, Wetness, Freshness |
| AcquisitionSources | Source actor/region/recipe |
| RecipeInputs / RecipeOutputs | Data references for transformations |
| AllowedActions | Pickup, Drop, Use, Equip, Process, Place, Consume, Inspect, Repair |
| WorldPersistencePolicy | How dropped/placed objects are saved |
| RequiredFeedback | UI, animation, sound, VFX identifiers |
| Status | Planned / implemented-unverified / verified / deferred |

## 3. Initial catalogue by family

### A. Wood and plant-derived material
- World/physical: tree, stump, fallen tree, whole log (long/short), branch, loose stick.
- Processing states: measured cut in progress, whole log, half-length log, split half-round, quarter-round, hewn/squared log, grooved half-log, hollowed trough, plank, pole, bundle of kindling.
- Small resources: bark, plant fiber, resin, reeds, leaves, seeds, mushrooms.
- Uses: shelter pieces, bridge parts, tools/handles, bindings, traps, fuel, crop inputs.
- Important distinction: half-length log and split half-round are different geometries/states.

### B. Stone and mineral
- World/physical: large boulder, rock outcrop, loose rock cluster.
- Outputs: fractured chunk, stone piece, shaped stone, gravel.
- Tools/processes: pickaxe/mallet, staged fracture, shaping, sorting.
- Uses: foundations, masonry, stove/hearth, simple tools, drainage, path surfaces.
- Required states: intact → cracked → fractured/pieces; hit feedback and yields should be consistent.

### C. Terrain-derived material
- Soil, contaminated soil, cleaned soil, sand, gravel, clay, mud.
- Terrain/material properties: hardness, diggability, moisture, contamination, support requirement, possible resource yield.
- Processing: digging, sifting, drying, cleaning, compaction, shaping, firing where supported.
- Uses: planting beds, fill, drainage, simple blocks/ceramics and paths.
- Voxel excavation produces defined outputs; do not spawn one inventory item for every edited voxel.

### D. Food and farming
- Seeds and seedlings per crop type; immature plant; mature crop; harvested produce; fiber/seed by-products.
- Minimum first crop set should be small and functionally distinct, not dozens of reskins.
- Each crop entry defines sowing window, growth duration by season, light/water/soil conditions, harvest outputs, seed return chance or rule, and failure/dormancy conditions.
- Food items define freshness or preservation only where it creates a real gameplay choice.

### E. Survival equipment and consumables
- Drinking water, basic food, first-aid supplies, respirator, replaceable filters, protective suit/parts, sleep/camp kit, fuel, light source and batteries.
- Filter life and exposure protection are separate fields; equipment must communicate remaining protection.
- Do not model detailed physiology unless a clear gameplay need is documented.

### F. Tools and craft equipment
- Axe, hand saw, splitting wedge, hammer, chisel, pickaxe, shovel, binding/rope kit, repair kit, simple watering item, seed pouch, lamp, measurement device.
- Tool definitions state what material/state they act on, required interaction, wear, action duration, output, and feedback.
- A tool should not be a generic key that bypasses the world state.

### G. Old-world technology
- Metal scrap, copper wire, cable, battery, circuit board, sensor, filter module, salvaged component, recorder/data module.
- Initial technology progression is repair, salvage, and reuse—not a full ore-to-steel factory or player-built robot industry.
- Every rare tech part must have at least one clear source and one clear use before it is included in the active economy.

### H. Construction and defense
- Log/beam/pole, plank, stone piece, support post, floor/deck, wall, roof, door/gate, stair/ramp, rope/binding, tunnel support, simple barricade, trap, alarm.
- Build parts are configuration-driven where possible. Anchor compatibility is owned by material-specific providers; the shared placement kernel remains material-neutral.
- Structural load/collapse rules should be added only for defined piece classes and bounded scenarios.

### I. Exploration and story
- Map/note, recorder, log entry, old-world key/tag, repairable module, measurement sample, region marker.
- Story records are inspectable/readable data, not ordinary stack items unless there is a real carrying decision.

## 4. Transformation record schema

Every recipe/process must define:

- RecipeId and version.
- Input item IDs and quantities.
- Preconditions: tool, station, terrain, weather, crop state, contamination, or piece state.
- Operation: immediate interaction or progress-based action.
- Duration and interruption semantics.
- Output item IDs/quantities and by-products.
- Tool wear, fuel/water/time cost, noise/light, and risk if relevant.
- Failure path and rollback policy.
- UI message, animation, sound, and VFX event IDs.
- Save/load representation for in-progress operations.

## 5. Example transformation chains

| Chain | Inputs/process | Outputs | Meaningful trade-off |
|---|---|---|---|
| Wood | Tree → log → split/shape | Beam, plank, pole, fuel, trap component | Building now vs heat/tools later |
| Stone | Boulder → staged fracture → shaping/sorting | Pieces, shaped stone, gravel | Strong parts vs drainage/fill |
| Clay | Clay → shape → dry/fire if available | Container or simple construction unit | Time/fuel vs storage/construction |
| Plant | Seed → grow → harvest | Food, seeds, fiber/by-product | Harvest now vs future crop |
| Salvage | Old device → dismantle/repair | Battery, cable, module | Repair intact device vs components |
| Soil | Contaminated ground → cleanup/containment | Safer planting area, recovered local soil | Consumables/time vs regional benefit |

These are target chains, not verified recipes in code.

## 6. Inventory acceptance conditions

- Unique IDs and stack rules are deterministic.
- Moving, splitting, stacking, dropping, equipping, consuming, crafting, and placing do not duplicate or destroy items on failure.
- World actors and inventory items have explicit ownership transitions.
- Physical large pieces cannot be duplicated by inventorial stacking.
- Processing and crop state survive save/load.
- UI displays enough information to explain weight/volume, state, protection life, freshness or durability when relevant.
- Every active recipe references existing item IDs and has a valid output path.
