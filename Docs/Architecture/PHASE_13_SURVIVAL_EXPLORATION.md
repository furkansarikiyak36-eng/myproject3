# Phase 13 — Survival Simulation and Exploration Context

**Status:** Skeleton only; no implementation completion claimed  
**Registry workstream:** M107–M108 and M110–M119, except M109 integration gate  
**Canonical owner:** LogisticsProduction  
**Prerequisites:** Shared clock/seed, core survival state, inventory/equipment, terrain and production interfaces

## Responsibility

Coordinate harsh-but-passable environmental survival and persistent discoveries. This is simulation and world-context work—not a story/cinematic phase. Narrative production remains deferred.

## Proposed source boundaries

- `SurvivalSimulationSubsystem`: shared exposure and need-state updates.
- `WeatherSeasonProvider`: deterministic weather/season values based on shared clock/seed.
- `ExposureResolver`: cold, heat, wetness and shelter/clothing modifiers.
- `NeedStateComponent`: hunger, thirst, stamina, fatigue and rest state.
- `FoodQualityComponent`: quality/spoilage using elapsed world time.
- `ExplorationDiscoverySubsystem`: durable discovery flags and unlock context.
- `SurvivalOperationResult`: explicit transition results and blocked reasons.

## Contracts to define

- Health, stamina, warmth, hunger/thirst and exposure must have one authoritative state owner.
- Weather, seasons, spoilage and fatigue advance from the same deterministic clock.
- Shelter quality and clothing insulation affect the same exposure calculation; no parallel conflicting formulas.
- Injury/illness/poisoning are explicit states/effects with configurable data and persistence.
- Food quality/spoilage does not advance twice during pause, reload or repeated ticks.
- Exploration discoveries persist and unlock world context without requiring cinematic sequences.
- Survival difficulty remains tunable and passable with appropriate materials, shelter and preparation.

## Acceptance gates

- [ ] Weather/season and exposure are deterministic and share one clock.
- [ ] Shelter and clothing effects compose through one resolver.
- [ ] Need/fatigue/injury states do not double-apply on reload or repeated updates.
- [ ] Food quality and spoilage are persistent and bounded.
- [ ] Discovery flags persist and unlock the intended environmental context.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE tests: deferred, not run.
