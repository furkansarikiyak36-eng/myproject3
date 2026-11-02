# Framework analysis of the master plan (2026-11-02)

Purpose: look at the whole plan (`MASTER_PLAN.md`) as one design and find what holds, what pulls against
each other and what may change. Nothing here is decided; proposals become decisions only when the user says so
(new questions D18–D24 at the end, also in `OPEN_DECISIONS.md`).

## 1. Where we stand

120 requirements in `SYSTEMS_REGISTER.md`: 34 Built, 48 Partial, 26 Missing, 7 Designed only, 4 data only,
1 deferred. Nothing has been played on the World map. The backend is broad (survival, fire/shelter, production,
farming cells, events, structure, save v31) but almost every system lacks its place in the world, its player
controls and its content. The biggest single gap is not code but **content**: animals, mutants, crops,
models, animations.

## 2. What holds together

- One loop runs through the pillars: **toxic world → make safe water/soil/shelter → grow and keep → the world
  recovers → threats grow with your footprint**. Survival, farming, ecology and raids all feed it.
- The physical building (logs, snap, structural graph) is the strongest part and already connects to fire,
  shelter, damage and raids.
- The data-driven backbone (game data, ownership ledger, fixed-step clock, save coordinator) fits every
  planned system; new systems mostly add data and rules, not new foundations.
- Signal-driven raids are a good fit: the more you build and produce, the more you are noticed. That ties the
  threat pillar to the building pillar without fixed horde nights.

## 3. Tensions and contradictions

| # | Tension | Why it matters | Proposal |
|---|---|---|---|
| 1 | **Space-craft premise vs primitive tools.** A character who arrives in a spacecraft builds with stone and wood. | Players will ask why no technology helps. | The craft is wrecked and powerless; only a few salvage items (knife, cloth, a filter); technology is the story's goal, not a tool. (D20) |
| 2 | **"Lifeless toxic world" vs farming and animals inside a 4–5 h demo.** | The restoration arc (life returns) takes long; the demo needs crops and animals early. | Start with surviving pockets (a sheltered valley, a few seeds, wild rabbits/deer at the edges); domestic animals come later in the arc or are found. (D22, D23) |
| 3 | **Chemical contamination vs pre-metal era.** Boiling does not remove chemicals; heavy gas protection is out of the era (D14). | Without a primitive answer, toxic water/soil become dead ends. | Primitive answers that work partly: charcoal and sand/gravel filters (reduce), clay vessels, rainwater collection under cover, distillation (removes non-volatile chemicals), clean-soil beds; heavy gas zones are places to avoid, not gates in the demo. |
| 4 | **Claim-based digging (D8) with 750 m cells (D10).** Centre + 4 neighbours = 5 x 0.56 km² = **2.8 km², ~35 % of the 8 km² playable area**, spanning 2.25 km. | Pre-building voxels for that area is impossible (~11,000 columns, several GB); digging must stay "turn into voxel when dug", so the first-dig hitch (42 ms) and start-up fixes are still needed. Also: is digging then limited at all? | Confirm the reading; if the intent is a smaller dig zone, use the claim for ownership and a smaller dig cell inside it (e.g. 150 m) for voxels. (D18) |
| 5 | **Resources found by digging outside the claim** (clay pits, sand banks, gravel). | With voxel digging only in the claim, these must come from somewhere. | Outside the claim: surface gathering of bulk materials (a clay bank gives clay by volume, no terrain change). (D19) |
| 6 | **Switchable first/third person (D7).** | Every aim-based mechanic (aim cutting, snap preview, dig, carry) was built for a first-person camera; the template character has no axe, carry or build animations. | One "aim provider" that both cameras use (camera trace in first person, character-forward + camera trace in third person); animations come from content work; third person can start without them. |
| 7 | **Two item models: physical pieces and bulk volume.** | Ownership ledger, inventory, storage, production and save all count units today. Adding litres + density touches all of them. | Do it first in M1 as one contract with tests, before more content is added on the unit model. |
| 8 | **Simulation breadth.** Climate chains, ecology, colonies, farming and an annual cycle all run at once. | Cost grows with the world; testing a year by playing is impossible. | Simulation LOD (Q5) and headless annual runs (Q7) move from M8 to M1/M4. |
| 9 | **Story last vs premise in the world.** | The crash site, old facilities and colony locations are geography; if geography is fixed first, they may not fit. | Reserve their places when geographic definitions are made (M2), fill them last. (D21) |
| 10 | **Two code lines.** `game/p06-p08-blind-code-closure` has ~11.9k lines (wildfire, farm processing, raid signals) on an old base. | Every week apart makes the merge harder; some of it duplicates our event system. | Port the unique parts in M1, then archive the branch. |
| 11 | **Not in the plan at all**: asset sourcing (models, animations, sounds), audio, UI/UX design, Steam/early-access work, player testing, localisation (Turkish/English). | These decide the release more than any mechanic. | Add a "Production" section; first question: where do models and animations come from (Fab/Marketplace, purchased packs, own work)? (D24) |

## 4. What may change in the plan

1. **Phase order**: M1 gets four contracts before anything else: ground query, volume/mass, contamination,
   aim provider (for D7). Simulation LOD and the annual test move to M1/M4. Claims (D10) and their dig rule move
   into M2 with the geography.
2. **Demo depth**: keep all 16 demo rows but mark each as *must* (core loop: terrain, logs, shelter, fire,
   survival, resources, production, persistence), *should* (farming, animals, toxic hazard, crossing) or *could*
   (raid, mutant encounter, digging). If time runs short, *could* rows show one simple case.
3. **Toxic world becomes the restoration engine**: contamination on water and soil is what farming,
   restoration and survival act on, so it is built together with farming (M4/M5), not as a separate layer.
4. **Content plan**: a list of every model/animation the demo needs per row, with its source (D24).

## 5. New questions

| Id | Question | Recommendation |
|---|---|---|
| D18 | Claim digging: is the voxel dig area really the whole 5-cell cross (~2.8 km²), or should a claim contain smaller dig cells? How many claims can the player have? | one claim area; digging anywhere inside it, voxels made only where dug |
| D19 | Outside the claim, do clay/sand/gravel come from surface gathering (no terrain change)? | yes |
| D20 | Premise: the craft is wrecked and powerless, only a few salvage items? | yes |
| D21 | Reserve places for crash site, facilities and colonies during geography (M2)? | yes |
| D22 | Demo start: surviving life pockets (some seeds, a few wild animals) rather than fully lifeless? | yes |
| D23 | Where do domestic animals come from in the demo? | found later in the arc; not in the first demo hour |
| D24 | Asset sourcing for models, animations, sounds | open — user decides |
