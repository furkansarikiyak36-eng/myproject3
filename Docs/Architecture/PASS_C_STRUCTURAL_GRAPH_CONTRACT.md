# Pass C — Shared Structural Graph Contract

## Objective

Introduce a world-local graph contract that can represent wood, stone, stick, foundation and terrain-anchor nodes in one support/load graph. Domain actors remain owners of their mesh geometry and authored anchors.

## Implemented in this first slice

- Node registration with a stable caller-provided GUID, domain kind, actor reference, local load, maximum supported load and damage threshold.
- Explicit parent-child edges carrying both anchor IDs.
- Cycle rejection and support-path validation.
- Recursive aggregate-load calculation and parent capacity check during connection.
- Safe-removal query that refuses a node with direct dependents.
- Damage accumulation with a threshold signal.
- Revision increments for graph mutations.

## Important boundaries

- This subsystem is not yet wired into log, stone or stick actors. Existing domain-local attachment arrays and anchor reservations remain active until adapters migrate in a later slice.
- It does not automatically spawn collapse physics or salvage resources when damage reaches a threshold. It reports the threshold crossing so the owning orchestration layer can resolve it.
- It does not yet validate anchor geometry, compatibility tags or anchor occupancy; those remain the responsibility of the domain adapter until unified.
- The graph is world-local and in-memory. Save/load and migration are not implemented here.
- A single-parent support graph is used in this first slice. Multi-support beams/bridges require an explicit edge model in a later revision.
- Load is a configurable scalar, not a full structural physics solver.

## Required follow-up

1. Add adapters that register existing wood/stone/stick actors and synchronize anchor commits/releases with graph edges.
2. Define safe removal policy: reject, detach dependents in stable order, or resolve collapse.
3. Add multi-parent support for bridge/roof/floor systems.
4. Persist node IDs, edges, revisions and damage state through the save coordinator.
5. Keep mechanic registry statuses unchanged until the final verification gate.
