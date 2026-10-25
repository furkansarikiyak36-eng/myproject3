# Pass E — Logistics Route Graph Contract

## Implemented
- Bounded world-local authored route graph with nodes and directed weighted edges.
- Node/edge enable switches support blocked paths without deleting authored route data.
- Edge insertion requires existing endpoints, finite non-negative costs, and rejects self-edges.
- Dijkstra path query returns ordered node IDs and total cost; unreachable/disabled endpoints fail closed.
- Node removal cleans all incident edges.

## Deliberate boundary
This is route planning only. It does not move carts, zipline payloads, or inventory stacks, and it does not derive edge costs from terrain, weather, threat, or stamina. A logistics movement adapter must consume route results and perform movement/resource ownership transactions.

## Limits and validation
Default limits are 512 nodes and 2048 edges. Algorithm is O(V^2 + E) and intended for a small authored route network, not a whole-world voxel graph. No Unreal compile/UHT/PIE tests were run.
