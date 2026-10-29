# Phase 7 — Shared Structure Graph, Support and Damage

**Status:** Partial source implementation; graph contracts and a conservative repair/salvage path are now present, but the phase is not engine-verified.  
**Registry scope:** M059–M070  
**Prerequisites:** Phases 1–6 contracts and physical-piece layers  
**Engine validation:** Deferred to the final gate; all recent P7 commits are marked `[unbuilt]`.

## Current source boundaries

- `WorldStructuralGraphSubsystem`: canonical graph, primary and secondary support links, anchor claims, aggregate load, damage/repair, support-path queries and snapshot representation.
- `StructuralPieceInterface`: material-neutral physical anchor and placement adapter.
- `StoneBuildingPiece`: stone-domain anchor rules and lightweight engine-cube proxy archetypes for wall segments/openings, floor/roof, ramp/stair, bridge deck, rope crossing anchor, elevated walkway, gate, fence, railing and defensive wall.
- `WorldGameDataSubsystem` and `Config/GameData/structure_repairs.json`: data-authored repair material, reusable tool, damage restored and standalone salvage yield.
- `WorldStructuralGraphSubsystem::TryRepairDamageWithInventory`: material-backed graph repair.
- `WorldStructuralGraphSubsystem::TryDismantleStandaloneNodeToInventory`: bounded salvage path for standalone pieces only.

The engine-cube shapes are greybox/proxy geometry. They are not final visual assets and do not, by themselves, implement door interaction, open/closed states, real rope traversal, or a validated bridge span.

## Implemented contracts

- Graph connections reject duplicate/self/cyclic edges, invalid support paths, overloaded support chains and anchor claims reused at the graph level.
- Secondary support links validate physical adapter anchors/tags and reserve/release both actors' anchors with the logical link.
- Disconnect rejects when a dependent would lose its last healthy ground path; rejected operations do not intentionally mutate the canonical links or physical secondary-anchor reservations.
- Aggregate load is split across primary and secondary support contacts according to the logical support model.
- Damage is clamped at the configured threshold and material/tool-backed repair reduces the graph's authoritative damage value.
- Standalone dismantle rejects parented/connected/dependent nodes, consumes their source ledger identity, returns data-authored recovery material, and restores inventory/ownership/graph snapshots if a later step rejects.
- Dependent removal planning is deterministic, leaf-first, and aware of secondary-support dependents.

## Accepted boundaries, not yet closure claims

- M064–M067 now have authorable stone-piece enum choices and primitive geometry fallbacks; the gameplay behavior requiring doors/windows to operate, validated bridge/walkway spans, rope travel, and gate/fence/raid integration remains incomplete.
- Connected-structure dismantle/salvage and multi-actor physical collapse remain deferred; the new recovery API deliberately handles standalone pieces only.
- Runtime persistence of dynamically generated proxy meshes and full save/load physical graph/anchor restoration require the engine gate.

## Acceptance gates

- [x] Source API and invariants statically reviewed.
- [x] Data-driven repair profiles and conservative standalone salvage source implemented.
- [x] Automation source authored for graph support/damage, repair/salvage, proxy shape creation and secondary-anchor reservation.
- [ ] Cross-domain physical graph/anchor behavior verified in Unreal.
- [ ] Unreal 5.8 build/UHT plus `MyProject3.World.Structure.*` automation tests run.
- [ ] M064–M067 interaction and traversal behavior; connected collapse/repair/salvage and physical save/load acceptance.
