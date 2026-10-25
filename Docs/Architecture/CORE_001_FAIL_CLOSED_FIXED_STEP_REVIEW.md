# CORE-001 — Fail-Closed Fixed-Step Rollback Review

**Branch:** `fix/world-simulation-fail-closed`  
**Base:** `landscape-builder-plan`  
**Pull request:** [#18 — Core correctness: fail closed after fixed-step rollback failure](https://github.com/furkansarikiyak36-eng/myproject3/pull/18)  
**Scope:** Guard against continuing deterministic world simulation after a failed cross-subsystem rollback.  
**Evidence level:** Static source review only. UE 5.8 UHT/UBT, Editor, PIE, and fault-injection were not run.

## Source finding

`UWorldSimulationCoordinatorSubsystem::AdvanceFixedStep` snapshots the clock, threat director, agriculture, survival, production, cargo, item ownership, and storage before committing a fixed step. If a participant rejects during commit, the coordinator attempts to restore the snapshots.

Before this change, a failed restore only wrote an error log. `AdvanceFixedStep` returned false, but subsequent calls were not prevented. That allowed a world with potentially mixed subsystem state to keep advancing.

## Change made

- Added a coordinator-owned fault flag and human-readable fault reason.
- When any rollback participant fails to restore, the coordinator sets the fault flag and emits an error that includes the reason.
- `AdvanceFixedStep` rejects further fixed-step requests while the coordinator is faulted.
- `IsWorldSimulationReady` returns false while faulted.
- Blueprint-callable diagnostic queries expose the fault flag and reason for UI/debugging.
- A clean rollback failure (i.e. rollback succeeds) still returns false for that step but does not poison the coordinator, allowing an ordinary retry.

## Recovery policy

A faulted world must be stopped and recovered/reset by its owning flow. This patch deliberately does not attempt an automatic retry or automatic world reload because a partial rollback can leave state that cannot safely be inferred from the coordinator alone.

This is a fail-closed guard, not proof of universal transaction atomicity. All authoritative time-dependent systems must continue to be advanced through the coordinator rather than ticking independently.

## Static acceptance

- [x] Rollback failure sets a durable-for-the-world coordinator fault flag.
- [x] Further coordinator fixed steps are rejected after the fault.
- [x] World simulation readiness becomes false after the fault.
- [x] The fault reason can be inspected from Blueprint.
- [x] Successful rollback still reports the original fixed-step failure without entering the faulted state.
- [ ] UE 5.8 Header Tool / Build Tool compile.
- [ ] Fault-injection case proving that one participant's restore rejection poisons the coordinator.
- [ ] Runtime case proving a faulted coordinator cannot advance time, threat, cargo, production or farming.
- [ ] Final integrated 119-mechanic acceptance run.

## Next dependency

CORE-002 should address item-transfer operation receipts and idempotency across snapshots, rollback, and durable save/load. The ownership subsystem currently exports item records as snapshots but not the in-memory transfer receipt map; its restore path resets receipts. That boundary should be clarified before claiming retry idempotency across restoration.
