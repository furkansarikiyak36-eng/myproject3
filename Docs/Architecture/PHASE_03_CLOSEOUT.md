# Phase 3 Closeout — Wood Processing

**Decision:** Source implementation advanced; Phase 3 remains open pending asset authoring, domain integrations, and Unreal validation.  
**Branch:** `phase-03-wood-processing`  
**Base:** `phase-02-core-contracts`

## Delivered in the current source pass

- Added result-bearing processing API while retaining the legacy boolean wrapper.
- Kept split-half prerequisites for quarter/gutter/trough/plank and whole-log prerequisites for split/hewn/kalas.
- Added a tracked-C++ caller audit; the gameplay input caller consumes structured failure messages.
- Documented operation IDs as correlation metadata, not idempotency keys.
- Prevented processing when any snap anchor is occupied, in addition to placed/held/attached-child checks.
- Added exact-source-mesh processing output sets. If any sets are configured, duplicate mappings conflict, unmapped sources fail closed, and missing action outputs are rejected. Legacy per-action properties remain as a migration path only when no sets are configured.
- Added an asset-family matrix based on repository filenames and recorded which assumptions still require real import inspection.

## Remaining implementation gates

- Populate and verify source/output mappings in Unreal actor/Blueprint defaults; filenames are not proof of dimensions or pivot alignment.
- Inspect actual FBX local axes, dimensions, pivot/origin, scale, collision, and same-length geometry pairings.
- Identify the canonical inventory/tool-durability owner before adding tool costs or durability mutation; no such owner was found in the tracked C++ source scan.
- Define authoritative resource yield/consumption for each action and connect it to the canonical inventory transaction rather than duplicating inventory state in `ALogBuildPiece`.
- Decide whether physical cutting progress/intermediate geometry belongs in this phase or a later interaction phase.
- Integrate processed piece type and identity with the canonical save/load path and construction structural graph once those owners are identified.
- Audit local/untracked Blueprint callers in the actual checkout.

## Validation boundary

No Unreal Engine 5.8 UHT/build, Editor, PIE, or gameplay test was run. No FBX dimensions/pivots/collision values were measured from the imported Unreal assets. No runtime gate or all-119 mechanic is marked passed.

The Phase 2 voxel-terrain gaps remain deferred as requested; this Phase 3 branch does not imply Phase 2 passed its exit gate.
