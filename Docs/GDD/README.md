# MyProject3 — GDD and Production Catalogue

This folder defines the intended game, production catalogue, and phased implementation plan. It is a design authority, not a report that every listed feature already exists in code.

## Documents

- [Master GDD](GDD_MASTER.md) — premise, pillars, gameplay loop, scope boundaries, and target vertical slice.
- [Inventory catalogue](INVENTORY_CATALOG.md) — items, materials, processing, carrying rules, and required item data.
- [World, VFX, animation, and audio catalogue](ASSET_VFX_AUDIO_CATALOG.md) — logical asset families and event catalogues. It does not recommend or bundle marketplace models, textures, or sound packs.
- [Phase 00 baseline audit](../Architecture/PHASE_00_BASELINE_AUDIT.md) — main-branch evidence, registry discrepancies, and what is not yet verified.
- [Branch integration matrix](../Architecture/PHASE_00_BRANCH_INTEGRATION_MATRIX.md) — recommended implementation branch and safe integration order.
- [M001–M119 baseline matrix](../Architecture/PHASE_00_M001_M119_BASELINE.csv) — per-mechanic baseline evidence, registry fields, and provisional phase mapping.
- [Shared data contracts](../Architecture/PHASE_01_SHARED_DATA_CONTRACTS.md) — identity, ownership, operations, units, events, persistence and domain state.
- [Shared contract source reconciliation](../Architecture/PHASE_01_SHARED_DATA_CONTRACTS_RECONCILIATION.md) — candidate API inspection, ownership/operation gaps, and terrain-state integration risks.
- [Phase 01 acceptance checklist](../Architecture/PHASE_01_ACCEPTANCE_CHECKLIST.md) — schema CI evidence and remaining source/engine gates.
- [Gameplay definition schema](../Architecture/GAMEPLAY_DEFINITION_CATALOG_SCHEMA.json) — draft schema for item/material/tool/build-part/recipe/crop/region definitions.
- [Feedback event schema](../Architecture/FEEDBACK_EVENT_CATALOG_SCHEMA.json) — separates committed gameplay events from SFX/VFX/animation/music cues.
- [Wood processing transitions](../Architecture/WOOD_PROCESSING_TRANSITIONS.json) — explicit length and cross-section dimensions and transition prerequisites.
- [Canonical 119-mechanic registry](../Architecture/MECHANIC_REGISTRY_119.md) — stable M001–M119 scope; inclusion does not mean a mechanic is complete.

- [Phased implementation plan](../Production/PHASED_IMPLEMENTATION_PLAN.md) — build order, dependencies, and gates.
- [System dependencies and acceptance](../Production/SYSTEM_DEPENDENCIES_AND_ACCEPTANCE.md) — integration constraints and definition of done.
- [Task template](../Production/TASK_TEMPLATE.md) — consistent format for implementation work.

## Authority and status rules

1. The checked-in source code and editor content are the evidence of current implementation.
2. This GDD describes intended design, not current implementation status.
3. Each catalogue entry is a required or candidate logical asset/system, not a claim that a mesh, sound, animation, or effect has already been authored.
4. Do not label a phase DONE because files or classes exist. Completion requires its stated acceptance checks.
5. Every item, mechanic, asset family, sound cue, and recipe must have a stable ID before implementation begins.

## Status vocabulary

- PROPOSED: valid candidate, not scope-locked.
- PLANNED: included in the agreed target.
- IN_PROGRESS: implementation branch has active work.
- BLOCKED: dependency or decision is unresolved.
- IMPLEMENTED_UNVERIFIED: code/content exists but required in-engine checks have not passed.
- VERIFIED: acceptance evidence exists for the target engine/editor state.
- DEFERRED: deliberately outside the current target.

## Scope exclusions

No Fab/vendor recommendations, imported texture-library list, external model packs, or third-party asset licensing claims are included here. Start with low-cost placeholder geometry and original/simple sound placeholders; replace them only after the mechanics, naming, pivots, collision, and dimensions are stable.
