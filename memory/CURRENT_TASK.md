# Current task (checkpoint)

**Read this file first.** Status: open. Continue from “Next step”.

## Rules
1. Keep work on `game/p00-adopt-group-a`; do not modify `main`.
2. Backend/mechanics first. Do not place scene actors or alter physical snap geometry.
3. P10 physical assembly is frozen.
4. Write focused automation-test source with behaviour changes; never claim it passed unless executed.
5. Keep the result at `code-written` if Unreal build/test is unavailable. No blind guesses about Unreal APIs; inspect headers first.
6. Update `memory/PLAN_STATUS.md`, `memory/ROADMAP.md`, `memory/STATE.md`, and a dated session note as implementation advances.

## Task

Status: `open`
Goal: Close phases 1-9 in order (each phase: every task Var in PLAN_STATUS, builds, all tests pass; PIE comes with the scene).
Asked by the user: "9 faz da bitirerek ilerleme zamanı geldi sırayla kapatalım fazları" (2026-10-30)
Started: 2026-10-30 · Branch: `game/p00-adopt-group-a` (single working branch)

### Plan
0. [x] Merged `work/phase-3-4-closure` into p00 (integration `15208d3`, 126/126). Its event director (752 lines, real effects: fire spread, flood
       route closures, raids, repair) replaces p00's (318 lines). Keep p00-only work: WorldHudDataSubsystem (adapt to
       the phase-3-4 director API), geology, action costs, charcoal pause, build fixes. One save schema line.
       Then build + all tests.
1. [ ] Phase 0 leftovers (P0.2 freeze note, P0.5 old demo recipe).
2. [ ] Phase 1 ... 9: per phase read PLAN_STATUS rows, close gaps, build + test, mark Var, commit.
Side lines p06/p07 hold a second farming API: do not merge; port only data files (zones, routes, discoveries,
animals) if phase 6/7 needs them.

### Read
- p00 vs phase-3-4: both from `ea00757`; p00 +12 (events v1, HUD, geology, costs, charcoal), phase-3-4 +179.

### Findings
- Two independent event directors. phase-3-4: AdvanceEventStep/InitializeEventDirector; p00: AdvanceToTime/InitializeDirector.

### Changes
- integrate/phase-3-4 = phase-3-4 + p00 geology/costs/build fixes + HUD port; 4 build fixes, 3 test fixes; 126/126.
- p00 merge commit: tree = integration, memory notes kept from p00.

### Next step
Step 1: Phase 0 leftovers, then Phase 1. Read memory/PLAN_STATUS.md rows P0.x and P1.x first.
