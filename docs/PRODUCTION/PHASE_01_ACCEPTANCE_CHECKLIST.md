# Phase 00/01 Working Checklist

This records inspected evidence and remaining gates. It intentionally does not mark the 119 mechanics as passed.

## Phase 00 — Baseline
- [x] Correct repository identified: furkansarikiyak36-eng/myproject3.
- [x] Current main baseline recorded: 65faad8602d4e14e894dc2cf4e3d507ca39cbb1d.
- [x] Main README and key log source headers inspected.
- [x] Checked-path presence/absence recorded for core contracts, inventory, stone, stick, terrain, agriculture, threat and save candidate files.
- [x] Relevant stacked branches compared.
- [x] Canonical M001–M119 registry copied without changing unreviewed/not_run statuses.
- [x] Traceability sheet generated with 119 unique IDs.
- [ ] Full source diff review of feature/simple-intent and Phase 01–06 stack.
- [ ] Source mapping for every M001–M119 acceptance criterion.
- [ ] Local/untracked Blueprint and content audit from user's Windows checkout.
- [ ] Selective integration strategy recorded.
- [ ] UE 5.8 build/Editor/PIE evidence: not run in this remote documentation pass.

## Phase 01 — Shared data contracts
- [x] Shared contract draft prepared.
- [x] Reuse candidates from Phase 02/06 and integrated-world branch documented.
- [x] JSON Schemas drafted for item, material, recipe, instance, operation and feedback event records.
- [ ] Validate schemas and example records with a JSON Schema validator.
- [ ] Reconcile schema fields against candidate C++ types and current callers.
- [ ] Finalize authoritative units and conversions.
- [ ] Confirm persistent instance-ID policy and save migration versions.
- [ ] Confirm event idempotency/persistence responsibilities without duplicate ownership.
- [ ] UE 5.8 UHT/build and editor/runtime evidence: not run.

## Status rule
A checked documentation item means the document or repository observation exists. It does not mean the related gameplay mechanic is implemented, compiled or runtime-verified.
