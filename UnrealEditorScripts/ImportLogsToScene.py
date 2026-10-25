# ImportLogsToScene.py
# Unreal Engine 5.8 Editor Python script.
# Imports the FBX log set from the repository and builds a dedicated staging level.
# It does not alter the existing Generated_Cube_Test level.

import os
import zipfile
import unreal

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FBX_ROOT = os.path.join(PROJECT_ROOT, "AssetPackages", "UnrealExport")

LOG_ASSET_DIR = "/Game/ImportedLogs/Logs"
JOINERY_ASSET_DIR = "/Game/ImportedLogs/Joinery"
TEXTURE_ASSET_DIR = "/Game/ImportedLogs/Textures"
HEIGHT_ZIP = os.path.join(PROJECT_ROOT, "AssetPackages", "endgrainv2_height.zip")
MAP_PATH = "/Game/Maps/Generated_Log_Asset_Test"

CELL_X = 850.0
ROW_Y = 1050.0
GROUND_SCALE_X = 110.0
GROUND_SCALE_Y = 85.0
ACTOR_PREFIX = "LOG_GALLERY_"


def log(message):
    unreal.log("[ImportLogsToScene] " + str(message))


def safe_set(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as exc:
        log("Property skipped: {} ({})".format(property_name, exc))
        return False


def find_asset_path_for_file(filepath):
    relative = os.path.relpath(os.path.dirname(filepath), FBX_ROOT)
    stem = os.path.splitext(os.path.basename(filepath))[0]
    if relative == ".":
        destination = LOG_ASSET_DIR
        group = "Logs"
    else:
        destination = JOINERY_ASSET_DIR
        group = "Joinery"
    return destination, destination + "/" + stem, stem, group


def prepare_import_task(filepath, destination):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filepath)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)

    options = unreal.FbxImportUI()
    safe_set(options, "automated_import_should_detect_type", False)
    safe_set(options, "import_mesh", True)
    safe_set(options, "import_as_skeletal", False)
    safe_set(options, "import_animations", False)
    safe_set(options, "import_materials", True)
    safe_set(options, "import_textures", False)
    safe_set(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)

    try:
        static_data = options.get_editor_property("static_mesh_import_data")
        safe_set(static_data, "combine_meshes", True)
        safe_set(static_data, "generate_lightmap_u_vs", True)
    except Exception as exc:
        log("Using default static-mesh import settings: {}".format(exc))

    task.set_editor_property("options", options)
    return task


def collect_fbx_files():
    if not os.path.isdir(FBX_ROOT):
        raise RuntimeError(
            "FBX source folder not found: {}. Run 'git pull origin main' first.".format(FBX_ROOT)
        )
    files = []
    for root, dirs, names in os.walk(FBX_ROOT):
        dirs.sort()
        for name in sorted(names):
            if name.lower().endswith(".fbx"):
                files.append(os.path.join(root, name))
    return files


def import_height_texture_archive():
    # Import image files from the supplied height-map ZIP as Texture2D assets.
    # The height map is deliberately NOT assigned as Base Color or applied to
    # the meshes automatically; its material usage needs a separate material setup.
    if not os.path.isfile(HEIGHT_ZIP):
        log("Height-map ZIP not found; continuing without it: " + HEIGHT_ZIP)
        return 0

    extract_root = os.path.realpath(
        os.path.join(PROJECT_ROOT, "Saved", "LogAssetImportTemp", "endgrainv2_height")
    )
    os.makedirs(extract_root, exist_ok=True)
    image_extensions = {".png", ".tga", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp", ".exr"}
    extracted = []

    try:
        with zipfile.ZipFile(HEIGHT_ZIP, "r") as archive:
            for member in archive.infolist():
                if member.is_dir():
                    continue
                normalized = os.path.normpath(member.filename.replace("\\", "/"))
                if os.path.isabs(normalized) or normalized == ".." or normalized.startswith(".." + os.sep):
                    log("Skipping unsafe ZIP entry: " + member.filename)
                    continue
                extension = os.path.splitext(normalized)[1].lower()
                if extension not in image_extensions:
                    continue
                target_path = os.path.realpath(os.path.join(extract_root, normalized))
                if os.path.commonpath([extract_root, target_path]) != extract_root:
                    log("Skipping ZIP entry outside extraction folder: " + member.filename)
                    continue
                archive.extract(member, extract_root)
                if os.path.isfile(target_path):
                    extracted.append(target_path)
    except Exception as exc:
        log("Could not read height-map ZIP; continuing without it: {}".format(exc))
        return 0

    unreal.EditorAssetLibrary.make_directory(TEXTURE_ASSET_DIR)
    tasks = []
    for filepath in sorted(set(extracted)):
        stem = os.path.splitext(os.path.basename(filepath))[0]
        target_asset = TEXTURE_ASSET_DIR + "/" + stem
        if unreal.EditorAssetLibrary.does_asset_exist(target_asset):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", filepath)
        task.set_editor_property("destination_path", TEXTURE_ASSET_DIR)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", True)
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    log("Height-map images discovered: {}; newly imported texture assets: {}".format(
        len(extracted), len(tasks)
    ))
    return len(extracted)


def group_for_mesh(stem, group):
    if group == "Joinery":
        return "07 - Joinery / cut connections"
    for prefix, label in (
        ("SM_RawLog_75_", "01 - 75 variants"),
        ("SM_RawLog_150_", "02 - 150 variants"),
        ("SM_RawLog_300_", "03 - 300 variants"),
        ("SM_RawLog_600_", "04 - 600 variants"),
        ("SM_RawLog_Long_", "05 - Long variants"),
        ("SM_RawLog_Short_", "06 - Short variants"),
    ):
        if stem.startswith(prefix):
            return label
    if stem == "SM_RawLog_75":
        return "01 - 75 variants"
    if stem == "SM_RawLog_150":
        return "02 - 150 variants"
    if stem == "SM_RawLog_300":
        return "03 - 300 variants"
    if stem == "SM_RawLog_600":
        return "04 - 600 variants"
    return "08 - Other"


def prepare_level():
    unreal.EditorAssetLibrary.make_directory("/Game/Maps")

    # NewLevel/LoadMap can close the current map without prompting; save first
    # so an existing working scene is not silently discarded.
    try:
        if not unreal.EditorLoadingAndSavingUtils.save_current_level():
            raise RuntimeError(
                "Could not save the currently open level; stopped to protect unsaved work."
            )
    except Exception as exc:
        raise RuntimeError("Could not safely switch levels: {}".format(exc))

    map_file = os.path.join(
        unreal.Paths.project_content_dir(), "Maps", "Generated_Log_Asset_Test.umap"
    )
    if os.path.isfile(map_file):
        log("Loading existing log staging map: " + MAP_PATH)
        if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
            raise RuntimeError("Could not load existing map: " + MAP_PATH)
    else:
        log("Creating log staging map: " + MAP_PATH)
        if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
            raise RuntimeError("Unreal could not create map: " + MAP_PATH)

    # Remove only actors previously generated by this script.
    # Any unrelated actors in this level are left alone.
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        try:
            if actor.get_actor_label().startswith(ACTOR_PREFIX):
                actor.destroy_actor()
        except Exception:
            pass


def add_ground_and_lighting():
    plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    if plane is None:
        plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    if plane is not None:
        ground = unreal.EditorLevelLibrary.spawn_actor_from_object(
            plane, unreal.Vector(0.0, 0.0, -4.0), unreal.Rotator(0.0, 0.0, 0.0)
        )
        ground.set_actor_label(ACTOR_PREFIX + "Ground")
        ground.set_actor_scale3d(
            unreal.Vector(GROUND_SCALE_X, GROUND_SCALE_Y, 1.0 if "Plane" in str(plane.get_name()) else 0.08)
        )
    else:
        log("Warning: could not load Engine BasicShapes Plane/Cube; no floor was added.")

    try:
        sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.DirectionalLight,
            unreal.Vector(0.0, 0.0, 3500.0),
            unreal.Rotator(-48.0, -32.0, 0.0),
        )
        sun.set_actor_label(ACTOR_PREFIX + "Sun")
    except Exception as exc:
        log("Directional light could not be created: {}".format(exc))

    try:
        sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyAtmosphere,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        sky.set_actor_label(ACTOR_PREFIX + "SkyAtmosphere")
    except Exception as exc:
        log("Sky atmosphere skipped: {}".format(exc))

    try:
        skylight = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyLight,
            unreal.Vector(0.0, 0.0, 1500.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        skylight.set_actor_label(ACTOR_PREFIX + "SkyLight")
        try:
            component = skylight.get_component_by_class(unreal.SkyLightComponent)
            if component:
                safe_set(component, "intensity", 1.0)
                if hasattr(component, "recapture_sky"):
                    component.recapture_sky()
        except Exception:
            pass
    except Exception as exc:
        log("Skylight skipped: {}".format(exc))


def place_mesh_gallery(mesh_records):
    groups = {}
    for record in mesh_records:
        groups.setdefault(group_for_mesh(record["stem"], record["group"]), []).append(record)

    group_order = [
        "01 - 75 variants",
        "02 - 150 variants",
        "03 - 300 variants",
        "04 - 600 variants",
        "05 - Long variants",
        "06 - Short variants",
        "07 - Joinery / cut connections",
        "08 - Other",
    ]
    visible_groups = [(name, sorted(groups.get(name, []), key=lambda item: item["stem"]))
                      for name in group_order if groups.get(name)]
    row_count = len(visible_groups)

    placed = 0
    for row_index, (group_name, records) in enumerate(visible_groups):
        y = (row_index - (row_count - 1) / 2.0) * ROW_Y
        start_x = -((len(records) - 1) * CELL_X) / 2.0
        log("Placing {}: {} meshes".format(group_name, len(records)))

        for col_index, record in enumerate(records):
            mesh = unreal.EditorAssetLibrary.load_asset(record["asset_path"])
            if mesh is None:
                log("Missing imported asset; not placed: " + record["asset_path"])
                continue

            x = start_x + col_index * CELL_X
            actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
                mesh,
                unreal.Vector(x, y, 0.0),
                unreal.Rotator(0.0, 0.0, 0.0),
            )
            if actor is None:
                log("Could not spawn actor for: " + record["stem"])
                continue

            actor.set_actor_label(
                ACTOR_PREFIX + group_name.split(" - ")[0] + "_" + record["stem"]
            )

            # Lift the mesh only enough to put its calculated bounds on the floor.
            # Works for either centered pivots or pivots placed at the bottom.
            try:
                origin, extent = actor.get_actor_bounds(False)
                target_z = max(2.0, float(extent.z - origin.z + 2.0))
                actor.set_actor_location(
                    unreal.Vector(x, y, target_z), False, False
                )
            except Exception:
                actor.set_actor_location(
                    unreal.Vector(x, y, 20.0), False, False
                )

            placed += 1

    return placed, visible_groups


def aim_viewport():
    camera_location = unreal.Vector(0.0, -9800.0, 8200.0)
    target = unreal.Vector(0.0, 0.0, 100.0)
    rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, target)

    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        subsystem.set_level_viewport_camera_info(camera_location, rotation)
    except Exception:
        try:
            unreal.EditorLevelLibrary.set_level_viewport_camera_info(
                camera_location, rotation
            )
        except Exception as exc:
            log("Could not set viewport camera: {}".format(exc))


def main():
    fbx_files = collect_fbx_files()
    if not fbx_files:
        raise RuntimeError("No .fbx files found in " + FBX_ROOT)

    unreal.EditorAssetLibrary.make_directory(LOG_ASSET_DIR)
    unreal.EditorAssetLibrary.make_directory(JOINERY_ASSET_DIR)
    unreal.EditorAssetLibrary.make_directory(TEXTURE_ASSET_DIR)

    tasks = []
    records = []
    for filepath in fbx_files:
        destination, asset_path, stem, group = find_asset_path_for_file(filepath)
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tasks.append(prepare_import_task(filepath, destination))
        records.append({
            "filepath": filepath,
            "asset_path": asset_path,
            "stem": stem,
            "group": group,
        })

    if tasks:
        log("Importing {} FBX files ({} already existed).".format(len(tasks), len(fbx_files) - len(tasks)))
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    else:
        log("All {} FBX assets were already imported; skipping reimport.".format(len(records)))

    import_height_texture_archive()
    unreal.EditorAssetLibrary.save_directory("/Game/ImportedLogs", False, True)

    prepare_level()
    add_ground_and_lighting()
    placed, groups = place_mesh_gallery(records)
    aim_viewport()

    try:
        unreal.EditorLevelLibrary.save_current_level()
    except Exception:
        unreal.EditorLoadingAndSavingUtils.save_current_level()

    log("DONE. FBX files discovered: {}".format(len(fbx_files)))
    log("DONE. Mesh actors placed: {}".format(placed))
    log("DONE. Gallery map: {}".format(MAP_PATH))
    log("The existing Generated_Cube_Test map was not modified.")
    log("Open the World Outliner and search for LOG_GALLERY_ to locate imported display actors.")


try:
    main()
except Exception as exc:
    unreal.log_error("[ImportLogsToScene] FAILED: {}".format(exc))
    raise
