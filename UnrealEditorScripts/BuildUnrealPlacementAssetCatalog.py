"""Build an editor-side catalogue of actual Unreal StaticMesh assets used by placement.

Run inside Unreal Editor Python:
    py "C:/.../MyProject3/UnrealEditorScripts/BuildUnrealPlacementAssetCatalog.py"

Writes Saved/PlacementAssetCatalog.json. This is intentionally separate from the
FBX manifest in Docs/Architecture/LOG_ASSET_MANIFEST.json. Names are hints only;
an author must explicitly assign nominal family/profile and review pivot/axis.
"""
import json
import os
import re
import unreal

ROOTS = ["/Game/DemoAssets/Logs", "/Game/ImportedLogs", "/Game/Maps"]
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "Placement")
OUT_FILE = os.path.join(OUT_DIR, "PlacementAssetCatalog.json")
ASSETS = unreal.EditorAssetLibrary
STATIC_MESH_CLASS = unreal.StaticMesh

def safe_bounds(mesh):
    try:
        box = mesh.get_bounding_box()
        minimum = box.min
        maximum = box.max
        size = {
            "x": round(maximum.x - minimum.x, 3),
            "y": round(maximum.y - minimum.y, 3),
            "z": round(maximum.z - minimum.z, 3),
        }
        extent_min = min(size.values())
        extent_max = max(size.values())
        axis = max(size, key=size.get)
        return {
            "measured_bounds_cm": size,
            "longest_dimension_axis_inferred": axis,
            "axis_is_authoritative": False,
            "shortest_dimension_cm": extent_min,
            "longest_dimension_cm": extent_max,
            "bounds_valid": extent_max > 0.0,
        }
    except Exception as error:  # noqa: BLE001
        return {"bounds_valid": False, "measurement_error": str(error)}

def collision_summary(mesh):
    try:
        body_setup = mesh.get_editor_property("body_setup")
        agg = body_setup.get_editor_property("agg_geom")
        return {
            "collision_available": True,
            "box_count": len(agg.box_elems),
            "sphere_count": len(agg.sphere_elems),
            "capsule_count": len(agg.sphyl_elems),
            "convex_count": len(agg.convex_elems),
        }
    except Exception as error:  # noqa: BLE001
        return {"collision_available": False, "collision_error": str(error)}

def likely_family(name):
    # Hint only. Never assign a canonical length/profile based on a name.
    if re.match(r"^SM_RawLog_\d+(?:_|$)", name):
        return "wood_log_name_hint"
    if "Long_" in name or "Short_" in name:
        return "unmapped_wood_name_hint"
    if "Joinery" in name or "Notch" in name or "Saddle" in name or "CrossLap" in name:
        return "joinery_name_hint"
    return "unassigned"

records = []
seen = set()
for root in ROOTS:
    try:
        paths = ASSETS.list_assets(root, recursive=True, include_folder=False)
    except Exception as error:  # noqa: BLE001
        unreal.log_warning("PLACEMENT_CATALOG cannot list %s: %s" % (root, error))
        continue
    for asset_path in paths:
        if asset_path in seen:
            continue
        seen.add(asset_path)
        asset = ASSETS.load_asset(asset_path)
        if not asset or not isinstance(asset, STATIC_MESH):
            continue
        name = asset.get_name()
        item = {
            "asset_path": asset_path,
            "asset_name": name,
            "source_scope": root,
            "family_assignment": "requires_manual_assignment",
            "family_name_hint": likely_family(name),
            "nominal_length_cm": None,
            "cross_section_profile": None,
            "source_fbx": None,
            "measurement_source": "loaded_unreal_static_mesh_bounds",
            "pivot_review": "not_verified_by_script",
            "orientation_review": "not_verified_by_script",
            "geometry_variant_review": "not_verified_by_script",
            "collision": collision_summary(asset),
        }
        item.update(safe_bounds(asset))
        records.append(item)

records.sort(key=lambda row: row["asset_path"])
os.makedirs(OUT_DIR, exist_ok=True)
payload = {
    "schema_version": "1.0.0",
    "generated_by": "BuildUnrealPlacementAssetCatalog.py",
    "generated_in_editor": True,
    "source_roots": ROOTS,
    "counts": {
        "static_meshes": len(records),
        "invalid_bounds": sum(1 for row in records if not row.get("bounds_valid", False)),
        "collision_missing": sum(1 for row in records if not row.get("collision", {}).get("collision_available", False)),
        "requires_manual_family_assignment": len(records),
    },
    "rules": [
        "Asset names are hints, not canonical geometry metadata.",
        "Measured bounds do not validate pivot, local axis, cut faces or visual/collision correctness.",
        "Every supported geometry tuple must be configured explicitly in the relevant mesh variant set.",
        "A generated .uasset can be present even when the FBX source manifest does not list it."
    ],
    "assets": records,
}
with open(OUT_FILE, "w", encoding="utf-8") as handle:
    json.dump(payload, handle, indent=2, ensure_ascii=False)
    handle.write("\n")
unreal.log("PLACEMENT_CATALOG wrote %s (%d StaticMesh assets)" % (OUT_FILE, len(records)))
