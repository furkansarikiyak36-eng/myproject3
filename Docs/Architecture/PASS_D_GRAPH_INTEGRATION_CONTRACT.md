# Pass D — Structural Graph Integration Contract

## Scope
Wire the shared world structural graph into the existing log, stone, and stick physical-piece actors without replacing their domain-owned mesh, anchor, carry, or processing state.

## Implemented
- Each log, stone, and stick actor owns a `UWorldObjectIdentityComponent` subobject and ensures an instance GUID before graph registration.
- `BeginPlay` registers the actor with the world-local graph using a domain node kind and explicit initial support state.
- Successful log snap, stone-anchor, and stick-anchor attachment attempts register the same parent/child relation in the graph after Unreal actor attachment succeeds.
- If graph connection validation fails, the domain connection rolls back its actor attachment and anchor reservations.
- Detachment asks the graph to disconnect the child node.
- Ground placement updates the graph's ground-support flag for stone and stick pieces; log placement updates it for unattached roots.
- Actor end-play detaches child links and unregisters its graph node to avoid stale weak-actor graph entries.

## Authority boundaries
- Domain actors remain authoritative for mesh geometry, authored anchor compatibility, held/placed state, and local anchor occupancy.
- The graph is authoritative for the cross-domain single-parent support/load relation and support-path checks.
- The current graph is single-parent. Multi-point structural constraints and multi-parent support are not modeled.
- Load and damage values are conservative defaults (local load 1, capacity 100000, damage threshold 100); project tuning must replace these values with authored data before gameplay balancing.
- Damage threshold crossing is reported by the graph; automatic collapse, debris spawning, salvage, and child fall simulation are not implemented here.
- This does not add persistent graph serialization or a save/load coordinator.

## Integration requirements for later passes
1. Replace default load/capacity/damage values with data-driven per-piece profiles.
2. Define a deliberate policy for removing a support node with dependents (block, cascade-detach, or collapse transaction).
3. Add multi-parent/brace support only if the intended mechanics require it; do not imply it is supported by this single-parent contract.
4. Add persistence/version migration for node IDs, parent edges, anchor reservations, damage, and support flags.
5. Run Unreal Header Tool, compile, and targeted PIE tests at the final validation gate. No Unreal build or gameplay test was run for this source pass.
