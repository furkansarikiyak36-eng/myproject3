# Phase 00 — Repository Baseline and Branch Integration Audit

**Repository:** furkansarikiyak36-eng/myproject3  
**Target:** Unreal Engine 5.8 / Windows  
**Planning branch:** planning/gdd-inventory-production-roadmap  
**Current main reviewed:** 65faad8602d4e14e894dc2cf4e3d507ca39cbb1d  
**Purpose:** A fact-based starting point. This report does not certify compilation, PIE, or completion of any mechanic.

## 1. Findings at a glance

- Main contains the first log carry/place/snap prototype and its setup scripts. It does not contain the newer shared placement kernel and broad gameplay-source additions visible on several feature/phase branches.
- The tracked prototype's own README states that its native classes need an Unreal Editor target build before the level script can use them. This audit does not claim a fresh UE 5.8 build or runtime test.
- A canonical M001–M119 scope registry already exists on phase-01-foundation-v2. It has 119 unique IDs, grouped into 10 systems. Every mechanic is unreviewed; static review, automated test, Unreal build, and runtime-test fields are all not_run.
- Phase branches are stacked on one another and contain source code, not only documents. They are not safe to merge into main as a loose group.
- The newer snap branch feature/simple-intent is significantly divergent from main. It must be reviewed as an implementation candidate, not treated as already integrated.

## 2. Current main baseline

Main is at 65faad8602d4e14e894dc2cf4e3d507ca39cbb1d. Comparing the prior source inventory baseline ff2fd51444463afad598b837e820dbc3df2d439b to current main showed three subsequent commits affecting project memory, decision notes, and the snap screenshot gallery documentation; no gameplay source file change appeared in that comparison.

### Relevant main paths

| Path | Observed responsibility | What this proves / does not prove |
|---|---|---|
| Source/MyProject3/LogBuildPiece.h/.cpp | Log type, processing mesh variants, snap points, attachment and held/placed flags | A prototype exists; not a complete resource lifecycle or verified gameplay loop |
| Source/MyProject3/LogCarryComponent.h/.cpp | Pickup/drop, preview, snap selection, free placement, processing-action selection | A basic interaction layer exists; not proof all M009–M028 pass |
| Source/MyProject3/LogBuildingTypes.h | Piece/action/snap data types | The current domain contract is log-specific |
| Source/MyProject3/LogSnapCharacter.h/.cpp, LogSnapGameMode.* | Test pawn and game mode | Prototype controls and mode exist |
| UnrealEditorScripts/BuildLogSnapPrototype.ps1 | Builds the Unreal Editor target | Build helper exists; it is not test evidence |
| UnrealEditorScripts/CreateLogSnapTestLevel.py | Creates the separate log playground map | Script existence does not prove successful execution in the editor |
| AssetPackages/UnrealExport/ | Project-owned log mesh variants | The listed meshes support a mesh-swap prototype, not procedural cutting |

The reviewed prototype documentation reports that cutting currently swaps between configured mesh variants. It does not represent a fully animated, tool-driven cutting simulation, nor does it establish durable save/load.

## 3. Branch graph and integration candidates

The branch comparisons below are GitHub compare results at audit time. Ahead/behind counts describe commit ancestry, not correctness.

| Branch | Relation observed | Contents seen | Phase 00 decision |
|---|---|---|---|
| phase-01-foundation | Older branch; 3 commits ahead and 66 behind main | Early foundation docs | Do not use as the working base without comparison |
| phase-01-foundation-v2 | 58 ahead / 3 behind main; diverged | Canonical 119 registry, source/call-chain/state-transition inventory, Phase 1 notes, and modifications to LogBuildPiece/LogCarryComponent | Use as the source for registry and audit documents; inspect code changes before any transplant |
| phase-02-core-contracts | 27 commits ahead of phase-01-foundation-v2 | GameplayCoreTypes.*, identity component/subsystem, committed-event subsystem, contracts/checklists | Candidate shared contract implementation; source-level completion only |
| phase-03-wood-processing | 6 commits ahead of phase-02 | Result-bearing processing entry point and processing docs | Candidate for integration after call sites are reviewed |
| phase-04-stone-masonry | 7 commits ahead of phase-03 | Stone piece/types and anchor logic | Initial stone layer; full stone mechanics and validation remain open |
| phase-05-stick-primitives | 6 commits ahead of phase-04 | Stick primitive piece/types | Initial stick/tool/trap layer; many M039–M048 requirements remain open |
| phase-06-inventory-crafting | 6 commits ahead of phase-05 | Inventory/crafting component/types | Initial stack/recipe transactions; UI, persistence, world pickup and full crafting flow remain incomplete |
| feature/global-snap-kernel | Large divergent feature branch; 3 commits behind main | Shared placement kernel and newer construction architecture | Review as a separate implementation line; do not copy individual files blindly |
| feature/contextual-interaction-resolver | Large divergent feature branch; 3 commits behind main | Contextual placement/cut work on top of snap work | Review against the kernel and latest branch |
| feature/simple-intent | Large divergent feature branch; 3 commits behind main | Contextual, symmetric snap behavior, gallery command, broader source tree | Primary snap candidate to review, not merged/verified on main |
| mechanic-detailing-pass-a-core through pass-h-integrated-world-systems | Stacked architecture/work branches; pass H is 349 commits ahead and 3 behind main | World core, operation ledger, clock, structural graph, terrain state, agriculture, threats, logistics, production, survival, storage, save coordination and demo plan appear across the series | Treat as a broad candidate stack. It needs source review, build and integration planning before adoption |

**Important:** Some compare responses hit the 300-file listing limit for the largest divergent branches. Counts/paths from those responses must not be interpreted as an exhaustive review of every changed file.

## 4. Canonical M001–M119 scope

The registry at Docs/Architecture/MECHANIC_REGISTRY_119.json on phase-01-foundation-v2 declares scope version 1.0. IDs are immutable; do not renumber or reuse them.

| ID range | Count | System |
|---|---:|---|
| M001–M008 | 8 | Core / World Contracts |
| M009–M028 | 20 | Log / Wood Processing |
| M029–M038 | 10 | Stone / Masonry |
| M039–M048 | 10 | Stick / Primitive Tools |
| M049–M058 | 10 | Inventory / Crafting |
| M059–M070 | 12 | Shared Structure / Damage |
| M071–M080 | 10 | Terrain / World State / Digging |
| M081–M090 | 10 | Farming / Animal Husbandry |
| M091–M100 | 10 | Raid Director / Threat AI |
| M101–M119 | 19 | Logistics / Production / Survival / Exploration |
| **Total** | **119** | |

Provenance note from the registry: the exact historical list behind an earlier “119 passed” report was not found. The current registry explicitly defines the scope from its own commit onward; it does not claim to reconstruct that older list.

## 5. Risks identified before writing more code

1. **Branch divergence:** snap code, phase-code, and main are not one integrated source baseline.
2. **Duplicate authority risk:** a new identity, inventory, graph, save, or operation system must not compete with a current domain owner.
3. **Missing build/runtime evidence:** phase closeout documents explicitly leave UE 5.8/UHT, Editor, PIE, and gameplay testing open.
4. **Main is narrower than intended scope:** stone, sticks, inventory/crafting, terrain state, agriculture, raid AI and world coordination appear in separate candidate branches, not as a verified main integration.
5. **Voxel uncertainty:** a terrain state subsystem is not by itself proof of runtime voxel mesh/collision editing, edit persistence, or UE 5.8 plugin compatibility.
6. **Scope gap:** regional ecological restoration, air/water remediation, and future spore/fungal systems are not clearly represented as dedicated mechanics in the current 119 names. Record these as candidate scope additions; do not assign M120+ until duplicate and design review is done.
7. **Data contracts:** current inventory types and log processing types do not yet form one fully validated, persisted ownership/recipe model across the full game.

## 6. Phase 00 completion criteria

- [x] Correct repository and current main commit identified.
- [x] Existing branch families and branch divergence recorded.
- [x] M001–M119 registry located; exact scope provenance and initial verification state recorded.
- [x] Current prototype source paths and known limits documented.
- [ ] Mechanic-by-mechanic source review completed against the chosen integration baseline.
- [ ] Branch integration base selected and pinned by commit SHA.
- [ ] Local/untracked Blueprint/editor asset callers audited in Unreal Editor.

Phase 00 is therefore **baseline recorded; deep per-mechanic audit and editor-only inspection remain open**.
