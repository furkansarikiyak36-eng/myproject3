# 2026-10-27 · ChatGPT · GDD and production roadmap
Branch: planning/gdd-inventory-production-roadmap
Base: main at 65faad8602d4e14e894dc2cf4e3d507ca39cbb1d

## Done
- Verified the repository is furkansarikiyak36-eng/myproject3 (not the older unrealgame repository).
- Opened a documentation-only planning branch.
- Added a GDD index and master design scope.
- Added an item/resource catalogue and logical world/VFX/animation/SFX/music catalogue.
- Added phase-by-phase implementation order, system dependencies, acceptance rules, and a reusable task template.

## Found
- main has the first log-carry/snap prototype and is behind newer snap work noted in memory.
- feature/global-snap-kernel, feature/contextual-interaction-resolver, and feature/simple-intent must be reviewed as integration inputs; the planning branch does not replace or merge them.
- The project README explicitly describes the current log prototype as uncompiled/unplayed in the documented baseline; do not claim runtime verification without fresh evidence.

## Open
- Finish per-mechanic source review on feature/simple-intent and record source paths/API callers.
- Reconcile the three newer main docs/memory commits into the candidate implementation branch only after checking overlaps.
- Decide the UE 5.8-compatible voxel approach through a bounded runtime-edit/save-load prototype.
- Wire the Phase 01 definition/event schemas into an appropriate validation path after API review.
- Run Unreal 5.8 build/UHT, editor/PIE, and save/load checks; none were run in this ChatGPT session.

## Phase 00–01 work continued
- Added Phase 00 baseline audit and branch integration matrix.
- Copied canonical M001–M119 scope/schema and prior Phase 01–02 architecture records into Docs/Architecture.
- Added a 119-row traceability baseline separating current-main evidence from candidate-branch evidence; 119 unique IDs checked.
- Found registry-version discrepancy: old phase-01-v2 registry is all unreviewed/not_run; feature/simple-intent registry has 90 implemented_unverified, 29 unreviewed, static review fail for all, automated pass for 17, build pass for 90, runtime not_run for all. JSON phase field equals 1 for all 119.
- Compared branch history: feature/simple-intent descends from Phase 06 and Phase H; recommended as candidate implementation line, not runtime-verified.
- Aligned the plan to Phase 00 audit plus the existing Phase 01–14 stack; noted legacy “16 phases” mismatch.
- Added shared data contract, gameplay definition catalog schema, feedback event schema and wood processing transition map; checked JSON syntax and CSV count only, not formal schema validation.
- No gameplay C++ or Unreal content changed; no Unreal build, UHT, PIE or save/load was run.


## Catalogue validation follow-up
- Investigated failed GitHub Actions run 40582999713: the shovel sample listed Sand while the tool capability schema excluded Sand.
- Updated tool and action target-family schemas to include Sand/Gravel and narrowed the current soil excavation shovel fixture to Soil, consistent with ACTION_EXCAVATE_SOIL.
- GitHub Actions run 40583305901 passed on commit cdaf57753b27898f091a85ced9fac1befb570d9b: 14 schemas, 119 unique registry IDs/dependencies, 45 sample records and sample-ID uniqueness passed.
- Updated Phase 01 checklist and Phase 00 audit with the exact CI evidence. This was catalogue validation only; no Unreal build/UHT/Editor/PIE/save-load test ran.
