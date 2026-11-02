# Open decisions register

Nothing here is decided. Each row has the options and Claude's recommendation; the user decides.
When decided: move the row to `DECISIONS.md` with the date and update `MASTER_PLAN.md`.
Asked on 2026-11-02 (D1–D12); D13+ come from the user's plan text.

| Id | Question | Options | Recommendation | Blocks |
|---|---|---|---|---|
| D1 | World size | A) keep 4x4 km (~8 km² playable) · B) grow to 5x5 km | A: the playable target (5–8 km²) is met; 5x5 doubles the map file (~220 MB) and needs World Partition | T1, T3 |
| D2 | When World Partition | now · when the world grows | when it grows; the current single map loads fine | T3 |
| D3 | Looks order | textures before models (user's list, plan text) · models before textures (old WORLD_PLAN) | textures first: they come from the ground id map and do not depend on models | M10/M11 |
| D4 | Physical assembly in the demo | existing log cut/carry/snap/graph in the demo, advanced joinery (notches, mitre, crossings) last · everything early | the first | A11, M3 |
| D5 | Material targets | 9 groups / 38 transformations (old decision) · 8–10 families, 35–45 raw, 20–30 processed, 8–12 special (plan) | plan ranges as the ceiling, 9/38 for the demo | R6 |
| D6 | Story timing | story premise in the design now, development last · story work earlier | the first (keeps "world first, story last") | E2, E3 |
| D7 | Character | first person (log mechanics are built for it) · third person (template now on the World map) · switchable | open; first person keeps every built mechanic working without rework | M2, all player input |
| D8 | Digging scope | A) every soil place (built, frozen) · B) only designated dig areas (field logic) · C) deep in areas, shallow elsewhere | B or C: bounded cost, no first-dig hitch, design control | T7–T13 |
| D9 | Mutant intelligence | needs-driven only (food, colony) · advanced types learn the player · both | both (plan's proposal), learning limited to advanced types | M6, M8 |
| D10 | Claim cells (250 m) in a single-player game | base boundary · raid target area · multiplayer preparation · drop | base boundary + raid target | T5, M5 |
| D11 | Other human groups | in the first demo · later | later | E4 |
| D12 | Where the plan lives | `memory/MASTER_PLAN.md` + linked registers (done 2026-11-02) · something else | as done | — |
| D13 | Multiplayer | single player only · co-op later | not discussed; plan assumes single player | Q1, T5 |
| D14 | Gas protection beyond cloth/charcoal | out of the primitive era · a late primitive solution | out of the base era (plan text says "probably later technology") | X7 |
| D15 | Rare mutant animals | include as rare event · leave out | plan says rare and separate; keep out of the demo | M3 |
| D16 | Production automation | none · limited (water wheel, conveyors of the era) | limited, later | P5 |
| D17 | Terrain source naming | "Terrascultor 2.0" in the plan = TerreSculptor exports we used? | confirm | T2 |
