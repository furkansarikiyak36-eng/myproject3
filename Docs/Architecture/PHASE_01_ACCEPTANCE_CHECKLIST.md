# Phase 1 Acceptance Checklist

This checklist distinguishes Phase 1 foundation/scope closure from the final end-of-phases Unreal validation. A checked design item is not a claim that gameplay is implemented or tested.

## Repository and compatibility
- [x] Confirm the working branch and baseline commit (`phase-01-foundation-v2`, based on `main` at `ff2fd51444463afad598b837e820dbc3df2d439b`).
- [x] Inventory tracked source, build/configuration and editor-script files and dependencies in `PHASE_01_SOURCE_INVENTORY.md`; local-only assets and Blueprint consumers remain outside this repository evidence.
- [x] Map tracked native state-transition and editor-script callers (`PHASE_01_STATE_TRANSITION_CALLER_MAP.md`); local/untracked Blueprint references remain unknown.
- [x] Document current placed/held/startup-default semantics and compatibility gate (`PHASE_01_PLACEMENT_HELD_STATE_CONTRACT.md`); no risky API refactor made before Blueprint audit.
- [x] Guard custom snap-point IDs against None, reserved automatic prefixes, and duplicates in source; Unreal/Blueprint behavior remains unverified.
- [x] Make snap acceptance independently reject occupied or non-receiving target anchors.
- [x] Reject direct snap reparenting, support-with-children attachment, and target-chain cycles (including malformed existing cycles) in source.
- [x] Make snap reservation conditional on successful actor attachment; parent/child bookkeeping and anchor occupancy commit only after the engine attachment succeeds.
- [x] Identify the current tracked log-processing/construction owner (`ALogBuildPiece`) and player-facing interaction owner (`ULogCarryComponent`).
- [x] Record the tracked test/build/editor-script scope and its limits; the historical “119 passed” count is not treated as a list of 119 named mechanics.
- [x] Record legacy/parallel-system policy: no class is deleted or activated as a new authority without call-site/ownership evidence. Local/untracked Blueprint activation remains outside this audit.

## Shared data and operation contracts
- [x] Define stable mechanic IDs and the distinction between definition IDs and instance IDs in the architecture contract; implementation-specific object-ID types remain owned by their domain phase.
- [x] Define explicit non-negative resource quantities and units as a shared rule; per-resource units are assigned with each domain's data.
- [x] Keep domain-specific processing states separate from generic lifecycle/installation states.
- [x] Define stable operation result categories and all-or-nothing failure semantics.
- [x] Define committed-event ownership and prohibit success events before state commit.
- [x] Define persistence boundary, versioning and safe migration/failure behavior.

## Authoritative 119-mechanic scope
- [x] Establish a canonical, explicitly numbered scope of exactly 119 mechanics in `MECHANIC_REGISTRY_119.json`.
- [x] Provide a readable registry in `MECHANIC_REGISTRY_119.md` and a machine-readable CSV export.
- [x] Assign each mechanic a stable ID (`M001`–`M119`) and one intended primary owner system.
- [x] Record dependencies/prerequisites, failure behavior, persistence expectation and an acceptance criterion for every entry.
- [x] Set all entries to `unreviewed`; registry inclusion does not claim implementation or test success.
- [x] Keep the larger source catalog and historical 119-check report as provenance, not as a falsely reconstructed exact list.

## Phase 1 source review
- [x] Complete the focused static review of the tracked log attachment/placement/physics path and record the open risks.
- [x] Guard the explicit failed-snap → free-placement fallback in source.
- [x] Verify registry integrity: 119 unique IDs, IDs M001–M119, dependency references valid, 119 Markdown rows and 119 CSV records.
- [x] Separate source-level checks from automated tests, Unreal build, and editor/runtime evidence.

## Deferred to the planned end-of-phases validation gate
- [ ] Map each registry item to final C++/Blueprint implementation call sites and attach evidence as its owning phase is integrated.
- [ ] Run automated tests against the final integrated code and record exact scope/results.
- [ ] Run Unreal Engine 5.8 build and capture the result.
- [ ] Run editor/runtime scenarios and the integrated demo; test all 119 mechanics in appropriate locations without clutter.
- [ ] Verify save/load, regression, failure paths and cross-system resource/state conservation in the final integrated build.

## Required invariants
- A groove cannot be applied until the required log-processing prerequisite is satisfied.
- A spike/trap cannot become operational until placement, grounding and trigger conditions pass.
- A failed operation leaves authoritative object/world state and material quantities unchanged.
- A successful operation commits state before publishing a success event and marks durable state for persistence when required.
- A loaded object retains stable identity and durable domain state.

## Phase 1 closeout decision
**Phase 1 foundation and scope definition are closed at the design/source-contract level.** The remaining unchecked items are integration/build/runtime validation gates deliberately retained for the end-of-phases test cycle; they are not represented as passed.
