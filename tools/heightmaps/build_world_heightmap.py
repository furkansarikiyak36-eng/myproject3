"""Builds the world heightmap from Config/GameData/world_layout.json and the source RAWs.

Run: python tools/heightmaps/build_world_heightmap.py
Inputs : SourceArt/Heightmaps/<source>.r16 (4096x4096 uint16 LE, each normalised on its own, no real scale)
Outputs: SourceArt/World/valley.r16 (size_px^2 uint16 LE, ready for Unreal Landscape import),
         SourceArt/World/valley_preview.png, SourceArt/World/valley_import.json (scale, location).

Each source map covers a large area of its own. A region shows it at its real scale:
  metres per source px = feature_m / measured feature size of the map (px)
  height range (m)     = tan(slope_deg) * metres per px / measured steep gradient of the map
so mountains stay mountain-sized and hills hill-sized. Regions sit side by side (soft Voronoi cells with
warped borders); the transition bands between them are filled by blending and a gentle filler relief.
The world edge rises, then the river (through the bands) and the lake are carved.
Same layout + same sources = same world.
"""
import json
import os

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "SourceArt", "Heightmaps")
OUT = os.path.join(ROOT, "SourceArt", "World")
LAYOUT = os.path.join(ROOT, "Config", "GameData", "world_layout.json")

_cache = {}


def source(name):
    """Source map normalised to 0..1 (1st/99th percentile) plus its measured scale."""
    if name not in _cache:
        data = np.fromfile(os.path.join(SRC, name + ".r16"), dtype="<u2")
        side = int(round(len(data) ** 0.5))
        a = data.reshape(side, side).astype(np.float32)
        lo, hi = np.percentile(a, 1), np.percentile(a, 99)
        a = (a - lo) / max(1.0, hi - lo)
        gy, gx = np.gradient(a)
        g95 = float(np.percentile(np.hypot(gx, gy), 95))  # steep gradient, height units per px
        s = a[::4, ::4] - a[::4, ::4].mean()  # feature size = autocorrelation length
        f = np.fft.fft2(s)
        ac = np.fft.ifft2(f * np.conj(f)).real
        ac /= ac[0, 0]
        prof = (ac[0, 1:512] + ac[1:512, 0]) / 2
        corr_px = 4.0 * (1 + int(np.argmax(prof < np.exp(-1))))
        _cache[name] = (a, g95, corr_px)
    return _cache[name]


def scale_of(spec):
    """(metres per source px, full height range in m) for a region-like spec."""
    _, g95, corr_px = source(spec["source"])
    mpp = spec["feature_m"] / corr_px
    relief = np.tan(np.radians(spec["slope_deg"])) * mpp / g95
    return mpp, relief


def sample(name, X, Y, cx, cy, mpp, rotate_deg=0.0):
    """Bilinear sample of a source at world metres (X, Y), centred on world (cx, cy) at mpp m/px."""
    a = source(name)[0]
    side = a.shape[0]
    t = np.radians(rotate_deg)
    dx, dy = X - cx, Y - cy
    u = (np.cos(t) * dx - np.sin(t) * dy) / mpp + side / 2
    v = -(np.sin(t) * dx + np.cos(t) * dy) / mpp + side / 2
    u = np.clip(u, 0, side - 1.001)
    v = np.clip(v, 0, side - 1.001)
    u0, v0 = u.astype(np.int32), v.astype(np.int32)
    fu, fv = (u - u0).astype(np.float32), (v - v0).astype(np.float32)
    top = a[v0, u0] * (1 - fu) + a[v0, u0 + 1] * fu
    bot = a[v0 + 1, u0] * (1 - fu) + a[v0 + 1, u0 + 1] * fu
    return top * (1 - fv) + bot * fv


def highpass(field, keep_m, mpp_world):
    """Only the features smaller than keep_m (blur by shrinking and growing again)."""
    n = field.shape[0]
    small = max(8, int(n * mpp_world / keep_m))
    lo = Image.fromarray(field.astype(np.float32)).resize((small, small), Image.BILINEAR)
    return field - np.asarray(lo.resize((n, n), Image.BICUBIC), dtype=np.float32)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def single(L):
    """The world is one source map, whole, at 1 m/px (centre crop to the landscape size)."""
    sg = L["single"]
    n = int(L["size_px"])
    data = np.fromfile(os.path.join(SRC, sg["source"] + ".r16"), dtype="<u2")
    side = int(round(len(data) ** 0.5))
    a = data.reshape(side, side).astype(np.float32)
    o = (side - n) // 2
    a = a[o:o + n, o:o + n]
    a = (a - a.min()) / max(1.0, float(a.max() - a.min()))
    # exponent > 1 sharpens the high ground into peaks and keeps the low ground gentle.
    H = sg["base_m"] + sg["height_range_m"] * a ** sg.get("exponent", 1.0)
    if "falloff" in L:
        H = falloff(L, H)
    return H


def falloff(L, H):
    """World edge: the land climbs into a rugged mountain range in the last width_m, so the edge is
    out of reach (steep) and out of sight (the crest hides it) but looks like terrain, not a wall."""
    f = L["falloff"]
    n = H.shape[0]
    mpp_world = float(L["metres_per_px"])
    half = (n - 1) / 2.0
    xs = ((np.arange(n) - half) * mpp_world).astype(np.float32)
    X, Y = np.meshgrid(xs, -xs)
    wmpp = f["warp_feature_m"] / source("unrelandv3")[2]
    Xw = X + f["warp_m"] * (sample("unrelandv3", X, Y, 0, 0, wmpp) - 0.5) * 2
    Yw = Y + f["warp_m"] * (sample("unrelandv3", X, Y, 300, -200, wmpp, 90) - 0.5) * 2
    # Two warp scales: long bends of the range plus smaller spurs and bays.
    w2mpp = max(1.0, wmpp / 3.0)  # >= 1 m/px keeps a 4 km world inside the 4096 px source
    Xw = Xw + 0.4 * f["warp_m"] * (sample("unrelandv1", X, Y, 0, 0, w2mpp) - 0.5) * 2
    Yw = Yw + 0.4 * f["warp_m"] * (sample("unrelandv1", X, Y, -400, 250, w2mpp, 90) - 0.5) * 2
    edge_d = half * mpp_world - (np.abs(Xw) ** 4 + np.abs(Yw) ** 4) ** (1 / 4)  # rounded square
    # Along the range: wider and higher in places (peaks), narrower and lower elsewhere (saddles).
    vary = sample("unrelandv8", X, Y, 0, 0, wmpp * 1.5)
    width = f["width_m"] * (0.7 + 0.6 * vary)
    m = smoothstep(width, f["crest_m"], edge_d)
    mpp, relief = scale_of(f)
    rugged = sample(f["source"], X, Y, 0, 0, mpp) - 0.5
    # Rock detail at 1 m/px, centred, so the whole world stays inside the 4096 px source.
    rocks = highpass(sample(f["source"], X, Y, 0, 0, 1.0), 150, mpp_world)
    rise = f["rise_m"] * (0.6 + 0.8 * vary)
    # Mountains grow out of the existing land: max() keeps inner hills that are already higher.
    mountains = H + m * (rise + relief * rugged) + m * relief * 0.8 * rocks
    os.makedirs(OUT, exist_ok=True)
    (np.clip(m, 0, 1) * 255).round().astype(np.uint8).tofile(os.path.join(OUT, "edge.r8"))  # for the masks
    return np.maximum(H, mountains).astype(np.float32)


def write(L, H, regions=()):
    """Encodes H for Unreal and writes the r16, the preview and the import settings."""
    n = H.shape[0]
    mpp_world = float(L["metres_per_px"])
    half = (n - 1) / 2.0
    land = L["landscape"]
    zscale = float(land["z_scale"])
    origin = float(land["origin_z_m"])
    v = 32768.0 + (H - origin) * 100.0 * 128.0 / zscale
    if v.min() < 0 or v.max() > 65535:
        raise SystemExit(f"height out of range for z_scale {zscale}: {v.min():.0f}..{v.max():.0f}")
    os.makedirs(OUT, exist_ok=True)
    v.round().astype("<u2").tofile(os.path.join(OUT, "valley.r16"))
    gy, gx = np.gradient(H, mpp_world)
    slope_deg = np.degrees(np.arctan(np.hypot(gx, gy)))
    slope = np.pi / 2 - np.radians(slope_deg)
    aspect = np.arctan2(-gx, gy)
    shade = np.clip(np.sin(np.radians(45)) * np.sin(slope) +
                    np.cos(np.radians(45)) * np.cos(slope) * np.cos(np.radians(315) - aspect), 0, 1)
    tint = (H - H.min()) / max(1.0, H.max() - H.min())
    rgb = np.stack([0.35 + 0.5 * tint, 0.45 + 0.35 * tint, 0.30 + 0.30 * tint], -1) * shade[..., None]
    img = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((1200, 1200), Image.LANCZOS)
    img.save(os.path.join(OUT, "valley_preview.png"))
    info = {
        "file": "SourceArt/World/valley.r16", "resolution": n, "format": "uint16 little-endian",
        "landscape_scale": [land["xy_scale_cm"], land["xy_scale_cm"], zscale],
        "landscape_location_cm": [-half * mpp_world * 100.0, -half * mpp_world * 100.0, origin * 100.0],
        "height_m": [float(H.min()), float(H.max())],
        "slope_deg_median_p95_p99": [float(np.median(slope_deg)), float(np.percentile(slope_deg, 95)),
                                     float(np.percentile(slope_deg, 99))],
    }
    json.dump(info, open(os.path.join(OUT, "valley_import.json"), "w"), indent=2)
    print(json.dumps(info))


def main():
    L = json.load(open(LAYOUT, encoding="utf-8"))
    if L.get("mode") == "single":
        write(L, single(L))
        return
    n = int(L["size_px"])
    mpp_world = float(L["metres_per_px"])
    half = (n - 1) / 2.0
    xs = ((np.arange(n) - half) * mpp_world).astype(np.float32)
    X, Y = np.meshgrid(xs, -xs)  # image row 0 = north
    world_half = half * mpp_world

    # Warped coordinates so region borders wander instead of being straight lines.
    w = L["border_warp_m"]
    wmpp = L["border_warp_feature_m"] / source("unrelandv3")[2]
    Xw = X + w * (sample("unrelandv3", X, Y, 0, 0, wmpp) - 0.5) * 2
    Yw = Y + w * (sample("unrelandv3", X, Y, 300, -200, wmpp, 90) - 0.5) * 2

    # 1) Regions: each source at its own real scale, weighted by a soft Voronoi cell.
    regions = L["regions"]
    dist = np.stack([np.hypot(Xw - r["x"], Yw - r["y"]) for r in regions]).astype(np.float32)
    dmin = dist.min(0)
    T = float(L["transition_m"])
    weights = np.clip(1.0 - (dist - dmin) / T, 0.0, 1.0) ** 2
    weights /= weights.sum(0)
    H = np.zeros_like(X)
    report = []
    for i, r in enumerate(regions):
        mpp, relief = scale_of(r)
        s = sample(r["source"], X, Y, r["x"], r["y"], mpp, r.get("rotate_deg", 0.0))
        own = weights[i] > 0.5
        med = float(np.median(s[own])) if own.any() else 0.5
        # Fine erosion detail at 1 m/px from another part of the same map, so the enlarged
        # source does not look smooth; only features under detail_keep_m are kept.
        fine = sample(r["source"], X, Y, r["x"] + 700, r["y"] - 500, 1.0, r.get("rotate_deg", 0.0) + 60)
        fine_relief = np.tan(np.radians(r["slope_deg"])) / source(r["source"])[1]
        fine = highpass(fine, L["detail_keep_m"], mpp_world) * fine_relief * L["detail_strength"]
        H += weights[i] * (r["base_m"] + relief * (s - med) + fine)
        report.append(f'{r["zone"]:18s} {r["source"]:12s} {mpp:5.2f} m/px  source covers {mpp * 4096 / 1000:5.1f} km  '
                      f'height range {relief:4.0f} m')
    del dist

    # 2) Transition bands: a gentle filler relief where no region dominates.
    band = 1.0 - weights.max(0)  # 0 inside a region, up to ~0.5 on a border
    del weights
    fl = L["filler"]
    fmpp, frelief = scale_of(fl)
    H += smoothstep(0.0, 0.35, band) * frelief * (sample(fl["source"], X, Y, 0, 0, fmpp) - 0.5)

    # 3) World edge: the land rises into rugged hills so the playable area is enclosed.
    ed = L["edge"]
    edge_d = world_half - (np.abs(Xw) ** 6 + np.abs(Yw) ** 6) ** (1 / 6)  # rounded square
    e = smoothstep(ed["width_m"], 0.0, edge_d)
    empp, erelief = scale_of(ed)
    H += e * (ed["rise_m"] + erelief * (sample(ed["source"], X, Y, 0, 0, empp, 45) - 0.5))

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
    wiggle = 12.0 * np.sin(np.linspace(0.0, 9.0 * np.pi, len(pts)))
    wiggle[0] = wiggle[-1] = 0.0
    pts = pts + normal * wiggle[:, None]
    seg_len = np.hypot(*(pts[1:] - pts[:-1]).T)
    total = seg_len.sum()
    samples = []
    for (ax, ay), (bx, by), length in zip(pts[:-1], pts[1:], seg_len):
        for t in np.linspace(0.0, 1.0, max(2, int(length)), endpoint=False):
            px, py = ax + t * (bx - ax), ay + t * (by - ay)
            c = int(np.clip(round(px / mpp_world + half), 0, n - 1))
            r_ = int(np.clip(round(-py / mpp_world + half), 0, n - 1))
            samples.append(H[max(0, r_ - 6):r_ + 7, max(0, c - 6):c + 7].mean() - 3.0)
    profile = np.minimum.accumulate(np.array(samples))
    profile = np.maximum(profile, rv["end_bed_m"])
    profile[-1] = rv["end_bed_m"]
    reach = rv["bed_halfwidth_m"] + rv["bank_width_m"]
    rdist = np.full(H.shape, np.inf, dtype=np.float32)
    along = np.zeros(H.shape, dtype=np.float32)
    walked = 0.0
    for (ax, ay), (bx, by), length in zip(pts[:-1], pts[1:], seg_len):
        # Only the pixels in this segment's bounding box (plus reach) can be near it.
        c0 = int(max(0, (min(ax, bx) - reach) / mpp_world + half))
        c1 = int(min(n, (max(ax, bx) + reach) / mpp_world + half + 2))
        r0 = int(max(0, (-max(ay, by) - reach) / mpp_world + half))
        r1 = int(min(n, (-min(ay, by) + reach) / mpp_world + half + 2))
        sx, sy = X[r0:r1, c0:c1], Y[r0:r1, c0:c1]
        vx, vy = bx - ax, by - ay
        t = np.clip(((sx - ax) * vx + (sy - ay) * vy) / (length * length), 0.0, 1.0)
        d = np.hypot(sx - (ax + t * vx), sy - (ay + t * vy))
        sub_d, sub_a = rdist[r0:r1, c0:c1], along[r0:r1, c0:c1]
        closer = d < sub_d
        sub_d[closer] = d[closer]
        sub_a[closer] = (walked + t * length)[closer]
        walked += length
    idx = np.clip((along / total * (len(profile) - 1)).astype(int), 0, len(profile) - 1)
    bed = profile[idx]
    bank = np.clip((rdist - rv["bed_halfwidth_m"]) / rv["bank_width_m"], 0.0, 1.0)
    channel = bed + rv["bank_rise_m"] * bank * bank
    near = rdist < reach
    blend = smoothstep(0.0, 1.0, bank)
    H = np.where(near & (channel < H), channel + (H - channel) * blend, H)

    # Lake: an irregular basin with a flat bottom and soft shore.
    lk = L["lake"]
    ang = np.arctan2(Y - lk["y"], X - lk["x"])
    lobes = 1.0 + 0.16 * np.sin(3 * ang + 0.7) + 0.09 * np.sin(5 * ang + 2.1)
    dl = np.hypot(Xw - lk["x"], Yw - lk["y"]) / lobes
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

    # Preview: hillshade + height tint, water in blue, region names.
    gy, gx = np.gradient(H, mpp_world)
    slope = np.pi / 2 - np.arctan(np.hypot(gx, gy))
    aspect = np.arctan2(-gx, gy)
    shade = np.clip(np.sin(np.radians(45)) * np.sin(slope) +
                    np.cos(np.radians(45)) * np.cos(slope) * np.cos(np.radians(315) - aspect), 0, 1)
    tint = (H - H.min()) / max(1.0, H.max() - H.min())
    rgb = np.stack([0.35 + 0.5 * tint, 0.45 + 0.35 * tint, 0.30 + 0.30 * tint], -1) * shade[..., None]
    water = ((H < lk["water_m"]) & (dl < lk["radius"] + lk["shore_m"])) | (near & (rdist < rv["bed_halfwidth_m"] + 0.5))
    rgb[water] = [0.15, 0.3, 0.55]
    size = 1200
    img = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((size, size), Image.LANCZOS)
    draw = ImageDraw.Draw(img)
    for r in regions:
        px = (r["x"] / mpp_world + half) * size / n
        py = (-r["y"] / mpp_world + half) * size / n
        draw.text((px - 40, py), f'{r["zone"].split(".")[-1]} ({r["source"].replace("unreland", "")})', fill=(255, 255, 255))
    img.save(os.path.join(OUT, "valley_preview.png"))

    info = {
        "file": "SourceArt/World/valley.r16", "resolution": n, "format": "uint16 little-endian",
        "landscape_scale": [land["xy_scale_cm"], land["xy_scale_cm"], zscale],
        "landscape_location_cm": [-half * mpp_world * 100.0, -half * mpp_world * 100.0, origin * 100.0],
        "height_m": [float(H.min()), float(H.max())], "lake_water_m": lk["water_m"],
    }
    json.dump(info, open(os.path.join(OUT, "valley_import.json"), "w"), indent=2)
    print("\n".join(report))
    print(json.dumps(info))


if __name__ == "__main__":
    main()
