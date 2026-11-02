# MASTER PLAN — the whole survival game (canonical)

Created 2026-11-02 from the user's "Master Game Production Plan" (Turkish, Parts I–XII) and reconciled
with the repository. This file is the single entry point; everything else links from here.

| Document | Content (required output) |
|---|---|
| this file | Game design (GDD), world production plan, architecture and data contracts, dependency map, phase plan, demo scope and acceptance |
| `SYSTEMS_REGISTER.md` | Mechanics and systems inventory + implementation status register (every requirement, traceable) |
| `OPEN_DECISIONS.md` | Open decisions register (nothing there is decided) |
| `REFERENCE_RESEARCH.md` | Reference game research dossier (8 games x 5 questions) |
| `VERIFICATION_PLAN.md` | Unreal verification plan (blind code vs build vs tests vs PIE) |
| `WORLD_PLAN.md` | World/ground detail and the measured voxel cost |
| `ROADMAP.md`, `PLAN_STATUS.md` | Older task list P0–P12; mapped to the phases below (section 7) |

Three kinds of statement, never mixed:
**Decided** (the user decided it) · **Designed** (discussed, wanted, not final) · **Open** (in `OPEN_DECISIONS.md`).
A design document or a placeholder class is never "implemented" (see status words in `SYSTEMS_REGISTER.md`).

---

## 1. Game identity (GDD)

**Decided**
- Survival / building game, Sons of the Forest style physical building; Unreal Engine 5.8, C++.
- World first, mechanics before looks, story last (2026-10-28).
- Primitive (pre-metal) era: wood, stone, clay, fibre, bone, hide, fire. Copper/bronze/iron are not in the
  base progression (possible later expansion).
- Time is the main resource; seasons, months and years matter.
- Free-form house building; fields are managed with square/rectangle presets.
- Development order: backend and mechanics → one coherent greybox world → materials/textures → models → PCG scatter.
- Code may be written blind on other accounts; Claude Code builds, tests and fixes in batches.

**Designed**
- Premise: a great disaster and explosion made the world toxic; humans are mostly gone. The main character
  wakes in space under automatic commands, the craft falls back to Earth; they learn who they are, what
  happened and how to survive.
- Arc: toxic, lifeless world → safe water, soil and production → plants and farming return → animals return →
  understanding fungal colonies and mutations → old facilities and the main story.
- The world is an ecosystem that changes with the player, not a map full of enemies.
- Early-access target: a coherent 4–5 hour start.
- Toxic hazards: acid rain, toxic gas/smoke, dirty water, contaminated soil, fungal spores, contaminated dust.
- Fungal colonies and mutated humans (5 archetypes: turned, hunter, armoured, spore carrier, advanced form);
  colonies regrow from remaining tissue, never endless spawns.
- Raids driven by world signals (noise, light, smell, food stores, clearing, production, dead mutants, colony
  interference, traces) in five stages: signs, approach, tension, attack, aftermath; local enemy memory.
- Animals: deer, rabbit, wolf, bear (wild); cow, sheep, chicken (domestic). Animals do not turn mutant by default.
- Other human groups: listed, not designed.

## 2. World production plan

**Decided**: one coherent Landscape first; geography after it; landscape components, World Partition cells,
geographic regions, claims and voxel chunks are separate concepts. TerreSculptor exports are a source, not a
runtime dependency.

User's production sequence and where we are:

| # | Step | State |
|---|---|---|
| 1 | Landscape definition | done: `world_layout.json`, single 4033 px landscape, 1 m/px |
| 2 | Main heightmap import | done: `unrelandv5` whole, peaks 300 m, edge mountains to ~800 m (`build_world_heightmap.py`, `-run=BuildValleyWorld`) |
| 3 | Technical validation | partial: built, screenshots, cost measured; not played by the user |
| 4 | Extensibility / World Partition preparation | not started (Open D2) |
| 5 | Heightmap inspection | partial: plan maps (slope, height, relative height, wetness) |
| 6 | Geographic definitions (watersheds, ridges, passes, valleys, rivers, lakes, settlement/farm/forest/resource areas, playable vs later areas) | not started |
| 7 | World regions and technical partitioning (5–6 basins, 12–18 micro-environments; 500 m design cells, 250 m claim cells) | not started |
| 8 | Voxel integration | partial: dig columns + 4 caves, frozen; cave entrance bug; no save |
| 9 | Water, geology, environment | not started on the World map (VerticalSlice_C has a creek) |
| 10 | Traversal and infrastructure | systems exist in code, not placed in the World map |
| 11 | Greybox | partial: stand-in forest, colours; mechanics not placed |
| 12 | Materials and textures | not started |
| 13 | Models | not started |
| 14 | PCG scatter | not started (commandlet scatter from plan maps stands in) |

World size: 4 x 4 km with ~8 km² playable inside edge mountains (user plan said 5 x 5 km envelope, 5–8 km²
playable: Open D1).

## 3. Systems by part (what the game contains)

Each line is a requirement; its status lives in `SYSTEMS_REGISTER.md` under the same id.

- **T Terrain**: T1 single landscape · T2 terrain source pipeline · T3 World Partition · T4 geographic definitions ·
  T5 regions/cells/claims · T6 plan maps · T7 voxel dig · T8 trenches/tunnels/underground rooms · T9 caves ·
  T10 timber shoring, collapse, drainage · T11 geology layers (soil, clay, sand, gravel, rock; hardness; groundwater) ·
  T12 landscape–voxel seams · T13 persistent terrain edits · T14 water bodies · T15 ground query authority.
- **A Physical assembly**: A1 physical item lifecycle · A2 tree felling → trunk · A3 log 600 cm, 8 x 75 cm, 8 side + 2 end snaps ·
  A4 cutting/splitting, half-log persistence · A5 global snap kernel (log, stone, post, plank, wall, platform, stair,
  bridge, roof) · A6 anchors and connection graph · A7 load and support · A8 multi-part collapse · A9 damage, repair,
  dismantle, recovery · A10 doors, windows, openings · A11 advanced joinery (notches, mitre, crossings) · A12 save/ownership.
- **B Building life**: B1 temporary shelters · B2 permanent houses (wood, earth/clay, stone), storage, kitchen, workshop, cellar ·
  B3 wear (rain, rot, plaster wear, leaks, settling, snow/rain load) · B4 fire spread · B5 maintenance · B6 defences
  (fence, palisade, stone wall, gate, watch platform/tower, barricade, ditch, earth bank).
- **V Traversal**: V1 load and balance · V2 slope/mud cost · V3 ropes (anchors, tension, ladder) · V4 bridges (log, supported,
  rope, stone) with span/capacity · V5 walkways, paving, stairs, scaffolds · V6 sleds/cargo.
- **R Resources**: R1 ten families (forest, stone, earth, fibre, animal, plant, water, fuel, farming, special biological) ·
  R2 raw → product chains · R3 material quality · R4 physical objects vs bulk volume vs liquids vs biological items vs
  environment properties · R5 containers (2/10/20/40/50/100 L) with volume and mass separate · R6 targets 35–45 raw,
  20–30 processed, 8–12 special.
- **I Inventory**: I1 slots, weight · I2 volume · I3 hand/arm limits, large carry · I4 storage and stations pulling from storage ·
  I5 spill, spoil, damp, contamination · I6 single ownership (world/held/storage/in-transit/consumed).
- **P Production**: P1 tools (14 listed) · P2 stations (9 listed) · P3 recipe schema (inputs, tools, station, time, fuel,
  intermediates, byproducts, output, quality/durability, maintenance, unlock) · P4 hybrid timing · P5 shared economy, rates.
- **S Survival**: S1 hunger · S2 thirst · S3 nutrition variety · S4 sleep/rest · S5 injury, bleeding, infection, disease ·
  S6 body temperature, wetness, wind chill · S7 clothing · S8 stamina, carrying · S9 hygiene · S10 contamination exposure.
- **F Food and water**: F1 gather, hunt, farm, herd · F2 cook, dry, store, spoil · F3 liquids by volume with properties (clean,
  rain, spring, contaminated).
- **H Fire and shelter**: H1 fuel → heat → shelter → health · H2 insulation, openings, sealing · H3 smoke and ventilation ·
  H4 comfort · H5 fire risk.
- **G Farming**: G1 crops (cereals, legumes, vegetables, fibre/oil, herbs, found seeds) · G2 13-step field cycle ·
  G3 soil (nutrients, moisture, organic matter, compaction, erosion, salinity, acidity, contamination) · G4 water, manure,
  compost, ash, pests, disease, rotation, seed quality, post-harvest loss · G5 field presets.
- **W Wildlife and ecology**: W1 wild species · W2 domestic species (feed, water, shelter, breeding, young, products) ·
  W3 behaviours (feeding, water, breeding, migration, hunting, hiding, disease, population, player effect) ·
  W4 restoration (vegetation, soil, water cleaning, reforestation, habitats, decontamination, colony control).
- **X Toxic world**: X1 acid rain · X2 toxic gas/smoke · X3 contaminated water · X4 contaminated soil · X5 spores ·
  X6 contaminated dust · X7 personal protection (cloth/leather masks, charcoal filters, hoods, eye, clothing, gloves, boots) ·
  X8 sealed rooms, shelters, ventilation, filter slots, drainage · X9 water treatment (cloth, sand/gravel, charcoal,
  ceramic, cistern, rain, boiling, distilling; boiling does not remove chemicals) · X10 contamination in food chain.
- **C Climate**: C1 four seasons · C2 regional climate · C3 natural events (flood, storm, lightning, wildfire, blizzard,
  landslide, rockfall, drought) · C4 toxic events · C5 biological events · C6 event chains · C7 preparation and recovery, warnings.
- **M Mutants and raids**: M1 fungal ecosystem (colonies, mycelium, spore vents, remains, core colonies, deep networks) ·
  M2 five archetypes · M3 wildlife reactions · M4 threat signals · M5 raid stages and types · M6 tactics · M7 defence
  integration, traps, alarms · M8 world memory and colony regrowth.
- **E Progression and story**: E1 pre-metal progression by discovery · E2 old facilities, crash site, history ·
  E3 environmental storytelling · E4 other human groups.
- **Q Technical**: Q1 save/load of every system · Q2 atomic operations, rollback · Q3 versioning, recovery · Q4 fixed-step
  simulation · Q5 simulation LOD for far animals/plants/enemies · Q6 voxel chunk and physics object budgets ·
  Q7 annual simulation tests · Q8 UI and feedback (why an action failed).

## 4. Architecture and data contracts

What every new system must follow (most of it already exists in code):

| Contract | Rule | Where |
|---|---|---|
| Identity | every persistent thing has a stable id | `WorldObjectIdentity*` |
| Ownership | one owner at a time: World / Held / Inventory / Storage / InTransit / Consumed / Transformed; never two | `WorldItemOwnershipSubsystem` |
| Operations | operation id + expected revision; idempotent receipts; all-or-nothing with rollback | ledger, subsystems |
| Fixed step | the simulation coordinator advances every subsystem on the shared clock | `WorldSimulationCoordinatorSubsystem` |
| Game data | rules and numbers in `Config/GameData/*.json`, loaded and checked | `WorldGameDataSubsystem` |
| Save | one coordinator, versioned schema (v31), chained migrations | `WorldSaveCoordinatorSubsystem` |
| Placement | one kernel: solve = preview = commit; providers per material | `Placement/` |
| Structure | the structural graph is the authority for support, damage, collapse | `WorldStructuralGraphSubsystem` |
| Ground | one query for height / ground id / soil depth / diggable; dug places answer with their edited surface (to build: T15) | `AWorldGroundActor` |
| World data | layout and plan maps are generated from `world_layout.json`; the map is rebuilt, not hand-edited | `tools/heightmaps/`, commandlet |
| Amounts (to build) | physical pieces are actors; bulk materials and liquids are volume (L) + density → mass (kg) in containers | R4, R5, I2 |
| Contamination (to build) | contamination is a property of water, soil, food, air; treatments remove only what they can (boiling ≠ chemicals) | X3, X9 |

## 5. Dependency map

```
Clock/calendar ─► Climate ─► Events ─► (crops, structures, routes, survival, raids)
Ground authority (T15) ─► dig/voxel, placement, plants, animals, water, AI
Game data ─► items, recipes, stations, crops, events, zones
Ownership + ledger ─► inventory, storage, production, carry, farming, animals, save
Snap kernel ─► every building piece ─► structural graph ─► damage, collapse, repair, shelter enclosure
Volume/mass contract (R5) ─► containers, bulk dig yield, water, grain, storage
Contamination contract ─► water, soil, food, survival, farming, animals, restoration
Signals (noise, light, smell, stores) ─► threat director ─► raids ─► structural damage
Save coordinator ◄─ every system
```
Rule: a system is built only after the systems it reads from have their contract (left side) in place.

## 6. Phase plan

Phases follow the user's order (backend and mechanics → greybox world → looks). Each phase ends with the
checks in `VERIFICATION_PLAN.md`.

| Phase | Content | Requirements |
|---|---|---|
| M0 | Reconcile design and repo; this plan | — |
| M1 | Backend gaps that many systems need: ground query authority; volume/mass bulk material + containers; contamination as data; merge or port the parallel branch `game/p06-p08-blind-code-closure` | T15, R4, R5, I2, X3/X4 data |
| M2 | World map as a playable greybox: geographic definitions on the plan maps; ground id map; water carved + water surface; regions/zones on the World map; character decision; existing mechanics placed in the World map (stations, gatherables, choppable trees, logs, building) | T4–T6, T14, D7 |
| M3 | Core loop on the World map: physical assembly (existing log/snap/graph), resources, inventory, production, survival, fire and shelter | A1–A10, R, I, P, S, F, H |
| M4 | Living world: farming cycle, domestic and wild animals, ecology basics | G, W1–W3 |
| M5 | Toxic world and climate chains: hazards, protection, treatment, regional climate | X, C |
| M6 | Threats: fungal colonies, mutant archetypes, signal raids, defences | M, B6 |
| M7 | Traversal and infrastructure placed in the world: ropes, bridges, walkways | V |
| M8 | Persistence and performance: everything saved (incl. voxel), simulation LOD, one-year run | Q, T13 |
| M9 | Demo integration (4–5 h) against the acceptance list below | all demo rows |
| M10 | Materials and textures (by ground id) | looks |
| M11 | Models | looks |
| M12 | PCG scatter (reads the plan maps) | looks |
| Later | advanced joinery (A11), story facilities (E2), other humans (E4), metal era | — |

Digging scope (T7–T10) stays frozen until D8 is decided; its fixes (start-up, hitch, entrance, save) are
listed in `WORLD_PLAN.md`.

## 7. Older task ids (ROADMAP P0–P12) in the new phases

P0 data → M1 · P1 calendar/climate/events → M5 (exists) · P2 survival → M3 · P3 fire/shelter → M3 ·
P4 gathering/production → M3 · P5 digging → T7–T10 (frozen) · P6 farming/animals → M4 · P7 movement/zones/
infrastructure → M2/M7 · P8 events/raids/damage → M5/M6 · P9 balance, helper, one year → M8/M9 ·
P10 physical assembly → M3 (existing parts) + Later (A11) · P11 story → Later · P12 polish/demo → M9–M12.

## 8. Demo scope and acceptance

The demo shows one representative example of every fundamental mechanic, not every content variation.
Each row passes when the user did it in PIE on the World map (level L5 in `VERIFICATION_PLAN.md`).

| System | Demo target | Accepted when |
|---|---|---|
| Landscape | one playable world | walk from start to the edge mountains without falling through or seeing the map edge |
| Voxel | one dig area | dig, stand in the pit, climb out; the pit is still there after save/load (scope: D8) |
| Physical assembly | log cut, carry, snap | fell a tree, cut a log, carry it, snap it into a wall |
| Building | basic shelter with support | a roofed shelter stands; removing a support collapses what it carried |
| Resources | wood, stone, clay, fibre | each gathered in the world |
| Inventory | physical and bulk materials | carry a log and a basket of clay; weight and volume shown |
| Production | a few recipes and stations | make a tool and fire a clay item at a station |
| Survival | hunger, thirst, heat, health | each changes over time and can be fixed |
| Fire | fuel, heat, smoke | a fire warms a closed shelter and needs fuel |
| Farming | a few crops, one growth cycle | prepare a field, plant, see stages, harvest |
| Animals | a few wild and domestic species | hunt one, keep one |
| Toxic world | at least one hazard and its protection | acid rain hurts without cover, a roof or mask prevents it |
| Traversal | one rope or bridge crossing | cross a gap with a built bridge or rope |
| Threat | basic mutant encounter | a mutant reacts and can be fought or avoided |
| Raid | simple signal-driven threat | stores/fire draw a raid that damages a wall |
| Persistence | buildings, resources, terrain edits | save, quit, load: all are back |

Not in the first demo: every resource, recipe, building piece, crop, animal, mutant; advanced raid tactics;
full annual ecological economy; later production chains; large NPC groups; big story facilities.
