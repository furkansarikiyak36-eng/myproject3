# Phase 10 — Raid Director / Threat AI (M091–M100)

## Goal
Add a bounded deterministic threat-planning domain: evaluate pressure, schedule raids, plan waves, filter spawn candidates, select targets and guard population/wave transitions. This is a state foundation, not complete enemy AI.

## Mechanic mapping
- M091: ScheduleRaid validates queue bounds and earliest world tick input.
- M092: EvaluateThreat calculates a clamped pressure-based difficulty.
- M093: ScheduleRaid/BuildWavePlans create bounded wave plans and enemy budgets.
- M094: queue/alive population bounds reject overflow before counters change.
- M095: spawn candidates must be valid, hidden from player visibility, and marked navigable.
- M096: deterministic target selection over caller-supplied eligible targets; actual NavMesh/pathfinding is external.
- M097: direct combat and structural damage intentionally not implemented; connect to authoritative structural integrity owner.
- M098: guarded Scheduled → Preparing → Active → Recovering → Active → Completed flow.
- M099: cancellation rejects terminal raids and raids with unresolved live enemies.
- M100: noise, valuable stock and defenses influence threat pressure.

## Ownership and invariants
Raid Director owns raid plans and threat counters only. Actor spawning, AI, navigation, combat, inventory, construction integrity, save/load and UI remain owned by other systems/adapters. Mutations require valid operation IDs; duplicates are rejected. Failed validation should not mutate counters. A spawn selection is not proof of actor spawn; only report successful spawns after the actor adapter confirms them.

## Limitations
- Earliest tick is validated and reported but not persisted or autonomously triggered; world-clock adapter needed.
- Random stream needs an explicit world seed before deterministic replay can be claimed.
- Candidate visibility/navigation flags are supplied, not calculated with UE queries.
- No enemy actor spawning, behavior tree, combat, structural damage, persistence, committed event integration, UI, animation or audio.
- No UE 5.8 UHT/build, Editor, PIE or runtime validation performed.

Source scaffolding only. M091–M100 remain unreviewed; do not mark M097 complete.
