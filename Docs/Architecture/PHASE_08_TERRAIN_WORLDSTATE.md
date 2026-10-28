# Phase 8 — Terrain, World State and Digging

**Branch:** `phase-08-terrain-worldstate-sequential`  
**Base:** `phase-07-structural-graph-sequential`  
**Target:** Unreal Engine 5.8  
**Status:** Scope and contracts first; M071–M080 remain unverified.

## Goals

- M071: partition mutable world state into stable, versioned chunks.
- M072: allow edits only in configured diggable layers; reject protected layers.
- M073: represent valid pits with consistent geometry/collision updates.
- M074: bound horizontal tunnels and small rooms by depth and volume.
- M075: expose support/instability diagnostics before underground collapse behavior is enabled.
- M076: define persistence/versioning boundary for terrain edits.
- M077: track local water amount and flow inputs.
- M078: track soil moisture, nutrients and runoff inputs.
- M079: track fire/rot/material-exposure state without claiming a complete simulation.
- M080: represent authored ruins/clues and durable discovery state.

## Architecture decision

Do not replace the whole Unreal Landscape with a voxel world. Use a hybrid: normal Landscape for the broad world, with explicitly configured editable cells/chunks in limited excavation zones. Protected layers and configured maximum depth/volume are authoritative. Terrain state, collision rebuild and save/load must share a versioned change record; visual mesh edits alone are not accepted as committed excavation.

## Implementation boundary

This phase must not silently mutate Unreal Landscape collision or claim working voxel excavation until the selected UE 5.8 implementation path is integrated and validated. Underground support checks are diagnostics until Phase 7's shared structural graph is implemented. No mechanic registry statuses change without evidence. No Unreal UHT/build, Editor, PIE or runtime tests are claimed here.