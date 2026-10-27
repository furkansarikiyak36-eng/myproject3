"""Creates the 450 cm (3/4 length) log meshes from the 600 cm ones.

Each SM_RawLog_600* mesh is duplicated as SM_RawLog_450* and built at 0.75 of
its length, so a full log can be cut into 3/4 + 1/4 like in Sons of the Forest.
Run: UnrealEditor-Cmd.exe MyProject3.uproject -run=pythonscript -script=<this file>
"""
import unreal

ROOT = "/Game/DemoAssets/Logs/"
SUFFIXES = ["", "_Half", "_Quarter_04", "_Hewn1", "_Hewn2", "_Kalas", "_Plank", "_Gutter", "_Trough"]
L = unreal.EditorStaticMeshLibrary
A = unreal.EditorAssetLibrary


def geom_summary(mesh):
    try:
        agg = mesh.get_editor_property("body_setup").get_editor_property("agg_geom")
        return "box=%d sphyl=%d convex=%d" % (len(agg.box_elems), len(agg.sphyl_elems), len(agg.convex_elems))
    except Exception as error:  # noqa: BLE001
        return "agg unavailable (%s)" % error


for suffix in SUFFIXES:
    source = ROOT + "SM_RawLog_600" + suffix
    target = ROOT + "SM_RawLog_450" + suffix
    if not A.does_asset_exist(source):
        unreal.log_warning("LOG450 missing source " + source)
        continue
    if not A.does_asset_exist(target):
        A.duplicate_asset(source, target)
    mesh = unreal.load_asset(target)
    settings = L.get_lod_build_settings(mesh, 0)
    settings.build_scale3d = unreal.Vector(0.75, 1.0, 1.0)
    L.set_lod_build_settings(mesh, 0, settings)
    # Rebuild simple collision for the shorter shape.
    L.remove_collisions(mesh)
    shape = unreal.ScriptingCollisionShapeType.NDOP18
    L.add_simple_collisions(mesh, shape)
    A.save_asset(target)
    box = mesh.get_bounding_box()
    unreal.log_warning("LOG450 %s x %.1f..%.1f %s (source %s)" % (target, box.min.x, box.max.x,
        geom_summary(mesh), geom_summary(unreal.load_asset(source))))


unreal.log_warning("LOG450 done")
if "-QuitAfter450" in unreal.SystemLibrary.get_command_line():
    unreal.SystemLibrary.quit_editor()
