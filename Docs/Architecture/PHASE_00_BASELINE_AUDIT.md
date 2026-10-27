# Phase 00 — Repository Baseline and Scope Audit

**Audit date:** 2026-10-27  
**Repository:** furkansarikiyak36-eng/myproject3  
**Current main SHA:** 65faad8602d4e14e894dc2cf4e3d507ca39cbb1d  
**Audit branch:** planning/gdd-inventory-production-roadmap  
**Method:** GitHub source/document inspection and branch comparison. No Unreal build, UHT, Editor, PIE, save/load, or performance test was run during this audit.

## 1. Baseline conclusion

The current main branch is a small log-carry / log-processing / snap prototype. It is not the latest accumulated implementation branch recorded in the repository's memory. Several later branches contain a shared core contract layer, wood-result handling, stone and stick pieces, inventory/crafting, world simulation subsystems, save snapshots, and a newer placement kernel. These are candidate source layers, not proof that the whole game is integrated or validated.

The practical rule is therefore:

- Use main as the reference for the current default branch and its user-facing README.
- Use feature/simple-intent as the leading candidate for the next integrated code baseline, because the branch graph shows it descends from both phase-06-inventory-crafting and mechanic-detailing-pass-h-integrated-world-systems.
- Keep this planning branch documentation-only. Do not merge large code histories, overwrite the newer placement kernel, or label unverified code complete.
- Reconcile the three newer main commits (memory, decisions, screenshots/documentation) carefully when integrating. The compare result shows those main commits did not change the gameplay source files, but they can still touch shared documentation paths.

## 2. Current main source evidence

The main README describes the prototype around:

- Source/MyProject3/LogBuildPiece.*: log piece type, mesh variants, snap points, attachment links and guarded state/mesh changes.
- Source/MyProject3/LogCarryComponent.*: pickup/drop, held preview, placement/snap search and processing selection.
- Source/MyProject3/LogSnapCharacter.* and LogSnapGameMode.*: player/test setup.
- Source/MyProject3/LogBuildingTypes.h: current piece/action/anchor types.
- UnrealEditorScripts/BuildLogSnapPrototype.ps1 and CreateLogSnapTestLevel.py: build helper and a generated log test map.

The README and prototype notes explicitly limit the current layer: processing selects pre-authored mesh variants rather than cutting geometry procedurally, and the first Unreal editor build/play session is not claimed by those documents.

### Main-branch mechanic evidence found

These entries have **partial source evidence** in the reviewed main files. This is not the same as a complete mechanic or successful runtime verification.

| ID | Registry mechanic | Evidence inspected | Limitation |
|---|---|---|---|
| M011 | Pick up and carry a loose log | LogCarryComponent.*, LogBuildPiece.*, LOG_SNAP_PROTOTYPE.md | Prototype path only; full ownership/save contract not established in main |
| M012 | Drop or quick-release a carried log | Same carry/piece sources | Physical settling and all edge cases require PIE validation |
| M016 | Split a whole log longitudinally | LogBuildPiece.*, LogBuildingTypes.h, prototype notes | Pre-authored variant swap; tool impact/progress not implemented |
| M017 | Quarter a split-half log | Same wood-processing sources | State transition exists as a prototype; runtime and persistence unverified |
| M018 | Hew or square a log | Same wood-processing sources | Variant configuration dependency; runtime and persistence unverified |
| M019 | Cut a longitudinal groove only after splitting | Same wood-processing sources | Prototype calls this gutter/groove; full cutting interaction is not implemented |
| M020 | Make a trough or channel | Same wood-processing sources | Variant selection, not procedural carving |
| M021 | Produce a plank from a split log | Same wood-processing sources | Variant selection; inventory/output transactions not present in main |
| M024 | Generate and validate snap anchors | LogBuildPiece.*, LogBuildingTypes.h | Current prototype anchor model only; does not prove the newer shared placement kernel is integrated |
| M025 | Place logs horizontally, vertically and diagonally | LogCarryComponent.*, prototype controls | Manual/free placement modes in this branch; newer contextual resolver belongs to a separate branch family |
| M026 | Maintain explicit parent-child construction links | LogBuildPiece.* | Log-piece attachment only; not the full world-wide structural graph |

No M001–M119 item is marked VERIFIED by this audit. Everything else is marked in the companion CSV as either no evidence found in the reviewed main entry points or not yet audited deeply enough. That wording is intentional: lack of evidence in the inspected files is not proof that no local/untracked Blueprint or editor content exists.

## 3. Canonical M001–M119 registry

There are two materially different registry revisions in the branch history, so their status fields must not be blended:

- The phase-01-foundation-v2 / phase-06 / Phase H revision (CSV blob ea1e3cc) has 119 unique IDs and marks all 119 unreviewed with static review, automation, build and runtime fields set to not_run.
- The newer feature/simple-intent revision (CSV blob 04fe4e0; current candidate version in this planning branch) contains the same 119 IDs but marks 90 as implemented_unverified and 29 as unreviewed; all 119 have static_review=fail; automated_test is pass for 17 and not_run for 102; unreal_build is pass for 90 and not_run for 29; runtime_test is not_run for all 119. The JSON records evidence strings, including a UE 5.8.4 editor build on integration/canonical-log-model dated 2026-10-26 and selected automation test names. These are useful evidence pointers, but the single build result does not independently prove every listed mechanic's acceptance criteria. Static review is explicitly not passing and no runtime test is recorded.
- All 119 JSON entries currently have phase=1. The field therefore does not distinguish the implementation phases described by the separate 14-phase roadmap. The companion baseline CSV keeps CanonicalRegistryPhaseField and ProposedImplementationPhase in separate columns rather than silently rewriting the canonical registry.
- **Acceptance-criterion quality gap:** 99 of 119 registry criteria still use the same generic “valid context produces the documented result; invalid prerequisites fail without partial mutation” template; only 20 are more mechanic-specific. The traceability matrix currently gives symbol-level candidate mappings for all 119 rows, but full owning-function/caller chains and detailed positive/negative/persistence evidence steps are still open. The phase cannot close merely because all IDs are assigned.

**Important provenance limit:** the registry itself states that the historical “119 passed” report did not provide the exact historical 119 names in the reviewed materials. The current registry therefore locks a declared 119-item scope; it does not claim to reconstruct that older list. Do not turn historical counts or a shared build entry into current per-mechanic passing claims.

Canonical files in this branch:
- MECHANIC_REGISTRY_119.json — machine-readable registry.
- MECHANIC_REGISTRY_119.md — readable scope and rules.
- MECHANIC_REGISTRY_119.csv — planning/test export.
- MECHANIC_REGISTRY_SCHEMA.json — row validation schema.
- PHASE_00_M001_M119_BASELINE.csv — current-main evidence plus candidate branch mapping, kept separate from the canonical registry's verification status.

## 4. Candidate branch findings

| Branch | Compare against current main | Source-layer evidence | Audit conclusion |
|---|---|---|---|
| phase-01-foundation-v2 | 58 commits ahead, 3 behind | Canonical scope registry, source/call-chain inventory; edits to LogBuildPiece.* and LogCarryComponent.cpp | Useful Phase 01 foundation and compatibility safeguards; requires review/reconciliation |
| phase-02-core-contracts | 85 ahead, 3 behind | GameplayCoreTypes.*, identity component/subsystem, committed-event subsystem | Shared contracts exist as a source layer; UHT/build/runtime validation remains open |
| phase-03-wood-processing | 91 ahead, 3 behind | Result-bearing wood processing entry point and docs | Initial layer, not full physical cutting, tool wear, inventory transaction or save/load |
| phase-04-stone-masonry | 98 ahead, 3 behind | Stone piece/types/anchor source | Initial piece/anchor layer, not all M029–M038 behaviors |
| phase-05-stick-primitives | 104 ahead, 3 behind | Stick piece/types and trap prerequisite layer | Initial layer; integration and validation remain open |
| phase-06-inventory-crafting | 110 ahead, 3 behind | Stack/recipe structures and inventory/crafting component | Initial transaction layer; full world pickup/UI/save integration remains open |
| mechanic-detailing-pass-h-integrated-world-systems | 349 ahead, 3 behind | Broad candidate source for structure graph, terrain state, agriculture, threats, survival, logistics, production, storage, coordinator and save | Broad stacked source layer; closeout says final integrated validation has not run |
| feature/simple-intent | 696 ahead, 3 behind | Shared placement kernel, contextual interaction and current snap development; includes the Phase H and Phase 06 histories | Preferred candidate integration baseline after source review; do not assume build/runtime verification |

The GitHub compare tool caps returned file listings at 300 files for the large snap branches, so their changed-file list is not a complete count. The ahead/behind numbers are repository history relationships, not quality or completion scores.

## 5. Branch topology decision

The comparisons establish that:

- phase-02-core-contracts is directly stacked after the Phase 01 foundation.
- Phase 03, 04, 05 and 06 are sequentially stacked after their preceding phase branches.
- mechanic-detailing-pass-h-integrated-world-systems contains the accumulated earlier phase work.
- feature/simple-intent is 586 commits ahead of phase-06-inventory-crafting with no behind commits, and 347 commits ahead of mechanic-detailing-pass-h-integrated-world-systems with no behind commits. It therefore descends from those implementation histories rather than being a separate sibling to merge wholesale.
- main has three later commits relative to the older baseline; review their memory/documentation changes before any integration.

**Implementation recommendation:** continue code work from feature/simple-intent as the candidate integrated line, then reconcile the latest main documentation/memory changes. Do not merge phase-01 through phase-06 and Phase H individually into it; those commits are already in its ancestry. The new planning branch remains separate and can later be merged as documentation once paths and status language are reviewed.

## 6. Architecture and scope discrepancies to resolve

1. A Phase 01 foundation file refers to “16 development phases”, while the accumulated ARCHITECTURE_14_PHASE_ROADMAP.md and actual phase branches define Phases 01–14. This plan adopts the existing 14-phase implementation stack plus a separate Phase 00 audit. The legacy 16-phase mention remains a documented discrepancy; do not silently renumber mechanic IDs or add phantom phases.
2. The Phase 01 operation/event/persistence document is a design contract proposal. The Phase 02 branch later implements several source types, but its own closeout says event dispatch is transient and world save/load is not implemented there.
3. The full world save snapshot and subsystem classes appear on the accumulated Phase H / simple-intent line; source presence does not prove that restore ordering, migration, or in-editor persistence works.
4. The newer snap kernel and the original prototype use different architecture paths. Code integration must preserve one authoritative placement system, not keep parallel kernels active.

## 7. Acceptance state

- [x] Correct repository and current main SHA recorded.
- [x] Current main README and key log source headers inspected.
- [x] Registry revisions compared: same 119 IDs, but status/evidence fields differ; current phase-field and per-mechanic evidence gaps recorded.
- [x] Candidate phase branches and their history relationships compared.
- [x] Current-main evidence separated from non-main candidate source layers.
- [ ] Full M001–M119 line-by-line source audit on the preferred candidate branch.
- [ ] Diff/compatibility review for public APIs and runtime state ownership.
- [ ] UE 5.8 build/UHT, editor load, PIE and save/load validation.


## 8. Catalogue validation evidence added after initial audit

- GitHub Actions run [40583305901](https://github.com/furkansarikiyak36-eng/myproject3/actions/runs/40583305901), commit `cdaf57753b27898f091a85ced9fac1befb570d9b`, completed successfully on 2026-10-27.
- The validator reports: 14 JSON Schema Draft 2020-12 schemas pass meta-schema validation; the M001–M119 registry contains 119 unique IDs with valid entries and resolved mechanic dependencies; 45 sample definitions validate; sample IDs are unique within each schema family.
- The initial failure was a real contract/sample mismatch: the shovel sample listed `Sand` as a tool target while the tool schema did not allow it. The tool/action schemas now list `Sand` and `Gravel`; the soil-excavation shovel sample is narrowed to `Soil`, matching its action family and required `SOIL` tag.
- This is **catalogue/schema CI only**. It does not validate the full set of external definition references, compile Unreal C++, load Unreal assets, or prove gameplay/PIE/save-load behavior.

The Phase 00 audit remains open until the per-mechanic source/caller review and the target-engine verification gates are completed.
