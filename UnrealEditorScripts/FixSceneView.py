# FixSceneView.py
# Resets only the Unreal Editor perspective viewport camera.
# It does NOT move actors, edit geometry, or save the level.

import unreal
import math

def _look_at_rotation(start, target):
    # Prefer Unreal's own look-at implementation when available.
    try:
        return unreal.MathLibrary.find_look_at_rotation(start, target)
    except Exception:
        try:
            return unreal.KismetMathLibrary.find_look_at_rotation(start, target)
        except Exception:
            dx = target.x - start.x
            dy = target.y - start.y
            dz = target.z - start.z
            horizontal = math.sqrt(dx * dx + dy * dy)
            yaw = math.degrees(math.atan2(dy, dx))
            pitch = math.degrees(math.atan2(dz, horizontal))
            return unreal.Rotator(pitch, yaw, 0.0)

def main():
    # Look at the test cube near the center of the ground plane.
    target = unreal.Vector(0.0, 0.0, 50.0)
    camera_location = unreal.Vector(1450.0, -1650.0, 1150.0)
    camera_rotation = _look_at_rotation(camera_location, target)

    # UE 5.8 supported API; changes viewport camera only.
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor_subsystem is None:
        raise RuntimeError("UnrealEditorSubsystem could not be acquired.")
    editor_subsystem.set_level_viewport_camera_info(camera_location, camera_rotation)

    unreal.log("FixSceneView: viewport camera reset. Level actors and geometry were not changed.")
    unreal.log("FixSceneView: camera location = {}, rotation = {}".format(camera_location, camera_rotation))

try:
    main()
except Exception as exc:
    unreal.log_error("FixSceneView failed: {}".format(exc))
    raise
