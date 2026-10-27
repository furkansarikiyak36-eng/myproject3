# Player hands (carry, place, cut)

Files: `Source/MyProject3/LogCarryComponent.*` (3000+ lines: grep, do not read whole), HUD in `LogSnapHUD.*`.

- `ResolveContext(FLogContextAim)` decides what the held log does from the aim
  (`ELogContextAction`: Snap, Stack, Deck, Strut, Ground, Post); `UpdateContextPlacement` runs it a few
  times a second; `ApplyContextResult` shows the kernel's solution; `PlaceHeldPiece` commits it.
- Manual modes only after R (`bManualPlacementMode`).
- Cuts: `ResolveCutChoice` picks the cut from the aimed point (length cut, recipe point, free 25 cm step,
  post shortening, sharpening, end face → lengthwise split).
- Debug: console command `mp3.SnapGallery [quit]` (`Source/MyProject3/Debug/SnapGalleryCommand.cpp`)
  builds every placement kind next to the player and saves screenshots to `Saved/Screenshots/SnapGallery`.
