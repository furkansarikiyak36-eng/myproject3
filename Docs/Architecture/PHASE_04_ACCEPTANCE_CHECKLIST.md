# Phase 4 Acceptance Checklist — Stone Masonry

## Implemented at source level
- [x] Add typed stone piece categories and explicit anchor data.
- [x] Reject missing/duplicate anchor IDs, missing compatibility tags and invalid anchor transforms at connection validation.
- [x] Require held, loose state before anchor connection or ground placement.
- [x] Require a placed/supportable target and valid support ancestry.
- [x] Reject self-links, occupied anchors, incompatible tags and disallowed anchor directions.
- [x] Restore transform if setting the alignment transform or actor attachment fails.
- [x] Release both anchor reservations when a connection is removed.
- [x] Avoid mutating the child array while iterating it during end-play cleanup.
- [x] Return disconnected pieces to loose physics.
- [x] Keep the stone owner independent from log-specific processing code.

## Still pending
- [ ] Audit all tracked source callers and local/untracked Blueprint consumers.
- [ ] Validate anchor transform composition and lifecycle in Unreal Editor/PIE.
- [ ] Add stone shaping recipes and source-specific output mesh mapping.
- [ ] Add stone-wall/corner/column/opening placement helpers.
- [ ] Integrate with the shared structural graph and damage/collapse system.
- [ ] Integrate inventory, bulk hauling, mixed wood-stone adapters and persistence.
- [ ] Unreal Engine 5.8 UHT/build, Editor/PIE and runtime tests.
- [ ] Final integrated evidence for M029–M038 and all 119 mechanics.

Checked boxes represent source-level implementation/review only, not runtime-verified acceptance.
