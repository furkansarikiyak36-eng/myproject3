# MyProject3 — Phased Implementation Plan

**Branch:** planning/gdd-inventory-production-roadmap  
**Base:** main (planning/documentation only)  
**Engine target:** Unreal Engine 5.8  
**Rule:** This branch defines order and contracts; it does not claim that gameplay has been implemented, compiled, or tested.

## Repository reality to account for

The checked main branch contains the first log-carry/snap prototype. Repo memory states that newer placement work exists on feature/global-snap-kernel, feature/contextual-interaction-resolver, and especially feature/simple-intent. The newer work must be reviewed and integrated deliberately; do not copy old code over it or assume it is already merged. Main is not the code baseline for declaring the latest snap work complete.

No branch in this plan should modify or merge unrelated project history. Implement each phase on a focused branch and integrate through review.

## Phase 0 — Baseline, inventory, and scope lock

**Purpose:** Establish what exists, where it exists, and what this project is trying to build.

- Read memory/STATE.md, newest session notes, decisions, and module docs.
- Inspect current main and relevant snap branches; record base SHAs and branch relationships.
- Inventory current C++ modules, config files, Unreal maps, scripts, content, and existing acceptance documents.
- Map M001–M119 to systems and mark each as implemented, partial, design-only, missing, or deferred only with evidence.
- Create a source-of-truth register. Keep this GDD as target design; keep code audit as current-state evidence.

**Deliverables:** baseline report, mechanic traceability matrix, unresolved-decision list, branch integration map.  
**Gate:** no feature is labelled complete from file existence alone; incompatible existing implementations are identified before new code is written.

## Phase 1 — Contracts and data models

**Purpose:** Define shared identifiers and system contracts before expanding code.

- Define stable IDs and data schemas for items, recipes, materials, tools, build parts, anchor profiles, crop definitions, region states, audio/VFX events, and persistence.
- Define ownership transitions for world actor ↔ carried actor ↔ inventory item ↔ placed structure.
- Define processing-state transition graphs for wood and staged fracture for stone.
- Define unit conventions, local axes, pivot rules, dimensions, collision expectations, and save versions.
- Define events for interaction accepted/rejected/committed/failed.

**Deliverables:** schemas, ID registry, transition tables, event catalogues, interface contracts.  
**Gate:** no system invents its own item identity, ownership rule, units, or duplicate save representation.

## Phase 2 — Voxel technology decision spike (required decision, execution later)

**Purpose:** Avoid committing to a terrain implementation that cannot meet runtime-edit, save, and performance requirements.

- Compare viable UE 5.8-compatible approaches and plugins against runtime digging, tunnels, persistence, collision, navigation, material types, licensing/maintenance, memory, multiplayer implications if any, and integration with construction.
- Build a bounded test area with one soft material and one hard material.
- Demonstrate dig/undo or deterministic edit, save/load of terrain changes, collision/navigation update, and coexistence with a placed support.
- Measure edit cost and memory at the proposed chunk/region bounds.
- Choose and document the implementation. Do not design around unlimited depth.

**Deliverables:** architecture decision record, prototype, limits and fallback plan.  
**Gate:** do not build all underground systems until this passes. This is a technical compatibility gate, not a claim that a plugin or prototype has already been validated.

## Phase 3 — Wood lifecycle and snap integration

**Purpose:** Consolidate the most important existing mechanic without restarting it.

- Reconcile feature/global-snap-kernel, feature/contextual-interaction-resolver, and feature/simple-intent with the current baseline.
- Confirm one material-neutral placement kernel; material providers own anchor/compatibility rules and data profiles hold dimensions/spacing.
- Resolve known risks: preview/commit parity, deck-placement path, global world scans in solve, long-file responsibilities, mirrored diagonal intersections, missing mitre end states, and authored NotchCradle sockets.
- Formalize whole log, half-length, split half-round, quarter-round, grooved, hollowed, hewn, plank, and pole state transitions.
- Ensure cut location, progress, valid parent state, and output geometry remain consistent. The system may use pre-authored mesh variants initially.
- Persist partial state and structural parent/child links.

**Deliverables:** one accepted wood lifecycle, provider contract, profile definitions, explicit known limitations.  
**Gate:** no illegal state transition; preview solution equals committed solution; failed placement/cut is atomic; carried/placed ownership and save state are stable.

## Phase 4 — Shared structural graph and build parts

**Purpose:** Expand physical construction without duplicating the placement system.

- Add build-part definitions for support, beam, floor/deck, wall, roof, ramp/stair, door, bridge and simple defense.
- Add material providers (wood first; stone/stick later) using the same kernel and profile schema.
- Define anchors by semantic feature IDs rather than mesh-array indices.
- Add bounded structural support rules only to explicitly supported piece types.
- Add repair/dismantle and parent-child/occupancy behavior.
- Ensure placed structures can coexist with terrain edits and save/load.

**Deliverables:** shared build-part schema, providers, example structure recipes, structural graph rules.  
**Gate:** compatible placement commits exactly the preview; invalid placements consume nothing; removing a parent has defined behavior for children; saves recover the same graph.

## Phase 5 — Inventory, carry, drop, and crafting transactions

**Purpose:** Implement one consistent ownership and resource economy.

- Implement item IDs, stack rules, mass/volume, equipment slots, durability/charges, and world representation.
- Keep large logs/boulders physical. Small resources may stack as inventory entries.
- Make pickup/drop/stack/split/equip/consume/craft atomic and idempotent.
- Define recipes as data with inputs, preconditions, durations, outputs, by-products, tool wear and interruption semantics.
- Add minimum UI feedback for capacity, state, required tool, invalid state, and recipe result.
- Persist inventories, carried item, dropped actors, in-progress operation and recipe version.

**Deliverables:** inventory runtime, data catalogue, initial recipes, save migration rules.  
**Gate:** no item duplication/loss on failed operations; every active recipe references valid item IDs; world and inventory ownership is unique and save/load-safe.

## Phase 6 — Resource sources and transformation workflows

**Purpose:** Make the world yield resources through readable state transitions.

- Build wood processing, staged rock fracture, soil/sand/gravel/clay gathering and basic plant-fiber collection.
- Define yields by source type and state; avoid one inventory item per voxel.
- Add simple work surfaces/stations only where they change outputs or safety.
- Ensure each material has a source, acquisition operation, at least one payoff, and a competing use.
- Add tool requirements and condition feedback; do not make tools generic unlock keys.

**Deliverables:** source actors, process definitions, yields, action feedback, initial resource economy.  
**Gate:** each active resource feeds a useful recipe or progression path; no dead-end materials without a documented later phase.

## Phase 7 — Voxel terrain and underground spaces

**Purpose:** Add the bounded digging system after the technology gate.

- Add diggable hardness/material definitions, depth and region bounds, edit persistence and collision updates.
- Define excavation outputs and how edited cells map to material yield.
- Add simple underground room/tunnel rules and limited support placement.
- Create a small set of support-risk cases; no unlimited physically simulated collapse.
- Establish how placed structures, foundations, water/contamination, and voxel edits interact.
- Initially, threats do not dig, and enemies do not cause unlimited terrain destruction.

**Deliverables:** bounded terrain system, excavation/resource hooks, support pieces, save/load path.  
**Gate:** edits persist; invalid operations are rejected cleanly; a supported small underground space can be created; performance budget is met in the agreed test area.

## Phase 8 — Survival, equipment, weather, and time

**Purpose:** Make travel and preparation meaningful.

- Define exposure types, thresholds, rate rules, countermeasures and warning states.
- Add respirator/filter life and protective equipment as clear data-driven items.
- Implement weather states that change travel, visibility, sound, exposure or resource risk only where intended.
- Separate daily weather/events, seasonal calendar, tech progression and long-term restoration.
- Add a small number of weather events first; each must change a decision, not just visuals.
- Define rest/sleep and time advancement without allowing repeated exploit loops.

**Deliverables:** exposure model, weather state machine, protection data, warning UI/audio contracts.  
**Gate:** hazards communicate danger before severe consequences where appropriate; equipment changes outcomes; weather/clock saves consistently.

## Phase 9 — Farming and seasonal material cycles

**Purpose:** Make cultivation a series of visible transformations.

- Implement soil preparation, sowing, growth stages, watering, harvest, seed return and failure/dormancy as data.
- Define crop-specific calendar windows and conditions.
- Use a small first crop set with distinct gameplay functions.
- Define how contamination and regional restoration affect soil and crop yield.
- Use coarse time-step growth; do not simulate every plant every frame.
- Ensure harvest choices compete with seed retention, food and material needs.

**Deliverables:** crop definitions, plot state, growth scheduler, harvest outputs and UI feedback.  
**Gate:** crop state and calendar persist; valid/invalid planting conditions are understandable; growth is deterministic enough for save/load.

## Phase 10 — Simple raids and base defense

**Purpose:** Test preparation and shelter decisions without high AI complexity.

- Define threat triggers and warning windows.
- Implement simple approach/attack/retreat state machine, a small number of target rules, and clear telegraphing.
- Add barricade, alarm, simple trap, repair, and safe escape options.
- Define consistent damage targets and recovery costs.
- Limit destructive reach; raiders do not voxel-dig in the first scope.
- Noise/light/activity may influence risk only with clear, explainable rules.

**Deliverables:** one end-to-end raid case, defense interactions, damage/repair rules and audio/UI signals.  
**Gate:** a player can prepare, understand why an attack happened, defend or recover, and continue without unrecoverable progress loss.

## Phase 11 — Exploration, story, old technology, and regional restoration

**Purpose:** Connect survival progression to the catastrophe mystery and recovery of the world.

- Define region data: contamination, soil/water quality, vegetation stage, threat level, points of interest and unlocks.
- Add inspectable records, old-world devices and environmental clues.
- Add salvage/repair progression before complex industrial production.
- Make regional recovery change selected resources, route conditions, ambience and opportunities.
- Do not let a few planted seeds globally fix the atmosphere.
- Keep advanced fungal humanoids/spore systems and deeper robot/industry mechanics deferred until the base loop is stable.

**Deliverables:** region state model, discovery records, old-tech repair path, restoration milestone rules.  
**Gate:** region states persist and produce visible/functional consequences; story findings connect to world systems without requiring long cinematics.

## Phase 12 — Animation, VFX, SFX, music, and UI integration

**Purpose:** Give every accepted mechanic clear feedback and the world a cohesive atmosphere.

- Implement feedback events alongside mechanics, then replace placeholders in a dedicated polish pass.
- Create required events from ASSET_VFX_AUDIO_CATALOG.md; every effect/sound has one stable ID and a trigger contract.
- Prioritize state clarity: valid/invalid placement, cut progress, fracture stage, exposure, weather warning, raid warning, crop state, restoration.
- Use spatial 3D audio for world sources, restrained UI sounds for menus, and layered/adaptive music states.
- Avoid animation success cues when the authoritative action failed.
- Add accessibility settings for ambience, SFX, music, warning mix, and visual intensity where relevant.

**Deliverables:** event registry, placeholder pass, final cue map, mix buses and feedback checklist.  
**Gate:** major state changes are perceivable without relying on music alone; cue concurrency and performance are controlled.

## Phase 13 — Vertical slice assembly and integration hardening

**Purpose:** Prove one integrated piece of the intended game.

- Assemble a bounded test map with simple lighting/ground, representative resource sources, one structure, a small diggable terrain zone, one hazard, one crop, one threat case, and a discovery point.
- Keep the level free of unrelated asset galleries and decorative clutter.
- Confirm configuration/data IDs, input mapping, save versioning, respawn/recovery, and error messages.
- Run compile, automated tests, editor load, PIE, save/load, and bounded performance checks on the actual UE 5.8 target. Record logs and repro steps.
- Compare results to acceptance criteria; do not infer in-game behavior from a successful Python/static check.

**Deliverables:** one reproducible vertical slice, verification evidence, bug list and next iteration plan.  
**Gate:** cross-system loop works from first resource gathering through building/preparation to a persisted outcome.

## Phase 14 — Content expansion and release preparation

**Purpose:** Expand only after the vertical slice is stable.

- Expand region variety, recipes, crops, weather cases, points of interest, and old-tech devices using existing schemas.
- Add creature/spore/advanced raid types only after threat and performance budgets are established.
- Finalize art/audio, optimization, accessibility, input remapping, settings, crash recovery and user documentation.
- Create a regression matrix for all M001–M119 mechanics and map each mechanic to source files, content, dependencies, and evidence.

**Deliverables:** content backlog, regression matrix, optimized build and release checklist.  
**Gate:** no feature is declared complete without a tested acceptance path and traceable evidence.

## Parallel work rules

- GDD, schemas, asset/event catalogues, and implementation task breakdown can be authored in parallel.
- The voxel compatibility spike is a critical decision dependency before full voxel production.
- Wood state contracts, item IDs, and ownership rules must agree before inventory/crafting integration.
- VFX/SFX identifiers can be catalogued early, but final production assets wait until event timing/state transitions stabilize.
- Do not run multiple agents editing the same source file or public interface without a declared owner and integration plan.

## Current branch deliverable

This planning branch adds and maintains documents only. It does not change gameplay C++, Unreal maps, plugin dependencies, or imported asset content. Implementation branches are created per phase after the source baseline and contracts are accepted.
