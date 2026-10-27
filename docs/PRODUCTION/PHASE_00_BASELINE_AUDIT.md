# Phase 00 — Repository Baseline and Integration Audit

**Audit date:** 2026-10-28  
**Repository:** `furkansarikiyak36-eng/myproject3`  
**Planning branch:** `planning/gdd-inventory-production-roadmap`  
**Engine target:** Unreal Engine 5.8  
**Audit type:** GitHub branch/commit/file review; no local checkout, Unreal build, Editor or PIE run was performed in this audit.

## Executive decision

Do not start new gameplay implementation on the planning branch. The repository already contains a staged implementation series. Phase 00 should first choose and document the correct integration baseline, preserve mechanic IDs, and mark evidence separately from implementation status.

The current `main` branch is the published prototype baseline. Several implementation branches are substantially ahead of it, but they are stacked and diverged from current `main`; their source must not be assumed to be integrated or engine-verified.

## 1. Current main baseline

Latest observed main commit during this audit:

- `65faad8602d4e14e894dc2cf4e3d507ca39cbb1d` — `memory: state, gallery findings and in-game screenshots`.
- The README describes the first log-carry/snap prototype and lists `LogBuildPiece.*`, `LogCarryComponent.*`, `LogSnapCharacter.*`, `LogSnapGameMode.*`, setup scripts and project-owned log FBXs.
- `LogBuildPiece.h` exposes log type, local longitudinal axis, automatic/custom snap points, held/placed state, variant meshes, processing actions and parent/child connection methods.
- `LogCarryComponent.h` exposes pickup, place, drop, rotation, placement mode, cut-action selection and processing calls.
- Current README describes processing as pre-authored mesh swaps, not procedural cutting or a fully animated tool workflow.
- The README says the native classes in the documented baseline still require an Unreal Editor build before the test-map script can run. This audit does not update that claim with a new build result.

### Main-only implementation evidence

The following mechanics have relevant source paths on main, but are **not thereby verified**:

- M011 pickup/carry and M012 drop/release.
- M016 longitudinal split, M017 quartering, M018 hewing/squaring, M019 groove after split, M020 trough and M021 plank variants.
- M024 snap-anchor operations, M025 placement orientation/placement, M026 parent-child construction links.

Main's documented scope does not provide evidence for the other 108 entries as complete mechanics. This is a conservative documentation classification, not a claim that no partial work exists on any other branch.

## 2. Existing phase implementation chain

Comparisons against current main show these branches have relevant work not integrated into main:

| Branch | Ahead of main | Behind main | Evidence found | Gate |
|---|---:|---:|---|---|
| `phase-01-foundation-v2` | 58 commits | 3 commits | M001–M119 registry, source inventory, call-chain/state-transition maps, some guarded changes to log source | Review source diff; build/runtime still open |
| `phase-02-core-contracts` | 85 commits | 3 commits | Shared operation results/requests, identity component/subsystem, transient committed-event subsystem | Core contracts exist on branch; engine validation open |
| `phase-03-wood-processing` | 91 commits | 3 commits | Result-bearing wood processing entry point and event notification | Partial wood workflow; integration/engine validation open |
| `phase-04-stone-masonry` | 98 commits | 3 commits | Stone type/actor and anchor/connection layer | Initial layer only; broader stone mechanics open |
| `phase-05-stick-primitives` | 104 commits | 3 commits | Primitive piece and state/anchor layer | Initial layer only; full mechanics open |
| `phase-06-inventory-crafting` | 110 commits | 3 commits | Stack/recipe transaction layer | UI/world pickup/save-load and engine validation open |
| `mechanic-detailing-pass-h-integrated-world-systems` | 349 commits | 3 commits | Later system code for structural graph, terrain state, farming, threats, logistics, survival, storage and save coordination | Large stacked branch; never merge blindly |

The phase 02–06 branches are a linear series: phase 02 is 27 commits ahead of phase 01; subsequent deltas add 6, 7, 6 and 6 commits respectively. Treat them as stacked feature work, not independent branches that can each be merged straight to main.

The older development branch names use a 16-phase implementation structure. This planning folder uses a separate GDD-oriented Phase 00–14 map. These numbers are not interchangeable; the mechanic registry's own `phase` field must be preserved and cross-referenced rather than renumbered.

## 3. Canonical M001–M119 registry

The authoritative files exist on `phase-01-foundation-v2`:

- `Docs/Architecture/MECHANIC_REGISTRY_119.json`
- `Docs/Architecture/MECHANIC_REGISTRY_119.md`
- `Docs/Architecture/MECHANIC_REGISTRY_119.csv`
- `Docs/Architecture/MECHANIC_REGISTRY_SCHEMA.json`

Static checks of the fetched CSV/JSON found 119 rows and 119 unique IDs, from M001 through M119. All entries are `unreviewed`; the CSV's static, automated, Unreal build and runtime evidence fields are all `not_run`.

Provenance caveat from the source registry: the earlier historical report said “119 passed”, but the exact historical 119-name list was not found. The new registry establishes an explicit scope from its own commit onward; it does not recreate or certify that historical test set.

**Rule:** Preserve all canonical IDs. Do not rewrite registry names or set status to complete merely because a subsystem, actor or source file exists. Link each mechanic to the actual owning code path and a reproducible acceptance case.

## 4. Source-level known risks to preserve

The shared project memory and phase docs identify the following open risks:

- `feature/simple-intent` is the newest snap work noted in project memory, but it is not merged into main.
- Two mirrored full-length wall diagonals can intersect/clip.
- 45-degree mitre end cuts do not exist in the documented work.
- `GatherPlacedBodies` scans the world on every snap solve, a potential performance risk.
- Seven joinery meshes need authored NotchCradle sockets.
- The contextual deck-placement path was marked untested in the project memory.
- `LogCarryComponent.cpp` and `LogBuildPiece.cpp` are very large; refactoring should follow a caller/interface map, not happen during a blind merge.
- Current log processing uses pre-authored mesh variants; tool impact, hit location, cut progress, tool wear/resource transaction and durable save/load are not established by the basic prototype documentation.

These are integration/test tasks, not permission to rewrite the snap foundation before reviewing the actual branch diffs.

## 5. Source ownership map (provisional)

| Domain | Candidate source paths observed in staged phase work | Current main evidence |
|---|---|---|
| Core operation/result/identity/events | `GameplayCoreTypes.*`, `WorldObjectIdentityComponent.*`, `WorldObjectIdentitySubsystem.*`, `WorldCommittedEventSubsystem.*` | Not established in main README/source scope |
| Wood processing and carry | `LogBuildPiece.*`, `LogCarryComponent.*` | Source exists; prototype and unverified |
| Stone | `StoneBuildingPiece.*`, `StoneBuildingTypes.h` | Not established in main documented scope |
| Stick/primitive | `StickPrimitivePiece.*`, `StickPrimitiveTypes.h` | Not established in main documented scope |
| Inventory/crafting | `InventoryCraftingComponent.*`, `InventoryCraftingTypes.h` | Not established in main documented scope |
| Structure graph | `WorldStructuralGraphSubsystem.*` | Not established in main documented scope |
| Terrain state | `WorldTerrainStateSubsystem.*` | Not established in main documented scope |
| Time/agriculture | `WorldSimulationClockSubsystem.*`, `WorldAgricultureSubsystem.*` | Not established in main documented scope |
| Threat/raid | `WorldThreatDirectorSubsystem.*`, `WorldRaidAdapterSubsystem.*` | Not established in main documented scope |
| Logistics/storage/production | `WorldLogisticsRouteSubsystem.*`, `WorldCargoTransportSubsystem.*`, `WorldStorageSubsystem.*`, `WorldProductionSubsystem.*` | Not established in main documented scope |
| Survival/save coordination | `WorldSurvivalSubsystem.*`, `WorldSimulationSaveGame.h`, `WorldSaveCoordinatorSubsystem.*` | Not established in main documented scope |

This table records candidate ownership from staged branch filenames, not final authority for every individual mechanic. Per-mechanic ownership requires reading the implementation and callers.

## 6. Phase 00 exit conditions

- [x] Correct repository and current main baseline confirmed.
- [x] Existing implementation branches and divergence recorded.
- [x] Canonical 119 registry found and ID count checked.
- [x] No-build/no-PIE limitation explicitly recorded.
- [ ] Review full diffs across the stacked implementation chain and select a candidate integration head.
- [ ] Validate that the candidate head contains the desired snap work without conflicting/duplicate kernels.
- [ ] Run a fresh local Unreal 5.8 compile and bounded editor checks when available.
- [ ] Approve a single base commit before source implementation continues.

Until the unchecked items are done, Phase 00 is **partially closed at repository-documentation level**, not fully closed for implementation.

## 8. Per-mechanic traceability file added

See [MECHANIC_TRACEABILITY_119.csv](MECHANIC_TRACEABILITY_119.csv). It contains one row for each canonical ID and separates:

- coverage observed on current `main` (source baseline at commit prefix `65faad8`);
- candidate coverage on `mechanic-detailing-pass-h-integrated-world-systems`;
- exact source path families reviewed;
- the remaining source-level gap;
- canonical registry status and the engine/runtime evidence fields.

The matrix contains 119 mechanic rows plus a header. Current `main` classification counts: `not-found` 101, `partial` 18. Candidate-stack classifications: `partial` 88, `contract-only` 4, `present-in-layer` 1, `transient-only` 1, `not-found` 25.

**Important interpretation:** candidate-stack classifications are a working static review, not a pass/fail judgment. They never change canonical registry rows away from `unreviewed`, and build/PIE fields remain `not_run`. Some rows are assigned a broad candidate status from source-layer evidence; the notes specify the missing end-to-end behavior. Review the named source/callers before implementation or merging.

## 9. Phase 00 status update

The documentation-level snapshot and first 119-row traceability matrix now exist. Phase 00 is still **partially closed** because full file-by-file diff review, selection of the integration base, local Unreal 5.8 compilation, Editor/PIE verification and final caller/Blueprint review have not been completed.
