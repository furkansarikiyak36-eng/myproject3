# Phase 00 — Baseline Audit and Integration Map

**Audit date:** 2026-10-28  
**Repository:** `furkansarikiyak36-eng/myproject3`  
**Target engine:** Unreal Engine 5.8  
**Planning branch:** `planning/gdd-inventory-production-roadmap`  
**Audit type:** GitHub source/docs/branch comparison. No Unreal build, UHT/UBT, Editor, PIE or save/load test was run for this audit.

## 1. Executive decision

Do not restart the project or independently re-create the M001–M119 registry, core operation types, inventory, structural graph, terrain state or save coordinator. The repository already contains a staged implementation line. The immediate job is to establish the trustworthy baseline, import the canonical scope registry into the planning documents, map existing contracts, identify integration conflicts and retain honest verification statuses.

The implementation chain visible from branch comparisons is:

`phase-01-foundation-v2` → `phase-02-core-contracts` → `phase-03-wood-processing` → `phase-04-stone-masonry` → `phase-05-stick-primitives` → `phase-06-inventory-crafting` → mechanic-detailing passes A–H → `feature/simple-intent`.

This chain is useful evidence of ancestry, not proof that every source layer is finished. `feature/simple-intent` is the furthest integration candidate in the checked branch set and is 347 commits ahead of the H integrated-world branch. It is also a large stacked branch, so it must be audited as a whole and not treated as a tiny snap-only patch.

## 2. Current main baseline

- Main HEAD observed: `65faad8602d4e14e894dc2cf4e3d507ca39cbb1d` (2026-10-27).
- The README describes the current main code as an initial log-carry/snap prototype using `ALogBuildPiece`, `ULogCarryComponent`, `ALogSnapCharacter`, and `ALogSnapGameMode`.
- The documented main setup requires an Unreal Engine 5.8 Editor target build before its test-map script can load the new classes.
- The README explicitly does not claim that this prototype has already been compiled or played successfully. This audit does not infer successful Unreal behavior from C++ files or GitHub presence.
- The most recent main-only commits relative to the older phase baseline are memory/screenshot-gallery documentation commits, not evidence that the later phase-system implementation has been integrated into main.

## 3. Branch and integration inventory

| Branch | Relationship/evidence observed | How to treat it |
|---|---|---|
| `main` | Current user-facing baseline; log prototype and project scripts | Preserve; do not overwrite with a large stacked branch without review |
| `phase-01-foundation-v2` | 58 commits ahead of main and 3 main-only commits; includes M001–M119 registry, static source inventory and changes to log actor/carry files | Foundational candidate; source and compatibility work must be carried forward selectively or through its ancestry |
| `phase-02-core-contracts` | 27 commits after Phase 1 with shared operation/identity/event C++ types | Reuse candidate contracts; Engine validation still open |
| `phase-03-wood-processing` | 6 commits after Phase 2; result-bearing wood processing and committed event | Partial domain implementation; not a finished physical cutting simulation |
| `phase-04-stone-masonry` | 7 commits after Phase 3; stone piece/anchor actor layer | Partial domain implementation; structural loads, complete stone lifecycle and persistence remain open |
| `phase-05-stick-primitives` | 6 commits after Phase 4; stick actor, processing state, anchors and trap gates | Partial domain implementation; many M039–M048 behaviors and integrations remain open |
| `phase-06-inventory-crafting` | 6 commits after Phase 5; initial stack inventory and recipe transaction layer | Partial domain implementation; full world pickup/UI/cancel/persistence remains open |
| `mechanic-detailing-pass-h-integrated-world-systems` | Descends from the Phase 6 line; adds ownership, graph, terrain-state, farming, logistics/production, survival, threat and save-related source | Broad source-layer candidate; not engine-certified; game-facing adapters remain incomplete |
| `feature/simple-intent` | Descendant of the integrated-world branch in the checked comparison; contains the most advanced contextual snap work and content/screenshot work | Preferred candidate for integration review, not auto-merge approval |
| `planning/gdd-inventory-production-roadmap` | Documentation-only planning branch based on current main | Keep the audit, GDD and schemas here; do not mix in unreviewed gameplay refactors |

The current branch contains this audit and copied canonical registry files. It does not change gameplay source, Unreal maps, plugins or imported content.

## 4. Canonical M001–M119 registry

The source registry was found on `phase-01-foundation-v2` and copied into this planning branch:
- `MECHANIC_REGISTRY_119.json` — canonical structured registry.
- `MECHANIC_REGISTRY_119.md` — readable list and invariants.
- `MECHANIC_REGISTRY_119.csv` — working/spreadsheet export.
- `MECHANIC_REGISTRY_SCHEMA.json` — entry schema.

Static integrity inspection of the CSV/JSON returned 119 records, 119 unique IDs, from M001 through M119. All records currently have status `unreviewed`; static review, automated test, Unreal build and runtime fields are `not_run`.

**Historical provenance limitation:** the old report's “119 checks passed” names were not found in the reviewed source files. This registry defines the canonical scope going forward; it must not be described as a verified recovery of that historical test list. Do not change IDs or report passing status without linking concrete evidence.

System allocation recorded by the registry:

| System | Count |
|---|---:|
| Core/world contracts | 8 |
| Log/wood processing | 20 |
| Stone/masonry | 10 |
| Stick/primitive tools | 10 |
| Inventory/crafting | 10 |
| Shared structure/damage | 12 |
| Terrain/world state/digging | 10 |
| Farming/animal husbandry | 10 |
| Raid director/threat AI | 10 |
| Logistics/production/survival/exploration | 19 |
| **Total** | **119** |

## 5. Current implementation evidence by domain

| Domain | Source evidence exists on | Observed scope and open boundary |
|---|---|---|
| Core result/request | Phase 2 and descendants: `GameplayCoreTypes.h/.cpp` | Stable result codes, operation request envelope and unitized resource amount exist as source; UHT/UBT and integration remain unverified |
| Object identity | Phase 2 and descendants: `WorldObjectIdentityComponent`, `WorldObjectIdentitySubsystem` | Optional instance GUID/definition/lifecycle metadata. It is not automatically attached to every actor and is not itself a save coordinator |
| Committed events | Phase 2 and descendants: `WorldCommittedEventSubsystem` | Transient notification after commit; not a durable event log/replay/persistence layer |
| Wood | Main plus Phase 1/3/descendant branches: `LogBuildPiece`, `LogCarryComponent` | Existing geometry-state and snap code; no full physical tool impact/progress/resource-wear lifecycle; branch-specific snap paths must be reconciled |
| Stone | Phase 4 and descendants: `StoneBuildingPiece`, `StoneBuildingTypes` | Explicit piece and anchor logic; full support/load/damage/persistence and every M029–M038 behavior not claimed |
| Sticks/tools/traps | Phase 5 and descendants: `StickPrimitivePiece`, `StickPrimitiveTypes` | Initial state/anchor/trap prerequisite layer; combat, inventory, lighting and build integration remain |
| Inventory/crafting | Phase 6 and descendants: `InventoryCraftingComponent`, `InventoryCraftingTypes` | Stack quantities/slots/weight and basic recipe transaction; complete world pickup, user-facing crafting flow and durable state not demonstrated |
| Ownership | H integrated branch and descendants: `WorldItemOwnershipSubsystem` | Stable physical item IDs, owner kinds, expected-revision transfers and idempotent receipts are present in source; stone/stick adapters, split/merge and output registration remain |
| Structural graph | H integrated branch and descendants: `WorldStructuralGraphSubsystem` | Support/removal contracts present in source; multi-actor destruction/salvage is not fully atomic and single-parent constraints remain |
| Terrain | H integrated branch and descendants: `WorldTerrainStateSubsystem` | Bounded, revisioned logical cell edits exist; this is **not** a runtime voxel renderer/collision/navigation adapter and does not yet make digging playable |
| Farming | H integrated branch and descendants: `WorldAgricultureSubsystem` | Logical crop/animal lifecycle exists; season/soil model, actor visuals/pathing, feed transaction and item-output connection remain |
| Threat/raids | H integrated branch and descendants: `WorldThreatDirectorSubsystem`, raid adapter | Scheduling and adapter source exist; full enemy selection, navigation, pursuit/attack and settlement-aware AI are not claimed |
| Logistics/production | H integrated branch and descendants: cargo, route, production and storage subsystems | Logical queues/cargo/ownership links exist; actual machine/fuel/power actors and physical cargo pathing remain |
| Save/load | H integrated branch and descendants: `WorldSaveCoordinatorSubsystem`, `WorldSimulationSaveGame` | Logical manifest and restore coordinator exist in source; physical actor transforms, structural links, rendered/collision terrain and version migration are incomplete. Save refuses while an item is Held in the reviewed version |
| Voxel technology | No completed engine-validated backend established by this audit | Runtime mesh edit, collision/nav update, support interaction, persistent terrain delta and performance gate still required |

## 6. Current snap-specific known risks

Project memory and the latest snap-gallery notes describe the newer work as visually inspected by its author, but not yet verified by the user in PIE. Remaining known risks include:
- Mirrored full-length diagonal wall logs intersect/clip at the crossing.
- 45-degree mitre end cuts are not implemented.
- `GatherPlacedBodies` scans all world actors on each solve, with a scalability risk.
- Seven joinery meshes require manually authored `NotchCradle` sockets.
- The contextual deck-placement path remains untested.
- Major source files contain very large methods/classes and need bounded refactoring after behavior is stabilized.
- A screenshot gallery is useful visual evidence; it is not the same as a reproducible PIE acceptance test.

## 7. Phase 00 conclusions and concrete outputs

- The canonical registry has been imported rather than recreated.
- The source history is stacked; do not independently merge phases 1–6 and A–H.
- `feature/simple-intent` is the candidate to review first for full current code/content, because it is a descendant of the stacked domain line. Main still needs a later, explicit integration comparison so main-only documentation is not lost.
- Existing operation/result, identity, ownership, event, inventory and terrain contracts must be reconciled, not duplicated.
- No Unreal build, UHT/UBT, Editor or PIE result is claimed here.

### Phase 00 acceptance

- [x] Correct repository verified: `furkansarikiyak36-eng/myproject3`.
- [x] Main baseline recorded.
- [x] Staged branch ancestry compared.
- [x] Canonical M001–M119 registry copied and record counts checked.
- [x] Source implementations distinguished from engine/gameplay verification.
- [x] Known cross-system gaps recorded.
- [ ] Full source-diff audit of the latest candidate branch completed.
- [ ] Local/untracked Blueprint references and actual user Editor state reviewed.
- [ ] Unreal build/UHT/UBT and PIE acceptance executed at the authorized validation gate.

Phase 00 is closed for the current GitHub source/branch inventory only. It is not a final code or runtime audit.
