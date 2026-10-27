# VXL-02 — Selective Smooth-Voxel Terrain Design

**Date:** 2026-10-28  
**Planning branch:** planning/gdd-inventory-production-roadmap  
**Target engine:** Unreal Engine 5.8  
**Status:** Architecture proposal plus source inspection; voxel backend is NOT selected or runtime-verified.

## 1. Phase numbering

The repository's numbered Phase 02 already means Core Identity, Operation and Event Contracts. Do not rename that phase or existing implementation branches. This document names the voxel feasibility workstream **VXL-02**. It is a technical gate before the numbered Phase 08 terrain implementation is treated as ready.

## 2. Player-facing rule

The player can dig only where the terrain profile permits it. Surface soil can be excavated with a shovel. Deeper strata become compacted, clay-rich, gravel-rich and increasingly stone-heavy. Once a rock-rich band is reached, starter tools must stop making progress. A later tool tier may unlock selected transition bands, but bedrock and protected areas remain non-diggable in the initial scope.

A blocked attempt must not change terrain density, collision, inventory, resource output, edit receipts or success events. It should provide restrained, legible feedback such as a hard stone strike and a clear blocked reason.

## 3. Shape and digging policy are separate

1. **Base terrain field:** deterministic 3D density/SDF-like data defines the smooth ground volume and any future caves.
2. **Geology/stratum query:** given the original terrain surface, horizontal location, region profile and depth below original ground, returns stratum composition, excavation permission, tool gate, resistance, yield profile and support risk.
3. **Sparse player-edit layer:** records only accepted player changes and reconstructs the edited density field. Render mesh, collision, navigation and VFX are derived outputs, not separate authorities.

Smooth voxel geometry does not mean every point is diggable. Conversely, a hard-rock area may still use a smooth surface mesh while its excavation policy rejects the player's tool.

## 4. Measure depth from original geology

Depth is measured from the deterministic, unmodified base surface/profile—not from the newly carved tunnel ceiling, pit floor, or wall. Otherwise every newly created surface can reset the layer calculation and repeat topsoil forever.

The sample profile can use deterministic lateral variation so strata do not appear as perfectly horizontal sheets. Boundary variation must be derived from the world seed, region/profile version and horizontal coordinates; never roll fresh random values each time a tool checks the same point.

## 5. Example profile — provisional test values

Depth bands below are centimetres beneath the original local surface. This table is a first test profile, not final balance data. Store it as configuration, not hard-coded C++ branches.

| Depth | Example composition | Starter-tool behavior |
|---|---|---|
| 0–40 cm | Topsoil with a little clay | Shovel allowed |
| 40–140 cm | Loam/soil and some clay | Shovel allowed, slower |
| 140–260 cm | Compacted clay, gravel and some stone | Basic pickaxe required |
| 260–360 cm | Stone-rich layer | Advanced tool tag required; unavailable in the first slice |
| 360–400 cm | Bedrock | Always blocked in the initial scope |
| Beyond profile | Unconfigured/deeper region | Blocked by default |

Different regions can expose different geology. A rocky ridge may have a very thin soil layer; a sandy region may permit shallow trenching but have more support risk. A numeric maximum depth is not enough by itself: the profile's strata and tool rules must also allow the edit.

## 6. Profile precedence and material rules

- The **region/stratum profile** decides whether excavation is permitted at that depth and which tool tier is required.
- Material definitions provide intrinsic properties, material identifiers, resistance and possible yields. A material-level diggable=false or protected constraint can further restrict an edit; it must never silently unlock a blocked stratum.
- A mixed stratum's material weights must be deterministic and sum to 1.0. They are used for visual blend and material-yield calculation, not sampled by rolling a new random value for every query.
- Layer boundaries can blend visually; permission boundaries remain explicit gameplay rules so players can understand why a tool stops working.
- Keep tool action requirements (for example, the basic fact that a shovel is used to dig soil) separate from a higher-tier stratum unlock (for example, an advanced pickaxe required for rock).

## 7. Existing candidate code — current gaps

Static inspection of feature/simple-intent found:

- UVoxelTerrainChunkComponent owns one float density grid, initializes a flat ground plane, applies a spherical density edit, extracts a surface through Unreal's Marching Cubes implementation and creates a collidable ProceduralMeshComponent section.
- Defaults are 32 cells per axis and 50 cm voxel spacing (roughly a 16 m chunk edge). This is a useful coarse harness, not an accepted final resolution.
- UWorldTerrainStateSubsystem owns bounded logical cell records and operation receipts.
- In the inspected source paths, the logical cell state and the chunk density edit are not joined by one authoritative edit transaction.
- The chunk component has no demonstrated stratum/material query, resource yield, neighbouring-chunk seam strategy, or save/load reconstruction integrated with the logical edit records.
- The current solid-volume estimate counts negative density sample points. It is not accurate enough to be the authoritative resource-yield calculation.

Do not create a second terrain owner. The implementation review must first establish how the candidate source pieces can be joined, and must not copy them wholesale over main without a reviewed integration branch.

## 8. First implementation workstream

Implement the profile contract and a pure query/policy function first. It should answer: at this original-surface depth and profile, what stratum is present, what is the material composition, is the action allowed, what tool tags are required, and what are its resistance/time/support/yield descriptors? It must not mutate terrain or inventory.

Then integrate the decision with a single edit transaction that validates first, proposes a density delta, applies the edit, rebuilds affected mesh/collision, records a durable edit delta and emits success only after commit. A failed mesh/collision or policy check must not leave density, logical state, inventory or receipts divergent.

## 9. Resolution and performance spike

Compare at least two edit resolutions in the same bounded test region:

- 50 cm voxels / 32 cells per chunk (current coarse harness reference).
- 25 cm voxels / 32 cells per chunk (8 m edge), as a candidate for readable player-scale tunnels.

These are experiment settings, not locked production values. Validate a tunnel that a player can traverse, a pit edge, a hard boundary, edits across two adjacent chunks, collision agreement, saved edit reconstruction and no visible cracks. Track target hardware, resident chunks, memory, p50/p95 rebuild and collision cooking time, and game-thread hitches. Do not keep the whole 5–7 km² world at maximum voxel resolution in memory.

## 10. Technology gate as of 2026-10-28

| Candidate | Verified documentation signal | Decision |
|---|---|---|
| Current project-owned SDF/Marching Cubes chunk | Existing candidate source provides a one-chunk test harness; required geology, cross-chunk, resource-yield and persistence integration is not yet demonstrated. | Use as the first spike harness, not as a production-ready system. |
| Unreal Engine 5.8 Mesh Terrain | Epic documents tunnels/overhangs and non-destructive modifiers but labels Mesh Terrain experimental. The reviewed docs do not establish the full player-driven runtime excavation plus durable player-edit flow required here. | Compare in a small prototype; do not assume it replaces a runtime voxel backend. |
| Voxel Plugin 2.0p8 | Published 2.0p8 documentation targets UE 5.6 and 5.7; its runtime sculpting documentation describes edits as experimental and warns about expensive synchronous edits. | Not a direct UE 5.8 choice without explicit compatibility evidence. |
| VoxelWorlds public repository | Its README claims UE 5.8, smooth Marching Cubes/Dual Contouring, streaming, sparse edits and persistence. A licence file/terms were not confirmed during this repository review. | Do not vendor/copy until licence/permission is confirmed and an isolated UE 5.8 compatibility build passes. |

Sources:
- [Unreal Engine 5.8 Mesh Terrain documentation](https://dev.epicgames.com/documentation/unreal-engine/mesh-terrain-in-unreal-engine)
- [Voxel Plugin 2.0p8 changelog](https://docs.voxelplugin.com/resources/change-log/2.0p8-changelog)
- [Voxel Plugin runtime edits and sculpting](https://docs.voxelplugin.com/knowledgebase/blueprints/runtime-edits-and-sculpting)
- [VoxelWorlds repository](https://github.com/draquel/VoxelWorlds)

The spike passes only after the selected implementation is built and exercised in the actual UE 5.8 target. Documentation claims and source presence are not an engine test.

## 11. Acceptance checklist

- [ ] Smooth surface geometry (not cubic/block terrain).
- [ ] Profile lookup uses depth from the original surface, never the freshly excavated surface.
- [ ] Surface soil can be dug with the correct starter tool.
- [ ] A deeper stone-rich stratum rejects the starter tool without state mutation.
- [ ] A future higher-tier tool rule can be configured without editing core code.
- [ ] Bedrock/protected regions reject excavation.
- [ ] A repeated query gives the same profile/material/tool answer for the same seed and coordinates.
- [ ] Material composition is deterministic and weights validate.
- [ ] Terrain edit, rendered mesh, collision and resource yield do not disagree.
- [ ] A bounded edit across adjacent chunks has no visible crack or invalid collision.
- [ ] Edit state reconstructs correctly after save/load.
- [ ] Memory/edit/collision costs are recorded at both tested resolutions.
- [ ] UE 5.8 build/UHT, Editor, PIE and save/load results are recorded separately.

Until these checks pass, the voxel backend remains an unverified candidate.
