# P8 — Regional events, snow loading and defensive raids

Status: **code-written / unbuilt** on `game/p08-events-defense`. The P6/P7 ancestor code is also source-only on this branch; no claim is made that Unreal has compiled or run it.

## Added contracts

### P8.1 — Regional hazards

- `Config/GameData/events.json` owns five region profiles and ten event definitions: flood, frost, hail, drought, wildfire, toxic rain, epidemic, snowstorm, avalanche and fog.
- `UWorldRegionalEventSubsystem` parses the JSON transactionally, checks IDs, region references, month/weather values, numeric ranges and chain cycles, then evaluates active events from the shared calendar, clock seed and climate state.
- Event occurrences are deterministic for the same seed/day/region/event. The evaluator combines route-cost, survival-severity, snow-load and raid-defense modifiers and reports structure-damage intensity as data.
- Event chains are explicit: wildfire requires drought, epidemic requires a regional flood event, and avalanche requires a snowstorm event. Invalid/missing definitions fail closed rather than inventing region data.

### P8.2 — Region-targeted, signal-driven raids

- Normal scheduled raids choose a configured region deterministically and use its authored `raid_target`; the legacy random-square target remains only as a fallback if regional data is unavailable.
- `TryStartRaidFromSignals` accepts normalized `[0,1]` storage-value, smell, light and smoke observations. It applies weighted pressure, a configurable threshold and cooldown, assigns the region target, and rejects repeated operation IDs.
- Threat snapshot schema v2 persists signal-operation receipts and cooldown time. Restore accepts v1 snapshots as legacy state with no signal receipts.
- Raid damage now reads regional defensive rating and the event's visibility/coordination modifier. The configured minimum damage fraction prevents defense from making raids mathematically harmless.

### P8.3 — Snow load and structure support

Placed structural actors can declare metadata with actor tags:

| Tag | Meaning |
|---|---|
| `Region:Zone.4` | The actor belongs to a catalogued region |
| `SnowExposed` | Snow can add environmental load |
| `SnowAreaM2:4.0` | Roof/catchment area in square metres; required with `SnowExposed` |
| `RaidDefense:1.0` | Contribution to regional raid defense while intact and supported |

- These tags are parsed when the graph node is registered and reconstructed from the actor when a graph snapshot is restored; no new authoritative save field or whole-save schema bump is needed.
- Snow load is computed from climate snow depth × region coefficient × tagged area × the current event multiplier. It participates in aggregate load and support-path checks, so an overloaded member no longer claims valid support.
- Defense contribution comes only from intact nodes with a valid support path in the raid's target region.

## Deliberate integration boundaries / still open

This is not a claim of full P8 gameplay acceptance. The following still need world/gameplay integration:
- A world sensor or station caller must sample storage value/smell/light/smoke and call `TryStartRaidFromSignals`; the new API is not automatically polled from all stations.
- Hazard modifiers are queryable; route costs and survival-condition triggers are not yet globally driven by the active event set. The evaluator's structure-damage intensity is not an automatic fire-spread/periodic damage loop.
- Repair still needs an item-backed material transaction and player/helper integration. Fire-spread/ignition actors, player warning/HUD feedback, and committed start/end hazard notifications are not implemented by this evaluator alone.
- Region tags and `DefenseTargetNodeIds` must be authored on the placed level actors/configuration. The existing demo has not been repopulated with these tags/targets.
- Tests were added as source only. UHT/UBT, automation tests, save/load in Unreal and PIE have **not** been run.

## Source-level tests added

- `MyProject3.World.Events.RegionalDefinitionsAreValidAndDeterministic`
- `MyProject3.World.Events.SignalRaidIsRegionTargetedAndIdempotent`
- `MyProject3.World.Events.SnowLoadCanOverloadExposedStructure`

The next required proof gate is the agreed batch Unreal build/test pass, then scene tagging and PIE/save-load acceptance.