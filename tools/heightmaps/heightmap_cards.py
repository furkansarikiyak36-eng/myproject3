"""Identity cards for the source heightmaps (step 1 of the landscape method).

Run: python tools/heightmaps/heightmap_cards.py
Reads every source map (SourceArt/Heightmaps/<name>.r16 or png/<name>.png), measures its character and writes
SourceArt/World/cards/<name>.png (marked preview), cards_sheet.png and cards.json.

Heights are normalised 0..1 per map (the files store no real height), so the numbers describe shape, not metres:
- hypsometric: mean normalised height; high = upland/plateau, low = lowland/basins
- flat / steep: share of the map much flatter / steeper than its own median slope
- feature_px: typical hill-to-valley size (autocorrelation length) -> real scale = feature_m / feature_px
- peaks, sinks: local highs and closed hollows (lake/basin candidates) with prominence > 3 % of relief
- channels: share of cells where much of the map drains through (river/stream lines)
- linearity, direction: how strongly ridges/valleys run one way (0 random .. 1 parallel) and that direction
- terrace: how much of the height range sits on a few levels (stepped land)
- edges: mean normalised height of the N/E/S/W borders and how rough they are (how it can join a neighbour)
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_world_heightmap import load_raw  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "SourceArt", "World", "cards")
NAMES = ["unrelandv1", "unrelandv2", "unrelandv3", "unrelandv4", "unrelandv5", "unrelandv6",
         "unrelandv7", "unrelandv8", "unrelandv10", "unreland12"]
N = 1024  # analysis resolution (1 px = 4 source px)


def blur(a, px):
    small = max(4, int(a.shape[0] / max(1.0, px)))
    img = Image.fromarray(a.astype(np.float32)).resize((small, small), Image.BILINEAR)
    return np.asarray(img.resize(a.shape[::-1], Image.BICUBIC), dtype=np.float32)


def local_extrema(a, size, prominence, find_max):
    """Local maxima (or minima) in a size x size window that stand out by prominence."""
    pad = size // 2
    p = np.pad(a, pad, mode="edge")
    windows = np.lib.stride_tricks.sliding_window_view(p, (size, size))
    ext = windows.max(axis=(2, 3)) if find_max else windows.min(axis=(2, 3))
    opposite = windows.min(axis=(2, 3)) if find_max else windows.max(axis=(2, 3))
    hit = (a == ext) & (np.abs(ext - opposite) > prominence)
    hit[:pad, :] = hit[-pad:, :] = hit[:, :pad] = hit[:, -pad:] = False  # border hits are not real
    return np.argwhere(hit)


def flow_accumulation(h):
    n = h.shape[0]
    pad = np.pad(h.astype(np.float64), 1, constant_values=np.inf)
    rows, cols = np.indices((n, n))
    target = (rows * n + cols).ravel().copy()
    best = np.zeros(n * n)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx == 0 and dy == 0:
                continue
            drop = ((h - pad[1 + dy:1 + dy + n, 1 + dx:1 + dx + n]) / (1.414 if dx and dy else 1.0)).ravel()
            better = drop > best
            best[better] = drop[better]
            target[better] = ((rows + dy) * n + (cols + dx)).ravel()[better]
    acc = np.ones(n * n)
    has = best > 0
    for i in np.argsort(-h.ravel()):
        if has[i]:
            acc[target[i]] += acc[i]
    return acc.reshape(n, n)


def card(name):
    raw = load_raw(name).astype(np.float32)
    lo, hi = np.percentile(raw, 1), np.percentile(raw, 99)
    full = np.clip((raw - lo) / max(1.0, hi - lo), 0, 1)
    side = full.shape[0]
    h = np.asarray(Image.fromarray(full).resize((N, N), Image.BILINEAR), dtype=np.float32)

    gy, gx = np.gradient(h)
    g = np.hypot(gx, gy)
    g50 = float(np.median(g))
    s = h - h.mean()
    f = np.fft.fft2(s)
    ac = np.fft.ifft2(f * np.conj(f)).real
    ac /= ac[0, 0]
    prof = (ac[0, 1:N // 2] + ac[1:N // 2, 0]) / 2
    feature_px = float((1 + int(np.argmax(prof < np.exp(-1)))) * side / N)

    smooth = blur(h, 6)
    peaks = local_extrema(smooth, 41, 0.03, True)
    sinks = local_extrema(smooth, 41, 0.03, False)
    acc = flow_accumulation(blur(h, 2))
    channel = acc > (N * N) * 0.002

    # Linearity: coherence of the smoothed structure tensor (ridges/valleys running one way).
    sg = blur(h, 4)
    ty, tx = np.gradient(sg)
    jxx, jyy, jxy = blur(tx * tx, 24), blur(ty * ty, 24), blur(tx * ty, 24)
    energy = jxx + jyy
    coherence = np.sqrt((jxx - jyy) ** 2 + 4 * jxy ** 2) / np.maximum(energy, 1e-12)
    coherence = coherence[energy > np.median(energy)]  # only where the land has real slope
    angle = 0.5 * np.degrees(np.arctan2(2 * jxy.mean(), (jxx - jyy).mean()))  # gradient direction
    ridge_dir = (angle + 90.0) % 180.0  # ridges run across the gradient

    hist, _ = np.histogram(h, bins=64, range=(0, 1))
    hist = np.sort(hist / hist.sum())[::-1]
    terrace = float(hist[:6].sum() - 6.0 / 64)  # extra weight on the 6 most used levels

    b = 16
    edges = {k: round(float(v.mean()), 2) for k, v in
             {"N": h[:b], "S": h[-b:], "W": h[:, :b], "E": h[:, -b:]}.items()}
    edge_rough = round(float(np.mean([h[:b].std(), h[-b:].std(), h[:, :b].std(), h[:, -b:].std()])), 3)

    info = {
        "name": name,
        "hypsometric": round(float(h.mean()), 3),
        "flat_pct": round(float((g < 0.3 * g50).mean() * 100), 1),
        "steep_pct": round(float((g > 3.0 * g50).mean() * 100), 1),
        "feature_px": round(feature_px, 0),
        "peaks": int(len(peaks)),
        "sinks": int(len(sinks)),
        "channels_pct": round(float(channel.mean() * 100), 2),
        "linearity": round(float(coherence.mean()), 3),
        "ridge_direction_deg": round(float(ridge_dir), 0),
        "terrace": round(terrace, 3),
        "edges": edges,
        "edge_roughness": edge_rough,
    }

    # Marked preview: hillshade, channels blue, peaks red, sinks cyan.
    sy, sx = np.gradient(h * 60.0)
    sl = np.pi / 2 - np.arctan(np.hypot(sx, sy))
    asp = np.arctan2(-sx, sy)
    shade = np.clip(np.sin(np.radians(45)) * np.sin(sl) + np.cos(np.radians(45)) * np.cos(sl) * np.cos(np.radians(315) - asp), 0, 1)
    rgb = np.stack([0.45 + 0.45 * h, 0.5 + 0.3 * h, 0.4 + 0.3 * h], -1) * shade[..., None]
    rgb[channel] = rgb[channel] * 0.3 + np.array([0.15, 0.35, 0.8]) * 0.7
    img = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((512, 512), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    for r, c in peaks:
        x, y = c / 2, r / 2
        d.polygon([(x, y - 6), (x - 5, y + 4), (x + 5, y + 4)], outline=(230, 40, 40))
    for r, c in sinks:
        x, y = c / 2, r / 2
        d.ellipse((x - 5, y - 5, x + 5, y + 5), outline=(40, 220, 230), width=2)
    d.rectangle((0, 0, 511, 18), fill=(0, 0, 0))
    d.text((4, 3), f"{name}  hyps {info['hypsometric']}  flat {info['flat_pct']}%  steep {info['steep_pct']}%  "
                   f"peaks {info['peaks']}  sinks {info['sinks']}", fill=(255, 255, 255))
    img.save(os.path.join(OUT, name + ".png"))
    return info, img


def main():
    os.makedirs(OUT, exist_ok=True)
    cards, imgs = [], []
    for name in NAMES:
        info, img = card(name)
        cards.append(info)
        imgs.append(img)
        print(json.dumps(info))
    sheet = Image.new("RGB", (512 * 5, 512 * 2))
    for i, img in enumerate(imgs):
        sheet.paste(img, ((i % 5) * 512, (i // 5) * 512))
    sheet.resize((1600, 640), Image.LANCZOS).save(os.path.join(OUT, "cards_sheet.png"))
    json.dump(cards, open(os.path.join(OUT, "cards.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
