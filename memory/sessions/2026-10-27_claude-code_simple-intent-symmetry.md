# 2026-10-27 · claude-code · simple intent, symmetry, bent joints
Branch: feature/simple-intent · Commits: 41d8480, 7de76ff, eaf6d8d

## Done
- Contextual placement without modes; HUD shows only the ghost and the cut line; text only on problems.
- Axe on a loose log's end face splits it lengthwise (test in MyProject3CoreTests).
- Symmetric diagonals: post top centred on a 45° grid from the structure's line; post-to-post spans
  centred; wall diagonal + exact mirror; corner to corner on a frame; mirrored struts
  (test `Placement.DiagonalsAreSymmetric`).
- End to end bends 45/90/135 (test `Placement.EndJointsBendIntoCorners`).
- 68/68 automation tests pass.
- Measured authorship (git blame, current Source): ~54% lines from Claude commits, ~46% from the
  user's account; Placement kernel and tests almost all Claude.

## Found
- The user says the bent joint does not look right in the scene. Not yet seen in PIE.

## Open
- Build and run `mp3.SnapGallery` in VerticalSlice_C, look at the screenshots, fix what is wrong.
- Mitre cuts (45° ends) do not exist.
