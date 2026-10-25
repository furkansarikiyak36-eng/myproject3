# Pass F — World Simulation Coordination Contract

## Purpose
Connect the deterministic simulation clock from Pass A to the raid/threat director from Pass E through one explicit world-local orchestration entry point.

## Added
- `UWorldSimulationCoordinatorSubsystem`
- Explicit bootstrap: initialize the shared logical clock, then initialize the threat director.
- Explicit fixed-step call: advance logical time, then evaluate the threat director once.
- A coordinator step counter and readiness query.

## Integration rules
- The coordinator does not tick itself. The owning game/world bootstrap must call `InitializeWorldSimulation`, and a fixed-step driver must call `AdvanceFixedStep`.
- `DeltaSeconds` must be a configured fixed step, not render-frame delta.
- Do not independently advance the clock and threat director in parallel with this coordinator; that would create duplicate or order-dependent simulation steps.
- The logistics route graph is authored/queryable data and does not need a time step until resource movement/production adapters are implemented.
- Committed-event publication remains a post-commit notification path; this coordinator does not make domain mutations atomic.

## Known limitation requiring a later pass
The threat director has no non-mutating preflight/prepare API. If the clock advances successfully and `AdvanceDirector` then fails, the call returns false but logical time may already have advanced. Callers must not interpret false as a full rollback. The final integration pass must introduce prepare/commit semantics or snapshot-backed rollback before treating this path as transaction-safe.

## Not included
- No automatic ticking, AI spawning, damage application, resource movement, production queue, survival needs, or terrain simulation.
- No durable save coordinator or restore ordering.
- No UHT/UBT, Editor, PIE, or gameplay validation was performed.
