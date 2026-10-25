# Pass G — Reference-Driven Gap Closure Plan

**Branch:** `mechanic-detailing-pass-g-reference-gap-plan`  
**Base:** `mechanic-detailing-pass-f-world-coordination`  
**Status:** Planning and source-gap definition only. No gameplay implementation or verification claim is made by this document.  
**Scope authority:** Canonical M001–M119 registry and the existing 14-phase roadmap.  
**Reference games:** Sons of the Forest, 7 Days to Die, Death Stranding, The Long Dark, Satisfactory, Hydroneer.

## 1. Purpose and decision

The reference-game analysis changes the implementation plan from a list of isolated mechanics into a dependency-driven simulation plan. The goal is not to copy six games wholesale. The goal is to combine their useful design patterns through shared authoritative world systems:

- **Sons of the Forest:** physical resource processing, explicit construction anchors, support and state-dependent assembly.
- **7 Days to Die:** pressure events, defensive structures, constrained digging and underground shelter.
- **Death Stranding:** terrain-aware carrying, traversable infrastructure and persistent route improvements.
- **The Long Dark:** meaningful survival trade-offs shaped by weather, shelter, fatigue and scarcity.
- **Satisfactory:** transparent production chains, queues, throughput and blocked-state reasons.
- **Hydroneer:** extraction-to-processing-to-storage loops and linked production equipment.

Do not implement separate incompatible versions of identity, time, item ownership, structural links, resource costs or persistence for each reference system.

## 2. Current baseline and evidence limits

Passes A–F already added source layers for a logical world clock, in-memory operation ledger, identity/lifecycle transitions, transient committed events, wood-processing operations, stone/stick anchors, inventory/crafting, structural graph, threat director, route graph and a simulation coordinator.

These are useful foundations, but their presence does not prove end-to-end mechanics. Known gaps in the current source review include:

1. **Operation atomicity:** the operation ledger is in-memory and is not yet a universal transaction boundary. Inventory snapshot rollback cannot undo an already-mutated actor, graph edge, terrain edit or published event.
2. **Clock/coordinator atomicity:** Pass F can advance the clock before the threat director rejects a step. A failed step can therefore leave partial world-time progress.
3. **Item ownership:** world pickup/drop and inventory ownership are not yet a single transaction; missing item weights can be treated as zero by the current inventory path.
4. **Wood processing:** a cut-action layer exists, but all registry actions still need explicit state/geometry/output mapping and consistent failure/retry rules.
5. **Construction:** the graph and actor adapters exist, but collapse resolution, multi-domain support, stable dependent-removal order and persistence are incomplete.
6. **Threat:** scheduling/state management exists; AI spawning, navigation, attack selection and structure damage adapters are missing.
7. **Logistics:** route search exists; it does not yet move actual cargo or feed production. Route costs are not yet derived from terrain, load, weather or threat.
8. **World simulation:** terrain editing, water/moisture/fire, farming, animals, weather and survival systems are not implemented as a connected deterministic simulation.
9. **Persistence:** SaveGame annotations and snapshots are not an integrated versioned world-save/restore pipeline.
10. **Verification:** no Unreal Header Tool, Unreal Build Tool, Editor, PIE or gameplay validation is claimed. Registry states must not be upgraded based on documentation alone.

**Status discipline:** the registry currently contains unreviewed/not-run entries. This plan does not silently relabel them. Every future status change must cite source evidence, and engine verification must remain separate from static source review.

## 3. Cross-system contracts to build first

### Contract A — Authoritative operation boundary

Every world-changing operation should have a request containing an operation ID, expected object/world revision where relevant, validated inputs and explicit cost/output rules. It should produce a structured result and either commit all authoritative mutations or leave all of them unchanged.

Required participants include inventory, world item actors, processing actors, anchor reservations, structural graph, terrain edits, production queues and threat damage.

- Reject unknown item definitions or unknown weights safely; never silently grant zero-cost/zero-weight resources.
- A retry using the same operation ID must not duplicate output or consume resources twice.
- Publish committed events only after the authoritative commit.
- Avoid pretending a sequence of local snapshots is a cross-system transaction. Use a defined prepare/commit protocol, compensating actions with strict guarantees, or a single transaction owner.

### Contract B — One logical world timeline

Use the logical world clock as the time authority for raid scheduling, production progress, crops/animals, weather, exposure, food spoilage and recovery.

- A fixed step is all-or-nothing across the participating systems.
- All systems prepare/evaluate the same step index and time before the coordinator commits it.
- If any participant refuses, no participant may retain a partial step.
- Random decisions derive from the world seed and stable operation/event identifiers, not uncontrolled frame timing.
- The coordinator remains explicitly driven by a fixed-step owner; it must not accidentally create a second tick source.

### Contract C — One world-object identity and ownership model

An item must be in exactly one authoritative ownership state: world, held, inventory/storage, in transit, consumed, or transformed into output. The concrete enum may be refined, but domain actors cannot invent conflicting ownership rules.

- Pickup: world actor and inventory capacity are committed together.
- Drop: inventory removal and world spawn/placement are committed together.
- Processing: input identity is consumed/transformed exactly once and output identities are created exactly once.
- Transport: cargo is neither simultaneously in storage and on a cart nor lost when a route is interrupted.
- Save/load preserves identity and ownership without duplicate actors.

### Contract D — Shared structural graph

The graph is authoritative for physical construction relationships across logs, stones, sticks, walls, bridges, traps and later production/shelter structures.

- Connections require compatible authored anchors; overlap alone is never a connection.
- Support/load checks walk a defined graph and reject cycles or invalid support paths.
- Removing/damaging a support computes the affected dependent set before mutation.
- Collapse/removal has deterministic ordering and produces consistent salvage.
- The system either supports multiple parents correctly or explicitly models a single-parent restriction; it must not imply arbitrary multi-support if it cannot calculate it.
- Graph state and actor attachments must commit and restore together.

### Contract E — World affordance and capability data

Tools, materials, structures, machines and terrain layers should declare capabilities and constraints through data-driven definitions instead of scattered hard-coded checks.

Examples: cut-action allowed for input state; anchor compatibility; structural capacity; diggable layer/depth; tool requirement; machine input/output; fuel; weather protection; storage capacity; route traversal restrictions.

This contract prevents each future mechanic from creating a new incompatible rule system.

## 4. Revised dependency-ordered implementation plan

This is a **work plan layered over existing phases**, not a replacement of the immutable M001–M119 IDs or permission to skip earlier dependencies. Existing Pass A–F code should be extended where correct, not rewritten merely to match these headings.

### Workstream 1 — Close core correctness gaps

**Reference value:** all six games.  
**Registry focus:** M001–M008, with shared requirements for M049–M058 and all time-dependent systems.

Deliver:
- Define authoritative operation request/result/revision/idempotency semantics.
- Make the operation ledger usable by domain operations, with explicit behavior for duplicate, in-progress, completed, cancelled and capacity-exhausted IDs.
- Resolve the Pass F partial-clock-advance failure mode with a prepare/commit step protocol or equivalent atomic strategy.
- Establish item-definition validation for weight, stack limit, category and processing outputs.
- Define versioned save schema, identity restoration and dependency-ordered restore contract.
- Ensure committed events are emitted only after commit and can be reconciled during restore.

Exit condition: architecture and source paths agree on failure atomicity, retry safety, world time and identity. This exit is a source review condition, not proof of Unreal runtime correctness.

### Workstream 2 — Finish physical wood, stone and stick lifecycles

**Reference value:** primarily Sons of the Forest; supports Satisfactory and Hydroneer.  
**Registry focus:** M009–M048 plus inventory links M049–M058.

Deliver:
- Tree harvest/felling and deterministic log generation from configured tree dimensions.
- Explicit log states for whole, cross-cut, split, quartered, hewn, grooved, trough/channel, plank, notched and spike outputs as supported by the registry.
- For every action: allowed input states, tool/cost requirements, output identity/mesh/type, transform preservation, repeat behavior and failure-with-no-mutation.
- Enforce the exact longitudinal-split prerequisite for grooves/troughs/planks where specified.
- Connect stone collection/shaping/stacking and stick harvesting/sharpening/traps to the same ownership and operation contracts.
- Make trap activation require its required assembled components, placement/grounding and trigger state; separate arming from trigger detection.
- Connect pickup, carry, drop, quick access and crafting so an item cannot exist in two places or be consumed twice.
- Add salvage/dismantling outputs through the same authoritative resource transformation path.

Explicitly defer perfect mesh artistry and high-fidelity cutting animation. First guarantee the correct state, output and gameplay constraints with available assets.

Exit condition: all M009–M058 have per-mechanic source-backed transition tables and no known duplicate ownership path. This does not mean all are engine-verified.

### Workstream 3 — Complete construction support, damage and recovery

**Reference value:** Sons of the Forest + 7 Days to Die + Death Stranding.  
**Registry focus:** M024–M027, M031–M038 and M059–M070.

Deliver:
- Connect all construction actors through one graph adapter.
- Define support path, anchor capacity, load/span/overhang limits and deterministic evaluation order.
- Support walls, corners, columns, beams, openings, floors, roofs, ramps, stairs, gates, fences, bridges, ropes and elevated crossings within the chosen registry scope.
- Make support removal a preflighted operation: either safely reject or commit the complete dependent-removal/collapse result.
- Route damage from weapons, hazards and raids into the same structure-damage authority.
- Define repair, dismantling, salvage and resource recovery without duplicate state.
- Make placement preview and committed transform obey the same anchor/geometry rules.

Important design limit: do not simulate full engineering-grade structural physics. Use predictable game rules and bounded support/load checks that are understandable and testable.

Exit condition: mixed-material construction, safe support removal, damage and collapse all use the same graph and operation boundary.

### Workstream 4 — Implement bounded terrain editing and environmental state

**Reference value:** 7 Days to Die + Death Stranding + Hydroneer.  
**Registry focus:** M071–M080, with dependencies from M077–M079.

Deliver:
- Define editable/protected terrain layers, maximum depth/range, chunk boundaries and per-operation work limits.
- Support pits, small horizontal tunnels and rooms without enabling unrestricted whole-world digging.
- Ensure terrain geometry, collision and navigable surface agree after an edit.
- Define underground support/collapse policy appropriate to the chosen simplified simulation.
- Persist terrain edits and restore them before dependent structures, routes and actors.
- Implement bounded water-level/flow/flooding, soil moisture/runoff and fire/rot/exposure only to the depth needed by farming, shelter and resource systems.
- Add environmental ruins/clues as world objects with stable IDs and discovery state; avoid expanding this into cinematic story production.

Performance guardrail: terrain operations must be bounded by edited region and configured layer, not by the total world volume.

Exit condition: a player can dig a permitted space, use it, save/load it and retain correct collision/support without unrestricted voxel simulation.

### Workstream 5 — Build farming and animal systems on the same clock

**Reference value:** farming-simulation depth, supported by The Long Dark's scarcity loop.  
**Registry focus:** M081–M090.

Deliver:
- Planting validates seed, soil, season and available plot.
- Crops advance through explicit stages using logical time; watering, nutrients and stress influence outcomes.
- Harvest output is deterministic under the same world state and configuration.
- Animals have ownership/location, feed, habitat capacity, growth/health, product generation and reproduction rules.
- Feed/manure/nutrient loops connect to soil without circular resource creation or unbounded simulation.
- All produced goods enter the same inventory/storage/transport ownership system.
- Save/load restores crop stage, animal state, timers and outputs coherently.

Scope limit: model a manageable set of crops and animals first; do not implement every farming-simulator feature or complex individual animal AI before the shared loop works.

Exit condition: farming and husbandry progress while the player is away, use shared time, have bounded costs and persist correctly.

### Workstream 6 — Turn the threat director into a world-facing raid system

**Reference value:** 7 Days to Die, with survival pressure from The Long Dark.  
**Registry focus:** M091–M100, connected to M059–M070 and M107–M118.

Deliver:
- Keep the existing deterministic director/state machine where correct.
- Add bounded spawn planning, valid spawn checks, caps/queues and explicit failure recovery.
- Connect target selection/navigation to player, shelter and defensible routes.
- Route attack and bypass decisions into structure damage and traversal constraints.
- Make raid state transitions safe under interruption, cancellation, missing spawn points and destroyed targets.
- Pressure/difficulty inputs may include configured noise, settlement stock and defense signals; every input must have a clear scale and bounded effect.
- Ensure the raid system does not spawn or damage twice after retry, save/load or fixed-step rejection.

Scope limit: begin with a small set of enemy archetypes and understandable tactics. Do not build a huge behavior tree ecosystem before the world-facing raid loop is reliable.

Exit condition: a raid can be scheduled, telegraphed, executed, interact with actual structures, end or fail, and restore consistently.

### Workstream 7 — Connect terrain-aware logistics and production chains

**Reference value:** Death Stranding + Satisfactory + Hydroneer.  
**Registry focus:** M101–M106, M049–M058 and M108–M109.

Deliver:
- Storage racks, carts/sleds and containers use the common cargo ownership model.
- Zipline/rope transport moves actual cargo along validated endpoints and capacity constraints.
- Route costs derive from authored traversal data and bounded modifiers for slope, load, weather, obstruction and threat where appropriate.
- Roads and bridges persist and measurably reduce route cost or unlock passage.
- Production stations declare inputs, outputs, duration, fuel/power if used, storage and queue capacity.
- Queues expose clear blocked reasons: missing input, output full, no fuel, broken link, invalid recipe or disabled station.
- Resource movement, queue consumption and output creation share the transaction boundary.
- Begin with primitive chains such as logs → split logs → planks → building components; expand mining chains only after this contract works.

Scope limit: no need for Satisfactory-scale factory automation, belts everywhere or complex power simulation in the first implementation.

Exit condition: a resource can be gathered, transported, processed, stored and consumed by a construction recipe without duplication or hidden loss.

### Workstream 8 — Complete survival, weather and exploration state

**Reference value:** The Long Dark, supported by environmental world state.  
**Registry focus:** M008, M107–M108 and M110–M119.

Deliver:
- Shared clock-driven hunger, thirst, stamina, rest, fatigue and injury/illness state.
- Weather and seasonal transitions with bounded effects on temperature, wetness and exposure.
- Clothing insulation/protection and shelter quality affect exposure through one understandable calculation path.
- Food quality/spoilage uses the same time and persistence contract.
- Fire, fuel, heat and cooking connect to shelter and survival without duplicate timers.
- Player-facing status reports expose relevant resource, durability, storage and survival values without requiring hidden debugging knowledge.
- Environmental discovery state persists and unlocks world context without requiring cinematic story systems.
- Every modifier must have a defined range, stacking rule and source; avoid unrelated systems each applying unbounded hidden penalties.

Scope limit: prioritize meaningful choices and readable feedback over medical simulation depth or dozens of survival meters.

Exit condition: weather, shelter, clothing, rest, food and exploration all use shared world time and persist coherently.

### Workstream 9 — Cross-system persistence and recovery

**Reference value:** all six games; essential to a persistent survival world.  
**Registry focus:** M001–M008, M076, M109 and every system with mutable state.

Deliver:
- Versioned world-save manifest and schema migration policy.
- Explicit restore order: world identity/clock → terrain → physical actors and graph → inventory/storage/cargo → farms/animals → production/routes → threat state → survival/weather/discovery.
- Validate references and repair or reject orphaned identities, graph edges, cargo links and queue reservations.
- Persist deterministic seeds, logical time, revisions, operation deduplication records where needed, and system snapshots.
- Define interrupted-save behavior and reject incomplete snapshots rather than loading a half-world.
- Document compatibility policy for old saves and newly added fields.

Exit condition: a representative integrated world survives save/reload without duplicated items, broken support graphs, reset queues, repeated raids or lost discoveries.

### Workstream 10 — 119-mechanic acceptance map and final demo

**Reference value:** all six games, combined rather than isolated.  
**Registry focus:** M001–M119.

For every mechanic record:
- setup/location;
- prerequisite state and required assets/config;
- player action;
- expected world-state/result;
- invalid action and expected rejection;
- retry/idempotency expectation;
- relevant cross-system effect;
- persistence expectation;
- evidence/status.

The final demo should be a **small number of intentional test areas**, not 119 unrelated objects scattered randomly:
1. **Resource yard:** tree/log states, cutting, splitting, planks, stones, sticks, pickup and crafting.
2. **Construction yard:** anchor compatibility, mixed-material structures, support/load, removal, collapse, repair and salvage.
3. **Terrain test pocket:** allowed/forbidden layers, pit, tunnel, underground room, collision and support.
4. **Farmstead:** crops, animals, storage, feed/manure and time progression.
5. **Raid settlement:** warning, spawn constraints, navigation, attacks, structural damage, recovery and survival pressure.
6. **Logistics/production site:** cargo, routes, roads/bridges/zipline, stations, queues and blocked reasons.
7. **Survival/exploration route:** weather, shelter, clothing, rest, food quality and persistent discoveries.

Each location should test many related mechanics naturally while preserving clear individual acceptance evidence. The demo is created after the code layers and integrations are complete, as requested. Unreal build/UHT/Editor/PIE/gameplay validation remains a separate final gate and must not be represented as passed before it is run.

## 5. Reference-to-system coverage map

| Reference | Primary implementation workstreams | Must affect |
|---|---|---|
| Sons of the Forest | 2, 3, 9, 10 | Item lifecycle, processing states, anchors, support, trap prerequisites, salvage |
| 7 Days to Die | 3, 4, 6, 8, 9, 10 | Structural damage, constrained digging, raid pressure, survival state |
| Death Stranding | 3, 4, 7, 9, 10 | Traversal, cargo burden, roads/bridges/ropes, route costs and persistence |
| The Long Dark | 5, 6, 8, 9, 10 | Scarcity, daily decisions, weather, exposure, shelter and recovery |
| Satisfactory | 1, 2, 7, 9, 10 | Input/output ownership, production queues, throughput, storage and blocked reasons |
| Hydroneer | 1, 2, 4, 7, 9, 10 | Extraction, raw-resource identity, processing, transport and linked machines |

## 6. Global acceptance invariants

No workstream is complete unless it preserves these invariants:

1. Invalid prerequisites reject the operation without partial authoritative mutation.
2. A resource has one authoritative owner and cannot be duplicated by retries.
3. Every processing operation has explicit input state, output state and cost.
4. Geometry/overlap alone does not create a construction connection.
5. Unsafe support removal is rejected or resolves all dependent pieces deterministically.
6. All time-dependent systems use the shared logical clock.
7. A failed fixed step cannot advance only a subset of world systems.
8. Terrain edits respect configured layers, range, depth and work budgets.
9. Production and transport expose blocked/failure reasons rather than silently stalling or losing resources.
10. Save/load preserves identities, graph relations, terrain, queues, survival state and discovery state.
11. The demo includes a positive and negative case for each mechanic, not just a happy-path visual.
12. Static review, Unreal compilation and gameplay verification are reported as separate evidence levels.

## 7. Execution and validation policy

- Continue the existing stacked-branch workflow; this plan is based on Pass F and must not be merged directly into main as a shortcut.
- Implement source layers and integrations in dependency order.
- Do not create the final demo early just to make incomplete mechanics appear present.
- Do not run or claim Unreal build/UHT/Editor/PIE/gameplay tests until the agreed final validation gate.
- Source review can identify compile risks but cannot certify engine compatibility.
- Keep the canonical registry IDs immutable; update statuses only with explicit evidence.
- Each implementation pass should add a contract, source review, known limitations and updated coverage notes.
- No branch/PR is merged without explicit authorization.

## 8. Immediate next implementation order

1. **Pass H:** transaction-safe fixed-step and operation boundary; close clock partial-advance issue and item weight/definition fail-closed behavior.
2. **Pass I:** finish resource ownership from world pickup/carry/drop through inventory, processing and output creation.
3. **Pass J:** complete structural graph support, safe removal and deterministic collapse/damage flow.
4. **Pass K:** bounded terrain editing and persistent terrain state.
5. **Pass L:** farming/animal lifecycle on logical time.
6. **Pass M:** threat director adapters for actual spawn, navigation and structural damage.
7. **Pass N:** cargo movement, terrain-aware routes and production queues.
8. **Pass O:** survival/weather/exposure/food/shelter and persistent discoveries.
9. **Pass P:** integrated save/load and recovery.
10. **Pass Q:** source coverage closure for M001–M119, then the final authorized Unreal validation gate and integrated demo.

This ordering is a practical execution sequence. If source inspection reveals a prerequisite is missing, the dependent work pauses for that contract rather than adding a duplicate implementation.
