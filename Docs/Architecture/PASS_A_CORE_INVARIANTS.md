# Pass A — Core Invariants (Initial Source Layer)

**Status:** Initial source layer added; static review and integration remain open.  
**Branch:** `mechanic-detailing-pass-a-core`  
**Registry focus:** M006, with event timestamp integration for M005.

## Delivered

- `UWorldSimulationClockSubsystem` owns explicit logical simulation time, tick index, seed and state revision.
- Time only advances when a future simulation driver calls `AdvanceSimulation`; it does not silently follow frame rate.
- Negative, NaN, infinite, uninitialized and overflow-prone advances are rejected.
- A versioned snapshot can be exported/restored. This is a persistence contract, not an automatic save system.
- Per-operation deterministic seeds are derived from the base seed and operation GUID without consuming a shared random stream.
- The committed-event broadcaster now uses logical simulation time instead of `UWorld::GetTimeSeconds()`.

## Integration still required

- A game/world bootstrap must call `InitializeClock` with the selected world seed.
- A fixed-step simulation driver must decide when and how to call `AdvanceSimulation`.
- The save coordinator must persist and restore the clock snapshot before restoring dependent systems.
- Systems must request deterministic per-operation seeds and avoid their own unrelated clocks/seeds.
- Operation-ID deduplication, a transaction coordinator and durable event history are not implemented by this pass.

## Validation boundary

No Unreal build/UHT/Editor/PIE/gameplay tests were run. Do not mark M006 or M005 verified from source presence alone.
