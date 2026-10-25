# Phase 8 — Terrain, World State and Constrained Digging

**Status:** Skeleton only; no implementation completion claimed  
**Registry scope:** M071–M080  
**Prerequisites:** Core identity/clock/save contracts; structural graph integration where terrain supports buildings

## Responsibility

Own deterministic, bounded terrain edits and durable world-state changes. The target is selective digging for underground rooms, channels and traps—not unrestricted high-cost voxel editing across the entire world.

## Proposed source boundaries

- `WorldStateTerrainSubsystem`: authoritative edit requests and world-state revisions.
- `TerrainEditPolicy`: editable layer, depth, region, material and protected-area rules.
- `TerrainEditRequest/Result`: operation ID, target region, requested delta, failure reason and revision.
- `TerrainChunkChangeRecord`: compact changed-cell/region data for persistence.
- `WorldDiscoveryState`: persistent discovery/unlock flags, coordinated with Phase 13/14.

Names are proposed boundaries, not existing source files.

## Contracts to define

- Only explicitly editable strata and configured depth/range may be modified.
- Every edit has bounded work and memory cost; invalid or out-of-range requests fail before mutation.
- Terrain collision/navigation and structural support invalidation are emitted as committed changes after the authoritative edit.
- Repeated operation IDs cannot apply the same edit twice.
- Save data stores deltas/versioned records rather than blindly serializing an unbounded whole world.
- Restore order must establish terrain before dependent structures, traps and navigation state.
- Discoveries and environmental context are data-driven; no cinematic/story dependency.

## Acceptance gates

- [ ] Editable and protected layers are explicit and configurable.
- [ ] Digging supports bounded underground spaces/traps without permitting unrestricted world excavation.
- [ ] Invalid depth/material/region requests fail without partial terrain change.
- [ ] Structure support and navigation consumers receive consistent post-commit updates.
- [ ] Terrain changes survive save/load and version mismatch is handled safely.
- [ ] Deterministic replay produces the same result for the same seed and operation sequence.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE tests: deferred, not run.
