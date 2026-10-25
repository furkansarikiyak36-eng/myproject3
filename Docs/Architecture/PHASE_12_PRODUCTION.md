# Phase 12 — Primitive Production and Queues

**Status:** Skeleton only; no implementation completion claimed  
**Registry scope:** M103–M106  
**Prerequisites:** Inventory/crafting transactions, logistics, shared clock, fuel/heat state and world infrastructure

## Responsibility

Build a compact progression from primitive wood processing to simple saw/zipline production and queued outputs. The player should understand why a queue is blocked and which resource, station, fuel or route is missing.

## Proposed source boundaries

- `ProductionSubsystem`: queue scheduling and production state.
- `ProductionRecipeDefinition`: inputs, outputs/byproducts, station tags, duration and fuel requirements.
- `ProductionQueueComponent`: queue ownership, current job and blocked reason.
- `ProductionResult`: operation ID, state revision and structured failure.
- `FuelHeatComponent`: fuel consumption, heat and cooking station state.

## Contracts to define

- Recipes are data-driven and validate all inputs/configuration before resource consumption.
- Queue advancement uses deterministic world time, not frame ticks.
- Input consumption, output/byproduct capacity and job state transition are atomic.
- Blocked jobs expose a reason and do not consume resources/time unexpectedly.
- Fuel and heat are authoritative shared state, not independently implemented by crafting and survival.
- Roads/bridges/ropes affect route cost through Phase 11 interfaces.
- Reloading a queued job cannot duplicate outputs or consume inputs twice.

## Acceptance gates

- [ ] Primitive production chain is represented as data and can be extended without new hard-coded branches.
- [ ] Queue reports missing inputs, station, fuel, capacity or route conditions.
- [ ] Failed start/finish leaves inventory and queue state consistent.
- [ ] Fuel/heat/cooking integrates with survival and shared world time.
- [ ] Production queue and outputs survive save/load without duplication.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE tests: deferred, not run.
