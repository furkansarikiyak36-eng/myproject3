# WOOD-003 — Canonical Log Length, Cross-Section and Mesh Variant Mapping

**Branch:** `feat/log-canonical-dimensions`  
**Base:** `fix/atomic-log-piece-splitting`  
**Pull request:** [#25 — Separate log length fraction and cross-section geometry](https://github.com/furkansarikiyak36-eng/myproject3/pull/25)  
**Scope:** Separate longitudinal length fraction from cross-section profile, select geometry by an exact authored variant key, and persist those dimensions in the authoritative item definition.  
**Evidence level:** Static source inspection and structural checks only. UE 5.8 UHT/UBT, Editor, PIE, mesh/pivot/collision validation and runtime save/load tests were not run.

## Source finding

The legacy `ELogPieceType` combines different concepts. For example, `QuarterLog` could be read as a quarter-length log, while `ELogCutAction::Quarter` was producing a quarter-round cross-section. The single legacy mesh slots also did not identify which log length they fit, so a shorter log could be given a full-length output mesh.

## Change made

### Independent dimensions

- Added `ELogLengthFraction`: full, three-quarter, half, quarter and other/authored length.
- Added `ELogCrossSectionProfile`: round, half-round, quarter-round, hewn, square beam, grooved half-round, trough, plank, notched, joint and other.
- Appended the earlier `QuarterSectionLog` enum member rather than renumbering the existing legacy enum values.
- Added explicit canonical fields and Blueprint getter APIs to `ALogBuildPiece`. The existing `PieceType` remains as a compatibility projection. Existing actors map their old type to the two dimensions unless the canonical flag is explicitly enabled.

### Authored geometry variant table

`FLogGeometryVariantMesh` records:
- one exact `LengthFraction` + `CrossSectionProfile` key,
- the output `UStaticMesh`,
- second-piece center-to-center local offset,
- complementary child rotation.

`TryResolveGeometryVariant` rejects duplicate exact keys. Non-full-length processed shapes require an exact table row; legacy mesh slots are accepted only for full-length compatibility. It does not silently use a full-length shape mesh for a half- or quarter-length piece.

### Definition and restore

- New processed item definitions use `Base.Geom.<Length>.<Profile>` (for example, `Wood.LogPiece.Geom.Half.HalfRound`).
- `ApplyDefinitionVariant` parses both dimensions, resolves the corresponding mesh, and restores the actor's dimensions and compatibility type.
- Legacy single-suffix definitions (such as `.SplitHalfLog`, `.HewnLog`, and the historical `.QuarterLog`) remain readable where an appropriate mesh can be resolved.
- Same-operation retry checks now use the canonical dimensions and still recognize the old suffix path.
- The split actor rollback restores length/profile metadata as well as the legacy type and transform.

## Static acceptance

- [x] Length and cross-section are represented independently.
- [x] The geometry asset table is keyed by the exact dimension pair.
- [x] Canonical item definitions write and parse both dimensions.
- [x] Exact geometry rows take priority; duplicate matches and unsupported shorter-piece fallbacks fail closed.
- [x] Split/quarter actions resolve meshes and split offsets through the dimension-aware resolver.
- [x] Failed split rollback restores canonical dimension fields.
- [x] Static call-site and brace-balance checks passed.
- [ ] UE 5.8 UHT/UBT compile.
- [ ] Fill actual Blueprint/content `GeometryVariantMeshes` rows for every supported length/profile pair.
- [ ] Validate mesh pivots, cut-face normals, child rotations, center offsets and collision in UE.
- [ ] Runtime round-trip test for canonical definitions via inventory drop, save/load and actor activation.
- [ ] Verify full-, half- and quarter-length source assets against each assigned processed variant.

## Important remaining limits

- The table is implemented in C++, but the actual project Blueprint/content rows were not authored or validated by this source-only task. If a required pair is absent, the operation is rejected rather than guessed.
- Persisting dimensions in the item definition helps reconstruct a dropped item, but the wider world-save gap remains: all physical actor transforms and structural graph connections are not yet fully persisted/recreated by the current world save coordinator.
- Partial sawing progress is not modeled yet.
- True longitudinal cuts and varied output lengths/byproducts (for example, 3/4 + 1/4 length) are not implemented by this change. `SplitHalf` and `Quarter` remain cross-section operations. Those need a separate atomic multi-output placement contract (WOOD-004).
- Historic `.QuarterLog` entries do not encode enough information to infer every old asset's intent; the suffix is only a compatibility alias, not a lossless migration.

## Next item

WOOD-004 — implement longitudinal length-cut actions, output positions for differently sized pieces, offcut/byproduct quantities and idempotent multi-output ownership in one transaction.
