# Phase 8 — Landscape Builder, Terrain World State and Constrained Digging

**Status:** Partial logical state layer exists; physical Landscape/mesh integration is not implemented or engine-verified.  
**Registry scope:** M071–M080; possible Landscape-driven extensions must use existing child requirements first and append M120+ only for distinct gameplay behavior.  
**Prerequisites:** Core identity/clock/save contracts; structural graph integration where terrain supports buildings.  
**Detailed plan:** `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md`

## Responsibility

Own deterministic, bounded terrain edits and durable world-state changes. The editor-side Landscape Builder is a toolchain, not automatically a new gameplay mechanic; see `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md` for scope-expansion policy. The goal is a reproducible outdoor Landscape plus selective digging for pits, short tunnels, underground rooms and traps—not unrestricted high-cost voxel editing across the entire world.

## Current source baseline

The H integrated branch contains `WorldTerrainStateSubsystem`, tracking edited cells, depth layers, material/excavation state, revisions, operation receipts and snapshot import/export. This is a logical state layer only. It does not modify Landscape heightmaps, render physical excavation geometry, update collision/navigation, invalidate structural support, or persist physical excavation geometry.

Treat it as a starting contract, not as a playable digging feature.

## Landscape Builder boundary

Landscape Builder is a project-owned editor workflow on Unreal Engine 5.8's native Landscape system. It should generate reproducible profiles and create/import the outdoor heightfield, not reimplement the full Landscape editor.

Use native Landscape Edit Layers for non-destructive surface work. Use supported sculpt/erosion tools for refinement, Landscape Splines for roads/trails and compatible corridors, Landscape Patch/Blueprint Brushes for targeted modifications when useful, and PCG for optional foliage/rock dressing. Confirm actual UE 5.8 plugin/module availability before declaring dependencies.

Profile fields should include stable ID/version/seed, world dimensions, elevation range, supported resolution, mountain/plateau/valley/basin ratios, noise/erosion settings, biome/altitude/slope masks, protected gameplay areas, roads/trails and designated dig pockets. A generation manifest records profile/version/seed, settings and input/output checksum.

Generation is editor-side and explicit; do not generate the whole world during normal gameplay startup. Prefer importable heightmap plus native Landscape workflow first; introduce an editor C++ module only if Python/Editor Scripting cannot reliably create/configure the required objects.

## Runtime excavation model

A Landscape heightfield is the outdoor surface, not a general 3D cave volume. For M071–M076, designate bounded pockets using a limited 3D cell/chunk representation or modular cave/soil meshes. Update physical geometry and collision only for affected chunks.

- Keep `WorldTerrainStateSubsystem` as the authoritative logical request/policy/revision layer.
- Validate operation ID/revision, depth, material, region bounds, work budget and protected-area rules before mutation.
- Commit logical state with matching physical geometry/collision, or reject without partial mutation.
- After commit, notify navigation and the structural graph when ground support changes.
- Persist compact versioned excavation deltas. Restore base Landscape/profile first, excavation chunks second, dependent structures/traps third, and navigation/routes afterward.
- Confirm the mesh-generation implementation and any required UE 5.8 module/plugin before implementation.
- Repeating an operation with the same ID must not excavate twice or duplicate physical output.

## Proposed source boundaries (not existing classes)

- Landscape Build Profile / Data Asset — generation settings.
- Landscape Builder Editor tool — heightmap/profile pipeline and native edit-layer setup.
- Landscape Generation Manifest — seed/version/checksum and region metadata.
- Terrain Region Registry — permitted dig pockets, protected regions and biome/surface masks.
- Terrain Excavation Adapter — logical cell to bounded physical mesh/collision synchronization.
- Terrain Navigation/Support Bridge — post-commit walkability and support notifications.
- Terrain Save Adapter — versioned deltas and restore ordering.

Do not create competing sources of truth: authored Landscape owns the base surface; terrain state owns approved mutable excavation; structural graph owns construction links; navigation owns walkability/path data.

## Contracts

- Only explicitly editable pockets/materials and configured depth/range may be changed.
- Every operation has bounded work/memory cost; invalid requests fail before mutation.
- Geometry and collision cannot disagree after commit.
- Navigation and structural support receive consistent post-commit updates.
- Operation receipts/revisions prevent duplicate edits.
- Save data stores compact versioned deltas, not an unbounded whole-world voxel representation.
- Restore Landscape base before excavation, structures, traps and navigation.
- Regenerating dressing cannot delete player structures or move authored gameplay locations.

## Acceptance gates — planned, not executed

- [ ] Same profile/seed produces the same heightmap and generation manifest.
- [ ] Native edit layers separate base terrain, shaping/refinement and supported spline/patch changes.
- [ ] Material/physical-surface layers and no-spawn gameplay masks are deterministic.
- [ ] Optional PCG dressing respects slope, biome, path, farm, construction and dig-pocket masks.
- [ ] Valid digging changes visible geometry, collision and navigation in the intended pocket.
- [ ] Invalid depth/material/region requests fail with no partial logical or physical mutation.
- [ ] Structural support is safely invalidated or the edit is rejected.
- [ ] Terrain edits survive save/load with version checks and correct restore order.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE validation deferred to the later final-validation stage; no results are claimed.
