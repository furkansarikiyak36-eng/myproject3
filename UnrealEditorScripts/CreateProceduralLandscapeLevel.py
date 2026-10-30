"""
Create or reset /Game/Maps/Procedural_Landscape:
An empty, clean scene with a procedural terrain/landscape environment,
photorealistic atmosphere and lighting (Lumen, Sky Atmosphere, Volumetric Clouds),
and the procedural world generation actor configured for survival building.

Execution:
Inside Unreal Editor (Output Log -> Python):
    py "C:/Users/merts/Downloads/MyProject3/UnrealEditorScripts/CreateProceduralLandscapeLevel.py"
or via Tools > Execute Python Script.
"""

import unreal

MAP_PATH = "/Game/Maps/Procedural_Landscape"
MAPS_DIR = "/Game/Maps"


def set_props(obj, **props):
    for name, value in props.items():
        try:
            obj.set_editor_property(name, value)
        except Exception as exc:
            unreal.log_warning(f"[ProceduralLandscape] {obj.get_name()}.{name} skipped: {exc}")


def main():
    unreal.log("==================================================")
    unreal.log("[ProceduralLandscape] Starting Procedural Landscape setup...")

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal_editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    # Ensure /Game/Maps directory exists
    if not unreal.EditorAssetLibrary.does_directory_exist(MAPS_DIR):
        unreal.EditorAssetLibrary.make_directory(MAPS_DIR)

    # 1. Create or open the new level
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.log(f"[ProceduralLandscape] Loading existing level: {MAP_PATH}")
        if not level_subsystem.load_level(MAP_PATH):
            raise RuntimeError(f"Could not load level: {MAP_PATH}")
        # Clean existing level actors for a clean rebuild
        actors = list(actor_subsystem.get_all_level_actors()) if actor_subsystem else []
        for a in actors:
            if a:
                actor_subsystem.destroy_actor(a)
        unreal.log("[ProceduralLandscape] Cleared old actors for fresh reconstruction.")
    else:
        unreal.log(f"[ProceduralLandscape] Creating new clean level: {MAP_PATH}")
        if not level_subsystem.new_level(MAP_PATH):
            raise RuntimeError(f"Could not create new level: {MAP_PATH}")

    spawn = actor_subsystem.spawn_actor_from_class if actor_subsystem else unreal.EditorLevelLibrary.spawn_actor_from_class

    # 2. Lighting and Atmosphere
    unreal.log("[ProceduralLandscape] Setting up atmosphere, dynamic sun, sky light, clouds and fog...")

    # Sun / Directional Light
    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(roll=0.0, pitch=-38.0, yaw=125.0))
    if sun:
        sun.set_actor_label("Procedural_DirectionalSun")
        sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        set_props(
            sun.light_component,
            atmosphere_sun_light=True,
            atmosphere_sun_light_index=0,
            intensity=8.5,
            light_color=unreal.Color(255, 245, 230, 255),
            dynamic_shadow_distance_movable_light=25000.0,
            cast_shadows=True,
            cast_cloud_shadows=True
        )

    # Sky Light
    sky_light = spawn(unreal.SkyLight, unreal.Vector(0, 0, 2500), unreal.Rotator(0, 0, 0))
    if sky_light:
        sky_light.set_actor_label("Procedural_SkyLight")
        sky_light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        set_props(sky_light.light_component, real_time_capture=True, intensity=1.2)

    # Sky Atmosphere
    sky_atmosphere = spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if sky_atmosphere:
        sky_atmosphere.set_actor_label("Procedural_SkyAtmosphere")

    # Volumetric Cloud
    volumetric_cloud = spawn(unreal.VolumetricCloud, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if volumetric_cloud:
        volumetric_cloud.set_actor_label("Procedural_VolumetricCloud")

    # Exponential Height Fog with Volumetric Fog enabled
    fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -200), unreal.Rotator(0, 0, 0))
    if fog:
        fog.set_actor_label("Procedural_HeightFog")
        set_props(
            fog.component,
            fog_density=0.018,
            fog_height_falloff=0.28,
            enable_volumetric_fog=True,
            volumetric_fog_distance=15000.0
        )

    # Post Process Volume (Lumen enabled)
    post_process = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if post_process:
        post_process.set_actor_label("Procedural_PostProcess")
        set_props(post_process, unbound=True)

    # 3. Procedural Landscape World Actor & Game Mode
    world_class = unreal.load_class(None, "/Script/MyProject3.VerticalSliceCWorldActor")
    game_mode = unreal.load_class(None, "/Script/MyProject3.LogSnapGameMode")

    if not world_class:
        unreal.log_warning("[ProceduralLandscape] VerticalSliceCWorldActor not found. Procedural ground actor skipped.")
    else:
        unreal.log("[ProceduralLandscape] Spawning Procedural Landscape World Actor...")
        world_actor = spawn(world_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        if world_actor:
            world_actor.set_actor_label("Procedural_Landscape_World")

    # Set Default GameMode in World Settings
    if game_mode and unreal_editor_subsystem:
        world = unreal_editor_subsystem.get_editor_world()
        if world:
            settings = world.get_world_settings()
            set_props(settings, default_game_mode=game_mode)

    # 4. Player Start (Looking scenic over the clearing and hills)
    player_start = spawn(unreal.PlayerStart, unreal.Vector(0, -450, 150), unreal.Rotator(roll=0.0, pitch=0.0, yaw=145.0))
    if player_start:
        player_start.set_actor_label("Procedural_PlayerStart")

    # 5. Position Editor Viewport Camera
    try:
        cam_pos = unreal.Vector(-1800, -2200, 1200)
        cam_rot = unreal.MathLibrary.find_look_at_rotation(cam_pos, unreal.Vector(0, 0, 100))
        unreal_editor_subsystem.set_level_viewport_camera_info(cam_pos, cam_rot)
    except Exception as exc:
        unreal.log_warning(f"[ProceduralLandscape] Could not adjust viewport camera: {exc}")

    # 6. Save level
    if level_subsystem.save_current_level():
        unreal.log(f"[ProceduralLandscape] Level successfully created and saved to: {MAP_PATH}")
    else:
        unreal.log_warning(f"[ProceduralLandscape] Failed to automatically save {MAP_PATH}; please save manually (Ctrl+S).")

    unreal.log("[ProceduralLandscape] SUCCESS! Procedural landscape scene is ready.")
    unreal.log("==================================================")


if __name__ == "__main__":
    main()
