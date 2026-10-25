# Phase 14 — Cross-System Integration, Persistence and Demo Coverage

**Status:** Skeleton only; final validation and demo not run  
**Registry gate:** M109 plus integration acceptance for M001–M119  
**Prerequisites:** All phase source layers and the mechanic registry

## Responsibility

Own the final integration contracts, save/load order, source-level consistency audit and the eventual 119-mechanic demo plan. Do not merge stacked work or claim engine correctness before the user-authorized final gate. This phase does not create story content.

## Proposed source boundaries

- `WorldSaveCoordinator`: versioned save/load orchestration and ordered restore.
- `WorldStateSnapshot`: stable identity/revision and domain payload references.
- `CrossSystemConsistencyAudit`: documented source-level invariants and report format.
- `MechanicCoverageManifest`: M001–M119 → prerequisites, test location, setup, action, expected result and cleanup.
- `DemoScenarioDirector`: deterministic scenario setup only after the mechanics are integrated.

These are proposed design boundaries, not existing source files.

## Restore ordering proposal

1. World header, schema version, deterministic clock/seed and stable identity table.
2. Terrain/world-state deltas and environment state.
3. Physical actors/pieces and their authoritative local states.
4. Structural graph, anchor reservations and support relationships.
5. Inventory, containers, farm plots/animals and resource ownership.
6. Production queues, fuel/heat, raid schedule and threat state.
7. Player survival/equipment, spoilage timers and exploration discoveries.
8. Rebuild derived collision/navigation/visual state; validate cross-references before gameplay resumes.

The exact ordering must be refined against actual Unreal serialization and actor lifecycle behavior; this document is not evidence that persistence exists.

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

## Acceptance gates

- [ ] All 119 IDs map to an owner, dependency chain and acceptance criterion.
- [ ] Every implemented mechanic has source review status separate from engine status.
- [ ] Integrated save/load restores terrain, structures, inventory, production, farming and threat progression consistently (M109).
- [ ] Cross-system failure paths preserve atomicity and avoid duplicate resources/events.
- [ ] Every M-ID has a practical demo scenario and expected result.
- [ ] Final demo scene layout is useful, readable and non-cluttered.
- [ ] Unreal build/UHT/Editor/PIE/gameplay validation is performed only at the final gate when authorized.
