# Pass C — Shared Structural Graph Contract (current source-aligned)

## Objective

A world-local graph represents structural relationships across wood, stone, sticks, foundations and terrain anchors. Domain actors remain authoritative for mesh geometry and their local connection-feature definitions; the graph owns committed support relations and aggregate-load checks.

## Current implemented API surface

- Stable-node registration, actor identity and domain kind.
- Primary parent relation with the two endpoint anchor IDs.
- Additional load-bearing contacts in `SecondarySupports`, added/removed through `TryAddSecondarySupport` and `TryRemoveSecondarySupport`.
- Cycle checks, support-path queries, dependent queries, leaf-first removal order, load aggregation, damage/repair, revision increments and snapshot export/restore.
- Log actor-local attachment reconstruction from canonical edges.
- A secondary support is an actual graph edge, not just a note attached to a mesh.

## Limits that remain real

- The graph is not a full structural mechanics solver. Loads are scalar configured values; sharing is simplified and does not model bending moments, stress, material fatigue or dynamic fracture.
- The graph does not own or solve geometric contact. Placement adapters must supply valid contact geometry and reserve domain anchors before committing edges.
- Secondary-support API presence does not make a placement atomic. Callers must check every return value and roll back the complete transaction on failure.
- Automatic collapse, debris/salvage production and general physical-world persistence remain separate integration tasks.
- Graph snapshots store node/edge state, but a whole-world persistence round trip still requires physical actor, transform, ownership and terrain restoration in the correct order.

## Connection invariants

1. Every graph mutation must validate node existence, identity, support path, anchor usage, cycle constraints, capacity and revision limits before publishing success.
2. A two-support beam must have one primary edge and one distinct secondary support edge after the same placement transaction.
3. If adding the secondary edge fails, callers must not keep the primary edge as a successful two-support placement.
4. Ground rest must not create a structural edge unless the selected operation profile explicitly defines that support relation.
5. Removing or replacing a support must remove or re-evaluate every dependent edge without leaving an orphaned anchor reservation.
6. Failed graph restore/reconstruction is a world consistency failure; do not silently continue simulation as if rollback succeeded.

## Required integration sequence

1. Make profile-backed placement the only authority for new placement requests.
2. Bridge log, stone and stick feature providers to stable IDs rather than array indices.
3. Route multi-anchor placement through a transaction journal and check every graph API result.
4. Define support-removal policies and explicit collapse transactions.
5. Extend durable world save/load to restore node IDs, all primary/secondary edges, actor-local anchors and physical transforms.
6. Keep runtime claims separate from source-level implementation evidence; no Unreal build or PIE success is implied by this contract.
