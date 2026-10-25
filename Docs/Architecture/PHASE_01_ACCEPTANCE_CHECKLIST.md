# Phase 1 Acceptance Checklist

This checklist distinguishes planning from verified implementation. Do not mark an item complete without evidence.

## Repository and compatibility
- [x] Confirm the working branch and baseline commit (`phase-01-foundation-v2`, based on `main` at `ff2fd51444463afad598b837e820dbc3df2d439b`).
- [ ] Inventory all current source files and build dependencies.
- [x] Map tracked native state-transition and editor-script callers (`PHASE_01_STATE_TRANSITION_CALLER_MAP.md`); local/untracked Blueprint references remain unknown.
- [x] Document current placed/held/startup-default semantics and compatibility gate (`PHASE_01_PLACEMENT_HELD_STATE_CONTRACT.md`); no risky API refactor made before Blueprint audit.
- [x] Guard custom snap-point IDs against None, reserved automatic prefixes, and duplicates in source; Unreal/Blueprint behavior remains unverified.
- [x] Make snap acceptance independently reject occupied or non-receiving target anchors; Unreal runtime coverage remains pending.
- [x] Reject direct snap reparenting, support-with-children attachment, and target-chain cycles (including malformed existing cycles) in source; Unreal runtime coverage remains pending.
- [x] Make snap reservation conditional on successful actor attachment; only commit parent/child bookkeeping and anchor occupancy after the attachment succeeds; Unreal runtime coverage remains pending.
- [x] Identify the authoritative log-processing/construction implementation (`ALogBuildPiece` plus `ULogCarryComponent`; broader Blueprint/editor dependency review remains open).
- [ ] Identify unused/legacy classes and confirm they are not accidentally activated.
- [ ] Record existing mechanic tests and their exact scope.

## Shared data
- [ ] Reconcile stable object IDs and definition IDs with existing project types.
- [ ] Reconcile resource/material units and validation rules.
- [ ] Keep domain-specific processing states separate from generic lifecycle/installation states.
- [ ] Define stable operation result codes.
- [ ] Define event payloads and ownership of state changes.
- [ ] Define persistence schema/versioning and migration policy.

## Mechanics registry
- [ ] Obtain the authoritative list of all 119 mechanics.
- [ ] Assign each mechanic a stable ID and one primary owner.
- [ ] Record prerequisites and invalid-operation behavior.
- [ ] Record dependencies and cross-system state changes.
- [ ] Record save/load requirements.
- [ ] Record code, build and runtime evidence separately.

## Validation
- [ ] Static review completed.
- [x] Guard the explicit failed-snap → free-placement fallback in source; runtime scenario remains pending.
- [x] Complete the focused attachment/placement/physics source sub-review and record open risks (`PHASE_01_ATTACHMENT_PHYSICS_RISK_REGISTER.md`); full repository/Blueprint audit remains open.
- [ ] Automated tests run where available.
- [ ] Unreal Engine 5.8 build run and result captured.
- [ ] Editor/runtime scenarios run and result captured.
- [ ] No claim of runtime success based only on a source inspection or Python test.

## Required invariant examples
- A groove cannot be applied until the required log-processing prerequisite is satisfied.
- A spike/trap cannot become complete until its required placement and grounding conditions are satisfied.
- A failed operation leaves the object and world state unchanged.
- A successful operation updates authoritative state and marks it for persistence when needed.
- A loaded object retains its stable identity and durable domain state.
