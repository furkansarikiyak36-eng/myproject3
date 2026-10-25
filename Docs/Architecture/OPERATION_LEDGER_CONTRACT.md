# Operation Ledger — Source Contract

**Status:** Initial in-memory implementation; not a transaction coordinator or durable ledger.

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
- Pending operations do not expose a final result.

## Limits

- The ledger is world-local, transient and not persisted. A world restart resets it.
- It does not roll back mutations, coordinate multiple domains, or recover an interrupted in-flight operation.
- All domain operations still need to adopt this protocol; the presence of the subsystem does not make existing APIs idempotent.
- Phase 14 must include ledger snapshot/restore or reconcile it with durable operation history before claiming crash/reload idempotency.
- No Unreal build/UHT/Editor/PIE/gameplay tests were run.
