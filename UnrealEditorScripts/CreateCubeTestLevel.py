"""
Unreal Engine 5.8 Editor Python script.

Creates (or repairs) /Game/Maps/Generated_Cube_Test with:
- A standard 100-unit cube
- A ground plane for visible shadows
- A Directional Light (sun)
- A Sky Light (ambient fill)
- A Sky Atmosphere

Run in Unreal Editor using Tools > Execute Python Script.
The script is safe to rerun: actors are found by label and are not duplicated.
If the target level already exists and another level is open, the current level
is saved before switching to the target level.
"""

import unreal

LEVEL_PATH = "/Game/Maps/Generated_Cube_Test"
MAPS_DIR = "/Game/Maps"
CUBE_ASSET_PATH = "/Engine/BasicShapes/Cube.Cube"
PLANE_ASSET_PATH = "/Engine/BasicShapes/Plane.Plane"


def find_actor_by_label(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor and actor.get_actor_label() == label:
            return actor
    return None


def get_or_spawn_actor(label, actor_class, location, rotation):
    actor = find_actor_by_label(label)
    if actor:
        return actor

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, location, rotation
    )
    if not actor:
        raise RuntimeError(f"Could not create actor: {label}")

    actor.set_actor_label(label)
    return actor


def set_optional_property(component, property_name, value):
    # Keep the script resilient if a property differs between engine builds.
    try:
        component.set_editor_property(property_name, value)
    except Exception as exc:
        unreal.log_warning(
            f"Could not set {property_name} on {component.get_name()}: {exc}"
        )


def create_or_repair_cube_test_level():
    assets = unreal.EditorAssetLibrary
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    assets.make_directory(MAPS_DIR)

    if assets.does_asset_exist(LEVEL_PATH):
        current_world = unreal.EditorLevelLibrary.get_editor_world()
        current_world_path = current_world.get_path_name() if current_world else ""

        # Do not reload the target if it is already open; otherwise save the
        # currently open level before switching so user edits are not discarded.
        if LEVEL_PATH not in current_world_path:
            unreal.EditorLevelLibrary.save_current_level()
            if not level_editor.load_level(LEVEL_PATH):
                raise RuntimeError(f"Could not open existing level: {LEVEL_PATH}")
    else:
        if not level_editor.new_level(LEVEL_PATH, False):
            raise RuntimeError(f"Could not create level: {LEVEL_PATH}")

    cube_mesh = assets.load_asset(CUBE_ASSET_PATH)
    plane_mesh = assets.load_asset(PLANE_ASSET_PATH)
    if not cube_mesh:
        raise RuntimeError(f"Could not load engine cube mesh: {CUBE_ASSET_PATH}")
    if not plane_mesh:
        raise RuntimeError(f"Could not load engine plane mesh: {PLANE_ASSET_PATH}")

    # Ground plane: 100 x 100 base mesh scaled to 2,000 x 2,000 units.
    floor = get_or_spawn_actor(
        "CubeTest_GroundPlane",
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    floor_mesh = floor.get_component_by_class(unreal.StaticMeshComponent)
    if not floor_mesh:
        raise RuntimeError("Ground plane actor has no StaticMeshComponent.")
    floor_mesh.set_static_mesh(plane_mesh)
    floor.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
    floor.set_actor_scale3d(unreal.Vector(20.0, 20.0, 1.0))

    # Cube rests on the ground plane: 100 units tall, center at Z=50.
    cube = get_or_spawn_actor(
        "TestCube",
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 50.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    cube_mesh_component = cube.get_component_by_class(unreal.StaticMeshComponent)
    if not cube_mesh_component:
        raise RuntimeError("Cube actor has no StaticMeshComponent.")
    cube_mesh_component.set_static_mesh(cube_mesh)
    cube.set_actor_location(unreal.Vector(0.0, 0.0, 50.0), False, False)

    # Atmosphere makes the scene's background sky visible.
    sky_atmosphere = get_or_spawn_actor(
        "CubeTest_SkyAtmosphere",
        unreal.SkyAtmosphere,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )

    # Sun / key light: cast dynamic shadows and participate in the atmosphere.
    sun = get_or_spawn_actor(
        "CubeTest_Sun_DirectionalLight",
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 500.0),
        unreal.Rotator(-45.0, -35.0, 0.0),
    )
    sun.set_actor_rotation(unreal.Rotator(-45.0, -35.0, 0.0), False)
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if not sun_component:
        raise RuntimeError("Directional light has no DirectionalLightComponent.")
    sun_component.set_editor_property("intensity", 30000.0)
    sun_component.set_editor_property("cast_shadows", True)
    sun_component.set_editor_property("atmosphere_sun_light", True)
    sun_component.set_editor_property("atmosphere_sun_light_index", 0)
    set_optional_property(sun_component, "mobility", unreal.ComponentMobility.MOVABLE)

    # Soft ambient fill so the cube's shadow side is not completely black.
    sky = get_or_spawn_actor(
        "CubeTest_SkyLight",
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 300.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    if not sky_component:
        raise RuntimeError("Sky light has no SkyLightComponent.")
    sky_component.set_editor_property("intensity", 1.0)
    sky_component.set_editor_property("real_time_capture", True)
    set_optional_property(sky_component, "lower_hemisphere_is_black", False)
    set_optional_property(sky_component, "mobility", unreal.ComponentMobility.MOVABLE)

    # Frame the viewport around the cube for a useful first look.
    try:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(
            unreal.Vector(350.0, -500.0, 300.0),
            unreal.Rotator(-25.0, 125.0, 0.0),
        )
    except Exception as exc:
        unreal.log_warning(f"Could not set viewport camera: {exc}")

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Scene was built, but saving the level failed.")

    unreal.log(
        f"Saved {LEVEL_PATH}: TestCube, ground plane, Directional Light, "
        "Sky Light, and Sky Atmosphere are present."
    )
    return cube


create_or_repair_cube_test_level()
