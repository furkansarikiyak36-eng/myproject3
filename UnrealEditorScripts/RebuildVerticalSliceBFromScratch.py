"""Rebuild VerticalSlice_B as a clean, lit ground-plane scene.

Unreal Engine 5.8: run this script once through Tools > Execute Python Script.
Only /Game/Maps/VerticalSlice_B is changed. If that map still exists, its actors
are cleared before rebuilding; other maps and assets are never touched.
"""
import unreal

MAP_PATH = "/Game/DemoAssets/Maps/VerticalSlice_B"
GROUND_LABEL = "VSliceB_Presentation_FallbackGround"
PREFIX = "VSliceB_Presentation_"
# Safety lock: this script deletes every actor in VerticalSlice_B before rebuilding.
# Keep False during normal work. Set True only when intentionally discarding B map actors.
ALLOW_DESTRUCTIVE_REBUILD = False
SUN_INTENSITY_LUX = 38.716587


def log(message):
    unreal.log("[VerticalSlice_B_Rebuild] " + str(message))


def spawn(actor_class, location, rotation, label):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, location, rotation
    )
    if not actor:
        raise RuntimeError("Could not spawn actor: " + label)
    actor.set_actor_label(label)
    return actor


def set_property(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        log("Optional property skipped (" + name + "): " + str(exc))


def open_or_create_clean_map():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not ALLOW_DESTRUCTIVE_REBUILD:
            raise RuntimeError(
                "Safety stop: " + MAP_PATH + " already exists. This script deletes every actor in that map. "
                "Use the normal VerticalSlice_B map workflow, or set ALLOW_DESTRUCTIVE_REBUILD = True "
                "only when intentionally replacing the map."
            )
        if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
            raise RuntimeError("Could not load existing map: " + MAP_PATH)
        # This is intentionally destructive, but strictly limited to VerticalSlice_B.
        for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
            if actor:
                if not unreal.EditorLevelLibrary.destroy_actor(actor):
                    raise RuntimeError(
                        "Could not remove actor: " + actor.get_actor_label()
                    )
        log("Cleared actors from existing VerticalSlice_B.")
    else:
        if not unreal.EditorAssetLibrary.does_directory_exist("/Game/DemoAssets/Maps"):
            unreal.EditorAssetLibrary.make_directory("/Game/DemoAssets/Maps")
        if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
            raise RuntimeError("Could not create map: " + MAP_PATH)
        log("Created new map: " + MAP_PATH)


def create_ground():
    ground = spawn(
        unreal.StaticMeshActor,
        unreal.Vector(0, 0, 0),
        unreal.Rotator(0, 0, 0),
        GROUND_LABEL
    )
    component = ground.get_component_by_class(unreal.StaticMeshComponent)
    plane = unreal.load_object(None, "/Engine/BasicShapes/Plane.Plane")
    if not component or not plane:
        raise RuntimeError("Engine BasicShapes Plane mesh could not be loaded.")
    component.set_static_mesh(plane)
    # UE plane is 100x100 cm by default; this yields a 25 km x 25 km test floor.
    ground.set_actor_scale3d(unreal.Vector(250, 250, 1))
    set_property(component, "mobility", unreal.ComponentMobility.STATIC)
    set_property(component, "collision_profile_name", "BlockAll")
    component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    return ground


def create_lighting():
    sun = spawn(
        unreal.DirectionalLight,
        unreal.Vector(0, 0, 5000),
        unreal.Rotator(-45, -32, 0),
        PREFIX + "DirectionalSun"
    )
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if sun_component:
        set_property(sun_component, "mobility", unreal.ComponentMobility.MOVABLE)
        set_property(sun_component, "intensity", SUN_INTENSITY_LUX)
        set_property(sun_component, "light_color", unreal.LinearColor(1.0, 0.94, 0.82, 1.0))
        set_property(sun_component, "cast_shadows", True)

    sky = spawn(
        unreal.SkyLight,
        unreal.Vector(0, 0, 3500),
        unreal.Rotator(0, 0, 0),
        PREFIX + "SkyLight"
    )
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        set_property(sky_component, "mobility", unreal.ComponentMobility.MOVABLE)
        set_property(sky_component, "intensity", 1.0)
        set_property(sky_component, "real_time_capture", True)

    try:
        spawn(
            unreal.SkyAtmosphere,
            unreal.Vector(0, 0, 0),
            unreal.Rotator(0, 0, 0),
            PREFIX + "SkyAtmosphere"
        )
    except Exception as exc:
        log("SkyAtmosphere skipped: " + str(exc))

    try:
        fog = spawn(
            unreal.ExponentialHeightFog,
            unreal.Vector(0, 0, 0),
            unreal.Rotator(0, 0, 0),
            PREFIX + "HeightFog"
        )
        fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        if fog_component:
            set_property(fog_component, "fog_density", 0.001)
            set_property(fog_component, "fog_height_falloff", 0.2)
            set_property(fog_component, "start_distance", 12000.0)
            set_property(fog_component, "fog_max_opacity", 0.2)
    except Exception as exc:
        log("Height fog skipped: " + str(exc))


def frame_viewport():
    try:
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        location = unreal.Vector(0, -22000, 15000)
        target = unreal.Vector(0, 0, 0)
        rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
        editor.set_level_viewport_camera_info(location, rotation)
    except Exception as exc:
        log("Viewport framing skipped: " + str(exc))


def main():
    open_or_create_clean_map()
    create_ground()
    create_lighting()

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save " + MAP_PATH)

    frame_viewport()
    log("DONE. Saved clean ground + lighting scene to " + MAP_PATH)
    log("Ground actor: " + GROUND_LABEL)
    log("Directional light intensity: " + str(SUN_INTENSITY_LUX) + " lux")
    log("No other map or asset was changed.")


main()
