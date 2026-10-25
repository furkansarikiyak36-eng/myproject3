"""
Unreal Engine 5.8 Editor Python script.
Creates a new level at /Game/Maps/Generated_Cube_Test and places one basic cube.

Run this file inside Unreal Editor via Tools > Execute Python Script
(or File > Execute Python Script, depending on the editor layout).

The script deliberately refuses to overwrite an existing level.
"""

import unreal

LEVEL_PATH = "/Game/Maps/Generated_Cube_Test"
MAPS_DIR = "/Game/Maps"
CUBE_ASSET_PATH = "/Engine/BasicShapes/Cube.Cube"


def create_cube_test_level():
    assets = unreal.EditorAssetLibrary

    if assets.does_asset_exist(LEVEL_PATH):
        raise RuntimeError(
            f"Level already exists: {LEVEL_PATH}. Rename it or change LEVEL_PATH before rerunning."
        )

    assets.make_directory(MAPS_DIR)

    # Create and open a fresh, empty level.
    if not unreal.EditorLevelLibrary.new_level(LEVEL_PATH):
        raise RuntimeError(f"Could not create new level: {LEVEL_PATH}")

    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)

    cube_mesh = unreal.load_object(None, CUBE_ASSET_PATH)
    if not cube_mesh:
        raise RuntimeError(f"Could not load the engine cube mesh: {CUBE_ASSET_PATH}")

    # The engine cube is 100 x 100 x 100 Unreal units. Z=50 rests it on Z=0.
    cube_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 50.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not cube_actor:
        raise RuntimeError("Could not spawn the cube actor.")

    cube_actor.set_actor_label("TestCube")
    cube_actor.static_mesh_component.set_static_mesh(cube_mesh)

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Cube was created, but saving the current level failed.")

    unreal.log(f"Created and saved {LEVEL_PATH} with actor TestCube.")
    return cube_actor


create_cube_test_level()
