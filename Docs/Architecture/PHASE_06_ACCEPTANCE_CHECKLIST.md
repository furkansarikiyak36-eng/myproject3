# Phase 6 — Acceptance Checklist

## Source-level foundation
- [x] Inventory stack, ingredient and recipe types exist.
- [x] Slot and carry-weight capacity are checked before adding items.
- [x] Missing or non-positive item weights fail closed.
- [x] Stack identity checks reject inconsistent category/tool/max-stack definitions.
- [x] Result-bearing add/remove/craft APIs return structured failure codes.
- [x] Legacy bool APIs remain available.
- [x] Recipe registration validates IDs, quantities, output and byproducts.
- [x] Crafting checks non-consumed tools as required ingredients.
- [x] Craft capacity is evaluated after simulated consumption, before mutation.
- [x] Crafting snapshots inventory and quick access for rollback.

## Open
- [ ] Audit all C++ callers, Blueprints and item-definition defaults.
- [ ] Connect world pickups/drops to inventory operations without duplicate grants.
- [ ] Add UI for categories, properties, quick access and missing ingredient quantities.
- [ ] Implement crafting-area input placement and alternate recipes.
- [ ] Define cancel/collect and unused-material policy.
- [ ] Add repair/protection/trap recipes and tool durability.
- [ ] Add save/load, migration and committed events.
- [ ] Run Unreal Engine 5.8 UHT/build, Editor, PIE and runtime tests.
- [ ] Verify M049–M058 against integrated acceptance evidence; registry remains unreviewed.
