# State (updated 2026-10-27)

UE 5.8 C++ survival / log-building game in the style of Sons of the Forest. Solo
developer; replies in Turkish. Main test level: /Game/DemoAssets/Maps/VerticalSlice_C.

## Branches

| Branch | State |
|---|---|
| main | Behind; the snap work is not merged yet. |
| feature/global-snap-kernel | Pushed. One placement kernel for all materials. |
| feature/contextual-interaction-resolver | Work on top of the kernel (contextual targets, aim cuts). |
| feature/simple-intent | Preferred candidate implementation line: current contextual placement/snap kernel; branch history descends from phase-06-inventory-crafting and mechanic-detailing-pass-h. Ahead of main with 3 newer main commits to reconcile (primarily memory/docs/screenshots). Must still pass UE build/PIE/save validation. |
| planning/gdd-inventory-production-roadmap | Documentation-only branch based on main. Adds GDD/catalogues, Phase 00 baseline/branch audit, 119-row source-evidence matrix, Phase 01 shared data contracts, JSON schemas, and wood transition contract. No gameplay C++ or Unreal content changed. |
| PR #34 architecture/global-snap-variation-library | Has a second, conflicting kernel. Recommended: close it. |

## Works (as recorded in project memory; verify the correct branch before relying on it)

- One placement kernel (Source/MyProject3/Placement/): solve is pure, preview equals commit, all-or-nothing commit.
- Logs: End.A, End.B, Station.NN every 75 cm (centres 37.5 + 75·i); joints by id.
- Contextual placement (no modes): post top → across, wall top → next row, side → T, end → continue, ground → lie, looking down → post, post side under a beam → strut. R switches to manual modes.
- Axe: aim decides the cut; the end face splits the log lengthwise.
- Symmetry: centred on a post top on a 45° grid, post-to-post spans, wall diagonals and their mirror, corner to corner, mirrored struts.
- End-to-end joints bend 45 / 90 / 135 toward the carry direction.

## Open

- Main is behind the newer snap work; compare branch histories before integration.
- Snap work was checked by eye with mp3.SnapGallery (screenshots), not yet by the user in PIE.
- Two mirrored wall diagonals of full-length logs clip into each other where they cross.
- Mitre (45° end) cuts do not exist; cut ends stay square.
- GatherPlacedBodies scans the whole world on every solve (performance risk).
- 7 joinery meshes need hand-placed NotchCradle sockets.
- The deck path in contextual placement is untested.
- Code quality: CommitCutAction is ~450 lines; LogCarryComponent.cpp and LogBuildPiece.cpp are 3000+ lines.
- Current planning open items: map M001–M119 to real code/content evidence, review snap branch diffs, and validate the UE 5.8 voxel approach with a bounded edit/save-load prototype before committing to full voxel production.

## Phase 00–01 audit (2026-10-27)

- Planning branch is documentation-only and remains unmerged.
- Candidate code baseline is feature/simple-intent, not main; do not merge ancestor phase branches separately into it.
- Two M001–M119 registry revisions exist. Earlier phase-01-v2 has 119 unreviewed/not_run entries. The current feature/simple-intent revision has 90 implemented_unverified, 29 unreviewed, static_review=fail for all, automation pass for 17, UnrealBuild=pass for 90, and runtime_test=not_run for all. The build evidence is not per-mechanic completion evidence.
- The candidate registry JSON gives phase=1 to every entry; the Phase 01–14 plan uses a separate provisional mapping in Docs/Architecture/PHASE_00_M001_M119_BASELINE.csv.
- Added GAMEPLAY_DEFINITION_CATALOG_SCHEMA.json, FEEDBACK_EVENT_CATALOG_SCHEMA.json, and WOOD_PROCESSING_TRANSITIONS.json. JSON syntax and CSV row/ID counts were checked, but formal JSON Schema validation and Unreal runtime loading are not implemented/verified.
- Phase 00 audit, branch topology, Phase 01 contracts, plan alignment and GDD index are updated. UE 5.8 build/UHT, PIE, save/load and the per-mechanic code audit remain open.
