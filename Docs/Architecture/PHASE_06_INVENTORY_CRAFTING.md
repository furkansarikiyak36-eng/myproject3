# Phase 6 — Inventory and Crafting

Status: initial inventory/crafting source layer; source review and engine integration remain open.  
Branch: phase-06-inventory-crafting  
Base: phase-05-stick-primitives

## Scope
M049–M058: world pickup, stacks/carry limits, categories/quick access, crafting inputs, recipe resolution, missing ingredients, atomic confirmation, tool preservation, cancellation/result collection, and repair/protection/trap-component recipes.

## Delivered
- Stack, category, ingredient and recipe data types.
- Slot and optional per-item weight limits.
- Stack-aware add/remove operations and quick access validation.
- Recipe registration and missing consumed ingredient reporting.
- Non-consumed ingredients (such as tools) are preserved.
- Crafting preflights output/byproduct capacity and snapshots inventory for rollback if a mutation unexpectedly fails.

## Remaining
World-item pickup/drop actor integration, UI/category display, alternate-recipe presentation, crafting-area placement, cancellation/session ownership, durable persistence, and domain-specific repair/protection/trap recipes remain integration work. This component is not yet connected to a world item actor or save/load subsystem.

No Unreal Engine 5.8 UHT/build, Editor, PIE or gameplay tests were run. Registry remains unreviewed/not_run.
