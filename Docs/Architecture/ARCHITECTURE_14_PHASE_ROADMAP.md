# 14-Phase Architecture Roadmap

**Status (2026-10-28):** Architecture baseline plus incremental implementation on `integration/phase-02-14-closure`; source availability is not engine/gameplay acceptance.  
**Primary source baseline for current implementation review:** `mechanic-detailing-pass-h-integrated-world-systems`  
**Scope authority:** `MECHANIC_REGISTRY_119.md/.csv/.json`

## Purpose

Maintain one dependency-aware map for all 14 phases. Phase/pass branches are stacked work, not independent proof of completion. Compare each branch against its intended base before integration; do not merge work branches directly to `main` before the final integration gate.

Existing IDs M001–M119 are immutable; they are the current baseline, not necessarily the lifetime ceiling. Landscape Builder is a cross-cutting terrain-authoring toolchain and does not automatically create gameplay mechanics. If distinct player-facing behaviors are approved after overlap review, append M120+ without renumbering existing IDs. Detailed rules are in `M001_M119_GAP_ANALYSIS_AND_IMPLEMENTATION_PLAN.md` and `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md`.

## Phase map

| Phase | Domain | Primary registry scope | State in current H source baseline |
|---|---|---|---|
| 1 | Foundation / scope registry | M001–M119 | Baseline documents exist; historic test-list provenance remains limited |
| 2 | Core world contracts | M001–M008 | Contracts exist; integration/persistence gaps remain |
| 3 | Log and wood processing | M009–M028 | Initial cut-action/identity layer exists; physical output gaps remain |
| 4 | Stone and masonry | M029–M038 | Piece/anchor layer exists; full gameplay loop and engine validation remain |
| 5 | Stick and primitive tools | M039–M048 | Initial state/anchor/trap layer exists; integrations remain |
| 6 | Inventory and crafting | M049–M058 | Logical inventory/recipe layer exists; actor/UI/persistence gaps remain |
| 7 | Shared structure graph, support and damage | M059–M070 | Graph foundation/adapters exist; multi-support, full collapse and persistence remain |
| 8 | Terrain, Landscape Builder and constrained digging | M071–M080 | Logical cells + bounded voxel chunk API + v14 density save/restore source exist; player caller, atomic logical/physical edit, Landscape seams, navigation, async mesh and UE validation remain |
| 9 | Farming and animal husbandry | M081–M090 | Logical simulation exists; actors, real inputs/outputs and habitat integration remain |
| 10 | Raid director and threat AI | M091–M100 | Scheduling/adapter layers exist; full target selection, navigation and attack AI remain |
| 11 | Logistics, carrying routes and resource movement | M101–M102 | Logical route/cargo layers exist; physical transport and terrain-aware routes remain |
| 12 | Primitive production and production queues | M103–M106 | Logical station/queue layer exists; real machine/fuel/physical-output integration remains |
| 13 | Survival simulation and exploration context (not story production) | M107–M108, M110–M119 | Logical state exists; equipment, food/fire/sleep and world-facing interactions remain |
| 14 | Cross-system integration, persistence and demo coverage | M109 plus integration acceptance across M001–M119 | Partial logical save/restore exists; physical world restore and demo acceptance remain |

The canonical registry groups M101–M119 under one broad owner. Phases 11–14 are an implementation/workstream split and do not change canonical ownership.

## Dependency order

1. Core identity, operation results, revisions, committed events, deterministic clock and save boundaries.
2. Physical item states and wood/stone/stick/inventory transactions.
3. Anchors, structural graph, support propagation, damage and collapse.
4. **Landscape Builder authoring pipeline**, then bounded terrain edits: editor-generated base surface → designated excavation pockets → physical geometry/collision → navigation/support notifications → persistence.
5. Farming and animal state.
6. Threat/raid scheduling and interaction with structures, terrain and player state.
7. Logistics and production queues, then survival systems.
8. Integrated save/load, discovery persistence, cross-domain consistency and final 119-mechanic demo.

Detailed plans: `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md` (terrain authoring) and `M001_M119_GAP_ANALYSIS_AND_IMPLEMENTATION_PLAN.md` (scope, source gaps and dependency-ordered closure).

## Landscape Builder scope

Use UE 5.8's native Landscape system and Edit Layers for the outdoor heightfield. Add a project-owned, deterministic profile-driven editor workflow to generate/import base terrain, define macroforms and protected gameplay masks, and record a seed/version/checksum manifest. Use splines, patches/brushes and PCG only where supported by the installed UE 5.8 version and useful for the project.

Do not duplicate the full native Landscape editor. Do not treat a heightfield as an arbitrary cave/tunnel volume. M071–M076 digging remains a separate bounded 3D pocket adapter driven by the authoritative terrain-state subsystem. No new M-IDs.

## Scope extension policy

- Keep every existing canonical ID and its meaning unchanged.
- Refine already-covered behavior as child requirements (for example `M072-R01`) rather than adding duplicate IDs.
- Register truly new gameplay behaviors only as M120+ after overlap review; update registry/schema/CSV/JSON/Markdown, dependencies, phase mapping and demo coverage together.
- Track Landscape authoring operations such as heightmap generation/import, macroform profiles, erosion settings, biome/slope masks, spline authoring, PCG dressing and seed/checksum manifests as LB tooling deliverables, unless they create an explicit player-facing gameplay rule.

## Required mechanic record

Every M-ID must record canonical name/owner, prerequisites/configuration, authoritative state owner/transitions, success effect, exact physical/logical output, failure/no-partial-mutation behavior, cross-system contracts, retry/idempotency, persistence schema/restore order, player-facing feedback, step-by-step acceptance evidence and verification status. Use child requirement keys such as `M019-R01` to refine criteria without adding or renumbering canonical M-IDs.

## Status vocabulary

- `skeleton`: architecture/contracts only.
- `partial`: some source behavior exists; scope remains.
- `implemented_unreviewed`: source claims behavior; review pending.
- `reviewed_static`: source review complete; engine behavior unproven.
- `verified_engine`: required Unreal build/UHT/runtime gate passed with evidence.
- `blocked`: dependency, asset, design or tool issue prevents progress.

Do not silently upgrade registry `unreviewed`/`not_run` values. Static source inspection is not an Unreal build, UHT, Editor, PIE or gameplay test.

## Global invariants

- Mesh overlap alone never creates a physical construction connection.
- Cut/groove/trough/spike/trap transitions require exact input state and valid output/configuration.
- Rejected operations leave authoritative state, inventory, anchors, graph relations and terrain unchanged.
- Retries cannot duplicate resources or edits; respect operation IDs/revisions.
- Placement preview and committed transform agree.
- Removing supports resolves dependents or rejects safely.
- Terrain edits obey explicit editable layers, depth/range, region masks and protected-world rules; no unrestricted whole-world voxel digging.
- Landscape generation is editor-side/reproducible; runtime excavation is limited to designated pockets with bounded geometry/collision work.
- Restore base Landscape before excavation chunks and dependent structures/navigation.
- Threat/production use deterministic world time/seed where applicable.
- Save/load restores identity, terrain, physical graph, inventory, farms, production, threat and survival coherently.
- Story/cinematic production remains out of scope for this architecture pass.

## Exit criteria

- [x] M001–M119 scope and cross-domain invariants recorded.
- [x] Dependency order and verification vocabulary stated.
- [x] Landscape Builder concept documented separately.
- [ ] Per-mechanic detailed implementation and cross-phase source audit completed.
- [ ] Landscape generation and bounded excavation implemented.
- [ ] Unreal build/UHT/Editor/PIE/gameplay gate (deferred until final validation is authorized).
- [ ] Final demo covers all 119 mechanics in useful, non-cluttered locations with evidence.
