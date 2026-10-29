# Current task (checkpoint)

**Every assistant reads this file FIRST.** Continue the blind-coding closure below.

## Rules
1. Backend/mechanics first; do not spend time on scene polish or P10 physical snap geometry.
2. No per-task Unreal builds or PIE. Author focused automation tests with code, but never claim tests passed unless executed.
3. Use exact declarations from headers; do not guess Unreal API names.
4. New saved fields require restore validation and save-schema migration.
5. Keep each commit/task narrow and record it in `PLAN_STATUS.md`, `ROADMAP.md`, and a session note.
6. Mark source work only `code-written` until the user's batch build/test is run.

## Task

Status: `in-progress`
Goal: Close blind-codeable backend gaps across Phases 1–10, then hand off one batch build/test and later PIE verification. Do not start P11/P12 story/polish/demo work until after the demo foundation exists.
Branch: `work/phase-3-4-closure`
Started: 2026-10-29 · ChatGPT

### Scope
- P0.2 ownership/carry/placement contract (done as docs).
- P1.3 natural-event data, deterministic lifecycle, effects and rollback-safe routing; P1.4 date/event status using existing presentation paths.
- Close blind-codeable portions of P2.1–P2.6, P3.1–P3.4, P4.1–P4.4, P5.1–P5.2, P6.1–P6.4, P7.1–P7.4, P8.1–P8.3 and P9.1–P9.3.
- Reconcile save schema and migration chain; write focused automation tests with each change.
- P10 physical assembly stays frozen until upstream mechanics close; do not add mitres, sockets, cross-joints or geometry follow-ups now.
- P11/P12 are deferred until after the playable demo foundation; no cinematics, polished UI, art or content production in this pass.
- Do not call a task compiled, tested, played or done without actual local evidence.

### Current plan
1. [x] Make placement/carry ownership and transaction rules explicit; P0.2 contract is committed.
2. [code-written] Natural-event runtime is in place: data parsing/validation, deterministic signal/active lifecycle, trigger cooldown, snapshot/rollback participation, weather/condition/crop/structure effects, fire damage to nearby wood/stick nodes, and post-commit flood-route closures. Compile/tests remain unverified.
3. [code-written] Close safe back-end gaps in survival movement, gathering ownership rollback, batch station inventory transfer, material-cost repair, signal-driven raid pressure/targeting, route persistence and save migration.
4. [in-progress] Add/refine focused automation coverage for every changed contract; tests are authored only and must not be reported as run.
5. [in-progress] Reconcile plan/status/session documents and continue on the remaining blind-codeable Phase 1–10 items. Batch Unreal build/automation is deferred to the user's later local pass.

### Findings
- Work branch includes primitive station placement, six gatherable types and charcoal fire-pause flow; scene/PIE state is still unverified.
- P0.2 is documented in `Source/MyProject3/Placement/carry.md`, `Source/MyProject3/Placement/CLAUDE.md` and `memory/modules/carry.md`. P10 geometry remains frozen.
- `Config/GameData/events.json` defines nine events; the loader validates their fields/references; the director runs through shared-clock preflight/rollback. Tests cover event lifecycle, flood-route closure, fire spread, raid signal pressure, plaster and charcoal data contracts, but nothing on this branch has been built or run in Unreal.
- P3.3 now has persistent wall-coating state and shelter-seal ray integration. P4.1 gather grants validate item definitions and roll back partial ledger/inventory changes. P4.3 F3/F4 batch generic station transfers.
- P7.1 applies carried mass, ground slope, weather and snow to player movement. P7.4 flood-prone route edges are blocked by active event claims and the graph is included in save schema v29.
- P8.2 raid pressure/target derives from stored quantity, food location/smell, fire light and fuel-adjusted smoke. P8.3 has material-gated repair and rollback-safe structural fire damage; snow-load/collapse physics remain open.
- No local Unreal build or automation suite has run for the current branch. Historical baseline metrics must not be used as proof for these changes.
- Next: continue blind-code closure from the most self-contained remaining gap; preserve P10 freeze and update the status ledger with only code-written evidence.
