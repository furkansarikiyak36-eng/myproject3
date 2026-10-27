# Phase 00–01 Acceptance Checklist

This checklist marks planning/review artefacts only. It does not claim that the gameplay code has compiled or run.

## Phase 00 — Baseline

- [x] Correct repository and main commit recorded.
- [x] Existing phase branches found; chain order and main divergence recorded.
- [x] Canonical M001–M119 registry found; 119 unique IDs checked.
- [x] Historical 119-pass provenance caveat recorded.
- [x] Current source evidence distinguished from staged branch source.
- [x] Unreal build/PIE validation limitation recorded.
- [ ] All changed-source diffs reviewed across phase 01–06 and the integrated-world branch.
- [ ] Select one candidate integration head and describe merge/cherry-pick/rebase strategy.
- [ ] Resolve current main ancestry differences on a working integration branch.
- [ ] Run Unreal 5.8 compile/UHT and capture logs.
- [ ] Run the current log carry/snap bounded PIE scenario; capture valid/invalid placement evidence.

## Phase 01 — Shared contracts

- [x] Definition identity is separated from runtime instance identity in design.
- [x] Request/result/event ownership rules are documented.
- [x] Atomicity and failed-operation no-mutation rule is documented.
- [x] Item, resource, recipe, construction, terrain and event ID namespaces are proposed.
- [x] Persistence responsibility and restore-order requirements are documented.
- [x] Main vs stacked branch evidence is attached to the traceability matrix.
- [ ] Confirm all contract fields against the existing `GameplayCoreTypes.*` implementation.
- [ ] Review duplicate/retry behavior, stale revisions and output identity on split/transform operations.
- [ ] Review Blueprint/public API compatibility and all state-transition callers.
- [ ] Validate C++/UHT compile on one selected combined branch.
- [ ] Validate save/load and error-path invariants in Unreal when persistent implementation is integrated.

## Status decision

- Phase 00: **Documentation audit produced; integration-baseline and UE validation remain open.**
- Phase 01: **Contract design produced; source/API reconciliation and UE validation remain open.**

A checkbox is only checked when a cited file/review result supports it. Code presence is not a runtime test.
