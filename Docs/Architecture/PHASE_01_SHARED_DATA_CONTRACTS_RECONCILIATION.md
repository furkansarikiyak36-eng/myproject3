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

The candidate `UWorldSimulationSaveGame` currently declares **schema version 13** in `Source/MyProject3/WorldSimulationSaveGame.h` at candidate source revision `29e2667277d7f6d41bf88e338b72334761f6d835`. The save model contains logical-domain snapshots, identity-bound physical actor records (including transforms/component physics), item transfer/split receipts, structural graph state, terrain cell records, inventories and other simulation-domain snapshots. `WorldSaveCoordinatorSubsystem.cpp` has migrations through v13, and captures/restores the actor/graph/domain snapshots in a defined order.

That is stronger than a purely logical-state stub, but it is **not yet a fully verified world round-trip**:

- Physical actor restore validates the snapshot against actors already present in the loaded world. A missing or class/component-mismatched actor causes the load to fail; the coordinator does not generally respawn arbitrary missing physical actors from the snapshot.
- The save includes `FWorldTerrainCellState` entries, not the density arrays in `UVoxelTerrainChunkComponent`. The inspected chunk component applies sphere edits to a local density field and rebuilds mesh/collision independently; no adapter from the terrain-cell ledger to that density field was found in the inspected files.
- `UWorldTerrainStateSubsystem` stores idempotency receipts in a `Transient` map. Its `ExportSnapshot` exports cells only, and `RestoreSnapshot` clears those receipts. A replay after load therefore cannot recover the original terrain-edit receipt by operation ID, although revision/precondition checks may still reject a stale replay.
- Saves are refused while physical items remain in the `Held` ownership state, because held/carry-component state is not included as a complete durable representation.
- The load path captures the pre-load state and attempts a rollback if applying the candidate save fails, but the coordinator itself reports that rollback may also fail and fault the simulation. This has not been tested with failure injection or in Unreal.
- World-partition streaming, actor recreation, Blueprint/component compatibility, visual/collision/navigation regeneration for edited terrain, and cross-domain save migration remain engine-level acceptance tasks.

Before marking persistence integrated, confirm:
- [ ] Instance/definition IDs and expected revisions survive a target-engine save/load.
- [ ] Physical actor snapshots have a deliberate missing-actor policy: fail closed, deterministic respawn, or level-owned actor reconciliation.
- [ ] Actor records map to exactly one world/held/inventory/storage owner.
- [ ] Structural graph edges and actor attachments agree after restore.
- [ ] Logical terrain cell records reproduce voxel density, mesh and collision, and any navigation representation.
- [ ] Terrain operation receipt behavior is documented and tested across save/load.
- [ ] Failed restore leaves a recoverable state and logs the failed domain.
- [ ] Schema migration and version rejection are tested against actual saved fixtures.

## 8. Decisions for the next implementation branch

1. Reuse the candidate shared operation, identity, ownership, event and save contracts after branch reconciliation; do not add duplicates on `main`.
2. Keep these schemas as design-time contracts until a separate decision chooses Data Assets, Data Tables or runtime JSON.
3. Resolve naming and owner boundaries before moving fields across classes.
4. Keep one shared placement kernel; domain-specific providers own compatibility.
5. No mechanic changes to `main` and no engine-validation claim is made by this documentation branch.

## 9. Targeted source/API review against `feature/simple-intent`

**Review kind:** static inspection of selected headers and implementation paths only. No file was compiled, no Unreal Header Tool was run, and no Editor/PIE/save-load scenario was executed in this task.

Inspected candidate source file revisions:
- `GameplayCoreTypes.h` — `fa8f98620d74d9f04f103cc890aa67f72cb31bb4`
- `WorldOperationLedgerSubsystem.cpp` — `3a68798d2d5fc5204bcd5050aa659a86fc7610b1`
- `WorldItemOwnershipSubsystem.cpp` — `37a700dac17c5d2284e724f055aa4bd7aae5aa60`
- `InventoryCraftingComponent.cpp` — `2f1f825797836f9d3f6372d89af030dcaf9c396d`
- `LogBuildPiece.cpp` — `37ac9e8c1fab2cbca7ad65520b273280e3835830`
- `WorldStructuralGraphSubsystem.cpp` — `c470923d0c18a113700175b2fcbc5ceac69ac1f8`
- `WorldTerrainStateSubsystem.cpp` — `74acbe755d9a083ea664d1a914b996b7a9201451`
- `VoxelTerrainChunkComponent.cpp` — `ebafb128a3c9021c4b7bf22092fec3096f2a168b`
- `WorldSaveCoordinatorSubsystem.cpp` — `ade371ce09cd50771e214692312b99d158a10ec0`

### Confirmed from the inspected source surface

1. **Core result and request types exist.** `GameplayCoreTypes.h` declares the shared operation-result code enum, `FWorldOperationResult`, `FWorldOperationRequest`, `FWorldResourceAmount`, committed-event payload, and generic lifecycle enum.
2. **Ownership and item definition are distinct.** `FInventoryStack.ItemId` is a definition key; `OwnedWorldItemId` is a physical-item GUID. `UWorldItemOwnershipSubsystem` exposes transfer, consume, definition update, split receipts, and snapshot APIs.
3. **Physical log splitting has a domain-owned receipt path.** `ALogBuildPiece::TryCommitPhysicalSplitOutputs` calls the ownership ledger's split operation using the logical operation ID, validates returned outputs, updates physical piece state, and publishes an `ObjectProcessed` notification after commit. The caller can recover previous split output IDs through the split receipt path.
4. **Inventory/physical actor bridging exists as candidate source.** `UInventoryCraftingComponent::TryStorePhysicalActor` transfers a registered world item to an inventory owner and guards the inventory row/lifecycle transition; `TryDropOwnedItem` stages an actor, assigns the existing item identity, transfers ownership, then activates the actor. These are multi-step compensation flows, not yet verified in Unreal.
5. **A save coordinator exists at the logical/source level.** `UWorldSaveCoordinatorSubsystem::CaptureCurrentState` rejects saves with pending operations or held items, captures physical actor snapshots, reconciles them with ownership records, snapshots the structural graph, inventory components, terrain cells and other simulation domains. `ApplySnapshot` validates schema/actor compatibility, restores identity/ownership and graph links in a deliberate order, and the load entry point attempts to restore a pre-load snapshot if applying the candidate save fails. Its own code reports simulation fault if rollback also fails.
6. **Terrain has two distinct representations.** `UWorldTerrainStateSubsystem::TryApplyCellEdit/ExportSnapshot/RestoreSnapshot` maintains bounded logical cell state. Separately, `UVoxelTerrainChunkComponent::ApplySphereEdit/RebuildMesh` changes a density field and rebuilds mesh/collision. In the inspected implementation files, the chunk component does not call the terrain-state subsystem. A single authoritative edit transaction joining cell state, chunk density, collision/navigation and persistence was not found in this spot-check.

### Concrete contract gaps to resolve before Phase 01 closeout

| Gap | Source evidence | Why this blocks closure | Required next action |
|---|---|---|---|
| Generic operation ledger adoption is incomplete | `WorldOperationLedgerSubsystem.cpp` defines `TryBeginOperation/CompleteOperation`; inspected domain files `LogBuildPiece.cpp`, `InventoryCraftingComponent.cpp`, `WorldItemOwnershipSubsystem.cpp`, `WorldStructuralGraphSubsystem.cpp`, and `WorldTerrainStateSubsystem.cpp` do not call those entry points. The save coordinator does inspect the ledger and restores its snapshot. | The ledger must not be assumed to make domain operations idempotent/atomic merely because it exists. Item-transfer and split receipts are separate mechanisms. | Build a per-operation table: global ledger receipt, domain receipt, expected revision, commit boundary, and retry behavior. Adopt the global ledger only where it supplies value; do not blindly wrap every call. |
| Some actor transfer helpers generate their own operation ID | `ALogBuildPiece::TryTransferOwnership` calls `TryTransferItem(FGuid::NewGuid(), ...)`; inspected inventory world/storage/drop helpers also create IDs internally. | A caller retry cannot reuse its original operation ID for those helper calls; revision/owner checks may reject a second transfer, but they cannot return the same receipt under the caller's ID. | Prefer passing a stable caller operation ID into multi-step commands, or document that those APIs are intentionally non-retryable and enforce a stable owner/revision conflict result. |
| Logical terrain edits are not connected to voxel chunk density in the inspected source | `WorldTerrainStateSubsystem.cpp` owns cell records; `VoxelTerrainChunkComponent.cpp` owns `Density`, `ApplySphereEdit` and `RebuildMesh`. No direct adapter is present in these two files. | A logical edit could be saved without its visible density/collision change, or a visual edit could exist without the logical state record. | Create one bounded terrain-edit service/adapter that preflights, applies logical and chunk changes together, rebuilds collision/navigation, and saves enough data to reproduce the result. |
| Inventory runtime types are smaller than the design-time recipe schema | `FInventoryStack` has definition ID, quantity, category, max stack and optional owned-world-item GUID; `FCraftingRecipe` has ingredients, output, by-products and enabled flag. Recipe version, tool/station preconditions, duration/interruption and other schema fields are not all represented in these structs. | The schema cannot be treated as an implemented runtime data model yet. | Decide which fields are runtime recipe data, which belong to process/production owners, and whether definitions use Data Assets/Data Tables/validated import. Record that mapping before data-driven production is implemented. |
| Full persistence remains engine-unverified | Snapshot APIs include actor, ownership, graph, inventory and terrain-cell data, but this source review did not validate actor creation/order, physical attachment restore, terrain density reconstruction or rollback under injected failure. | Source existence is not proof that a saved world reproduces the same playable state. | Keep UHT/UBT, Editor load, PIE round-trip and fault-injection tests as independent gates. |

### Status decision

- **Design schemas:** CI-validated for schema shape and representative fixtures.
- **Header/source mapping:** targeted static review performed for the listed contracts.
- **All M001–M119 source call paths:** still open; broad symbol matches do not close each mechanic.
- **Unreal compile/UHT/Editor/PIE/save-load:** not run in this planning task.
- **Phase 01:** remains open until the remaining field/API caller mapping and ownership/terrain contract decisions are recorded. Do not upgrade registry mechanics or claim runtime completion from this source review.

## 10. Consolidation follow-up — 2026-10-28

- The authoritative authoring schemas and samples live under `Docs/Architecture/Schemas/` and `Docs/Architecture/Samples/`; the validator is `Tools/Validation/validate_gameplay_catalogue.py`. Do not add parallel lower-case schema copies or treat JSON Schema authoring records as runtime-loaded game data.
- The canonical per-mechanic source/status matrix is `Docs/Architecture/PHASE_00_M001_M119_TRACEABILITY_MATRIX.csv`; registry status and source-review status are separate fields.
- The planning PR is still a draft, documentation/tooling-only change. No gameplay C++ or Unreal content was changed by this consolidation. Source APIs and domain operations remain candidate-branch evidence, not proof of current-main integration.
- Phase 01 remains open until the ownership/transaction/terrain gaps above receive explicit implementation decisions, all relevant callers and Blueprint contracts are reviewed, and UHT/UBT, Editor/PIE, save/load and failure-path checks are recorded separately.

## 11. Core API/caller follow-up — 2026-10-28

- Added [PHASE_01_CORE_API_CALLER_AUDIT.md](PHASE_01_CORE_API_CALLER_AUDIT.md), which follows selected candidate functions for operation results/identity, ownership transfers and split receipts, inventory transactions, structural graph edges, logical terrain edits, voxel density edits, save/load and fixed-step rollback.
- A targeted callsite scan found no calls to the generic operation-ledger mutation entry points in the 17 reviewed domain implementation files. The ledger is used for pending checks and snapshot export/restore, but domain idempotency currently depends on narrower receipts and expected-revision rules.
- The log split path accepts a caller-supplied operation ID and has a domain receipt; several inventory storage/drop/craft helpers generate fresh IDs internally. Stable retry identity is therefore not yet consistent across the full operation chain.
- The logical terrain cell ledger and voxel chunk density/mesh component were inspected as separate owners; no connecting edit transaction was found in the inspected source path.
- The audit is targeted source inspection only. All-M001–M119 call-chain review, public API/Blueprint compatibility, UE 5.8 build/UHT/PIE and full save/load validation remain open.
