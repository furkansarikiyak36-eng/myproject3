# MyProject3 — Phase 1–14 and Vertical Slice Gap Audit

**Audit basis:** tracked files and architecture/closeout documents on `phase-13-environment-survival-sequential`, plus the Phase 14 integration plan. This is a source/documentation audit, not an Unreal runtime audit.

**Repository identity:** `furkansarikiyak36-eng/myproject3`. Do not mix this audit with the separate historical `unrealgame` repository.

## Executive finding

Phases 1–14 are **not equivalent to a complete game or a complete vertical slice**. Several phases explicitly delivered data contracts or transient subsystem foundations and left the gameplay integrations, authored asset configuration, world/editor setup, save/load adapters, and UE 5.8 evidence open. The authoritative M001–M119 registry remains `unreviewed`; do not change that status based on this report.

Use the labels:
- **P0 — vertical-slice blocker:** the core loop cannot be called complete without it.
- **P1 — system integration blocker:** a subsystem exists but is not authoritative/connected end to end.
- **P2 — content/polish/scale:** needed for a convincing final demo or broader game, after the loop works.
- **Evidence pending:** source/docs exist but Unreal verification has not been performed.

## 1. Vertical slice: the missing end-to-end player loop

- [ ] **P0** Define one reproducible, playable route: spawn → gather tree/resource → fell/produce raw logs → process a log correctly → carry/drop it → snap it to a support → build a small supported structure → gather/use inventory materials → survive a weather/time interval → save → reload → confirm the same world state.
- [ ] **P0** Define a concrete acceptance script with initial conditions, controls, exact actions, expected state after every action, expected failures, and reset instructions.
- [ ] **P0** Build and verify a real UE 5.8 `.umap` for the vertical slice; code that can generate a map is not proof that a saved, usable map exists.
- [ ] **P0** Confirm the actual GameMode, pawn, camera, input bindings, collision channels, interaction distance and player spawn.
- [ ] **P0** Confirm every required C++ class is reflected/loaded and the editor target builds; a script failing to find a class means the native class/build prerequisite is not met.
- [ ] **P0** Run the map in PIE and verify the entire loop without console-only shortcuts.
- [ ] **P0** Create a clean reset/replay path so each test begins from a known world state.
- [ ] **P0** Separate VerticalSlice_A (known reference/baseline) from VerticalSlice_B (integration target); never delete or overwrite the baseline to make B appear complete.
- [ ] **P0** Validate B's camera angle, scale, lighting, ground plane, actor placement, collision and visual readability in an actual screenshot/capture.
- [ ] **P1** Add a minimal in-world interaction prompt/status display for target, held object, selected cut action, snap candidate, operation failure and inventory result.
- [ ] **P1** Remove debug-only feedback as the sole indication of a valid interaction; retain debug overlays behind a development toggle.
- [ ] **P1** Ensure a player can understand what is actionable, why an action failed, and what state changed without reading the Output Log.

## 2. Wood/log system — highest-risk gameplay contract

- [ ] **P0** Reconcile the separate geometry contract dimensions: log length fraction (full / 3/4 / 1/2 / 1/4) versus cross-section/processing shape (whole / half-round / quarter / hewn / groove / trough / plank / joint). Never infer one from the other.
- [ ] **P0** Finish a canonical asset catalog with exact source mesh → allowed action → exact output mesh mappings for every used size/variant.
- [ ] **P0** Inspect actual imported FBX dimensions, long axis, pivot/origin, scale, collision, normals, end faces and bottom-height behavior inside UE. Filename-only asset inventory is not enough.
- [ ] **P0** Resolve the known 75 cm, long/short, three-quarter-length and other variant gaps. The existence of an enum or filename is not evidence that the output mapping exists.
- [ ] **P0** Confirm that a split-half piece remains a split-half piece after later processing and cannot silently turn into a straight whole log.
- [ ] **P0** Ensure groove/trough/plank actions are legal only from their intended prerequisite state; test every invalid transition.
- [ ] **P0** Replace the current single action/mesh-swap prototype with a defined interaction model: tool eligibility, hit/range validation, progress, interruption/cancel, animation, feedback and final commit. If the design deliberately uses discrete actions, document that instead of implying physical cutting.
- [ ] **P0** Add authoritative resource yield/cost for felling and each processing action; connect to the inventory owner, not a duplicate counter inside the log actor.
- [ ] **P0** Define the lifecycle for tree → felled tree/stump → generated logs → loose/carrying/placed/attached/processed/destroyed pieces. Prevent duplicate drops or re-harvesting.
- [ ] **P0** Verify carry/drop and placement physics: collision, mass/weight, rotation, surface detection, slope, penetration, fall/settle, out-of-world behavior and collision restoration.
- [ ] **P0** Verify snap transforms on all anchor directions and rotations, including exact anchor reservation, occupied target rejection, attachment failure rollback, detach and parent destruction.
- [ ] **P0** Define whether children can be detached, moved, cut or destroyed and what happens to their descendants.
- [ ] **P1** Integrate stable object IDs, revisions, committed events, structure graph and persistence with processed/carry/placed/snap state.
- [ ] **P1** Add non-ideal cases: same operation replay, stale revision, missing output mesh, invalid target, obstructed placement, actor destroyed during interaction, save while carrying/attached.
- [ ] **P1** Define whether physical geometry is pre-authored variants or runtime-generated geometry; the current prototype swaps imported variants and does not procedurally cut mesh geometry.
- [ ] **P2** End-grain/inner-face materials, cut marks, chips/sawdust, impact sounds, tool animation and final visual polish (after the geometry contract is verified).

## 3. Snap / attachment / structural integrity

- [ ] **P0** Use one shared, stable anchor/connection contract across logs, stones, sticks and building pieces; prevent parallel systems from each owning incompatible attachment truth.
- [ ] **P0** Define anchor schema: stable ID, local transform, accepted connection tags, direction/orientation, capacity, occupancy, support role and detach policy.
- [ ] **P0** Test cycles, duplicate links, self-links, occupied anchors, invalid support ancestry, actor attach failure and cleanup after destruction.
- [ ] **P0** Implement actual structural support/load propagation; a graph reporting “unsupported” does not physically collapse the actor.
- [ ] **P0** Define material-specific strength, span/overhang limits, load distribution, durability, damage thresholds and deterministic collapse ordering.
- [ ] **P1** Connect damage/repair costs to authoritative inventory transactions and events.
- [ ] **P1** Connect mixed wood/stone/stick structures to the same structural graph and persistence protocol.
- [ ] **P1** Add wall/floor/roof/column/opening/bridge support fixtures and test support before/after removing a key piece.
- [ ] **P1** Define physics behavior after graph-driven collapse and ensure no duplicate collision or stale attachment references.
- [ ] **P2** Add readable structural preview, support warnings and material/connection feedback.

## 4. Stone, stick, tools, traps and primitive construction

- [ ] **P0** Populate and verify source-specific stone shape mappings for block/slab/column/lintel/corner; measure real dimensions/pivots/collision.
- [ ] **P0** Connect stone shaping to tools, action progress, resource yield and inventory costs.
- [ ] **P0** Connect stick gathering, short-stick/stake processing and resource yield to world interaction.
- [ ] **P0** Implement actual ground-contact validation rather than trusting a caller-provided boolean; verify stake placement on slopes/uneven ground.
- [ ] **P0** Define tool ownership, durability, breakage, repair, required tool class and invalid-tool feedback.
- [ ] **P1** Implement primitive tool recipes and usable equipment behavior (spear/bow/arrow/torch as intended scope dictates).
- [ ] **P1** Implement trap state machine: blueprint/assembly → placed → grounded/armed → triggered → spent/reset/dismantled; validate trigger conditions and safety.
- [ ] **P1** Implement fences/barriers, storage/racks, simple furniture and garden/animal-support structures that the scope requires.
- [ ] **P1** Implement dismantle/recovery policy, including loss/waste and child-structure handling.
- [ ] **P1** Ensure torch fuel/ignition/extinguish/light/sound lifecycle is saved and cannot burn fuel twice per tick.
- [ ] **P2** Add stone/wood impact feedback, shaping sounds and visual states.

## 5. Inventory, crafting, storage and resource authority

- [ ] **P0** Choose one authoritative resource owner across inventory, transport containers, production, building and world pickups. Phase 11 itself flags potential conflict with InventoryCrafting.
- [ ] **P0** Make world pickup/drop atomic: a resource cannot exist simultaneously in world and inventory or disappear between them.
- [ ] **P0** Define stable item definitions, units, stack compatibility, mass/volume, max stack, durability, quality, spoilage and world actor representation.
- [ ] **P0** Replace the container's provisional “one capacity unit per resource unit” assumption with item-definition volume/mass rules.
- [ ] **P0** Implement production/crafting input reservation and consumption, output/byproduct materialization, cancellation/refund policy and capacity re-check at completion. Queue bookkeeping alone is not a production loop.
- [ ] **P0** Prevent concurrent jobs or duplicate operation retries from spending the same inputs twice.
- [ ] **P1** Add crafting station/session, recipe discovery, alternate recipes, missing ingredient quantities and cancel/collect interactions.
- [ ] **P1** Add quick slots/equipment UI, categories, item details, weight/capacity indicators, transfer splitting/merging and invalid-action feedback.
- [ ] **P1** Integrate tool durability, repairs, protection items, traps and construction recipes.
- [ ] **P1** Add storage/rack/containers with world interaction and persistent contents.
- [ ] **P1** Persist inventory, containers, jobs, reservations and world pickups consistently.
- [ ] **P2** Balance stack sizes, carrying burden and progression; don't use unvalidated tuning as final design.

## 6. Terrain / voxel / landscape / world modification

- [ ] **P0** Choose and document the terrain architecture: Unreal Landscape heightfield, voxel chunks, or hybrid. A layer-policy ledger is not terrain geometry.
- [ ] **P0** Implement actual dig/edit operations that update visible geometry and collision, not only an in-memory cell revision.
- [ ] **P0** Define chunk/cell coordinates, resolution, voxel/layer encoding, edit boundaries, deterministic seed, LOD and ownership.
- [ ] **P0** Rebuild mesh/collision after edits and handle seams across chunk boundaries.
- [ ] **P0** Enforce permitted layers, maximum depth/volume, protected areas, duplicate edits and rollback in the actual terrain backend.
- [ ] **P0** Implement the first useful terrain forms: shallow pit, trench, small underground room/trap, ramp/berm; prevent unsupported giant edits from destroying performance.
- [ ] **P1** Implement persistence of terrain edits and exact geometry reconstruction after reload.
- [ ] **P1** Add terrain stability/collapse policy for underground rooms and tunnels.
- [ ] **P1** Add water/runoff/drainage interactions, wet ground/mud, fire/rot effects only where they connect to gameplay.
- [ ] **P1** Integrate terrain material/soil state with farming and route cost with logistics.
- [ ] **P1** Define road/bridge/rope construction as real world objects and traversal modifiers, not just a route-cost boolean.
- [ ] **P2** Build the larger procedural world generator, biomes, erosion, vegetation distribution, streaming and LOD after the small terrain edit loop works.

## 7. Farming and animal husbandry

- [ ] **P0** Connect crop/animal simulation to actual terrain/soil cells, seasons, weather and shared world clock.
- [ ] **P0** Connect planting, watering, harvesting, feeding and collection to inventory transactions.
- [ ] **P0** Define crop species/data: growth duration, stages, temperature/season window, moisture/nutrients, yield, seed return and failure states.
- [ ] **P0** Define animal species data: habitat/space, feed/water, health, growth, products, collection, reproduction and death rules.
- [ ] **P1** Implement complete reproduction and habitat checks rather than relying solely on caller-supplied capacity.
- [ ] **P1** Make products/manure/soil nutrient changes authoritative and persistent; avoid repeated collection.
- [ ] **P1** Add farm plots, pens, feeding/watering stations and player interactions/UI.
- [ ] **P1** Define weather disaster effects on crops/animals and ensure those effects use the shared clock exactly once.
- [ ] **P1** Verify save/load continuity at growth, harvest, animal product and starvation boundaries.

## 8. Raids, enemy AI and combat

- [ ] **P0** Connect raid scheduling to the authoritative world clock and inject stable world seed/random stream.
- [ ] **P0** Spawn actual enemy actors from validated candidates; population counters must change only after successful spawn/despawn.
- [ ] **P0** Implement navigation/pathfinding, target selection updates, obstruction handling and failed spawn recovery.
- [ ] **P0** Implement combat interactions, enemy damage, player damage/death/respawn or failure policy.
- [ ] **P0** Route enemy attacks through the shared structural damage owner so M097 isn't an isolated side channel.
- [ ] **P1** Define raid escalation/threat inputs, warning window, wave budget, spawn fairness and limits.
- [ ] **P1** Implement raid cancellation/interruption, cleanup, unloaded target behavior and save/load during a wave.
- [ ] **P1** Add readable tells, audio cues, hit feedback, damage numbers/status, and a test fixture with predictable spawn locations.
- [ ] **P1** Verify that raid AI and background simulation stay within performance budgets.

## 9. Logistics, primitive production, food, discovery and survival

- [ ] **P0** Replace the route-cost helper with a route graph/path solver over terrain, roads, bridges and traversal links.
- [ ] **P0** Implement actual transport interaction for carrying/storage/containers; add sled/cart/rope/zipline only as the selected scope demands.
- [ ] **P0** Make production jobs consume/reserve ingredients and create outputs/byproducts transactionally.
- [ ] **P0** Define the authoritative survival writer: Phase 11 `UpdateSurvival` and Phase 13 `SimulateSurvivalStep` must not both mutate the same state.
- [ ] **P0** Use one canonical world clock for weather, food spoilage, farming, production, raids and survival.
- [ ] **P0** Connect hunger/thirst to actual food/water inventory; define consumption, quality, poisoning, nutrition and failed-use behavior.
- [ ] **P1** Implement cooking stations, fuel use, preservation and shelter heat in a coherent resource loop.
- [ ] **P1** Implement illness/injury/poisoning rules, treatment items and recovery/failure states; fields alone are not a system.
- [ ] **P1** Connect clothing/equipment and shelter actors to protection values derived from actual equipped items and built shelter.
- [ ] **P1** Make discoveries persistent and link them to world context/unlocks; a transient record alone does not deliver exploration.
- [ ] **P1** Add ropes/bridges/roads as functional traversable construction and make them affect traversal cost/route selection.
- [ ] **P2** Balance daily/seasonal survival, food quality decay and weather exposure with explicit tuning evidence.

## 10. Environment, weather, disasters and atmosphere

- [ ] **P0** Replace subsystem-local time with the authoritative world clock; test long-world-time and season-boundary cases.
- [ ] **P0** Validate numeric inputs against NaN/infinity/out-of-range values and ensure failed operations do not consume operation IDs or partially mutate state.
- [ ] **P0** Resolve duplicate survival tick risk and define deterministic tick ordering.
- [ ] **P1** Connect weather to sky/light, rain/snow visuals, wind, fog, thunder/lightning and audio.
- [ ] **P1** Add snow accumulation/melt, slippery/wet terrain and environmental traversal impacts if part of the design.
- [ ] **P1** Implement actual disaster events (avalanche/earthquake/major storm) only with bounded affected regions, warning/avoidance logic and recoverable world changes.
- [ ] **P1** Connect weather to crop growth, animal needs, food preservation, fire and shelter.
- [ ] **P1** Persist environment seed, world time, season, weather period and any durable world effects.
- [ ] **P2** Add seasonal foliage, lighting profiles and atmospheric polish after simulation correctness.

## 11. Save/load, deterministic operations and domain events

- [ ] **P0** Implement deterministic serializer/exporter for every authoritative subsystem; Phase 12 currently accepts opaque byte sections.
- [ ] **P0** Implement side-effect-free validation and staged import for all participants before any active world state changes.
- [ ] **P0** Implement cross-section stable-ID/reference validation and all-or-nothing commit/rollback.
- [ ] **P0** Save/load CoreWorld clock/seed, actor identities/lifecycle, log/stone/stick processing and attachment graph, inventory, terrain, crops/animals, raids, production, food, discoveries and environment state.
- [ ] **P0** Add save → destroy/recreate world → load → compare authoritative state round-trip tests.
- [ ] **P0** Add corrupted checksum, duplicate/missing section, unsupported version/schema, truncated/oversized payload and partial-import failure tests.
- [ ] **P1** Define schema migrations, backups/recovery, slot naming, autosave/checkpoint policy and safe-save boundaries.
- [ ] **P1** Ensure held objects, active crafting jobs, raids, mid-growth crops and terrain edits survive reload correctly.
- [ ] **P1** Integrate domain events only after successful commit; document transient notifications versus durable event history.
- [ ] **P1** Define operation idempotency/deduplication semantics. Phase 3 explicitly treats operation IDs as correlation metadata, not deduplication keys.
- [ ] **P1** Test deterministic replay from the same seed and operation sequence.
- [ ] **P2** Add player-facing save/load UI and recovery messaging.

## 12. Vertical-slice content, map and asset integration

- [ ] **P0** Create a small but complete authored test map with ground, spawn, workstations, test fixtures, storage, farm pen, raid perimeter and route between stations.
- [ ] **P0** Confirm all meshes used by the slice are imported, assigned in actor/Blueprint defaults, correctly scaled/oriented and have useful collision.
- [ ] **P0** Do not assume `.fbx` files in Git are already imported and assigned in the Unreal `.uasset` registry.
- [ ] **P0** Keep placeholder geometry simple and consistent; don't add high-poly assets before gameplay geometry/scale is stable.
- [ ] **P1** Create an asset manifest for mesh/material/sound/effect references, import settings, license/provenance and missing-asset report.
- [ ] **P1** Fix fallback ground/light/camera composition and ensure the saved map, not just the generation script, is the reviewed artifact.
- [ ] **P1** Add basic sky/time/weather presentation and interaction feedback.
- [ ] **P2** Add final textures, end-grain, decals, particles, ambient sound, music layers and polished environmental storytelling after core loops are accepted.

## 13. UI, controls, accessibility and feedback

- [ ] **P0** Document one consistent control map for move/look/jump/interact/carry/drop/rotate/place/cut/inventory/craft.
- [ ] **P0** Ensure key bindings are not conflicting between core actions, Cut Mode and UI focus.
- [ ] **P0** Show held state, target name, valid/invalid snap, placement mode and operation failure reason.
- [ ] **P1** Add inventory/crafting UI, quick access, build preview, recipe costs and survival meters.
- [ ] **P1** Add context-sensitive prompts for tool prerequisites, cut prerequisites, ground support and occupied anchors.
- [ ] **P1** Support input cancellation and recovery when a menu opens, target disappears, player is interrupted or the map reloads.
- [ ] **P2** Add remapping, accessibility options, scalable text and configurable feedback intensity.

## 14. Testing, engineering quality and release evidence

- [ ] **P0** Run UE 5.8 UHT and Editor-target C++ build on the actual integration branch.
- [ ] **P0** Resolve compile/UHT/include/reflection errors before interpreting source presence as implementation.
- [ ] **P0** Run focused automated tests for data validation, transitions, geometry mapping, snap rules, transactions, deterministic ticks and persistence.
- [ ] **P0** Run PIE for every core loop and record exact expected/observed results.
- [ ] **P0** Test failure cases and verify no partial state/material mutation.
- [ ] **P0** Audit actual local Blueprint assets/callers in the user's checkout; tracked C++ review cannot cover untracked assets.
- [ ] **P1** Add automation for map generation/import idempotency, missing assets, duplicate IDs, anchor cleanup and save/load.
- [ ] **P1** Capture logs, screenshots/video, engine version, branch/commit SHA, seed and test scope in an evidence bundle.
- [ ] **P1** Run bounded performance tests for physics-heavy logs, structural graph, terrain chunks, farming ticks and raid populations.
- [ ] **P1** Add memory/streaming budgets and verify no actor/timer/delegate leaks after repeated load/reset.
- [ ] **P2** Establish packaging/standalone launch and clean-machine startup checks.

## 15. M001–M119 traceability

- [ ] Map every ID to exact implementation owner, header/cpp/Blueprint call site, and required test.
- [ ] Check each dependency is implemented and tested before dependent mechanics are accepted.
- [ ] For every `save_required` item, link the round-trip case.
- [ ] For every item, include positive, boundary and negative cases where applicable.
- [ ] Distinguish statuses such as **unreviewed**, **source-mapped**, **static-reviewed**, **UE-build-passed**, **PIE-passed**, **save/load-passed**; do not collapse them into “done.”
- [ ] Reconcile the canonical registry with the actual scope and implementation without renumbering IDs.
- [ ] Only change registry statuses when evidence is attached; current audit does not change any status.

## Recommended implementation order (do not treat phases as completion gates)

1. **Gate A — Buildable slice:** UE 5.8 build, saved B map, spawn/input/camera/ground, basic player HUD.
2. **Gate B — Physical log loop:** tree/resource → raw log → valid processing → carry/drop → snap/unsnap, with geometry mappings and failure atomicity.
3. **Gate C — Structure + inventory:** shared anchor graph, support checks, pickup/transfer/craft transactions, one small shelter.
4. **Gate D — Persistence:** one canonical clock/seed, one survival writer, one end-to-end participant adapter, then staged world restore and round-trip tests.
5. **Gate E — Survival route:** weather exposure, food/water transactions, shelter/clothing, one crop and one animal loop.
6. **Gate F — Terrain/raid/production:** actual terrain edits and persistence, production inputs/outputs, enemy spawning/AI/combat/damage.
7. **Gate G — 119-mechanic evidence:** complete row-by-row test matrix, final compact demo, performance and packaging.

## Current conclusion

The highest-risk omission is not a missing phase number; it is the absence of a verified, connected vertical slice and an evidence-backed M001–M119 acceptance pass. Keep the phase PRs separate and unmerged until the intended integration gate. The next work should close the vertical-slice blockers in order, not create more subsystem skeletons and call the project finished.
