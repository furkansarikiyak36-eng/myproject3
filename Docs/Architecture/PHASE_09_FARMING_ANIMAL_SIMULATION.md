# Phase 9 — Farming & Animal Husbandry

**Branch:** `phase-09-farming-animal-simulation-sequential`  
**Base:** `phase-08-terrain-worldstate-sequential`  
**Scope:** M081–M090  
**Status:** Initial deterministic domain foundation; all mechanics remain unverified.

## Implemented foundation

- Fixed-size deterministic simulation ticks and a simulation-owned clock.
- Configurable crop and animal definitions with duplicate/invalid definition rejection.
- Planting checks required season, soil moisture and nutrient thresholds.
- Crop stage progression, moisture/nutrient adjustments, stress tracking and deterministic yield calculation.
- Animal registration with a caller-supplied per-species habitat capacity, feeding reserve, growth/health updates, product collection state and manure nutrient accumulation.
- Explicit result-bearing operations and read-only snapshots.

## Important limitations

- This subsystem is an in-memory state ledger. It does not own or consume inventory resources; feeding and harvesting report state changes but must be connected to the authoritative inventory transaction system before production use.
- Soil inputs are supplied by callers; no terrain moisture/nutrient integration exists yet.
- The tick is deterministic given identical state and calls, but no persisted world clock, random seed, save/load, season calendar, reproduction/spawning, animal movement, welfare/habitat simulation, or domain event emission is implemented.
- Product production uses a simple integer-ready counter, not realistic milk/egg/wool rates. No actor visuals or world interactions are wired.
- No Unreal Engine 5.8 UHT/build, Editor, PIE or runtime tests were run.

M081–M090 remain open in the 119-mechanic registry.