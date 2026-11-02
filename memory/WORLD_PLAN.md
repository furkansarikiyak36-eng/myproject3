# World plan: one ground model (2026-11-01)

Agreed with the user: no further voxel work now; one plan where terrain, ground, water,
placement, mechanics and looks fit together. Data first, looks last.

## The ground model (every system reads the same layers)

| Layer | What | Where |
|---|---|---|
| 1 Shape | heightmap, 4 x 4 km, edge mountains | `tools/heightmaps/build_world_heightmap.py` -> `SourceArt/World/valley.r16` (frozen) |
| 2 Plan maps | slope, height, relative height (ridge/valley), wetness, edge -> soil depth, trees, bushes, rocks, caves | `tools/heightmaps/build_world_masks.py`, rules in `world_layout.json` "masks"/"caves" |
| 3 Ground authority | one query: height, ground id, soil depth, diggable; dug places answer with their edited surface | `AWorldGroundActor` (later a subsystem) |
| 4 Edits | digs stored as changes on top of the heightmap (saved) | voxel columns today; save not done |
| 5 Looks | terrain material, models, PCG read layers 2-3 | `M_WorldTerrain`, commandlet dressing |

Rule: a system never asks the landscape directly for "the ground"; it asks layer 3.

## Steps, in order

Superseded 2026-11-02 by the phases in `MASTER_PLAN.md` section 6 (M1-M12); kept for the world/ground detail.

1. **Ground id map** (Python): Topsoil/grass, Clay, Sand, Gravel, Rock, Mud from slope, wetness, height,
   relative height. `ground_id.r8` + preview. Terrain colours and dig yields read it.
2. **Ground query API** in the ground actor (height / id / soil depth / diggable) and switch the users:
   dig tool, placement kernel ground traces, tree/rock placement, helper NPC. Tests.
3. **Water**: carve river + lake in the heightmap script (deterministic); water surface with the Water
   plugin, terrain carving off; measure its cost like the voxel cost below. Drink/rain rules link to it.
4. **Playable world**: choppable trees (instance -> tree actor near the player), gatherables placed by
   ground id, decide the character (third person template vs first person LogSnapCharacter mechanics).
5. **Mechanics phases** from `ROADMAP.md`, played on this world.
6. **Digging completion** (voxel is frozen as it is until here): fixes from the cost table, save of
   edits, item yield by ground id and depth, other traces through dug columns, far LOD seams.
7. **Models + PCG**: replace stand-ins; PCG reads the plan maps.
8. **Textures** by ground id.
9. **Physical assembly** (P10).

## Voxel cost, measured 2026-11-01 (i7-11800H, 1280x720, World map, 600 frames after 10 s)

| Case | Frame | GPU |
|---|---|---|
| Ground on (masked landscape, 124 voxel chunks, dig tool) | 9.26 ms | 8.02 ms |
| Voxel off, masked landscape | 9.07 ms | 8.07 ms |
| No mask, no voxel | 9.13 ms | 8.09 ms |

Per frame: no measurable cost (differences are noise, < 0.2 ms). The masked landscape costs nothing measurable.

| One-off / memory | Value | Note |
|---|---|---|
| Start-up (4 caves) | 34-38 s | almost all from 1089 landscape traces per column chunk (each walks every collision component); reading heights from data instead should leave ~5 s (124 chunks x 43 ms), carving caves when the player comes near removes it |
| One column chunk (init + mesh + collision) | 43 ms | `MyProject3.Terrain.Voxel.GroundCostTimings` |
| One dig (edit + mesh + collision) | 42 ms | a 2-3 frame hitch at 60 fps; async collision cooking or 16-cell chunks would cut it |
| Memory, 124 chunks | 17 MB density + 18.7 MB mesh (CPU) + about the same on GPU and in collision | ~0.45 MB per chunk |

Measure again with: `UnrealEditor.exe MyProject3.uproject /Game/World/World -game -windowed -GroundCostFrames=600 -ExecCmds="r.VSync 0,t.MaxFPS 0"` (add `-NoWorldGround` to switch the ground off).
