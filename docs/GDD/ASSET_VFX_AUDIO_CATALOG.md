# MyProject3 — World Asset, VFX, Animation, SFX, and Music Catalogue

This is a logical production inventory. It does not list marketplace products, imported texture packs, or vendor-specific assets. Start with cheap placeholder meshes and original/basic audio placeholders. Final art and mixing are later passes.

## 1. World and gameplay asset families

| ID prefix | Family | Initial logical variants |
|---|---|---|
| WLD_ | Terrain/world | Ground, slope, cliff, cave wall, cave floor, shallow water, toxic pool, path, excavation edge |
| VEG_ | Vegetation | Tree, stump, sapling, dead tree, branch, shrub, grass, crop stages, restored plant |
| RES_ | Resource source | Boulder, rock outcrop, clay patch, gravel/sand patch, fiber plant, resin source, scrap deposit |
| LOG_ | Wood geometry | Whole log lengths, split half-round, quarter-round, hewn, groove, trough, plank, pole, joint/notch examples |
| BLD_ | Building | Support, wall, deck, roof, door/gate, ramp/stair, bridge, tunnel brace, repairable piece |
| TRP_ | Defense | Stake/trap, alarm/noisemaker, barricade, simple trigger and repairable defense |
| TOOL_ | Tools | Axe, saw, wedge, hammer, chisel, shovel, pickaxe, measuring device, lamp |
| SURV_ | Survival props | Mask/filter, suit parts, water/food container, camp/sleep gear, medical kit |
| TECH_ | Old technology | Pod wreckage, sealed equipment, recorder, battery, cable, sensor, repair station |
| STORY_ | Narrative objects | Logbook, records, markings, keys/tags, identifiable debris and points of interest |
| CHAR_ | Characters/threats | Player body/hands, basic distant silhouette, limited initial threat archetype; later creatures are deferred |
| UI_ | Interface | Inventory slot/icon, interaction prompt, carry preview, snap ghost, state/progress display, exposure meter, map marker |

For each actual asset, record stable ID, source class, expected scale, pivot/orientation, collision, material slots, LOD/performance policy if applicable, interaction type, persistence rule, and required feedback events. Do not standardize a file name before confirming the real mesh dimensions and axis.

## 2. Animation/action inventory

- Locomotion: idle, walk, jog/run, crouch if approved, jump/land, climb/step over only if needed.
- Carry: pickup, carry-ready, heavy/awkward carry, rotate/place, drop, stumble/recover only if gameplay requires it.
- Tool actions: axe swing/impact, saw stroke, wedge strike, hammer, chisel, pickaxe, shovel, repair, plant, water, harvest.
- Interaction: inspect, collect small item, equip mask/filter, swap filter, interact with recorder, use station.
- Building: preview/confirm/cancel, connect, brace/support, repair, dismantle.
- Reactions: damage, hazard/exposure warning, breath/strain, cold/wet reaction only if mechanics use them.
- Threats: warning/approach, attack, retreat, stagger, trap reaction, simple damage reaction; avoid expensive synchronized siege animation in the first pass.
- Farming: plant, water and harvest can initially use compact interactions rather than unique full-body animation for every crop.

Animations must be state-aware. The player should not perform a successful cut/place animation if the authoritative action failed.

## 3. VFX catalogue

| ID | Event | Intent |
|---|---|---|
| VFX_WOOD_DUST | Wood tool impact/cut | Show contact and progression |
| VFX_WOOD_CHIP | Wood cutting/splitting | Small localized chips |
| VFX_STONE_DUST | Stone impact | Hit strength and surface feedback |
| VFX_STONE_CRACK | Staged rock damage | Communicate intact/cracked/fractured state |
| VFX_DIG_SOIL | Soil voxel removal | Show material-specific excavation |
| VFX_DIG_ROCK | Hard terrain excavation | Show higher resistance and debris |
| VFX_LANDSLIDE_SMALL | Bounded soil/rock failure | Warning and result; avoid unbounded simulation |
| VFX_SNAP_VALID | Valid placement preview | Quiet, readable confirmation |
| VFX_SNAP_INVALID | Invalid connection | Clear invalid feedback; no noisy full-screen effect |
| VFX_BUILD_DUST | Place/repair/dismantle | Distinguish action completion |
| VFX_STRUCTURE_DAMAGE | Raid/weather damage | Readable damage state |
| VFX_RAIN_SPLASH | Rain on ground/objects | Scale to weather intensity |
| VFX_LIGHTNING | Storm flash/strike if implemented | Lighting event feedback |
| VFX_TOXIC_HAZE | Contaminated region | Persistent environmental readability |
| VFX_EXPOSURE_WARNING | Exposure threshold | Limited screen/audio signal; preserve visibility |
| VFX_CROP_STAGE | Crop growth/harvest | Communicate state change without simulating every leaf |
| VFX_RESTORATION | Regional improvement | Gradual, restrained change; not an instant green explosion |
| VFX_OLD_TECH | Device activation/repair | Distinct but restrained electrical effect |
| VFX_RAID_SIGNAL | Threat warning | World-anchored signal + UI/audio cue |

Effects should be event-driven, budgeted, and poolable where appropriate. No gameplay state may depend on an effect system successfully spawning.

## 4. SFX catalogue

### Physical materials
- Wood: light/heavy hit, cut progress, saw, split, crack, snap/joint lock, set down, drag, carry strain, drop, break.
- Stone: light/heavy hit, crack stage, fracture, pieces falling, stone pickup, stone-on-stone placement.
- Soil/voxel: shovel, pickaxe, loose soil, hard rock, gravel, tunnel support placement, support creak/collapse warning.
- Construction: ghost valid/invalid, connection, hammer/build, repair, dismantle, door/gate, bridge/rope tension where used.
- Tools/equipment: equip, stow, tool wear/break, mask equip, filter removal/install, battery/cable/device interaction.
- Inventory/UI: pickup, stack, craft start/complete/fail, equip, cannot carry, invalid action; keep UI sounds sparse.
- Survival: breathing through respirator, strained breathing, footsteps by surface, wet clothes only if readable, eating/drinking, injury response only if gameplay uses it.
- Environment: wind low/high, leaves, rain light/heavy, thunder distant/near, lightning/storm warning, snow/ice if included, water drip, cave/underground reverb.
- Threat: distant cue, approach, tripwire/alarm, barrier hit, attack, retreat, structure impact, raid end/aftermath.
- World restoration: subtle new vegetation/wildlife or environmental-system activation, only when a meaningful state changes.
- Narrative/old technology: recorder start/stop, data playback, relay activation, device startup/failure.

Every SFX event should include triggering gameplay event, surface/material variant, range, priority, concurrency/limit, attenuation intent, and whether it is 2D UI or spatial 3D.

## 5. Music and adaptive audio cues

| Cue ID | Situation | Musical direction |
|---|---|---|
| MUS_WORLD_ABANDONED | General exploration | Dark ambient, sparse texture, long gaps |
| MUS_SPACE_MEMORY | Space/capsule memory | Cold drone, subtle pulse, limited motif |
| MUS_DISCOVERY | Story evidence | Sparse piano/synth motif, no forced climax |
| MUS_TOXIC_REGION | High-risk contaminated area | Thin high-frequency tension and low drone |
| MUS_UNDERGROUND | Cave/tunnel/shelter interior | Low, close, muffled texture; silence is allowed |
| MUS_STORM_BUILD | Preparing/entering dangerous weather | Slow increase in density, not constant loudness |
| MUS_RAID_WARNING | Threat window | Short, distinct, informative cue |
| MUS_RAID_ACTIVE | Active raid | Rhythmic tension, mix must not mask threat SFX |
| MUS_SAFE_SHELTER | Secure rest | Warm but restrained ambience |
| MUS_FARMING | Calm daily work | Optional light organic texture, not mandatory looping melody |
| MUS_RESTORATION | Region reaches meaningful milestone | Small version of a recognizable hopeful motif |
| MUS_OLD_TECH | Old system awakens | Mechanical/electronic texture; not generic sci-fi blast |
| MUS_LOW_SUPPLIES | Sustained scarcity | Optional subtle adaptive layer; avoid repeated stingers |
| MUS_ENDING | Future ending | Deferred until story arc is designed |

Music should be state-based and adaptive: intensity, contamination, shelter safety, storm, and raid states can alter layers. Use stems/layers and transition rules rather than changing track every few seconds. Silence and environmental audio are intentional composition choices. Do not use placeholder loops as final music.

## 6. Audio implementation requirements

Each event gets a stable ID plus trigger conditions, priority, mix bus, spatial/2D rule, concurrency policy, intensity parameters, and fallback behavior. The gameplay state remains authoritative; audio only communicates it. Important warnings must never rely only on music.

Suggested mix buses: Master, WorldAmbience, Weather, Foley, ToolsAndConstruction, Threat, UI, Music, VoiceOrRecordings. Voice/recording content should remain limited until the story script is approved.

## 7. Production order

1. Graybox: essential impact/confirm/error sounds and restrained ambient loops.
2. Mechanics integration: correct surface/action variants, warning cues, raid signaling, exposure feedback.
3. Atmosphere pass: region-specific ambience, underground response, weather transitions, adaptive music.
4. Final pass: consistent loudness, concurrency, mixing, accessibility controls, performance checks.

Do not create final animation/VFX/SFX for a mechanic whose action states and success/failure rules are still changing.
