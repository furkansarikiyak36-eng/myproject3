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

- 2026-11-01: heavy scenes removed (9109f95). 7 World Partition actor/object folders + prototipv2/Untitled maps sent to the recycle bin (~2.7 GB); zip moved to SourceArt/Heightmaps/. Next: user checks /Game/World/Valley in the editor.
- 2026-11-01: world rebuilt at real scale (6fac1bd builder): 3 km, 9 regions (one per source map), map /Game/World/Valley 3025x3025, 53 MB. Next: user checks it in the editor; then material, water, placement.
- 2026-11-01: user: 3 km too small, use unrelandv5 alone. world_layout.json mode=single: v5 whole at 1 m/px (4033 crop, ~4x4 km), height range 120 m (a choice). Map /Game/World/World (90 MB, gitignored, rebuild with the commandlet -Map=/Game/World/World). Old /Game/World/Valley still exists (was open in the editor).
- 2026-11-01: v5 world relief raised to 300 m with peak curve 2.2 (9eb1df0); /Game/World/World rebuilt (108 MB, gitignored). Next: user checks it; then material, water, placement. Old /Game/World/Valley still exists.
- 2026-11-01: edge falloff added (world_layout.json 'falloff'): irregular mountain range in the last ~520 m, crest hides the map edge. r16 ready; map NOT rebuilt (editor had /Game/World/World open). Next: rebuild with -Map=/Game/World/World when the editor is closed.
- 2026-11-01: /Game/World/World rebuilt with the edge mountains (115 MB, local only). Next: user checks it from inside; then material, water, resource placement, optional invisible boundary.
- 2026-11-01: World map dressed: Third Person template character (game mode BP_ThirdPersonGameMode; template content copied by tools/setup_thirdperson.ps1, gitignored), slope/height colour terrain material M_WorldTerrain, stand-in forest (89.6k trees, 34.6k bushes, 11.5k rocks as HISM basic shapes; settings world_layout.json 'dressing'). Map 145 MB local. Not checked in game yet. Trees are not choppable (log mechanics need LogSnapCharacter, -GameMode=LogSnap).
- 2026-11-01: placement now planned from terrain maps: tools/heightmaps/build_world_masks.py (slope, height, relative height = ridge/valley, wetness by D8 flow, edge) -> trees/bushes/rocks plan maps; rules in world_layout.json 'masks' (tree line 380 m, no trees on peaks, thin on ridges, woods pulled to valleys/wet ground). Commandlet only samples the plans. World map rebuilt: 66k trees, 27k bushes, 17k rocks (139 MB local). Pipeline: build_world_heightmap.py -> build_world_masks.py -> -run=BuildValleyWorld -Map=/Game/World/World.
- 2026-11-01: ground + caves done: AWorldGroundActor (WorldGroundActor.h/.cpp), voxel chunk floor/columns/colours/partial edits, dig tool routes landscape hits to the ground actor (test tool without inventory on the third person pawn, key N), soil_depth + caves in build_world_masks.py, landscape on 'Ground' channel with dig mask in M_WorldTerrain. Tests 137/137. Headless run: 4 caves carved, 124 chunks, 16 open columns. NOT done: saving dug chunks (lost on reload), dig yields no items on the world ground (WorldTerrainState ledger not used there), other traces (placement) still hit the hidden landscape over dug columns, landscape LOD seams at distance. Not seen in game by the user yet.
- 2026-11-01: user: no further voxel work now; one plan. Voxel cost measured (per frame none; start-up 34-38 s from landscape traces; dig 42 ms; ~0.45 MB per chunk). Plan + numbers in memory/WORLD_PLAN.md. Next step there: 1 ground id map.
- 2026-11-01: World scene saved to GitHub: World.umap + heightmap PNGs via Git LFS, Third Person template content committed, screenshots + Docs/World/README.md (taken with -WorldShots). Screenshots show the cave entrance is broken (landscape not cut cleanly, mouth not visible) - listed as known issue (voxel frozen).
- 2026-11-02: user's Master Game Production Plan reconciled with the repo: memory/MASTER_PLAN.md (GDD, world production, contracts, dependencies, phases M0-M12, demo acceptance), SYSTEMS_REGISTER.md, OPEN_DECISIONS.md (D1-D17, waiting for the user), REFERENCE_RESEARCH.md, VERIFICATION_PLAN.md. No code changed. Next: user answers D1-D17, then M1.
- 2026-11-02: step 1 (heightmap identity cards) done: tools/heightmaps/heightmap_cards.py, SourceArt/World/cards/, Docs/World/HEIGHTMAP_KARTLARI_TR.md. Waiting: user picks 3-4 regions (step 2).
- 2026-11-02: user asked to try 5x5 km. Trial: Config/GameData/world_layout_trial5x5.json -> SourceArt/World/trial5x5/ (4 regions 2x2 ~2 km each: v5 upland NW, v8 plateau NE, v6 lake/marsh SW, v10 valley SE; v4 edge mountains; no river/lake yet). Builder got --layout/--out; edge samples stay inside the source for bigger worlds. Real World map untouched. Waiting for the user's view.
- 2026-11-02: user: regions too big, world = many smaller landscapes, test in Unreal directly. Trial now 3x3 regions (~1.3 km, features ~1/3 smaller) on 5x5 km: /Game/World/World5x5Trial (196 MB, local; commandlet -WorldDir=SourceArt/World/trial5x5, masks --world). Measured: frame 13.9 ms / GPU 12.7 ms (4 km World: 9.3 / 8.0), start-up 55 s (cave carving), 103k trees. Screenshots Docs/World/screenshots_trial5x5/.
- 2026-11-02: user asked for one test scene per heightmap (v4, v5, v7, v9=v8 file, v10, v12, v3, v1). Built with tools/heightmaps/make_test_scenes.py (whole map 1 m/px ~4 km, height by steep-slope target, no edge mountains, no caves) -> /Game/World/Test/Test_<id> (local). Comparison sheet Docs/World/test_scenes/test_scenes_sheet.jpg (screenshots washed out by exposure/fog; user to walk them in the editor). Waiting for the user's view.
- 2026-11-02: step 2 (join) done as a build: Config/GameData/world_layout_regions5.json -> SourceArt/World/regions5 -> /Game/World/World5Regions (local, ~200 MB). 5 x 5 km, the user's five regions at 1 m/px (test-scene scale): v9 plateau NE (base 260), v4 mountains NW (150), v7 volcanic E (110), v5 upland centre-W (110), v12 soft hills S (90); 450 m soft transitions, warped borders, v4 edge mountains. Measured: frame 14.8 ms / GPU 13.1 ms, start-up 57 s (caves), 100k trees. Screenshots Docs/World/screenshots_regions5/. Next: step 3 missing geography (lake, valley floor, river) after the user's look.
- 2026-11-02: step 3 (missing geography) built into /Game/World/World5Regions: river from the mountain/plateau border between v5 and v7 to the south with a wide valley floor (banks 160 m, rise 30 m), lake in the west of the soft hills (r 230 m + 130 m shore, water level 82 m). Regions mode now uses the falloff edge mountains; valley_import.json carries the lake; plan maps keep trees/bushes out of the lake bed (98k trees). No water surface yet (step 7). Not walked by the user yet.
- 2026-11-02: user: show water by colour, not by steeper valleys. Valley kept soft; AWorldWaterActor (procedural lake disc + river ribbon following the bed) with M_Water (translucent, depth fade, glossy dark blue), data from valley_import.json 'water'; plan maps keep the river strip (+12 m) free of trees. Screenshots 08_lake, 09_river added. Water looks pale from far (exposure/haze). Water plugin test still step 7.
- 2026-11-02: user: build water from a flow map. tools/heightmaps/hydrology.py (layout 'hydrology' block): priority flood on a 4 m grid (paths follow the hollow's own lows), flow accumulation, lakes capped at 5 m above a hollow's bottom (8 lakes, 0.28 km2), rivers where catchment >= 0.35 km2 (20 rivers), bed only downhill and cutting through rims (deepest 75 m, valley sides widen with the cut), terminal lake where the main river meets the edge mountains (300 m shore). Outputs: <world>/flow/*.png (accumulation, direction, rivers, lakes), water.r8 (plan maps keep trees off water + 12 m), valley_import.json water {rivers, lakes(rects)}. AWorldWaterActor draws many rivers + lake rectangles. Known: a dry cut channel next to the terminal lake.
- 2026-11-02: user: flow must come from erosion; water always downhill. hydrology.erode (layout 'erosion'): 60 stream-power steps on an 8 m grid (implicit, never cuts below its outlet), light creep, deposition into hollows, rerouted every 6 steps, faded out 250 m before the edge mountains; change added to the 1 m terrain. Result: valleys cut up to 29 m, river cuts through rims 48 m (was 75), 21 rivers, 11 lakes 0.49 km2. River water surface is a running minimum (never rises downstream). Lake/tree mask from the final terrain under each lake surface (+12 m). Build ~7 min.
- 2026-11-02: user enabled the Water plugin (WaterExtras/WaterAdvanced). BuildValleyWorld now makes Water plugin bodies from the flow map (rivers as WaterBodyRiver splines with width/depth, lakes as WaterBodyLake from traced lake outlines, one WaterZone over the map), via the editor actor factories (default materials), spawned transient then saved so the Water editor hook does not add a landscape brush (that crashed the commandlet: Slate). bAffectsLandscape=false. -SimpleWater keeps the old stand-in surfaces. Frame cost with plugin bodies 14.75 ms (was 14.8). OPEN: water surface does not render in -game screenshots (no Water log warnings); next: user opens the map in the editor to see if the zone renders/rebuilds.
