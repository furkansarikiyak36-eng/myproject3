# Phase 14 Acceptance Checklist

## A. Integration contracts
- [ ] Identify the actual authoritative world clock and seed owner; remove or adapt subsystem-local clocks.
- [ ] Document exact tick order and prove survival is applied once per simulation interval.
- [ ] Map every operation to one authoritative state owner and stable operation ID.
- [ ] Verify failure atomicity for inventory transfer, production, construction, terrain edits and restore.
- [ ] Specify committed-event timing; failed or staged operations emit no committed event.

## B. Save/load participant protocol
- [ ] Every section has a stable ID and explicit schema version.
- [ ] Export is deterministic for equivalent authoritative state.
- [ ] Import validation has no side effects.
- [ ] Cross-section IDs/references validate before any owner commits.
- [ ] Restore stages every participant before mutation.
- [ ] Any validation/staging failure leaves pre-load world state unchanged.
- [ ] CRC mismatch, duplicate/missing section, unsupported format/schema, truncated payload and oversized payload are rejected.
- [ ] Round-trip compares all authoritative fields, not only save manifest metadata.
- [ ] Migration and recovery are either implemented with tests or explicitly left as release blockers.

## C. 119-mechanic demo coverage
- [ ] Review all 119 rows in `PHASE_14_M001_M119_DEMO_MATRIX.md`.
- [ ] Attach source mapping to each canonical ID; no inference from owner name alone.
- [ ] Check dependencies in valid order and invalid order.
- [ ] Include positive, boundary and negative tests with no partial mutation.
- [ ] For each `save_required` row, verify state equivalence after reload.
- [ ] Provide one compact fixture/demo location and reset procedure per mechanic group.
- [ ] Do not update registry status without reviewed evidence and an explicit decision.

## D. Environmental and survival integration
- [ ] Use the canonical clock for season, weather, food quality, farming, production and raids.
- [ ] Resolve Phase 11 vs Phase 13 survival update ownership.
- [ ] Ensure exposure calculations do not double-apply needs or temperature changes.
- [ ] Weather effects on crops/animals are explicit, bounded and deterministic where required.
- [ ] Visual weather, audio, lightning and disasters are separately tracked as unimplemented until built.

## E. Unreal verification gate (deferred)
- [ ] UE 5.8 UHT/build passes on the actual target branch.
- [ ] Editor opens the integration map without missing-class or asset errors.
- [ ] PIE scenarios pass for each demo station and cross-system flow.
- [ ] Save/load, replay determinism and failure rollback pass in UE runtime.
- [ ] Evidence bundle records exact commit, seed, engine version, test scope and logs.

**Acceptance status at branch creation:** documentation and coverage planning only. Unreal build/editor/PIE/runtime checks have not been run. Phase 14 is not complete until its implementation work and evidence gates are satisfied.
