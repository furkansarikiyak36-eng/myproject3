# Current task (checkpoint)

**Every assistant reads this file FIRST.** If the status below is `open`, continue from
"Next step". Do **not** analyse again what "Findings" already says.

## Rules (the limit can end the session at any moment)

1. New task from the user: replace the section below (status `open`), write the goal and plan,
   save the file **before** reading any code.
2. After **every** step (one file read, one search, one edit): add 1–3 lines to "Findings" or
   "Changes", update "Next step", save. Small and often; never keep results only in the chat.
3. Read only what `memory/CODEBASE.md` points to. Write down in "Read" each file you opened, so
   nobody opens it again for the same question.
4. Finished: status `done`; move lasting facts to `CODEBASE.md` (Findings) and the session file;
   clear this file back to the empty template at the next new task.
5. Cannot write to the repo (read-only GitHub connector)? End **every** answer with the updated
   block of this file so the user can paste or commit it.

---

## Task

Status: `done`
Goal: Audit every claim in memory/ + the game plan against the code before phase 0 starts; fix wrong facts.
Asked by the user: "planı üretmeye başlamadan önce tüm dosyaları analiz et, doğru bilinen yanlış var mı; Unreal açmadan kurulur mu"
Started: 2026-10-29 · Last saved: 2026-10-29 · Branch: feature/simple-intent

### Plan
1. List checkable claims in CLAUDE.md, STATE.md, CODEBASE.md, ROADMAP.md, DECISIONS.md, plan doc.
2. Verify each with grep/small reads (no full-file reads).
3. Fix wrong facts, record findings here and in CODEBASE.md.
4. Answer: can phases run without opening Unreal.

### Read (files already opened, with the part)
- —

### Findings
- Tests: 48 core + 20 placement = 68. CLAUDE.md/CODEBASE claim correct.
- main is NOT simply behind: origin/main has 3 commits not in simple-intent (65faad8, 9ab58a4 memory; 8491e96 SOTF acceptance gates doc); simple-intent has 700 not in main.
- BIG: remote has a newer branch chain by another assistant (2026-10-28/29): phase-03..phase-14 *-sequential, group-a-code-closure, fix/vertical-slice-b-core-loop. Memory/plan describe only simple-intent. Must check what code they add.
- group-a-code-closure: based on 321e55a (our pre-plan commit), +182 commits by ChatGPT (author furkansarikiyak36-eng), 45 files, +6.6k lines, last 2026-10-29. Adds: fuel items consumed, item freshness (spoilage time), item-bound food + rest, survival conditions/treatment, equipment slots + clothing insulation + water protection, gated discovery rewards, region access by equipment, excavation -> owned resource items -> production stock, item-backed farming/feeding/harvest, terrain-aware route costs, raid damage pulses, voxel save, TerrainDigToolComponent, save schema up to v19. So CODEBASE.md/plan 'missing' claims are wrong for that branch.
- group-a status (its own docs): NEVER compiled (no UHT/UBT), tests authored not run, no PIE.
- Three separate code lines, no shared commits (subject overlap 0): (1) simple-intent = placement kernel; (2) group-a-code-closure = simple-intent@321e55a + 182 unbuilt commits; (3) fix/vertical-slice-b-core-loop (base 0bd92c9, +342, has RaidPlaceholderPawn/WorldRaidTargetComponent, NO Placement kernel, 17k lines differ) and phase-03..14 chain (base 2610099 Oct 25, +107, has the gameplay vision doc the user pasted). (3) cannot merge with (1) cheaply.
- origin/main has 3 commits not in simple-intent (memory + SOTF acceptance doc).
- Our branch, verified: HeatSource fuel = seconds (AddFuel(BurnSeconds)); a carried log put on a fire burns (adds seconds). ShelterSensor = line traces, not the graph. RaidAdapter uses the structural graph (damage per pulse). Terrain decision doc still 'awaiting owner approval'. KernelNamesNoMaterial test exists. Survival profile has Thirst, Stamina, Fatigue, ClothingInsulation; thirst relief only via ConsumeFood.
- WRONG in CODEBASE/plan: rope bridges (level rope carries boards) and zipline (sloped rope slides logs) EXIST (RopeLineActor); Floor mode builds bridges/ramps; VerticalSlice_C already has a creek with a far ford, old rope bridge, cliff terrace with stair stringers, lookout with zipline = proto exploration barriers. Weather is driven by the C level (night snow / day wind), not only set from outside.

### Changes (files edited, commits)
- group-a build #2 (scratch patch of UHT lines): 6 C++ errors (VoxelTerrainChunkComponent.cpp:184,369 C2064; WorldSurvivalSubsystem.cpp:924,946,1016 C2039 ExpectedInventoryOwnerId; :943 C4390). Worktree removed.
- Fixed CODEBASE.md (branch table, corrections, build findings), STATE.md, ROADMAP.md (P0.0, Q00), plan doc note.

### Next step
None. Waiting for the user: Q00 (fix and adopt group-a?), Q0 (merge to main?).
