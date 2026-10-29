# P7 — Shared Structural Graph Workstream Closeout

**Canonical scope:** M059–M070, as mapped by `ARCHITECTURE_14_PHASE_ROADMAP.md`.  
**Status:** logical graph, data-driven repair and safe standalone salvage improved; physical assembly and Unreal acceptance remain open.

## Source work completed

- Primary and secondary support edges now check anchor occupancy on both the parent/support and child endpoints. A node with dependents cannot be reparented through the low-level graph API.
- Disconnecting a node preflights dependent support paths and rejects any operation that would leave a dependent without its last healthy support.
- Leaf-first removal planning includes dependents linked through secondary supports, not just primary-parent edges.
- `structure_repairs.json` defines wood, stone and stick repairs with target kind, consumed material, reusable tool, repair amount and conservative salvage output.
- `TryRepairDamageWithInventory` validates node kind/revision/damage, material count and tool presence, then consumes the material and repairs the same authoritative graph damage value. The tool remains reusable.
- `TryDismantleStandaloneNodeToInventory` consumes the physical item's ledger identity and returns authored salvage only for a standalone node without parent links, secondary supports or dependents. Connected pieces are rejected instead of being removed with stale anchor/graph references.
- Automation source covers missing-material failure, resource-backed repair, tool preservation, standalone salvage, stale revisions, support-preserving disconnect, load distribution, snapshot export, procedural bridge proxy pieces and physical secondary-anchor reservation/release.
- Stone-piece authoring has distinct enum slots for wall, door/window frame, wall opening, floor/roof, ramp/stair, bridge/walkway, gate/fence/railing and defensive wall variants. Those slots require appropriate physical meshes/anchors in Blueprint content; the enum does not claim geometry is already authored.

## Remaining physical construction work (not fully closed)

- M064: actual wall-opening/door/window geometry and actor-facing placement/removal rules.
- M065: authored floors, roofs, ramps and stairs with measured support/span constraints.
- M066: bridges, rope crossings and elevated walkways with multi-anchor reservations and traversability/capacity acceptance.
- M067: authored gate, fence, railing and defensive-wall geometry and raid/traversal integration.
- Connected-piece dismantle with alternate-support promotion, actor-local anchor synchronization, material recovery and leaf-first physical collapse transactions.
- Repair/salvage integration through every connected log/stone/stick actor adapter.
- Full physical-world save/restore acceptance.

## Verification boundary

- [x] Static source contracts/data and automation test source authored.
- [ ] Unreal Engine 5.8 build/UHT and Editor automation execution — not run here.
- [ ] Mesh/collision, connected collapse, cross-material anchors, repair/salvage and save/load PIE acceptance — pending.
- [ ] Do not merge to `main` until the local engine validation gate is green.
