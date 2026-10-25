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
- `WorldObjectIdentityComponent.h/.cpp`
  - Optional actor component providing a GUID instance ID, authored definition key, generic lifecycle state, and monotonic state revision.
  - Identity is generated when absent; existing valid identity is preserved unless a duplicate is found in the same world.\n  - A world subsystem tracks active component IDs and assigns a new GUID when duplicated actor data reuses an active ID.
  - Lifecycle changes use an expected-state guard and a restricted transition table.
  - Consumed and destroyed states are terminal. Same-state and stale-state transitions are rejected.
  - The component does not currently own or implement world save/load.

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
- [x] Add stable instance ID and authored definition ID fields.
- [x] Add guarded lifecycle transitions and revision increment on accepted transition only.
- [x] Keep generic lifecycle separate from domain-specific processing state.
- [x] Avoid auto-inserting the component into existing actors or replacing their current state owners.
- [x] Perform source-level review of the new public types and keep existing log/carry signatures unchanged.
- [ ] Run Unreal Engine 5.8 build/UHT and editor/runtime tests at the agreed end-of-phases gate.

## Explicitly not claimed

- No Unreal Engine build, UHT, editor session, PIE, or gameplay test was run.
- The 119 mechanics are not implemented by adding these shared types; registry status remains unreviewed until owning implementations and evidence are attached.
- No world clock, deterministic random subsystem, survival needs simulation, event bus, or save/load system is implemented here.
