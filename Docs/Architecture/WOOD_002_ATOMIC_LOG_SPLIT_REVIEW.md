# WOOD-002 — Atomic Physical Log Split Outputs

**Branch:** `fix/atomic-log-piece-splitting`  
**Base:** `fix/log-quarter-cross-section-semantics`  
**Review status:** Source-layer implementation; engine validation pending.  
**Scope:** Convert split-half and quarter-section processing from a single-actor mesh swap into two separately owned physical outputs.

## Source finding

The prior processing path only changed the existing actor's mesh/type and updated a single ownership record. That was inadequate for a real split: the second physical half/quarter had no stable item record and could not safely participate in inventory, storage, construction or persistence.

## Changes

- Added `FWorldItemSplitReceipt` and `TrySplitItem` to the ownership subsystem. The original stable item ID is retained for the first output; additional output records receive unique GUIDs.
- The split request validates operation ID, source ID/revision/owner, output definitions, distinct finite output locations and item-ledger capacity before changing authoritative records.
- Reusing the same operation ID with the same complete split request returns the recorded output records. Reusing it with different source/output/location inputs is rejected.
- Split receipts participate in ownership snapshots, coordinator rollback and SaveGame schema v7. The v4→v5→v6→v7 migration path initializes receipt arrays that did not exist in older schemas.
- `ALogBuildPiece::TrySplitOutputPieces` prepares the second actor with deferred spawning, configures its output mesh and unique identity, commits the ownership split, updates the source actor and finishes child spawning.
- The actor path verifies that the child has the expected world-owned item record and structural graph node. Failed registration triggers an ownership restore attempt and source actor state restoration.
- Source and child center positions are derived from a mesh-local center-separation vector. Zero, longitudinal-only or degenerate offsets are rejected before mutation.
- Added separate mesh-local child rotation offsets for split-half and quarter-section outputs. These are copied to child actors and can be authored per mesh/pivot rather than assuming all assets share the same cut plane.

## Static checks completed

- [x] Split request API and receipt record are declared.
- [x] Output IDs and source ID/revision rules are wired into ownership mutation.
- [x] Split receipts are included in SaveGame v7, coordinator rollback and complete ownership snapshots.
- [x] Deferred child spawn, identity assignment, item-owner verification and graph-node verification are present.
- [x] Rollback path restores ownership snapshots and source actor transform/mesh/type/definition/snap points.
- [x] Child output rotations are configurable independently for split-half and quarter-section.
- [x] Structural scan found balanced braces in the modified log and ownership implementation files.
- [ ] Unreal 5.8 Header Tool / Build Tool compile.
- [ ] Configure per-Blueprint mesh, center offset and rotation values.
- [ ] Runtime test confirms two visible physical actors with unique item IDs for each split action.
- [ ] Retry test proves the same operation ID does not create a third actor and returns the same output IDs.
- [ ] Failure-injection test proves actor/graph registration failure restores the item ledger and source actor.
- [ ] Save/load restores the two actual actors and their transforms/graph nodes as well as receipt data.

## Important remaining limits

The repository's save coordinator still does not fully restore all physical actor transforms and structural graph links. Persisting split receipts does not by itself restore a split scene after reload. A configured center offset and rotation are also necessary for each actual mesh; the C++ defaults are fallback assumptions, not evidence that a content asset is oriented correctly.

Other processing actions (hewn, groove, trough, squared beam, plank) still follow the single-actor transform path. This implementation only handles physical two-output behavior for `SplitHalf` and `Quarter`; it does not yet model arbitrary offcuts, source quantity yield or every wood-processing state.

## Next work

WOOD-003: inventory the actual mesh assets and classify each variant separately by length fraction and cross-section geometry. Use that asset evidence to finish the state → action → outputs → geometry/collision table before adding additional output counts.
