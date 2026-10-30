# Session checkpoint — P8 events and defense

Date: 2026-10-30
Branch: `game/p08-events-defense`
Status: `code-written` only; no Unreal build or automation tests were run.

## Implemented in source
- `Config/GameData/events.json`: five region profiles with target coordinates and snow coefficients; twelve event definitions with regional/month/weather/climate/terrain conditions, probabilities, durations and route/survival/structure/snow/defense modifiers.
- `UWorldRegionalEventSubsystem`: transactional JSON load/validation, supported event types, reference/range checks, chain-cycle rejection and deterministic seed/day/region/event-based evaluation.
- `UWorldStructuralGraphSubsystem`: reconstruct runtime region/snow/defense metadata from actor tags; calculate snow load as depth × region tuning × area × event multiplier; include it in aggregate load and live support checks. Snapshot restore validates authored structural loads separately so temporary weather overload does not make a previously valid save un-loadable.
- `UWorldThreatDirectorSubsystem`: choose configured region targets for scheduled raids; add storage-value/smell/light/smoke signal trigger API with weighted pressure, threshold, cooldown and bounded idempotency receipts. Threat snapshot v2 persists signal receipts/cooldown and accepts v1 snapshots.
- `UWorldRaidAdapterSubsystem`: regional defense rating and current event defense modifier scale damage pulses with a non-zero minimum damage floor.
- `Tests/GameWorldTests.cpp`: tests for event data/deterministic evaluation, region-targeted idempotent signal raids/defense rating, and snow-load overload behavior.

## Documents updated
- `Docs/Architecture/P8_EVENTS_DEFENSE_CLOSEOUT.md`
- `Docs/Architecture/PASS_E_THREAT_DIRECTOR_CONTRACT.md`
- `memory/PLAN_STATUS.md`, `memory/CURRENT_TASK.md`, `memory/ROADMAP.md`, `memory/CODEBASE.md`

## Known gaps / proof gate
- Signal tags are sampled automatically each fixed step, but they still need authored region/signal tags or a component that feeds live storage, smoke, smell and light values.
- Route-cost multipliers and fixed-step structural damage/snow collapse are connected. Survival-condition, water/crop pollution, hazard transition notifications and fire-spread consumers remain.
- Item-backed material repair is source-written; helper/player repair integration and repair UI remain open.
- Actor tags (`Region:<id>`, `SnowExposed`, `SnowAreaM2:<area>`, `RaidDefense:<rating>`) and `DefenseTargetNodeIds` require level authoring; the existing demo scene has not been tagged/re-targeted.
- Added tests are source only. No UHT/UBT build, automation tests, Unreal save/load or PIE run was performed. Regenerate `memory/CODE_INDEX.md` locally at the agreed batch build gate.