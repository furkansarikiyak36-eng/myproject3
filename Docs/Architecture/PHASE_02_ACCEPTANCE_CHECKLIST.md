# Phase 2 Acceptance Checklist — Core Contracts

## Shared operation result
- [x] Stable result-code enum added in `GameplayCoreTypes.h`.
- [x] Result envelope distinguishes machine-readable code from human-readable message.
- [x] Each result receives a valid operation GUID when caller supplies none.
- [x] Failure factory rejects `Success` as a failure code and clears affected-object IDs.
- [x] Existing log/carry public signatures remain unchanged.
- [ ] Compile/UHT verification pending final integrated Unreal Engine 5.8 gate.

## World-object identity
- [x] Instance GUID field added.
- [x] Definition key is a stable `FName`, separate from display text.
- [x] Missing instance GUID is generated at BeginPlay.
- [x] Generic lifecycle and state revision are explicit.
- [x] Expected-state checks reject stale lifecycle transitions.
- [x] Terminal lifecycle states cannot transition back.
- [x] Add world-scoped active-ID registry; duplicate active IDs are reassigned before normal gameplay use.
- [ ] Persistence round-trip is not implemented or verified.

## Committed events\n- [x] Event payload contains schema version, event ID, operation ID, event type, affected stable IDs and world time.\n- [x] Dispatcher rejects invalid event type, unsupported schema and missing operation ID.\n- [x] Dispatcher is notification-only and does not mutate domain state or claim durable delivery.\n- [ ] Domain integration must call it only after authoritative state commit; no existing gameplay operation has been migrated to emit events yet.\n\n## Compatibility and ownership
- [x] Existing `ALogBuildPiece` remains the current log-processing/construction authority.
- [x] `ULogCarryComponent` remains the input/preview owner.
- [x] No global dispatcher, event bus, or duplicate world-state store introduced.
- [x] New identity component is opt-in rather than silently added to existing actors.
- [x] Keep component opt-in; local/untracked Blueprint use must be reviewed before wiring it into existing assets.
- [ ] Full UE build, editor/runtime checks and all-119 integrated demo remain deferred to the final test gate.

## Phase 2 exit criteria
Phase 2 now includes identity/lifecycle, shared operation outcomes, and the minimal committed-event contract/dispatcher. Static source-contract work is ready for review; no existing mechanic has been migrated to use the new contracts. Engine-level acceptance remains open until Unreal Engine 5.8 build and runtime evidence are recorded.
