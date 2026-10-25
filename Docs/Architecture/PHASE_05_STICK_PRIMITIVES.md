# Phase 5 — Stick and Primitive Tools

**Status:** Initial domain layer added; code review and engine integration remain open.  
**Branch:** \`phase-05-stick-primitives\`  
**Base:** \`phase-04-stone-masonry\`

## Registry scope

M039–M048: gather/break sticks; sharpen stakes; primitive tools and melee weapons; spear/bow/arrows; torches; stakes/fences/barriers; traps and trigger lines; storage/racks/furniture; garden/animal-support structures; dismantling and material recovery.

## Source delivered

- \`EStickPrimitiveState\` and \`EPrimitiveTrapState\` provide explicit piece and trap states.
- \`AStickPrimitivePiece\` owns a physical mesh, carry/place state, authored snap anchors and explicit parent-child links.
- Processing transitions reject unsupported state changes and reject processing placed/connected pieces.
- Trap arming requires all components, a valid trigger, and grounding when configured. Triggering requires the trap to be armed and still placed.
- Anchor IDs are unique; compatibility tags must match; connection reservations occur only after actor attachment succeeds.
- Removing a connection releases both anchor reservations and parent-child bookkeeping.

## Deliberate boundaries

This is a domain foundation, not a claim that all ten mechanics are complete. Item harvesting, inventory/resource costs, weapon combat, bow projectile behavior, lighting fuel, fence layout, trigger-line sensing, furniture interaction, garden/animal integrations, and salvage yield still need their owning system layers. Ground trace and surface suitability are caller responsibilities. The state machine is not durable across save/load yet.

## Validation

- [x] Source files and acceptance documentation added.
- [ ] Static source review of all callers and interactions.
- [ ] Unreal Engine 5.8 UHT/build and Editor/PIE/gameplay tests are intentionally deferred to the final gate.
- [ ] M039–M048 are not marked verified; registry remains \`unreviewed\` / \`not_run\`.
