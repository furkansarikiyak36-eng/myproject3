# Unreal verification plan

Every claim about a feature names the level it reached. Blind code (written without a compiler, e.g. on
other accounts) is level L0 until Claude Code builds and tests it.

| Level | Meaning | How | Who |
|---|---|---|---|
| L0 | written | code exists in git | any account |
| L1 | compiles | `Build.bat MyProject3Editor Win64 Development ...` (editor closed) | Claude Code |
| L2 | automation tests pass | `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MyProject3; Quit"`; count `Result={Success}` | Claude Code |
| L3 | runs in the real map | headless game run on `/Game/World/World`, log lines checked (e.g. `LogWorldGround`) | Claude Code |
| L4 | seen | `-WorldShots=<folder>` screenshots, frame cost with `-GroundCostFrames=600` | Claude Code |
| L5 | played | the user plays it in PIE and reports | user |

Rules
- Batch blind-code commits, then one L1+L2 pass; fix build errors and wrong tests in that pass.
- A new behaviour gets an automation test (L2) in the same commit.
- Map changes are rebuilt with the commandlet, never hand-edited, so L3/L4 can be repeated.
- A demo row in `MASTER_PLAN.md` section 8 is accepted only at L5.
- Record the level in `SYSTEMS_REGISTER.md` (PIE column for L5) and in `PLAN_STATUS.md` for old task ids.

Per phase (minimum before the next phase starts)

| Phase | L2 | L3/L4 | L5 (user) |
|---|---|---|---|
| M1 backend gaps | contract tests (ground query, volume/mass, contamination) | — | — |
| M2 world greybox | plan-map and placement tests | headless run, screenshots of each region | walk the world, check edges and water |
| M3 core loop | existing + new tests | headless run with mechanics placed | fell, cut, carry, build a shelter, craft, eat, drink, warm up |
| M4 living world | farming and animal tests | one simulated year headless | plant and harvest, keep an animal |
| M5 toxic/climate | hazard and treatment tests | event chain run | survive acid rain with cover/mask |
| M6 threats | raid/colony tests | raid run with logs | defend a wall |
| M7 traversal | rope/bridge tests | screenshots of crossings | cross a gap |
| M8 persistence/perf | save round-trip of every system; annual test | frame cost and memory | save, quit, load |
| M9 demo | all | full run | 4–5 h playthrough against section 8 |

Commands (Windows, from the project folder)
- Rebuild world: `python tools/heightmaps/build_world_heightmap.py`, `python tools/heightmaps/build_world_masks.py`, `UnrealEditor-Cmd MyProject3.uproject -run=BuildValleyWorld -Map=/Game/World/World`
- Screenshots: `UnrealEditor.exe MyProject3.uproject /Game/World/World -game -windowed -ResX=1280 -ResY=720 -WorldShots=<folder>`
- Frame cost: `... -GroundCostFrames=600 -ExecCmds="r.VSync 0,t.MaxFPS 0"` (add `-NoWorldGround` to compare)
