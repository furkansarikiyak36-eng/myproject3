"""Analyses 16-bit RAW heightmaps (SourceArt/Heightmaps/*.r16) and writes hillshade previews.

Run: python tools/heightmaps/analyze_heightmaps.py [output_dir]
Prints per map: value range, mean, roughness (mean slope), edge levels; writes <name>.png previews
and contact_sheet.png into output_dir (default SourceArt/Heightmaps/previews).
"""
import glob
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "SourceArt", "Heightmaps")


def load_r16(path, size=4096):
    data = np.fromfile(path, dtype="<u2")
    side = int(round(len(data) ** 0.5))
    return data.reshape(side, side).astype(np.float32)


def hillshade(h, z_scale=1.0):
    gy, gx = np.gradient(h * z_scale)
    slope = np.pi / 2 - np.arctan(np.hypot(gx, gy))
    aspect = np.arctan2(-gx, gy)
    az, alt = np.deg2rad(315), np.deg2rad(45)
    shade = np.sin(alt) * np.sin(slope) + np.cos(alt) * np.cos(slope) * np.cos(az - aspect)
    return np.clip(shade, 0, 1)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(SRC, "previews")
    os.makedirs(out, exist_ok=True)
    tiles = []
    for path in sorted(glob.glob(os.path.join(SRC, "*.r16"))):
        name = os.path.splitext(os.path.basename(path))[0]
        h = load_r16(path)
        small = h[::8, ::8]  # 512 x 512
        gy, gx = np.gradient(small)
        rough = float(np.mean(np.hypot(gx, gy)))
        edges = {k: float(v.mean()) for k, v in
                 {"N": h[0], "S": h[-1], "W": h[:, 0], "E": h[:, -1]}.items()}
        print(f"{name:12s} min {h.min():6.0f} max {h.max():6.0f} mean {h.mean():6.0f} "
              f"range {h.max() - h.min():6.0f} rough {rough:6.1f} edges "
              + " ".join(f"{k}{v:6.0f}" for k, v in edges.items()))
        norm = (small - small.min()) / max(1.0, small.max() - small.min())
        shade = hillshade(norm * 60.0)
        img = (0.55 * shade + 0.45 * norm) * 255
        tile = Image.fromarray(img.astype(np.uint8)).resize((256, 256))
        tile.save(os.path.join(out, name + ".png"))
        tiles.append((name, tile))
    cols = 5
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("L", (cols * 260, rows * 280), 255)
    draw = ImageDraw.Draw(sheet)
    for i, (name, tile) in enumerate(tiles):
        x, y = (i % cols) * 260 + 2, (i // cols) * 280 + 2
        sheet.paste(tile, (x, y + 20))
        draw.text((x + 4, y + 4), name, fill=0)
    sheet.save(os.path.join(out, "contact_sheet.png"))
    print("previews:", out)


if __name__ == "__main__":
    main()
