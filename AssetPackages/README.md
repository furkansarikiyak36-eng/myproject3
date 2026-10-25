# AssetPackages — MyProject3

This is the repository's shared asset inbox / inventory. Store incoming archives here, and keep imported/test-ready exports in clearly named subfolders.

## Current contents

- `endgrainv2_height.zip` — supplied end-grain / height-map image archive. `UnrealEditorScripts/ImportLogsToScene.py` can import its images as Texture2D assets. The playable `CreateLogSnapTestLevel.py` script does not process this ZIP or automatically apply the images to materials.
- `UnrealExport/` — FBX static meshes used by the log mechanic prototype.
- `UnrealExport/Archive/` — special notches and joint geometry.

## Log FBX groups

The current FBX set includes:
- Whole-log lengths: 150, 300, 600 and additional short/long variants.
- Prepared shape variants: `Half` (split half-round, not a shorter log), `Quarter`, `Gutter`, `Trough`, `Hewn1/2`, `Kalas`, `Plank`, and flat-notch variants.
- Archive joint parts: cross-lap, half-lap, fork, V-notch, saddle and related mating pieces.

Names are treated as metadata hints, not as a substitute for checking actual mesh dimensions and orientation. The prototype assumes the log's long axis is local X by default; correct this per asset if a mesh was exported on another axis.

## Adding new packages

- Give each archive a descriptive name (for example, `Forest_Props_Pack.zip`).
- Keep the original source/license notes next to the archive when available.
- Before importing, inspect the file list, license, dependencies, scale, orientation, and collision setup.
- Do not silently replace existing assets. Prefer a new versioned package name until the new version has been checked.

## Large files

GitHub's browser upload limit and normal Git's 100 MiB single-file limit can prevent large archives from being committed. Use Git LFS or an external file store for larger packages, and check storage/bandwidth quotas. Uploading an archive here does not by itself import its contents into the Unreal project's `Content/` directory.
