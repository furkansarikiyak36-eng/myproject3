# Pass E — Threat Director Contract

## Implemented in this slice
- A world-local threat director with explicit initialization and fixed-step advancement.
- Uses the shared simulation clock; it does not advance from render-frame delta.
- Deterministically schedules a warning window, active raid window, bounded pressure scalar, and target location from world seed and raid serial.
- Exposes explicit resolve (repelled/failed), cancel, current record, next schedule, and bounded terminal history.
- Active raids that exceed their planned end become Failed; a terminal result is recorded before a future raid can start.

## Ownership boundary
This is the raid schedule/state machine only. It does not spawn AI, select real settlement targets, query the structural graph, spend resources, or apply damage. An AI/spawn adapter must consume the record and enact the raid. Target location is a deterministic placeholder within a configurable-by-code square, not a real-world settlement query.

## Integration / persistence
- Must be initialized after `UWorldSimulationClockSubsystem::InitializeClock`.
- The fixed-step simulation driver must call `AdvanceDirector` after advancing the clock.
- Properties are marked SaveGame, but no save coordinator or full snapshot/restore API is wired yet; SaveGame annotations alone do not guarantee persistence.
- Schedule tuning must be authored and validated before gameplay balance.

## Validation status
Source review only. No Unreal Header Tool, UBT build, editor, PIE, or gameplay test was run.
