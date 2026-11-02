"""One test scene per source heightmap (the whole map, 1 m/px, ~4 x 4 km, no edge mountains, no caves).

Run: python tools/heightmaps/make_test_scenes.py [names...]
For each map it writes Config/GameData/test/world_test_<id>.json, builds SourceArt/World/test_<id>/ (heightmap,
plan maps) and prints the commandlet line that makes /Game/World/Test/Test_<id>.
Height range: chosen so that the steep parts (95th percentile slope) match the map's character; v5 keeps the
setting the user approved for the World map (300 m, peak curve 2.2).
"""
import json
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_world_heightmap import load_raw  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.dirname(os.path.abspath(__file__))

# id: (source file, steep-slope target in degrees, character)
SCENES = {
    "v4": ("unrelandv4", 38, "rugged mountains"),
    "v5": ("unrelandv5", None, "rugged upland (World map settings)"),
    "v7": ("unrelandv7", 34, "volcanic cones"),
    "v9": ("unrelandv8", 30, "high plateau (v9 = v8 file)"),
    "v10": ("unrelandv10", 14, "soft valley floor"),
    "v12": ("unreland12", 18, "broad soft hills"),
    "v3": ("unrelandv3", 16, "basins"),
    "v1": ("unrelandv1", 16, "rounded hills"),
}
SIZE = 4033


def height_range(source, target_deg):
    a = load_raw(source).astype(np.float32)
    o = (a.shape[0] - SIZE) // 2
    a = a[o:o + SIZE, o:o + SIZE]
    a = (a - a.min()) / max(1.0, float(a.max() - a.min()))
    gy, gx = np.gradient(a)
    g95 = float(np.percentile(np.hypot(gx, gy), 95))
    return float(np.tan(np.radians(target_deg)) / g95)


def main():
    names = sys.argv[1:] or list(SCENES)
    base = json.load(open(os.path.join(ROOT, "Config", "GameData", "world_layout.json"), encoding="utf-8"))
    os.makedirs(os.path.join(ROOT, "Config", "GameData", "test"), exist_ok=True)
    for sid in names:
        source, target, character = SCENES[sid]
        if target is None:
            rng, exp = base["single"]["height_range_m"], base["single"]["exponent"]
        else:
            rng, exp = round(height_range(source, target), 1), 1.0
        layout = {
            "_about": f"Test scene {sid}: {character}; whole map at 1 m/px, no edge mountains, no caves.",
            "mode": "single", "seed": base["seed"], "size_px": SIZE, "metres_per_px": 1.0,
            "single": {"source": source, "base_m": 0, "height_range_m": rng, "exponent": exp},
            "landscape": {"xy_scale_cm": 100, "z_scale": 200, "origin_z_m": round(rng / 2.0)},
        }
        lpath = os.path.join(ROOT, "Config", "GameData", "test", f"world_test_{sid}.json")
        json.dump(layout, open(lpath, "w", encoding="utf-8", newline="\n"), indent=2)
        out = os.path.join(ROOT, "SourceArt", "World", f"test_{sid}")
        subprocess.run([sys.executable, os.path.join(TOOLS, "build_world_heightmap.py"), "--layout", lpath, "--out", out], check=True)
        subprocess.run([sys.executable, os.path.join(TOOLS, "build_world_masks.py"), "--world", out], check=True,
                       stdout=subprocess.DEVNULL)
        caves = os.path.join(out, "masks", "caves.json")
        if os.path.exists(caves):
            os.remove(caves)  # test scenes have no caves (fast start)
        print(f"{sid}: {character}, height range {rng} m, exponent {exp} -> "
              f"-run=BuildValleyWorld -Map=/Game/World/Test/Test_{sid} -WorldDir=SourceArt/World/test_{sid}", flush=True)


if __name__ == "__main__":
    main()
