# Phase 00 — Branch Integration Matrix

This matrix records where source work lives and the safe integration order. A branch comparison is not a test result.

## Candidate branch families

| Priority | Branch | Relationship/evidence | Decision |
|---|---|---|---|
| 1 | feature/simple-intent | Latest snap work recorded in project memory; placement kernel, contextual resolver, and later fixes. It descends from Phase H and Phase 06. | Preferred implementation baseline after source audit and target-engine validation |
| 2 | mechanic-detailing-pass-h-integrated-world-systems | Stacked source for world systems and Phase 01–06 layers; contains identity, item ownership, terrain, farming, survival, production, storage, threat, structural graph and save types. | Preserve as ancestry/source reference; do not merge wholesale into its descendant |
| 3 | phase-06-inventory-crafting | Initial inventory/crafting layer, ancestor of the broad implementation history | Reference when reviewing item stack/recipe behavior; not a separate merge target |
| 4 | phase-05-stick-primitives, phase-04-stone-masonry, phase-03-wood-processing, phase-02-core-contracts, phase-01-foundation-v2 | Sequential phase commits and closeout documents | Use for provenance, implementation tasks and known limitations; no need for individual cherry-picks if using their descendant |
| 5 | main | Current default branch; small log prototype and three newer memory/documentation commits relative to the older phase baseline | Keep as the public/default reference; integrate docs carefully |

## History checks

| Comparison | Result | Meaning |
|---|---|---|
| phase-01-foundation-v2 → phase-02-core-contracts | Phase 02 is 27 commits ahead, 0 behind | Direct stack |
| phase-02-core-contracts → phase-03-wood-processing | Phase 03 is 6 ahead, 0 behind | Direct stack |
| phase-03-wood-processing → phase-04-stone-masonry | Phase 04 is 7 ahead, 0 behind | Direct stack |
| phase-04-stone-masonry → phase-05-stick-primitives | Phase 05 is 6 ahead, 0 behind | Direct stack |
| phase-05-stick-primitives → phase-06-inventory-crafting | Phase 06 is 6 ahead, 0 behind | Direct stack |
| phase-06-inventory-crafting → feature/simple-intent | feature/simple-intent is 586 ahead, 0 behind | The newer snap branch includes the accumulated Phase 06 history |
| mechanic-detailing-pass-h-integrated-world-systems → feature/simple-intent | feature/simple-intent is 347 ahead, 0 behind | The newer snap branch includes the broad world-system source history |
| main → feature/simple-intent | feature/simple-intent is 696 ahead, 3 behind | The candidate is substantially ahead in implementation but must reconcile the three newer main commits |

## Safe integration sequence

1. Treat feature/simple-intent as the candidate full implementation tree, not as verified product.
2. Compare the latest main commits (memory, decisions, screenshot records) with the candidate branch. Preserve both current code and valid project memory.
3. Run targeted source/diff review on the exact branch head before touching the same public files.
4. Confirm one authoritative placement kernel; no duplicate old snap solver may remain active alongside it.
5. Review the Phase 01–06 closeout limitations and the Phase H source. A component being present is not evidence the mechanic is complete.
6. Create an integration branch for code work only after the source/API review; keep this GDD branch documentation-only.
7. Build the Unreal Engine 5.8 Editor target, verify UHT, open the project, run PIE interactions, then test save/load. Record each separately.
8. Merge to main only after integration gates; do not merge large stacked branch histories based on commit counts alone.

## Do not do

- Do not cherry-pick Phase 01–06 and Phase H onto feature/simple-intent; comparisons indicate that work is already in its ancestry.
- Do not assume 696 commits ahead means 696 completed features.
- Do not claim current main includes source systems that are only present on the candidate branch.
- Do not set any mechanic to VERIFIED without actual corresponding evidence.
