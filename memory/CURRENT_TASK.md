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

Status: `open`
Goal: Finish every "Kısmi" task of `memory/PLAN_STATUS.md` (27), backend + player input where the task says so.
Asked by the user: "kısmileri bitirelim" (2026-10-29)
Started: 2026-10-29 · Last saved: 2026-10-29 · Branch: `game/p00-adopt-group-a` (from origin/group-a-code-closure + merge of feature/simple-intent)

### Plan (one commit per step; tick when tested)
0. [ ] Adopt group-a: new branch, merge simple-intent, fix 10 UHT + 6 C++ errors, run all tests, fix failures.
1. [ ] P1.1 calendar (day/month/season/year from clock, days per month 5)
2. [ ] P1.2 monthly climate, snow depth, ground wetness
3. [ ] P2.1 water items (dirty/boiled), P2.2 sleep place, P2.4 spoilage modifiers, P2.5 condition triggers
4. [ ] P3.1 wet wood + charcoal fuel, P3.2 enclosure from graph
5. [ ] P4.1-P4.4 gatherables, recipes, stations, tool requirements (needs data files P0.3/P0.4 first)
6. [ ] P5.1 layer data
7. [ ] P6.2-P6.4 field steps, seeds, animal chain
8. [ ] P7.1-P7.4 player route cost, zones, reward placement, infrastructure
9. [ ] P8.2-P8.3 raid signals, snow load/avalanche/fire, repair
10. [ ] P9.2 helper uses stations; P10 thatch/notches (frozen: skip); save gaps
Order may change; record why here.

### Read
- —

### Findings
- —

### Changes
- —

### Next step
Step 0: git switch -c game/p00-adopt-group-a origin/group-a-code-closure; merge feature/simple-intent; build.
