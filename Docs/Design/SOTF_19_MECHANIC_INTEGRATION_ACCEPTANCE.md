# SOTF-Inspired 19-Mechanic Integration Acceptance

Scope: mechanics 1–19 from the project's SOTF reference checklist. Mechanic 20 (helper/Kelvin AI) is explicitly out of scope.

This document defines acceptance gates, not a claim that the Unreal 5.8 build or PIE session has passed. Source presence is not gameplay completion.

## Non-negotiable physical-piece contract

Every physical wood piece must have:
- A stable unique identity and exactly one canonical owner/state: World, Held, Structure, Container, Transport, Consumed.
- Independent length fraction and cross-section profile; legacy PieceType is compatibility metadata, not the sole geometry truth.
- A configured mesh variant with verified local axis, pivot, center offset, collision, mass and bounds.
- A valid state transition for each operation; invalid operations must not mutate mesh, transform, ownership, anchors, graph edges, or item count.
- An idempotent operation ID for split/consume/transfer operations so retries cannot duplicate output.
- A persistence record sufficient to reconstruct the runtime actor and its structural/ownership relationships.

## Dependency order

1. Geometry catalogue: define valid (length, cross-section, operation) combinations and fail closed when a mesh/output is not configured.
2. Atomic processing: output actors, identities, mesh geometry, ownership ledger and parent removal/transition must commit together or roll back together.
3. Contextual interaction: target, tool, hit location, progress and operation validation select one supported action; preview and committed transform must agree.
4. Carry/physics: held, dropped, thrown, pushed/rolled and placed transitions share ownership authority.
5. Snap/structure: custom anchors, occupied reservations, cross-material supports and safe removal use the canonical structural graph.
6. Plans: plan slots consume/attach only valid physical pieces and use the same placement validator as free building.
7. Containers and transport: storage, sled and zipline transfer the same identity rather than making visual copies.
8. World integration: C-slice preplaced cabin, bridge, stairs and spike wall register every piece and support edge using the same graph APIs as player-built structures.
9. Persistence: restore actors, geometry, ownership, containers, held state policy and graph links without duplicate actors or stale reservations.
10. Survival consequences: fuel consumption, shelter/heat, damage/repair and defensive interactions respond to the same world state.

## Acceptance tests (required before marking complete)

### A. Cut geometry and atomic output
- Full 600 cm round log cut at midpoint yields exactly two independent 300 cm outputs (or the configured scale-equivalent dimensions).
- A longitudinal split preserves length and creates the configured distinct cross-section; outputs do not overlap and have correct local orientation.
- Quartering is permitted only from an eligible split-half state; gutter/trough/plank are permitted only from their defined prerequisite states.
- A spike can only be produced/committed in its supported grounded state.
- Missing mesh/offset/length-cut configuration rejects the operation without changing the source actor or ledger.
- Repeating the same operation ID does not duplicate output; a failed operation leaves the source and all outputs/ownership/anchors unchanged.
- Cutting progress persists; the wrong action cannot hijack active progress without an explicit cancel/reset rule.

### B. Carry, drop, throw and placement
- Pickup is rejected when the piece has dependents or its owner is not World.
- Held item has one authoritative owner and is not simultaneously collidable as a world piece.
- Drop restores a valid world owner and collision/physics.
- Preview and committed placement transforms match within configured tolerance.
- A failed snap releases provisional reservations and leaves both pieces/graph unchanged.
- Push/roll preserves identity and updates the physical transform without teleporting through blocking collision.

### C. Structure and plan
- Every placed piece has a structural node and all real supports/anchor links are represented.
- A structure spawned by the C-slice setup and an identical player-built structure produce equivalent graph relationships.
- Removing the only required support is rejected or invokes the explicitly selected collapse rule; it must not silently leave unsupported dependents.
- Free placement and plan-slot filling share validation for geometry, material, transform, support and ownership.
- Stone/stick/wood cross-material anchors are restored and released symmetrically.

### D. Storage and transport
- A piece entering a container ceases to be a world actor and exists exactly once in container ownership.
- Full/incompatible containers reject without consuming or moving the piece.
- Sled and zipline transfer ownership at the start/end of transport and never expose a duplicate interactable actor.
- Consuming fuel removes only the consumed piece from the authoritative ledger.

### E. Save/restore and C slice
- Save/load restores piece IDs, length/profile, mesh selection, transform, physics state, ownership, container membership, structural edges and anchor reservations.
- Runtime-created output actors are recreated when absent; load must not assume all saved actors already exist in the level.
- Held/carry state is either fully restored or explicitly normalized to a safe world drop; it must not silently disappear.
- C-slice cabin, bridge, stairs and spike wall have complete graph registration and deterministic initial ownership.
- Repeat save/load twice; no duplicate actors, stale anchors, missing graph nodes or changed piece counts.

## 19 mechanic families in scope

1 tree yield; 2 carry/drop/place; 3 length cutting; 4 longitudinal splitting; 5 quarter/hewn/groove/trough/plank processing; 6 grounded spike; 7 free construction; 8 floors/roofs; 9 stairs/ramps; 10 beams/supports; 11 doors/windows; 12 defensive walls/gates; 13 bridges/rope crossings; 14 towers/platforms/tree structures; 15 storage; 16 log sled; 17 zipline; 18 firewood/fuel; 19 thrown-log physics and impact.

Helper/Kelvin-style autonomous tasking (20) is intentionally excluded.

## Completion policy

A family is:
- **Present in source** when relevant code exists.
- **Integrated** when it uses the shared identity/geometry/ownership/graph contracts.
- **Accepted** only after its tests pass in Unreal 5.8, including relevant PIE interactions.

Do not mark a family accepted based only on static code inspection, unit-like source checks, or a generated map script. Record exact build/PIE results and any unverified asset-side mesh configuration.
