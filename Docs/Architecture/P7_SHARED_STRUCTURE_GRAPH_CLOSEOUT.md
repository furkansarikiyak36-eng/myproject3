# P7 — Shared Structural Graph Workstream Closeout

**Canonical scope:** M059–M070, as mapped by `ARCHITECTURE_14_PHASE_ROADMAP.md`.  
**Status:** source-level mechanics and connected dismantle/collapse routes implemented; engine acceptance remains open.

## Source work completed

- Primary/secondary support graph, anchor reservations, support-path validation, aggregate load, damage/repair and deterministic leaf-first removal.
- Operable door/gate leaf state and window shutter state; wall-opening passage semantics; raid-blocking queries that distinguish clear passages from barriers.
- Bridge/walkway second-anchor/span preflight, concurrent traversal weight tickets and explicit release.
- Visible rope anchor-to-anchor link, span and traveler-weight limits, ticket-based traversal progression and endpoint cleanup.
- Connected dismantle returns authored salvage for each removed structure node while consuming unique source ownership records; raid collapse uses the same batch transaction without salvage.
- Alternate healthy supports are promoted to primary and physical actor/anchor links are rebuilt; transaction rollback restores the graph, inventory, item ledger and actor attachments on pre-destruction failures.
- Source tests cover these behaviors, including stale revisions and ownership preservation.

## Important limits

- Proxy geometry is greybox; no final assets are implied.
- Door/gate switching is immediate, not animated. A caller/Blueprint must drive rope traversal `Alpha`; player input/navigation integration is not yet tested end to end.
- Window shutters do not create a full-sized passage; wall opening is a separate full passage variant.
- Engine-specific destruction callbacks, physical collisions, raid navigation and save/load have not been exercised in UE 5.8.

## Verification boundary

- [x] Source/data logic and automation acceptance source authored.
- [ ] UE 5.8 UBT/UHT and `MyProject3.World.Structure.*` automation execution — not run.
- [ ] PIE verification for moving leaf collision, rope movement, bridge loading, raid navigation, alternate-support collapse and persistence — pending.
- [ ] Do not merge to `main` until the local engine validation gate is green.
