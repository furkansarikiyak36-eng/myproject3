# WOOD-001 — Quarter Cross-Section Naming and Legacy Suffix Compatibility

**Branch:** `fix/log-quarter-cross-section-semantics`  
**Base:** `audit/reference-game-and-landscape-status`  
**Pull request:** [#22 — Distinguish quarter-round cross section from quarter-length log](https://github.com/furkansarikiyak36-eng/myproject3/pull/22)  
**Scope:** Make the output type of the cross-section quartering action explicit without renumbering existing serialized enum values.  
**Evidence level:** Static source inspection only; Unreal 5.8 UHT/UBT, Editor, PIE, and asset/runtime tests were not run.

## Source finding

The legacy `ELogPieceType::QuarterLog` enum was displayed as “1/4 Log”, but `ELogCutAction::Quarter` also used it for a quarter-round cross-section. The same ambiguous `.QuarterLog` definition suffix was written to the item ownership ledger and read during variant restoration.

This makes length-oriented naming and cross-section shape naming easy to conflate.

## Change made

- Appended `ELogPieceType::QuarterSectionLog` to the end of `ELogPieceType`, after the existing values, to preserve old enum numeric values.
- `ELogCutAction::Quarter` now produces `QuarterSectionLog` and the canonical `.QuarterSectionLog` item-definition suffix.
- `SetPieceTypeByName` recognizes explicit `quartersection` / `quarterround` names before the generic quarter/length label.
- `ApplyDefinitionVariant` recognizes the new suffix and continues to interpret the old `.QuarterLog` suffix as a compatibility alias for the older quartering operation.
- Same-operation retry handling accepts the prior receipt when a legacy quarter suffix is loaded into the explicit quarter-section actor type.

## Static acceptance

- [x] Existing enum entries remain in the same order; the new enum was appended.
- [x] Quarter action, retry suffix and committed definition use `QuarterSectionLog`.
- [x] New and legacy suffixes resolve to the same configured quarter mesh, while new operations write only the canonical suffix.
- [x] Static source scan and brace-balance check passed.
- [ ] UE 5.8 Header Tool / Build Tool compile.
- [ ] Check configured Blueprint/data assets for correct quarter mesh and expected variant mapping.
- [ ] Runtime positive/negative test for whole → split-half → quarter-section, including repeat operation ID.
- [ ] Implement atomic creation/registration of all physical output pieces for a real split; this patch changes the semantic label only.
- [ ] Verify output mesh geometry, collision, support/placement and save/load.

## Remaining 2.1 work

This does not close Sons-of-the-Forest-style physical processing. The current operation still changes one actor's mesh/type and updates its item definition. A true split must create all resulting pieces with stable identities, ownership records, collision and geometry as one recoverable transaction; otherwise a failed child spawn or retry can leave the source consumed or duplicate pieces. That is the next work item (WOOD-002).
