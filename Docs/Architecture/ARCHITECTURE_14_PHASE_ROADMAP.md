# 14-Phase Architecture Roadmap

**Status:** Architecture baseline / implementation coverage not yet claimed  
**Working branch:** `architecture-14-phase-skeletons`  
**Base:** `phase-06-inventory-crafting`  
**Scope authority:** `MECHANIC_REGISTRY_119.md/.csv/.json`

## Purpose

Create one dependency-aware map for all 14 phases before expanding individual mechanics. Preserve existing phase branches and draft PRs; this branch is stacked on Phase 6 and must not be merged directly to `main` before the final integration gate.

The canonical mechanic IDs M001–M119 are immutable. This roadmap maps ownership and order; it does not rename, delete, or mark registry entries implemented or verified.

## Phase map

| Phase | Domain | Primary registry scope | State at roadmap start |
|---|---|---|---|
| 1 | Foundation / scope registry | M001–M119 registry and architecture records | Baseline documents exist; historical test-list provenance remains limited |
| 2 | Core world contracts | M001–M008 | Initial contracts added; integration/persistence gaps remain |
| 3 | Log and wood processing | M009–M028 | Initial cut-action layer added; remaining mechanics/integrations open |
| 4 | Stone and masonry | M029–M038 | Initial explicit-anchor piece layer added; source review/engine validation open |
| 5 | Stick and primitive tools | M039–M048 | Initial state/anchor/trap layer added; several integrations open |
| 6 | Inventory and crafting | M049–M058 | Initial inventory/recipe transaction layer added; world/UI/persistence integrations open |
| 7 | Shared structure graph, support and damage | M059–M070 | Skeleton to define |
| 8 | Terrain, world state and constrained digging | M071–M080 | Skeleton to define |
| 9 | Farming and animal husbandry | M081–M090 | Skeleton to define |
| 10 | Raid director and threat AI | M091–M100 | Skeleton to define |
| 11 | Logistics, carrying routes and resource movement | M101–M102, with dependencies to production | Skeleton to define; exact cross-domain ownership must remain explicit |
| 12 | Primitive production and production queues | M103–M106 | Skeleton to define |
| 13 | Survival simulation and exploration context (not story production) | M107–M108, M110–M119 | Skeleton to define; M109 is an integration gate owned by Phase 14 |
| 14 | Cross-system integration, persistence and demo coverage | M109 plus integration acceptance across M001–M119 | Skeleton to define |

**Mapping note:** The canonical registry groups M101–M119 under one broad system owner. Phases 11–14 are an implementation/workstream split, not a change to the registry's primary-owner field. Where a mechanic naturally crosses domains, its canonical owner remains unchanged and integration is handled through explicit contracts.

## Dependency order for detailed implementation

1. Core identity, operation results, revisions, committed events, deterministic clock and save boundaries.
2. Physical item states and processing prerequisites: wood, stone, sticks and inventory/crafting transactions.
3. Shared anchors, parent-child graph, support propagation, structural damage and collapse.
4. Terrain edit policy and world-state changes, then farming and animal state.
5. Threat/raid scheduling and its interaction with structures, terrain and player state.
6. Logistics and production queues, then weather/exposure/needs/fatigue/food/clothing/shelter.
7. Integrated save/load, discovery persistence, cross-domain consistency and the final 119-mechanic demo plan.

This is a dependency order, not permission to skip prerequisites or to label a mechanic complete because its class exists.

## Required mechanic record

Every M-ID must eventually have:
- canonical name and owning system;
- explicit prerequisites and configuration/data dependencies;
- authoritative state owner and allowed state transitions;
- success effect and observable result;
- failure conditions and guarantee of no partial mutation;
- cross-system events/contracts and idempotency behavior where applicable;
- persistence schema/version and restore order;
- acceptance criteria and a verification status separate from implementation status.

## Status vocabulary

- `skeleton`: architecture and contracts only.
- `partial`: some source behavior exists; known scope remains.
- `implemented_unreviewed`: source claims the behavior but source review is pending.
- `reviewed_static`: source-level review completed; engine behavior not yet proven.
- `verified_engine`: required Unreal build/UHT/runtime gate passed with recorded evidence.
- `blocked`: dependency, asset, design or tool issue prevents progress.

The registry's current `unreviewed` / `not_run` values must not be silently upgraded. Static source inspection is not an Unreal build, UHT, Editor, PIE or gameplay test.

## Global integration invariants

- Mesh overlap alone never creates a physical construction connection.
- A cut/groove/trough/spike/trap transition requires its exact input state and valid output/configuration.
- Rejected operations leave authoritative state, inventory, anchors and graph relations unchanged.
- Resources cannot be duplicated or consumed twice by retries; operation IDs/revisions must be respected where available.
- Placement preview and committed transform must agree.
- Removing supports must resolve dependent structures or reject safely; no orphaned graph links.
- Terrain edits are limited by explicit editable layers, depth/range and protected-world rules; no unrestricted whole-world voxel digging.
- Threat and production simulation use deterministic world time/seed where applicable.
- Save/load restores identity, terrain, physical graph, inventory, farms, production, threat and survival state coherently.
- Story/cinematic production is out of scope for this architecture pass.

## Exit criteria for this architecture step

- [x] Existing Phase 1–6 scope and known limitations recorded.
- [x] Phase 7–14 skeleton docs created.
- [x] Dependency order and cross-domain invariants stated.
- [ ] Per-mechanic detailed implementation pass completed.
- [ ] Cross-phase source audit completed.
- [ ] Unreal build/UHT/Editor/PIE/gameplay gate: deliberately deferred until user authorizes final validation.
- [ ] Final demo mapped to all 119 mechanics in useful, non-cluttered locations.
