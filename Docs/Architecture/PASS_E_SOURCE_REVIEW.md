# Pass E — Source Review

## Added
- `WorldThreatDirectorSubsystem.h/.cpp`: deterministic bounded raid schedule/state machine.
- `WorldLogisticsRouteSubsystem.h/.cpp`: bounded authored route graph and weighted shortest-path query.
- Threat and logistics contract documents.

## Static review
- [x] Threat director refuses initialization without initialized world simulation clock.
- [x] Threat timing is based on logical simulation time, not render delta.
- [x] Raid pressure and placeholder target are derived from deterministic seeded streams.
- [x] Raid history is bounded by configured maximum.
- [x] Route graph rejects invalid IDs, missing endpoints, self-edges, non-finite/negative costs, and configured capacity overflow.
- [x] Disabled nodes and edges are respected by route queries.
- [x] Node removal removes incident route edges.
- [x] Route query fails closed when no route exists.
- [ ] UHT/UBT compilation and Unreal runtime tests.
- [ ] AI spawn/target adapter and structural-damage integration.
- [ ] Resource movement adapter with inventory ownership transactions.
- [ ] Save coordinator snapshot/restore for director and route graph.

## Boundaries
Neither subsystem autonomously ticks. The simulation driver must call the director at fixed steps. Route graph models authored paths, not voxel-level terrain. Threat state does not spawn enemies or apply damage. This is source-level work only; no Unreal validation was performed.
