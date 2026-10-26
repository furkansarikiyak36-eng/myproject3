"""Create /Game/DemoAssets/Maps/VerticalSlice_C: the creek-side clearing.

Run inside the editor (Output Log -> Python):
    py "C:/Users/merts/Downloads/MyProject3/UnrealEditorScripts/CreateVerticalSliceCMap.py"
or headless:
    UnrealEditor-Cmd.exe MyProject3.uproject -run=pythonscript -script=<this file>
Never touches VerticalSlice_A or VerticalSlice_B. Re-running opens the map.
"""
import unreal

MAP_PATH = "/Game/DemoAssets/Maps/VerticalSlice_C"


def set_props(obj, **props):
    for name, value in props.items():
        try:
            obj.set_editor_property(name, value)
        except Exception as exc:  # A property name changed between engine versions.
            unreal.log_warning("[VerticalSlice_C] %s.%s skipped: %s" % (obj.get_name(), name, exc))


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorLevelLibrary.load_level(MAP_PATH)
        unreal.log("[VerticalSlice_C] Map exists; opened it.")
        return

    world_class = unreal.load_class(None, "/Script/MyProject3.VerticalSliceCWorldActor")
    game_mode = unreal.load_class(None, "/Script/MyProject3.LogSnapGameMode")
    if not world_class or not game_mode:
        raise RuntimeError("Compile MyProject3Editor (with VerticalSliceCWorldActor) first.")
    if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
        raise RuntimeError("Could not create " + MAP_PATH)

    spawn = unreal.EditorLevelLibrary.spawn_actor_from_class

    # Sky, sun, clouds and haze; the world actor moves the sun through the day.
    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(roll=0.0, pitch=-40.0, yaw=120.0))
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    set_props(sun.light_component, atmosphere_sun_light=True, intensity=7.0,
              light_color=unreal.Color(255, 240, 220, 255), dynamic_shadow_distance_movable_light=12000.0)
    sky_light = spawn(unreal.SkyLight, unreal.Vector(0, 0, 2000), unreal.Rotator(0, 0, 0))
    sky_light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    set_props(sky_light.light_component, real_time_capture=True, intensity=1.0)
    spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    spawn(unreal.VolumetricCloud, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -200), unreal.Rotator(0, 0, 0))
    set_props(fog.component, fog_density=0.025, fog_height_falloff=0.35, enable_volumetric_fog=True)
    post = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    set_props(post, unbound=True)

    # Start by the dead fire, looking at the unfinished cabin.
    spawn(unreal.PlayerStart, unreal.Vector(0, -450, 150), unreal.Rotator(roll=0.0, pitch=0.0, yaw=148.0))
    world_actor = spawn(world_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    world_actor.set_actor_label("VerticalSliceC_World")

    settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    set_props(settings, default_game_mode=game_mode)

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save " + MAP_PATH)
    unreal.log("[VerticalSlice_C] Created " + MAP_PATH)


main()
