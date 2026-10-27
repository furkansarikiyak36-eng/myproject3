# Phase 00 — Baseline Repository Audit

**Repository:** furkansarikiyak36-eng/myproject3  
**Audit date:** 2026-10-27  
**Current-main baseline:** 65faad8602d4e14e894dc2cf4e3d507ca39cbb1d  
**Audit mode:** GitHub file retrieval and branch comparison only. No local clone, Unreal build, UHT, Editor, PIE, or runtime test was run.

## Executive finding

The repository has substantial prior work spread across multiple stacked feature branches. The right next action is not to rewrite the same systems or merge an entire branch blindly. Keep main as the current baseline and use earlier phase branches as sources for reviewed, selective integration. Several candidate branches contain native C++ and content but are three commits behind the current mainline and diverge from it. They are not proven to compile or run against the current main.

## Current main source surface

The main README describes the project as an initial physical log-carry/place/snap prototype. The following tracked source paths were inspected:

| Path | Observed responsibility | Audit result |
|---|---|---|
| Source/MyProject3/LogBuildPiece.h/.cpp | Log type and mesh variants, processing prerequisites, snap points, parent/child attachment, held/placed state | Present; exact coverage of each M009–M028 item is not mapped yet |
| Source/MyProject3/LogCarryComponent.h/.cpp | Player trace, pickup/drop, held preview, snap/free placement and processing inputs | Present; runtime behavior not verified in this audit |
| Source/MyProject3/LogBuildingTypes.h | Piece types, cut actions and snap-point data | Present |
| Source/MyProject3/LogSnapCharacter.h/.cpp | Prototype player/input bindings | Documented in README |
| Source/MyProject3/LogSnapGameMode.h/.cpp | Prototype game mode | Documented in README |
| UnrealEditorScripts/BuildLogSnapPrototype.ps1 | Windows editor-target build helper | Documented; helper does not execute gameplay tests |
| UnrealEditorScripts/CreateLogSnapTestLevel.py | Creates the log playground map | Documented; must run within Unreal's Python environment |

The current main branch did not contain the following checked paths: GameplayCoreTypes.h, WorldObjectIdentityComponent.h, WorldCommittedEventSubsystem.h, StoneBuildingPiece.h, StickPrimitivePiece.h, InventoryCraftingTypes.h, WorldTerrainStateSubsystem.h, WorldAgricultureSubsystem.h, WorldThreatDirectorSubsystem.h, and WorldSaveCoordinatorSubsystem.h. This means those candidate implementations are not present at those exact paths in the checked mainline. It does not prove there is no equivalent logic elsewhere.

## Branch comparison

“Ahead/behind” describes commit topology, not quality or completion.

| Branch | Ahead | Behind | Changed-file view | Conclusion |
|---|---:|---:|---|---|
| phase-01-foundation-v2 | 58 | 3 | 16 files; includes LogBuildPiece.cpp/.h, LogCarryComponent.cpp, registry and foundation/audit documents | Useful source review and registry; older base needs reconciliation |
| phase-02-core-contracts | 85 | 3 | 27 files; adds operation types, identity component/registry and committed-event subsystem | Candidate shared contracts; no Unreal integration evidence |
| phase-03-wood-processing | 91 | 3 | Sequential stack after Phase 02; modifies LogBuildPiece and adds result-bearing processing | Review against current snap branch and existing callers |
| phase-04-stone-masonry | 98 | 3 | Adds stone piece/type layer after Phase 03 | Candidate code only; full stone mechanics are not claimed complete |
| phase-05-stick-primitives | 104 | 3 | Adds stick piece/types after Phase 04 | Candidate code only; interactions and persistence remain open |
| phase-06-inventory-crafting | 110 | 3 | Adds inventory/crafting types and component after Phase 05 | Candidate layer; world pickup/drop, UI and save integration remain open |
| feature/global-snap-kernel | 675 | 3 | Large code/content divergence from main | Do not merge wholesale; inspect kernel and content deltas |
| feature/contextual-interaction-resolver | 688 | 3 | Large code/content divergence from main | Review with kernel ancestry before any integration |
| feature/simple-intent | 696 | 3 | Latest snap-work input named in project memory; large content/code delta | Review first as the preferred snap input, not a drop-in merge |
| mechanic-detailing-pass-h-integrated-world-systems | 349 | 3 | 111 changed files in comparison, including many subsystem files and phase docs | Broad candidate stack, high integration risk; no verified end-to-end build claim |

Phase 03 descends from Phase 02, Phase 04 from Phase 03, and so forth. The integrated-world branch includes earlier work plus later systems. Do not combine the same changes from multiple stacked branches twice. Compare each child branch with its declared parent and with current main.

## Canonical mechanic registry

The Phase 01 candidate branch provides a scope registry with exactly 119 unique IDs M001–M119, in CSV, JSON and Markdown forms. Its provenance explicitly says the precise historical list behind an earlier “119 passed” report was not found. This registry defines the canonical scope from that branch onward; it does not claim to recover the historical test list.

All 119 entries are marked unreviewed; static review, automated tests, Unreal build and runtime checks are all not_run. These statuses remain unchanged until evidence is attached. The registry is copied to docs/PRODUCTION so this GDD branch can reference it without relying on another branch to read the scope.

See MECHANIC_TRACEABILITY_MATRIX.csv for a working sheet that preserves registry status and adds candidate branch, current-main evidence and audit fields.

## Compatibility findings

1. **Windows directory case collision:** legacy phase docs use a top-level Docs directory, while this planning branch uses docs. Do not check in both uppercase and lowercase paths into the same checkout; Windows/NTFS can treat them as the same directory. All new files here use lowercase docs.
2. **No duplicate core types:** Phase 02 already defines candidate FWorldOperationResult, operation/request/resource contracts, generic object identity and a transient committed-event notifier. Review and reuse or deliberately replace them only after source diff review.
3. **Generic lifecycle is not domain state:** log processing and snap state remain with the log domain; inventory, terrain, crops and structural graph keep their own authoritative state.
4. **Identity is not persistence:** the Phase 02 candidate says its identity registry is world-scoped and event notification is transient. It does not implement durable save/load.
5. **Inventory is not end-to-end:** Phase 06 provides stack/recipe structures and transactional crafting logic, but its closeout identifies missing world pickup/drop integration, UI/session flow and durable persistence.
6. **Snap work is not interchangeable:** current project memory records mirrored diagonal clipping, missing mitre end cuts, whole-world actor scans during snap solving, seven authored NotchCradle sockets and an untested deck path.
7. **Voxel remains a gate:** the integrated-world branch contains a bounded terrain-state candidate, but runtime voxel rendering, collision, persistence and UE 5.8 compatibility need a separate bounded prototype.

## Phase 00 disposition

**Static baseline report prepared; Phase 00 is not fully closed.**

Completed:
- Confirmed repository and current main baseline SHA.
- Identified mainline source responsibilities and checked candidate file presence/absence.
- Compared relevant stacked implementation branches.
- Copied the canonical registry without changing status.
- Added a traceability matrix with 119 unique entries.

Still open:
- Review feature/simple-intent and Phase 01–06 source diffs file-by-file.
- Map every M001–M119 acceptance criterion to concrete source/config/content paths.
- Inspect local/untracked Blueprint references and assets from the user's Windows checkout.
- Decide selective integration order and resolve duplicate/competing implementation paths.
- Run UE 5.8 build/editor/PIE gates when the actual project environment is available.

## Next phase

Phase 01 is a contract/schema reconciliation phase. Schema documents are design contracts only. Candidate C++ definitions on the stacked branches are reuse candidates; this planning branch must not create another competing C++ identity, event, inventory or terrain owner.
