# Phase 1 — Data Contract Schemas

These JSON Schema Draft 2020-12 files define design-time catalog contracts for MyProject3. They are **not** yet a runtime loader, DataTable, DataAsset, or claim that the referenced gameplay code has been merged into `main`.

## Authority and naming

- `MECHANIC_REGISTRY_119.json` owns M001-M119 scope identity.
- Definition IDs (item/material/recipe/build-part/region/event IDs) are separate from mechanic IDs.
- An instance `FGuid` is separate from its definition `FName`. Existing code uses both `FInventoryStack.ItemId` (definition key) and `FInventoryStack.OwnedWorldItemId` (unique physical world item).
- Human-readable/localized display text is not an identifier.
- Placement profile numbers remain in `Config/Placement/PlacementProfiles.json` on the candidate source branch; build-part metadata must reference that profile/provider rather than create a second snap calculation system.
- Operation/result, identity/lifecycle, item ownership, event notification, operation ledger, and save snapshots reuse existing candidate C++ contracts. Do not add another owner merely because there is a schema file.

## Files

- `item-definition.schema.json`: item family, inventory category, world representation, stack and persistence policy.
- `material-definition.schema.json`: resource/terrain material properties.
- `recipe-definition.schema.json`: transactional input/process/output recipes.
- `process-transition.schema.json`: state-gated physical transformations such as cut/split/groove/fracture.
- `build-part-definition.schema.json`: scale, pivot, collision, semantic anchors and placement-provider references.
- `region-definition.schema.json`: data-driven region starting conditions and recovery milestones.
- `feedback-event-definition.schema.json`: logical SFX/VFX/animation/UI/music/ambience cue catalogue.

## Runtime choice

Do not require the game to read these JSON files at runtime yet. First validate the contracts, then choose whether each data family becomes a Primary Data Asset, Data Table, authored configuration or runtime JSON. That implementation decision must preserve stable IDs and support validation in editor/build tools.

## Phase 1 acceptance

1. Validate each schema with a Draft 2020-12 validator.
2. Validate sample definitions against schemas and check unique IDs/references across all catalogue files.
3. Check definitions against existing C++ types without introducing duplicate authorities.
4. Document migrations before changing any persisted field/type.
5. Keep Unreal build, Editor/PIE and save/load verification as separate gates.
