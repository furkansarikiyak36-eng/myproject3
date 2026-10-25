# Landscape Builder — Implementation Plan (UE 5.8)

**Status:** Plan only; no code implementation or Unreal validation is claimed.  
**Baseline:** `mechanic-detailing-pass-h-integrated-world-systems`  
**Work branch:** `landscape-builder-plan`  
**Mechanic IDs:** Preserve M001–M119. This is a cross-cutting workstream, not a new registry.

## 1. Intent and terminology

“Landscape Builder” means a project-owned, reproducible authoring pipeline built on Unreal's native Landscape system, not a replacement for the Landscape editor. The user referred to “Landscape 2.0”; this plan treats that as the desired native Landscape workflow, because a separate Epic product officially named exactly “Landscape 2.0” has not been established from this repository.

Useful Epic references:
- https://dev.epicgames.com/documentation/unreal-engine/landscape-overview
- https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-landscapes-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-edit-layers-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/landscape-patch-system
- https://dev.epicgames.com/documentation/unreal-engine/landscape-blueprint-brushes-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/world-partition-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/using-pcg-with-world-partition-in-unreal-engine

## 2. Repository baseline confirmed

- `MyProject3.uproject` targets Unreal Engine 5.8.
- The H integrated branch has a logical `WorldTerrainStateSubsystem` with bounded cell/depth/material rules, operation receipts, revisions and snapshot import/export.
- That subsystem does **not** modify Landscape heightmaps, render physical excavation geometry, update collision/navigation, invalidate structural support or persist physical excavation geometry.
- No Landscape Builder implementation or committed `.umap/.uasset` level content was found in the inspected branch tree.
- The project descriptor enables editor-side Modeling Tools, Python Script Plugin and Editor Scripting Utilities. PCG and Landscape Patch are not explicitly enabled in the descriptor; verify actual UE 5.8 installation/module availability before depending on them.

This is repository/source evidence only. Unreal Build Tool, Header Tool, Editor, PIE and gameplay tests were not run.

## 3. Four-layer architecture

### A. Native Landscape surface

Use native Landscape for the outdoor heightfield: mountains, hills, plateaus, valleys, basins, slopes and suitable surface openings. Keep changes non-destructive with separate Landscape Edit Layers where supported:
1. Base heightmap / macroform.
2. Regional shaping and masks.
3. Erosion/sculpt refinement.
4. Landscape Splines for authored roads/trails and suitable corridors.
5. Landscape Patch or Blueprint Brushes for targeted procedural edits when they materially help.
6. Material paint and biome masks.

Build against the APIs/tools actually available in UE 5.8; do not assume another engine version's exact editor scripting behavior.

### B. Deterministic Landscape Builder (editor-side)

First delivery should be profile-driven and output an importable 16-bit heightmap (or another UE 5.8-supported format), then create/configure native Landscape through an Editor workflow. Only add a C++ editor module if Python/Editor Scripting cannot reliably create the required assets.

Profile fields:
- Stable profile ID, version and seed.
- World dimensions, horizontal scale, elevation range and supported Landscape resolution.
- Explicit ratios/regions for mountain, plateau, valley, basin and playable lowland.
- Ridge direction, valley/river basin corridors and seeded detail noise.
- Optional erosion/smoothing settings.
- Biome, altitude, slope and moisture/river-distance masks.
- Protected regions, settlement/build yards, farms, raid lanes, roads/trails and designated dig pockets.
- Deterministic seed inputs for each generated subsystem.

Macroforms and playable corridors must be explicit; noise is detail, not the only world-shape logic. Record a generation manifest with profile/version/seed, settings and input/output checksums. Same inputs must reproduce the same output.

### C. Surface materials, routes and dressing

- Configure a Landscape material with target layers and physical surface classifications (e.g. soil, rock, mud, sand).
- Derive masks from altitude, slope, authored biome regions and proximity to water/routes.
- Use Landscape Splines for roads/trails and suitable surface corridors; a spline alone does not guarantee simulated river flow.
- Use PCG for trees, rocks and foliage only if supported and useful. Respect slope, biome, paths, farm plots, construction yards, visibility lanes and dig pockets.
- Keep terrain, materials, spline work and foliage independently regenerable. Foliage regeneration must not erase player structures or move authored gameplay markers.
- Use World Partition/Data Layers/HLOD according to the actual world size and performance needs, not by default.

### D. Runtime bounded excavation adapter

A Landscape heightfield is not a general 3D cave volume and cannot by itself represent arbitrary overhangs or traversable underground tunnels. Keep the outdoor surface in Landscape and use a bounded 3D representation only inside designated excavation pockets.

For M071–M076:
- Keep `WorldTerrainStateSubsystem` as the authoritative logical request/policy/revision layer, after contract review.
- Define a small number of designated dig pockets in the generated profile.
- Represent each pocket with bounded 3D cells/chunks or modular cave/soil meshes; do not create a whole-world voxel grid.
- Convert accepted changes into visible physical geometry and collision for affected chunks only, using a mesh implementation verified for UE 5.8.
- On commit, notify navigation and structural graph/support systems for the affected region.
- Validate operation ID, revision, material, depth, region, work budget and protected-area rules before mutation.
- Invalid requests must fail with no partial logical or physical change; retries must not duplicate excavation.
- Save compact versioned deltas, not an unbounded world dump. Restore base Landscape/profile first, excavation chunks second, then dependent structures and navigation.

## 4. Proposed components (not existing classes)

| Component | Responsibility |
|---|---|
| Landscape Build Profile / Data Asset | Seed, world dimensions, macroforms, masks and settings |
| Landscape Builder Editor tool | Generate/import heightmap and configure native Landscape/edit layers |
| Landscape Generation Manifest | Profile/version/seed/checksum and generated-region record |
| Terrain Region Registry | Protected areas, biome masks and permitted excavation pockets |
| Terrain Excavation Adapter | Logical terrain state → bounded physical mesh/collision updates |
| Terrain Navigation/Support Bridge | Post-commit navigation and structural-support notifications |
| Terrain Save Adapter | Versioned chunk deltas and safe restore order |

Authority must not be duplicated: authored Landscape owns the base surface; terrain state owns approved mutable excavation state; the structural graph owns construction links; navigation owns walkability/path data.

## 5. Delivery sequence

### LB0 — Branch and engine capability audit
Confirm UE 5.8, native Landscape workflow, Edit Layers support, Editor Scripting behavior and whether PCG/Patch or a mesh-generation module is needed. Record map dimensions and target play area before selecting resolution.

### LB1 — Profile schema and macroform generator
Validate profile fields and supported dimensions. Generate repeatable heightmap output with explicit macroform templates plus seeded detail. Reject invalid settings before asset creation. Record manifest/checksum.

### LB2 — Native Landscape creation and materials
Import/create Landscape with supported component/resolution settings, collision and edit layers. Apply base material, paint layers and physical surface classifications. Keep base and refinement separate.

### LB3 — Erosion, routes and water corridors
Use built-in sculpt/erosion tools as refinement, protecting playable paths and building surfaces. Author road/trail/river corridors and stable settlement/farm areas. Define bridge/rope/zipline endpoints where traversal systems require them.

### LB4 — Biomes and environment dressing
Add deterministic masks and optional PCG dressing. Exclude no-spawn zones and authored gameplay areas. Decide what is baked versus partitioned only after profiling the target world.

### LB5 — Physical excavation pockets
Connect logical terrain requests to bounded physical geometry/collision. Use identical rules for preview and commit. Reject invalid edits before mutation; update navigation/support only after commit.

### LB6 — Save/load and cross-system integration
Persist profile/seed/version and compact excavation deltas. Restore base → excavation → structures/traps → navigation/routes. Handle schema mismatch safely and reconcile foundations, bridges and route costs.

### LB7 — Demo acceptance map
Add terrain tests to the existing 7-zone/119-mechanic demo plan:
- Terrain Pocket: visible pit/tunnel, valid depth, protected-depth rejection and restore marker.
- Construction Yard: support/foundation behavior near the dig pocket.
- Logistics: road/bridge traversal and route-cost effects, when adapters exist.
- Exploration: biome and exposed-route differences, when integrated.

Each case records expected visible geometry, collision/navigation, failure-with-no-mutation, retry/idempotency, persistence and evidence location. Unreal validation remains deferred to the user's later final-validation stage.

## 6. M001–M119 touchpoints

Do not add, remove or renumber IDs. This work supports:
- M024–M027 and M059–M070: anchors, foundations, structure stability and support invalidation.
- M071–M076: bounded region/chunk edits, depth/material policy, visible pits/tunnels, support response and persistence.
- M078–M080: optional environmental masks, runoff/moisture and discovery placement inputs.
- M101–M106: terrain-aware routes, roads/bridges and transport access.
- M118: shelter placement and surface exposure context.

Landscape generation is an enabling toolchain; it does not mean these gameplay mechanics are implemented.

## 7. Guardrails and definition of done

- Never generate the whole world every normal gameplay startup.
- Keep generation jobs bounded and editor-side for the first EA delivery.
- Rebuild only changed excavation chunks and affected collision/navigation.
- Do not claim arbitrary runtime caves from Landscape heightfields alone.
- Do not make Gaea-like external erosion simulation a mandatory runtime dependency.
- Do not silently move authored roads, structures, farms or demo markers during regeneration.
- Do not upgrade registry verification statuses from planning or source commits.

Done means: same profile/seed gives same heightmap/manifest; native Landscape layers/materials are configured; routes and dressing respect masks; a valid pocket excavation changes visible geometry and collision; invalid requests do not mutate; support/navigation and save/load remain coherent; and later UE 5.8 build/UHT/Editor/PIE gates pass with recorded evidence.

**Current state: planned, not implemented or engine-verified.**
