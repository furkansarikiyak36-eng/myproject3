# P01 — Shared Data Contracts and Existing Source Reconciliation

**Status:** Contract baseline drafted; runtime integration and UE verification remain open.  
**Planning branch:** `planning/gdd-inventory-production-roadmap`  
**Candidate source stack inspected:** `feature/simple-intent` (contains the H world-system stack plus later snap work; not merged/engine-certified by this document).  
**Target:** Unreal Engine 5.8.

## 1. Goal

Establish one stable vocabulary for definitions, world instances, operations, ownership, processing states, build anchors, feedback, and persistence. Reuse existing candidate C++ contracts where they already cover the requirement. This phase must not create a second identity registry, inventory owner, structural graph, placement kernel, world clock, event broadcaster, or save coordinator.

The JSON Schema files in `Schemas/` are design-time catalog contracts. They do not mean the game has a JSON runtime loader or that code on `feature/simple-intent` has been merged into `main`.

## 2. Identity namespaces — keep them separate

| Namespace | Purpose | Existing/candidate representation | Rule |
|---|---|---|---|
| Mechanic ID | Scope and acceptance tracking | `M001`–`M119` in the canonical registry | Never use as an item/recipe/asset identifier; never renumber. |
| Definition ID | What a thing or rule is | `FName` keys such as `FWorldOwnedItemRecord.DefinitionId` and `FInventoryStack.ItemId` | Stable authored key; display text and file names are not identity. |
| Instance ID | One specific world object/item | `FGuid`, e.g. `UWorldObjectIdentityComponent.InstanceId` and `FWorldOwnedItemRecord.ItemId` | Unique per instance; not interchangeable with a definition ID. |
| Operation ID | One logical action/transaction attempt | `FGuid` in `FWorldOperationRequest`, operation ledger and domain receipts | Retries use the same ID and must not create duplicate effects. |
| Anchor/feature ID | Semantic construction attachment point | Authored IDs such as `End.A`, `End.B`, `Station.NN`, plus authored joint features | Use stable feature IDs/tags, never mesh-array positions or inferred filename numbers. |
| Process/recipe ID | Transformation rule | Definition ID referenced by recipe/process catalogues | Explicit inputs, output state, preconditions, cost and failure behavior. |
| Feedback definition ID | A cue definition such as SFX/VFX/UI/music | Separate stable cue ID (e.g. `SFX_WOOD_SPLIT`) | A cue ID is not the gameplay operation/event itself. |

### Existing naming ambiguity to protect against

In the candidate code, `FInventoryStack.ItemId` is a definition key (`FName`), but `FWorldOwnedItemRecord.ItemId` is a unique item instance (`FGuid`) and its `DefinitionId` is the authored type key. A physical item held as an inventory stack also carries `OwnedWorldItemId`. Do not collapse these fields or pass one namespace where another is required. If a later code refactor improves names, it must preserve saved data and public/Blueprint compatibility or provide a migration.

## 3. Existing shared contracts to reuse

The following are present in the candidate source branch and should be audited/integrated, not recreated on this planning branch:

| Concern | Candidate file(s) | Responsibility boundary |
|---|---|---|
| Machine-readable operation outcome/request | `GameplayCoreTypes.h/.cpp` | Stable result code, operation/request identity, target/definition/revision context, human-readable message separate from machine code. |
| Object identity/generic lifecycle | `WorldObjectIdentityComponent.*`, `WorldObjectIdentitySubsystem.*` | Opt-in world actor identity, definition key, generic lifecycle and registration. It does not own log/crop/animal domain state. |
| Idempotency/operation receipt | `WorldOperationLedgerSubsystem.*` | Tracks operation IDs and prior results. It does **not** make arbitrary multi-system mutation atomic on its own. |
| Committed-world notification | `WorldCommittedEventSubsystem.*` | Broadcasts committed facts after authoritative state changes. It is transient notification, not a durable event log. |
| Physical item ownership | `WorldItemOwnershipSubsystem.*` | Item instance ID, definition/base definition, mesh-variant family, quantity, owner kind/ID, world location and revision; transfer/split receipts are domain data. |
| Inventory stacks and recipes | `InventoryCraftingTypes.h`, `InventoryCraftingComponent.*` | Stack quantities/categories, recipe data and inventory operations. Unique physical actors require explicit linkage via owned instance ID. |
| Wood processing/geometry | `LogBuildPiece.*`, `LogBuildingTypes.h`, `LogCarryComponent.*`, `LogBuildToolsComponent.*` | Domain-owned cut/split/groove/trough/plank state, mesh variants, anchors and player interaction. |
| Placement | `Source/MyProject3/Placement/*`, `Config/Placement/PlacementProfiles.json` | Shared material-neutral kernel/solvers and material-specific feature providers. The project profile catalogue uses lowercase dotted IDs such as `log.end_to_end`; the build-part schema preserves that convention. |
| Structural graph | `WorldStructuralGraphSubsystem.*`, `StructuralPieceInterface.h`, `LogAttachmentComponents.*` | Shared support/load relationships. Domain actors still own their mesh, local state and authored anchors. |
| Logical terrain state/render prototype | `WorldTerrainStateSubsystem.*`, `VoxelTerrainChunkComponent.*` | Logical cell edit records are separate from the Marching Cubes chunk mesh prototype. Their complete integration, save/load and collision/navigation behavior are not verified. |
| Simulation time/orchestration | `WorldSimulationClockSubsystem.*`, `WorldSimulationCoordinatorSubsystem.*` | Explicit logical time and fixed-step coordination; systems not wired into the coordinator do not advance automatically. |
| Logical save/load | `WorldSaveCoordinatorSubsystem.*`, `WorldSimulationSaveGame.h` | Versioned logical snapshots and restore orchestration; full physical actor/graph/terrain restoration remains an open gate. |

Branch links:
- [Phase 1 foundation and registry](https://github.com/furkansarikiyak36-eng/myproject3/tree/phase-01-foundation-v2/Docs/Architecture)
- [Phase 2 core contracts](https://github.com/furkansarikiyak36-eng/myproject3/tree/phase-02-core-contracts/Docs/Architecture)
- [Broad candidate source stack](https://github.com/furkansarikiyak36-eng/myproject3/tree/feature/simple-intent/Source/MyProject3)

## 4. Domain ownership and transaction contract

A state-changing operation should follow this logical sequence:

1. Resolve requester, target, stable definition/instance IDs, tool and context.
2. Validate target/revision/state/range/permission/tool/material/configuration without mutation.
3. Calculate the proposed result (outputs, transforms, graph edges, resource costs) without committing.
4. Reserve/check the operation ID and expected revisions; reject duplicate/conflicting requests deterministically.
5. Commit domain-owned changes and ownership changes under a documented transaction/rollback boundary.
6. Store the final operation receipt; publish committed events only after the authoritative domain result is final.
7. Mark the required persistent domains dirty/snapshot-ready.

**Important current gap:** the candidate operation ledger and per-domain receipts offer deduplication, but the source-status report describes best-effort rollback across world subsystems. Do not claim all-or-nothing behavior across actors, item ownership, structural graph, terrain and production until failure injection and engine-level tests prove it. Any operation that cannot safely roll back must fail closed and report the state accurately.

A failed operation must not consume ingredients, duplicate physical outputs, reserve a snap anchor, emit a success cue, or publish a misleading committed event. Retry behavior must use the same operation ID and return a consistent result.

## 5. Separate state dimensions

Do not force these into one universal enum:

- Generic actor lifecycle: world / held / placed / consumed / destroyed.
- Item ownership: world / held / inventory / storage / in transit / consumed / transformed.
- Wood processing: whole / cross-cut-length variant / split half-round / quarter section / hewn / grooved / trough / plank / notched etc.
- Structural state: attached graph relations, supports, load, damage, occupancy.
- Terrain: cell/material/depth/edit/revision state plus renderer/collision representation.
- Crop/animal state: domain-specific growth/lifecycle state.
- Environmental/region state: contamination, soil/water quality, vegetation and recovery progress.

The first two dimensions overlap in words but answer different questions. Define bridge transitions for physical items and validate consistency; do not assume a one-to-one mapping for every ownership type.

## 6. Data schemas introduced in this branch

| Schema | What it defines | C++/configuration connection |
|---|---|---|
| `Schemas/item-definition.schema.json` | Definition key, coarse inventory category, detailed family, physical/stacked representation, max stack, state/durability/persistence policy | Maps to `FInventoryStack` and item ownership definition fields; unique instance stays a separate GUID |
| `Schemas/material-definition.schema.json` | Material family, hardness/diggability, tool requirements, yields, transition references and surface audio | Supports terrain/resource data; does not replace runtime terrain cells |
| `Schemas/recipe-definition.schema.json` | Inputs, outputs, preconditions, duration, resource/tool costs, interruption and atomic commit policy | Catalog contract for crafting/production; actual execution remains owned by inventory/production/process domains |
| `Schemas/process-transition.schema.json` | Allowed input states, tools, ground/attachment preconditions, output states/mesh family and atomic transformation | Maps to domain transitions; geometry/mesh variant must agree with state |
| `Schemas/build-part-definition.schema.json` | Dimensions in cm, local axis/pivot/collision, anchors and provider/profile references | Must reuse `PlacementProfiles.json` and existing semantic anchor IDs; no second snap kernel |
| `Schemas/region-definition.schema.json` | Region starting state, resources, ambient cues and restoration thresholds | Data contract only; regional simulation should remain coarse and persistent |
| `Schemas/feedback-event-definition.schema.json` | Logical sound/VFX/animation/UI/music cue, trigger, priority, concurrency and fallback policy | Cues react to accepted operations; they never authoritatively mutate gameplay state |

### Schema namespace rules

- Definition/cue IDs in the new catalog use uppercase ASCII and underscores (for example `ITEM_LOG_WHOLE_600`, `MAT_SOIL_CONTAMINATED`, `REC_REPAIR_FILTER`, `SFX_WOOD_SPLIT`).
- Existing placement profile IDs are deliberately not forced into that namespace because the current config uses lowercase dotted names (for example `log.end_to_end`).
- Anchor feature IDs preserve their actual semantic spelling. `End.A`, `End.B`, and `Station.NN` are valid feature IDs and are not definition IDs.
- File names, display names, localization keys, mesh digits and UI labels are not stable identity.

## 7. Persistence contract boundary

The candidate `UWorldSimulationSaveGame` is versioned (the source declares schema version 2) and contains logical-domain snapshot types. The H–Q status report explicitly says full save/load does not yet restore all physical actor instances/transforms, structural graph links, world-partition state and terrain mesh/collision data; migration and cross-domain rollback also require more work.

Phase 1 therefore defines stable keys and ownership/revision expectations but does not declare persistence complete. Before marking the contract integrated, confirm:
- instance/definition IDs and expected revisions survive save/load;
- actor records map to exactly one world/held/inventory/storage owner;
- structural graph edges and actor attachments agree after restore;
- terrain cell edits rebuild mesh/collision/navigation;
- failed restore leaves a recoverable state and logs the failed domain;
- schema migration is versioned and tested.

## 8. Decisions for the next implementation branch

1. Reuse the candidate shared operation, identity, ownership, event and save contracts after branch reconciliation; do not add duplicates on `main`.
2. Keep these schemas as design-time contracts until a separate decision chooses Data Assets, Data Tables or runtime JSON.
3. Resolve naming and owner boundaries before moving fields across classes.
4. Keep one shared placement kernel; domain-specific providers own compatibility.
5. No mechanic changes to `main` and no engine-validation claim is made by this documentation branch.
