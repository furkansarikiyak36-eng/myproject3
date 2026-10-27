# MyProject3 — Phase 00–14 Implementation Roadmap

**Working document branch:** planning/gdd-inventory-production-roadmap  
**Repository:** furkansarikiyak36-eng/myproject3  
**Engine target:** Unreal Engine 5.8  
**Scope:** One audit phase (Phase 00), followed by the existing Phase 01–14 implementation stack  
**Status:** Planning / source audit only. No Unreal build or runtime validation is claimed by this document.

## Phase numbering decision

The repository already has a stacked implementation sequence: Phase 01 foundation, Phase 02 core contracts, Phase 03 wood, Phase 04 stone, Phase 05 sticks, Phase 06 inventory/crafting, and a 14-phase architecture roadmap through Phase 14 integration/persistence/demo. An older foundation document says “16 development phases.” That mismatch is recorded as an unresolved documentation discrepancy in the Phase 00 audit; it is not a reason to renumber M001–M119 or invent two phases.

This plan therefore adds **Phase 00** as a baseline/audit-only step and retains the existing **Phase 01–14** implementation numbering.

## Implementation baseline decision

The current default main branch is the documented legacy log prototype. Repository branch comparisons show that feature/simple-intent is a descendant of both phase-06-inventory-crafting and mechanic-detailing-pass-h-integrated-world-systems. It is the preferred candidate implementation line for review, not a verified product. It is 3 commits behind current main; those newer main commits primarily contain project memory/docs/screenshots, and those changes must be reconciled during integration.

Do not merge Phase 01–06 and Phase H individually into feature/simple-intent; they are already in its ancestry. Do not assume the candidate branch builds or its save/load works simply because source files exist.

## Phase 00 — Baseline audit and scope reconciliation

**Goal:** Establish what exists on main, what exists only on candidate branches, and what has evidence.

- Inspect main README, core log source entry points, build scripts, config and tracked content.
- Compare phase-01-foundation-v2, phase-02-core-contracts through phase-06-inventory-crafting, mechanic-detailing-pass-h, and feature/simple-intent.
- Preserve the canonical M001–M119 IDs. Create a separate baseline matrix that does not overwrite the registry's own verification fields.
- Record the registry discrepancy precisely: the latest registry snapshot contains 90 `implemented_unverified` and 29 `unreviewed` records; all 119 have `static_review=fail` and `runtime_test=not_run`; 90 list `unreal_build=pass` while 29 list `not_run`. The build evidence cited in those rows is a build on `integration/canonical-log-model` (2026-10-26), not proof that the current `feature/simple-intent` HEAD compiles. Here `static_review=fail` means the mechanic's end-to-end acceptance criterion is not met by available evidence; it is not a compiler-error status. The older phase-01-v2 registry uses `unreviewed/not_run`; preserve this discrepancy instead of overwriting one registry's evidence fields. The JSON phase field remains 1 for all entries and is not the later implementation-phase assignment.
- Separate branch history, source presence, static review, automated tests, Unreal build, PIE behavior and save/load evidence.
- Keep this GDD branch documentation-only.

**Deliverables:** Phase 00 audit, branch integration matrix, 119-row traceability baseline, unresolved-discrepancy list.  
**Gate:** there is one recommended implementation line; no mechanic is marked verified without item-specific evidence; registry history/status inconsistencies are visible rather than silently normalized.

## Phase 01 — Shared foundation and canonical scope

**Goal:** Lock stable rules and scope without replacing the existing gameplay owner.

- Maintain the canonical M001–M119 list and schema.
- Define domain boundaries: world/object identity, interactions, resource/material, processing/crafting, construction, terrain, traversal, survival, farming/animals, environment, threat, exploration, persistence.
- Define stable definition IDs versus instance IDs, domain-state ownership, units, operation semantics, event semantics, asset/event naming, and compatibility constraints.
- Preserve the existing ALogBuildPiece and ULogCarryComponent contract until all callers and editor/Blueprint dependencies are reviewed.
- Record which rules are design-only versus code evidence.

**Existing inputs:** phase-01-foundation-v2 and Phase 01 architecture records.  
**Deliverables:** scope registry, source inventory, call-chain map, state-transition map, shared data contract, risks and acceptance rules.  
**Gate:** no duplicate authoritative owner is introduced; the ID list remains stable; all unresolved schema/phase discrepancies are explicit.

## Phase 02 — Core identity, operation and event contracts

**Goal:** Provide shared request/result/identity/event contracts while keeping domain mutations in their existing owners.

- Review candidate GameplayCoreTypes, identity component/subsystem and committed-event subsystem.
- Confirm stable definition and instance IDs, operation GUIDs, expected revisions, explicit unitized resource amounts, result codes and committed event schema.
- Ensure failure does not mutate world state or report success; do not add a universal dispatcher that silently owns every game system.
- Keep transient multicast events separate from durable receipts and save snapshots.
- Resolve generic lifecycle versus existing log held/placed/processing states before adding adapters.

**Existing inputs:** phase-02-core-contracts source/docs, later candidate branch versions and Phase 01 contracts.  
**Gate:** API compatibility and idempotency rules are reviewed; UE 5.8 build/UHT and runtime checks still required.

## Phase 03 — Wood processing and physical log lifecycle

**Goal:** Complete the game’s most important physical-resource loop without discarding the newest placement architecture.

- Review the wood processing changes and the feature/simple-intent cut/placement callers together.
- Define correct transitions for whole log, measured cross-cut pieces, split half-round, quarter, hewn/squared, groove, trough, plank and pole.
- Keep half-length logs distinct from longitudinal split halves.
- Require the correct parent state for each operation; a groove must not be applied to a whole un-split log.
- Add tool/hit/progress feedback later; pre-authored mesh variants are acceptable initially.
- Resolve preview/commit parity, deck path coverage, diagonal intersections, mitre-cut scope, authored notch sockets and per-solve world-scan performance.
- Save partial state and preserve physical ownership.

**Existing inputs:** phase-03-wood-processing plus current feature/simple-intent implementation.  
**Gate:** each allowed transition has a matching state/geometry; invalid transition is rejected atomically; placement preview equals the committed result; engine validation recorded before marking complete.

## Phase 04 — Stone and masonry

- Use a material-specific stone provider on the shared placement kernel; do not fork a second snap system.
- Define staged fracture, chunk yield, stone-piece dimensions, anchor compatibility and mixed wood/stone rules.
- Add wall/corner/column/opening patterns only when the required pieces and support rules exist.
- Defer full realistic rigid-body fracture.
- Acceptance includes fracture-state consistency, supported placements, invalid-anchor rejection, save/load and UE 5.8 checks.

## Phase 05 — Sticks and primitive tools

- Define stick/branch lifecycle, sharpened stakes, simple spear/bow/torch/fence/trap parts where in scope.
- Keep trap arming prerequisites separate from trap effects.
- Reuse item identity, ownership, placement, recipe and feedback contracts.
- Acceptance includes safe pickup/drop/place/processing, failed-operation rollback, and integration with inventory and structures.

## Phase 06 — Inventory, carry, storage and crafting transactions

- Review FInventoryStack, FCraftingRecipe, UInventoryCraftingComponent, item ownership and receipt logic as a single system.
- Keep large logs and boulders as physical world actors; small resources may be stackable.
- Define unique world-item ownership across world, carry, inventory, storage, crafting, drop and placement.
- Validate slot, mass and optional volume constraints; preflight outputs/by-products before consuming inputs.
- Add recipe ID/version, preconditions, tools/stations, outputs/by-products, wear/cost, interruption and rollback behavior.
- Acceptance requires no duplication/loss on failure, and save/load of inventory, ownership and physical items.

## Phase 07 — Shared structural graph, support, damage and openings

- Confirm one authoritative structure graph and avoid parallel parent/child graph owners.
- Define semantic anchor IDs, occupancy, support ancestry, load/span limits and safe removal behavior.
- Add damage/repair, doors/windows/openings, floor/roof, ramps/stairs, bridges and defensive walls in bounded pieces.
- Acceptance requires deterministic support/removal, no orphaned anchor reservations, no unsafe removal of supporting pieces, and persistence of graph links.

## Phase 08 — Terrain state, bounded voxel digging and underground spaces

**Technical decision gate at the start of this phase:** compare UE 5.8-compatible runtime-edit approaches against runtime digging, collision/navigation updates, save/load, material hardness, memory, performance, engine/plugin compatibility, maintenance and licensing. Demonstrate a small soft/hard terrain area before full implementation. No voxel technology is considered selected merely because a candidate header exists.

- Define depth layers and diggable material classes; medium-depth scope only.
- Add pits, short tunnels and small underground rooms, plus bounded support/collapse rules.
- Keep terrain state, rendering/collision adapter, material yield and structural support contracts explicit.
- Save terrain edits deterministically.
- Initial raids do not dig through voxel cells; no unlimited depth or per-voxel full physics.

**Gate:** runtime edit, collision, support interaction and save/load pass in the agreed test region; performance limits are recorded.

## Phase 09 — Farming and animal husbandry

- Implement land preparation, soil/water conditions, sowing, growth, harvest, seed return and a small set of distinct crops.
- Add animal lifecycle/feed/output rules only through the same data/clock/persistence contracts.
- Keep world-clock updates coarse and deterministic; no per-frame crop growth simulation.
- Define season windows and the effect of contamination/restoration on crop viability.
- Acceptance requires valid calendar transitions, understandable failed planting, deterministic results and save/load.

## Phase 10 — Raid director and simple threat AI

- Define threat windows, warning signals, spawn limits, target choice, approach/attack/retreat states and bounded damage.
- Add barriers, simple traps/alarms, repair and recovery.
- Threats initially cannot dig through voxel terrain; no advanced coordinated siege simulation.
- Noise/light/activity modifiers must be explainable and telegraphed.
- Acceptance requires the player can prepare, read warning cues, defend or recover, and continue without unrecoverable loss.

## Phase 11 — Logistics and route movement

- Add load/unload for physical cargo and storage, and rope-line/zipline transportation if its physical constraints can be supported.
- Make route cost and safe shelter choices interact with weather, load and terrain.
- Preserve ownership receipts for cargo transfers; prevent one item from being owned by two containers/actors at once.
- Acceptance includes transfer rollback, capacity handling, route validity, and persistence.

## Phase 12 — Primitive production and queues

- Add only useful, bounded processing chains such as raw wood → prepared piece/plank/fuel, basic cooking/heat and limited production queues.
- Define input/output/by-product, fuel, time, blocked reasons, noise/light and interruption.
- Defer factory-scale automation, ore-to-steel industry and player-built robot production.
- Acceptance requires valid recipe IDs, atomic resource accounting, clear blocked reasons and save/load of queue state.

## Phase 13 — Survival simulation, exploration and regional restoration

- Add weather/season state, temperature/wetness, hunger/thirst, stamina/rest, sleep/fatigue, injury/poisoning, food quality, clothing and shelter protection.
- Add persistent discovery and old-technology repair as exploration context.
- Model world restoration with region-level state (contamination, water/soil quality, vegetation stage and opportunities), not a global simulation of every plant.
- Keep daily weather, seasonal calendar, technology progression and long-term restoration on distinct time scales.
- Acceptance requires readable warnings/countermeasures, deterministic clocks, persistent region/discovery state and meaningful local effects.

## Phase 14 — Integration, persistence and vertical slice

- Review the full save coordinator/snapshot code and restore order; the candidate currently declares save schema version 13, but schema declaration alone is not proof of correct serialization.
- Restore identity, terrain, physical actors, structural graph, inventory/storage, crop/animal state, production, threat, survival, clock/weather and discoveries in a deterministic order.
- Rebuild derived collision/navigation/visual state and validate cross-references before resuming play.
- Build one clean vertical-slice map with ground/light, resource yard, physical structure, small diggable area, one exposure hazard, one crop, one simple threat, one discovery and save/load marker.
- Run source validation, automation, UE 5.8 build/UHT, Editor load, PIE, save/load and bounded performance checks as separate evidence.
- Produce an M001–M119 acceptance map with setup, success path, failure path, retry/idempotency, cross-system effect, persistence and capture/log references.

**Gate:** the integrated slice completes a useful player loop from gathering through processing/building/preparation to a persisted outcome.

## Cross-phase rules

1. The newest feature/simple-intent branch is the current candidate implementation line because it already contains the stacked earlier phase histories. Review it before changing overlapping files.
2. Do not mark a mechanic verified solely because it appears in the registry or a source file exists.
3. Do not merge stacked branches individually into their descendant.
4. Use project-owned placeholder geometry and simple sound/VFX placeholders while contracts and dimensions are changing; final production assets come later.
5. Keep every item, recipe, asset/event ID and mechanic ID traceable to an owning system, source/content path, dependency and acceptance case.
6. Code, static review, automation, Unreal build, PIE and save/load are separate status fields.
7. Any new mechanics beyond M001–M119 need new IDs after duplicate review; never renumber existing IDs.
8. Keep this branch documentation-only. Gameplay implementation belongs in the reviewed implementation branch and focused integration work.

## Current status

- Phase 00: repository/branch audit and 119-row baseline matrix are published; catalogue CI passed for 14 schemas, 119 registry IDs and 45 sample records. Full per-mechanic source review, public API/Blueprint compatibility review and engine validation remain open.
- Phase 01: shared data/lifecycle contracts, definition/event schemas and wood transition contract are published; schema/sample validation is CI-verified. Full cross-reference audit, source/API integration review, runtime catalog-loading decision and UE validation remain open.
- Implementation status: the source layers are present on candidate branches, but this document does not claim the full game is integrated or runtime-verified.
