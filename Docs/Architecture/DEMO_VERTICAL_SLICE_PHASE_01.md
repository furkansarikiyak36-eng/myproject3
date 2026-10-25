# Demo Vertical Slice — Phase 01

**Repository:** MyProject3 / Unreal Engine 5.8  
**Branch:** `demo/vertical-slice-phase-01`  
**Base:** `fix/p0-drop-activation-fail-closed`  
**Status:** Demo construction specification; not a built Unreal map and not runtime-verified.

## Goal

Move from mechanic-layer work to one readable, connected playable demo. The demo must be a *small survival settlement*, not a tiny test room and not 119 disconnected showcase stations. One world state, one save, shared resources, visible cause and effect.

The first playable slice proves a coherent subset end to end. M001–M119 remain the full acceptance inventory; mechanics not yet implemented or wired must be explicitly marked blocked rather than represented by signs, props, or fake UI.

## World layout

Use a compact but navigable outdoor area. Coordinates below are relative planning coordinates in metres, not Unreal units (convert at implementation; standard UE scale is 100 cm per metre).

| Zone | Suggested centre | Purpose / required connections |
|---|---:|---|
| 1. Arrival + survival camp | (0, 0) | Player start, fire/cooking point if implemented, shelter, shared world clock and survival feedback. Safe reset/save checkpoint. |
| 2. Timber stand | (35, 15) | Harvestable trees, loose logs, sticks; a clear slope/grounding area so tree fall and grounded output can be observed. |
| 3. Wood yard | (25, -20) | Full-log inspection, length cuts, longitudinal split, half-log-only groove/trough, plank output. Keep raw, half-processed and finished pieces in separate marked bays. |
| 4. Stone + primitive tools | (-20, -20) | Loose stone/sticks, tool/crafting station, physical material hand-off to building yard. |
| 5. Construction yard | (-20, 15) | Foundation/support demonstration, snap anchors, mixed-material build, damage/repair/removal and collapse-safe checks. |
| 6. Farm + water edge | (45, -35) | Defined crop plots, water access, animal pen only for behaviours that actually exist. Shared-clock growth/harvest; no decorative fake simulation. |
| 7. Storehouse + production | (0, -55) | Storage capacity, production station, queued jobs, outputs, cargo source/destination and save/load continuity. Connect to timber yard and construction yard with a visible route. |
| 8. Defensive approach + discovery | (-45, 45) | Clear raid approach and defensible choke point; one discoverable ruin/clue. Use only if threat AI and persistent discovery are actually wired. |

**Route rule:** the main path links arrival → timber/stone → wood yard/construction → storehouse → farm and defensive approach. Add one meaningful obstacle or slope and a supported crossing only if its route-cost/transport behaviour exists. Do not add disconnected decorative zones just to make the map look larger.

**Scale rule:** use a space budget that allows a player to move logs, turn long pieces, see structures from a distance and reach the next work area without collision clutter. Place interaction props where the relevant operation can be approached from the correct side. Do not pack every mechanic into one pile.

## First playable vertical slice

This is the minimum coherent end-to-end path to build and then validate in Unreal:

1. Start a fresh seeded world; initialize the coordinator through an explicit game-mode/world bootstrap and fixed-step driver. If the driver/bootstrap is missing, the slice is blocked—do not assume a WorldSubsystem ticks itself.
2. Fell one harvestable tree. Confirm the tree reaches the configured grounded state before it creates its output logs. Repeated interaction must not create duplicate logs.
3. Pick up and drop a loose log. Confirm the physical actor, inventory/held state and ownership ledger agree after each transition.
4. Carry the log to the wood yard. Cut a valid length, then split it longitudinally. Attempt a groove/trough on the whole log first: it must reject with no state mutation. Repeat on the eligible half-log: it may succeed only if the exact output mesh/profile is configured.
5. Carry the processed pieces to the construction yard. Build a small supported frame from configured wood/stone/stick pieces. Mere mesh overlap must not create a graph edge. Try to remove a load-bearing support and verify either safe rejection or the configured dependent-collapse result.
6. Transfer one resource into storage, run one configured production recipe, and move the actual output into inventory/storage without duplication. If the chosen recipe/output cannot be produced from physical/logical inputs, mark this step blocked; do not substitute a prop.
7. Advance time using the fixed-step driver. Demonstrate only the systems genuinely connected to the same clock (for example, a crop growth transition or a raid warning). Never simulate elapsed time by directly editing displayed values.
8. Save, reload, and inspect the same world. Tree/log state, processed pieces, construction graph, item ownership, storage, production and clock must remain consistent. Repeating load must not duplicate resources.
9. Run the negative-case set below. Record results in a run sheet and keep screenshots/logs from the actual UE 5.8 session for the user/Claude Code to supply.

## Required negative cases

- Tree output before grounded state; duplicate harvest; invalid or missing log mesh/profile.
- Whole-log groove/trough; unsupported cut fraction; unavailable exact mesh row.
- Pick-up/drop repeated twice; inventory full; failed actor activation; failed ownership rollback.
- Storage full; production input missing; output capacity exhausted; cargo destination invalid.
- Floating/unsupported structural piece; edge inferred from overlap alone; unsafe support removal.
- Invalid protected-area excavation; unsupported terrain operation; stale save/reload reference.
- Fixed-step called before initialization, with invalid delta, or after fail-closed fault.
- Repeated save/load, repeated interaction, and operation retry with the same identity.

For every rejected operation, capture: starting state, action, result/failure reason, ending state, ownership/revision evidence, and whether retry remains safe.

## M001–M119 coverage approach

The demo must have a traceability sheet with one row per canonical ID and these fields:

- ID and acceptance condition
- Demonstration location and player action
- Preconditions and expected state transition
- Observable physical and logical result
- Negative/retry case
- Save/load expectation
- Source owner/file/PR
- Evidence level: source review / UE compile / runtime / accepted
- Status: implemented / partial / blocked / not in this slice

A shared location may exercise multiple IDs, but each ID keeps its own acceptance row. Do not change IDs or mark an ID complete merely because the area or subsystem exists. Use the canonical registry at `Docs/Architecture/MECHANIC_REGISTRY_119.md`.

## Build order

### D1 — Demo readiness inventory
- Identify the actual GameMode/player controller/character/interaction entry points and available level/asset content.
- Confirm which systems are callable from gameplay and which are only subsystem APIs.
- Identify exact configured mesh variant sets and valid wood processing profiles.
- Confirm bootstrap/fixed-step driver, interaction UI/feedback, and SaveGame entry points.
- Publish a source-backed blocker list before assembling props.

### D2 — Build the first route, not the full world
- Create the arrival camp, timber stand, wood yard and construction yard first.
- Add only assets needed to stage the vertical slice; no placeholder mechanic props that imply working behaviour.
- Connect these areas with clear traversal and adequate physical handling space.
- Wire actual interaction prompts and failure reasons where the project already supports them.

### D3 — Extend only after the first slice is coherent
- Add storehouse/production next, then farm, then defensive approach/discovery.
- A zone enters the demo only when its inputs, outputs, save state and failure behaviour are connected.
- If a mechanic is missing, keep it on the blocker sheet; don't fake it to claim 119/119.

### D4 — UE 5.8 acceptance gate
- User/Claude Code runs compile/UHT, editor load, PIE and scenario checks in the target project.
- This branch's source/document review is not a substitute for those checks.
- No Unreal build, UHT, editor, PIE or visual test was run by this work.

## Definition of done for this phase

- [ ] Source-backed readiness inventory identifies real entry points and blockers.
- [ ] First four zones are assembled in the actual UE project and navigable.
- [ ] Tree → grounded log → physical processing → supported construction works end to end.
- [ ] At least one storage/production transfer and save/load cycle works without duplication.
- [ ] Negative cases have captured expected and actual outcomes.
- [ ] All 119 IDs have traceability rows; unimplemented IDs remain explicitly blocked.
- [ ] UE 5.8 compile and runtime evidence is recorded by the user/Claude Code.

**Current truth:** this document defines the build contract. It does not claim that the map or any new gameplay code has already been created.
