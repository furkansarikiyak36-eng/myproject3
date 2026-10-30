# P8 — Regional events, snow loading and defensive raids

Status: **code-written / unbuilt** on `game/p08-events-defense`. The P6/P7 ancestor code is also source-only on this branch; no claim is made that Unreal has compiled or run it.

## Added contracts

### P8.1 — Regional hazards

- `Config/GameData/events.json` owns five region profiles and twelve event definitions: flood, frost, hail, drought, lightning, wildfire, ash/toxic rain, epidemic, snowstorm, avalanche, windstorm and fog. Snow load is calculated separately from climate depth and exposed roof area.
- `UWorldRegionalEventSubsystem` parses the JSON transactionally, checks IDs, region references, month/weather values, numeric ranges and chain cycles, then evaluates active events from the shared calendar, clock seed and climate state.
- Event occurrences are deterministic for the same seed/day/region/event. The evaluator combines route-cost, survival-severity, snow-load and raid-defense modifiers and reports structure-damage intensity. Region-tagged route edges consume the route-cost multiplier directly.
- Event chains are explicit: drought → lightning → wildfire → ash/toxic rain; flood → epidemic; and snowstorm → avalanche. Invalid/missing definitions fail closed rather than inventing region data.

### P8.2 — Region-targeted, signal-driven raids

- Normal scheduled raids choose a configured region deterministically and use its authored `raid_target`; the legacy random-square target remains only as a fallback if regional data is unavailable.
- `UWorldRaidAdapterSubsystem::AdvanceRegionalRaidSignals` scans region-tagged actors on each committed fixed step and aggregates tags `RaidSignal.Storage:<0..1>`, `RaidSignal.Smell:<0..1>`, `RaidSignal.Light:<0..1>`, and `RaidSignal.Smoke:<0..1>`. Contributions are summed and saturated per region/signal family.
- The adapter sends one immutable sample per region and logical tick to `TryStartRaidFromSignals`. The director applies weighted pressure, threshold and cooldown, assigns the region target, and persists an operation receipt. Repeating the same tick sample returns success without creating another raid.
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
- Snow load is computed from climate snow depth × region coefficient × tagged area × the current event multiplier. It participates in aggregate load and support-path checks. The coordinator runs a post-commit hazard pass each fixed step; over-capacity exposed supports are damaged to the collapse threshold and removed through the existing leaf-first collapse resolver.
- Active event damage rates (windstorm, hail, wildfire, avalanche and snowstorm) are applied to tagged structural nodes once per committed fixed step. A failed post-commit collapse marks the simulation faulted rather than allowing later ticks to continue.
- `TryRepairStructureWithMaterial` repairs a damaged graph node only after consuming a unique inventory-owned `Resource` or `BuildingPart` item. The operation ledger stores the repair receipt; same operation ID and exact request returns the original result without consuming another item.
- Defense contribution comes only from intact nodes with a valid support path in the raid's target region.

## Deliberate integration boundaries / still open

This is not a claim of full P8 gameplay acceptance. The following still need world/gameplay integration:
- Scene actors still need `Region:<id>` and `RaidSignal.*` tags, or a real sensor component that updates equivalent values. The adapter samples tagged actors automatically, but it does not calculate live values from every storage container, light component, smoke emitter or food source.
- Regional route cost and structural damage are now consumed automatically; survival-condition triggers, water/crop contamination and hazard transition notifications are not yet connected. Fire spread/ignition propagation is not implemented.
- Material-backed repair is source-written, but player/helper interaction, repair animations/visual feedback and material balance are not integrated. Fire spread/ignition actors, player warning/HUD feedback and committed start/end hazard notifications remain open.
- Region tags and `DefenseTargetNodeIds` must be authored on the placed level actors/configuration. The existing demo has not been repopulated with these tags/targets.
- Six P8 automation tests were added as source only. UHT/UBT, automation tests, save/load in Unreal and PIE have **not** been run.

## Source-level tests added

- `MyProject3.World.Events.RegionalDefinitionsAreValidAndDeterministic`
- `MyProject3.World.Events.SignalRaidIsRegionTargetedAndIdempotent`
- `MyProject3.World.Events.SnowLoadCanOverloadExposedStructure`
- `MyProject3.World.Events.RegionalHazardsIncreaseRouteCost`
- `MyProject3.World.Events.StructuralRepairConsumesOwnedMaterialOnce`
- `MyProject3.World.Events.RegionTaggedSensorsCanTriggerRaid`

The next required proof gate is the agreed batch Unreal build/test pass, then scene tagging and PIE/save-load acceptance.