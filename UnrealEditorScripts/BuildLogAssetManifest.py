# BuildLogAssetManifest.py
# Unreal Engine 5.8 Editor Python script (SOTF-M05).
#
# Imports every log FBX under AssetPackages/UnrealExport (Archive excluded),
# measures each static mesh, maps it to the canonical log geometry
# (length fraction + cross-section profile) and writes a manifest. A measured
# length that deviates from the filename by more than the tolerance, or a
# required geometry combination without a mesh, is reported as an error.
#
# Run headless:
#   UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript
#       -script="<Project>/UnrealEditorScripts/BuildLogAssetManifest.py"

import json
import os
import re

import unreal

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FBX_ROOT = os.path.join(PROJECT_ROOT, "AssetPackages", "UnrealExport")
ASSET_DIR = "/Game/DemoAssets/Logs"
MANIFEST_JSON = os.path.join(PROJECT_ROOT, "Docs", "Architecture", "LOG_ASSET_MANIFEST.json")
MANIFEST_MD = os.path.join(PROJECT_ROOT, "Docs", "Architecture", "LOG_ASSET_MANIFEST.md")
LENGTH_TOLERANCE = 0.02
FULL_LENGTH_CM = 600

# Nominal family length (cm) relative to the 600 cm full log.
LENGTH_FRACTIONS = {600: "FullLength", 450: "ThreeQuarterLength", 300: "HalfLength",
                    150: "QuarterLength", 75: "EighthLength"}

# Filename suffix -> canonical ELogCrossSectionProfile.
PROFILE_SUFFIXES = {
    "": "Round",
    "Half": "HalfRound",
    "Quarter_04": "QuarterRound",
    "Hewn1": "Hewn",
    "Hewn2": "HewnTwoFaces",
    "Kalas": "SquareBeam",
    "Plank": "Plank",
    "Gutter": "GroovedHalfRound",
    "Trough": "Trough",
    "FlatNotch_TopBottom": "Notched",
}

# Combinations the processing actions can request for the round log family.
REQUIRED = [(fraction, "Round") for fraction in LENGTH_FRACTIONS.values()] + [
    ("FullLength", profile) for profile in
    ("HalfRound", "QuarterRound", "Hewn", "HewnTwoFaces", "SquareBeam", "Plank", "GroovedHalfRound", "Trough")]

NAME_PATTERN = re.compile(r"^SM_RawLog_(\d+)(?:_(.+))?$")


def log(message):
    unreal.log("[BuildLogAssetManifest] " + str(message))


def import_fbx(path):
    stem = os.path.splitext(os.path.basename(path))[0]
    asset_path = ASSET_DIR + "/" + stem
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", ASSET_DIR)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.get_editor_property("static_mesh_import_data").set_editor_property("combine_meshes", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.EditorAssetLibrary.load_asset(asset_path)


def measure(mesh):
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    origin = bounds.origin
    sizes = {"X": extent.x * 2.0, "Y": extent.y * 2.0, "Z": extent.z * 2.0}
    axis = max(sizes, key=sizes.get)
    return {
        "size_cm": {key: round(value, 2) for key, value in sizes.items()},
        "main_axis": axis,
        "length_cm": round(sizes[axis], 2),
        # Pivot offset from bounds centre; zero means a centred pivot.
        "pivot_offset_cm": [round(-origin.x, 2), round(-origin.y, 2), round(-origin.z, 2)],
    }


def main():
    files = sorted(
        os.path.join(FBX_ROOT, name) for name in os.listdir(FBX_ROOT)
        if name.lower().endswith(".fbx"))
    entries, errors, warnings = [], [], []
    present = set()

    for path in files:
        stem = os.path.splitext(os.path.basename(path))[0]
        mesh = import_fbx(path)
        if not mesh:
            errors.append(f"{stem}: import failed")
            continue
        entry = {"asset": mesh.get_path_name(), "file": os.path.basename(path)}
        entry.update(measure(mesh))

        match = NAME_PATTERN.match(stem)
        nominal = int(match.group(1)) if match and match.group(1).isdigit() else None
        suffix = (match.group(2) or "") if match else None
        fraction = LENGTH_FRACTIONS.get(nominal) if nominal else None
        profile = PROFILE_SUFFIXES.get(suffix) if suffix is not None else None

        if match is None:
            entry["status"] = "unmapped"
            warnings.append(f"{stem}: name does not follow SM_RawLog_<cm>[_<variant>]; "
                            f"measured {entry['length_cm']} cm on {entry['main_axis']}, needs a manual family assignment")
        elif fraction is None or profile is None:
            entry["status"] = "unmapped"
            warnings.append(f"{stem}: no canonical mapping for length {nominal} / suffix '{suffix}'")
        else:
            entry["length_fraction"] = fraction
            entry["cross_section_profile"] = profile
            entry["nominal_length_cm"] = nominal
            deviation = abs(entry["length_cm"] - nominal) / nominal
            entry["length_deviation"] = round(deviation, 4)
            if deviation > LENGTH_TOLERANCE:
                entry["status"] = "length_mismatch"
                errors.append(f"{stem}: filename says {nominal} cm but the mesh measures "
                              f"{entry['length_cm']} cm ({deviation:.1%})")
            else:
                entry["status"] = "ok"
                present.add((fraction, profile))
        entries.append(entry)

    missing = [f"{fraction} + {profile}" for fraction, profile in REQUIRED if (fraction, profile) not in present]
    manifest = {"tolerance": LENGTH_TOLERANCE, "full_length_cm": FULL_LENGTH_CM, "meshes": entries,
                "missing_required_combinations": missing, "errors": errors, "warnings": warnings}
    with open(MANIFEST_JSON, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2)

    lines = ["# Log Asset Manifest", "",
             "Generated by `UnrealEditorScripts/BuildLogAssetManifest.py`; do not edit by hand.", "",
             f"Length tolerance: {LENGTH_TOLERANCE:.0%}. Meshes: {len(entries)}. "
             f"Errors: {len(errors)}. Warnings: {len(warnings)}.", "",
             "| File | Status | Fraction | Profile | Measured length (cm) | Axis | Pivot offset (cm) |",
             "|---|---|---|---|---|---|---|"]
    for entry in entries:
        lines.append("| {file} | {status} | {fraction} | {profile} | {length} | {axis} | {pivot} |".format(
            file=entry["file"], status=entry["status"],
            fraction=entry.get("length_fraction", "-"), profile=entry.get("cross_section_profile", "-"),
            length=entry["length_cm"], axis=entry["main_axis"], pivot=entry["pivot_offset_cm"]))
    lines += ["", "## Missing required combinations", ""] + ([f"- {item}" for item in missing] or ["- none"])
    lines += ["", "## Errors", ""] + ([f"- {item}" for item in errors] or ["- none"])
    lines += ["", "## Warnings", ""] + ([f"- {item}" for item in warnings] or ["- none"])
    with open(MANIFEST_MD, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")

    log(f"{len(entries)} meshes, {len(errors)} errors, {len(warnings)} warnings, {len(missing)} missing combinations")
    for item in errors + missing:
        unreal.log_error("[BuildLogAssetManifest] " + item)


main()
