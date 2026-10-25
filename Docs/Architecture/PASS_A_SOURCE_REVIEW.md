# Pass A — Source Review Notes

**Status:** Review by source inspection only. Unreal build/UHT/runtime gates are not run.

## Changes in this pass

- Added `UWorldSimulationClockSubsystem` with explicit initialization, guarded logical-time advancement, tick index, world seed, operation-derived deterministic seed and versioned snapshot export/restore.
- Updated committed-event publication to require an initialized simulation clock and use its logical time. This removes dependence on `UWorld::GetTimeSeconds()` for event timestamps.
- Changed inventory weight handling to fail closed when a stack lacks a positive configured weight. Unknown-weight stacks no longer count as weight zero in capacity checks; `GetCurrentWeightGrams` returns `MAX_int32` when the current inventory has invalid weight data.

## Contract limits that remain

- The clock is not auto-initialized; bootstrap code must set the seed before committed-event publication.
- The clock does not tick itself. A future fixed-step simulation driver must advance it.
- Snapshot export/restore is not automatic durable save/load; Phase 14 must persist it and restore it before dependent systems.
- Operation-derived seeds are deterministic integer mixing, not a cryptographic hash.
- There is still no operation ledger/deduplication store or generic transaction coordinator.
- Inventory item definitions/weights must be authored before adding or crafting those items. This strict behavior may reveal missing data configuration and must be covered by acceptance scenarios.
- The registry statuses for M005/M006/M050 remain unchanged pending full integration and engine validation.

## Static source review checklist

- [x] Negative/non-finite logical-time delta rejected.
- [x] Snapshot schema/time/tick/revision validated on restore.
- [x] Event timestamp no longer reads frame/world elapsed time.
- [x] Unknown or non-positive inventory weight fails capacity validation.
- [ ] Confirm Unreal Header Tool compatibility.
- [ ] Confirm UHT/C++ build on the project's exact Unreal version.
- [ ] Wire clock initialization into the actual game/world bootstrap.
- [ ] Wire fixed-step simulation driver and snapshot into save coordinator.
- [ ] Run source-level acceptance tests, then final authorized Unreal build/UHT/Editor/PIE/gameplay gate.

**No engine validation is claimed.**
