# Phase 01 — Core API and Cross-Domain Caller Audit

**Review date:** 2026-10-28  
**Repository:** furkansarikiyak36-eng/myproject3  
**Candidate source line:** feature/simple-intent  
**Review type:** targeted static C++ source/callsite inspection; no local Unreal checkout, UHT/UBT build, Editor, PIE, save/load round-trip, or fault-injection test was run in this review.

## 1. Purpose and limits

This audit follows the shared contracts into selected domain implementations. It is narrower than the complete M001–M119 call-chain audit. It records what inspected source currently does and the open integration conditions needed before Phase 01 can close.

Source was inspected on feature/simple-intent, including the exact source blob revisions listed at the end. These are candidate sources, not automatically part of main. The working planning branch remains documentation/tooling-only.

## 2. Core request/result flow

| Concern | Source path and symbols | What the code establishes | Remaining gap |
|---|---|---|---|
| Stable operation result | GameplayCoreTypes.cpp: FWorldOperationResult::Success/Failure; GameplayCoreTypes.h: EWorldOperationResultCode, FWorldOperationRequest | Machine-readable failure codes; result carries operation ID/message/affected instance IDs; failure factory prevents Success code and clears affected IDs; request can validate operation type, target IDs and expected revisions | A request/result envelope is not a transaction coordinator. Not every domain returns it, and no common wrapper enforces preflight/commit/rollback across every system |
| Request identity | FWorldOperationRequest::EnsureOperationId/IsStructurallyValid | Missing operation ID can be generated; operation type must be named; target/revision records must be structurally valid | Structural validity does not prove target ownership, range, tool requirements, revision equality, capacity, or gameplay preconditions |
| World instance identity | WorldObjectIdentityComponent.cpp: BeginPlay, EnsureIdentity, TryTransitionLifecycle; WorldObjectIdentitySubsystem.cpp: RegisterIdentity/UnregisterIdentity | Missing actor IDs are generated; registration is world-local; duplicate IDs on different live components are reallocated; lifecycle transition checks expected state and increments revision | Component is opt-in; current log state and ownership state are not automatically synchronized with generic lifecycle for every actor. World-local registry is not the durable save/restore authority |
| Definition identity | UWorldObjectIdentityComponent::SetDefinitionId; ownership DefinitionId/BaseDefinitionId | Definition names are separate from per-instance GUIDs; processed outputs can retain a base definition | Definition validity is not checked by SetDefinitionId; authoring/schema validation and runtime loaded definitions are separate and not yet wired together |

### Required rule

Callers may express intent, but the owning domain must validate and commit its own authoritative state. A generic request object does not authorize a caller to mutate another system's private state or create a competing copy of it.

## 3. Operation ledger and idempotency

### Observed implementation

WorldOperationLedgerSubsystem.cpp implements:
- TryBeginOperation(OperationId): reserves a new pending ID and rejects invalid/duplicate IDs or capacity overflow.
- CompleteOperation(Result): completes only a pending entry and validates result-code/success agreement and affected IDs.
- TryGetCompletedResult(OperationId): returns a recorded completed result.
- CancelUncommittedOperation(OperationId): removes only a pending operation.
- ExportSnapshot/RestoreSnapshot: copies the in-memory ledger to/from a validated snapshot.

The class documentation explicitly treats this as an in-memory deduplication ledger, not a transaction/rollback engine, and it is not durable after world teardown unless a caller snapshots it.

### Candidate call-site scan

A source scan checked these 17 candidate implementation files for calls to TryBeginOperation, CompleteOperation, CancelUncommittedOperation, and TryGetCompletedResult:
- LogBuildPiece.cpp, LogCarryComponent.cpp, InventoryCraftingComponent.cpp, WorldItemOwnershipSubsystem.cpp, WorldStructuralGraphSubsystem.cpp, WorldTerrainStateSubsystem.cpp, VoxelTerrainChunkComponent.cpp, WorldAgricultureSubsystem.cpp, WorldSurvivalSubsystem.cpp, WorldProductionSubsystem.cpp, WorldCargoTransportSubsystem.cpp, WorldStorageSubsystem.cpp, WorldThreatDirectorSubsystem.cpp, WorldRaidAdapterSubsystem.cpp, WorldSimulationCoordinatorSubsystem.cpp, WorldSaveCoordinatorSubsystem.cpp, and WorldCommittedEventSubsystem.cpp.

**Finding:** no domain call to those four entry points was found in that reviewed file set. The coordinator and save coordinator use ledger snapshot/pending-operation methods, but this does not mean domain operations are already registered in the ledger.

Domain-owned receipts do exist for selected item transfer/split and process/production flows. Do not blanket-wrap operations with the global ledger until the transaction owner and receipt semantics are deliberately selected.

## 4. Item identity, ownership, inventory and crafting

### Physical item ownership

WorldItemOwnershipSubsystem.cpp:
- RegisterItem requires a valid item GUID, nonempty definition, positive quantity/revision, valid owner type and finite location.
- TryTransferItem requires operation ID, item ID, expected revision and expected owner. It checks the full request against a prior receipt before returning that recorded result; otherwise it validates the current owner/revision, mutates owner/location once, increments revision, and stores a transfer receipt.
- TrySplitItem validates world ownership, output definitions/locations/capacity and stores a split receipt so retries can return the prior output records instead of splitting a second time.
- TryConsumeItem delegates to transfer to Consumed.

This provides more specific idempotency than the generic ledger for those operations, but receipts depend on the caller supplying and reusing the same operation ID.

### Physical log split

ALogBuildPiece::TryCommitPhysicalSplitOutputs:
1. Validates current ownership is a world-owned single physical item; validates that there are exactly two output definitions, lengths, profiles, legacy types, valid meshes and distinct finite output transforms.
2. Allocates the second log actor deferred and assigns its state before activation.
3. Snapshots ownership records/transfer/split receipts.
4. Calls Ownership->TrySplitItem(OperationId, ...) with the caller-provided operation ID.
5. Validates the returned output count; on later physical commit failure, attempts to destroy the child, restore the ownership snapshot and restore the original actor's mesh/type/transform/identity/collision.
6. Publishes ObjectProcessed only after successful commit in the inspected path.

**Remaining gap:** rollback calls report a result but are not evidence that every UE actor/graph/collision side effect can always be reversed. This path needs failure injection and in-editor testing; it must fail closed if ownership rollback fails.

### Inventory stack / physical item boundary

InventoryCraftingComponent.cpp keeps inventory stack rows and recipe definitions while also consulting WorldItemOwnershipSubsystem to bridge physical items, storage, drops, consumption, and crafting. This is a UI/container facade plus an operation layer, not yet proof that all stack quantities and physical ownership transitions share one atomic transaction.

The inspected store/drop/storage/craft helpers commonly pass FGuid::NewGuid() to ownership transfers internally. That means the public call cannot necessarily retry the same nested transfer using its original operation ID and obtain the same receipt. Revision/owner checks may reject the retry, but that is not the same as returning the original completed outcome.

**Required decisions before closure:**
- Keep ItemId (definition) separate from OwnedWorldItemId (physical instance).
- State which object is the canonical quantity owner for each item form: physical single instance versus stack of abstract resources.
- Choose one canonical mutation owner for quantity. Other inventory/container components can present views or use a transaction façade, but must not independently mutate duplicate counts.
- Thread caller operation IDs through multi-step store/drop/craft/consume commands, or document the command as non-retryable and define deterministic conflict recovery.
- Test compensation rollback when actor spawn, ownership transfer, inventory capacity, output registration, or destruction fails.

## 5. Structural graph versus actor-local snap state

WorldStructuralGraphSubsystem.cpp implements:
- RegisterNode/UnregisterNode;
- TryConnect/TryDisconnect;
- anchor occupancy checks, cycle checks, support-path checks and aggregate-load checks;
- deterministic dependent-removal order;
- damage/repair records and snapshot restore;
- RebuildLogActorAttachmentsFromSnapshot to reconcile actor-local log attachments from graph snapshot.

LogBuildPiece also stores parent/child links, snap anchor occupancy and placement/held state. That duplication is currently a compatibility bridge, not permission for two unsynchronized authorities.

**Required contract:** mesh provider owns local anchor geometry/tags; one accepted attach operation validates candidate pose and compatibility, then commits actor attachment, anchor occupancy and graph edge together. The graph should be the shared cross-material owner of support/removal relationships. Until all callsites are migrated, local log state must be reconciled through one adapter and a consistency check. Preview and commit must consume the same placement solution.

**Failure concern:** graph-level validation alone does not prove UE actor attachment/collision rollback and actor-local anchor reservations stay aligned during every failure. This requires PIE/failure-path evidence.

## 6. Logical terrain state versus voxel geometry

WorldTerrainStateSubsystem.cpp:
- TryApplyCellEdit validates operation ID, depth limits/allowlist, edit type, capacity, expected revision and edit-specific prerequisites before changing a logical cell record.
- It stores operation receipts for repeated logical cell edits and returns the prior resulting state for a matching retry.
- ExportSnapshot/RestoreSnapshot persist the logical cell records; in the inspected code, receipts are not included in the cell snapshot and are cleared on restore.

VoxelTerrainChunkComponent.cpp:
- InitializeFlatGround creates a local density array.
- ApplySphereEdit changes this density array directly.
- RebuildMesh extracts a marching-cubes mesh and rebuilds collision through the procedural mesh section.

**Critical integration finding:** no call from the reviewed VoxelTerrainChunkComponent.cpp into WorldTerrainStateSubsystem::TryApplyCellEdit, or vice versa, was found. These are separate logical and visual/physical representations in the inspected source. Therefore a cell ledger entry is not proof that the visible density field, collision, navigation and saved terrain edit agree.

**Required action:** a single bounded terrain-edit service must preflight the request, mutate/reconcile logical terrain and chunk density, rebuild collision/navigation, emit the committed event only after success, and persist enough information to regenerate the same edited geometry. Use bounded regions and edited-cell limits; do not expand this into unlimited voxel simulation.

## 7. Save/load and simulation coordinator

### Save coordinator

WorldSaveCoordinatorSubsystem.cpp:
- CaptureCurrentState requires the clock, threat director and several domain subsystems; rejects pending ledger operations and held items; captures ownership, operation/item receipts, physical actor snapshots, structural graph, inventories, logical terrain cells, agriculture, survival/discoveries, production, cargo and storage.
- The current candidate save model is schema version 13.
- ApplySnapshot validates the snapshot against physical actors already present in the loaded world, restores clock then ownership/operation receipts, applies physical actor state, restores graph and rebuilds actor-local log attachment links, then restores inventory and logical domains.
- LoadWorldState migrates schemas 4–12 to version 13 where supported, captures the current state before applying a candidate snapshot, and attempts to restore the previous snapshot if load fails. If rollback fails it logs an error and marks simulation faulted.

This is a substantial source-level save contract, but not a verified full world round-trip. If physical actor identity/class/component state does not match the actors in the loaded level, load fails closed; general missing actor respawn is not established. Terrain density values/mesh/collision are not included in the listed TerrainCells snapshot.

### Fixed-step coordinator

WorldSimulationCoordinatorSubsystem::AdvanceFixedStep preflights clock, threat, agriculture, survival, production and cargo; snapshots the logical participants plus ownership/storage/operation-ledger state; then advances each domain. If a step rejects after commit begins, it attempts snapshot-backed rollback and faults the simulation if rollback fails.

This is stronger than a single uncoordinated clock advance, but runtime failure injection is still needed to establish the rollback contract. The transaction does not by itself connect the voxel density field to terrain-cell state or prove physical-actor/structural graph restoration for every failure case.

## 8. Phase 01 decisions and exit gates

### Accepted for design baseline

- [x] Stable definition IDs and physical instance GUIDs are distinct namespaces.
- [x] Operation result, item transfer/split receipt, event notification, graph, logical terrain and save snapshot owners are mapped to existing candidate source.
- [x] Existing domain-specific receipts are kept distinct from the generic operation ledger.
- [x] Duplicate-authority risks are recorded: item quantity/ownership, graph versus actor-local connections, and logical cell state versus voxel density.
- [x] Targeted call-site scan recorded for the operation-ledger entry points in 17 domain files.

### Still open before Phase 01 closeout

- [ ] Complete exact owner-function/caller review for all M001–M119 rows; symbol matches alone do not close acceptance.
- [ ] Decide and implement stable caller operation-ID propagation for store/drop/storage/craft/consume helpers.
- [ ] Define one canonical item quantity/ownership mutation path and a migration plan for stack inventory.
- [ ] Define a single authoritative attach/detach transaction between actor transforms, local anchors and graph relationships.
- [ ] Integrate logical terrain cell state with voxel density, collision/navigation updates and save/restore.
- [ ] Validate every affected public Blueprint/API caller and editor-authored object before signature/refactor changes.
- [ ] Run target UE 5.8 UHT/UBT, Editor load, PIE success/failure cases, save/load round-trip and failure injection. Record logs and evidence per case.

## 9. Candidate source revisions inspected

- GameplayCoreTypes.h: fa8f98620d74d9f04f103cc890aa67f72cb31bb4; GameplayCoreTypes.cpp: f1c4be96155307f0af77a6e401c7e9ace99439ef.
- WorldObjectIdentityComponent.cpp: 6b9ea85891ab1cb2412ec6dcba62f0a9dff8bf73; WorldObjectIdentitySubsystem.cpp: 8b00e4de3b00456ac6e305c072f68f9251df04ae.
- WorldCommittedEventSubsystem.cpp: 2e2b15dbfb70ad70e132e56c11c0d43f6538f50a.
- WorldOperationLedgerSubsystem.cpp: 3a68798d2d5fc5204bcd5050aa659a86fc7610b1.
- WorldItemOwnershipSubsystem.cpp: 37a700dac17c5d2284e724f055aa4bd7aae5aa60.
- InventoryCraftingComponent.cpp: 2f1f825797836f9d3f6372d89af030dcaf9c396d.
- LogBuildPiece.cpp: 37ac9e8c1fab2cbca7ad65520b273280e3835830.
- WorldStructuralGraphSubsystem.cpp: c470923d0c18a113700175b2fcbc5ceac69ac1f8.
- WorldTerrainStateSubsystem.cpp: 74acbe755d9a083ea664d1a914b996b7a9201451; VoxelTerrainChunkComponent.cpp: ebafb128a3c9021c4b7bf22092fec3096f2a168b.
- WorldSaveCoordinatorSubsystem.cpp: ade371ce09cd50771e214692312b99d158a10ec0.
- WorldSimulationCoordinatorSubsystem.cpp: f97957f6bba6b78d5a0f52d17ebadbc1ef9b5b77.

**Honest status:** this is a targeted static source/call-site audit. It does not establish that any listed path compiles, loads in Unreal, or works in PIE.
