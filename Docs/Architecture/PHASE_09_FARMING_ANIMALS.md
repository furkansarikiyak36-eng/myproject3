# Phase 9 — Farming and Animal Husbandry

**Status:** Skeleton only; no implementation completion claimed  
**Registry scope:** M081–M090  
**Prerequisites:** Core world clock/seed, inventory/crafting, world-state terrain and persistence contracts

## Responsibility

Create a deterministic simulation layer for crops, plots, growth stages, harvests, animal needs and outputs. World actors and UI are adapters; the simulation owns authoritative state and resource transactions.

## Proposed source boundaries

- `FarmingSimulationSubsystem`: plot/crop/animal state and time-step coordination.
- `FarmPlotComponent`: plot identity, soil/terrain compatibility and occupancy.
- `CropDefinition` / `CropInstanceState`: data-driven growth, water/nutrient needs and harvest outputs.
- `AnimalDefinition` / `AnimalInstanceState`: needs, health, care and output schedules.
- `FarmOperationResult`: structured failure, resource cost and revision metadata.

## Contracts to define

- Growth uses the shared deterministic world clock; frame rate must not alter growth results.
- Planting and harvesting are atomic inventory/world operations.
- Soil, season, water and other required conditions are validated before committing state.
- Animals have explicit needs/state transitions and cannot generate outputs from invalid or duplicated ticks.
- Harvest/byproduct capacity is checked before consumption or state advancement.
- Save/load preserves growth progress, animal state and production timers.
- Terrain edits and structure placement cannot silently orphan a plot; changes require explicit invalidation or rejection.

## Acceptance gates

- [ ] Crop lifecycle is data-driven and deterministic.
- [ ] Invalid planting/harvest attempts leave inventory and plot state unchanged.
- [ ] Animal care/output is bounded and cannot duplicate on retries or time-step restoration.
- [ ] Inventory capacity and byproducts participate in transaction preflight.
- [ ] Weather/season inputs are shared with Phase 13, not independently simulated.
- [ ] Save/load restores farm state without resetting progress or generating duplicate output.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE tests: deferred, not run.
