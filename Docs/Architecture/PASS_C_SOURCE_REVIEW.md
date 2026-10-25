# Pass C — Source Review

## Source changes

Added `UWorldStructuralGraphSubsystem` with:
- registration and validation for structure nodes;
- parent-child edge management;
- cycle rejection and ground-supported path checks;
- aggregate-load calculation and max-load gate;
- safe-removal query;
- damage threshold tracking;
- per-node revision increments.

## Known limitations / review points

- Not compiled with Unreal Build Tool or Unreal Header Tool.
- No domain actor currently registers itself automatically in this subsystem.
- The graph is not persisted and has no multi-parent edges.
- `UnregisterNode(..., false)` may leave dependents with a parent ID that no longer resolves; callers must use it only as an explicit teardown path and resolve descendants first. Future API should replace this boolean escape hatch with an explicit subtree-removal policy.
- Load propagation uses recursive traversal over the current node map; complexity and cycle edge cases require the final test gate.
- No automatic collapse/salvage action is triggered by the damage threshold.
- No mechanic registry status is upgraded.

## Verification boundary

Performed: source inspection and source edits.

Not performed: Unreal build/UHT, editor startup, PIE, physics validation, or gameplay tests.
