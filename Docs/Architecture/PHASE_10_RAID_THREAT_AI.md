# Phase 10 — Raid Director and Threat AI

**Status:** Skeleton only; no implementation completion claimed  
**Registry scope:** M091–M100  
**Prerequisites:** Deterministic clock/seed, player/world state, structure graph/damage, terrain and inventory contracts

## Responsibility

Coordinate persistent threat pressure and raids with readable rules. The director schedules and resolves threat state; individual agents execute behavior through bounded AI adapters. Avoid hidden, unbounded spawning or raid outcomes that bypass world rules.

## Proposed source boundaries

- `RaidDirectorSubsystem`: threat budget, raid schedule and encounter state.
- `ThreatDefinition` / `RaidWaveDefinition`: data-driven composition and difficulty.
- `ThreatTargetResolver`: target selection based on valid world state and explicit priorities.
- `RaidOperationResult`: deterministic outcome and failure/blocked reasons.
- `RaidPersistenceRecord`: schedule, seed, wave progression and unresolved encounter state.

## Contracts to define

- Scheduling uses the shared world clock and deterministic seed.
- Threat budget and spawn count are bounded; invalid locations cannot spawn agents.
- Targeting uses explicit world/structure state, not mesh proximity alone.
- Attacks damage structures through the shared structural damage contract.
- Raid results cannot bypass collision, terrain, trap, durability or resource rules.
- Pausing/reloading cannot duplicate waves, rewards or damage events.
- Difficulty must remain passable through configured equipment/material preparation; no arbitrary unavoidable damage.

## Acceptance gates

- [ ] Raid schedule and encounter state are deterministic and persistent.
- [ ] Spawn/target selection rejects invalid or protected contexts.
- [ ] Structure damage uses one authoritative pathway.
- [ ] Traps and terrain changes interact through documented contracts.
- [ ] Repeated event IDs/reloads do not duplicate waves, rewards or damage.
- [ ] Threat pressure can be tuned and explained with data, not hard-coded hidden rules.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE tests: deferred, not run.
