# Pass D — Source Review

## Changed
- `LogBuildPiece.h/.cpp`
- `StoneBuildingPiece.h/.cpp`
- `StickPrimitivePiece.h/.cpp`
- `WorldStructuralGraphSubsystem.h/.cpp`

## Static review checklist
- [x] Shared identity component is attached to each supported physical-piece actor type.
- [x] Graph registration uses the identity component's instance GUID.
- [x] Graph edge creation follows successful actor attachment.
- [x] Failed graph edge creation releases the attempted anchor reservations and rolls back attachment bookkeeping.
- [x] Placement state can update graph ground support.
- [x] Detach requests clear the graph parent edge.
- [x] EndPlay removes child relations and unregisters the actor node.
- [ ] Unreal Header Tool / UBT compile.
- [ ] Runtime tests for cross-domain attachment, capacity rejection, dynamic root placement, detachment, and EndPlay cleanup.
- [ ] Validate behavior when actors already contain legacy identity components in Blueprint subclasses.
- [ ] Tune authored loads/capacities/damage thresholds and choose collapse policy.

## Known limitations / risks
- Graph connections only occur when both pieces have registered graph nodes. The actor classes now create identity subobjects, but existing Blueprint subclasses should be checked for duplicate legacy identity components.
- Ground placement is currently explicit and assumes the caller has already validated the surface. These methods do not perform collision traces or slope/material checks.
- Graph support remains single-parent; a child cannot distribute load across multiple anchors.
- Damage is tracked in the graph but does not yet trigger an authoritative collapse transaction.
- This review is source-level only. It is not evidence of successful Unreal compilation, UHT, editor startup, or PIE/gameplay validation.
