# Phase 2 — Core Identity and Operation Contracts

**Status:** Source implementation added; static review pending final integrated validation.  
**Branch:** `phase-02-core-contracts`  
**Base:** Phase 1 closeout commit `2786174fb3700cb924aea20e8de9b350b4935749`  
**Engine target:** Unreal Engine 5.8

## Goal

Implement the smallest shared C++ contract layer needed by later domains without replacing the existing log prototype, introducing a parallel world-state owner, or routing every interaction through a global dispatcher.

## Implemented source files

- `GameplayCoreTypes.h/.cpp`
  - Stable machine-readable `EWorldOperationResultCode` outcomes.
  - `FWorldOperationResult` envelope with success flag, stable code, request/operation GUID, explanatory text, and IDs of objects actually changed by a successful operation.
  - Factory methods prevent a failure from being constructed with the `Success` code and ensure every result has a diagnostic operation ID.
  - `FWorldOperationRequest` carries operation identity/type, optional requester, target IDs, source/tool/recipe definition keys, expected revisions and optional placement context. Structural validation rejects missing IDs/types and malformed target/revision entries.
  - `FWorldResourceAmount` requires an explicit resource definition key and unit key, rejects negative/non-finite quantities, and offers a non-mutating sufficiency check.
- `WorldObjectIdentityComponent.h/.cpp`
  - Optional actor component providing a GUID instance ID, authored definition key, generic lifecycle state, and monotonic state revision.
  - Identity is generated when absent; existing valid identity is preserved unless a duplicate is found in the same world.
  - A world subsystem tracks active component IDs and assigns a new GUID when duplicated actor data reuses an active ID.
  - Lifecycle changes use an expected-state guard and a restricted transition table.
  - Consumed and destroyed states are terminal. Same-state and stale-state transitions are rejected.
  - The component does not currently own or implement world save/load.

- `WorldCommittedEventSubsystem.h/.cpp` and `FWorldCommittedEvent`
  - Versioned committed-fact payload with event ID, operation ID, affected object IDs, world time, and diagnostic text.
  - World-scoped multicast notification; rejects missing operation IDs, `None` event types, and unsupported schema versions.
  - Event dispatch is notification only; it is not durable, queued, replayed, or persisted. Domain owners must publish only after committing state.

## Lifecycle transition table

| Current state | Allowed next states |
|---|---|
| World | Held, Placed, Consumed, Destroyed |
| Held | World, Placed, Consumed, Destroyed |
| Placed | World, Held, Destroyed |
| Consumed | None |
| Destroyed | None |

This is only generic instance lifecycle. Wood processing (whole/split/grooved), structural integrity, crop growth, animal needs, terrain state, and other domain state must remain in their domain owners.

## Integration rules

- The new component is opt-in; it is not automatically added to every existing actor or Blueprint.
- Existing `ALogBuildPiece` remains the authority for current log piece type, processing prerequisites, snap anchors, and structural links.
- Existing `ULogCarryComponent` remains the player-facing input/preview owner. It must not directly mutate the identity component to bypass a domain owner's validation.
- Later domain phases should migrate one mechanic at a time, after checking tracked C++ callers and local Blueprint consumers.
- `SaveGame` metadata is a serialization annotation only; no working save slot, world snapshot, migration, or load restoration is claimed by this phase.
- Active-world uniqueness is enforced by the subsystem for actors that register this component. Save/load round-trip and cross-world identity semantics are still not implemented or verified.

## Static acceptance checklist

- [x] Add shared result codes without changing existing log API signatures.
- [x] Ensure operation results carry machine-readable failure codes and an operation ID.
- [x] Ensure failure factory cannot emit the Success code or affected-object success list.
- [x] Add a validated request envelope with expected-revision guards and optional context.
- [x] Add a unitized non-negative resource amount contract without introducing an inventory owner.
- [x] Add stable instance ID and authored definition ID fields.
- [x] Add guarded lifecycle transitions and revision increment on accepted transition only.
- [x] Keep generic lifecycle separate from domain-specific processing state.
- [x] Avoid auto-inserting the component into existing actors or replacing their current state owners.
- [x] Add a versioned committed-event payload and explicit post-commit notification entry point.
- [x] Validate event type, schema version, and operation ID before broadcast.
- [x] Perform source-level review of the new public types and keep existing log/carry signatures unchanged.
- [ ] Run Unreal Engine 5.8 build/UHT and editor/runtime tests at the agreed end-of-phases gate.

## Explicitly not claimed

- No Unreal Engine build, UHT, editor session, PIE, or gameplay test was run.
- The 119 mechanics are not implemented by adding these shared types; registry status remains unreviewed until owning implementations and evidence are attached.
- No world clock subsystem, deterministic random subsystem, survival needs simulation, durable event queue/replay, or save/load system is implemented here. The event subsystem only timestamps notifications with the existing UWorld time. Resource/inventory mutation remains deferred to the resource domain.
