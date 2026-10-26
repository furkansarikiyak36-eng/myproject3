"""One-time destructive cleanup for /Game/Maps/VerticalSlice_B.

Keeps exactly the actor labelled VSliceB_Presentation_FallbackGround and destroys
all other actors in that level. Does not delete assets or modify other maps.
Run from Unreal Editor's Execute Python Script after reviewing the map backup.
"""
import unreal

MAP_PATH = "/Game/Maps/VerticalSlice_B"
KEEP_LABEL = "VSliceB_Presentation_FallbackGround"
LOG_PREFIX = "[VerticalSlice_B_Cleanup] "


def log(message):
    unreal.log(LOG_PREFIX + str(message))


def main():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Map does not exist: " + MAP_PATH)

    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load map: " + MAP_PATH)

    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    keepers = [a for a in actors if a and a.get_actor_label() == KEEP_LABEL]

    # Fail closed: never delete anything unless the exact preservation target exists once.
    if len(keepers) != 1:
        raise RuntimeError(
            "Safety stop: expected exactly one actor labelled '" + KEEP_LABEL +
            "', found " + str(len(keepers)) + ". Nothing was deleted."
        )

    keeper = keepers[0]
    deleted = 0
    failed = []

    for actor in actors:
        if not actor or actor == keeper:
            continue
        label = actor.get_actor_label()
        class_name = actor.get_class().get_name()
        if unreal.EditorLevelLibrary.destroy_actor(actor):
            deleted += 1
            log("Deleted: " + label + " [" + class_name + "]")
        else:
            failed.append(label + " [" + class_name + "]")

    if failed:
        raise RuntimeError(
            "Cleanup incomplete; " + str(len(failed)) +
            " actors could not be deleted: " + ", ".join(failed) +
            ". Map was not saved."
        )

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("All other actors were deleted in editor memory, but map save failed.")

    log("Saved " + MAP_PATH + ". Preserved only: " + KEEP_LABEL)
    log("Deleted " + str(deleted) + " actors. Other maps and asset files were not touched.")


main()
