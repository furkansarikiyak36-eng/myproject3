"""Repair the existing VerticalSlice_B presentation without touching VerticalSlice_A.

Run this in Unreal Editor after compiling MyProject3Editor.
It opens /Game/Maps/VerticalSlice_B, creates an editor-visible terrain presentation,
organizes the light rig, asks the world actor to generate its demo layout, frames the
viewport, and saves only VerticalSlice_B.
"""
import unreal

MAP_PATH = "/Game/Maps/VerticalSlice_B"
WORLD_CLASS_PATH = "/Script/MyProject3.VerticalSliceBWorldActor"
PRESENTATION_PREFIX = "VSliceB_Presentation_"
WORLD_LABEL = "VSliceB_WorldBootstrap"
PLAYER_LABEL = "VSliceB_PlayerStart"


def log(message):
    unreal.log("[VerticalSlice_B_Presentation] " + str(message))


def safe_set(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as exc:
        log("Property skipped " + property_name + ": " + str(exc))
        return False


def spawn_labelled(actor_class, location, rotation, label):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
    if actor:
        actor.set_actor_label(label)
    return actor


def delete_old_presentation_actors():
    # VerticalSlice_B is a dedicated demo map: replace old presentation lights and
    # anything created by this script, while preserving the gameplay/world actors.
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        try:
            label = actor.get_actor_label()
            class_name = actor.get_class().get_name()
            is_script_actor = label.startswith(PRESENTATION_PREFIX)
            is_old_light = class_name in {
                "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog"
            }
            is_old_fallback_plane = class_name == "StaticMeshActor" and label.startswith("VSliceB_")
            if is_script_actor or is_old_light or is_old_fallback_plane:
                unreal.EditorLevelLibrary.destroy_actor(actor)
        except Exception:
            pass


def configure_directional_sun():
    actor = spawn_labelled(
        unreal.DirectionalLight,
        unreal.Vector(0, 0, 5000),
        unreal.Rotator(-42, -32, 0),
        PRESENTATION_PREFIX + "DirectionalSun"
    )
    if not actor:
        raise RuntimeError("Could not create Directional Light.")
    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    if component:
        safe_set(component, "mobility", unreal.ComponentMobility.MOVABLE)
        safe_set(component, "intensity", 100000.0)
        safe_set(component, "light_color", unreal.LinearColor(1.0, 0.94, 0.82, 1.0))
        safe_set(component, "cast_shadows", True)
    return actor


def configure_sky_light():
    actor = spawn_labelled(
        unreal.SkyLight,
        unreal.Vector(0, 0, 3500),
        unreal.Rotator(0, 0, 0),
        PRESENTATION_PREFIX + "SkyLight"
    )
    if not actor:
        raise RuntimeError("Could not create Sky Light.")
    component = actor.get_component_by_class(unreal.SkyLightComponent)
    if component:
        safe_set(component, "mobility", unreal.ComponentMobility.MOVABLE)
        safe_set(component, "intensity", 1.25)
        safe_set(component, "real_time_capture", True)
    return actor


def create_sky_and_fog():
    try:
        sky = spawn_labelled(
            unreal.SkyAtmosphere, unreal.Vector(0, 0, 0),
            unreal.Rotator(0, 0, 0), PRESENTATION_PREFIX + "SkyAtmosphere"
        )
        if not sky:
            log("SkyAtmosphere actor unavailable; using directional and skylight only.")
    except Exception as exc:
        log("SkyAtmosphere skipped: " + str(exc))

    try:
        fog = spawn_labelled(
            unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0),
            unreal.Rotator(0, 0, 0), PRESENTATION_PREFIX + "HeightFog"
        )
        if fog:
            component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
            if component:
                safe_set(component, "fog_density", 0.003)
                safe_set(component, "fog_height_falloff", 0.2)
                safe_set(component, "start_distance", 5000.0)
                safe_set(component, "fog_max_opacity", 0.35)
    except Exception as exc:
        log("Height fog skipped: " + str(exc))


def create_editor_visible_ground():
    # Fallback ground is deliberately below the procedural heightfield.
    # It remains visible in the editor if the world actor has not generated its terrain yet.
    actor = spawn_labelled(
        unreal.StaticMeshActor,
        unreal.Vector(0, 0, -500),
        unreal.Rotator(0, 0, 0),
        PRESENTATION_PREFIX + "FallbackGround"
    )
    if not actor:
        raise RuntimeError("Could not create fallback ground plane.")
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    plane = unreal.load_object(None, "/Engine/BasicShapes/Plane.Plane")
    if not component or not plane:
        raise RuntimeError("Could not load the engine BasicShapes Plane mesh.")
    component.set_static_mesh(plane)
    actor.set_actor_scale3d(unreal.Vector(250, 250, 1))
    safe_set(component, "collision_profile_name", "BlockAll")
    safe_set(component, "mobility", unreal.ComponentMobility.STATIC)
    return actor


def main():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError(
            MAP_PATH + " does not exist. Run CreateVerticalSliceBMap.py first."
        )

    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not open " + MAP_PATH)

    actor_class = unreal.load_class(None, WORLD_CLASS_PATH)
    if not actor_class:
        raise RuntimeError(
            "Could not load VerticalSliceBWorldActor. Compile MyProject3Editor and restart Unreal."
        )

    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    demo = None
    start = None
    for actor in actors:
        if actor.get_class() == actor_class:
            demo = actor
        elif actor.get_actor_label() == PLAYER_LABEL:
            start = actor

    if demo is None:
        demo = unreal.EditorLevelLibrary.spawn_actor_from_class(
            actor_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
        )
        if not demo:
            raise RuntimeError("Could not spawn VerticalSliceBWorldActor.")
    demo.set_actor_label(WORLD_LABEL)

    if start is None:
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(0, 0, 450), unreal.Rotator(0, 0, 0)
        )
        if not start:
            raise RuntimeError("Could not create PlayerStart.")
    start.set_actor_label(PLAYER_LABEL)

    delete_old_presentation_actors()
    configure_directional_sun()
    configure_sky_light()
    create_sky_and_fog()
    create_editor_visible_ground()

    # BuildWorld is BlueprintCallable, so Python can invoke it in the editor world.
    # This generates the terrain, marked routes and demo work zones before Play.
    try:
        result = demo.build_world()
        log("BuildWorld() returned: " + str(result))
        if not result:
            log("World generation reported failure; fallback plane and lighting were still created.")
    except Exception as exc:
        log("BuildWorld() could not run in editor context: " + str(exc))
        log("Fallback ground remains available; check Output Log before Play.")

    # Save only VerticalSlice_B. No other map is loaded or modified.
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save " + MAP_PATH)

    # Editor overview aimed at the full playable footprint.
    try:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        location = unreal.Vector(0, -21000, 15500)
        target = unreal.Vector(0, 0, 0)
        rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
        editor_subsystem.set_level_viewport_camera_info(location, rotation)
    except Exception as exc:
        log("Viewport framing skipped: " + str(exc))

    log("Presentation repair saved to " + MAP_PATH)
    log("Inspect the map before Play; then press Play to verify gameplay. Send any [VerticalSlice_B_Presentation] errors.")


main()
