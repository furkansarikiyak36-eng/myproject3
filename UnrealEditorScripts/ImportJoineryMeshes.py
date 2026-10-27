"""Imports the notched joinery logs and gives each one a socket at its notch.

The FBX files in AssetPackages/UnrealExport/Archive are 75 cm log pieces with a
cut-out notch (cross-lap, saddle, V, half-lap, fork). The notch is found from
the mesh itself: along the log's length (X), the place where the top surface
dips lowest is a notch on top, where the bottom rises highest a notch below.

- Notch on top: socket "NotchCradle", tag NotchCradle; X points up out of the
  notch, Z across the log (the direction the crossing log will lie).
- Notch below: socket "NotchMate", tag NotchMate; X points down out of the
  notch, Z along the log.

ALogBuildPiece turns mesh sockets into joint points ("Socket.<name>"), so the
placement profiles log.notch_pair and log.station_in_notch can use them.

Run in the full editor (static mesh edits need it):
  UnrealEditor.exe MyProject3.uproject -ExecutePythonScript=<this file> -QuitAfterJoinery
"""
import os
import unreal

ROOT = os.path.normpath(os.path.join(unreal.Paths.project_dir(), "AssetPackages", "UnrealExport", "Archive"))
DEST = "/Game/DemoAssets/Logs/Joinery"
BIN_CM = 2.5


def log(text):
    unreal.log_warning("JOINERY " + text)


def import_fbx(path):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    name = os.path.splitext(os.path.basename(path))[0]
    return unreal.load_asset(DEST + "/" + name)


def vertices(mesh):
    description = mesh.get_static_mesh_description(0)
    points = []
    for index in range(description.get_vertex_count()):
        points.append(description.get_vertex_position(unreal.VertexID(index)))
    return points


def find_notch(points, box):
    """Returns (is_top, x, floor_z, depth) for the deepest notch."""
    length = box.max.x - box.min.x
    count = max(1, int(length / BIN_CM))
    tops = [None] * count
    bottoms = [None] * count
    for p in points:
        i = min(count - 1, max(0, int((p.x - box.min.x) / BIN_CM)))
        tops[i] = p.z if tops[i] is None else max(tops[i], p.z)
        bottoms[i] = p.z if bottoms[i] is None else min(bottoms[i], p.z)
    filled = [i for i in range(count) if tops[i] is not None]
    if not filled:
        return None
    top_level = max(tops[i] for i in filled)
    bottom_level = min(bottoms[i] for i in filled)
    low_top = min(filled, key=lambda i: tops[i])
    high_bottom = max(filled, key=lambda i: bottoms[i])
    top_depth = top_level - tops[low_top]
    bottom_depth = bottoms[high_bottom] - bottom_level
    centre = lambda i: box.min.x + (i + 0.5) * BIN_CM
    if top_depth >= bottom_depth:
        # The notch floor is the lowest top point; average the bins at that depth.
        floor = [i for i in filled if tops[i] <= tops[low_top] + 0.5]
        return True, sum(centre(i) for i in floor) / len(floor), tops[low_top], top_depth
    ceiling = [i for i in filled if bottoms[i] >= bottoms[high_bottom] - 0.5]
    return False, sum(centre(i) for i in ceiling) / len(ceiling), bottoms[high_bottom], bottom_depth


def set_socket(mesh, is_top, x, z):
    for old_name in ("NotchCradle", "NotchMate"):
        existing = mesh.find_socket(old_name)
        if existing:
            mesh.remove_socket(existing)
    socket = unreal.StaticMeshSocket(mesh)
    if is_top:
        socket.set_editor_property("socket_name", "NotchCradle")
        socket.set_editor_property("tag", "NotchCradle")
        # X up out of the notch, Z across the log.
        rotation = unreal.MathLibrary.make_rot_from_xz(unreal.Vector(0, 0, 1), unreal.Vector(0, 1, 0))
    else:
        socket.set_editor_property("socket_name", "NotchMate")
        socket.set_editor_property("tag", "NotchMate")
        # X down out of the notch, Z along the log.
        rotation = unreal.MathLibrary.make_rot_from_xz(unreal.Vector(0, 0, -1), unreal.Vector(1, 0, 0))
    socket.set_editor_property("relative_location", unreal.Vector(x, 0, z))
    socket.set_editor_property("relative_rotation", rotation)
    mesh.add_socket(socket)


files = sorted(f for f in os.listdir(ROOT) if f.lower().endswith(".fbx")) if os.path.isdir(ROOT) else []
log("found %d FBX in %s" % (len(files), ROOT))
for name in files:
    mesh = import_fbx(os.path.join(ROOT, name))
    if not mesh:
        log("%s: import failed" % name)
        continue
    box = mesh.get_bounding_box()
    try:
        notch = find_notch(vertices(mesh), box)
    except Exception as error:  # noqa: BLE001
        notch = None
        log("%s: vertices unreadable (%s)" % (name, error))
    if not notch:
        log("%s: no notch found; no socket" % name)
        continue
    is_top, x, z, depth = notch
    diameter = box.max.z - box.min.z
    # Only a clear notch (a quarter to a bit over half the log deep) gets a socket;
    # anything else is left for a hand-placed socket in the mesh editor.
    if not (0.25 * diameter <= depth <= 0.6 * diameter):
        for old_name in ("NotchCradle", "NotchMate"):
            existing = mesh.find_socket(old_name)
            if existing:
                mesh.remove_socket(existing)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        log("%s: notch unclear (depth %.1f of %.1f); add a NotchCradle/NotchMate socket by hand" % (name, depth, diameter))
        continue
    set_socket(mesh, is_top, x, z)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log("%s: length %.1f, %s notch at x %.1f z %.1f depth %.1f" % (
        name, box.max.x - box.min.x, "top" if is_top else "bottom", x, z, depth))

log("done")
if "-QuitAfterJoinery" in unreal.SystemLibrary.get_command_line():
    unreal.SystemLibrary.quit_editor()
