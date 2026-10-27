# Snap gallery · 2026-10-27

Our own screenshots from the game (VerticalSlice_C, console command `mp3.SnapGallery`,
commit a7c90d2). Every case goes through the player's path: hold a log, look, see the
preview, press E. `_1_preview` is the ghost before placing, `_2_placed` the result from above.
Built on a flat platform above the level; 600 cm round logs.

| Case | What it shows | Result |
|---|---|---|
| `bend_+45`, `bend_+90`, `bend_+135`, `bend_-90` | Looking down at the last cm of a lying log, carried at that angle: end to end bend. | Correct (L and < corners close at the end). |
| `post_45` | Log across a post top, carried at 45°. | Correct: centred on the post. |
| `post_span` | Two posts on a diagonal, log carried toward the second. | Correct: rests on both, centred. |
| `wall_diagonal`, `wall_mirror` | A diagonal on a wall at a station, then its mirror. | Symmetric, but the two 6 m logs clip into each other where they cross. |
| `frame_corner` | Two parallel beams, from an end station to the other beam's corner. | Correct. |
| `strut_a`, `strut_b` | A strut on each side of a post under a beam. | Correct; the long loose logs beside are the off-cuts. |

The axe and hand in the corner are the player's first-person view.
