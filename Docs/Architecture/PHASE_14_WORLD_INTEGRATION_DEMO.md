# Phase 14 — World Integration, Persistence Adapters and 119-Mechanic Demo Plan

## Purpose
Phase 14 is the integration-contract phase after Phase 13. It defines the safe route from independent subsystem foundations to a coherent, auditable demo. It must not imply that the earlier subsystem APIs are already integrated or runtime-verified.

## Branch contract
- Parent: `phase-13-environment-survival-sequential`.
- Keep this PR as a draft and do not merge to `main`.
- Do not rebase on the older `unrealgame` repository or on `main`.

## Integration priorities
1. **One world authority:** define one canonical world seed, monotonic world time, tick owner and operation/event identity policy. Farming, raids, food quality, production and environment must consume the same clock rather than invent local clocks.
2. **One survival writer:** Phase 11's `ULogisticsProductionSubsystem::UpdateSurvival` and Phase 13's `UEnvironmentSurvivalSubsystem::SimulateSurvivalStep` must not both mutate the same survival state in a normal frame. Select one authoritative update path and make any legacy path an adapter or explicitly disabled route.
3. **Transactional operations:** inventory, containers, production, physical objects and structures need explicit ownership and staged validation. Failed operations must not consume materials, alter lifecycle state, emit committed events, or leave partial changes.
4. **Persistence participants:** Phase 12's opaque snapshot container is only transport. Each authoritative owner needs deterministic export, side-effect-free validation, staged import, commit/rollback policy and a schema version.
5. **Dependency-ordered restore:** load candidate snapshot; validate format, checksums, required sections, schema versions, identities and cross-section references; stage all participants; only then commit. A failed participant must leave the currently loaded world unchanged.
6. **Deterministic replay:** seed + canonical time + stable operation inputs should produce the same selected weather, growth/production/raid scheduling decisions where deterministic behavior is specified.
7. **Demo evidence:** every M001–M119 row must map to source owner, dependencies, positive and negative cases, persistence case where required, demo area, and UE 5.8 evidence.

## Suggested section ownership (contracts to verify before coding adapters)
| Section ID | Intended owner | State candidates | Current truth |
|---|---|---|---|
| `CoreWorld` | Core/world contracts | world seed/time, stable IDs, lifecycle and committed operation sequence | Not automatically exported by Phase 12 |
| `LogSystem` | Log/wood | tree/log state, processing stage, carried/placed state, anchors | Adapter not demonstrated |
| `StoneSystem` | Stone/masonry | stone lifecycle, placement and assembly links | Adapter not demonstrated |
| `StickSystem` | Stick/tools | stick/tool state and trap prerequisites | Adapter not demonstrated |
| `InventoryCrafting` | Inventory/crafting | item quantities, reservations, recipes/jobs or crafting state | Adapter not demonstrated |
| `StructuralGraph` | Structure/damage | stable nodes, anchors, edges, damage and collapse state | Adapter not demonstrated |
| `WorldStateTerrain` | Terrain/world state | permitted edits, cell revisions and protection policy | Adapter not demonstrated |
| `FarmingSimulation` | Crops/animals | crop stages, soil inputs, animals, feed/products | Adapter not demonstrated |
| `RaidDirector` | Raids/threat | schedule, wave state, targets and counters | Adapter not demonstrated |
| `LogisticsProduction` | Logistics/production/food/discovery | containers, jobs, food quality, discoveries | Adapter not demonstrated |
| `EnvironmentSurvival` | Weather/survival | season/weather period and environment tuning/state | Adapter not demonstrated |

Section IDs above are proposed integration contracts, not proof that each section is registered, serialized, or compatible today. Cross-owner references must use stable IDs, not raw actor pointers or array indices.

## Tick order proposal
1. Read/advance the single authoritative world clock.
2. Calculate environment/weather state for that time.
3. Apply survival once, through the selected authoritative path.
4. Advance farming, animal needs, food quality and production from the same elapsed world time.
5. Evaluate raid scheduling and threat state.
6. Commit queued world-changing operations and domain events in a documented order.
7. Publish UI/audio/visual notifications only from committed results.
8. Snapshot only at a consistent boundary; never serialize half-applied transactions.

This is a proposed ordering to validate against the actual subsystem contracts, not code that is already running.

## Demo layout (placeholder geometry first)
- Core test bench: identity, lifecycle, operation failures, deterministic clock and save/load.
- Wood/log worksite: tree → logs → processing states → carry/drop → anchor/snap assembly.
- Stone and stick work pads: placement prerequisites and tool/trap state transitions.
- Inventory/crafting station beside the worksite: atomic transfers and recipe transactions.
- Build-and-damage yard: anchors, structural graph, stability and damage propagation.
- Small terrain test cells: allowed/protected layers, excavation limits and persistence.
- Farm/animal pen: season/moisture/growth, feed, products and animal capacity.
- Raid perimeter: scheduled wave, target selection, counters and cancellation.
- Survival/logistics camp: food quality, storage/transport, routes, weather exposure, shelter/clothing and discoveries.
- One compact integration route links the stations; avoid a cluttered mega-scene. Use simple placeholder geometry until the systems pass acceptance.

## Out of scope for this phase
- Claiming all 119 mechanics implemented.
- Changing canonical registry IDs, acceptance criteria or statuses without evidence.
- UE Editor/PIE testing when the user has explicitly deferred it.
- Polished meshes, expensive materials, cinematic sequences or final art.
- Treating static source review as Unreal build/runtime verification.

## Required evidence package
For every acceptance run record: branch/commit SHA, UE version, map/fixture, seed, starting save, operations executed, expected vs observed state, negative-case result, save/load result when applicable, logs and unresolved failures. Mark results as **not run**, **static only**, **UE build**, or **UE runtime**; never collapse these into one “passed” label.
