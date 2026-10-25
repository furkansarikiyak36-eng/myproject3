# CreateLogSnapTestLevel.py
# Creates a small playable log-carry / snap prototype map for Unreal Engine 5.8.
#
# Prerequisite: compile the MyProject3 Editor target so these native classes exist:
# /Script/MyProject3.LogBuildPiece, /Script/MyProject3.LogSnapGameMode
#
# This script only creates /Game/Maps/LogSnap_Playground and imports selected
# FBX files. Existing levels are saved before switching and are not edited.

import os
import unreal

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FBX_ROOT = os.path.join(PROJECT_ROOT, "AssetPackages", "UnrealExport")
MAP_PATH = "/Game/Maps/LogSnap_Playground"
ASSET_LOGS = "/Game/ImportedLogs/Logs"
ASSET_JOINERY = "/Game/ImportedLogs/Joinery"
PREFIX = "LOGSNAP_DEMO_"

# Source FBX path (relative to FBX_ROOT), destination asset path and intended role.
ASSET_SPECS = [
    ("SM_RawLog_600.fbx", ASSET_LOGS + "/SM_RawLog_600", "SupportBeam"),
    ("SM_RawLog_300.fbx", ASSET_LOGS + "/SM_RawLog_300", "LooseWhole"),
    ("SM_RawLog_150_Half.fbx", ASSET_LOGS + "/SM_RawLog_150_Half", "SplitHalf150"),
    ("SM_RawLog_300_Half.fbx", ASSET_LOGS + "/SM_RawLog_300_Half", "SplitHalf300"),
    ("SM_RawLog_300_Quarter_04.fbx", ASSET_LOGS + "/SM_RawLog_300_Quarter_04", "Quarter300"),
    ("SM_RawLog_300_Gutter.fbx", ASSET_LOGS + "/SM_RawLog_300_Gutter", "Gutter300"),
    ("SM_RawLog_300_Trough.fbx", ASSET_LOGS + "/SM_RawLog_300_Trough", "Trough300"),
    ("SM_RawLog_300_Hewn1.fbx", ASSET_LOGS + "/SM_RawLog_300_Hewn1", "Hewn1300"),
    ("SM_RawLog_300_Hewn2.fbx", ASSET_LOGS + "/SM_RawLog_300_Hewn2", "Hewn2300"),
    ("SM_RawLog_300_Kalas.fbx", ASSET_LOGS + "/SM_RawLog_300_Kalas", "Kalas300"),
    ("SM_RawLog_300_Plank.fbx", ASSET_LOGS + "/SM_RawLog_300_Plank", "Plank300"),
    ("SM_RawLog_75_FlatNotch_TopBottom.fbx",
     ASSET_LOGS + "/SM_RawLog_75_FlatNotch_TopBottom", "FlatNotch75"),
    ("Archive/SM_RawLog_75_Notch_Saddle.fbx",
     ASSET_JOINERY + "/SM_RawLog_75_Notch_Saddle", "LooseNotch"),
]


def log(message):
    unreal.log("[CreateLogSnapTestLevel] " + str(message))


def require_class(path, friendly_name):
    native_class = unreal.load_class(None, path)
    if native_class is None:
        raise RuntimeError(
            "{} is not loaded. Compile the MyProject3 Editor target, restart/reopen "
            "the editor if prompted, then rerun this script.".format(friendly_name)
        )
    return native_class


def make_import_task(filepath, destination):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filepath)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    try:
        data = options.get_editor_property("static_mesh_import_data")
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("generate_lightmap_u_vs", True)
    except Exception as exc:
        log("Using default FBX static-mesh settings: {}".format(exc))

    task.set_editor_property("options", options)
    return task


def ensure_assets():
    unreal.EditorAssetLibrary.make_directory(ASSET_LOGS)
    unreal.EditorAssetLibrary.make_directory(ASSET_JOINERY)

    pending = []
    for relative_path, asset_path, role in ASSET_SPECS:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            continue
        disk_path = os.path.join(FBX_ROOT, relative_path.replace("/", os.sep))
        if not os.path.isfile(disk_path):
            raise RuntimeError("Required FBX is missing: " + disk_path)
        destination = ASSET_JOINERY if relative_path.startswith("Archive/") else ASSET_LOGS
        pending.append(make_import_task(disk_path, destination))

    if pending:
        log("Importing {} required FBX assets.".format(len(pending)))
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(pending)
    else:
        log("Required FBX assets are already imported.")

    missing = []
    loaded = {}
    for relative_path, asset_path, role in ASSET_SPECS:
        mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        if mesh is None:
            missing.append(asset_path)
        else:
            loaded[role] = mesh

    if missing:
        raise RuntimeError("Failed to load these imported assets: " + ", ".join(missing))
    return loaded


def make_transform(location, rotation):
    return unreal.Transform(
        location=location,
        rotation=rotation,
        scale=unreal.Vector(1.0, 1.0, 1.0)
    )


def bounds_and_floor_actor(actor, x, y):
    actor.set_actor_location(unreal.Vector(x, y, 0.0), False, False)
    origin, extent = actor.get_actor_bounds(False)
    current = actor.get_actor_location()
    # Move the actor so the bottom of its calculated world bounds rests just
    # above Z=0. This also handles FBX pivots offset from the mesh center.
    new_z = current.z + extent.z - (origin.z - current.z) + 2.0
    actor.set_actor_location(unreal.Vector(x, y, new_z), False, False)
    origin, extent = actor.get_actor_bounds(False)
    return origin, extent


def spawn_native_piece(piece_class, mesh, role, location, placed=False, variants=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        piece_class, location, unreal.Rotator(0.0, 0.0, 0.0)
    )
    if actor is None:
        raise RuntimeError("Failed to spawn native log actor for role: " + role)

    actor.set_actor_label(PREFIX + role)
    actor.set_editor_property("can_be_carried", True)
    actor.set_editor_property("starts_placed", bool(placed))
    actor.set_piece_static_mesh(mesh)
    actor.set_piece_type_by_name(mesh.get_name())

    # Attach length-matched cut variants to the untouched whole log. The native
    # actor only permits a transition if the current piece state makes sense.
    if variants:
        for property_name, asset_role in variants.items():
            variant_mesh = assets_for_variants.get(asset_role)
            if variant_mesh is not None:
                actor.set_editor_property(property_name, variant_mesh)

    if placed:
        actor.set_placed_state(True)
    return actor


def spawn_demo_floor_and_lighting():
    plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    if plane is None:
        raise RuntimeError("Could not load /Engine/BasicShapes/Plane.")

    floor = unreal.EditorLevelLibrary.spawn_actor_from_object(
        plane, unreal.Vector(0.0, 0.0, -5.0), unreal.Rotator(0.0, 0.0, 0.0)
    )
    floor.set_actor_label(PREFIX + "Ground")
    floor.set_actor_scale3d(unreal.Vector(200.0, 200.0, 1.0))

    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 2500.0),
        unreal.Rotator(-48.0, -30.0, 0.0)
    )
    sun.set_actor_label(PREFIX + "Sun")

    try:
        sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyAtmosphere,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0)
        )
        sky.set_actor_label(PREFIX + "SkyAtmosphere")
    except Exception as exc:
        log("Sky atmosphere skipped: {}".format(exc))

    try:
        sky_light = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyLight,
            unreal.Vector(0.0, 0.0, 1500.0),
            unreal.Rotator(0.0, 0.0, 0.0)
        )
        sky_light.set_actor_label(PREFIX + "SkyLight")
    except Exception as exc:
        log("Sky light skipped: {}".format(exc))


def add_support_top_snap_points(support_actor):
    # Add two target points on the top surface of a placed 600-length beam.
    # Their SnapTag intentionally matches log end points ("LogEnd"), allowing
    # an upright post's end to snap to the beam top. The face points upward.
    origin, extent = support_actor.get_actor_bounds(False)
    actor_location = support_actor.get_actor_location()
    local_center_x = origin.x - actor_location.x
    local_center_y = origin.y - actor_location.y
    local_top_z = origin.z - actor_location.z + extent.z

    for suffix, local_x in (("LeftPost", local_center_x - 190.0),
                            ("RightPost", local_center_x + 190.0)):
        point_transform = make_transform(
            unreal.Vector(local_x, local_center_y, local_top_z),
            unreal.Rotator(90.0, 0.0, 0.0)
        )
        support_actor.add_custom_snap_point(
            "Top_" + suffix,
            "LogEnd",
            point_transform,
            True,   # can source
            True,   # can receive
            True,   # accepts all piece types in this prototype
            True    # flip incoming orientation so the log extends upward
        )


def add_player_start(game_mode_class):
    start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(0.0, -1250.0, 110.0),
        unreal.Rotator(0.0, 90.0, 0.0)
    )
    start.set_actor_label(PREFIX + "PlayerStart")

    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        settings = world.get_world_settings() if world else None
        if settings:
            settings.set_editor_property("default_game_mode", game_mode_class)
    except Exception as exc:
        log("Could not set map-level GameMode override: {}".format(exc))


def prepare_map():
    # Protect any edits in the user's currently open map before switching.
    try:
        saved = unreal.EditorLoadingAndSavingUtils.save_current_level()
        if not saved:
            raise RuntimeError("The current level did not save successfully.")
    except Exception as exc:
        raise RuntimeError(
            "Stopping before map switch to protect the current level: {}".format(exc)
        )

    map_file = os.path.join(
        unreal.Paths.project_content_dir(), "Maps", "LogSnap_Playground.umap"
    )

    unreal.EditorAssetLibrary.make_directory("/Game/Maps")
    if os.path.isfile(map_file):
        log("Loading existing playground map.")
        if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
            raise RuntimeError("Failed to load " + MAP_PATH)
    else:
        log("Creating new playground map.")
        if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
            raise RuntimeError("Failed to create " + MAP_PATH)

    # Idempotent cleanup: only delete actors owned by this generated test map.
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        try:
            if actor.get_actor_label().startswith(PREFIX):
                actor.destroy_actor()
        except Exception:
            pass


def aim_editor_view():
    location = unreal.Vector(0.0, -1700.0, 1350.0)
    target = unreal.Vector(0.0, 100.0, 80.0)
    rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        subsystem.set_level_viewport_camera_info(location, rotation)
    except Exception as exc:
        log("Viewport camera unchanged: {}".format(exc))


def main():
    piece_class = require_class("/Script/MyProject3.LogBuildPiece", "ALogBuildPiece")
    game_mode_class = require_class("/Script/MyProject3.LogSnapGameMode", "ALogSnapGameMode")
    assets = ensure_assets()

    prepare_map()
    spawn_demo_floor_and_lighting()
    add_player_start(game_mode_class)

    # Stable support beam: fixed in place, with two top anchors for vertical logs.
    beam = spawn_native_piece(
        piece_class,
        assets["SupportBeam"],
        "SupportBeam",
        unreal.Vector(0.0, 180.0, 0.0),
        placed=True
    )
    bounds_and_floor_actor(beam, 0.0, 180.0)
    add_support_top_snap_points(beam)

    # A second placed beam gives the player a clearly visible end-to-end snap
    # target. Its centre is offset by one nominal 600-unit log length.
    beam2 = spawn_native_piece(
        piece_class,
        assets["SupportBeam"],
        "EndSnapBeam",
        unreal.Vector(600.0, 180.0, 0.0),
        placed=True
    )
    bounds_and_floor_actor(beam2, 600.0, 180.0)
    add_support_top_snap_points(beam2)

    # A second parallel support row creates more than one meaningful construction
    # target. All rows are separate stable actors with reserved socket capacity.
    rear_beam = spawn_native_piece(
        piece_class,
        assets["SupportBeam"],
        "RearSupportBeam",
        unreal.Vector(0.0, 780.0, 0.0),
        placed=True
    )
    bounds_and_floor_actor(rear_beam, 0.0, 780.0)
    add_support_top_snap_points(rear_beam)

    rear_beam2 = spawn_native_piece(
        piece_class,
        assets["SupportBeam"],
        "RearEndSnapBeam",
        unreal.Vector(600.0, 780.0, 0.0),
        placed=True
    )
    bounds_and_floor_actor(rear_beam2, 600.0, 780.0)
    add_support_top_snap_points(rear_beam2)

    # Only the full 300-length log receives a complete same-length processing
    # set. A player can split it, then quarter it or make a groove/trough/plank.
    assets_for_variants = assets
    cut_variants_300 = {
        "split_half_variant_mesh": "SplitHalf300",
        "quarter_variant_mesh": "Quarter300",
        "hewn_variant_mesh": "Hewn1300",
        "gutter_variant_mesh": "Gutter300",
        "trough_variant_mesh": "Trough300",
        "kalas_variant_mesh": "Kalas300",
        "plank_variant_mesh": "Plank300",
    }

    loose_specs = [
        ("LooseWhole_Cuttable", "LooseWhole", 0.0, -260.0, True),
        ("LooseWhole_ReserveA", "LooseWhole", 420.0, -260.0, False),
        ("LooseWhole_ReserveB", "LooseWhole", -420.0, -260.0, False),
        ("LooseHalf", "SplitHalf300", 260.0, -520.0, False),
        ("LooseGutter", "Gutter300", -260.0, -520.0, False),
        ("LooseHewn", "Hewn1300", 520.0, -520.0, False),
        ("LooseKalas", "Kalas300", 0.0, -820.0, False),
        ("LoosePlank", "Plank300", -520.0, -820.0, False),
        ("LooseNotch", "LooseNotch", 260.0, -900.0, False),
    ]
    for label, role, x, y, attach_variants in loose_specs:
        actor = spawn_native_piece(
            piece_class,
            assets[role],
            label,
            unreal.Vector(x, y, 0.0),
            placed=False,
            variants=cut_variants_300 if attach_variants else None
        )
        bounds_and_floor_actor(actor, x, y)

    aim_editor_view()
    unreal.EditorAssetLibrary.save_directory("/Game/ImportedLogs", False, True)

    try:
        unreal.EditorLevelLibrary.save_current_level()
    except Exception:
        unreal.EditorLoadingAndSavingUtils.save_current_level()

    log("PLAYGROUND READY: " + MAP_PATH)
    log("Controls: WASD move, mouse look, E pick up/place, G drop, R rotate 90 degrees.")
    log("Right mouse cycles Horizontal / Vertical / Diagonal placement mode.")
    log("C cycles processing: split / quarter / hewn / gutter / trough / kalas / plank.")
    log("F applies the selected operation to a loose, unplaced log in interaction range.")
    log("The cuttable full log has same-length variants; invalid cut sequences are rejected.")
    log("Target beam has two added top snap anchors; its ends also use automatic LogEnd anchors.")
    log("Only this generated test map was rebuilt. Existing other maps were not modified.")


try:
    main()
except Exception as exc:
    unreal.log_error("[CreateLogSnapTestLevel] FAILED: {}".format(exc))
    raise
