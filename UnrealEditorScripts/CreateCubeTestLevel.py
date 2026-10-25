"""
Unreal Editor Python script: create a new level and place one basic cube.

Run inside Unreal Editor's Python environment:
    Tools > Execute Python Script...
Requires the Python Editor Script Plugin (and Editor Scripting Utilities where applicable).

Creates: /Game/Maps/Generated_Cube_Test
The script intentionally refuses to overwrite an existing level.
"""

import unreal

LEVEL_PATH = "/Game/Maps/Generated_Cube_Test"
CUBE_ASSET_PATH = "/Engine/BasicShapes/Cube.Cube"


def create_cube_test_level():
    assets = unreal.EditorAssetLibrary

    if assets.does_asset_exist(LEVEL_PATH):
        raise RuntimeError(
            f"Level already exists at {LEVEL_PATH}. Rename or move it before running this script."
        )

    assets.make_directory("/Game/Maps")

    created = unreal.EditorLevelLibrary.new_level(LEVEL_PATH)
    if created is False:
        raise RuntimeError(f"Could not create level: {LEVEL_PATH}")

    # Make sure the new level is the active editor level.
    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)

    cube_mesh = unreal.load_object(None, CUBE_ASSET_PATH)
    if not cube_mesh:
        raise RuntimeError(f"Could not load engine cube asset: {CUBE_ASSET_PATH}")

    cube_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not cube_actor:
        raise RuntimeError("Could not spawn StaticMeshActor.")

    cube_actor.set_actor_label("TestCube")
    cube_actor.static_mesh_component.set_static_mesh(cube_mesh)

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log(f"Created {LEVEL_PATH} with one cube actor.")
    return cube_actor


create_cube_test_level()
