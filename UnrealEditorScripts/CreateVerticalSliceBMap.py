"""Create /Game/Maps/VerticalSlice_B without modifying any existing map.

Run from Unreal Editor's Python console after compiling MyProject3Editor.
The script refuses to overwrite an existing VerticalSlice_B.umap.
"""
import os
import unreal

MAP_PATH = "/Game/DemoAssets/Maps/VerticalSlice_B"
PREFIX = "VSliceB_"


def log(message):
    unreal.log("[VerticalSlice_B] " + str(message))


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist("/Game/DemoAssets/Maps"):
        unreal.EditorAssetLibrary.make_directory("/Game/DemoAssets/Maps")

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError(
            MAP_PATH + " already exists. Stopping instead of overwriting it."
        )

    actor_class = unreal.load_class(None, "/Script/MyProject3.VerticalSliceBWorldActor")
    if not actor_class:
        raise RuntimeError(
            "Could not load /Script/MyProject3.VerticalSliceBWorldActor. "
            "Compile MyProject3Editor and restart the editor first."
        )

    # Only create a brand-new map. Never load or save VerticalSlice_A here.
    if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
        raise RuntimeError("Could not create " + MAP_PATH)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("Editor world is unavailable after new_level().")

    # Basic lighting, placed for the demo map only.
    unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 2500),
        unreal.Rotator(-45, -30, 0)
    )
    unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0, 0, 1800),
        unreal.Rotator(0, 0, 0)
    )

    # Player start above the central camp pad.
    start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0, 0, 450),
        unreal.Rotator(0, 0, 0)
    )
    if not start:
        raise RuntimeError("Failed to create PlayerStart.")

    demo = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0)
    )
    if not demo:
        raise RuntimeError("Failed to spawn VerticalSliceBWorldActor.")

    demo.set_actor_label(PREFIX + "WorldBootstrap")
    start.set_actor_label(PREFIX + "PlayerStart")

    # Save only the newly-created map.
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save " + MAP_PATH)

    # Set a useful editor viewport overview; this does not affect gameplay camera.
    try:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        location = unreal.Vector(0, -19000, 14500)
        target = unreal.Vector(0, 0, 0)
        rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
        editor_subsystem.set_level_viewport_camera_info(location, rotation)
    except Exception as exc:
        log("Viewport framing skipped: " + str(exc))

    log("Created and saved " + MAP_PATH)
    log("Actor placed: " + demo.get_actor_label())
    log("Open this map and press Play. Send the Output Log if setup errors appear.")


main()
