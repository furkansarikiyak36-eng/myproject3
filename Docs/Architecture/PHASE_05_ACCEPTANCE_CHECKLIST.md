# Phase 5 — Acceptance Checklist

## Source-level foundation
- [x] Stick-specific type and processing action enums exist.
- [x] Processing mesh outputs are mapped from an exact source mesh.
- [x] Duplicate source mapping and missing output fail closed.
- [x] Processing rejects held, placed and non-raw pieces before mutation.
- [x] Unsupported enum values are rejected.
- [x] Mesh and piece type transition together; type is not changed before alignment succeeds.
- [x] Grounded stake placement rejects non-stake types and requires caller-confirmed ground contact.
- [x] Failure results use the shared `FWorldOperationResult` contract.

## Open / requires implementation or evidence
- [ ] World gather interaction and resource yield.
- [ ] Tool requirements, break progress, durability and animation.
- [ ] Primitive tool / melee / spear / bow / arrow recipes and equipment behavior.
- [ ] Torch fuel, ignition, light and extinguishing lifecycle.
- [ ] Fence/barrier construction and trigger-line trap state machine.
- [ ] Storage/rack/furniture and garden/animal-support structures.
- [ ] Dismantle/recovery integration with inventory and persistence.
- [ ] Audit all C++ callers, Blueprints and authored defaults.
- [ ] Populate and validate source/output meshes, pivots, bounds and collision.
- [ ] Unreal Engine 5.8 UHT/build, Editor and PIE/runtime validation.
- [ ] Verify M039–M048 against integrated acceptance evidence; do not infer completion from this checklist.
