# Phase 3 — Wood Processing Asset Matrix and Integration Gates

**Branch:** `phase-03-wood-processing`  
**Source of filenames:** `AssetPackages/UnrealExport/` and `AssetPackages/UnrealExport/Archive/`  
**Status:** Filename inventory only; no FBX bounds/pivot/collision values are asserted.

## Candidate mesh families from repository filenames

| Family | Existing filename evidence | Candidate role | Still must be verified |
|---|---|---|---|
| Whole log | `SM_RawLog_150.fbx`, `SM_RawLog_300.fbx`, `SM_RawLog_600.fbx` | Whole-log source geometry | Actual local long axis, dimensions, pivot, collision |
| Split half-round | `SM_RawLog_75_Half.fbx`, `SM_RawLog_150_Half.fbx`, `SM_RawLog_300_Half.fbx`, `SM_RawLog_600_Half.fbx` | `SplitHalf` output | Confirm each variant matches the source length and is a split half-round rather than a half-length log |
| Quarter | `SM_RawLog_75_Quarter_04.fbx`, `SM_RawLog_150_Quarter_04.fbx`, `SM_RawLog_300_Quarter_04.fbx`, `SM_RawLog_600_Quarter_04.fbx` | `Quarter` output | Same-length pairing, section orientation and pivot |
| Gutter | `SM_RawLog_75_Gutter.fbx`, `SM_RawLog_150_Gutter.fbx`, `SM_RawLog_300_Gutter.fbx`, `SM_RawLog_600_Gutter.fbx` | `Gutter` output | Same-length pairing; ensure the groove face is oriented as expected |
| Trough | `SM_RawLog_75_Trough.fbx`, `SM_RawLog_150_Trough.fbx`, `SM_RawLog_300_Trough.fbx`, `SM_RawLog_600_Trough.fbx` | `Trough` output | Same-length pairing, cavity orientation and collision |
| Hewn | `SM_RawLog_75_Hewn1/2.fbx`, `SM_RawLog_150_Hewn1/2.fbx`, `SM_RawLog_300_Hewn1/2.fbx`, `SM_RawLog_600_Hewn1/2.fbx` | `Hewn` output candidates | Decide whether 1/2 are alternate styles, stages, or different section contracts; do not choose automatically by filename |
| Kalas / squared beam | `SM_RawLog_75_Kalas.fbx`, `SM_RawLog_150_Kalas.fbx`, `SM_RawLog_300_Kalas.fbx`, `SM_RawLog_600_Kalas.fbx` | `Kalas` output | Same-length pairing and rectangular section dimensions |
| Plank | `SM_RawLog_75_Plank.fbx`, `SM_RawLog_150_Plank.fbx`, `SM_RawLog_300_Plank.fbx`, `SM_RawLog_600_Plank.fbx` | `Plank` output | Confirm expected yield/count and whether one mesh represents one plank or a plank-shaped log section |
| Special joints | `Archive/SM_RawLog_75_*` (cross-lap, fork, half-lap, saddle, V-notch, cradle/mate) | Later structural joint variants | Not ordinary cut outputs; need separate joint contracts and matching pairs |
| Short/long raw logs | `SM_RawLog_Short_01..04.fbx`, `SM_RawLog_Long_01..08.fbx` | Additional source lengths | Actual length class, orientation and which output families exist |

## Current implementation gap

`ALogBuildPiece` currently has one output-mesh property per action (`SplitHalfVariantMesh`, `QuarterVariantMesh`, etc.). It does not select an output by source length or section. That means a configured output could be the wrong length for a given source actor even when non-null. This must not be “fixed” by guessing dimensions from filenames or assigning a universal mesh to every length.

## Required next implementation contract

1. Create an explicit, authored processing-mesh set keyed by source mesh/length class; include a defined fallback policy and reject ambiguous/missing mappings before any mutation.
2. Keep old per-action properties as a migration fallback until Blueprint defaults have been migrated and audited.
3. Define whether `Hewn1` and `Hewn2` are alternative recipes or distinct outputs before binding them.
4. Record validated import data per mesh: local axis, dimensions in centimeters, pivot/origin, transform scale, collision mode, and section orientation.
5. Add automated source-level checks for duplicate/missing family mappings; then verify the imported assets in Unreal Editor.

## Integration boundaries

- The tracked source scan currently finds no canonical inventory/tool-durability component. Wood processing must not create a second inventory authority. Tool cost/durability and resource yields stay open until the canonical owner and transaction API are identified.
- Stable actor identity is available only when `UWorldObjectIdentityComponent` is present; the current optional identity is not a save/load implementation.
- Runtime geometry, pivot, collision, and gameplay checks are still required in Unreal Engine 5.8. None is claimed by this filename-only matrix.
