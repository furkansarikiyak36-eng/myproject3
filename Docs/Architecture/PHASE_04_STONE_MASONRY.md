# Phase 4 — Stone Masonry Domain Layer

**Status:** Initial source implementation plus defensive lifecycle corrections; asset, shared-domain integration and Unreal validation remain open.  
**Branch:** `phase-04-stone-masonry-sequential`  
**Base:** `phase-03-wood-processing`  
**Target:** Unreal Engine 5.8

## Scope

Registry range M029–M038 is the stone domain: loose stone handling, shaping definitions, individual placement/alignment, stacking, walls/corners, columns/beams/openings, support/collapse, damage/repair/salvage, bulk haul, and mixed wood-stone construction.

This phase establishes the stone-piece and explicit-anchor foundation. It does **not** claim all ten registry mechanics are complete.

## Source delivered

- `EStonePieceType` defines typed raw stone, block, slab, column, lintel, corner and other piece states.
- `FStoneSnapAnchor` defines authored anchor ID, compatibility tag, local transform, source/receiver permissions and occupancy.
- `AStoneBuildingPiece` owns a stone mesh, held/placed state, authored anchors and explicit parent-child connections.
- Authored anchor data is checked for missing IDs/tags, invalid transforms and duplicate IDs before a connection is accepted. Runtime anchor creation also rejects duplicates.
- Connection placement aligns the source anchor transform to the target anchor transform. A reservation is committed only after actor transform and attachment succeed; failed attachment restores the prior transform.
- Connected pieces require a support chain leading to a placed piece. Cyclic support ancestry is rejected.
- Removing a connection releases both anchor reservations and the parent-child relation; detached pieces return to loose physics.
- End-play cleanup iterates a snapshot because child disconnection mutates the parent's child list.
- Carrying rejects pieces that are connected, supporting children, or still have occupied anchors.
- A loose held stone can be committed to a caller-provided ground transform; actual surface tracing, ground validation and material requirements remain caller responsibilities.

## Deliberately deferred

- Mesh-specific shaping recipes and resource costs.
- Automatic anchor generation, stone-wall layout helpers, corner/column/opening builders.
- Shared structural graph, load-bearing calculations, collapse simulation, damage/repair/salvage.
- Inventory/bulk hauling, tool requirements and mixed-material structural adapters.
- Save/load persistence and durable world events.
- Unreal Engine build/UHT, Editor/PIE and runtime tests.

## Acceptance checklist

- [x] Dedicated stone domain types and actor owner.
- [x] Explicit authored anchor IDs and compatibility tags.
- [x] Duplicate/malformed anchor data rejected by connection validation.
- [x] Anchor connection checks permissions, occupancy, compatibility and support ancestry.
- [x] Actor transform is aligned before attachment; reservation follows successful attachment.
- [x] Connection removal releases reservations and returns the detached piece to loose physics.
- [x] End-play child cleanup avoids mutating the iterated array.
- [x] Ground placement requires held, unplaced state.
- [ ] Review all source callers and Blueprint consumers.
- [ ] Unreal Engine 5.8 UHT/build and Editor/PIE validation at the agreed final gate.
- [ ] Complete M029–M038 domain integration and evidence; registry remains `unreviewed` / `not_run`.

Source review is not runtime evidence.
