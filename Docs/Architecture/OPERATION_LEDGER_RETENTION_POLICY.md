# World operation ledger retention policy

## Decision

The world operation ledger has **no generic time-to-live**. An operation ID represents an idempotency key, so deleting its result solely because it is old could allow a delayed retry to repeat a mutation. IDs are not recycled by the ledger.

A completed row may be forgotten only after the owning domain has committed a separately persisted receipt or monotonic watermark that is sufficient to answer a retry without replaying the mutation. The ForgetCompletedOperationWithExternalReceipt API is deliberately explicit and must only be called after that durable domain state is committed. The raid adapter follows this order by committing the raid pulse watermark before forgetting the pulse row.

Pending rows are never eligible for eviction. The save coordinator refuses to capture while a pending operation exists. If the bounded ledger reaches MaxEntries, a new operation is rejected and an error is logged; it does not evict an unreceipted completed result. This is a safety-over-availability policy, not a claim that operation history can grow without bound.

## Remaining domain work

Any additional operation family that needs indefinite world uptime must identify and implement its own durable idempotency receipt/watermark before it is allowed to prune ledger rows. A TTL may be introduced only when the owning caller has an explicit, enforceable retry horizon and the expired ID can never be replayed beyond that horizon. Do not add generic oldest-first eviction.

## Validation boundary

The policy is defined in source and documented. Runtime saturation, save/load continuity and retry-after-restore still require integration validation in Unreal; no such test is implied by this document.
