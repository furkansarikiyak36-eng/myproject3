# Phase 4 — Stone Masonry Domain Layer

**Status:** Initial source implementation; source review and Unreal validation remain open.  
**Branch:** `phase-04-stone-masonry`  
**Base:** `phase-03-wood-processing`  
**Target:** Unreal Engine 5.8

## Scope

Registry range M029–M038 is the stone domain: loose stone handling, shaping definitions, individual placement/alignment, stacking, walls/corners, columns/beams/openings, support/collapse, damage/repair/salvage, bulk haul, and mixed wood-stone construction.

This phase adds the initial physical-piece and explicit-anchor layer. It does **not** claim that all ten registry mechanics are complete.

## Source added

- `EStonePieceType` provides typed raw stone, block, slab, column, lintel, corner and other piece states.
- `FStoneSnapAnchor` defines authored anchor ID, compatibility tag, local transform, source/receiver permissions and occupancy.
- `AStoneBuildingPiece` owns a stone mesh, carried/placed flags, authored anchors and explicit parent-child connections.
- Anchor IDs must be unique and compatibility tags must match. Geometry overlap alone never creates a connection.
- Connection placement aligns the source anchor transform to the target anchor transform. A reservation is committed only after actor attachment succeeds; failed attachment restores the prior transform.
- Connected pieces require a valid support chain leading to a placed piece. Cyclic support ancestry is rejected.
- Removing a connection releases both anchor reservations and removes the parent-child relationship.
- A loose held stone can be committed to a caller-provided ground transform; actual surface tracing, ground validation and material requirements remain caller responsibilities.

## Deliberately deferred

- Mesh-specific cutting/shaping recipes and resource costs.
- Automatic anchor generation, stone-wall layout helpers, corner/column/opening builders.
- Shared structural graph, load-bearing calculations, collapse simulation, damage/repair/salvage.
- Inventory/bulk hauling, tool requirements and mixed-material structural adapters.
- Save/load persistence and durable world events.
- Unreal Engine build/UHT, Editor/PIE, and runtime tests.

## Acceptance checklist

- [x] Dedicated stone domain types and actor owner added.
- [x] Explicit authored anchor IDs and compatibility tags.
- [x] Duplicate anchor IDs rejected.
- [x] Anchor connection checks permissions, occupancy, compatibility and support ancestry.
- [x] Actor transform is aligned before attachment; reservation follows successful attachment.
- [x] Connection removal releases reservations.
- [x] Ground placement requires held, unplaced state.
- [ ] Review all source callers and Blueprint consumers.
- [ ] Unreal Engine 5.8 UHT/build and Editor/PIE validation at the agreed final gate.
- [ ] Complete M029–M038 domain integration and evidence; registry remains `unreviewed` / `not_run`.

This is source implementation only; no engine test is implied.
