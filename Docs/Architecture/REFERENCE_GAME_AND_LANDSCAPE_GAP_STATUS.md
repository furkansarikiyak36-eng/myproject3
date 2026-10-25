# Reference Game Gaps and Landscape Builder Status — 2.1–2.9 / LB0–LB4

**Project:** MyProject3  
**Review branch:** `audit/reference-game-and-landscape-status`  
**Source baseline:** H-integrated systems plus the current stacked core-fix branch  
**Review date:** 2026-10-25  
**Evidence level:** Repository docs and C++ source inspection only. Unreal 5.8 UHT/UBT, Editor, PIE and gameplay tests were not run.

## Executive result

**None of the nine game-reference groups is complete end-to-end.** Several have useful logical/source layers, but critical player-facing integrations remain open. A class/function existing is not completion evidence.

The core PR chain adds source-level guardrails and persistence work, but those changes are still draft/unmerged and do not make the nine gameplay groups complete:
- PR #18 — fail-closed fixed-step coordinator after rollback failure.
- PR #19 — preserve item transfer idempotency receipts through rollback and SaveGame v5.
- PR #20 — snapshot/persist the generic operation ledger through SaveGame v6 and reject saving pending operations.

Static checks for those changes passed, but the Unreal build and runtime tests are still open. Treat these as **source work in draft PRs**, not engine-verified completion.

## Status vocabulary

- **Partial:** a useful source/logical layer exists, but one or more required end-to-end behavior chains are missing.
- **Not implemented:** the plan/document exists, but the implementation layer could not be found in the reviewed code.
- **Static-only:** a source or document condition is checked, but no Unreal runtime evidence exists.
- **Complete:** reserved for the full acceptance outcome across prerequisites, visible/physical result, negative behavior, related systems, retry and save/load. No group below currently meets this bar.

---

## 2.1 — Sons of the Forest: physical wood processing and construction

**Status: PARTIAL — not complete.**

**Progress in WOOD-001 / WOOD-002:** `QuarterSectionLog` and `.QuarterSectionLog` distinguish quarter-round output from the legacy `.QuarterLog` suffix. `TrySplitItem` now atomically registers the first output (retaining the source ID) and a new output (new GUID) with a durable split receipt; `SplitHalf` / `Quarter` actor paths spawn a deferred second physical actor, bind its ownership ID, validate its structural node and attempt rollback on failure. SaveGame v7/fixed-step rollback carry split receipts. **Still partial:** mesh-specific center offsets and child rotations must be configured and visually verified per mesh. UE 5.8 build/PIE, failure injection, exact physical geometry, actor transform persistence and end-to-end source counts remain unverified.

### Already present in source
- `ALogBuildPiece` owns cut-action eligibility and blocks processing while placed, held or carrying attached pieces.
- `CanApplyCutAction` maps whole/split-half states to specific configured output mesh slots for split-half, quarter, hewn, gutter, trough, squared-beam and plank operations.
- `TryApplyCutAction` checks the output mesh before changing the authoritative item definition and then applies the mesh/type change.
- `SetPieceStaticMesh` regenerates automatically generated snap points against the replacement mesh while retaining authored points.
- Log actors register with the item ownership ledger; the structural graph provides support/connection primitives.

### Still missing / not proven
- [ ] Separate **cut progress** from completed piece type; there is no complete partial-cut progress state and visible progression contract.
- [ ] Separate length fraction (whole, 3/4, 1/2 length, 1/4 length) from cross-section shape (whole round, half-round, quarter-round, hewn, squared, groove, trough). The existing `QuarterLog` / `Quarter` naming risks mixing length and cross-section meanings.
- [x] Source-level output path now supports two physically separate outputs for `SplitHalf` and `Quarter`, with an ownership split receipt, stable source ID, unique child ID, deferred child spawn, graph-registration verification and rollback attempt. This is a code-layer result only, not a verified engine result.
- [ ] Configure and verify per-mesh cross-section center offsets; default zero offsets deliberately reject splitting rather than placing overlapping pieces.
- [ ] Add/review authoring data for any additional offcuts/multiple output counts; other cut actions still use single-actor transformation semantics.
- [ ] Define the state → allowed action → resulting piece(s) → mesh/collision → length/quantity table for every action.
- [ ] Confirm every required output mesh/asset is actually assigned in the intended Blueprint/content assets; C++ properties alone do not prove that.
- [ ] Complete tree-felling/grounding/length-to-log generation, with stable IDs and collision.
- [ ] Complete grounded spike/tent/trap preconditions, actor adapters for stones/sticks, safe support removal, salvage and damage/repair in the shared graph.
- [ ] Prove preview transform equals committed placement transform in UE.
- [ ] Negative cases must leave actor, geometry, ownership, anchors and inventory unchanged.

**Progress in WOOD-002/003:** `TrySplitItem` commits the original item plus a unique second item under a split receipt; `SplitHalf` / `Quarter` use deferred second-actor creation with ownership/graph verification and a rollback attempt. Geometry metadata now separates `ELogLengthFraction` from `ELogCrossSectionShape`; split-related eligibility uses cross-section shape so a shortened round log does not fail merely because its legacy `PieceType` label names a length fraction. The repository inventory lists 48 root FBX meshes and 10 Archive/joinery meshes, based on filenames only.

**Still not verified:** actual FBX bounds/pivots/orientations, output mesh assignment, per-Blueprint offsets/rotations, the absent `SM_RawLog_75.fbx` base-round mesh, and ambiguous `Long_01..08` / `Short_01..04` dimensions. The import script stages a gallery; it does not complete production Blueprint configuration or physical actor save/load.

**Next implementation:** use the exact asset inventory to prepare a staging-level measurement manifest. Do not promote filename-derived categories to verified geometry until the meshes have been imported and measured in UE.

## 2.2 — 7 Days to Die: constrained excavation, raids and structural pressure

**Status: PARTIAL — not complete.**

### Already present in source
- `WorldTerrainStateSubsystem` stores bounded, depth-layer-gated logical cell edits with revisions, operation receipts and snapshot APIs.
- `WorldThreatDirectorSubsystem` has timed raid states and preflight/advance/snapshot logic.
- `WorldRaidAdapterSubsystem` can spawn a configured raid pawn and applies damage through the structural graph.

### Still missing / not proven
- [ ] Turn logical terrain edits into visible geometry and matching collision.
- [ ] Connect excavation to navigation and structural support invalidation.
- [ ] Complete small underground room/tunnel geometry with protected-depth/volume limits.
- [ ] Implement valid spawn selection, target selection, pursuit, navigation, attack behavior and obstacle bypass. A scheduled raid or spawned pawn alone is not complete enemy AI.
- [ ] Derive settlement pressure from explicit/tunable noise, stores, defenses and relevant world state.
- [ ] Ensure actor destruction, graph removal and salvage commit together rather than leaving dangling nodes/resources.
- [ ] Ensure raid/pawn/damage/wave state survives retry and save/load without duplication.

**Next implementation:** finish the bounded excavation physical adapter and the configured pawn's real movement/targeting integration; keep world-wide voxel conversion out of scope.

## 2.3 — Death Stranding: terrain-aware transport and infrastructure

**Status: PARTIAL — not complete.**

### Already present in source
- `WorldLogisticsRouteSubsystem` provides a logical route graph and route lookup.
- `WorldCargoTransportSubsystem` advances logical cargo progress and attempts ownership/storage delivery.
- Item ownership and storage snapshots are part of the current source layer.

### Still missing / not proven
- [ ] Calculate route cost from actual terrain/elevation/surface, road/bridge/rope/zipline availability, cargo load, weather and threat rather than only authored graph-edge costs.
- [ ] Connect route nodes/endpoints to real built infrastructure and world transforms.
- [ ] Move a physical character/cargo representation along the selected route; logical progress is not visible travel.
- [ ] Validate route changes when a bridge/rope/road is destroyed or a terrain pocket changes.
- [ ] Complete capacity reservation, full/partial transfer behavior, destination actor materialization and retry-safe delivery.
- [ ] Save/load physical route endpoints and infrastructure relationships.

**Next implementation:** first link one real road/bridge or rope/zipline to route edges and its endpoint transforms; then connect physical transport.

## 2.4 — Farming Simulator: crop, soil, animal and product loop

**Status: PARTIAL — not complete.**

### Already present in source
- `WorldAgricultureSubsystem` holds logical crop and animal records and exposes registration, crop-moisture adjustment, animal feeding and deterministic step APIs.
- Crop/animal records can be snapshot/restored and the coordinator advances the agriculture layer on the shared logical step.

### Still missing / not proven
- [ ] Physical plot/soil actors, planting and harvesting actions are connected to those records.
- [ ] Season, soil type/nutrients, rainfall/runoff and planted-region constraints are modeled as actual growth inputs.
- [ ] Feed is transactionally consumed from inventory/storage; the logical hunger reduction alone is not the full feeding loop.
- [ ] Harvest, milk/egg/wool and byproducts are registered once in authoritative item ownership and storage.
- [ ] Habitat capacity, animal pathing, reproduction and terrain/support invalidation are connected.
- [ ] Retried time steps or save/load must not double-grow, double-harvest or duplicate animal products.

**Next implementation:** close one modest crop loop and one animal/product loop before expanding species/crop count.

## 2.5 — Subnautica: environmental discovery and modular exploration

**Status: PARTIAL — not complete.**

### Already present in source
- `WorldSurvivalSubsystem` records discoveries and carries discovery records through snapshots/SaveGame.
- The shared structural graph and generic construction pieces are foundations for modular construction.

### Still missing / not proven
- [ ] Place real ruin/clue/environmental actors with stable identities.
- [ ] Connect interaction to a durable discovery and visible player-facing change/unlock/context.
- [ ] Recreate or reconcile discovery-linked actors/context after save/load.
- [ ] Connect placeable modular base pieces to actual construction compatibility, support, integrity and save contracts rather than treating discovery records as the whole feature.

**Next implementation:** one diegetic ruin/clue actor → discovery record → visible unlock/context → save/load round trip. No cinematic story is required.

## 2.6 — Frostpunk: cold exposure and settlement resource pressure

**Status: PARTIAL — not complete.**

### Already present in source
- The survival layer includes weather/severity and profile-level clothing insulation/shelter protection.
- Survival state is advanced through the shared logical clock/coordinator.

### Still missing / not proven
- [ ] Connect shelter, clothing, wetness and regional temperature to a single validated exposure result and actual needs/health consequences.
- [ ] Link fire, fuel stock, heat output and cooking to authoritative inventory/storage/production consumption.
- [ ] Ensure cold/heat effects are measurable and testable, not separate UI-only values.
- [ ] If a settlement-wide resource layer is kept, connect daily consumption to stores and real production; full city-policy mechanics are outside this project's required scope.

**Next implementation:** integrate a small shelter–clothing–fire/fuel loop into the authoritative survival and inventory systems.

## 2.7 — The Long Dark: daily survival decisions

**Status: PARTIAL — not complete.**

### Already present in source
- Survival profiles contain needs/exposure-related logical fields; weather and clothing/shelter protection are represented.
- Survival data and discoveries have snapshot/save fields.

### Still missing / not proven
- [ ] Real sleep/rest action that advances shared time and updates fatigue/stamina consistently.
- [ ] Injury, illness, poisoning and medicine/effect progression.
- [ ] Food freshness/spoilage advanced by shared time and connected to actual food items.
- [ ] Cooking/fire/fuel loop and consumable effects.
- [ ] Equipment/clothing inventory binding; equipped protection must not be a free independent value.
- [ ] Region-specific temperature and indoor/outdoor exposure must be grounded in world location and shelter geometry.
- [ ] Player-visible needs and consequences linked to the authoritative profile.

**Next implementation:** one end-to-end “cold travel → shelter/equipment → rest/food → recover” survival scenario.

## 2.8 — Hydroneer: excavation yields, processing and resource flow

**Status: PARTIAL — not complete.**

### Already present in source
- A bounded logical terrain cell-edit subsystem exists.
- Production has stations, recipes, stock, queues and snapshot APIs.

### Still missing / not proven
- [ ] Define excavation material/strata and exact resource yield for an accepted dig.
- [ ] Create unique or stackable output resources in the authoritative ownership ledger after a successful excavation.
- [ ] Connect mined resources through storage to actual processing recipes and products.
- [ ] Add physical machines/processing actors only where needed for the chosen EA loop; conveyor/pipe automation is not yet connected to the logical route graph.
- [ ] Failed excavation/processing must leave terrain, tool, stock and outputs unchanged.

**Next implementation:** one bounded ore/material pocket → extracted resource → storage → processing station → owned output.

## 2.9 — Satisfactory: production chains and factory logistics

**Status: PARTIAL — not complete.**

### Already present in source
- `WorldProductionSubsystem` supports logical recipe/station/stock/job queues and blocked states.
- `WorldCargoTransportSubsystem` tracks logical route progress and storage delivery.

### Still missing / not proven
- [ ] Atomic input reservation/consumption, output-capacity reservation and job completion across production stock and item ownership.
- [ ] Register physical processing output as a real item, with a single owner.
- [ ] Link fuel/power/heat requirements to production and survival where required.
- [ ] Derive route costs from world infrastructure and terrain.
- [ ] Give blocked jobs actionable reasons for missing input, station, fuel, capacity or route, and ensure retries do not consume resources twice.
- [ ] Physical conveyance/machine movement is not implemented merely by a logical cargo queue.

**Next implementation:** close one raw wood → processed timber/plank → storage/transport chain before adding larger factories.

---

## LB0–LB4 — Landscape Builder, using the original numbering from the first checklist

**Numbering note:** the later formal `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN.md` calls “Biomes and environment dressing” LB4 and “Physical excavation pockets” LB5. In the initial checklist referenced by the user here, LB4 means physical excavation. This status section preserves that original numbering and records the formal-plan mapping explicitly.

### LB0 — Engine and branch capability audit

**Status: PARTIAL / STATIC-ONLY.**
- [x] H-integrated source baseline selected; stacked branch ancestry inspected.
- [x] Planning branch and current core-fix chain identified; branch counts refreshed in the audit document.
- [ ] Verify exact UE 5.8 editor availability and project plugin/module compatibility in the real installed engine.
- [ ] Confirm native Landscape/Edit Layers/Editor Scripting/PCG and any Mesh Terrain tools in the installed editor, rather than infer from documentation alone.
- [ ] Record chosen dimensions, resolution, playable footprint and machine/runtime budgets.

### LB1 — Profile-driven world and macroform generation

**Status: NOT IMPLEMENTED (plan only).**
- [ ] Add a validated profile schema for world dimensions, seed, vertical scale, macroform template, resolution and protected/editable regions.
- [ ] Implement deterministic heightmap generation with macroform shapes (mountains, ridges, valleys, basins and playable lowlands) plus seeded detail.
- [ ] Reject invalid or unsupported dimensions before asset creation.
- [ ] Emit a 16-bit heightmap in a UE-supported import format plus a versioned manifest/checksum.
- [ ] Verify that identical profile/version/seed produces identical output.

### LB2 — Native Landscape creation and surface materials

**Status: NOT IMPLEMENTED (plan only).**
- [ ] Create/import a native Landscape through an editor workflow; validate component/resolution settings.
- [ ] Configure collision, edit layers and a base material.
- [ ] Add terrain paint layers and physical surface classification (soil, rock, mud, sand etc.).
- [ ] Keep base generation, later sculpt/refinement and runtime edits in distinct layers/representations.
- [ ] Save/reopen the level and verify the Landscape still has expected collision/material settings.

### LB3 — Erosion, routes and water corridors

**Status: NOT IMPLEMENTED as integrated world tooling.**
- [ ] Define the erosion/refinement stage and protect playable paths/building pads.
- [ ] Author road/trail/river corridors using appropriate Landscape/Spline tooling.
- [ ] Define bridge/rope/zipline endpoints that can become real route graph nodes.
- [ ] Connect route costs to the authored terrain/road state; current generic route graph does not prove this integration.
- [ ] Water/runoff/flooding rules still need a real simulation/world-facing adapter if they are kept in scope.

### LB4 — Physical excavation pockets (original numbering)

**Status: PARTIAL — logical terrain state only.** Maps to formal-plan LB5.
- [x] A bounded, depth-layer-gated logical cell-edit API exists with revision/receipt/snapshot support.
- [ ] Connect accepted edit requests to visible 3D excavation geometry and matching collision.
- [ ] Support only the bounded test pockets/short tunnels/small rooms; do not convert all outdoor Landscape to voxels.
- [ ] Reject protected cells, unsupported depth and over-budget edits before mutation.
- [ ] On commit, update nearby structural support and navigation/route data.
- [ ] Persist excavation deltas and restore them before rebuilding collision/navigation.
- [ ] Demonstrate that preview geometry equals the committed geometry and invalid edits cause no mutation.

## Completion rule for this matrix

- [ ] No category is marked complete on the strength of source-file existence.
- [ ] Per-category acceptance includes a valid action, invalid action, physical/logical result, cross-system consumer, retry and save/load case.
- [ ] Unreal engine verification remains separately recorded; it is not implied by static source review.
- [ ] The final demo is one connected world that proves source → processing → construction, terrain → support/navigation, farm → item ownership/production, logistics → delivery, raid → damage, survival → resource use, and save/load continuation.

## Recommended next execution order

1. Finish the remaining branch ancestry/unique-diff audit; retain the stacked PRs as drafts until their chain is reviewed.
2. Close the physical wood-processing contract and highest-risk state/output mismatch under 2.1. In particular, remove ambiguity between length-cut pieces and cross-section splits before adding automatic spawned outputs.
3. Complete the shared stone/stick ownership adapters and structural support/damage outcomes.
4. Connect raid pawn behavior to actual target/navigation/attack semantics and constrained excavation.
5. Close one farming/animal loop.
6. Close one logistics/production loop.
7. Close one shelter/weather/food/rest/survival loop and a persistent discovery.
8. Implement Landscape Builder LB1–LB3; connect LB4 bounded physical excavation after the source/world edit contract is sound.
9. Once code layers and integrations are complete, build the small single-world demo and run the per-ID positive/negative acceptance gates in the authorized UE 5.8 environment.

