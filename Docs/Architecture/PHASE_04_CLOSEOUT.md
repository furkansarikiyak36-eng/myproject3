# Phase 4 Closeout — Stone Masonry

**Decision:** Stone-piece/anchor foundation and defensive source corrections added; phase remains open for domain integration and validation.  
**Branch:** `phase-04-stone-masonry-sequential`  
**Base:** `phase-03-wood-processing`

## Delivered
- Stone piece categories and explicit authored snap-anchor data.
- Result-bearing throw operation for loose held stone.
- Source-specific mesh mapping and validated shape transitions for block, slab, column, lintel and corner.
- Dedicated stone actor with held/placed state and parent-child attachment records.
- Anchor permission, occupancy, compatibility, data-integrity and support-chain checks.
- Transform alignment and attachment reservation logic with rollback on failure.
- Connection removal path that releases reservations and restores loose physics.
- Safe end-play cleanup using a snapshot of attached children.
- Phase documentation and acceptance checklist.

## Not claimed
- No Unreal Engine 5.8 build/UHT, Editor, PIE or gameplay test was run.
- The ten M029–M038 registry mechanics are not all implemented by this initial layer.
- Structural load calculations, collapse, damage/repair, bulk inventory, tool/resource costs, physical shaping progress, mixed-material structural integration and persistence remain future work.
- Source-specific stone output mappings are supported by code but have not been populated or validated against imported Unreal assets.
- The 119-mechanic registry remains `unreviewed` / `not_run`.
- This branch is based on the latest Phase 3 source branch, but it does not imply Phase 3 or Phase 2 has passed its own exit gate.

Keep this branch separate and do not merge to `main` before the planned end-of-phases integration gate.
