# Pass D — Structural Graph Integration Contract (updated)

## Scope

Synchronize log, stone and stick actor-level attachment state with the canonical world structural graph while preserving each domain's mesh geometry, processing state and authored feature definitions.

## Current integration responsibilities

- Each physical piece registers a stable structural node after identity/ownership initialization.
- Domain-local attachment APIs validate their own feature tags, occupancy and material-specific constraints.
- Successful same-material or cross-material attachment operations register the matching canonical primary edge.
- Failed graph insertion must undo the domain attachment and restore feature occupancy.
- Graph detach/removal must keep domain parent/child pointers and actor attachments consistent.
- Ground placement updates a node's root support state where appropriate; ground contact is not automatically a socket connection.
- `SecondarySupports` and `TryAddSecondarySupport` represent additional load-bearing contacts. Callers must check the return value and must not report two-support success after a failed secondary edge.

## Authority boundaries

- Domain actor/provider: actual mesh, feature geometry, material compatibility, local occupancy and piece processing state.
- Global Placement Kernel: candidate discovery, transforms, orientation solving, profile constraints and preview selection.
- Placement transaction: reservations, staged actor changes, graph edge commits and rollback.
- Structural graph: durable-in-memory primary/secondary support relations, cycles, support paths, simplified aggregate load and damage state.
- Physics adapter: settling, constraints, hinge behavior and fall/collapse outcomes.

## Required migration

1. Replace duplicate placement solvers gradually, not by deleting all domain APIs at once.
2. Convert persisted actor-local array indices to stable feature IDs when writing a connection.
3. Support two-ended beams using one candidate solution and one transaction.
4. Ensure failed graph/ownership/lifecycle steps leave no partial attachment.
5. Document all graph snapshots and restore limitations. Snapshot API presence alone does not prove physical-world save/load.
6. Unreal UHT/UBT, Editor/PIE and failure-injection validation remain a separate gate; this update makes no such claim.
