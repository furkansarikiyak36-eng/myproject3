"""Analysis maps of the world terrain, and the placement plan made from them.

Run after build_world_heightmap.py:  python tools/heightmaps/build_world_masks.py
Inputs : SourceArt/World/valley.r16, valley_import.json, edge.r8; rules in world_layout.json "masks".
Outputs: SourceArt/World/masks/
  analysis (8-bit PNG, for looking at):
    slope.png      0..90 deg
    height.png     world height, min..max
    relative.png   height against the surroundings: bright = ridge or peak, dark = valley
    wetness.png    where water gathers (flow accumulation on gentle ground)
    edge.png       the edge mountains (out of the playable area)
  plan (8-bit, 0..255 = probability per candidate; .r8 is raw, read by -run=BuildValleyWorld):
    trees.r8/.png, bushes.r8/.png, rocks.r8/.png
  plan.png         everything in one picture
Every rule is a number in world_layout.json "masks", so the plan is changed there, not in code.
"""
import json
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WORLD = os.path.join(ROOT, "SourceArt", "World")
OUT = os.path.join(WORLD, "masks")
LAYOUT = os.path.join(ROOT, "Config", "GameData", "world_layout.json")


def ramp(x, a, b):
    """0 at a, 1 at b (a > b allowed for a falling ramp), smooth in between."""
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blur(field, radius_px):
    """Wide blur by shrinking and growing again."""
    n = field.shape[0]
    small = max(4, int(n / max(1.0, radius_px)))
    img = Image.fromarray(field.astype(np.float32)).resize((small, small), Image.BILINEAR)
    return np.asarray(img.resize((n, n), Image.BICUBIC), dtype=np.float32)


def flow_accumulation(H, step):
    """D8 flow accumulation on a coarse grid: how many cells drain through each cell."""
    h = H[::step, ::step].astype(np.float64)
    n = h.shape[0]
    pad = np.pad(h, 1, constant_values=np.inf)
    best = np.zeros_like(h)
    target = np.arange(n * n).reshape(n, n)
    drop_best = np.zeros_like(h)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx == 0 and dy == 0:
                continue
            nb = pad[1 + dy:1 + dy + n, 1 + dx:1 + dx + n]
            drop = (h - nb) / (1.414 if dx and dy else 1.0)
            better = drop > drop_best
            drop_best[better] = drop[better]
            rows, cols = np.indices((n, n))
            target[better] = ((rows + dy) * n + (cols + dx))[better]
    flat_down = target.ravel()
    has_down = (drop_best > 0).ravel()
    acc = np.ones(n * n)
    for i in np.argsort(-h.ravel()):  # highest first, so each cell passes on its total
        if has_down[i]:
            acc[flat_down[i]] += acc[i]
    big = Image.fromarray(np.log1p(acc).reshape(n, n).astype(np.float32)).resize(H.shape[::-1], Image.BILINEAR)
    return np.asarray(big, dtype=np.float32)


def patches(n, scale_px, seed):
    """Smooth 0..1 noise with features of about scale_px."""
    rng = np.random.default_rng(seed)
    out = np.zeros((n, n), np.float32)
    for k, w in ((1.0, 0.65), (0.4, 0.35)):
        cells = max(2, int(n / (scale_px * k)))
        img = Image.fromarray(rng.random((cells, cells)).astype(np.float32)).resize((n, n), Image.BICUBIC)
        out += w * np.asarray(img, dtype=np.float32)
    lo, hi = np.percentile(out, 1), np.percentile(out, 99)
    return np.clip((out - lo) / (hi - lo), 0, 1)


def save8(name, field, lo=0.0, hi=1.0, raw=False):
    v = (np.clip((field - lo) / (hi - lo), 0, 1) * 255).round().astype(np.uint8)
    if raw:
        v.tofile(os.path.join(OUT, name + ".r8"))
    Image.fromarray(v).resize((1008, 1008), Image.BILINEAR).save(os.path.join(OUT, name + ".png"))


def main():
    L = json.load(open(LAYOUT, encoding="utf-8"))
    R = L["masks"]
    info = json.load(open(os.path.join(WORLD, "valley_import.json")))
    n = info["resolution"]
    zs = info["landscape_scale"][2]
    oz = info["landscape_location_cm"][2] / 100.0
    v = np.fromfile(os.path.join(WORLD, "valley.r16"), "<u2").reshape(n, n).astype(np.float32)
    H = oz + (v - 32768.0) * zs / 128.0 / 100.0  # metres
    mpp = info["landscape_scale"][0] / 100.0
    edge_path = os.path.join(WORLD, "edge.r8")
    edge = (np.fromfile(edge_path, np.uint8).reshape(n, n) / 255.0 if os.path.exists(edge_path)
            else np.zeros((n, n), np.float32))
    os.makedirs(OUT, exist_ok=True)

    # Analysis.
    gy, gx = np.gradient(H, mpp)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    relative = H - blur(H, R["relative_radius_m"] / mpp)  # + ridge / peak, - valley
    wet = flow_accumulation(H, R["wetness_step_px"])
    lo, hi = np.percentile(wet, R["wet_from_pct"]), np.percentile(wet, 99.8)
    wet = np.clip((wet - lo) / (hi - lo), 0, 1)  # only the main drainage lines
    wet = blur(wet, 12 / mpp) * ramp(slope, 15, 4)  # water gathers on gentle ground
    wet = np.clip(wet / max(1e-3, np.percentile(wet, 99)), 0, 1)
    save8("slope", slope, 0, 90)
    save8("height", H, H.min(), H.max())
    save8("relative", relative, -30, 30)
    save8("wetness", wet)
    save8("edge", edge)

    # Plan: trees. Each factor is 0..1 and they multiply.
    t = R["trees"]
    by_slope = ramp(slope, t["max_slope_deg"], t["max_slope_deg"] - t["slope_fade_deg"])
    by_height = ramp(H, t["treeline_m"], t["treeline_m"] - t["treeline_fade_m"])  # tree line
    by_exposure = 1.0 - (1.0 - t["ridge_keep"]) * ramp(relative, t["ridge_start_m"], t["ridge_full_m"])  # thin on ridges, none on peaks
    by_exposure *= ramp(relative, t["peak_m"], t["peak_m"] - 6.0)
    by_wet = 1.0 + t["wet_boost"] * wet
    # Where woods grow: noise patches, pulled towards valleys, lower slopes and wet ground.
    lowness = 1.0 - ramp(blur(relative, 60 / mpp), -15.0, 15.0)
    score = (t["noise_weight"] * patches(n, t["patch_scale_m"] / mpp, L["seed"]) + t["valley_weight"] * lowness
             + t["wet_weight"] * blur(wet, 40 / mpp))
    score /= t["noise_weight"] + t["valley_weight"] + t["wet_weight"]
    woods = ramp(score, t["patch_threshold"] - 0.06, t["patch_threshold"] + 0.06)
    woods = np.maximum(woods, t["meadow_trees"])  # a few lone trees in the meadows
    trees = np.clip(woods * by_slope * by_height * by_exposure * by_wet, 0, 1) * t["density"]
    save8("trees", trees, raw=True)

    # Bushes: forest edges, wet ground, below the tree line, not on steep faces.
    b = R["bushes"]
    edge_of_woods = np.clip(blur(woods, 25 / mpp) * (1.0 - woods) * 4.0, 0, 1)
    bushes = (b["base"] + b["forest_edge"] * edge_of_woods + b["wet"] * wet) * ramp(slope, b["max_slope_deg"], b["max_slope_deg"] - 8)
    bushes *= ramp(H, t["treeline_m"] + 40, t["treeline_m"] - 20)
    save8("bushes", np.clip(bushes, 0, 1), raw=True)

    # Rocks: steep ground, ridges and the edge mountains.
    r = R["rocks"]
    rocks = r["base"] + r["slope"] * ramp(slope, 20, 45) + r["ridge"] * ramp(relative, 5, 20) + r["edge"] * edge
    rocks *= ramp(slope, 70, 60)
    save8("rocks", np.clip(rocks, 0, 1), raw=True)

    # One picture of the plan.
    shade = 0.55 + 0.45 * np.clip(1.0 - slope / 60.0, 0, 1)
    rgb = np.stack([0.62 * shade, 0.68 * shade, 0.50 * shade], -1)  # open ground
    rgb = rgb * (1 - edge[..., None] * 0.5) + edge[..., None] * 0.5 * np.array([0.45, 0.42, 0.40])
    rgb = rgb * (1 - wet[..., None] * 0.6) + wet[..., None] * 0.6 * np.array([0.25, 0.45, 0.70])
    tr = np.clip(trees / max(1e-3, t["density"]), 0, 1)[..., None]
    rgb = rgb * (1 - tr * 0.85) + tr * 0.85 * np.array([0.08, 0.30, 0.10])
    rk = np.clip(rocks, 0, 1)[..., None] * 0.6
    rgb = rgb * (1 - rk) + rk * np.array([0.55, 0.55, 0.55])
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((1200, 1200), Image.LANCZOS).save(os.path.join(OUT, "plan.png"))

    play = edge < 0.05
    print(json.dumps({
        "tree_cover_playable_pct": round(float((tr[..., 0] > 0.3)[play].mean() * 100), 1),
        "trees_above_treeline": int(((trees > 0.01) & (H > t["treeline_m"])).sum()),
        "trees_on_peaks_pct": round(float((trees[relative > t["peak_m"]] > 0.05).mean() * 100) if (relative > t["peak_m"]).any() else 0.0, 1),
        "wet_pct": round(float((wet > 0.3)[play].mean() * 100), 1),
    }))


if __name__ == "__main__":
    main()
