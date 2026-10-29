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
0. [x] Adopt group-a (250ce13, 85/85): new branch, merge simple-intent, fix 10 UHT + 6 C++ errors, run all tests, fix failures.
1. [x] P1.1 calendar (88/88) (day/month/season/year from clock, days per month 5)
2. [x] P1.2 monthly climate, snow depth, ground wetness (91/91)
3. [x] P2.1/P2.5 consume conditions (92), P2.2 rest quality (93), P2.4 spoilage by storage+temperature (94)
4. [~] P3.1 fuel heat (wet wood/charcoal) done 95/95; P3.2 enclosure from graph next
5. [ ] P4.1-P4.4 gatherables, recipes, stations, tool requirements (needs data files P0.3/P0.4 first)
6. [ ] P5.1 layer data
7. [ ] P6.2-P6.4 field steps, seeds, animal chain
8. [ ] P7.1-P7.4 player route cost, zones, reward placement, infrastructure
9. [ ] P8.2-P8.3 raid signals, snow load/avalanche/fire, repair
10. [ ] P9.2 helper uses stations; P10 thatch/notches (frozen: skip); save gaps
Order may change; record why here.
User 2026-10-29: "p4-yap p10-atla" -> do P0.3/P0.4 data files as part of P4; skip P10 (frozen).

### Read
- —

### Findings
- Wetness already exists in the survival profile (rain x shelter x clothing, drying); PLAN_STATUS corrected (P2.3 Yok -> Kısmi).
- VerticalSlice_C keeps its own Hours and never calls the coordinator: clock, survival needs, fuel do not advance in the main level (only UpdateLocation + day/night SetWeather). Only WorldDemoBootstrapActor drives AdvanceFixedStep. -> player-wiring task (P1.4).
- —

### Changes
- Branch game/p00-adopt-group-a created, merged simple-intent (8dc514c, DECISIONS conflict kept both).
- Fixes: survival header private config lost BlueprintReadWrite; FBox IsValid() -> IsValid (field) x2; receipt ExpectedInventoryOwnerId -> ExpectedItemOwnerId (3 sites). Build: Succeeded.
- Tests: 85 total, 81 pass, 4 fail: Survival.Body.ColdCoolsCoreAndBurnsCalories (line 680 'Advance one hour' false) and Survival.Heat.RainDrownsOpenFireButNotHearth (736-742) = OLD tests broken by group-a; Terrain.Voxel.BoundedDigChecksMaterialToolAndRevision (866 collision not published) and Terrain.Voxel.DigRejectsNearGroundedStructure (947-949 dig not refused) = new group-a tests.
- Cause 1-2: group-a made AdvanceSurvivalStep take the END time (coordinator passes TargetTime); old tests passed start time -> tests fixed. Cause 4: test spawns bare AActor without root, SetActorLocation ignored -> test fixed. Cause 3: trace exactly through pit-centre vertex (radius 100, voxel 50) -> trace offset; to confirm.

### Next step
Step 4b: P3.2 shelter enclosure from the structural graph. Read ShelterSensorComponent.cpp (line traces) and WorldStructuralGraphSubsystem API (GetAllDependents, node kinds). Idea: sensor traces only count hits on actors registered in the structural graph that are ground-supported (a real built roof/wall, not a tree); per-direction gaps lower protection.
