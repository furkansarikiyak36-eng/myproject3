# Decisions

Newest first. Each: the decision, then why.

- **2026-10-28 · Terrain for the first playable slice is hybrid.** Native Landscape owns the macro outdoor surface; bounded smooth voxel chunks own designated dig pockets. Use UE `FMarchingCubes` + `UProceduralMeshComponent`; no third-party voxel dependency by default. **This is an architecture choice, not gameplay acceptance.** The measured 32³ synchronous rebuild cost is above a 16.7 ms frame budget, so async mesh/collision, chunk seams, navigation and Landscape transitions remain release gates.
- **2026-10-28 · Bounded excavation must fail closed.** Radius/region, material tool-strength threshold, expected density revision, unique operation receipt and grounded-structure clearance are checked before the edit. SaveGame v14 stores voxel density and receipts by stable actor identity. Physical voxel state and logical terrain-cell state are not yet one atomic transaction; do not claim that they are integrated until that gap is closed.
- **2026-10-27 · Shared memory in the repo (`memory/`).** All assistants read and write it.
  Why: stop re-analysing the whole repo in every chat and every account.
- **2026-10-27 · Reference images are kept as our own text analyses (`references/`), never as files.**
  Why: a lasting reference without storing other people's material.
- **2026-10-27 · Placement follows the aim; no visible modes.** E places, the axe cuts, R only corrects.
  The player sees a ghost and one line; text appears only when something does not fit.
  Why: the old mode list was too complex compared with Sons of the Forest.
- **2026-10-27 · Diagonals are symmetric.** 45° grid counted from the structure's own line; centred
  on a post; spans centre between two supports; a mirror snaps exactly to the mirrored place.
- **2026-10-26 · One placement kernel.** Material rules live in providers, numbers in
  `Config/Placement/PlacementProfiles.json`; the kernel never names a material (a test enforces it).
  Why: a new material should only need a provider and profiles.
- **2026-10-26 · Log station map.** One station per complete 75 cm module of the real length,
  End.A at the −axis end, End.B at the +axis end, joints by feature id, never by index.
- **Angles stay in the background.** Angles snap to a 15°/45° grid internally; the player never picks a number.
- **Work style.** Implement the described structure literally and end to end; a wrapper over the old
  system is not done. Say what is verified (build, tests) and what is not (PIE). Commit per working
  step on a feature branch; push only when asked.
