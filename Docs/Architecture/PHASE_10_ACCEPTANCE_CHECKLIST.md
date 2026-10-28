# Phase 10 Acceptance Checklist

## Source review
- [ ] Run UE 5.8 UHT on Blueprint-exposed signatures.
- [ ] Validate duplicate operation IDs are rejected before mutation.
- [ ] Validate queue/alive counters after rejected operations and adapter spawn failures.
- [ ] Validate wave transitions reject unresolved enemies and terminal-state reuse.
- [ ] Validate cancellation cannot leave alive-population leaks.
- [ ] Validate NaN, infinity, negative and backwards-tick inputs.
- [ ] Store and enforce scheduled world tick via a clock adapter (not implemented).
- [ ] Connect M097 to the authoritative structural-integrity owner (not implemented).
- [ ] Inject explicit world seed for reproducible spawn selection (not implemented).

## Deferred Unreal integration gate
- [ ] Build MyProject3Editor with UE 5.8/UHT.
- [ ] Inspect subsystem Blueprint API in Editor.
- [ ] PIE: schedule, plan, select spawn/target and advance waves.
- [ ] Inject failed actor spawns and record only successful actors.
- [ ] Test cancellation/recovery and duplicate-operation replay.
- [ ] Add real NavMesh, AI, combat, structural-damage and persistence tests.

All M091–M100 remain unreviewed. Source presence is not acceptance evidence.
