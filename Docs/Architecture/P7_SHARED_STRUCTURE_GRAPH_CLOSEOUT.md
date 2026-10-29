# P7 — Shared Structural Graph Workstream Closeout

**Canonical scope:** M059–M070, as mapped by `ARCHITECTURE_14_PHASE_ROADMAP.md`.  
**Status:** graph-foundation blind-code pass closed; full physical construction and engine acceptance are not claimed.

## Implemented source contracts reviewed

- One world-local graph owns structural node identity, primary connections, anchor occupancy, support links, damage and revision state.
- Ground support is resolved through explicit primary and secondary support links; mesh overlap is not treated as a connection.
- Aggregate loads are calculated across support paths and checked against configured node capacities.
- Secondary supports can share load; support paths remain valid if one support is damaged and another remains healthy.
- Damage is clamped to the configured threshold; repair reduces the same authoritative damage value.
- Dependent removal ordering is deterministic and leaf-first for primary dependent chains.
- Graph snapshots include actor identity, material kind, parent/anchor data, secondary supports, load, support, damage and revision.
- A secondary-support removal now preflights the remaining support path and aggregate loads. A rejected removal restores the prior support list and does not advance revisions.

## Automation coverage authored

`MyProject3.World.Structure.SupportDamageAndRemovalContracts` covers grounded foundations, mixed-material graph nodes, primary and secondary support, load distribution, occupied-anchor rejection, invalid self-support, damage thresholds, repair, failed support removal, deterministic removal order and graph snapshot export.

## Scope still explicitly outside this blind-code closeout

The current graph is a logical authority, not a complete construction catalogue or physical transaction layer. The following require further domain actor/asset/integration work and must not be reported as complete merely because graph links can represent them:

- M064: door, window and wall-opening piece variants and their removal behavior.
- M065: floor, roof, ramp and stair physical variants and placement constraints.
- M066: bridge, rope crossing and elevated walkway physical variants, span and traversal acceptance.
- M067: gate, fence, railing and defensive-wall physical variants.
- M068/M069: inventory-backed repair material consumption, dismantle/salvage output and atomic actor/graph/ownership transactions.
- M070: end-to-end pickup prevention/authorization in every physical actor adapter.
- Cross-domain actor destruction/collapse and physical restore are not proven by the graph unit contracts.

These remain follow-up implementation/engine acceptance work; do not silently mark the canonical registry verified.

## Verification boundary

- [x] Static source review of graph APIs and invariants.
- [x] Graph support-removal failure path made non-destructive.
- [x] Automation acceptance source authored.
- [ ] Unreal Engine 5.8 build/UHT and Editor automation execution — not run here.
- [ ] Physical actor/mesh/collision, PIE collapse, repair/salvage and save/load acceptance — deferred.
- [ ] Do not merge to `main` until the local engine validation gate is green.
