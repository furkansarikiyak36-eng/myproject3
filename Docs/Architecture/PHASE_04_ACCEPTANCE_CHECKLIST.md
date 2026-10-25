# Phase 4 Acceptance Checklist — Stone Masonry

## Implemented at source level
- [x] Add typed stone piece categories and explicit anchor data.
- [x] Reject missing/duplicate anchor IDs and missing compatibility tags.
- [x] Require held, loose state before anchor connection or ground placement.
- [x] Require a placed/supportable target and a valid support ancestry chain.
- [x] Reject self-links, occupied anchors, incompatible tags and disallowed anchor directions.
- [x] Restore the previous transform if actor attachment fails.
- [x] Release both anchor reservations when a connection is removed.
- [x] Keep the new stone owner independent from log-specific processing code.

## Still pending
- [ ] Audit end-play cleanup and support-removal behavior against real world actor lifecycles.
- [ ] Verify target anchor occupancy cannot become stale across destruction/replacement.
- [ ] Add stone shaping recipes and mesh-specific output mapping.
- [ ] Add stone-wall/corner/column/opening placement helpers.
- [ ] Integrate with the future shared structural graph and damage/collapse system.
- [ ] Integrate inventory, bulk hauling, mixed wood-stone adapters and persistence.
- [ ] Unreal Engine 5.8 UHT/build, Editor/PIE and runtime tests.
- [ ] Final integrated evidence for M029–M038 and all 119 mechanics.

Checked boxes represent source edits only, not runtime-verified acceptance.
