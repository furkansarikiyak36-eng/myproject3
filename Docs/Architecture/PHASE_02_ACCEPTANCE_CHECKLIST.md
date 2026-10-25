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

## Compatibility and ownership
- [x] Existing `ALogBuildPiece` remains the current log-processing/construction authority.
- [x] `ULogCarryComponent` remains the input/preview owner.
- [x] No global dispatcher, event bus, or duplicate world-state store introduced.
- [x] New identity component is opt-in rather than silently added to existing actors.
- [x] Keep component opt-in; local/untracked Blueprint use must be reviewed before wiring it into existing assets.
- [ ] Full UE build, editor/runtime checks and all-119 integrated demo remain deferred to the final test gate.

## Phase 2 exit criteria
Phase 2 source-contract work is ready for review after static checks of the new files and confirmation that existing log APIs are unchanged. Engine-level acceptance remains open until Unreal Engine 5.8 build and runtime evidence are recorded.
