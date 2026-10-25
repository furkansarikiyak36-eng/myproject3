# WOOD-004 — Data-Driven Mesh Families and Persistent Geometry Identity

**Branch:** `fix/log-mesh-variant-sets`  
**Base:** `fix/log-geometry-dimensions`  
**Scope:** Select exact output meshes by nominal family + length fraction + cross-section shape, and preserve the selection across split, inventory drop and SaveGame.  
**Evidence level:** Source inspection, repository asset inventory and targeted structural checks. Unreal 5.8 UHT/UBT, Editor, PIE, imported mesh inspection and runtime tests were not run.

## Source findings

Earlier `ALogBuildPiece` exposed one output pointer per shape (for example, one `SplitHalfVariantMesh` and one `QuarterVariantMesh`). A single Blueprint therefore could not select between multiple nominal mesh families without being a different authored Blueprint for every size. The geometry metadata separated length fraction from cross-section shape, but no exact tuple table connected those two attributes to the intended output asset.

The selection also needed to survive item ownership transitions. An actor-only selector would be lost when the item entered inventory/storage and was materialized again.

## Changes made

- Added `FLogPieceMeshVariant` with an exact key: `ELogLengthFraction × ELogCrossSectionShape → UStaticMesh`.
- Added `FLogProcessingMeshSet`: stable `VariantSetId`, descriptive `NominalLengthLabel`, `BaseRoundMesh`, exact variant rows and the split-specific center offsets/child rotation settings.
- Added `ProcessingMeshSets` to `ALogBuildPiece` and an editable/saveable `SelectedProcessingMeshSetId`.
- A source actor auto-selects a set only when its current mesh uniquely matches one set's base or variant mesh; an authored explicit selector is also supported.
- Duplicate set IDs, ambiguous mesh matches, duplicate geometry tuples, null variant rows and a set without a base-round mesh are rejected. If a mesh table is configured, missing geometry tuples fail closed instead of falling back to the legacy single-pointer properties.
- Cut eligibility, output selection, definition-variant restoration, center offsets and complementary child rotations all use the same selected set.
- The child split actor inherits the set/table and selector.
- Added `MeshVariantSetId` to `FWorldOwnedItemRecord`; new world items record their selected family, split outputs inherit it, and inventory drop applies it before/after construction.
- Geometry variant DefinitionIds can encode a non-full length fraction as part of the suffix (for example `Wood.LogPiece.LengthHalf.SplitHalfLog`). `ApplyDefinitionVariant` restores the length fraction before querying the exact mesh tuple. Full-length common output suffixes remain backward-compatible.
- Bumped SaveGame schema from v7 to v8. The v7→v8 migration initializes the optional selector field as absent for old saves; it deliberately does not guess a nominal family from a filename.

## Static acceptance

- [x] Authored mesh family table and exact geometry-tuple records declared.
- [x] Resolver used by cut eligibility, output action, variant restoration and split transform configuration.
- [x] Missing/ambiguous tables and duplicate state mappings fail closed.
- [x] Selected family identity follows the ownership record through split outputs and inventory drop.
- [x] Length fraction + cross-section state can be restored from a geometry definition suffix.
- [x] SaveGame schema v8 and v7→v8 compatibility path added.
- [x] Source structural checks found balanced braces in modified C++ files and confirmed declared helper/call-site wiring.
- [ ] UE 5.8 UHT / Build Tool compile.
- [ ] Fill in actual `ProcessingMeshSets` entries for each imported nominal family.
- [ ] Measure the FBX bounds, pivots, local axis, output collision and complementary-piece transforms in UE.
- [ ] Runtime tests: same-set split, shortened-log split, repeated operation ID, inventory drop/re-pickup, storage/cargo transfer, save/load and invalid-configuration rejection.
- [ ] Final integrated 119-mechanic acceptance run.

## Configuration rules

For each real nominal mesh family, author one set with a unique `VariantSetId`, a base-round mesh and exactly one row for every supported length-fraction/cross-section tuple. Use the imported mesh references and verified dimensions; do not derive actual centimeters from names like `75`, `150`, `300`, `600`, `Long` or `Short`.

The code cannot fill the table automatically because the repository tracks the FBX inbox, not a fully configured Unreal Content/Blueprint library. The 75 group still lacks `SM_RawLog_75.fbx`, and the Long/Short variants remain unmeasured.

## Remaining risks

- General physical actor transforms and structural graph links are not yet fully restored by the world-save coordinator. Persisting item geometry identity is necessary, but is not itself a complete physical-world round trip.
- A legacy SaveGame that predates `MeshVariantSetId` may have no explicit family key. If its actor cannot be uniquely identified from the configured mesh table, the actor is intentionally quarantined rather than choosing a random size.
- Unreal serialization/UHT behavior and the actual imported geometry remain unverified.
