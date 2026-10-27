# Placement Asset Catalog — Source of Truth and Ownership

## Why there are two inventories

`Docs/Architecture/LOG_ASSET_MANIFEST.json` is generated from the FBX inbox. It is useful for source dimensions, naming anomalies and missing FBX-side combinations, but it cannot see Unreal-created or duplicated `.uasset` variants.

`UnrealEditorScripts/BuildUnrealPlacementAssetCatalog.py` inventories loaded StaticMesh assets under `/Game/DemoAssets/Logs`, `/Game/ImportedLogs` and map-related roots. Run it inside Unreal Editor Python; it writes `Saved/Placement/PlacementAssetCatalog.json`. The generated output is deliberately not committed as a claim that it was executed.

## Catalog rules

- File names are hints only. The script does not assign an authoritative length, cross-section or mesh family from a suffix.
- Bounds are measured from a loaded Unreal asset. The longest bounds dimension is reported as an inference, not a guaranteed local longitudinal axis.
- Collision body-shape counts are reported when the asset exposes them.
- Pivot, face orientation, collision correctness and joinery match are still manual/editor acceptance fields.
- Runtime mesh sets must contain exact keys for the supported length/profile tuples. Missing tuple selection fails closed.
- A generated 450 cm `.uasset` is not treated as missing solely because no corresponding FBX was found.
- `SM_RawLog_75` round-base geometry remains an explicit missing-content item until a real asset is measured and assigned, or the relevant profile is disabled.

## Authoring workflow

1. Run the script inside Unreal Editor.
2. Review the generated bounds against the intended log/stone/stick geometry.
3. Explicitly assign nominal family, canonical length, cross-section profile, local axis and source asset where known.
4. Verify pivot, collision and contact face orientation in the editor.
5. Populate `FLogProcessingMeshSet` / corresponding family tables with exact asset references.
6. Only then enable profiles that require those variants.
