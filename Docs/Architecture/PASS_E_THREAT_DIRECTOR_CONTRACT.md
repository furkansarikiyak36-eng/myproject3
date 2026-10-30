# Pass E — Threat Director Contract

## Implemented in source
- A world-local threat director with explicit initialization and fixed-step advancement from the shared simulation clock.
- Deterministically schedules warning and active raid windows with bounded pressure and terminal history.
- Normal scheduled raids choose a configured regional target when `events.json` is available; a random-square target is kept as a legacy fallback.
- `TryStartRaidFromSignals` accepts normalized storage-value, smell, light and smoke signals. It uses a weighted threshold and cooldown, targets a named region, and rejects repeated operation IDs.
- Signal operation receipts and last signal-raid time are persisted in threat snapshot schema v2. Restore accepts schema v1 saves with an empty receipt set.
- `WorldRaidAdapterSubsystem` applies damage pulses through the canonical structural graph. Regional defense tags reduce damage, while a minimum-damage floor prevents complete immunity.

## Ownership boundary
The director owns raid planning/state and signal receipts. It does not sample every storage container, smoke source, light or scent emitter itself; the world-owned sensors/stations must call the signal API. The adapter does not own structural graph data, and it does not apply repairs or material transactions.

## Regional targets and defense
`events.json` defines stable region IDs and raid target locations. Structural actors contribute only when tagged with `Region:<id>` and `RaidDefense:<rating>`, and only when the graph says the piece is intact and supported. Configure `DamageReductionPerDefenseRating` and `MinimumRaidDamageFraction` on the adapter.

## Persistence and validation
The threat snapshot schema moved from v1 to v2 for signal receipts/cooldown; v1 snapshots remain accepted. The main world save schema does not change solely for this nested snapshot change. New target-region fields default to `NAME_None` for old saves.

## Status / remaining integration
Source-written on `game/p08-events-defense`; no Unreal Header Tool, UBT, automation run, editor, PIE or save/load test has been executed. Wire real signal producers, configure region/defense tags and pawn/target data in the level, and verify old-save migration in Unreal before calling this tested.