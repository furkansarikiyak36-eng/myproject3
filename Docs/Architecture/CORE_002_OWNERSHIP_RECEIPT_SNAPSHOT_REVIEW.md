# CORE-002 — Preserve Ownership Transfer Receipts Across Snapshots

**Branch:** `fix/ownership-transfer-receipt-snapshots`  
**Base:** `fix/world-simulation-fail-closed`  
**Scope:** Preserve transfer/definition-change idempotency receipts in fixed-step rollback and SaveGame snapshots.  
**Evidence level:** Static source review and targeted structural checks only. UE 5.8 UHT/UBT, Editor, PIE, and runtime retry tests were not run.

## Source finding

`UWorldItemOwnershipSubsystem` kept successful transfer receipts in a transient map so a repeated operation ID could return the original committed result. The original snapshot contained only item records; `RestoreSnapshot(Items)` reset the receipt map. The world-step rollback therefore could erase receipts that predated the failing step. Save/load also omitted these receipts.

## Change made

- Added deterministic snapshot export of both item records and transfer receipts.
- Added an overloaded restore path that validates the proposed item map and receipt map before replacing either map; invalid snapshots do not partially replace either map.
- Preserved receipt history in the coordinator's pre-step rollback snapshot.
- Added `ItemTransferReceipts` to `UWorldSimulationSaveGame` and bumped the schema version from 4 to 5.
- Updated save capture and restore to include receipts.
- Added a narrow v4→v5 compatibility path: old v4 saves load with an empty receipt list. Their already-incremented item revisions remain authoritative, so a retry with a stale pre-transfer revision is rejected rather than reapplying the transfer. It cannot return an old receipt that was not stored in that legacy save.

## Receipt validation

The new restore overload validates unique/valid operation IDs, item references, revision relationships, enum ranges, owner requirements, result-record identity/revision/state, definition-change metadata and finite locations. It commits the proposed items and receipts only after all checks pass.

## Static acceptance

- [x] Export produces stable ordering for item records and receipt IDs.
- [x] Fixed-step rollback captures and restores ownership receipts with item records.
- [x] SaveGame capture and restore include transfer receipts.
- [x] v4 saves have an explicit empty-receipt migration path to schema v5.
- [x] Static structural checks verified both overloads, receipt validation and call-site wiring.
- [ ] UE 5.8 Header Tool / Build Tool compile.
- [ ] Runtime test: repeated transfer request returns the saved original result and changes no state twice.
- [ ] Runtime test: failed fixed-step rollback restores the pre-step receipt set.
- [ ] Runtime test: schema v4 save loads without receipts and schema v5 round-trip preserves receipts.
- [ ] Final integrated 119-mechanic acceptance run.

## Scope boundary

This preserves item ownership subsystem receipts, not every general-purpose `WorldOperationLedgerSubsystem` entry. Actor transforms, structural graph persistence and physical terrain/collision persistence remain separate open integration work.
