# WOOD-003 — Log Mesh Variant Catalog and Geometry-State Contract

**Branch:** `fix/log-geometry-dimensions`  
**Base:** `fix/atomic-log-piece-splitting`  
**Review date:** 2026-10-25  
**Evidence:** exact GitHub filename inventory, repository README/import-script review and static C++ source review. **No FBX was imported or measured in Unreal for this review.**

## Executive status

The code now represents two independent attributes instead of overloading one enum for both:
- `ELogLengthFraction`: full, 3/4, 1/2 or 1/4 of the source log's nominal length.
- `ELogCrossSectionShape`: full-round, half-round, quarter-round, hewn, square, grooved, trough, plank, notched, joint or other.

`FLogPieceGeometryState` carries these fields on `ALogBuildPiece`. The legacy `ELogPieceType` remains for compatibility, display and existing call sites; geometry metadata is synchronized from it when a piece is authored by its legacy label. Accepted processing operations update the cross-section profile while preserving the current length fraction.

Action eligibility now uses cross-section shape:
- SplitHalf requires a full-round cross-section, but does not require the source's legacy label to equal `WholeLog`.
- Quarter, Gutter, Trough and Plank require a half-round cross-section.
- Hewn and Kalas require a full-round cross-section.

The new naming parse distinguishes a raw-log cross-section variant named like `SM_RawLog_150_Quarter_04` from a standalone legacy `QuarterLog` length-fraction label. This is a filename-derived interpretation; actual shape and pivot still require UE verification.

## Exact root FBX inventory

GitHub lists **48 root FBX files** and **10 FBX files in Archive**.

| Nominal mesh-name group | Base full-round FBX | Shape-variant suffixes seen | Count / open issue |
|---|---|---|---|
| 75 | **Missing** | FlatNotch_TopBottom, Gutter, Half, Hewn1, Hewn2, Kalas, Plank, Quarter_04, Trough | 9 FBX files; base-round absent |
| 150 | Present | Gutter, Half, Hewn1, Hewn2, Kalas, Plank, Quarter_04, Trough | 9 FBX files |
| 300 | Present | Gutter, Half, Hewn1, Hewn2, Kalas, Plank, Quarter_04, Trough | 9 FBX files |
| 600 | Present | Gutter, Half, Hewn1, Hewn2, Kalas, Plank, Quarter_04, Trough | 9 FBX files |

### Ambiguous nominal-length groups

The following files need actual local bounds and orientation measurements; their names are not a reliable dimension contract by themselves:

- `SM_RawLog_Long_01.fbx`
- `SM_RawLog_Long_02.fbx`
- `SM_RawLog_Long_03.fbx`
- `SM_RawLog_Long_04.fbx`
- `SM_RawLog_Long_05.fbx`
- `SM_RawLog_Long_06.fbx`
- `SM_RawLog_Long_07.fbx`
- `SM_RawLog_Long_08.fbx`
- `SM_RawLog_Short_01.fbx`
- `SM_RawLog_Short_02.fbx`
- `SM_RawLog_Short_03.fbx`
- `SM_RawLog_Short_04.fbx`

### Exact root filenames

- `SM_RawLog_150.fbx`
- `SM_RawLog_150_Gutter.fbx`
- `SM_RawLog_150_Half.fbx`
- `SM_RawLog_150_Hewn1.fbx`
- `SM_RawLog_150_Hewn2.fbx`
- `SM_RawLog_150_Kalas.fbx`
- `SM_RawLog_150_Plank.fbx`
- `SM_RawLog_150_Quarter_04.fbx`
- `SM_RawLog_150_Trough.fbx`
- `SM_RawLog_300.fbx`
- `SM_RawLog_300_Gutter.fbx`
- `SM_RawLog_300_Half.fbx`
- `SM_RawLog_300_Hewn1.fbx`
- `SM_RawLog_300_Hewn2.fbx`
- `SM_RawLog_300_Kalas.fbx`
- `SM_RawLog_300_Plank.fbx`
- `SM_RawLog_300_Quarter_04.fbx`
- `SM_RawLog_300_Trough.fbx`
- `SM_RawLog_600.fbx`
- `SM_RawLog_600_Gutter.fbx`
- `SM_RawLog_600_Half.fbx`
- `SM_RawLog_600_Hewn1.fbx`
- `SM_RawLog_600_Hewn2.fbx`
- `SM_RawLog_600_Kalas.fbx`
- `SM_RawLog_600_Plank.fbx`
- `SM_RawLog_600_Quarter_04.fbx`
- `SM_RawLog_600_Trough.fbx`
- `SM_RawLog_75_FlatNotch_TopBottom.fbx`
- `SM_RawLog_75_Gutter.fbx`
- `SM_RawLog_75_Half.fbx`
- `SM_RawLog_75_Hewn1.fbx`
- `SM_RawLog_75_Hewn2.fbx`
- `SM_RawLog_75_Kalas.fbx`
- `SM_RawLog_75_Plank.fbx`
- `SM_RawLog_75_Quarter_04.fbx`
- `SM_RawLog_75_Trough.fbx`
- `SM_RawLog_Long_01.fbx`
- `SM_RawLog_Long_02.fbx`
- `SM_RawLog_Long_03.fbx`
- `SM_RawLog_Long_04.fbx`
- `SM_RawLog_Long_05.fbx`
- `SM_RawLog_Long_06.fbx`
- `SM_RawLog_Long_07.fbx`
- `SM_RawLog_Long_08.fbx`
- `SM_RawLog_Short_01.fbx`
- `SM_RawLog_Short_02.fbx`
- `SM_RawLog_Short_03.fbx`
- `SM_RawLog_Short_04.fbx`

### Exact Archive/joinery filenames

- `SM_RawLog_75_CrossLap_Lower.fbx`
- `SM_RawLog_75_CrossLap_Upper.fbx`
- `SM_RawLog_75_Notch_Fork.fbx`
- `SM_RawLog_75_Notch_HalfLap.fbx`
- `SM_RawLog_75_Notch_Saddle.fbx`
- `SM_RawLog_75_Notch_V.fbx`
- `SM_RawLog_75_SaddleA_Cradle.fbx`
- `SM_RawLog_75_SaddleA_Mate.fbx`
- `SM_RawLog_75_SaddleB_Lower.fbx`
- `SM_RawLog_75_SaddleB_Upper.fbx`

## Filename-derived interpretation (not geometry-verified)

| Name suffix | Current interpretation / use | Verification |
|---|---|---|
| Base name (e.g. `SM_RawLog_150.fbx`) | Full-round nominal-size source mesh | Must measure local length, cross-section radius, pivot and axis |
| `_Half` | Split half-round, not half-length; this is explicitly noted in `AssetPackages/README.md` | Need verify flat-face plane, pivot origin, collision and complementary orientation |
| `_Quarter_04` | Likely quarter-round cross-section output; package README groups Quarter with prepared shapes | Not confirmed by mesh geometry; inspect whether “04” encodes a mesh variant or output count |
| `_Gutter` | Grooved/gutter shape | Verify cut plane/depth and pivot |
| `_Trough` | Hollow trough shape | Verify hollow geometry, collision and open-side orientation |
| `_Hewn1`, `_Hewn2` | Two hewn variants | Determine which is flattened/alternative material or a different geometry state |
| `_Kalas` | Squared beam variant | Verify dimensions, local axes and corner orientation |
| `_Plank` | Plank output shape | Verify output width/thickness and whether source length is preserved |
| `_FlatNotch_TopBottom` | Flat-notch joint/cut variant | Verify compatible hosts and notch dimensions |
| `Long_01..08`, `Short_01..04` | Unclassified raw-log variants | Measure and map to nominal length; do not guess from numbering |
| `Archive/*` | CrossLap / Notch / Saddle / HalfLap / Fork / V-notch mating pieces | Verify pair IDs, mating dimensions and source-piece prerequisites |

## Important content-pipeline gap

`AssetPackages/UnrealExport/` is an asset inbox, not the Unreal `Content/` folder. The reviewed root has no tracked production `Content/` directory containing configured `ALogBuildPiece` Blueprints. `UnrealEditorScripts/ImportLogsToScene.py` imports FBXs into `/Game/ImportedLogs` and builds a staging gallery; it does not configure each Blueprint's `SplitHalfVariantMesh`, `QuarterVariantMesh`, gutter/trough/hewn/kalas/plank variants, center-to-center separation offsets or child-output rotations. The height-map ZIP is also not automatically assigned to end-grain materials.

The static C++ settings now exist, but no actual mesh assignment is proven by that code.

## Required Unreal measurement worksheet

For every imported mesh, record:

1. Exact source filename and imported asset path.
2. Local bounds (X/Y/Z), effective long axis and nominal source length.
3. Pivot position relative to the full-round source.
4. Cross-section shape: full / half / quarter / hewn / square / groove / trough / plank / notch / joint.
5. Whether output preserves the source's nominal length; if it changes, length fraction and output count.
6. Collision representation and visible geometry agreement.
7. Complementary piece transform: center-to-center offset plus child local rotation.
8. Compatible operation, required input state, blocked state and stable output definition ID.
9. Successful import/configuration evidence and the accepted Blueprint/mesh reference.

## Concrete unresolved findings

- `SM_RawLog_75.fbx` is absent while eight shape variants plus `FlatNotch_TopBottom` exist for the 75 group. Decide whether to add a base mesh or explicitly block workflows requiring one.
- The 150/300/600 groups each have a base mesh and eight shape variants, but their actual dimensions/pivots have not been checked.
- Long/Short groups need measurements and normalized length labels.
- `Hewn1` vs `Hewn2` is not defined semantically in data.
- No source-level contract currently creates arbitrary offcut counts for every length cut. `SplitHalf` and `Quarter` create two outputs; Hewn, Gutter, Trough, Kalas and Plank still use single-actor transformation semantics.
- The new orthogonal metadata is not a replacement for actual actor-transform/structural-graph persistence. SaveGame v7 stores split receipts, but the repository's broader world-save gap remains open.

## Next acceptance gate

After entering actual UE-derived asset measurements, lock a data row for every used combination of nominal source mesh + length fraction + cross-section shape. Only then assign Blueprint variants and accept the physical split behavior. Filename classification alone must not be counted as geometry-verified.
