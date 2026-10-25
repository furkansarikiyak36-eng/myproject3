# Phase 7 — Shared Structure Graph, Support and Damage

**Status:** Skeleton only; no implementation completion claimed  
**Registry scope:** M059–M070  
**Prerequisites:** Phases 1–6 contracts and physical-piece layers  
**Engine validation:** Deferred to final gate

## Responsibility

Provide one shared structural graph for log, stone and stick pieces. Each domain retains its own piece types and processing rules; this phase owns cross-domain connection/support relationships and structural outcomes.

## Proposed source boundaries

- `StructuralGraphSubsystem`: authoritative graph queries and graph-level operations.
- `StructuralNodeComponent`: stable node identity, support classification and local capacity metadata.
- `StructuralConnection` / `StructuralGraphTypes`: connection IDs, endpoint anchors, load/support relation and result codes.
- `StructuralDamageResolver`: damage propagation, support loss and collapse/rejection decisions.
- Adapters on existing log/stone/stick pieces: expose anchors and domain state without duplicating anchor ownership.

These names are design placeholders, not files already present.

## Contracts to define

- A connection requires compatible, available anchors and a successful physical attachment.
- Cycles, duplicate connections, stale identities and invalid endpoints are rejected deterministically.
- Support is evaluated from grounded/foundation nodes through explicit graph edges, not mesh overlap.
- Removing or damaging a node must either recompute dependent support and resolve collapse or reject without partial graph mutation.
- Disconnecting a piece releases reservations on both endpoints and clears parent-child bookkeeping.
- Structural damage has one authoritative owner; a hit must not be applied twice by multiple adapters.
- Repair/salvage outputs must transact with inventory only after a valid state transition.

## Dependencies / integration

Consumes: stable identity, operation result/revision contracts, domain anchors and piece states, inventory transactions.  
Provides: support queries to placement, traps, terrain-adjacent structures, raid damage and demo acceptance.

## Acceptance gates

- [ ] Cross-domain graph represents log–stone–stick links consistently.
- [ ] Duplicate/invalid/cyclic connection attempts do not mutate state.
- [ ] Unsupported structures are rejected or collapse according to configured policy.
- [ ] Removing one support leaves no orphan edge or reserved anchor.
- [ ] Damage, repair and salvage do not duplicate or lose resources on failure.
- [ ] Persistence representation and restore ordering are documented.
- [ ] Static source review completed.
- [ ] Unreal build/UHT/Editor/PIE tests: deferred, not run.
