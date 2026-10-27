# Phase 14 — Cross-System Integration, Persistence and Demo Coverage

**Status (2026-10-28):** Save coordinator and domain snapshots exist in source, with voxel density added in schema v14 on `integration/phase-02-14-closure`; full runtime restoration/119-mechanic demo remains unverified.  
**Registry gate:** M109 plus integration acceptance for M001–M119  
**Prerequisites:** All phase source layers and the mechanic registry

## Responsibility

Own the final integration contracts, save/load order, source-level consistency audit and the eventual 119-mechanic demo plan. Do not merge stacked work or claim engine correctness before the user-authorized final gate. This phase does not create story content.

## Proposed source boundaries

- `WorldSaveCoordinatorSubsystem` / `WorldSimulationSaveGame`: versioned save/load orchestration and ordered restore exist in source.
- `FWorldVoxelChunkSnapshot`: new schema-v14 payload stores chunk actor identity, density dimensions/data/revision and bounded-dig receipts.
- `CrossSystemConsistencyAudit`, full `MechanicCoverageManifest`, and `DemoScenarioDirector` remain proposed/future boundaries or incomplete artifacts; their existence is not implied by the save coordinator.

## Restore ordering proposal

1. World header, schema version, deterministic clock/seed and stable identity table.
2. Terrain/world-state deltas and environment state.
3. Physical actors/pieces and their authoritative local states.
4. Structural graph, anchor reservations and support relationships.
5. Inventory, containers, farm plots/animals and resource ownership.
6. Production queues, fuel/heat, raid schedule and threat state.
7. Player survival/equipment, spoilage timers and exploration discoveries.
8. Rebuild derived collision/navigation/visual state; validate cross-references before gameplay resumes.

Voxel chunk density is now captured and validated separately from generic actor SaveGame fields. On restore, its mesh/collision is rebuilt after physical actor state and before the structure graph is restored. The current implementation validates that every initialized voxel chunk is already present and has a stable unique identity; it does not recreate missing voxel actors. Ordering, rollback and physics behavior still require UE 5.8 runtime verification.

## Atomicity and integration gates

- Failed operations must not leave inventory, anchors, terrain, structure graph or queue state partially mutated.
- Domain events are published after authoritative commit; observers must not be able to reinterpret event delivery failure as operation rollback.
- Restore must reject or safely migrate unknown versions and dangling references.
- Stable IDs and operation IDs prevent duplicate creation/consumption across retries and load.
- Structural, terrain, raid and survival systems share the same state and event contracts.
- Source-level checks must be documented separately from Unreal/UHT/runtime verification.

## 119-mechanic demo coverage manifest

For each canonical M-ID, eventually record:
- scenario/location and prerequisites;
- starting state and required assets;
- exact player/system action;
- expected state/result and visible feedback;
- failure-path check where relevant;
- cleanup/reset so scenarios do not contaminate later checks;
- evidence status: not planned / planned / source-reviewed / engine-verified.

The demo should test mechanics in useful, intentional locations; avoid clutter and do not force all mechanics into one tiny scene. Multiple focused stations/areas are preferred.

## Current persistence source delta — not engine accepted

- [x] SaveGame schema version raised to 14 with `FWorldVoxelChunkSnapshot`: actor identity, cells-per-axis, voxel size, density revision, density samples and bounded-dig receipts.
- [x] Save capture refuses initialized voxel chunks without valid unique actor identity or a valid export.
- [x] Load pre-validates a complete one-to-one snapshot set against initialized chunks in the current level.
- [x] Restoring density rebuilds the chunk mesh and collision.
- [x] Legacy schema 13 advances to 14 without synthesizing density; it fails closed if the current level has initialized voxel chunks but the old save lacks their snapshots.
- [ ] Missing physical actors/chunks are not reconstructed; authored level identity must match the save.
- [ ] Save/load round-trip and rollback have not been run through Unreal 5.8 / PIE in this session.
- [ ] Logical terrain cells and physical voxel density are not yet transactionally reconciled.
- [ ] The M001–M119 demo map and evidence coverage are not complete or accepted.

## Acceptance gates

- [ ] All 119 IDs map to an owner, dependency chain and acceptance criterion.
- [ ] Every implemented mechanic has source review status separate from engine status.
- [ ] Integrated save/load restores terrain, structures, inventory, production, farming and threat progression consistently (M109).
- [ ] Cross-system failure paths preserve atomicity and avoid duplicate resources/events.
- [ ] Every M-ID has a practical demo scenario and expected result.
- [ ] Final demo scene layout is useful, readable and non-cluttered.
- [ ] Unreal build/UHT/Editor/PIE/gameplay validation is performed only at the final gate when authorized.
