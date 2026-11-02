# Systems register — inventory and implementation status

Every requirement of `MASTER_PLAN.md` section 3, with its state in the repository (branch
`game/p00-adopt-group-a`, 2026-11-02, 137/137 automation tests). Update the row when its state changes.

Status words (the only ones allowed):
- **Built** — code compiles and has passing automation tests here. Not the same as played.
- **Partial** — built for part of the requirement; "Gap" says what is missing.
- **Branch** — code exists only on `game/p06-p08-blind-code-closure` (parallel line, never built on this branch).
- **Data** — only JSON data / a placeholder; no behaviour.
- **Designed** — only in design text. **Missing** — not designed in detail, no code. **Frozen/Later** — deferred by decision.

PIE column: whether the user played it. "—" = no record. Nothing on the World map has been played yet.

## T Terrain and world
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| T1 | single landscape | Built | `/Game/World/World`, `BuildValleyWorldCommandlet` | 4x4 km vs 5x5 (D1) | — |
| T2 | terrain source pipeline | Partial | `tools/heightmaps/*`, heightmap PNGs (LFS) | no formal "terrain source interface"; one source format | — |
| T3 | World Partition | Missing | — | D2 | — |
| T4 | geographic definitions | Missing | plan maps give slope/relative/wetness only | watersheds, ridges, passes, rivers, lakes, settlement/farm/forest areas | — |
| T5 | regions, design cells, claims | Data | `zones.json` (VerticalSlice_C only) | none on the World map; claim purpose D10 | — |
| T6 | plan maps | Built | `build_world_masks.py`, `SourceArt/World/masks/` | ground id map | — |
| T7 | voxel dig | Partial · Frozen | `AWorldGroundActor`, `VoxelTerrainChunkComponent` | 35 s start-up, 42 ms dig hitch, scope D8 | — |
| T8 | trenches, tunnels, rooms | Partial | `AEarthworkActor` (cell-based cellar/cistern/trench/embankment) | not on voxel ground | — |
| T9 | caves | Partial · Frozen | 4 generated voxel caves | entrance not cut cleanly, dark, not saved | — |
| T10 | shoring, collapse, drainage | Partial | unsupported earthwork rooms cave in | voxel collapse, drainage | — |
| T11 | geology layers | Partial | `layers.json`, geology resolver (old dig path) | not on the World ground; groundwater | — |
| T12 | landscape–voxel seams | Partial | mask + collision bypass | seams at entrances and far LOD | — |
| T13 | persistent terrain edits | Partial | voxel density save for placed chunks | World ground chunks not saved | — |
| T14 | water bodies | Missing on World | creek in VerticalSlice_C | carve + surface (Water plugin test) | — |
| T15 | ground query authority | Missing | — | M1 | — |

## A Physical assembly / B Building life
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| A1 | physical item lifecycle | Built | ownership subsystem, ledger | — | — |
| A2 | tree felling → trunk | Built | `HarvestableTreeActor` | World map trees are stand-ins, not choppable | — |
| A3 | 600 cm log, 75 cm stations, end + side snaps | Built | `LogBuildPiece` (End.A/B, Station.*) | — | — |
| A4 | cut, split, half-log persists | Built | `LogBuildPiece`, `LogCarryComponent` | — | — |
| A5 | global snap kernel | Built | `Placement/` | — | — |
| A6 | anchors, connection graph | Built | anchors, `MaterialConnectionSubsystem` | — | — |
| A7 | load and support | Partial | structural graph supports | load distribution values | — |
| A8 | multi-part collapse | Partial | dependents in graph | physical break after thresholds | — |
| A9 | damage, repair, dismantle, recovery | Built | graph + `LogBuildToolsComponent` | balance | — |
| A10 | doors, windows, openings | Built | `ConstructionOpeningActor` | — | — |
| A11 | notches, mitre, crossings | Frozen/Later | — | — | — |
| A12 | save, ownership | Built | save v31 | physics transforms after load unproven | — |
| B1 | temporary shelters | Partial | `BuildPlanActor` (lean-to) | rain cover, bed | — |
| B2 | permanent houses, cellar, workshop | Partial | log walls/cabin plans, earthwork cellar | earth/clay and stone houses | — |
| B3 | wear (rain, rot, leaks, snow load) | Missing | — | — | — |
| B4 | fire spread | Partial · Branch | event fire damage (here); `WorldCombustibleComponent` (branch) | merge D-branch | — |
| B5 | maintenance | Partial | helper `MaintainWalls`, repair | — | — |
| B6 | defences | Partial | log walls, gates, stick traps | palisade/watch tower/ditch as defence | — |

## V Traversal
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| V1 | load and balance | Partial | `CarryLoadComponent` mass → speed | balance on narrow bridges | — |
| V2 | slope/mud/weather cost | Partial | player speed modifiers, route graph | walking ≠ route cost model | — |
| V3 | ropes | Partial | `RopeLineActor` (bridge rope, zipline) | anchors strength, climbing, ladder | — |
| V4 | bridges | Partial | rope bridge, bridges/ramps | span/capacity rules | — |
| V5 | walkways, paving, stairs, scaffold | Partial | stairs/steps in build tools | walkways, paving | — |
| V6 | sleds, cargo | Built | sleds, `WorldCargoTransportSubsystem` | — | — |

## R Resources / I Inventory / P Production
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| R1 | ten families | Data | `items.json` (89 items) | animal/special families thin | — |
| R2 | raw → product chains | Built | `transformations.json` (59 rows) | — | — |
| R3 | material quality | Missing | — | — | — |
| R4 | object vs bulk vs liquid vs biological vs property | Missing | everything is item units | M1 | — |
| R5 | containers by volume, mass separate | Missing | — | M1 | — |
| R6 | 35–45 / 20–30 / 8–12 targets | Data | 89 items | D5 | — |
| I1 | slots, weight | Built | `InventoryCraftingComponent` | — | — |
| I2 | volume | Missing | — | — | — |
| I3 | hand limits, large carry | Built | `LogCarryComponent` | — | — |
| I4 | storage, stations pull from storage | Partial | `WorldStorageSubsystem`, stations | stations do not pull from storage | — |
| I5 | spill, spoil, damp, contamination | Partial | spoilage by storage and temperature | spill, damp, contamination | — |
| I6 | single ownership | Built | ownership kinds + receipts | — | — |
| P1 | tools | Partial | axe, digging stick + data tools | tool wear | — |
| P2 | stations | Built | `ProductionStationActor`, 10 stations | meshes | — |
| P3 | recipe schema | Partial | inputs, tools, station, time, fuel, byproducts | quality/durability, maintenance | — |
| P4 | hybrid timing | Built | hand instant, station timed | — | — |
| P5 | economy rates | Missing | — | balance pass | — |

## S Survival / F Food and water / H Fire and shelter
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| S1 | hunger | Built | `WorldSurvivalSubsystem` (energy) | — | — |
| S2 | thirst | Built | water items, drinking | — | — |
| S3 | nutrition variety | Missing | — | — | — |
| S4 | sleep, rest, fatigue | Built | rest on the clock | — | — |
| S5 | injury, disease | Built | conditions with treatment | bleeding, infection detail | — |
| S6 | temperature, wetness, wind | Built | felt temperature, wetness | wind chill detail | — |
| S7 | clothing | Built | 5 slots, insulation, water resistance | — | — |
| S8 | stamina, carrying | Built | stamina, carry mass | — | — |
| S9 | hygiene | Missing | — | — | — |
| S10 | contamination exposure | Partial | toxic rain event → illness | dose model | — |
| F1 | gather, hunt, farm, herd | Partial | gatherables, capture/slaughter | hunting loop | — |
| F2 | cook, dry, store, spoil | Built | recipes, spoilage | — | — |
| F3 | liquids with properties | Partial | dirty/boiled water items | volume, rain/spring/contaminated kinds | — |
| H1 | fuel → heat → shelter → health | Built | `HeatSourceComponent`, `ShelterSensorComponent` | — | — |
| H2 | insulation, openings, sealing | Built | enclosure rays, plaster | per-gap heat loss | — |
| H3 | smoke and ventilation | Missing | — | — | — |
| H4 | comfort | Partial | rest place quality | — | — |
| H5 | fire risk | Partial · Branch | rain puts out fires; wildfire spread on branch | — | — |

## G Farming / W Wildlife and ecology
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| G1 | crops | Data | 3 provisional crops | real crop list | — |
| G2 | 13-step field cycle | Partial | 7 field actions, field cell state | full chain, visuals | — |
| G3 | soil model | Partial | fertility, moisture, weeds, mulch, exhaustion | salinity, acidity, compaction, erosion, contamination | — |
| G4 | manure, compost, pests, rotation | Partial · Branch | annual farming work on branch | — | — |
| G5 | field presets | Partial | 3x3 farm patch | presets | — |
| W1 | wild species | Partial | `WildlifeActor` (small animals, hostile biter) | deer, rabbit, wolf, bear as species | — |
| W2 | domestic species | Partial | capture, pen, slaughter | feeding, breeding, products (milk, wool, eggs) | — |
| W3 | ecological behaviour | Missing | — | — | — |
| W4 | restoration | Missing | — | — | — |

## X Toxic world / C Climate
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| X1 | acid rain | Partial | toxic rain event, illness | damage to structures, plants, soil | — |
| X2 | toxic gas/smoke | Missing | — | — | — |
| X3 | contaminated water | Partial | dirty water → illness | chemical vs biological | — |
| X4 | contaminated soil | Missing | — | — | — |
| X5 | spores | Missing | — | — | — |
| X6 | contaminated dust | Missing | — | — | — |
| X7 | masks, filters, clothing | Missing | clothing slots exist | — | — |
| X8 | sealed rooms, ventilation | Missing | enclosure exists | — | — |
| X9 | water treatment | Partial | boiling | filters; boiling must not remove chemicals | — |
| X10 | contamination in food chain | Missing | — | — | — |
| C1 | four seasons, calendar | Built | `WorldCalendar` | — | — |
| C2 | regional climate | Missing | one climate | — | — |
| C3 | natural events | Built | 11 events (`UWorldEventDirectorSubsystem`) | landslide, rockfall, lightning | — |
| C4 | toxic events | Partial | toxic rain | gas spread, dust storm, toxic flood | — |
| C5 | biological events | Partial | epidemic | spore storm, colony spread | — |
| C6 | event chains | Built | `requires_recent` chains | longer chains | — |
| C7 | preparation, recovery, warnings | Partial | warning phase, counter items | animal/cloud warnings | — |

## M Mutants and raids / E Progression and story
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| M1 | fungal ecosystem | Designed | — | — | — |
| M2 | five archetypes | Designed | — | — | — |
| M3 | wildlife reactions | Designed | — | — | — |
| M4 | threat signals | Partial | threat pressure from stores, smell, light/smoke | noise, clearing, colony interference | — |
| M5 | raid stages and types | Partial | raid states Warning→Active→Repelled | raid actors | — |
| M6 | tactics | Designed | — | — | — |
| M7 | defence, traps, alarms | Partial · Branch | stick traps; raid defences JSON on branch | — | — |
| M8 | world memory, regrowth | Designed | — | D9 | — |
| E1 | discovery progression | Partial | 6 discoveries, unlock gates | — | — |
| E2 | facilities, crash site, history | Designed | — | — | — |
| E3 | environmental storytelling | Designed | — | — | — |
| E4 | other human groups | Missing | — | D11 | — |

## Q Technical
| Id | Requirement | Status | Where | Gap | PIE |
|---|---|---|---|---|---|
| Q1 | save of every system | Partial | coordinator, schema v31 | World ground, raids, ecology | — |
| Q2 | atomic operations, rollback | Built | ledger, snapshots | — | — |
| Q3 | versioning, recovery | Built | chained migrations | — | — |
| Q4 | fixed-step simulation | Built | coordinator | — | — |
| Q5 | simulation LOD | Missing | — | — | — |
| Q6 | voxel/physics budgets | Partial | voxel cost measured | object budget | — |
| Q7 | annual simulation test | Missing | — | — | — |
| Q8 | UI and failure reasons | Partial | debug HUD, F7, on-screen reasons | real UI | — |
| — | helper companion (in repo, not in the plan text) | Built | `HelperCompanionActor` | stations | — |
| — | character | Partial | first person `LogSnapCharacter` (mechanics); Third Person template on the World map | D7 | — |

## Parallel branch to reconcile

`game/p06-p08-blind-code-closure`: +315 commits on an old base, 60 files / ~11.9k lines not here
(own regional event system, wildfire spread with firebreaks, farm processing, voxel hazards, raid signals,
`WorldInfrastructureActor`, `WorldCombustibleComponent`, infrastructure/knowledge/raid_defenses/routes JSON).
Never built on this branch. Merge or port is part of M1.
