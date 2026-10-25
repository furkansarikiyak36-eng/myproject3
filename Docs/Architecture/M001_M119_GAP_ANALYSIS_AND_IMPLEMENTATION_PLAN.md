# M001–M119 Gap Analysis and Implementation Plan

**Snapshot:** 2026-10-25  
**Primary source baseline:** `mechanic-detailing-pass-h-integrated-world-systems`  
**Planning branch:** `landscape-builder-plan`  
**Evidence level:** repository/document/source inspection only. No Unreal Editor, UHT/UBT, PIE or gameplay run was performed.

## Executive decision

Do not treat the existence of 119 named mechanics, phase documents, C++ classes or a demo map as proof of a complete game loop. The next work is to turn each mechanic into a precise state/behavior contract, close the authoritative cross-system foundations, and then complete physical/world-facing adapters in dependency order.

The canonical IDs M001–M119 remain fixed. Expand the acceptance requirements underneath those IDs; do not renumber them or silently create a second competing registry. Keep story/cinematics outside this implementation pass. Mesh artistry and animation polish are secondary to correct state transitions, physical outputs, placement constraints and persistence.

## 1. Evidence and scope limitations

The current registry itself records that an earlier MyProject2 catalogue and `MEKANIK_DERINLIK_PLANI.md` covered roughly 480–500 sub-mechanics, but the exact historical 119-name list was not recovered in the reviewed files. Therefore the current registry is an explicit scope baseline, not proof that every sub-mechanic in the earlier catalogue was recovered.

The registry currently contains all IDs M001–M119. In its CSV:
- **99 of 119 acceptance criteria are generic** (“valid context produces the documented result; invalid targets/prerequisites fail without partial mutation”).
- 20 criteria give a more mechanic-specific rule.
- All 119 statuses remain `unreviewed`; static review, Unreal build and runtime columns remain `not_run`.

That is the main specification gap: named scope exists, but many entries still lack exact inputs, state transitions, resulting geometry/actors/resources, rejection conditions, dependencies, user feedback, save behavior and test steps. The acceptance map assigns seven demo zones, but it is still a plan; it does not yet provide a complete executable setup/action/expected-state/cleanup script for every mechanic.

## 2. Branch audit: which source is the useful baseline?

The repository currently exposes 20 branches. The default `main` tree has 99 files, 11 files under Source, and no canonical M119 registry/architecture docs. It must not be the only inspection target.

| Branch family | Current evidence | Decision |
|---|---|---|
| `main`, `Chatgpt`, old `phase-01-foundation` | Older/smaller snapshots; M119 registry absent on `main` and `Chatgpt`. `phase-01-foundation` is diverged from main. | Do not use as the sole source baseline or merge blindly. |
| `phase-01-foundation-v2` through `phase-06-inventory-crafting` | Progressively stacked phases; earlier source and documentation. | Inspect as history/reference; compare unique diffs against their declared parents. |
| `mechanic-detailing-pass-a-core` through `pass-g-reference-gap-plan` | Successive source-review and integration passes. | Stacked branches are not independent features; do not merge each one separately into main. |
| `mechanic-detailing-pass-h-integrated-world-systems` | Largest reviewed integrated source baseline in this audit: 209 files and 62 `.h/.cpp` files; contains ownership, clock, save coordinator, structural, terrain, farming, threat, logistics and production layers. | Use as the source review baseline, not as an engine-verified build. |
| `landscape-builder-plan` | H-based planning branch; adds documentation, not gameplay code. | Keep the M119 gap plan and Landscape Builder plan together for review; do not treat docs as implementation. |

At the time of this snapshot, the Landscape Builder plan branch is three commits ahead of H and the H branch is substantially ahead of G. Ahead/behind counts measure Git ancestry, not mechanic completion. Re-run comparisons before integration.

## 3. Confirmed source/document gaps by domain

These findings are drawn primarily from `PASS_H_Q_IMPLEMENTATION_STATUS.md`, `PASS_G_REFERENCE_DRIVEN_GAP_CLOSURE_PLAN.md` and `MECHANIC_COVERAGE_AUDIT_PASS_01.md`. They are static repository findings, not runtime observations.

### A. Core world, time, identity and transactions — M001–M008, M049–M058, M081, M109

- The shared operation boundary and logical simulation coordinator exist, but a failed multi-system step can only be rolled back best-effort; subsystem restore can itself fail. This is not yet a universal atomic transaction across actors, graph edges, inventory, terrain and events.
- The item ownership ledger is bridged to log actors, inventory, storage and cargo; stone/stick actor adapters, actor respawn from inventory/storage, split/merge ownership rules and processing-output registration remain incomplete.
- Inventory/crafting still needs a single authoritative item-definition path and complete item-category/weight/stack rules, UI diagnostics, alternate recipe selection, returned materials/tool durability, cancellation and cross-domain atomicity.
- Logical clock and snapshots exist in the H layer, but persistence does not yet restore the physical world as a whole. M109 is a cross-system contract, not just serializing the current subsystem fields.
- M081 is placed under farming in the canonical registry but names the shared deterministic clock. One clock owner and one documented tick order must serve farming, raids, production, spoilage, survival and world events; it should not be reimplemented per subsystem.

**Required outcome:** stable identity + authoritative item ownership + operation ID/revision + preflight/commit/rollback policy + deterministic shared time must agree on the same state and failure semantics.

### B. Physical wood/log pipeline — M009–M028

- The source contains a log cut-action/output-mesh path for some transitions, but the reviewed documents explicitly say each canonical action still needs exact state/geometry/output mapping and negative/retry coverage.
- The production state must distinguish a whole log, partial cut, fully separated piece, longitudinal half, quarter, hewn piece, groove, trough, plank and notched/jointed piece. A tool action must not swap in a visually unrelated generic/full-log mesh.
- The user-reported failure where a partially cut log becomes a straight/full log is a high-priority state-to-mesh acceptance case. It cannot be fixed by adding another registry name; the cut progress/state, output piece type and physical mesh/collision must change together.
- Groove/trough/plank creation must enforce their actual input state. A groove on a whole log must be rejected; a trough must require the intended split-half and an available output asset/geometry. Failed actions must not consume the source piece or duplicate outputs.
- Tree felling and log generation from tree dimensions remain more than a hit counter: they need a defined fall/grounded lifecycle, dimensions-to-piece rules, collision behavior and stable IDs.
- Snap placement must use a preview that matches the committed transform; support and parent/child relationships must be explicit. Grounding is a real state, not a UI label.

**Required outcome:** one documented processing state machine and an explicit state → allowed action → output geometry/material quantity table. Asset quality may be basic for early access; wrong output type/state is not acceptable.

### C. Stone, sticks, traps and primitive combat — M029–M048

- Stone code is a piece/anchor/carry/place foundation, not a complete masonry loop. Harvesting/shaping, multi-stone wall/corner/column/opening composition, support/damage/repair/salvage and mixed wood–stone integration remain incomplete.
- Stick state/anchor/trap-arm prerequisites exist, but harvesting, tool/weapon behavior, projectile behavior, fire/light, trigger sensing, defensive structures and salvage are not complete as a connected loop.
- Primitive traps need explicit component completeness, ground contact, armed/disarmed state, trigger volume/line, who can trigger them, one-shot/reusable behavior, damage/effect, reset and save behavior. An ungrounded spike must not act as an operational trap.
- Physical object counts must match the ownership ledger: collecting, splitting, crafting, storing, hauling, dismantling and destroying must never leave the same resource represented in two places or delete it on a failed operation.

**Required outcome:** finish one end-to-end wood path, one stone wall path and one stick/trap path first; then generalize shared behavior across materials.

### D. Shared construction graph, support and damage — M059–M070

- A shared graph foundation exists, but graph + actor destruction + salvage is not a fully atomic multi-actor transaction and the current graph retains single-parent constraints.
- The design still needs load-bearing/support semantics that distinguish mere contact/overlap from a structural connection; grounded foundation paths; span/overhang/load limits; multi-support relations; stable leaf-first collapse; and deterministic cleanup of anchors, inventory and ownership.
- Doors/windows/openings, floors/roofs/ramps/stairs, bridges/rope crossings, gates/railings and mixed-material joints must be connected to the same graph, damage and save model rather than existing as unrelated placement types.
- Damage from tools, collisions, raids, fire and environmental exposure must enter one authoritative integrity/damage path, not be applied separately by each actor adapter.

**Required outcome:** graph owns connections/support/damage outcomes; domain actors own their piece-specific state and expose anchors/capabilities through adapters.

### E. Terrain, Landscape Builder, digging and environmental simulation — M071–M080

- The terrain subsystem stores bounded, layer-gated logical cell edits, revisions, operation receipts and snapshots. It is **not** a renderer or collision adapter and does not yet modify Landscape/mesh collision, navigation, water, underground support or physical actors.
- M072's criterion explicitly requires geometry and collision consistency; the current logical edit layer alone cannot satisfy it. M074 needs connected tunnel/room geometry within depth and volume limits, not merely a set of excavated logical cells.
- Arbitrary tunnels/overhangs cannot be represented by an outdoor heightfield alone. Use native Landscape as the authored outdoor surface, then a separately bounded 3D excavation-pocket layer for rooms/traps. Do not convert the whole world to voxels.
- Water flow/flooding, soil moisture/nutrients/runoff, fire spread/rot/exposure and ruin/discovery actors are names in the registry, but the status report does not establish connected world-facing simulation for them.
- Terrain edits, support invalidation, navigation updates, physical excavation geometry, discovery state and persistence need committed-change messages from one authoritative terrain state.

**Required outcome:** profile-driven editor Landscape generation + protected/editable masks + bounded runtime excavation adapter + collision/navigation/support/save integrations. See `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md`.

### F. Farming and animal systems — M081–M090

- Crop stages/moisture/progress and animal hunger/health/product records exist as logical simulation layers.
- Farm/animal actor visuals, feeding-item transactions, season/soil data, habitat/pathing and transfer of produced goods into item ownership remain unconnected.
- The loop must close across seed/feed inventory → plot/animal state → deterministic time/weather/soil effects → harvest/byproduct capacity → ownership ledger/storage/production. Growth or animal output cannot repeat merely because a save is loaded or a tick retries.
- Reproduction requires habitat capacity; crop and animal state must be invalidated/refused safely when terrain or supporting structures change.

### G. Raids, AI and settlement pressure — M091–M100

- Threat scheduling/planning and a configured raid pawn/structure-damage adapter exist, but the current adapter is not complete enemy AI.
- Target selection, navigation, pursuit/attack behavior, settlement-aware targeting, spawn validity and route/bypass decisions remain incomplete.
- Raid damage must use the same structural integrity/graph path as normal damage. Threat pressure from settlement noise, stored resources and defenses must be explicit/tunable and based on authoritative state.
- Pause/reload/retry must not duplicate waves, enemies, damage, rewards or population counters; blocked and cancelled raids need recovery outcomes.

### H. Logistics and production chain — M101–M106

- Logical route/cargo and production queue state exists, including blocked reasons and snapshot data.
- Route costs are not yet derived from roads, bridges, rope/zipline, terrain, cargo load, weather or threat. Cargo has no physical travel/pathing or destination actor-spawn adapter.
- Production is not yet connected to actual machines, fuel/power/heat and physical processing outputs. The intended progression from raw wood to processed pieces/planks and then transport must be a real ownership-aware chain.
- Transfers need source quantity reservation, destination capacity checks, clear full/partial transfer policy and idempotent delivery. Production input/output/job state must commit together; blocked jobs must explain missing material, station, fuel, capacity or route without consuming resources unexpectedly.

### I. Survival, weather, shelter and exploration — M107–M119 except M109

- The logical state layer includes needs, stamina/fatigue, insulation, shelter protection, wetness, weather severity and persistent discovery records.
- The status report still lists food spoilage, fire/fuel/cooking, injury/medicine, sleep/rest actions, equipment-inventory binding and region-specific temperature as missing integrations.
- M118 requires a measurable relationship between shelter quality and exposure, using the same authoritative survival state rather than a separate shelter-only value.
- M119 requires discoveries to persist and reveal intended environmental context after reload. Discovery flags alone are not enough if the ruin/clue actor, unlockable context or player-facing indication never appears.
- Keep environmental storytelling diegetic and data-driven; no story/cinematic implementation is needed to complete this mechanics pass.

### J. Persistence and integrated restore — M007, M076, M109 and all save-required mechanics

- The H save payload is versioned (schema v2) for logical domains and restores in an order, but it does not yet persist/respawn physical actors, their transforms, structural graph links, world-partition state or terrain collision/mesh data.
- Schema migration from v1 is not implemented; cross-domain rollback remains best-effort. Save is refused while an item is Held because carry-component state is not represented in the manifest.
- Physical resources, actor IDs, graph edges, anchor reservations, terrain geometry/deltas, inventory ownership, farm/animal state, cargo, queues, raids and survival/discovery state must restore without dangling references or duplicated outputs.
- Restore order and error recovery must be specified against actor lifecycle and derived-state rebuilding; the current document is a partial logical save contract, not proof of a full playable world round trip.

## 4. Missing precision required for every mechanic record

For each existing M-ID, add the following fields to the contract/acceptance map. A mechanic is not ready for closure until these are explicit:

1. **Setup and prerequisites:** exact starting object/state, required tool/material/configuration, support/ground/region rules.
2. **Action and state transition:** source state, allowed action, progress or transition rules, destination state.
3. **Physical result:** exact output type, mesh/actor/geometry/collision, quantity and transform; mark N/A only for purely logical mechanics.
4. **Failure behavior:** each known rejection reason and proof that source state, inventory, anchors, graph, time and output remain unchanged.
5. **Retry/idempotency:** repeated operation ID, stale revision, duplicate interaction, pause/reload and competing interactions.
6. **Cross-system effects:** which single authoritative owners receive a committed event/update and which systems must not mutate the same state independently.
7. **Persistence:** fields/deltas, schema version, stable IDs and restore dependencies.
8. **Player-facing feedback:** visible action/result/blocked reason so the mechanic is understandable in the demo.
9. **Evidence row:** positive case, negative case, boundary case, retry case, integration case, save/load case and reset/cleanup.
10. **Implementation status:** skeleton / partial / reviewed static / verified engine, with links to source evidence; never mark verified-engine without the authorized engine gate.

Expand vague acceptance criteria under the existing IDs with stable sub-requirement keys (for example, `M019-R01`, `M019-R02`). These are child requirements, not new canonical M-IDs.

## 5. Dependency-ordered implementation plan

This plan prioritizes code correctness and integration, while keeping Unreal execution tests deferred until all planned mechanic layers are implemented, as requested.

| Order | Workstream | Main scope | Exit condition before moving on |
|---|---|---|---|
| 0 | Scope/branch lock | Freeze H as the inspected baseline; preserve 119 IDs; map current files to M-IDs; review unique branch diffs instead of merging stacks blindly. | One source/branch inventory and ownership map; no main-only assumptions. |
| 1 | Contract/detail pass | Expand the 99 generic criteria; define state tables for wood/stone/stick, terrain, support, queues, survival and discovery. | Every M-ID has setup, action, expected state/output, failure, persistence and evidence instructions. |
| 2 | Core consistency | M001–M008, operation boundaries, deterministic world time, item definitions/ownership, inventory/crafting; close partial-step and cross-domain transaction gaps. | One owner per state; no unknown items silently bypass limits; failures do not leave partial logical mutations. |
| 3 | Physical resource loops | M009–M058; tree/log state-to-mesh mapping, correct half-cut/groove/trough/plank outputs, stone/stick actors and traps, inventory/UI/output adapters. | At least one complete end-to-end loop per resource family with prerequisites and no duplicate/lost items. |
| 4 | Shared construction | M059–M070; multi-material graph, support/load, anchor reservations, damage, collapse, repair, dismantle and salvage. | Every placement/removal/damage path uses the same graph/integrity owner and leaves no orphan relationships. |
| 5 | Landscape and terrain | M071–M080; deterministic editor-side Landscape profile/build workflow, protected masks, bounded excavation geometry/collision, navigation/support notifications, water/soil/fire/environment links. | Outdoor Landscape and limited underground pockets are distinct layers; edit, collision, navigation, structures and save share revisions. |
| 6 | Farming/ecology | M081–M090; same clock, soil/season/weather inputs, crops, feeding, habitat, goods and nutrient loop. | Deterministic inputs/outputs connect to ownership and storage without duplicate ticks or products. |
| 7 | Raid/threat | M091–M100; bounded spawn, target/path/attack/bypass, one damage path, settlement pressure and recovery. | A raid can progress, be blocked/repelled/interrupted and save/reload without phantom state. |
| 8 | Logistics/production | M101–M106; carts/storage/zipline, terrain-aware route cost, real machine/fuel/output integration and blocked-reason UI. | Resource goes from physical/source ownership through route/station to a single destination/output. |
| 9 | Survival/exploration | M107–M108, M110–M119; exposure, shelter, clothing, needs, rest/sleep, injury, spoilage, cooking/fire, environmental clues and discovery unlocks. | Gameplay consequences use the shared clock and survival state; discoveries and context survive reload. |
| 10 | Persistence/integration | M007, M076, M109 and every save-required mechanic; actors/transforms/graph/terrain/queues/ownership restore order and migration/rollback. | A complete world round trip has no missing actor, dangling graph, duplicate item/output or divergent derived state. |
| 11 | Demo/evidence preparation | Complete the 119 scenario manifest and seven useful zones; arrange setup, actions, expected results, failure cases and reset. | Every ID has an executable scenario and evidence destination; demo is readable and not cluttered. |
| 12 | Final validation gate | Only after the code layers/integration are closed and engine access is available: source consistency review, then authorized Unreal build/UHT, Editor/PIE and mechanic acceptance runs. | Actual evidence recorded per M-ID; registry status updated only from evidence. |

## 6. Practical demo zones (keep the existing seven-zone concept)

1. **Resource yard:** tree → log → partial/full cuts → halves/quarters/planks/grooves/troughs → carrying and inventory; include deliberate invalid-input stations.
2. **Construction yard:** grounded anchors, mixed wood/stone/stick structures, foundations, span/load examples, doors/openings, bridge/rope crossing, support removal, repair/salvage.
3. **Terrain pocket:** editable/protected layers side by side, pit, bounded horizontal tunnel/room, blocked-depth case, collapse/support example, collision/navigation and save marker.
4. **Farmstead:** soil and seasons, water/moisture, crop stages, animal feed/habitat, manure/soil cycle, harvest and product storage.
5. **Raid settlement:** telegraphed raid, legal/illegal spawn zones, navigation/bypass, traps, structural damage, defenses, interruption and recovery.
6. **Logistics/production site:** storage, cart/sled, rope/zipline, route detour, primitive processing chain, fuel/heat, output capacity and visible blocked queue.
7. **Survival/exploration route:** exposed vs sheltered segment, clothing comparison, weather, food/water/rest, spoilage/illness states, ruin/clue discovery, persistent unlock and save marker.

Each station should be designed around the behavior being proved, not around showing every asset. The final demo must include both valid and invalid cases; a screenshot is not sufficient proof of hidden state, idempotency or atomicity.

## 7. Priority and acceptance rules

- **P0 — Correctness blockers:** partial mutation on failure, ownership duplication/loss, deterministic-time drift, wrong processing output, unsafe support removal, inconsistent restore.
- **P1 — Playable-loop blockers:** physical stone/stick/trap loops, structure support/damage, bounded terrain geometry/collision, actual AI behavior, real cargo/production outputs.
- **P2 — System-depth completion:** soil/water/fire/weather, animal ecology, survival integrations, discovery unlocks, route modifiers and machine links.
- **P3 — Presentation polish:** richer meshes/materials, tool animations, UI polish and exact visual fit after state/geometry/collision behavior is correct.

Do not use “class exists”, “source file exists”, “plan exists”, branch commit count or planned demo coverage as completion evidence. Do not change registry statuses until the corresponding source/engine evidence is available.

## Source references

- Canonical scope and criteria: `MECHANIC_REGISTRY_119.md` / `.csv`
- Initial per-mechanic source inventory: `MECHANIC_COVERAGE_AUDIT_PASS_01.md`
- Cross-system gap closure: `PASS_G_REFERENCE_DRIVEN_GAP_CLOSURE_PLAN.md`
- H integration and remaining gaps: `PASS_H_Q_IMPLEMENTATION_STATUS.md`
- Demo zone/evidence contract: `Docs/Testing/DEMO_ACCEPTANCE_MAP_M001_M119.md`
- Landscape pipeline: `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md`
- Branch stack rules: `BRANCH_INTEGRATION_AUDIT.md`

