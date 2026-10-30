# P8 — Regional events, survival exposure, wildfire and defensive raids

Status: **code-written / unbuilt** on `game/p06-p08-blind-code-closure`. No UHT/UBT build, Unreal automation run, PIE or save/load acceptance has been performed.

## P8.1 — Regional hazards and player exposure

- `Config/GameData/events.json` owns five region profiles and twelve event definitions: flood, frost, hail, drought, wildfire, toxic rain, epidemic, snowstorm, avalanche, fog, windstorm and lightning. The active windows and event chains are deterministic from shared clock/calendar/seed/climate inputs.
- `UWorldRegionalEventSubsystem` parses the definitions transactionally, validates IDs/references/ranges/chains, and aggregates route cost, raid defense, snow load, structure damage, survival exposure and fire ignition/spread/burn modifiers.
- Zone entry now records `RegionId` and player location in the same survival-profile mutation as the configured discovery. Operation receipts preserve retry semantics.
- The post-clock coordinator step applies bounded regional health, fatigue, stamina, thirst, wetness and core-temperature changes. Shelter protection, insulation and water resistance mitigate exposure. `LastRegionalExposureTickIndex` prevents a repeated call from applying the same logical tick twice and is saved with the profile.
- Remaining: convert selected hazards into named Injury/Illness/Poisoning conditions, contamination of water/crops/food from toxic rain or ash, hazard start/end notifications and player warning HUD; connect real zone volumes to zone entry.

## P8.2 — Region-targeted, signal-driven raids

- Normal scheduled raids choose a configured region deterministically and use its authored `raid_target`.
- The adapter scans region-tagged actors for normalized `RaidSignal.Storage`, `Smell`, `Light` and `Smoke` contributions. Values aggregate per region and logical tick; the threat director applies weighted pressure, threshold, cooldown and target region. Threat snapshot v2 persists the signal receipts/cooldown.
- Combustible actors add `RaidSignal.FireLight:0.35` and `RaidSignal.FireSmoke:0.65` while burning; those tags are parsed as light/smoke raid inputs. The existing sensor values for storage/smell/ordinary lighting/smoke still need real producers rather than manually authored numeric tags.
- Supported regional defenses reduce raid pulse damage, with a non-zero minimum damage floor.
- Remaining: connect live storage value, scent, ordinary lights and smoke emitters; author region/target/defense tags in the level; configure and verify raid pawn materialization and save/load.

## P8.3 — Snow, structural damage, repair and wildfire

- Placed structural actors use `Region:<id>`, `SnowExposed`, `SnowAreaM2:<area>` and `RaidDefense:<rating>` metadata. Climate snow depth × regional coefficient × exposed area × event modifier contributes to graph loads and support validation.
- The fixed-step coordinator applies regional structural damage and snow overload through the graph's leaf-first collapse resolver. A post-commit failure faults the simulation rather than continuing with uncertain actor state.
- `TryRepairStructureWithMaterial` validates an owned Resource/BuildingPart item, expected node/item revisions and an operation receipt; retries do not consume a second item. Player/helper repair interaction and per-piece material recipes are not wired.
- `UWorldCombustibleComponent` is separate from campfire `UHeatSourceComponent`. It stores a stable identity, region, fuel, moisture, ignition susceptibility/threshold, spread radius, optional point firebreak flag, ignition receipts, burn/moisture tick markers and optional structural node/damage tuning.
- `UWorldFirePropagationSubsystem` evaluates regional event modifiers and advances combustible components after the clock commit. Natural ignition/spread use stable seed/tick/region/source/target hashes; spread is limited to the same region and radius, blocked by a point firebreak component, reduced by moisture, and newly ignited surfaces do not burn/relay until the following tick. Fuel consumption, optional linked structural damage and dynamic fire light/smoke tags are included.
- Remaining: author combustible components on trees/thatch/log piles/wood structures, bind graph node IDs and tune material-specific values; replace point firebreak prototype with actual line/volume geometry; add extinguishing/water suppression, visual/audio fire and smoke, fire-driven material destruction beyond linked structural damage; verify component SaveGame round-trip and collapse in Unreal.

## Source-only automation tests

`GameWorldTests.cpp` currently contains 41 automation tests total. Fifteen named Farming/Events tests cover field work, crop/seed persistence, fence action, harvest storage, hand/station processing, regional event definitions/terrain filters, survival exposure, wildfire, signal raids, snow collapse, route cost, repair, region-tag sensors and countermeasures. None has been executed on this branch.

The required proof gate remains one batch Unreal UHT/UBT build, full automation suite, fixes, then PIE and save/load acceptance. Do not mark these tasks built/tested/played until those steps actually happen.
