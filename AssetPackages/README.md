# AssetPackages — MyProject3

This is the repository's shared asset inbox / inventory. Store incoming archives here, and keep imported/test-ready exports in clearly named subfolders.

## Current contents

- `endgrainv2_height.zip` — supplied end-grain / height-map image archive. `UnrealEditorScripts/ImportLogsToScene.py` can import its images as Texture2D assets. The playable `CreateLogSnapTestLevel.py` script does not process this ZIP or automatically apply the images to materials.
- `UnrealExport/` — FBX static meshes used by the log mechanic prototype.
- `UnrealExport/Archive/` — special notches and joint geometry.

## Log FBX groups (filename-derived inventory)

The current `UnrealExport/` directory contains **48 root FBX meshes and 10 Archive/joinery FBX meshes**.

- Nominal size groups `75`, `150`, `300`, and `600` have nine files each, except the 75 group has no `SM_RawLog_75.fbx` base-round mesh (it has eight shaped outputs plus `FlatNotch_TopBottom`).
- Base round meshes are present for `SM_RawLog_150.fbx`, `SM_RawLog_300.fbx`, and `SM_RawLog_600.fbx`.
- The `Half` filename is documented as a split half-round, **not** a shorter log.
- `Quarter_04` is catalogued as a likely quarter-round cross-section output from the package naming, but its actual geometry/pivot still needs visual import verification. It must not be assumed to mean a quarter-length log.
- `Gutter`, `Trough`, `Hewn1`, `Hewn2`, `Kalas`, and `Plank` are shape-processing variants. Match an output to a base nominal size only when the corresponding asset exists.
- `SM_RawLog_Long_01..08` and `SM_RawLog_Short_01..04` have ambiguous names and need dimensions/shape metadata from the imported meshes.
- `Archive/` contains ten joinery pieces: CrossLap lower/upper, Notch Fork/HalfLap/Saddle/V, and SaddleA/SaddleB mating parts.

This is a **filename inventory**, not a geometric audit. The C++ `FLogProcessingMeshSet` now provides an explicit length-fraction × cross-section mesh map, but the actual imported mesh references and transform offsets still must be assigned in Blueprint. The importer script groups/imports FBXs into `/Game/ImportedLogs` and creates a staging gallery, but it does not configure `ALogBuildPiece` Blueprint variant fields, collision equivalence, center offsets or output rotations. `AssetPackages/` is not the Unreal `Content/` folder; the repository does not currently include a configured production Blueprint/material setup for each variant.

Names remain metadata hints, not a substitute for checking actual mesh dimensions, pivot, local axes and orientation. The prototype assumes the long axis is local X by default; correct this per asset if a mesh was exported on another axis.

## Adding new packages

- Give each archive a descriptive name (for example, `Forest_Props_Pack.zip`).
- Keep the original source/license notes next to the archive when available.
- Before importing, inspect the file list, license, dependencies, scale, orientation, and collision setup.
- Do not silently replace existing assets. Prefer a new versioned package name until the new version has been checked.

## Large files

GitHub's browser upload limit and normal Git's 100 MiB single-file limit can prevent large archives from being committed. Use Git LFS or an external file store for larger packages, and check storage/bandwidth quotas. Uploading an archive here does not by itself import its contents into the Unreal project's `Content/` directory.
