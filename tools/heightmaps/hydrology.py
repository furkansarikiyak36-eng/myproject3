"""Water from the land itself: flow map, lakes and rivers.

Used by build_world_heightmap.py when the layout has a "hydrology" block. On a coarse grid (grid_m) of the
terrain before the edge mountains:
1. depression filling (priority flood from the edge of the playable area): every cell gets the level at
   which water can drain away; where that level is above the ground, water collects
2. flow direction (D8, steepest descent on the filled surface) and flow accumulation (catchment area)
3. lakes: filled hollows deeper than lake_min_depth_m and larger than lake_min_area_km2;
   a terminal lake where the biggest river reaches the edge mountains
4. rivers: cells whose catchment exceeds min_catchment_km2, traced downstream into polylines; width and
   depth grow with the square root of the catchment; the bed only ever goes down
Rivers and lakes are carved into the final terrain; outputs: the flow map images (<out>/flow/), water.r8
(1 = water or a 12 m bank strip, for the plan maps) and the water surface data for Unreal.
"""
import heapq
import os
from collections import deque

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

NB = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def priority_flood(h, outflow):
    """Priority flood from the outflow edge. Returns the filled level of every cell, its downstream cell
    (the one it was reached from) and the visiting order. Inside a filled hollow the original lowest ground is
    visited first, so paths follow the hollow's own valleys instead of running straight to the spill point."""
    n = h.shape[0]
    filled = h.astype(np.float64).copy()
    parent = np.full(n * n, -1, dtype=np.int64)
    done = outflow.copy()
    pad = np.pad(outflow, 1, constant_values=True)
    edge = outflow & (~pad[2:, 1:-1] | ~pad[:-2, 1:-1] | ~pad[1:-1, 2:] | ~pad[1:-1, :-2])
    heap = [(float(filled[r, c]), float(h[r, c]), int(r), int(c)) for r, c in np.argwhere(edge)]
    heapq.heapify(heap)
    order = []
    while heap:
        z, _, r, c = heapq.heappop(heap)
        order.append(r * n + c)
        for dr, dc in NB:
            rr, cc = r + dr, c + dc
            if 0 <= rr < n and 0 <= cc < n and not done[rr, cc]:
                done[rr, cc] = True
                if filled[rr, cc] < z:
                    filled[rr, cc] = z
                parent[rr * n + cc] = r * n + c
                heapq.heappush(heap, (float(filled[rr, cc]), float(h[rr, cc]), rr, cc))
    return filled, parent, np.array(order, dtype=np.int64)


def accumulate(parent, order, n):
    """Cells draining through each cell: later-visited cells drain into the cell that reached them."""
    acc = np.ones(n * n)
    for i in order[::-1]:
        p = parent[i]
        if p >= 0:
            acc[p] += acc[i]
    return acc.reshape(n, n)


def components(mask):
    """Connected components (8-neighbour) of a boolean mask: list of (rows, cols) arrays."""
    n = mask.shape[0]
    seen = np.zeros_like(mask)
    out = []
    for r0, c0 in np.argwhere(mask):
        if seen[r0, c0]:
            continue
        q = deque([(r0, c0)])
        seen[r0, c0] = True
        cells = []
        while q:
            r, c = q.popleft()
            cells.append((r, c))
            for dr, dc in NB:
                rr, cc = r + dr, c + dc
                if 0 <= rr < n and 0 <= cc < n and mask[rr, cc] and not seen[rr, cc]:
                    seen[rr, cc] = True
                    q.append((rr, cc))
        a = np.array(cells)
        out.append((a[:, 0], a[:, 1]))
    return out


def interior_mask(L, nc, g, n, mpp, world_half_m):
    """Coarse cells inside the edge mountains (rounded square, as the falloff without its warp)."""
    half = (n - 1) / 2.0
    rr, cc = np.indices((nc, nc))
    x = (cc * g + (g - 1) / 2.0 - half) * mpp
    y = (rr * g + (g - 1) / 2.0 - half) * mpp
    play_half = world_half_m - L.get("falloff", {}).get("width_m", 0) * 0.85
    return (np.abs(x) ** 4 + np.abs(y) ** 4) ** 0.25 < play_half


def erode(L, H, mpp, world_half_m):
    """Base erosion of the whole land before the rivers (stream power + hillslope creep + deposition).

    On a coarse grid, repeated: route the water (priority flood), let channels cut towards the cell they drain
    to in proportion to sqrt(catchment) (implicit stream-power step, so it never cuts below its outlet), smooth
    the slopes a little, and let sediment settle into closed hollows. The change is added to the fine terrain,
    which keeps its own detail. Water still only ever flows downhill: every cell drains to a lower or equal cell.
    """
    cfg = L["erosion"]
    n = H.shape[0]
    g = max(1, int(round(cfg["grid_m"] / mpp)))
    nc = n // g
    cell_m = g * mpp
    h0 = H[:nc * g, :nc * g].reshape(nc, g, nc, g).mean(axis=(1, 3)).astype(np.float64)
    h = h0.copy()
    interior = interior_mask(L, nc, g, n, mpp, world_half_m)
    rows, cols = np.divmod(np.arange(nc * nc), nc)
    hollows_before = None
    every = cfg.get("reroute_every", 1)
    for it in range(cfg["iterations"]):
        if it % every == 0:  # re-route the water now and then (the slow part)
            filled, parent, order = priority_flood(h, ~interior)
            if hollows_before is None:
                hollows_before = float(((filled - h) > 1.0).mean() * 100)
            target = np.where(interior.ravel(), parent, -1)
            area = accumulate(target, order, nc).ravel() * cell_m * cell_m
            pr, pc = np.divmod(np.maximum(target, 0), nc)
            dist = np.where((pr != rows) & (pc != cols), 1.414, 1.0) * cell_m
            F = cfg["k"] * area ** cfg["m"] / dist
        hf = h.ravel()
        for i in order:  # outlets first, so every receiver is already updated
            t = target[i]
            if t >= 0 and hf[i] > hf[t]:
                hf[i] = (hf[i] + F[i] * hf[t]) / (1.0 + F[i])
        h = hf.reshape(nc, nc)
        lap = np.pad(h, 1, mode="edge")
        lap = lap[:-2, 1:-1] + lap[2:, 1:-1] + lap[1:-1, :-2] + lap[1:-1, 2:] - 4 * h
        h = h + cfg["diffusion"] * lap * interior
        if it % every == every - 1:
            h = h + cfg["deposition"] * np.maximum(filled - h, 0.0) * interior
    filled, _, _ = priority_flood(h, ~interior)
    delta = (h - h0).astype(np.float32)
    full = np.array(Image.fromarray(delta).resize((nc * g, nc * g), Image.BICUBIC), dtype=np.float32)
    # Fade the change out over the last 250 m before the edge mountains: no step at the border.
    half = (n - 1) / 2.0
    xs = ((np.arange(nc * g) - half) * mpp).astype(np.float32)
    fx, fy = np.meshgrid(np.abs(xs), np.abs(xs))
    play_half = world_half_m - L.get("falloff", {}).get("width_m", 0) * 0.85
    edge = play_half - (fx ** 4 + fy ** 4) ** 0.25
    full *= smoothstep(0.0, 250.0, edge)
    out = H.copy()
    out[:nc * g, :nc * g] += full
    print(f"erosion: {cfg['iterations']} steps on a {cell_m:.0f} m grid, deepest cut {-delta.min():.1f} m, "
          f"most deposit {delta.max():.1f} m, closed hollows {hollows_before:.1f}% -> {((filled - h) > 1.0).mean() * 100:.1f}% of cells")
    return out


def run(L, H0, H, out_dir, mpp, world_half_m):
    """Returns the carved terrain and the water surface data (Unreal cm)."""
    cfg = L["hydrology"]
    n = H.shape[0]
    g = max(1, int(round(cfg["grid_m"] / mpp)))
    nc = n // g
    cell_m = g * mpp
    h = H0[:nc * g, :nc * g].reshape(nc, g, nc, g).mean(axis=(1, 3))
    half = (n - 1) / 2.0

    def fine(rc):  # coarse cell -> fine pixel centre
        return rc * g + (g - 1) / 2.0

    # Playable interior: inside the edge mountains (same rounded-square rule as the falloff, without warp).
    rr, cc = np.indices((nc, nc))
    x = (fine(cc) - half) * mpp
    y = (fine(rr) - half) * mpp
    play_half = world_half_m - L.get("falloff", {}).get("width_m", 0) * 0.85
    interior = (np.abs(x) ** 4 + np.abs(y) ** 4) ** 0.25 < play_half
    filled, parent, order = priority_flood(h, ~interior)
    target = np.where(interior.ravel(), parent, -1)
    acc = accumulate(target, order, nc)
    area_km2 = acc * cell_m * cell_m / 1e6
    depth = filled - h

    # Lakes: filled hollows that are deep and large enough.
    # Big hollows do not fill to their spill point (evaporation, seepage): the water stands at most
    # lake_max_depth_m above the hollow's lowest point. Rivers end where they enter a hollow.
    lake_mask = np.zeros_like(interior)
    lake_level = np.full(h.shape, np.nan)
    hollow = np.zeros_like(interior)
    lakes = []
    for rows, cols in components(interior & (depth > cfg["lake_min_depth_m"])):
        if len(rows) * cell_m * cell_m / 1e6 >= cfg["lake_min_area_km2"]:
            hollow[rows, cols] = True  # only lake-sized hollows end a river; small dips are just crossed
        level = min(float(filled[rows, cols].max()), float(h[rows, cols].min()) + cfg["lake_max_depth_m"])
        wet = h[rows, cols] < level
        if wet.sum() * cell_m * cell_m / 1e6 >= cfg["lake_min_area_km2"]:
            lake_mask[rows[wet], cols[wet]] = True
            lake_level[rows[wet], cols[wet]] = level
            lakes.append({"rows": rows[wet], "cols": cols[wet], "level": level})

    # Rivers: trace streams downstream from their heads.
    stream = interior & (area_km2 >= cfg["min_catchment_km2"])
    up = np.zeros(nc * nc, dtype=np.int32)
    idx = np.flatnonzero(stream.ravel())
    tg = target[idx]
    np.add.at(up, tg[tg >= 0], 1)
    heads = [i for i in idx if up[i] == 0]
    visited = np.zeros(nc * nc, dtype=bool)
    polylines = []
    for head in sorted(heads, key=lambda i: -filled.ravel()[i]):
        path = []
        i = head
        while i >= 0 and stream.ravel()[i]:
            path.append(i)
            if visited[i]:
                break
            visited[i] = True
            i = target[i]
        if i >= 0 and not stream.ravel()[i] and interior.ravel()[i]:
            path.append(i)
        if len(path) >= 4:
            polylines.append(path)

    # Terminal lake where the largest river leaves the playable area (against the edge mountains).
    terminal = None
    exits = [p for p in polylines if target[p[-1]] < 0 or not interior.ravel()[target[p[-1]]]]
    if exits and cfg.get("terminal_lake_radius_m", 0) > 0:
        main = max(exits, key=lambda p: acc.ravel()[p[-1]])
        back = main[max(0, len(main) - 1 - int(cfg["terminal_lake_radius_m"] * 1.4 / cell_m))]
        tr, tc = divmod(back, nc)
        terminal = (tr, tc, float(filled[tr, tc]) - 1.0)

    # Carve rivers into the final terrain; collect the water ribbons.
    Hc = H.copy()
    lake_flat = lake_mask.ravel()
    max_cut = 0.0
    rivers = []
    water_img = Image.new("L", (n, n), 0)
    wdraw = ImageDraw.Draw(water_img)
    polylines.sort(key=lambda p: -acc.ravel()[p[-1]])
    for path in polylines[:cfg.get("max_rivers", 60)]:
        r = np.array([p // nc for p in path], dtype=np.float64)
        c = np.array([p % nc for p in path], dtype=np.float64)
        if len(r) >= 9:  # smooth the 8-direction zigzag, keep the ends
            k = np.ones(9) / 9
            r[4:-4] = np.convolve(r, k, "valid")
            c[4:-4] = np.convolve(c, k, "valid")
        a = np.array([area_km2.ravel()[p] for p in path])
        hw = np.clip(cfg["width_base_m"] + cfg["width_per_sqrt_km2"] * np.sqrt(a), cfg["width_base_m"], cfg["width_max_m"]) / 2
        dp = np.clip(cfg["depth_base_m"] + cfg["depth_per_sqrt_km2"] * np.sqrt(a), cfg["depth_base_m"], cfg["depth_max_m"])
        in_lake = np.array([lake_flat[p] for p in path])
        if terminal is not None:  # the river ends where it reaches the terminal lake
            in_lake |= np.hypot(r - terminal[0], c - terminal[1]) * cell_m < cfg["terminal_lake_radius_m"]
        ground = np.array([h.ravel()[p] for p in path])
        level = np.array([filled.ravel()[p] for p in path])
        # The river cuts its own way out of closed hollows (breaching): its bed follows the ground and only
        # ever goes down; on a lake it lies at the lake level.
        lk = np.array([lake_level.ravel()[p] for p in path])
        bed = np.minimum.accumulate(np.where(in_lake, lk, ground) - dp)
        cut = np.maximum(ground - bed - dp, 0.0)
        max_cut = max(max_cut, float(cut.max()))
        surface = np.minimum.accumulate(bed + dp * 0.75)  # the water surface never rises downstream
        # Where the river cuts deep through a rim, its valley sides widen (about 40 deg), so no cliff walls.
        bw = np.maximum(cfg["bank_width_m"], cut * 1.2)
        fr, fc = fine(r), fine(c)
        pts = []
        for i in range(len(path) - 1):
            r0, c0, r1, c1 = fr[i], fc[i], fr[i + 1], fc[i + 1]
            reach = hw[i] / mpp + bw[i] / mpp + 2
            a0, a1 = int(max(0, min(r0, r1) - reach)), int(min(n, max(r0, r1) + reach + 1))
            b0, b1 = int(max(0, min(c0, c1) - reach)), int(min(n, max(c0, c1) + reach + 1))
            sr, sc = np.mgrid[a0:a1, b0:b1]
            vr, vc = r1 - r0, c1 - c0
            ln2 = max(vr * vr + vc * vc, 1e-6)
            t = np.clip(((sr - r0) * vr + (sc - c0) * vc) / ln2, 0, 1)
            d = np.hypot(sr - (r0 + t * vr), sc - (c0 + t * vc)) * mpp
            b = bed[i] + (bed[i + 1] - bed[i]) * t
            bank = np.clip((d - hw[i]) / bw[i], 0, 1)
            channel = b + (dp[i] + cfg["bank_rise_m"] + cut[i]) * bank * bank
            sub = Hc[a0:a1, b0:b1]
            near = d < hw[i] + bw[i]
            sub[:] = np.where(near & (channel < sub), channel + (sub - channel) * smoothstep(0, 1, bank), sub)
        # Ribbons only where the river runs on open ground: a hollow (lake or wet basin) ends a piece.
        piece = []
        for i in range(len(path) + 1):
            if i < len(path) and not in_lake[i]:
                piece.append(i)
                continue
            if len(piece) >= cfg.get("min_river_points", 6):
                rivers.append([[round((fc[j] - half) * mpp * 100, 1), round((fr[j] - half) * mpp * 100, 1),
                                round(surface[j] * 100, 1), round(hw[j] * 100 + 150, 1)] for j in piece])
                wdraw.line([(fc[j], fr[j]) for j in piece], fill=255,
                           width=int(2 * (hw[piece].max() + 12) / mpp), joint="curve")
            piece = []

    # Terminal lake basin and the lake surfaces.
    lake_out = []
    if terminal is not None:
        tr, tc, tlevel = terminal
        R = cfg["terminal_lake_radius_m"]
        cy, cx = fine(tr), fine(tc)
        S = cfg.get("terminal_lake_shore_m", 300)  # wide shore: no cliff where the basin meets the mountains
        span = int((R + S + 10) / mpp)
        a0, a1 = int(max(0, cy - span)), int(min(n, cy + span + 1))
        b0, b1 = int(max(0, cx - span)), int(min(n, cx + span + 1))
        yy, xx = np.mgrid[a0:a1, b0:b1]
        ang = np.arctan2(yy - cy, xx - cx)
        dist = np.hypot(yy - cy, xx - cx) * mpp / (1 + 0.15 * np.sin(3 * ang + 1.1) + 0.08 * np.sin(5 * ang))
        shore = smoothstep(R, R + S, dist)
        sub = Hc[a0:a1, b0:b1]
        basin = (tlevel - 5.0) + shore * shore * (sub - (tlevel - 5.0))
        sub[:] = np.where(dist < R + S, np.minimum(sub, basin), sub)
        cang = np.arctan2(fine(rr) - cy, fine(cc) - cx)
        cdist = np.hypot(fine(rr) - cy, fine(cc) - cx) * mpp / (1 + 0.15 * np.sin(3 * cang + 1.1) + 0.08 * np.sin(5 * cang))
        tm = cdist < R + 4
        lakes.append({"rows": np.argwhere(tm)[:, 0], "cols": np.argwhere(tm)[:, 1], "level": tlevel})
        lake_mask |= tm
    for lk in lakes:
        rects = []
        rows, cols = lk["rows"], lk["cols"]
        for row in np.unique(rows):
            cs = np.sort(cols[rows == row])
            start = prev = cs[0]
            for col in list(cs[1:]) + [None]:
                if col is not None and col == prev + 1:
                    prev = col
                    continue
                # one cell of overlap all round; the terrain hides what is above the shore
                x0 = ((start - 1) * g - half) * mpp * 100
                x1 = ((prev + 2) * g - half) * mpp * 100
                y0 = ((row - 1) * g - half) * mpp * 100
                y1 = ((row + 2) * g - half) * mpp * 100
                rects.append([round(x0, 1), round(y0, 1), round(x1, 1), round(y1, 1)])
                if col is not None:
                    start = prev = col
        lake_out.append({"z": round(lk["level"] * 100, 1), "rects": rects})
    # Lake water on the final terrain: every point under a lake surface that lies below its level.
    lake_fine = np.zeros((n, n), dtype=np.uint8)
    for lk_out in lake_out:
        z = lk_out["z"] / 100.0
        for x0, y0, x1, y1 in lk_out["rects"]:
            c0, c1 = int(max(0, x0 / 100 / mpp + half)), int(min(n, x1 / 100 / mpp + half + 1))
            r0, r1 = int(max(0, y0 / 100 / mpp + half)), int(min(n, y1 / 100 / mpp + half + 1))
            sub = lake_fine[r0:r1, c0:c1]
            sub[Hc[r0:r1, c0:c1] < z] = 255
    lake_full = Image.fromarray(lake_fine).filter(ImageFilter.MaxFilter(int(12 / mpp) | 1))  # + 12 m of shore
    water = np.maximum(np.asarray(water_img), np.asarray(lake_full))
    water.astype(np.uint8).tofile(os.path.join(out_dir, "water.r8"))

    # Flow map images.
    fdir = os.path.join(out_dir, "flow")
    os.makedirs(fdir, exist_ok=True)
    la = np.log1p(area_km2 * 1e3)
    Image.fromarray((np.clip(la / la.max(), 0, 1) * 255).astype(np.uint8)).save(os.path.join(fdir, "flow_accumulation.png"))
    tr_, tc_ = np.divmod(np.where(target >= 0, target, np.arange(nc * nc)), nc)
    dr = (tr_.reshape(nc, nc) - rr).astype(np.float32)
    dc = (tc_.reshape(nc, nc) - cc).astype(np.float32)
    rgb = np.stack([(dc + 1) * 127.5, (dr + 1) * 127.5, np.zeros_like(dr)], -1).astype(np.uint8)
    Image.fromarray(rgb).save(os.path.join(fdir, "flow_direction.png"))
    Image.fromarray((stream * 255).astype(np.uint8)).save(os.path.join(fdir, "rivers.png"))
    Image.fromarray((lake_mask * 255).astype(np.uint8)).save(os.path.join(fdir, "lakes.png"))
    print(f"hydrology: deepest cut through a rim {max_cut:.1f} m")
    print(f"hydrology: {len(rivers)} rivers (largest catchment {area_km2.max():.1f} km2), {len(lake_out)} lakes "
          f"({int(lake_mask.sum() * cell_m * cell_m / 1e4) / 100:.2f} km2), terminal lake: {terminal is not None}")
    return Hc, {"rivers": rivers, "lakes": lake_out}
