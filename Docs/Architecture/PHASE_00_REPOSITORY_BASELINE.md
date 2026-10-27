# P00 — Repository Baseline and Branch Reconciliation

**Snapshot:** 2026-10-27  
**Planning branch:** `planning/gdd-inventory-production-roadmap`  
**Current default branch:** `main` at `65faad8602d4e14e894dc2cf4e3d507ca39cbb1d`  
**Engine target:** Unreal Engine 5.8  
**Evidence level:** GitHub source/docs/branch inspection only. No Unreal build, UHT/UBT, Editor, PIE, save/load run, or runtime performance measurement was performed by this audit.

## 1. Decision summary

The project does not have one clean, validated implementation branch for all of the intended systems. `main` is the small current baseline, while several stacked branches contain substantial source and documentation work. The most comprehensive inspected candidate is `feature/simple-intent`, which contains the H world-system source stack plus later snap changes. It is **not automatically a safe merge target**: comparison with current `main` reports 696 commits ahead, 3 behind, and 300 changed paths reported by the compare API; it still requires whole-branch review and target-engine verification.

Do not restart the game systems on `main` by recreating classes. Do not merge a 300-path candidate based only on the branch/PR title. Review the candidate source as a whole, then integrate through explicit acceptance gates.

## 2. Current baseline

### Main branch

- Current checked HEAD: `65faad8602d4e14e894dc2cf4e3d507ca39cbb1d`.
- The current README describes the first log-carry/snap prototype: `ALogBuildPiece`, `ULogCarryComponent`, `ALogSnapCharacter`, `ALogSnapGameMode`, and the test-map/editor scripts.
- The README explains that the prototype uses pre-authored mesh variants and does not yet procedurally cut geometry or implement the whole construction rule set.
- Project memory says the latest snap work lives on `feature/simple-intent` until integrated.
- This audit did not run a local editor build or PIE. That is not a statement that the user's local work has never compiled; it means no fresh build/runtime evidence was available in this GitHub audit.

### Canonical mechanic scope

The prior Phase 1 foundation branch provides the machine-readable registry now copied into this branch:

- `MECHANIC_REGISTRY_119.json` — source of truth.
- `MECHANIC_REGISTRY_119.csv` — spreadsheet/test-planning view.
- `MECHANIC_REGISTRY_119.md` — human-readable catalogue and invariants.
- `MECHANIC_REGISTRY_SCHEMA.json` — entry validation schema.

The registry contains exactly 119 unique IDs, M001–M119. The registry's own provenance says this is an explicit scope baseline established by that work, **not a verified recovery of the exact historical 119-test list**. All 119 entries remain `unreviewed`; static/automated/build/runtime evidence columns are `not_run`. Do not treat the number 119, the registry, or an earlier "119 passed" report as proof that these current definitions pass one-to-one.

The older `MECHANIC_COVERAGE_AUDIT_PASS_01.md` was based on the `phase-06-inventory-crafting` source snapshot. Its 119 rows classify 43 as `partial`, 1 as `contract-only`, 1 as `present-in-layer`, 1 as `transient-only`, and 73 as `not-found`. **This is a historical coverage snapshot, not a current classification of the full `feature/simple-intent` tree.** The new traceability matrix preserves that snapshot and adds a per-mechanic candidate source-path review field; every row still requires exact function/caller validation.

A more recent gap analysis reports that 99 of the 119 acceptance criteria still use a generic template and only 20 are mechanic-specific. That is a scope-quality gap to fix before treating the registry as an executable test plan.

## 3. Branch landscape and decision

See [PHASE_00_BRANCH_INTEGRATION_MATRIX.csv](PHASE_00_BRANCH_INTEGRATION_MATRIX.csv) for the branch-by-branch ledger.

### Foundation and domain phase stack

- `phase-01-foundation-v2`: registry, architecture rules, source inventory, transition/call-chain documents and focused snap/attachment safeguards. Compared with current main: 58 ahead, 3 behind; 16 changed paths.
- `phase-02-core-contracts`: adds `GameplayCoreTypes`, world identity component/subsystem and committed-event notification subsystem. Compared with current main: 85 ahead, 3 behind; 27 changed paths. Its closeout calls the work source-contract complete but says UHT/UBT and Editor/PIE remain open.
- `phase-03-wood-processing`, `phase-04-stone-masonry`, `phase-05-stick-primitives`, and `phase-06-inventory-crafting` form a stacked implementation series. Phase 6 is 110 commits ahead and 3 behind main with 48 changed paths.
- `mechanic-detailing-pass-h-integrated-world-systems`: 349 ahead, 3 behind, 111 changed paths in the compare result. It contains source layers for clock/coordinator, ownership, structure graph, terrain state, agriculture, threat, logistics/production, survival, and save coordination.
- `feature/simple-intent`: 696 ahead, 3 behind main; descendant of the H branch (347 ahead, 0 behind that branch). It contains the broad source stack and newer snap work. Its documented status still says no UHT/UBT, Editor, PIE, or gameplay validation was run.

### Snap-system line

`feature/global-snap-kernel`, `feature/contextual-interaction-resolver`, `feature/curated-snap-complements`, and `feature/simple-intent` form an evolving snap line according to the project memory and branch comparisons. PR #34's alternate snap/variation library is flagged by the existing integration audit as a second/conflicting kernel. Do not combine it automatically with the kernel/resolver line represented by PRs #35, #37 and #38. The governing decision remains **one material-neutral placement kernel with material-specific providers**.

### Main-only commits to reconcile

The merge base for the broad candidate is `ff2fd51444463afad598b837e820dbc3df2d439b`. Current main has three commits after that base: the wood-mechanics acceptance-gates document and two project-memory updates. These are small by intent but still must be preserved/reconciled when choosing an integration direction.

## 4. Current candidate architecture observed in source

The following source-layer contracts exist on the candidate stack. Existence is not engine/runtime proof.

| Concern | Candidate source | Current interpretation / open condition |
|---|---|---|
| Operation result/request | `GameplayCoreTypes.h/.cpp` | Reuse stable machine-readable results, operation IDs and validation envelope; no duplicate universal dispatcher. |
| Object identity/lifecycle | `WorldObjectIdentityComponent.*`, `WorldObjectIdentitySubsystem.*` | Opt-in identity and world-local registration. Domain actors are not all migrated merely because the component exists. |
| Operation idempotency | `WorldOperationLedgerSubsystem.*` | Operation receipts/deduplication help retries; the ledger does not itself make world mutations atomic. |
| Committed events | `WorldCommittedEventSubsystem.*` | Transient notification after a successful domain commit; not durable event persistence. |
| Item ownership | `WorldItemOwnershipSubsystem.*`, `InventoryCraftingTypes.h` | Unique physical item identity and inventory stack identity are separate; split/merge, actor respawn and processing-output adapters remain gaps. |
| Structural graph | `WorldStructuralGraphSubsystem.*`, material piece actors | Shared support/removal graph exists as a source layer; graph change + Unreal attachment + salvage/destruction is not proven to be one atomic transaction. |
| Terrain state/rendering | `WorldTerrainStateSubsystem.*`, `VoxelTerrainChunkComponent.*` | Logical layer-gated cell state and a smooth Marching Cubes chunk prototype exist. The component describes one chunk; neighboring chunk streaming, persistence and complete renderer/collision/navigation integration are not established. |
| Farming and animals | `WorldAgricultureSubsystem.*`, related actors | Logical lifecycle exists; season/soil data, feeding-item transaction, animal habitat/pathing and product-to-inventory ownership remain incomplete. |
| Threats | `WorldThreatDirectorSubsystem.*`, `WorldRaidAdapterSubsystem.*` | Threat/raid state source exists; full target selection, navigation, pursuit/attack and settlement-aware AI remain incomplete. |
| Logistics and production | `WorldCargoTransportSubsystem.*`, `WorldLogisticsRouteSubsystem.*`, `WorldProductionSubsystem.*`, storage | Logical routes/queues/ownership exist; physical transport travel and real production machine/fuel/output adapters remain incomplete. |
| Save/load | `WorldSaveCoordinatorSubsystem.*`, `WorldSimulationSaveGame.h` | Versioned logical snapshot source exists. Physical actor respawn/transforms, graph links and terrain mesh/collision/world-partition restoration remain open. |

## 5. Required Phase 00 outcomes

- [x] Current main HEAD and relevant branch candidates recorded.
- [x] Canonical M001–M119 registry copied into the planning branch without renumbering.
- [x] Historical source-coverage snapshot and its limits recorded.
- [x] A 119-row traceability worksheet created with names, owners, prerequisite/dependency fields, candidate source paths, old coverage snapshot, and explicit unverified fields.
- [ ] Verify every row against exact source functions/callers on `feature/simple-intent`; replace broad file-family candidates with symbols/line anchors.
- [ ] Review actual PR base/head and net diff for snap PRs; choose one placement kernel line.
- [ ] Reconcile current main-only commits and the large candidate branch without losing the user's latest main changes.
- [ ] Run current target UE 5.8 build/UHT and Editor/PIE checks at the agreed integration gate; record evidence separately.
- [ ] Decide the release/integration base after code review and target build, not from commit counts.

## 6. Branching rule going forward

The current branch `planning/gdd-inventory-production-roadmap` is the document/design branch based on current main. It contains no gameplay C++ or Unreal map edits. Use the broad `feature/simple-intent` branch as an **audit candidate**, not as an approved merge base. Once the source/call-site audit and contract reconciliation are complete, create a focused implementation branch from the deliberately chosen base and integrate through an explicit pull request. Do not pull all 696 commits into main as a substitute for review.
