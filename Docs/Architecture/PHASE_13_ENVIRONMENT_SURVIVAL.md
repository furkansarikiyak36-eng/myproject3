# Phase 13 — Environment, Seasons and Survival

## Registry scope
This phase targets the unverified M110–M118 environment/survival cluster: seasonal weather, cold/heat/wetness, hunger/thirst, rest/stamina/fatigue, illness/injury remain shared state, clothing insulation and shelter protection. It does not change registry status.

## Added foundation
- A world-seed/time-driven weather state with four seasons and bounded weather profiles.
- Deterministic weather selection from world seed and weather-period index; same inputs should select the same period weather.
- A bounded environment clock step with duplicate operation-ID rejection.
- A survival simulation step that uses weather, precipitation, wind, shelter quality, clothing insulation, waterproofing and heat-source strength.
- Editable survival tuning values for later balance work.

## Time model
The prototype uses a configurable simulation convention of 60 real seconds per in-game hour and 90 in-game days per season. These are scaffolding defaults, not final design decisions. Weather rolls change in six-game-hour periods. The world clock must eventually be provided by one authoritative CoreWorld clock; this subsystem currently maintains its own time state.

## Integration gaps
- This subsystem does not spawn rain/snow particles, change sky/light, create lightning, trigger avalanches/earthquakes, alter landscape snow, or route weather into crop/animal simulation.
- It does not yet consume food/water items, equip clothing actors, query actual shelter geometry, manage cooking/fuel, or persist its state through Phase 12 adapters.
- Illness/injury/poisoning fields are preserved but no diagnosis/treatment rules are implemented here.
- The existing LogisticsProduction survival tick remains in the codebase; integration must select one authoritative survival update path to avoid double ticking.
- M107/M111–M118 remain only partially covered by this subsystem; no registry status is changed.

## Validation status
No Unreal Engine 5.8 UHT/build, Editor, PIE or runtime tests were run. See the acceptance checklist before claiming completion.
