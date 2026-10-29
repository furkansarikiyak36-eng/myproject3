# Carry, placement, and material interaction contract

This file is the shared contract for player carry actions and the placement kernel. It is intentionally material-agnostic; material-specific features belong in providers/components, while geometry solving and atomic commit belong in `Placement/`.

## Authority and ordering

1. **One placement authority.** All new physical placement previews and commits go through `UPlacementKernelSubsystem`. Do not add a second per-material snap solver or let a material component silently override a successful kernel solution.
2. **Preview must equal commit.** The exact normalized solution shown to the player is the one committed. A failed precondition must not leave half-created links, consumed items, changed transforms, or partially attached actors.
3. **Solve is pure.** A solve may query the world but must not mutate actors, inventories, structural links, operation receipts, or physics state.
4. **Commit is transactional.** Validate target identity/revision, actor lifecycle, support, collision, material constraints, and resource ownership before mutation. Record enough undo state to restore every mutation if a later step fails.
5. **Identity is not array position.** Connections use stable feature/anchor IDs (for example `End.A`, `End.B`, `Station.NN`, or authored socket IDs), never a transient feature-array index.
6. **One physical actor, one lifecycle.** A body transitions through world → held → placed → consumed/destroyed using the ownership/lifecycle authority. A failed transition must leave the prior state intact.
7. **No free duplication.** Taking, dropping, placing, dismantling, crafting, or transferring a physical item must have one authoritative owner at each step. A retry with the same operation ID must return the prior receipt or fail closed, never apply twice.
8. **Carry is not placement.** Carry capacity, mass, movement penalty, and held transforms are carry concerns. Snap features, support links, valid surfaces, and placement transactions are kernel/provider concerns. Carry code may request placement; it must not calculate a competing snap.
9. **Tool and stamina preflight precede mutation.** Check required tool, current inventory, range, stamina, action cooldown, target revision, and permitted state first. Charge/consume only after the action commits successfully; use a receipt for retryable world operations.
10. **No implicit terrain edits.** A failed placement does not flatten, excavate, fill, or otherwise mutate terrain to make itself fit. Terrain support must be explicit and permitted by the terrain rules.
11. **Grounded pieces remain grounded.** A spike, post, or other ground-dependent piece needs a valid terrain/support feature; nearby position alone is not support. Upper pieces need a valid support path in the structural graph.
12. **Cuts change capabilities.** A half-cut log is a distinct state with distinct available features. It must not be treated as an uncut straight log; grooves/notches are legal only after their required preparation step.
13. **Physical assembly scope is frozen until P10.** Do not add new log/stone/stick joinery meshes, notches, mitres, mortise/tenon geometry, or cross-joint geometry before the dedicated P10 phase is reopened by the user. Fix contracts and data around existing geometry only.
14. **Fail closed on ambiguity.** Duplicate IDs, stale revisions, missing feature definitions, unsupported surfaces, conflicting owners, or non-finite transforms must reject the operation with no partial side effects.
15. **Tests are authored with rules.** Cover success, invalid target, missing support/tool/material, stale revision, failed rollback, and idempotent retry. An authored test is not a passing test until run in the Unreal automation environment.

## Material-provider boundary

- The kernel owns generic placement solving and transactions.
- Providers expose material-specific features, body constraints, support anchors, and reversible link operations.
- Structural graph code owns support/load relationships; providers do not independently invent graph edges.
- Inventory/ownership owns item identity and transfer; neither the kernel nor carry component may duplicate ownership state.
- Visual mesh and collision setup must agree with provider features, but visual validation is a separate PIE requirement.

## Required acceptance record

For any new carry/placement mechanic, record separately:
- `code-written`: source and tests authored.
- `built`: Unreal build succeeded.
- `tested`: relevant automation tests passed.
- `played`: user verified behavior in PIE.
- `done`: PIE and save/load round-trip verified.

Never promote a task above the highest evidenced level.
