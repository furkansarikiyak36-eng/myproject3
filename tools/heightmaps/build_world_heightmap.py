"""Builds the world heightmap from Config/GameData/world_layout.json and the source RAWs.

Run: python tools/heightmaps/build_world_heightmap.py
Inputs : SourceArt/Heightmaps/<source>.r16 (4096x4096 uint16 LE, each normalised on its own)
Outputs: SourceArt/World/valley.r16 (size_px^2 uint16 LE, ready for Unreal Landscape import),
         SourceArt/World/valley_preview.png, SourceArt/World/valley_import.json (scale, location).

Every source map is renormalised to metres by its stamp (base + amplitude), so maps that were scaled
differently fit together; stamps blend into a radial base valley with soft masks, then the river and the
lake are carved. Same layout + same sources = same world.
"""
import json
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "SourceArt", "Heightmaps")
OUT = os.path.join(ROOT, "SourceArt", "World")
LAYOUT = os.path.join(ROOT, "Config", "GameData", "world_layout.json")

_cache = {}


def source(name):
    if name not in _cache:
        data = np.fromfile(os.path.join(SRC, name + ".r16"), dtype="<u2")
        side = int(round(len(data) ** 0.5))
        _cache[name] = data.reshape(side, side).astype(np.float32)
    return _cache[name]


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def window(name, cx, cy, size):
    """A size x size window of a source map, normalised to 0..1 by its 2nd/98th percentiles."""
    src = source(name)
    half = size // 2
    x0 = int(np.clip(cx - half, 0, src.shape[1] - size))
    y0 = int(np.clip(cy - half, 0, src.shape[0] - size))
    w = src[y0:y0 + size, x0:x0 + size]
    lo, hi = np.percentile(w, 2), np.percentile(w, 98)
    return np.clip((w - lo) / max(1.0, hi - lo), 0.0, 1.0)


def main():
    L = json.load(open(LAYOUT, encoding="utf-8"))
    n = int(L["size_px"])
    mpp = float(L["metres_per_px"])
    half = (n - 1) / 2.0
    xs = (np.arange(n) - half) * mpp
    X, Y = np.meshgrid(xs, -xs)  # image row 0 = north
    R = np.hypot(X, Y)

    b = L["base"]
    # Low-frequency noise (blurred source) bends every mask so no zone is a perfect circle.
    warp_src = window("unrelandv3", 2048, 2048, 512)
    warp = np.array(Image.fromarray((warp_src * 255).astype(np.uint8)).resize((n, n), Image.BICUBIC),
                    dtype=np.float32) / 255.0
    Rw = R * (1.0 + 0.18 * (warp - 0.5))

    # 1) Macro shape: a valley floor, an enclosing mountain rim, and smooth bumps/hollows per zone.
    rim = smoothstep(b["rim_start_m"], b["rim_end_m"], Rw)
    macro = b["floor_m"] + b["rim_height_m"] * rim
    for s in L["stamps"]:
        if s["zone"].startswith("Rim."):
            continue  # rims only add detail; the rim function carries their height
        d = np.hypot(X - s["x"], Y - s["y"]) * (1.0 + 0.25 * (warp - 0.5))
        sigma = 0.75 * s["radius"]
        macro += np.exp(-(d / sigma) ** 2) * (s["base_m"] - b["floor_m"])

    # 2) Detail: each source adds relative relief inside its warped mask; outside every mask a
    #    background relief grows from gentle on the floor to rugged on the rim.
    detail = np.zeros_like(macro)
    msum = np.zeros_like(macro)
    for s in L["stamps"]:
        r = s["radius"]
        d = np.hypot(X - s["x"], Y - s["y"]) * (1.0 + 0.3 * (warp - 0.5))
        m = smoothstep(r, r * 0.35, d)
        if s["zone"].startswith("Rim."):
            m = m * rim  # rim relief stays on the mountains, never on the valley floor
        if not np.any(m > 0):
            continue
        size = int(2 * r / mpp) + 2
        tile = window(s["source"], s["window_x"], s["window_y"], size)
        cols = np.clip(((X - s["x"]) / mpp + size / 2).astype(int), 0, size - 1)
        rows = np.clip((-(Y - s["y"]) / mpp + size / 2).astype(int), 0, size - 1)
        med = float(np.median(tile))
        rel = (tile[rows, cols] - med) / max(med, 1.0 - med, 1e-3)  # -1..1
        detail += m * rel * s["amplitude_m"]
        msum += m
    over = np.maximum(msum, 1.0)
    detail /= over
    free = np.clip(1.0 - msum, 0.0, 1.0)
    bg_gentle = window(b["detail_source"], 2048, 2048, n) - 0.5
    bg_rugged = window("unrelandv7", 2048, 2048, n) - 0.5
    bg_amp = b["detail_amplitude_m"] * 2.0 * (1 - rim) + 160.0 * rim
    background = bg_gentle * (1 - rim) + bg_rugged * rim
    H = macro + detail + free * background * bg_amp

    # River: a bed that follows the terrain a little below it and only ever runs downhill.
    rv = L["river"]
    ctrl = np.array(rv["points"], dtype=np.float64)
    ctrl = np.vstack([ctrl[0], ctrl, ctrl[-1]])
    curve = []
    for i in range(1, len(ctrl) - 2):
        p0, p1, p2, p3 = ctrl[i - 1], ctrl[i], ctrl[i + 1], ctrl[i + 2]
        for t in np.linspace(0.0, 1.0, 24, endpoint=False):
            t2, t3 = t * t, t * t * t
            curve.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
                                (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    curve.append(ctrl[-2])
    pts = np.array(curve)
    tangent = np.gradient(pts, axis=0)
    normal = np.stack([-tangent[:, 1], tangent[:, 0]], 1)
    normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-6)
    wiggle = 9.0 * np.sin(np.linspace(0.0, 7.0 * np.pi, len(pts)))
    wiggle[0] = wiggle[-1] = 0.0
    pts = pts + normal * wiggle[:, None]
    seg_len = np.hypot(*(pts[1:] - pts[:-1]).T)
    total = seg_len.sum()
    samples = []
    for (ax, ay), (bx, by), length in zip(pts[:-1], pts[1:], seg_len):
        for t in np.linspace(0.0, 1.0, max(2, int(length)), endpoint=False):
            px, py = ax + t * (bx - ax), ay + t * (by - ay)
            c = int(np.clip(round(px / mpp + half), 0, n - 1))
            r_ = int(np.clip(round(-py / mpp + half), 0, n - 1))
            samples.append(H[max(0, r_ - 6):r_ + 7, max(0, c - 6):c + 7].mean() - 3.0)
    profile = np.minimum.accumulate(np.array(samples))
    profile = np.maximum(profile, rv["end_bed_m"])
    profile[-1] = rv["end_bed_m"]
    dist = np.full(H.shape, np.inf)
    along = np.zeros(H.shape)
    walked = 0.0
    for (ax, ay), (bx, by), length in zip(pts[:-1], pts[1:], seg_len):
        vx, vy = bx - ax, by - ay
        t = np.clip(((X - ax) * vx + (Y - ay) * vy) / (length * length), 0.0, 1.0)
        d = np.hypot(X - (ax + t * vx), Y - (ay + t * vy))
        closer = d < dist
        dist[closer] = d[closer]
        along[closer] = (walked + t * length)[closer]
        walked += length
    idx = np.clip((along / total * (len(profile) - 1)).astype(int), 0, len(profile) - 1)
    bed = profile[idx]
    bank = np.clip((dist - rv["bed_halfwidth_m"]) / rv["bank_width_m"], 0.0, 1.0)
    channel = bed + rv["bank_rise_m"] * bank * bank
    near = dist < rv["bed_halfwidth_m"] + rv["bank_width_m"]
    blend = smoothstep(0.0, 1.0, bank)
    H = np.where(near & (channel < H), channel + (H - channel) * blend, H)

    # Lake: an irregular basin with a flat bottom and soft shore.
    lk = L["lake"]
    ang = np.arctan2(Y - lk["y"], X - lk["x"])
    lobes = 1.0 + 0.16 * np.sin(3 * ang + 0.7) + 0.09 * np.sin(5 * ang + 2.1)
    dl = np.hypot(X - lk["x"], Y - lk["y"]) / lobes * (1.0 + 0.2 * (warp - 0.5))
    shore = smoothstep(lk["radius"], lk["radius"] + lk["shore_m"], dl)
    basin = lk["bottom_m"] + shore * shore * (H - lk["bottom_m"])
    H = np.where(dl < lk["radius"] + lk["shore_m"], np.minimum(H, basin), H)

    H = np.clip(H, L["height_min_m"], L["height_max_m"])

    # Encode for Unreal: WorldZ(cm) = LandscapeZ + (v - 32768) * ZScale / 128.
    land = L["landscape"]
    zscale = float(land["z_scale"])
    origin = float(land["origin_z_m"])
    v = 32768.0 + (H - origin) * 100.0 * 128.0 / zscale
    if v.min() < 0 or v.max() > 65535:
        raise SystemExit(f"height out of range for z_scale {zscale}: {v.min():.0f}..{v.max():.0f}")
    os.makedirs(OUT, exist_ok=True)
    v.round().astype("<u2").tofile(os.path.join(OUT, "valley.r16"))

    # Preview: hillshade + height tint, river and lake water in blue.
    gy, gx = np.gradient(H, mpp)
    slope = np.pi / 2 - np.arctan(np.hypot(gx, gy))
    aspect = np.arctan2(-gx, gy)
    shade = np.clip(np.sin(np.radians(45)) * np.sin(slope) +
                    np.cos(np.radians(45)) * np.cos(slope) * np.cos(np.radians(315) - aspect), 0, 1)
    tint = (H - H.min()) / max(1.0, H.max() - H.min())
    rgb = np.stack([0.35 + 0.5 * tint, 0.45 + 0.35 * tint, 0.30 + 0.30 * tint], -1) * shade[..., None]
    water = (H < lk["water_m"]) | (near & (dist < rv["bed_halfwidth_m"] + 0.5))
    rgb[water] = [0.15, 0.3, 0.55]
    img = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((1008, 1008))
    img.save(os.path.join(OUT, "valley_preview.png"))

    info = {
        "file": "SourceArt/World/valley.r16", "resolution": n, "format": "uint16 little-endian",
        "landscape_scale": [land["xy_scale_cm"], land["xy_scale_cm"], zscale],
        "landscape_location_cm": [-half * mpp * 100.0, -half * mpp * 100.0, origin * 100.0],
        "height_m": [float(H.min()), float(H.max())], "lake_water_m": lk["water_m"],
    }
    json.dump(info, open(os.path.join(OUT, "valley_import.json"), "w"), indent=2)
    print(json.dumps(info))


if __name__ == "__main__":
    main()
