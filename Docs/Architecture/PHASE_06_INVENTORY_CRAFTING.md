# Phase 6 — Inventory and Crafting

**Status:** Initial inventory/crafting foundation; M049–M058 are not declared complete.  
**Branch:** `phase-06-inventory-crafting-sequential`  
**Base:** `phase-05-stick-primitive-tools`  
**Engine target:** Unreal Engine 5.8

## Canonical scope (M049–M058)

- M049 world item pickup into inventory
- M050 stacks and carry limits
- M051 category/properties/quick access
- M052 crafting inputs / crafting area
- M053 recipe resolution and alternate recipes
- M054 missing ingredient quantities
- M055 atomic crafting confirmation
- M056 preserve tools and return unused materials
- M057 cancel crafting and collect result
- M058 repair/protection/trap-component recipes

## Implemented source foundation

- `FInventoryStack`, `FRecipeIngredient`, and `FCraftingRecipe`.
- Stack-aware add/remove operations, slot capacity, carry weight, category and quick-access selection.
- Result-bearing add/remove/craft APIs using the shared `FWorldOperationResult`; legacy bool wrappers remain for compatibility.
- Recipe registration rejects malformed IDs, quantities, outputs, byproducts and duplicate recipe IDs.
- Craft preflight checks all ingredients, including non-consumed tools; only consumed ingredients are removed.
- Capacity preflight simulates the inventory after consumed ingredients are removed, then validates all outputs and byproducts. This avoids rejecting valid crafts merely because ingredients currently occupy slots/weight.
- Crafting snapshots items and quick access, then rolls back if an unexpected mutation fails.
- Item weights must be explicitly configured as positive grams for all items entering or remaining in inventory. Missing weights fail capacity checks closed.

## Not implemented / integration required

- World pickup/drop actor and carry component integration.
- UI display for categories, properties, missing quantities and quick access.
- Crafting-area placement and alternate recipe selection/presentation.
- Craft session ownership, cancel/collect flow and unused-material return policy.
- Concrete repair/protection/trap recipes, tool wear, durability and crafting animation/progress.
- Stable inventory persistence, save migration and committed domain events.
- Caller/Blueprint audit, item-definition registry, runtime UI and multiplayer/authority policy.

## Acceptance and validation boundary

Source checks do not prove UHT compatibility, successful C++ compilation, runtime behavior or data defaults. No Unreal Engine 5.8 UHT/build, Editor, PIE or gameplay tests were run. Keep registry status unchanged until evidence exists.
