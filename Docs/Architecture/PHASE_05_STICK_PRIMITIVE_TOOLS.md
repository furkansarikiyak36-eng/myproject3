# Phase 5 — Stick & Primitive Tools

**Status:** Initial source foundation. Full M039–M048 integration and Unreal validation remain open.  
**Branch:** `phase-05-stick-primitive-tools`  
**Base:** `phase-04-stone-masonry-sequential`  
**Target:** Unreal Engine 5.8

## Scope from the canonical registry

This phase owns M039–M048:
- M039 gather and break sticks
- M040 sharpen a stick / make a double-ended stake
- M041 primitive tools and melee weapons
- M042 spear, bow and arrows
- M043 torches and simple light sources
- M044 stakes, fences and simple barriers
- M045 primitive traps and trigger lines
- M046 stick storage, racks and furniture
- M047 garden and animal-support structures
- M048 dismantle stick structures and recover materials

This source increment is a foundation, not completion of all ten mechanics.

## Implemented in this increment

- `EStickPieceType` and `EStickProcessingAction` establish typed stick state and explicit processing actions.
- `FStickProcessingMeshSet` maps an exact source mesh to short-stick, sharpened-stake and double-ended-stake outputs.
- `AStickBuildingPiece` owns physical mesh, held/placed state and result-bearing processing / grounded-stake placement operations.
- Processing rejects held/placed/non-raw sticks, unknown actions, duplicate source-mesh mappings and missing output meshes before mutation.
- Successful mesh replacement attempts to preserve the world-axis bounds bottom; type changes only after the geometry transition succeeds.
- Stake placement accepts only stake-compatible types and requires the caller to confirm that a real ground-contact query passed. This API does not perform a trace itself.

## Not implemented yet

- World gathering interaction, harvesting quantities, break animation/progress, tool durability and resource accounting.
- Recipe/ingredient validation, primitive tools, melee combat, spear/bow/arrow assembly.
- Torch fuel, ignition, light, smoke, extinguishing and save/load.
- Fence/barrier layout, grounded spike gameplay, trap components/trigger lines/arming/disarming.
- Storage capacity, rack/furniture builders, garden/animal-support placement.
- Dismantling/recovery yields, inventory integration, persistence and committed events.
- Full M039–M048 caller/Blueprint audit and Unreal Engine 5.8 UHT/build/Editor/PIE validation.

## Acceptance boundary

Source checks can establish API shape and defensive guards only. They cannot prove Unreal Header Tool compatibility, physics behavior, authored mesh dimensions/pivots/collision, correct ground traces, or runtime gameplay. Keep registry status unchanged until the required evidence exists.
