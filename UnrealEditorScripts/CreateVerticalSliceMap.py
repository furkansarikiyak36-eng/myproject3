# CreateVerticalSliceMap.py
# Unreal Engine 5.8 Editor Python script (DEMO-01).
#
# Creates /Game/DemoAssets/Maps/VerticalSlice_A: lighting, sky, a player start
# and one AVerticalSliceDemoActor, which builds the playable slice at BeginPlay
# (voxel ground, logs, stones, sticks, stone footing with a log, fire).
# Prerequisites: compiled MyProject3Editor and BuildLogAssetManifest.py run once
# so the log meshes exist under /Game/DemoAssets/Logs.
#
# Run headless:
#   UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript
#       -script="<Project>/UnrealEditorScripts/CreateVerticalSliceMap.py"

import unreal

MAP_PATH = "/Game/DemoAssets/Maps/VerticalSlice_A"


def log(message):
    unreal.log("[CreateVerticalSliceMap] " + str(message))


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAP_PATH)
    if not level_editor.new_level(MAP_PATH):
        unreal.log_error("[CreateVerticalSliceMap] could not create " + MAP_PATH)
        return

    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800),
                                        unreal.Rotator(0, -42, 35))
    sun.set_actor_label("Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("intensity", 8.0)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")
    sky_light = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600))
    sky_light.set_actor_label("SkyLight")
    sky_light.light_component.set_editor_property("real_time_capture", True)
    actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)).set_actor_label("Fog")

    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-700, 150, 120),
                                          unreal.Rotator(0, -8, 0))
    start.set_actor_label("PlayerStart")

    demo = actors.spawn_actor_from_class(unreal.VerticalSliceDemoActor, unreal.Vector(0, 0, 0))
    demo.set_actor_label("VerticalSliceDemo")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.LogSnapGameMode)

    level_editor.save_current_level()
    log("created " + MAP_PATH)


main()
