# MyProject3 — Phase 00–14 Implementation Roadmap

**Working document branch:** `planning/gdd-inventory-production-roadmap`  
**Current default-branch snapshot:** `main` at `65faad8602d4e14e894dc2cf4e3d507ca39cbb1d`  
**Engine target:** Unreal Engine 5.8  
**Status:** Planning and source reconciliation; no new UE build/runtime claim.

This roadmap is deliberately aligned with the repository's existing `Docs/Architecture/ARCHITECTURE_14_PHASE_ROADMAP.md` and stacked phase branches. Phase 00 is a new preflight. It does **not** renumber or replace the existing Phase 1–14 mechanic work. The canonical M001–M119 IDs remain unchanged.

## Non-negotiable rules

- The current code and content on the chosen integration base are the truth about implementation; GDD/catalogue files define intended design.
- Reuse existing phase branches and core contracts. Do not recreate item ownership, identity, world clock, event dispatcher, save coordinator, structural graph or placement kernel just because their code is not on `main`.
- `feature/simple-intent` is a large audit candidate, not an automatically approved merge base. It has 696 commits ahead / 3 behind `main` and many changed paths; review before integration.
- A branch-level build record is not evidence that this planning branch or a future merged candidate compiled.
- No feature is complete because a class, schema, sample, map or registry row exists.
- Audio, VFX, animation and UI feedback are cross-cutting and should follow accepted gameplay events; final polish belongs after state rules stabilize.

## Phase 00 — Repository baseline and branch reconciliation

**Goal:** Know exactly which code/content exists, where it exists, and what can safely be reused.

- Record `main` HEAD and compare the relevant branch histories.
- Preserve the canonical M001–M119 registry; do not renumber or infer completion from presence in the registry.
- Map each mechanic to candidate code symbols, caller paths, data/config, known gaps, and verification evidence.
- Review the current main-only commits against the broad feature branch before choosing a merge base.
- Identify conflicting systems, especially competing snap kernels, duplicate ownership authorities, parallel event/persistence systems, and similarly named but distinct fields.

**Current deliverables:** repository baseline, branch integration matrix, registry and traceability worksheet.  
**Still required:** exact per-mechanic source/caller review, final source base decision, and target UE integration verification.  
**Exit gate:** every M-ID has an evidence-backed status and next action; the source base is chosen intentionally. This is not a gameplay implementation phase.

## Phase 01 — Shared data contracts and source reconciliation

**Goal:** Establish one shared vocabulary for IDs, item/material/recipe definitions, physical state, ownership, operations, anchors, feedback, and persistence.

- Reuse the existing Phase 1 foundation documents and the candidate Phase 2 C++ contract layer; do not create duplicate runtime owners on the documentation branch.
- Keep mechanic IDs, definition IDs, instance GUIDs, operation IDs, recipe/process IDs, anchor feature IDs, and feedback cue IDs separate.
- Specify item, material, recipe, process-transition, build-part, crop, region, weather and feedback-event schemas.
- Keep inventory stacks distinct from unique physical world items and enforce exclusive ownership transitions.
- Separate a log's length/geometry/processing state from its held/placed/structural state. A half-length log is not the same as a split half-round.
- Define operation success/failure, rollback, idempotent retry, event timing and save migration rules.
- Validate schemas and representative samples; reconcile schema fields with existing C++/config before promoting them to runtime DataAssets/DataTables/JSON.

**Current deliverables:** shared-contract reconciliation, contract register, design-time schemas and sample definitions.  
**Still required:** automated JSON Schema validation, sample/reference validation, per-M traceability closeout, and an explicit runtime data-storage decision.  
**Exit gate:** no duplicated authority, all active IDs/references resolve, invalid operations have deterministic no-partial-mutation behavior, and migrations are defined before persistent fields change.

## Phase 02 — Core world contracts (M001–M008)

**Reuse candidate files:** `GameplayCoreTypes.*`, `WorldObjectIdentityComponent.*`, `WorldObjectIdentitySubsystem.*`, `WorldCommittedEventSubsystem.*`, plus candidate operation-ledger, clock and save contracts where present.

- Review stable operation result/request types, identity registration, revisions, committed event semantics, deterministic world time and persistence boundary.
- Confirm identity/lifecycle does not replace log, crop, animal, inventory or terrain domain state.
- Treat the operation ledger as retry/receipt support, not proof that arbitrary multi-system changes are atomic.
- Treat the current committed-event subsystem as transient notification unless a durable event log is separately designed.
- Add no duplicate global dispatcher or save coordinator.

**Exit gate:** source/caller review passes, idempotency and failure paths are explicit, and engine validation is separately recorded.

## Phase 03 — Log and wood processing (M009–M028)

- Review the existing `ALogBuildPiece` / `ULogCarryComponent` lifecycle and the newer placement-kernel/provider line before changing either.
- Reconcile cut/split/quarter/hewn/groove/trough/plank/notch/spike actions with exact valid parent states and output variants.
- Keep full-log length, split-half-round, quarter piece, plank and pole geometry distinct.
- Ensure custom anchors survive mesh changes; semantic anchor IDs are stable; child/parent links and reservations commit only after attachment succeeds.
- Resolve known snap issues: preview/commit parity, deck placement, diagonal intersections, mitre cut states, manually authored NotchCradle sockets, and solve-time full-world scans.
- Keep actions atomic; prevent invalid state transitions and protect partially processed state on save/load.

**Exit gate:** each M009–M028 mechanic has its own accepted path/failure path; existing carry/place behavior and saved state remain coherent.

## Phase 04 — Stone and masonry (M029–M038)

- Reuse the existing stone piece/anchor layer if the chosen code base confirms it.
- Define stone shapes, staged fracture, placement compatibility, support and mixed-material connections through shared contracts.
- Distinguish a stone actor's local held/placed state from the shared structure graph and inventory ownership.
- Defer full physical simulation of each stone to a measured need; start with deterministic fracture stages.

**Exit gate:** stones can be acquired/shaped/placed with valid anchor/support rules; removal, damage and resource recovery are defined and atomic.

## Phase 05 — Stick and primitive tools (M039–M048)

- Reuse the existing stick actor/state/anchor layer.
- Define gathering, sharpening, stakes, primitive tools/light, simple barriers/traps/racks and disassembly.
- Make grounded-spike and trap arming/trigger/reset preconditions explicit.
- Share tool, inventory, lighting and structure contracts; do not duplicate ownership or snap logic.

**Exit gate:** required states and trap prerequisites behave deterministically; inventory and connected structures cannot duplicate or lose resources on failure.

## Phase 06 — Inventory and crafting (M049–M058)

- Reuse the current candidate inventory stack/recipe types and transaction layer after source review.
- Define stack limits, mass/volume, equipment/quick access, physical actor identity, transfer/split/merge, and crafting transaction behavior.
- Preserve the distinction between small stackable resources and large world actors such as whole logs and boulders.
- Provide capacity checks, missing-ingredient reasons, cancel/rollback behavior, and a clear world-drop/pickup ownership path.
- Add persistence only through the chosen save authority.

**Exit gate:** failed pickup, transfer, craft, consume, drop or place leaves state unchanged; recipe IDs and outputs resolve; save/load is covered by an explicit contract.

## Phase 07 — Shared structural graph, support and damage (M059–M070)

- Unify log, stone and stick connections behind one authoritative graph and shared placement contract.
- Represent connections with stable semantic anchor IDs, not array indices or mesh overlap.
- Define ground-to-load support, span/overhang limits, damage ownership, repair, dismantle and deterministic dependent-piece removal/collapse.
- Keep physics/visual collision as adapters; never let an adapter silently become the graph authority.

**Exit gate:** attach/detach/support/damage/repair/recovery update the graph and physical actors consistently; failure does not leave orphan links or duplicate salvage.

## Phase 08 — Terrain, world state and constrained digging (M071–M080)

**Critical technology gate before full implementation:** choose and validate a UE 5.8-compatible approach for runtime edits, terrain/chunk bounds, collision/navigation updates, save/load, memory, and interaction with supports/structures. Do not assume the presence of a logical terrain-state class means runtime voxel rendering is finished.

- Target medium-depth voxel gameplay: editable layers, bounded depth, pits, horizontal tunnels and small rooms.
- Keep per-cell edits, excavation yields, support invalidation, water/contamination and collision/navigation updates in a defined commit path.
- Distinguish terrain-state data from renderer/collision/navigation adapters.
- Do not let threats dig through terrain in the first scope; do not implement unbounded whole-world deformation.
- Defer to measured limits rather than selecting a technology based on visual quality alone.

**Exit gate:** a bounded test area can be dug, saved/loaded, and safely coexist with a structure; performance and edit limits are recorded.

## Phase 09 — Farming and animal husbandry (M081–M090)

- Define soil/plot states, seed/plant stages, watering, seasonal windows, harvest/by-products and seed return.
- Define animal needs, habitat, care, growth, outputs and reproduction with bounded update steps.
- Use the shared deterministic clock; frame rate cannot alter crop/animal output.
- Preflight output capacity and input consumption before advancing a crop/animal state.
- Connect contamination and restoration where specified; do not make a planting action instantly fix global atmosphere.

**Exit gate:** crop and animal state, production timers and inventory transactions survive save/load without duplicate harvests/products.

## Phase 10 — Raid director and threat AI (M091–M100)

- Keep raids simple but meaningful: clear warning windows, bounded spawn budgets, explicit target priorities, simple approach/attack/retreat logic, traps, barriers, repair and recovery.
- Apply structure damage through the shared damage path; do not let threat code bypass terrain or resource ownership.
- Initially, enemies do not excavate voxel terrain.
- Preserve opportunities to prepare and recover; failures should impose understandable costs rather than casually erase hours of progress.

**Exit gate:** deterministic threat schedule, bounded agent count, readable cause/effect, no duplicate waves/damage on retry or load, and a recoverable failed raid path.

## Phase 11 — Logistics and resource movement (M101–M102)

- Separate hauling/route queries from item definition, storage ownership, and production.
- Define source quantity, destination capacity, transfer operation ID and blocked reasons.
- Keep physical cargo distinct from abstract inventory/stack transfer.
- Use route cost from valid terrain and infrastructure (roads, bridges, ropes, zipline) without making logistics own those structures.

**Exit gate:** failed transfers leave source/destination unchanged; route and carried-item state restore safely.

## Phase 12 — Primitive production and queues (M103–M106)

- Build small, meaningful transformations: basic saw/production chain, queue state, fuel/heat/cooking, road/bridge/rope benefits.
- Keep inputs, outputs, by-products, station tags, duration and fuel data-driven.
- Advance queues using the shared clock and expose blocked reasons.
- Do not introduce large factories, vehicle/machine fleets, or player-built robots into the first scope.

**Exit gate:** queue start/finish is atomic, blocked jobs consume no unintended resources/time, and output capacity/save-load are tested.

## Phase 13 — Survival simulation and exploration context (M107–M108, M110–M119)

- Coordinate warmth, hunger/thirst, stamina, fatigue, injury/illness/poisoning, food quality, clothing, shelter and exposure through one authoritative resolver.
- Define daily weather, seasonal calendar, technology progression and long-term regional recovery as distinct time scales.
- Use readable preparation: respirator/filter, protected spaces, shelter, routes and supplies.
- Keep exploration discoveries persistent and attach them to context/unlocks without requiring cinematics.
- Regional restoration may alter soil/water quality, vegetation stage, resource availability, local hazards and ambience. These are GDD additions: cross-reference them to the registry and add approved new IDs only after overlap review; never silently renumber M001–M119.
- Defer fungal humanoids/spore threats and advanced robot/industry systems until the core world loop is stable.

**Exit gate:** hazards are understandable and passable through preparation; time-driven state advances once; discoveries/restoration persist and produce functional as well as visual consequences.

## Phase 14 — Cross-system integration, persistence and demo (M109 plus M001–M119 coverage)

- Define ordered save/restore across stable IDs, terrain, physical items, structure graph, inventory/containers, farm/animals, logistics/production, raids, survival and discoveries.
- Rebuild derived visual/collision/navigation state and validate references before simulation resumes.
- Build a useful, uncluttered demo with distinct zones for resources, construction, bounded terrain, farmstead, raids, logistics/production and survival/exploration.
- Test valid actions, invalid actions, retries, cross-system consequences, save/load and recovery for each mechanic.
- Record exact UE 5.8 build/UHT result, Editor/PIE reproductions, automated results, logs and relevant screenshots. Static checks do not substitute for in-engine behavior.

**Exit gate:** integrated vertical slice runs through a complete player loop and every M001–M119 has evidence or an explicit blocked/deferred decision.

## Audio, VFX, animation and UI — cross-cutting track

Every phase defines the logical event IDs its actions emit. Start with small placeholders; finalize timing, mixing and animation only after state transitions stabilize. Spatial SFX and ambience communicate the world; UI cues communicate status; threat/exposure warnings cannot depend on music alone. Final audio/VFX polish, accessibility, performance and content expansion are part of Phase 14.

## Status vocabulary

- `planned`: agreed target, not yet implemented.
- `source_present_unreviewed`: candidate code found; exact behavior/call path not reviewed.
- `reviewed_static`: code/source review done; not engine proof.
- `build_verified`: named branch/build/engine result recorded.
- `runtime_verified`: acceptance scenario run in the target editor/game and evidence recorded.
- `blocked` / `deferred`: reason and next condition documented.

The canonical registry currently records historical candidate build/test information. Never copy its status onto a different branch without re-running or explicitly citing the exact evidence source.
