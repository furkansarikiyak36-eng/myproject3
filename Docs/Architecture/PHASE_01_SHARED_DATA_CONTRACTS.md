# Phase 01 — Shared Gameplay Data Contracts

**Status:** Contract specification aligned to source layers found in the candidate implementation history. This document does not add C++ to this planning branch, and it does not claim the contracts are integrated or engine-validated.

## 1. Stable identity and definition keys

The project must separate **what an object is** from **which instance it is**.

| Concept | Contract | Current candidate evidence | Important boundary |
|---|---|---|---|
| Definition ID | Stable FName key (e.g. ITEM_WOOD_BRANCH, MAT_WOOD, RCP_SPLIT_LOG) | GameplayCoreTypes.h, item/recipe types | Never use a display name or mesh path as identity |
| Instance ID | FGuid for a persistent world instance | UWorldObjectIdentityComponent | The component is optional; do not silently migrate every actor |
| Operation ID | FGuid per attempted state-changing request | FWorldOperationRequest / FWorldOperationResult | Retries must not duplicate state-changing outcomes |
| Event ID | FGuid for each committed event payload | FWorldCommittedEvent | Event notifications are not automatically a durable event log |
| Revision | Monotonic per-instance state revision | Identity component / expected revision requests | Stale requests fail before changing state |

Recommended ID prefixes:
- ITEM_: inventory item or portable ingredient.
- MAT_: material/resource definition.
- TOOL_: tool definition.
- BLD_: build-part definition.
- RCP_: crafting or transformation recipe.
- CRP_: crop definition.
- REG_: region or region-state definition.
- OP_: operation type, where a stable enum/registry key is needed.
- EVT_: logical event/feedback catalogue key.
- VFX_, SFX_, MUS_, ANIM_: feedback assets/events.
- M001–M119: immutable mechanic-scope IDs; never reused for items or events.

These prefixes are naming rules, not a requirement to create a C++ enum for every entry. Data-driven IDs should remain data-driven where that reduces rebuilds.

## 2. Definition data versus runtime state

Definitions are immutable authored/configured facts; runtime state changes during play.

| Domain | Definition-level data | Runtime instance/state data |
|---|---|---|
| Item | ID, category, world representation, mass/volume, stack limit, supported actions | Quantity, condition, freshness, charges, ownership |
| Physical log | mesh variants, dimensions, axis, allowed processing transitions, anchors | processing state, held/placed state, parent/child links, revision |
| Stone piece | geometry/profile, material, anchor rules | damage/fracture stage, placement links, condition |
| Terrain material | material ID, hardness, diggability, support class, contamination class | edited cell/chunk state, depth layer, revision |
| Recipe | ID/version, inputs, preconditions, tool/station, outputs/by-products, costs | progress, reservation, interruption/result |
| Crop | ID, sowing windows, growth parameters, outputs, requirements | current stage, elapsed growth, water/stress, harvest readiness |
| Region | ID, bounds, baseline contamination and available opportunities | restoration progress, water/soil status, hazard state |
| Audio/VFX event | stable cue ID, trigger, mix/priority/concurrency policy | playing instance/active effect only; never authoritative gameplay state |

A display name, actor name, localized string, object-array index, or transient UObject pointer is not a persistent identity.

## 3. Generic lifecycle is not the domain state machine

Candidate core types define a generic lifecycle: World, Held, Placed, Consumed, Destroyed.

That lifecycle is **not** a substitute for:
- wood processing (whole, measured cut, split half-round, quarter, hewn, grooved, trough, plank);
- rock fracture stage;
- crop growth stage;
- item durability/charges/freshness;
- terrain cell edit state;
- structure integrity/support;
- survival exposure or discovery state.

Domain owners remain authoritative for their state. For the existing log actor, ALogBuildPiece continues to own wood processing and placement compatibility until all callers/Blueprint dependencies are reviewed. Do not create a second copy of bIsHeld, bIsPlaced, piece type, snap anchors or attachment graph as an independent source of truth. If generic identity/lifecycle metadata is added, it is a cross-reference/guard, not a second gameplay owner.

## 4. Operation request/result contract

Candidate source contract: FWorldOperationRequest, FWorldOperationResult, and EWorldOperationResultCode in GameplayCoreTypes.h.

An operation request may carry:
- operation GUID and stable operation type;
- optional requesting instance GUID;
- target instance GUIDs;
- source, tool and recipe definition IDs;
- expected object revisions;
- context position and semantic anchor ID.

Required transaction order:

1. Validate operation identity/type and target references.
2. Ask the domain owner to validate range, state transition, tool/resource prerequisites, configuration and occupancy.
3. Calculate the proposed outcome without mutating authoritative state.
4. Commit once; update owning state/revisions and ownership atomically.
5. Report the stable result code and IDs of the objects actually changed.
6. Publish feedback only after successful commit; mark persistent state dirty through the owning persistence path.

Failure must leave authoritative state unchanged unless the mechanic explicitly defines a documented cost-before-failure. A failure result must not use the Success code or include affected IDs that imply a successful commit.

Use existing code values such as InvalidTarget, OutOfRange, MissingTool, MissingMaterial, InvalidState, PrerequisiteNotMet, BlockedByPlacement, UnsupportedSurface, CapacityExceeded, MissingConfiguration, InsufficientResources, Conflict, Unsupported, and InternalError. Do not add a parallel PreconditionFailed enum just because an older design note used that phrase; map it to the existing PrerequisiteNotMet unless a reviewed migration says otherwise.

## 5. Units and coordinate rules

Use explicit units in data and function names wherever a raw number could be misread.

| Measurement | Project contract |
|---|---|
| Unreal world distance | centimetres |
| Actor transforms / positions | Unreal coordinates and declared local mesh axis |
| Mass in the current inventory candidate | grams |
| Volume if used for carry capacity | cubic centimetres (explicitly named) |
| Duration/time accumulation | seconds; world clock owns simulation-time progression |
| Angles | degrees internally; user interaction should remain contextual rather than demanding numeric angle selection |
| Resource quantity | amount + explicit resource definition ID + unit ID |

Do not mix kilograms and grams or metres and centimetres in recipe/inventory arithmetic. Do not assume every imported mesh uses local X as its long axis; record actual axis/scale/pivot for each geometry profile.

## 6. Inventory and recipe compatibility contract

Candidate inventory types currently use FInventoryStack, FRecipeIngredient, FCraftingRecipe, and UInventoryCraftingComponent.

The current candidate stack structure contains an item ID, quantity, category, max stack and tool flag; the later candidate also supports OwnedWorldItemId for a unique physical world item. The component provides slot and gram-based carrying constraints and recipe operations. This is an initial layer, not the final complete item-definition schema.

Required extensions or review items before expanding the economy:
- central item definitions for mass, volume, stackability, world representation, supported actions and state model;
- explicit treatment of large physical logs/boulders versus small stackable ingredients;
- item ownership receipts/IDs across pickup, carry, storage, drop, crafting, and world spawn;
- recipe version, preconditions, station/tool requirements, by-products, duration, wear/cost and rollback behavior;
- capacity preflight that includes output/by-products before inputs are consumed;
- save/load for the full item state and any in-progress transaction.

Do not rewrite the inventory component just to match a document. First review current methods, receipt/ownership subsystem, and automated tests in the candidate branch. Extend through its existing ownership contract.

## 7. Committed event and feedback contract

Candidate source type FWorldCommittedEvent has a schema version, event type, event GUID, operation GUID, affected object IDs, world time and message. UWorldCommittedEventSubsystem is explicitly a transient multicast notification route.

Rules:
- publish only after the owning domain has committed a real state change;
- previews and failed attempts are not committed events;
- VFX, SFX, animation and UI listen to accepted outcomes, but do not decide whether the operation succeeded;
- important warnings (raid, exposure, storm) must not depend on music alone;
- do not treat event multicast as persistence or replay. Persistent world state remains in domain snapshots/receipts and the save coordinator.

If a required event type is absent, add it through a reviewed, versioned contract change. Keep logical feedback IDs (SFX_..., VFX_..., MUS_...) separate from C++ event type IDs.

## 8. Persistence boundary

The candidate implementation contains UWorldSimulationSaveGame and UWorldSaveCoordinatorSubsystem source. The snapshot includes physical actor snapshots, inventory, terrain states, crops, animals, survival, discoveries, weather, production, cargo, storage, and structural graph data. Its current declared save schema version is 13.

This is evidence that a save architecture has been authored in that branch; it is **not** evidence that all participants serialize correctly or that a save restores safely in UE 5.8. Required validation still includes:
- deterministic restore ordering;
- actor identity conflict handling;
- parent/child anchor and structural graph reconstruction;
- carried/held/placed ownership consistency;
- inventory versus unique world-item ownership consistency;
- terrain edits and collision/navigation rebuilding;
- crop/animal/time/weather/raid/production state;
- schema migration, interrupted/invalid save rejection, and repeated load/save;
- actual Editor/PIE evidence.

The operation event dispatcher is transient; do not confuse it with the durable operation ledger or saved snapshots.

## 9. Contract gaps to carry into Phase 02+

| Gap | Why it matters | Action |
|---|---|---|
| Main has no shared core contract source | Newer contracts are in non-main branches | Use candidate branch history; do not duplicate into main by hand |
| Log held/placed state is already owned by the log actor | Identity component could create competing lifecycle state | Preserve domain owner; add only an explicit adapter/migration |
| Inventory definition data is sparse | Stack types alone cannot represent all physical/conditioned items | Add data-driven definitions after ownership methods are audited |
| Event multicast is transient | It cannot alone support replay or save/load | Persist state/receipts in domain/save systems |
| Full world save has not been engine-validated | Snapshot classes can compile yet restore incorrectly | Keep build, PIE and save/load gates open |
| Feedback IDs are not a complete event registry | Different systems may invent incompatible sound/FX names | Bind each catalogued cue to a stable gameplay outcome/event |
| Branch history has two apparent phase-number schemes | Plans and task references could diverge | Use Phase 00 audit plus the existing Phase 01–14 stacked implementation map; record the 16-phase legacy reference until resolved |

## 10. Acceptance checklist

- [x] Definition ID and instance ID are explicitly distinguished in the contract.
- [x] Generic lifecycle is separated from domain-specific state machines.
- [x] Operation request/result/event responsibilities are documented against existing candidate C++ types.
- [x] Existing inventory types are treated as a base layer, not mistaken for the complete inventory data model.
- [x] Persistence contract distinguishes transient event notification from durable saved state.
- [ ] Source API/Blueprint caller review completed for the chosen implementation head.
- [ ] Candidate branch reconciled with current main documentation and memory.
- [ ] Unreal 5.8 build/UHT evidence recorded.
- [ ] Editor/PIE operation and save/load evidence recorded.


## 11. Machine-readable contract artifacts

- GAMEPLAY_DEFINITION_CATALOG_SCHEMA.json defines the planned catalog envelope for items, materials, tools, build parts, recipes, crops and regions.
- FEEDBACK_EVENT_CATALOG_SCHEMA.json separates committed gameplay facts (EVT_...) from their SFX/VFX/animation/music cues.
- WOOD_PROCESSING_TRANSITIONS.json records length fraction separately from cross-section profile and defines the intended transformation prerequisites and output requirements.
- PHASE_00_M001_M119_BASELINE.csv adds a proposed implementation-phase mapping and current-main evidence columns without rewriting the canonical registry fields.

GitHub Actions run [40583305901](https://github.com/furkansarikiyak36-eng/myproject3/actions/runs/40583305901) passed catalogue validation on 2026-10-27: all 14 JSON Schema Draft 2020-12 schemas pass meta-schema checks; the M001–M119 registry has 119 unique IDs, valid entries and resolved mechanic dependencies; 45 representative sample records validate; and sample IDs are unique within each schema family. The validator intentionally does not require every cross-reference to resolve when the target definition is absent from the representative sample set. These are design-time catalogue checks, not evidence that the runtime loads catalogs, that C++ compiles, or that Editor/PIE/save-load behavior is correct.

## 12. Canonical phase field discrepancy

In the latest candidate registry JSON all 119 mechanics currently have phase=1. The separate Phase 01–14 architecture roadmap assigns implementation domains to later phases. Until a reviewed registry migration is agreed, do not use the JSON phase field as the implementation-order field. Use ProposedImplementationPhase in PHASE_00_M001_M119_BASELINE.csv as a provisional mapping and keep the original field/status untouched for traceability.
