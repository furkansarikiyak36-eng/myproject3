# Phase 11 — Logistics and Resource Movement

**Status:** Skeleton only; no implementation completion claimed  
**Registry workstream:** M101–M102 (canonical owner remains LogisticsProduction)  
**Prerequisites:** Inventory, physical-piece state, world identity and structural/placement contracts

## Responsibility

Model the early logistics loop: gather/move resources and make hauling, storage and route choices meaningful without introducing an overcomplicated simulation. This workstream separates transport from production so production queues can consume well-defined inputs.

## Proposed source boundaries

- `LogisticsSubsystem`: transport requests, destination and route-cost queries.
- `ResourceContainerComponent`: capacity, accepted categories and committed contents.
- `HaulRequest/Result`: source, destination, quantity, operation ID and blocked reason.
- `RouteCostProvider`: shared traversal cost interface for terrain, roads, bridges and ropes.

## Contracts to define

- Transfers reserve/check source quantity and destination capacity before committing.
- A failed transfer leaves both source and destination unchanged.
- Physical carried objects remain distinct from abstract stack transfers; adapters must not duplicate the same resource in both forms.
- Route cost consumes world/terrain and infrastructure queries; it does not own roads/bridges/ropes.
- Storage, inventory and world pickups share item identity/category definitions.
- Repeated transfer requests are idempotent where operation IDs are supported.

## Acceptance gates

- [ ] Transfer preflight checks source amount and destination capacity.
- [ ] Partial transfer behavior is explicit; no silent item loss.
- [ ] Physical carry and stack inventory cannot represent the same unit twice.
- [ ] Route cost reflects valid infrastructure and terrain changes.
- [ ] Save/load restores container contents and outstanding logistics state safely.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE tests: deferred, not run.
