# Blind coding continuation — P5.1 geology and P2.2 action costs

Date: 2026-10-29
Branch: `game/p00-adopt-group-a`
Commit range: `e48c3c4` → `ffcb364` → `609f445` → `28755f3`
Status: `code-written / unbuilt-unrun`

## P5.1 geology changes
- Extended terrain yield rules with allowed geology regions and integer weights.
- Parsed region lists/weights from `Config/GameData/layers.json` and copied them into the terrain authority rules.
- Added a stable, seed-based resolver for `(logical cell, depth, region, world seed)`; no global RNG or frame order.
- Resolved material is written to the logical terrain cell on first excavation; later excavation cannot silently replace an established material.
- Added `TryExcavateForGeology` and routed the N-key dig component through it; the old editable material input is deprecated/ignored.
- Added `MyProject3.World.Dig.GeologyResolvesByRegionDepthAndStableSeed`.

Limit: the active geology `RegionId` is configurable on the digging adapter, not auto-derived from world region volumes. Chunk seams/mesh/collision remain test-stage work.

## P2.2 changes
- `DismantlePiece` now applies shared Build preflight and cost by default, including direct C++ callers.
- Gate/window batch operations pass `false` only after their own aggregate preflight and charge, avoiding duplicate costs per piece.
- `ALogContainerActor` now charges ongoing Carry stamina/energy using elapsed shared simulation time. It processes bounded time slices and releases towing when the puller is resting/exhausted.
- Added `MyProject3.World.Needs.SledTowChargesSharedClockActivity` (authored, not run).

## Validation
- No Unreal 5.8 build, UHT, automation execution, PIE, or save/load roundtrip was performed.
- The previously verified 102-test result predates these changes and must not be attributed to them.
- P10 remains frozen.
