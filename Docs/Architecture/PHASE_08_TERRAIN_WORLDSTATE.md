# Phase 8 — Landscape Builder, Terrain World State and Constrained Digging

**Status (2026-10-28):** Logical terrain state, bounded voxel chunk source layer, v14 density snapshot path, and a first player-facing `N`-key dig tool are present on `integration/phase-02-14-closure`; trusted inventory-tool binding, Landscape seam/navigation, async meshing, and UE acceptance remain open.  
**Registry scope:** M071–M080; possible Landscape-driven extensions must use existing child requirements first and append M120+ only for distinct gameplay behavior.  
**Prerequisites:** Core identity/clock/save contracts; structural graph integration where terrain supports buildings.  
**Detailed plan:** `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md`

## Responsibility

Own deterministic, bounded terrain edits and durable world-state changes. The editor-side Landscape Builder is a toolchain, not automatically a new gameplay mechanic; see `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md` for scope-expansion policy. The goal is a reproducible outdoor Landscape plus selective digging for pits, short tunnels, underground rooms and traps—not unrestricted high-cost voxel editing across the entire world.

## Current source baseline

`WorldTerrainStateSubsystem` tracks permitted logical cell edits, depth layers, material/excavation state, revisions, operation receipts and logical snapshot import/export. `UVoxelTerrainChunkComponent` now has a bounded, soil-strength-aware dig API, optimistic revision checks, idempotent operation receipts, explicit density snapshot validation/restore, and a conservative clearance guard around grounded roots in `WorldStructuralGraphSubsystem`. `WorldSaveCoordinatorSubsystem` captures density samples by actor identity in SaveGame schema v14 and rebuilds mesh/collision when restoring an existing chunk.

This is a source-layer increment, not a playable digging feature yet: no player/tool input caller binds the authoritative tool definition to `TryApplyBoundedDig`; the logical terrain-cell change and voxel density edit are not yet one cross-domain atomic operation; Landscape visibility holes/seams, adjacent chunk stitching, asynchronous generation, navigation refresh and physical item yield remain open. The save/restore path requires stable identities and pre-existing voxel chunk actors in the loaded level; it can hydrate an existing uninitialized component, rejects missing chunks or an initialized chunk without a snapshot, and does not spawn actors.

## Current implementation delta (source-level only)

- [x] Soft soil / hard soil / rock have separate configurable minimum tool-strength thresholds (prototype defaults 1 / 3 / 5).
- [x] Excavation is limited by configurable radius, editable local bounds, chunk bounds, and expected density revision.
- [x] A successful bounded dig rebuilds mesh/collision before returning success; inability to rebuild rejects and rolls back the local density/revision.
- [x] Replaying a matching operation ID returns its prior receipt; conflicting reuse is rejected.
- [x] A registered grounded structure root inside the configured spatial clearance blocks the dig.
- [x] SaveGame v14 stores per-chunk stable actor identity, dimensions, density samples, density revision and accepted dig receipts; restore regenerates mesh/collision for an already-present chunk.
- [x] Add `TryApplyVoxelExcavation` as a coordinated entry point: capture density snapshot, apply bounded physical dig, commit logical cell, restore the density snapshot if logical commit rejects. This is source-level rollback logic, not a verified runtime transaction.
- [ ] Connect actual player/tool interaction to the bounded API using tool strength from trusted item/tool data.
- [ ] Verify the support guard against foundation/post/bridge test cases and replace spatial clearance with explicit terrain-support invalidation where necessary.
- [ ] Run `MyProject3.Terrain.Voxel.LogicalPhysicalExcavationIsAtomic` and verify rollback/commit behavior in UE 5.8; source test exists but has not been executed.
- [ ] Build/PIE test save → dig → save → alter → load → trace collision, including changed and missing chunk cases.
- [ ] Handle neighboring chunk samples/seams, async mesh generation, Landscape visibility holes, navmesh refresh and excavation-derived item yield.

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
