# Pass B — Source Review

## Change in this pass

`ULogCarryComponent` now coordinates pickup, drop and placement with `UWorldObjectIdentityComponent` when the optional component is present.

### Pickup

- Rejects lifecycle states other than `World` and `Placed`.
- Requests a revision-incrementing transition to `Held`.
- Only then updates the log carry/placed flags.

### Drop

- Requests `Held -> World`.
- If the identity component rejects the transition, the carry operation returns without clearing the held reference.

### Placement

- Free placement requests `Held -> Placed` before changing domain flags.
- Snap placement attempts the transition after the anchor operation succeeds. On lifecycle rejection it detaches the new parent relationship and restores the previous actor transform.

## Risks requiring later engine/source gate

- This change is not compiled; Unreal Header Tool, C++ compatibility and runtime behavior remain unverified.
- The identity component may be absent on existing log Blueprints. Such actors retain legacy behavior and do not gain shared lifecycle guarantees automatically.
- Actor startup does not yet reconcile `bStartsPlaced` with an identity component's authored `LifecycleState`.
- The log cut/mesh transformation still has no durable operation deduplication and does not yet update a canonical item-definition key.
- Tree felling, resource yields, split outputs, stone/stick adapters and inventory transfer remain outside this pass.
- Snap cleanup relies on the existing `DetachFromSnapParent` cleanup path; anchor reservation cleanup must be validated in the final engine gate.

## Verification boundary

Performed: source inspection and targeted source edits only.

Not performed: Unreal build, UHT, editor startup, PIE, physics/collision validation, save/load, or gameplay tests.

No mechanic registry status is upgraded by this pass.
