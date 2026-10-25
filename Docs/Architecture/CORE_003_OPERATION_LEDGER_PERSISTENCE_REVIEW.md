# CORE-003 — Operation Ledger Snapshot and Save Persistence

**Branch:** `fix/durable-operation-ledger`  
**Base:** `fix/ownership-transfer-receipt-snapshots`  
**Pull request:** [#20 — Persist operation ledger snapshots](https://github.com/furkansarikiyak36-eng/myproject3/pull/20)  
**Scope:** Preserve completed generic operation-ledger entries through fixed-step rollback and durable SaveGame snapshots; reject saving an in-flight operation.  
**Evidence level:** Static source inspection and targeted structural checks only. UE 5.8 UHT/UBT, Editor, PIE and runtime save/load/fault-injection tests were not run.

## Source finding

`UWorldOperationLedgerSubsystem` initially held Pending/Completed entries only in a transient map. It had no snapshot/restore API and was not included in `UWorldSimulationSaveGame`. This meant any operation paths using the subsystem would lose their completed-result de-duplication history after world reload. Saving a Pending operation would also be unsafe because the ledger does not contain a transaction journal capable of recovering partial domain mutations.

A source spot-check of `LogBuildPiece.cpp`, `InventoryCraftingComponent.cpp`, `StoneBuildingPiece.cpp`, `StickPrimitivePiece.cpp`, `WorldTerrainStateSubsystem.cpp` and `WorldStructuralGraphSubsystem.cpp` found no direct calls to `TryBeginOperation` / `CompleteOperation`. The ledger's API is not yet adopted by all gameplay operations, so persistence alone does not make those operations idempotent.

## Change made

- Added deterministic snapshot export (sorted by operation ID) and atomic restore for the operation ledger.
- Restore validates entry IDs, unique IDs, state enum values, completed-result identity, result code/success consistency and affected object IDs before replacing the live map.
- Added `HasPendingOperations`; SaveGame capture refuses the world while any operation is Pending.
- Added operation-ledger entries to SaveGame schema v6; capture and restore include the ledger.
- Added fixed-step rollback snapshot/restore of the ledger so a failed tick restores the prior entry set.
- Added a narrow migration chain: v4 → v5 initializes the transfer-receipt field; v5 → v6 initializes the operation-ledger field. Legacy saves do not invent old receipt or operation history.

## Static acceptance

- [x] Snapshot export/restore APIs exist and export has stable ordering.
- [x] Restore validates all entries before swapping the live map.
- [x] Fixed-step rollback includes the operation ledger.
- [x] Save refuses pending operations.
- [x] SaveGame v6 captures/restores ledger entries and includes v4/v5 compatibility initialization.
- [x] Structural checks confirm call-site wiring and schema migration paths.
- [ ] UE 5.8 Header Tool / Build Tool compile.
- [ ] Runtime test: completed operation result survives save/load and duplicate ID returns the original result.
- [ ] Runtime test: pending operation causes SaveWorldState to reject without changing state.
- [ ] Runtime test: failed fixed step restores the pre-step ledger.
- [ ] Final integrated 119-mechanic acceptance run.

## Scope boundary / next work

The generic operation ledger still does not coordinate transactions or make mutations atomic by itself. Domain operations must be mapped and selectively connected to it, while retaining existing domain-owned receipts where those already provide the correct idempotency boundary. Avoid routing one operation through two competing authorities.
