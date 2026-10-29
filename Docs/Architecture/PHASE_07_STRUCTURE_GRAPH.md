# Phase 7 — Shared Structure Graph, Support and Damage

**Status:** Source-level mechanics and transaction routes implemented for M059–M070; Unreal validation is still required.  
**Registry scope:** M059–M070  
**Prerequisites:** Phases 1–6 contracts and physical-piece layers  
**Engine validation:** UE 5.8 UBT/UHT, Editor automation and PIE not run in this pass.

## Current source boundaries

- `WorldStructuralGraphSubsystem`: primary/secondary support graph, anchor claims, load, damage, repair, leaf-first removal, snapshot restoration and connected dismantle/collapse transactions.
- `StructuralPieceInterface`: material-neutral physical anchor and placement adapter for logs, stones and sticks.
- `StoneBuildingPiece`: stone construction proxies, operable door/gate leaf, window shutter, wall-opening passage semantics, bridge/walkway span and traversal reservations, rope socket linking and controlled traversal.
- `WorldGameDataSubsystem` and `Config/GameData/structure_repairs.json`: material costs, reusable tools, repair amount and conservative per-piece salvage.
- `WorldRaidAdapterSubsystem`: skips already-passable openings, auto-selects the nearest blocking stone piece when no valid explicit target remains, and routes structural collapse into the common graph/ownership transaction. `WorldDemoBootstrapActor` now advances the adapter after each committed fixed step; adapter failure faults and stops the simulation. A visible raid pawn Blueprint is optional; structural damage still runs without one.

## M064–M067 source completion

- Door and gate leafs have an explicit open/close API with moving pivot geometry; pressing E while aiming at a door/gate/window toggles the leaf, and the logical passage and raid-blocking state changes with it.
- Window shutters can open, but opening a window is not treated as a full-size passage.
- `WallOpening` has a clear-passage state independent of a movable door.
- Bridge deck/elevated walkway accepts traversal only with a primary and second independent support, bounded span, valid support path, and both physical support anchors reserved. Active traversal IDs reserve a shared weight budget and can be released.
- Rope crossing anchors create a visible link between two compatible sockets, reject invalid/overlong spans, reserve each endpoint, and provide ticket-based movement along the line with a per-traveler and shared load limit.
- Raid target processing respects `IsRaidBlocking()`: open doors/gates and wall openings are removed from an explicit target list. If that leaves no target (or no list was authored), the adapter picks the nearest blocking stone structure to the raid target location and attacks one structure per pulse; closed gates, window frames, fences and defensive walls therefore participate without requiring a hand-authored target list. Explicit valid target lists retain their deterministic order.

## M063/M069 source completion

- Connected dismantling gathers a deterministic leaf-first structure plan and preflights every actor identity, lifecycle, world-item owner, graph revision, physical anchor and salvage recipe before changing ownership.
- Every unique source item is transferred from `World` to `Consumed`; dismantle yields the data-authored conservative salvage for every removed piece. Raid-induced collapse uses the same atomic graph/actor/ownership route without salvage.
- Alternate healthy supports are promoted deterministically to primary support rather than orphaning a surviving bridge/walkway. The graph rebuild reconstructs actor-local links and physical anchor reservations for surviving pieces.
- Rollback restores ownership ledger, inventory snapshot, graph snapshot and actor attachments when a pre-destruction step fails. Actor destruction happens only after the graph batch removal and lifecycle transition path has been committed.

## Boundaries and validation

- Geometry uses engine-cube greybox placeholders unless a Blueprint-authored mesh is provided; it is not final art.
- Door/gate toggling is immediate rather than timeline-animated. `J` enters the nearest compatible rope within 280 cm or cancels traversal; Tick advances it at the configured speed with swept movement. Player input and collision still require UE validation.
- Window shutters remain a small opening, not a crawl-through; raid navigation/pathfinding and collision around every physical proxy remain engine-test work.
- The new automation tests exercise door/gate/passage state, rope connection and movement, bridge capacity/span and alternate-support promotion, plus connected salvage and ownership. They are source only until the UE 5.8 test gate is run.
- Physical graph/anchor persistence and generated proxy persistence still require save/load and PIE acceptance.

## Acceptance gates

- [x] M059–M063 logical graph, support, damage and removal contracts in source.
- [x] M064–M067 greybox interaction/traversal/raid-state APIs in source.
- [x] M063/M069 connected ownership/salvage/collapse transaction route in source.
- [x] Acceptance test source authored for interactive pieces and connected transaction.
- [ ] Unreal 5.8 compile/UHT and Editor automation tests — not run here.
- [ ] PIE tests for animation/collision/traversal/raid navigation and physical save/load — pending.
