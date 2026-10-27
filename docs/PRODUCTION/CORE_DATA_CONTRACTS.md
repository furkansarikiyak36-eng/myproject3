# Core Data Contracts — Authoring Baseline

These are the design-time data contracts for Phase 01. The adjacent JSON Schemas validate authoring examples; they are not yet wired into Unreal runtime loading. Runtime code/DataAssets must later be mapped to the same fields without creating a second source of truth.

## 1. ID families

Use uppercase stable IDs, ASCII letters/digits/underscores, and never recycle an ID.

| Prefix | Meaning | Example |
|---|---|---|
| `ITEM_` | Item definition | `ITEM_RESPIRATOR_FILTER` |
| `MAT_` | Material/resource definition | `MAT_WOOD_LOG` |
| `TOOL_` | Tool definition | `TOOL_HAND_AXE` |
| `EQUIP_` | Equipment definition | `EQUIP_RESPIRATOR` |
| `SEED_` | Seed definition | `SEED_RYE` |
| `CROP_` | Crop definition | `CROP_RYE` |
| `REC_` | Recipe/process definition | `REC_LOG_SPLIT_HALF` |
| `BLD_` | Build-piece definition | `BLD_LOG_BEAM` |
| `PROFILE_ ` | Placement profile | `PROFILE_WOOD_LOG_600` |
| `ANCHOR_` | Semantic anchor ID/tag | `ANCHOR_LOG_END_A` |
| `EVT_` | Committed gameplay event | `EVT_OBJECT_PROCESSED` |
| `VFX_` | VFX cue | `VFX_STONE_CRACK` |
| `SFX_` | Sound cue | `SFX_LOG_SPLIT` |
| `MUS_` | Music cue/state | `MUS_RAID_WARNING` |
| `REGION_` | World-region definition | `REGION_ASH_FOREST` |
| `DISC_` | Persistent discovery | `DISC_CAPSULE_RECORDER` |

The space shown in the `PROFILE_ ` table cell is explanatory only; the actual prefix should be `PROFILE_` without whitespace.

## 2. Definition versus instance

- Definition: what an object/resource is, e.g. `MAT_WOOD_LOG`.
- Instance: a specific physical log, identified by a GUID and holding its own processing/placement state.
- Stack: a quantity of one definition in a chosen owner/slot. If the stack itself is saved as an entity, it needs a stack-instance ID.
- Asset: a mesh, icon, animation, audio cue, or VFX realization. Asset path is a reference, not gameplay identity.
- Display name: localized presentation key; never used as persistence identity.

A single definition may map to multiple mesh variants. The authoritative piece state selects the permitted variants; the current mesh is a representation, not the state source.

## 3. ItemDefinition

Required:
- `DefinitionId`
- `Category`
- `WorldRepresentation`
- `MassGrams`, `VolumeMillilitres`
- `MaxStack`
- `DurabilityModel` and relevant starting/max value when used
- `AllowedActions`
- `PersistencePolicy`
- `Status`

Conditional rules:
- A physical large object (long log, large boulder, placed build piece) is not an abstract stack.
- A stackable resource must declare a stack maximum and unit.
- Tools/equipment are not automatically consumed when used; durability/charges are a distinct model.
- A filter can use `FilterLife`; food can use freshness only if spoilage creates a meaningful decision.
- Unknown definition IDs reject the operation rather than creating ad-hoc inventory entries.

## 4. MaterialDefinition

Required:
- `DefinitionId`
- `MaterialFamily` (wood, stone, soil, clay, sand/gravel, plant, metal/scrap, water/food or other approved family)
- `Hardness` in a documented unitless tuning scale
- `Diggable` and permitted depth/range policy where terrain applies
- `DensityKgPerM3` only when physics/carry/structure calculations actually use it
- `ValidTools`, `SourceDefinitionIds`, `YieldRules`
- `StateModel` if moisture/contamination/rot/processing has gameplay impact

Do not infer physical realism from an arbitrary number. Establish unit/range meaning and validation before a value is consumed by gameplay code.

## 5. RecipeDefinition

Required:
- recipe ID and schema version;
- input definitions and quantities/units;
- prerequisite piece/source state, tool, station, terrain, time, season or weather if applicable;
- immediate versus progress-based operation;
- duration and interrupt/cancel rules;
- outputs and by-products;
- tool wear, fuel/water cost, noise/light/risk where relevant;
- atomic failure/rollback policy;
- resulting state transition and event IDs;
- save representation for in-progress jobs.

Every active recipe needs a source, a valid precondition, a useful result, and an explained resource/time opportunity cost. Do not add a resource with no use or a recipe with no valid output path.

## 6. BuildPieceDefinition and placement profile

The definition identifies material family, piece type, expected dimensions, permitted mesh variants, collision/pivot expectations, local longitudinal axis, and placement profile. Profiles hold tunable station spacing/anchor compatibility data. Geometry-specific anchor coordinates belong to the authored piece/mesh profile, never a transient array index.

- Units for world dimensions are Unreal centimeters.
- Log long axis remains configurable; current main expects local X by default.
- Anchors use semantic IDs/tags; array index is runtime lookup only.
- Preview and commit consume the same computed placement solution.
- Material providers own compatibility rules; the placement kernel remains material-neutral.
- No class may silently fabricate a missing mesh variant or accept a connection because a display name matches.

## 7. EventDefinition

Each event has stable ID, schema version, event type, operation ID, simulation time/tick (where the logical clock applies), affected stable instance IDs, and a typed payload. Publish after commit only. Failed/previewed requests are not committed-world events.

Transient UI delegates and durable event records are separate concerns. No event broadcaster is treated as save storage.

## 8. Persistence schema

Every persisted collection must include:
- schema version;
- stable instance/definition IDs;
- revisions or expected-version rules for mutable records;
- explicit owner/location/state;
- migration or safe-rejection behavior;
- deterministic restore ordering;
- validation for missing/duplicate cross-references;
- derived-data rebuild requirements (mesh, collision, navigation, preview and audio/VFX state).

Do not save raw pointers, actor names as the only identity, array indices as anchor identity, or localized strings as IDs.

## 9. Required integration decisions before Phase 02 merge

1. Reconcile `InventoryCraftingComponent.Items` and `WorldItemOwnershipSubsystem.Items` so quantity has one authority.
2. Reconcile actor-local snap parent/child links and `WorldStructuralGraphSubsystem` so structural relations have one authority.
3. Align item definition IDs, world instance GUIDs and ownership-record IDs.
4. Decide which operations use the shared `FWorldOperationRequest/Result` API and keep public Blueprint APIs as adapters until verified.
5. Define how placed geometry/graph/anchor reservations/terrain rendering data are captured and restored.
6. Decide whether a durable event journal is needed; the current broadcaster is transient and must not be treated as durable.

