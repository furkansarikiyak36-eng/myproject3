# Decisions

Newest first. Each: the decision, then why.

- **2026-10-28 · VXL-02 selective smooth-voxel policy.** Smooth Marching-Cubes terrain does not imply all areas are diggable. Excavation is gated by a data-driven stratum profile evaluated beneath the original base surface; deeper rock can require a future tool tier, while bedrock/protected layers remain blocked. The profile/policy query is a separate first step from editing density, collision and persistence. Why: avoid infinite excavation and keep geological gameplay understandable and bounded.
- **2026-10-28 · Coding-model work is task-bounded.** A fast model may implement a small scoped task, but must return exact diffs, base SHA and honest test evidence. Review/integration checks the actual changes; Unreal compile/UHT/Editor/PIE/save-load remain separate evidence gates. Why: reduce coding latency without importing guessed APIs, duplicated owners or unverified code.
- **2026-10-28 · One canonical documentation tree.** Keep GDD/catalogues in `Docs/GDD/`, architecture/contracts/registry/schemas in `Docs/Architecture/`, and the phase roadmap in `Docs/Production/`. Do not maintain lower-case duplicate copies of registries or schemas. Why: avoid stale conflicting sources and make the CI-validated catalogue authoritative.
- **2026-10-27 · Phase map uses Phase 00 audit + existing Phase 01–14 stack.** The repository contains an older “16 development phases” mention, but the accumulated implementation branches and architecture roadmap define 14 implementation phases. Keep the mismatch recorded until explicitly resolved; do not renumber M001–M119. Why: avoid duplicate/ambiguous phase ownership.
- **2026-10-27 · feature/simple-intent is the candidate implementation baseline.** Branch comparison shows it descends from Phase 06 inventory/crafting and Phase H world systems; do not merge those ancestors separately. Reconcile the three newer main documentation/memory commits carefully, then validate on UE 5.8. Why: avoid parallel kernels and duplicate code histories.
- **2026-10-27 · Registry status fields must be reported verbatim and with evidence.** The earlier Phase 01 registry revision has all 119 unreviewed/not_run; the current candidate registry revision has mixed source/test fields but all static review fail and all runtime tests not run. Its phase field is 1 for all records, so implementation phase mapping lives separately in the baseline matrix until a reviewed migration. Why: do not overstate mechanic completion from source presence or one shared build result.
- **2026-10-27 · Schema files are design contracts until wired and validated.** Gameplay definition and feedback-event JSON schemas plus the wood processing transition map are authored in the planning branch. JSON parseability was checked; formal JSON Schema validation and runtime catalog loading are still open. Why: define formats before extending C++ while being honest about integration status.
- **2026-10-27 · Initial world systems are depth-bounded.** Voxel terrain is medium-depth (runtime digging, tunnels, compact underground rooms, saved edits, and limited support/collapse rules); raids are simple but meaningful; enemies do not dig through voxels in the first scope. World size and region count remain provisional and data-driven. Why: protect the core survival/building loop from unlimited simulation cost.
- **2026-10-27 · Placeholder-first content production.** Logical asset, VFX, animation, SFX, and music catalogues describe what the game needs, not what is already authored or imported. Start with simple placeholders; final art/audio waits until dimensions, state transitions, and gameplay events stabilize. No marketplace pack is a dependency of the design. Why: keep mechanics and project integration independent of purchased content.
- **2026-10-27 · GDD and production plan are separate from implementation evidence.** GDD files describe intended design; source/content, build logs, automated checks, and Editor/PIE evidence describe actual implementation status. A class or asset existing does not prove the player-visible feature is complete. Why: make the M001–M119 audit and acceptance decisions traceable.
- **2026-10-27 · Shared memory in the repo (memory/).** All assistants read and write it.
  Why: stop re-analysing the whole repo in every chat and every account.
- **2026-10-27 · Reference images are kept as our own text analyses (references/), never as files.**
  Why: a lasting reference without storing other people's material.
- **2026-10-27 · Placement follows the aim; no visible modes.** E places, the axe cuts, R only corrects.
  The player sees a ghost and one line; text appears only when something does not fit.
  Why: the old mode list was too complex compared with Sons of the Forest.
- **2026-10-27 · Diagonals are symmetric.** 45° grid counted from the structure's own line; centred
  on a post; spans centre between two supports; a mirror snaps exactly to the mirrored place.
- **2026-10-26 · One placement kernel.** Material rules live in providers, numbers in
  Config/Placement/PlacementProfiles.json; the kernel never names a material (a test enforces it).
  Why: a new material should only need a provider and profiles.
- **2026-10-26 · Log station map.** One station per complete 75 cm module of the real length,
  End.A at the -axis end, End.B at the +axis end, joints by feature id, never by index.
- **Angles stay in the background.** Angles snap to a 15°/45° grid internally; the player never picks a number.
- **Work style.** Implement the described structure literally and end to end; a wrapper over the old
  system is not done. Say what is verified (build, tests) and what is not (PIE). Commit per working
  step on a feature branch; push only when asked.
