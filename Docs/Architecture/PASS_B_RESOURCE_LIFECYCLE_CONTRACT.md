# Pass B — Physical Resource Lifecycle Contract

## Goal

Make the existing physical log carry/place flow respect the shared object lifecycle when an actor has a `UWorldObjectIdentityComponent`, without forcing identity components onto legacy Blueprint actors.

## Lifecycle contract

- Loose resource: `World`
- Picked up by the player: `World|Placed -> Held`
- Dropped without construction: `Held -> World`
- Successfully placed as a construction piece: `Held -> Placed`
- A lifecycle transition rejection must not be silently ignored when the optional identity component is present.
- Actors without the optional identity component keep the existing domain-only behavior for backward compatibility.

## This pass implements

- Pickup now asks the identity component to transition from its current `World` or `Placed` state to `Held` before changing the log's domain state.
- Drop now requires `Held -> World` before releasing the carry owner.
- Free placement requires `Held -> Placed` before committing the log's placed state.
- Snap placement attempts the lifecycle transition after anchor reservation; if it is rejected, the new snap parent connection is detached and the previous transform restored.
- The log carry path includes the identity component directly; no separate duplicated lifecycle store is introduced.

## Explicitly not complete

- Tree felling and deterministic log spawning.
- Splitting one tree/log into multiple independently identified resource actors.
- A single inventory/world ownership transaction; physical logs are not yet converted into inventory stacks.
- Stone and stick lifecycle integration.
- Durable operation deduplication and save/load of carry/attachment relationships.
- Reconciliation of lifecycle state when authored actor defaults conflict with runtime domain state.
- Tool costs/durability, harvesting feedback, and resource yield tables.

## Integration rules

1. Domain state remains authoritative for physical geometry and construction-specific conditions.
2. The identity component provides shared lifecycle state and revision, not a second copy of geometry or construction state.
3. A rejected lifecycle transition must not leave a newly created structural attachment behind.
4. Optional identity components are compatibility adapters, not proof that all resource actors are fully integrated.
5. Registry statuses remain unchanged until the final verification gate. Source review is not an Unreal compile or gameplay test.
