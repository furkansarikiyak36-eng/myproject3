# P7 — Shared Structural Graph Workstream Closeout

**Canonical scope:** M059–M070, as mapped by `ARCHITECTURE_14_PHASE_ROADMAP.md`.  
**Status:** graph, proxy pieces, material-backed repair and safe standalone salvage implemented in source; final phase closure is pending Unreal validation and connected-structure integration.

## Source work completed

- One canonical structural graph owns primary connections, secondary support links, support paths, aggregate load, damage and snapshot representation.
- Primary connection preflight checks node dependents and graph-level parent/child anchor claims; secondary support validates both adapter anchors/tags and reserves both physical anchor flags. Removal releases both flags.
- Disconnect preflights dependents against support loss and refuses a removal that would leave an attached part unsupported.
- Deterministic leaf-first planning considers secondary-support dependents.
- `structure_repairs.json` defines wood, stone and stick repair costs, reusable tools, repair amounts and conservative recovery yields.
- `TryRepairDamageWithInventory` consumes repair material while preserving the tool.
- `TryDismantleStandaloneNodeToInventory` consumes the original standalone ledger identity and returns authored salvage; parented, braced or depended-on pieces are rejected.
- New stone-domain archetypes build fallback engine-cube proxy geometry if no authored mesh is assigned: wall, door/window frames and opening, floor/roof, ramp/stair, bridge deck, rope-crossing anchor, elevated walkway, gate, fence, railing and defensive wall.
- Automation source covers support/load/damage, failed-disconnect preservation, resource-backed repair, reusable-tool preservation, standalone salvage, stale revisions, proxy construction and physical secondary-anchor reservation/release.

## Still not equivalent to a finished structural game loop

- Proxy pieces are greybox placeholders, not authored final assets.
- M064 door/window operation (open/close/occupancy), M065 structural-span limits, M066 load/traversal-safe bridge and rope transport, and M067 gate/fence/raid/traversal behavior remain incomplete.
- Connected dismantle/salvage and physical multi-piece collapse remain deferred. The new safe dismantle API is restricted to standalone pieces.
- Full physical graph/anchor/mesh save-load acceptance remains deferred.

## Verification boundary

- [x] Source/data contracts and acceptance test source authored.
- [ ] Unreal Engine 5.8 UBT/UHT and Editor automation tests — not run here.
- [ ] Cross-material placement, connected collapse, damage/repair and save/load PIE acceptance — pending.
- [ ] Do not merge to `main` until the local engine validation gate is green.
