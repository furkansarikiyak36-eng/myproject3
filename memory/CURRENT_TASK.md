# Current task (checkpoint)

**Read this file first.** Status: open. Continue from “Next step”.

## Rules
1. Keep work on `game/p00-adopt-group-a`; do not modify `main`.
2. Backend/mechanics first. Do not place scene actors or alter physical snap geometry.
3. P10 physical assembly is frozen.
4. Write focused automation-test source with behaviour changes; never claim it passed unless executed.
5. Keep the result at `code-written` if Unreal build/test is unavailable. No blind guesses about Unreal APIs; inspect headers first.
6. Update `memory/PLAN_STATUS.md`, `memory/ROADMAP.md`, `memory/STATE.md`, and a dated session note as implementation advances.

## Task

Status: `open`
Goal: Analyse the user's World Partition test scenes (may be heavy) and fix them; unify the RAW heightmaps from the landscape zip in Content into one heightmap; build the scene that counts as the game world.
Asked by the user (2026-11-01): "world partition sahneleri ağırlaştırılmış olabilir analiz et ve düzelt; content içine landscape zipinde raw mapler ekledim, bütün değil, düzeltip bana world sayılacak sahneyi kur"
Started: 2026-11-01 · Branch: `game/p00-adopt-group-a`

### Plan
1. [x] Find the new maps and the zip; measure sizes, external actors, landscape resolution.
2. [x] Extract RAWs outside Content (SourceArt/Heightmaps), analyse resolution/range/edges, preview.
3. [x] Merge: base height function from zones.json + RAW stamps per zone, blended seams -> one 16-bit heightmap.
4. [x] Build the world map (landscape import) - needs the editor closed for C++ build or a Python/commandlet path.
5. [ ] Fix/trim heavy scenes (with user approval for any deletion).

### Findings
- Content/__ExternalActors__ = 2.7 GB in 7 World Partition maps. Only 2 have a .umap (prototipv2 614 MB/147 files, Untitled 5 MB).
  Prototype_V1 (590 MB), Prototype_v2 (589 MB), prototypev3 (732 MB), prototip (117 MB), prototypev3_terrain (32 MB) have NO .umap:
  orphaned actor folders, ~2.06 GB dead weight. __ExternalActors__ is git-ignored (.gitignore:13): WP maps are not in git at all.
- Content/landscape-...zip 844 MB (untracked; >100 MB, cannot go to GitHub). Holds 10 x 4096x4096 uint16 LE .r16 + .rpl + TerreSculptor .TSworld.
  Extracted .r16/.rpl to SourceArt/Heightmaps/.
- Every r16 is normalised to the full 0..65535 range on its own: vertical scales are not comparable (this is why they do not fit together).
  Character: v12, v1 rolling hills; v10 gentle with channels; v2 terraces/plateaus; v3 basins+ridges; v4 rugged mountains (roughest);
  v5 flat with fine detail; v6 low dunes/flow; v7 radial mountain ridges; v8 high plateau with deep valleys.
- zones.json coordinates are the small VerticalSlice_C prototype (tens of metres); the world needs its own layout file.

### Changes
- tools/heightmaps/{analyze,build_world_heightmap}.py, Config/GameData/world_layout.json, SourceArt/World/valley.r16 (+preview, import json).
- Editor/BuildValleyWorldCommandlet (-run=BuildValleyWorld) -> Content/World/Valley.umap (non-WP, landscape 2017x2017, sun/sky/fog, PlayerStart, simulation bootstrap, LogSnapGameMode). Build OK; commandlet saved the map (exit 1 only from the pre-existing GameFeatureData asset-manager warning).

### Next step
User opens /Game/World/Valley in the editor and checks it. Ask approval: delete 5 orphaned __ExternalActors__ folders (~2.06 GB), remove the zip from Content.

### Old note
Step 3: write tools/heightmaps/build_world_heightmap.py + Config/GameData/world_layout.json (2 km valley, 2017x2017, stamps per zone, river, lake), preview. Ask the user before deleting orphaned actor folders or moving the zip.
