# Phase 00 — Repository Baseline and Branch Audit

**Audit date:** 2026-10-28  
**Target:** Unreal Engine 5.8  
**Audited repository:** furkansarikiyak36-eng/myproject3  
**Current-main baseline:** 65faad8602d4e14e894dc2cf4e3d507ca39cbb1d  
**Status:** Static GitHub/source review only. No local Unreal build or Editor/PIE run was performed as part of this audit.

## 1. Executive finding

There is substantially more work on existing phase branches than is present on main. The next step is **not** to create a second set of core types or a second M001–M119 list. It is to reconcile the existing branch chain, preserve the current source-of-truth IDs, and decide which implementation commits can be integrated safely.

The existing canonical 119-mechanic registry is structurally complete as a list: 119 CSV rows, 119 unique IDs from M001 through M119, all currently marked unreviewed, and all four evidence fields marked not_run in that registry. The registry explicitly says it defines a new scope version and does not recover the exact historical list behind a prior “119 passed” report.

## 2. Branch topology and integration risk

| Branch | Relationship observed | Source/content found | Audit interpretation |
|---|---|---|---|
| main | Current baseline, SHA 65faad8 | Initial log carry/place/snap prototype, editor scripts, existing mesh archive references | Safe reference point, but behind newer snap work |
| phase-01-foundation-v2 | Compare against main: 58 ahead, 3 behind; diverged | M001–M119 registry; source inventory/call-chain/state-transition docs; changes in LogBuildPiece and LogCarryComponent | Valuable baseline work, but not a fast-forward of main. Review source diff and integrate intentionally |
| phase-02-core-contracts | 27 commits ahead of phase-01-foundation-v2; compare against main: 85 ahead, 3 behind | GameplayCoreTypes, WorldObjectIdentityComponent/Subsystem, WorldCommittedEventSubsystem | Reuse candidate for contracts; source-level only, Unreal validation pending |
| phase-03-wood-processing | 6 commits after phase 2 | Result-bearing wood processing entry point; existing boolean API retained | Initial layer, not all M009–M028; validation pending |
| phase-04-stone-masonry | 7 commits after phase 3 | Stone piece type/actor and anchor rules | Initial stone placement layer, not full stone mechanics |
| phase-05-stick-primitives | 6 commits after phase 4 | Stick actor/types, processing/anchor/trap prerequisites | Initial layer, not complete primitive-tools loop |
| phase-06-inventory-crafting | 6 commits after phase 5 | Inventory stack and recipe transaction component/types | Initial layer; world pickup/UI/persistence integration remain open |
| mechanic-detailing-pass-h-integrated-world-systems | Compare against main: 349 ahead, 3 behind; 111 changed files in compare summary | Additional structural graph, terrain-state, ownership, simulation, agriculture, survival, logistics, production, threat, and save coordinator source | Broad candidate branch, not ready to merge without checking docs/code drift, compilation, runtime and integration behavior |
| planning/gdd-inventory-production-roadmap | Current documentation branch created for this plan | GDD, inventories, phased plan, acceptance/dependency docs | Keep as planning/documentation branch; do not merge source branches implicitly |

### Key topology warning

The phase-01-to-phase-06 line is internally sequential, but is based on a main snapshot three commits behind current main. The later integrated H branch has a much larger divergence and includes more source than several phase docs describe. Do not merge a broad branch solely because it has more files or a “closeout” document. Review the individual source diff and current API first.

## 3. Current main: evidence-backed scope

The main README describes the current implementation as the initial log carry + snap prototype. The tracked native core includes:

- LogBuildPiece.h/.cpp — log piece type, processing variants, snap points, occupancy, parent/child links, held/placed state and collision/physics changes.
- LogBuildingTypes.h — log piece/snap/cut/placement types.
- LogCarryComponent.h/.cpp — pickup/drop, carry preview, free/snap placement and processing-action selection.
- LogSnapCharacter.h/.cpp and LogSnapGameMode.h/.cpp — simple test character/game mode.
- UnrealEditorScripts/BuildLogSnapPrototype.ps1 and CreateLogSnapTestLevel.py — build helper and editor-only level setup.
- AssetPackages/UnrealExport — project-owned FBX log variants and joint examples.

The main README itself warns that the prototype uses pre-authored mesh variants, does not procedurally cut meshes, and is not a claim that the project has been compiled or played successfully.

## 4. Known log/snap gaps carried into Phase 00

Source notes and repository memory identify these unresolved items for the newer snap work:

- Main does not include the latest contextual snap implementation.
- Mirrored full-length wall diagonals intersect/clip where they cross.
- Mitre-style 45-degree end cuts do not exist.
- The contextual deck path was recorded as untested.
- A snap solve path was recorded as scanning placed bodies across the world on every solve; this is a performance risk to measure and redesign.
- Seven joinery meshes require geometry-specific NotchCradle sockets.
- A source class or visual screenshot does not prove all mesh variants, collision, physics, input, or Blueprint consumers behave correctly.

These are audit inputs, not assertions that every later branch still has each defect. Recheck them on the selected integration head.

## 5. Existing architecture and data layers found on non-main branches

### Shared core contracts — phase-02-core-contracts

- GameplayCoreTypes.h/.cpp: structured operation results, stable result codes, request envelope and unitized resource amount.
- WorldObjectIdentityComponent.h/.cpp: opt-in instance GUID, definition key, lifecycle guard and revision.
- WorldObjectIdentitySubsystem.h/.cpp: world-local active identity registry.
- WorldCommittedEventSubsystem.h/.cpp: world-scoped transient committed-event dispatch.

Limits recorded by the branch: identity is opt-in; log actors were not silently migrated; event delivery is transient; durable event replay, save/load/migration and cross-session identity are not proven by this layer.

### Material and inventory domains — phases 3–6

- Wood processing result API exists as an initial extension of the current log class.
- Stone has a dedicated piece actor/types layer.
- Stick/primitive tools have a dedicated piece/types layer.
- InventoryCraftingComponent and InventoryCraftingTypes define stack items, slots/weight, ingredients, recipes, output checks and a rollback snapshot.

Limits recorded in phase closeouts: full interaction/UI integration, all mechanics within the registry range, save/load, and UE 5.8 build/Editor/PIE verification are open.

### Later integrated-system candidate — mechanic-detailing-pass-h

Static file inspection finds source for:

- WorldStructuralGraphSubsystem
- WorldItemOwnershipSubsystem and WorldStorageSubsystem
- WorldOperationLedgerSubsystem
- WorldSimulationClockSubsystem and WorldSimulationCoordinatorSubsystem
- WorldTerrainStateSubsystem
- WorldAgricultureSubsystem
- WorldSurvivalSubsystem
- WorldThreatDirectorSubsystem and WorldRaidAdapterSubsystem
- WorldLogisticsRouteSubsystem and WorldCargoTransportSubsystem
- WorldProductionSubsystem
- WorldSaveCoordinatorSubsystem and WorldSimulationSaveGame

This establishes **source presence on that branch only**, not gameplay completeness. Specific limits observed include:

- Terrain state edits are bounded logical cell records; rendering and collision updates are explicitly left to a terrain adapter. No compatible runtime voxel mesh/collision implementation was established by the reviewed header/cpp.
- The threat director owns raid schedule/state; a separate adapter must materialize a pawn/AI behavior.
- The simulation clock does not tick itself; the caller must invoke fixed/bounded steps.
- The save coordinator refuses saves while an item is in Held ownership because the carry-component snapshot is not represented. Schema version 4 is checked exactly; a version migration path was not established by the reviewed code.
- Agriculture exposes crop/animal registration, moisture/feed and time-step APIs; the API reviewed does not establish every harvest, reproduction, manure/nutrient, or seasonal rule.
- Save coordinator, ownership subsystem, inventory stack component and structural graph need an explicit single-authority contract before integration.

## 6. M001–M119 registry findings

The canonical registry on phase-01-foundation-v2 has these system counts:

| Registry system | IDs | Count |
|---|---:|---:|
| Core / World Contracts | M001–M008 | 8 |
| Log / Wood Processing | M009–M028 | 20 |
| Stone / Masonry | M029–M038 | 10 |
| Stick / Primitive Tools | M039–M048 | 10 |
| Inventory / Crafting | M049–M058 | 10 |
| Shared Structure / Damage | M059–M070 | 12 |
| Terrain / World State / Digging | M071–M080 | 10 |
| Farming / Animal Husbandry | M081–M090 | 10 |
| Raid Director / Threat AI | M091–M100 | 10 |
| Logistics / Production / Survival / Exploration | M101–M119 | 19 |

All 119 entries are unreviewed in the canonical file at the reviewed branch. Their presence or a source class name is not proof that the specified behavior is implemented. The separate traceability CSV in this plan gives conservative source-evidence pointers without editing the canonical registry's verification fields.

## 7. Phase 00 outputs and gates

- [x] Correct repository confirmed; old unrealgame repository not used.
- [x] Main baseline commit recorded.
- [x] Existing phase branch topology compared.
- [x] Canonical registry count and status fields inspected.
- [x] Current main prototype files identified from README and native headers.
- [x] Non-main source candidates inventoried.
- [x] Main-vs-branch evidence separated in traceability matrix.
- [ ] Review complete diffs for phase-01-v2 through phase-06 and H; choose a single integration head.
- [ ] Inspect full tracked file/asset tree and Unreal project plugin/build configuration.
- [ ] Run repository-level JSON/CSV schema checks where practical.
- [ ] Build MyProject3Editor with the actual UE 5.8 toolchain.
- [ ] Run relevant Editor/PIE interaction and save/load scenarios.

Phase 00 is **static-audit provisionally complete**, but branch-selection and Unreal validation gates remain open.

## 8. Source references

- Canonical registry (readable): Docs/Architecture/MECHANIC_REGISTRY_119.md on phase-01-foundation-v2.
- Canonical registry (machine-readable): Docs/Architecture/MECHANIC_REGISTRY_119.json and .csv on phase-01-foundation-v2.
- Source inventory: Docs/Architecture/PHASE_01_SOURCE_INVENTORY.md on phase-01-foundation-v2.
- Call-chain review: Docs/Architecture/PHASE_01_CALL_CHAIN_REVIEW.md on phase-01-foundation-v2.
- Held/placed contract: Docs/Architecture/PHASE_01_PLACEMENT_HELD_STATE_CONTRACT.md on phase-01-foundation-v2.
- Core C++ contract: Source/MyProject3/GameplayCoreTypes.h on phase-02-core-contracts.
- Cross-system acceptance map: Docs/Testing/DEMO_ACCEPTANCE_MAP_M001_M119.md on mechanic-detailing-pass-h-integrated-world-systems.
