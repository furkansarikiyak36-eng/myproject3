# Phase 8 — Acceptance Checklist

- [x] Define explicit terrain layer types and chunk/cell keys.
- [x] Define finite excavation depth and volume limits.
- [x] Reject edits to protected or unconfigured layers.
- [x] Reject duplicate cell registration and duplicate excavation.
- [x] Track per-cell revision and total excavated volume in the in-memory ledger.
- [ ] Connect ledger cells to a selected UE 5.8 editable terrain implementation.
- [ ] Keep geometry and collision updates atomic with terrain-state commits.
- [ ] Implement bounded pit, horizontal tunnel and room geometry.
- [ ] Integrate underground support diagnostics with Phase 7 structural graph.
- [ ] Implement durable save/load and schema migration.
- [ ] Implement water flow, soil/runoff, fire/rot and exposure simulation.
- [ ] Implement persisted ruin/clue discovery integration.
- [ ] Audit Blueprint callers and data configuration.
- [ ] Run Unreal Engine 5.8 UHT/build, Editor and PIE at the agreed final gate.

**Status:** Initial source foundation only. M071–M080 remain open; no Unreal tests were run.