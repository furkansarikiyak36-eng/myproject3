# Operation Ledger — Source Contract

**Status:** Snapshot/restore and SaveGame integration exist in source. Unreal validation is pending. This subsystem is not a transaction coordinator, and not every gameplay domain has adopted it.

## Required caller protocol

1. Receive or create one operation ID for one logical command.
2. Call `TryBeginOperation`; if it returns false, do not mutate. Query a completed result if the ID was already used.
3. Validate all prerequisites and preflight all participating systems.
4. Commit through the owning domain transaction.
5. Record the final success or failure using `CompleteOperation`.
6. Only call `CancelUncommittedOperation` when the caller can prove no authoritative mutation occurred.

## Safety properties

- Duplicate IDs cannot start a second operation while the entry remains in this world's ledger.
- The ledger refuses new IDs at capacity rather than evicting old IDs and silently permitting replay.
- Completed results can be returned to retries.
- Snapshot export is ordered by operation ID for stable evidence and comparison.
- Snapshot restore validates all entries before replacing live state.
- The fixed-step coordinator checkpoints/restores the ledger during a failed step rollback.
- Save capture is rejected while any operation is Pending; the ledger has no transaction journal that could safely recover an interrupted mutation.
- Saved completed entries restore the same operation/result IDs so retries can return the saved result.

## Persistence and schema

- SaveGame schema v6 contains the operation-ledger entry array.
- v4 saves migrate through v5 to v6. Missing transfer receipts and operation history are initialized as empty arrays; no historical records are invented.
- A legacy save without a receipt/history record cannot return that historical result after reload. Revision/owner guards must still reject stale resource transfers instead of applying a second transfer.
- Unknown schema versions are rejected rather than partially applied.
- The ledger still has its configured entry cap; it does not silently evict completed records.

## Limits / remaining integration

- The ledger does not itself roll back mutations or coordinate multiple domains.
- Gameplay operations must explicitly adopt this protocol; the presence of the subsystem does not make unrelated APIs idempotent.
- A source spot-check of `LogBuildPiece.cpp`, `InventoryCraftingComponent.cpp`, `StoneBuildingPiece.cpp`, `StickPrimitivePiece.cpp`, `WorldTerrainStateSubsystem.cpp` and `WorldStructuralGraphSubsystem.cpp` found no direct calls to `TryBeginOperation` / `CompleteOperation`. The next task is to map real domain call sites and connect operations selectively, without creating a competing authority beside existing domain-owned receipts.
- Physical actors/transforms, the structural graph and terrain geometry/collision still need full world-persistence integration.
- No Unreal build/UHT/Editor/PIE/gameplay tests were run.
