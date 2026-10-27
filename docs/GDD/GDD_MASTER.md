# MyProject3 — Master Game Design Document

**Revision:** 0.1  
**Status:** Planning baseline; not a feature-completion report  
**Engine target:** Unreal Engine 5.8  
**Project:** furkansarikiyak36-eng/myproject3

## 1. High concept

A survival and physical-construction game set on a toxic, damaged Earth after a global catastrophe. The protagonist has survived in space through automated systems. Their return capsule falls to Earth; identity, the cause of the catastrophe, and the state of the old world are initially unknown. The player discovers the truth through places, objects, environmental clues, and recoverable records rather than relying on long cinematics.

The game should offer a dangerous space and meaningful choices, not a long list of mandatory missions. The player chooses where to travel, what to build, which resources to spend, when to farm, when to hide, and which parts of the world to restore.

## 2. Target experience

- A small number of deeply connected systems is preferable to a large list of shallow, disconnected mechanics.
- Physical construction should feel tangible: pick up, carry, rotate, cut, split, align, snap, join, and repair.
- Medium-depth voxel terrain should support practical digging, tunnels, and small underground rooms without an unlimited-depth simulation.
- Transforming materials should create choices: the same wood may be needed for shelter, fuel, tools, traps, or a bridge.
- Environmental pressure should reward preparation and route choice without making every trip a forced combat encounter.
- Restoring a region should gradually change what can grow, what resources appear, how safe it becomes, and which opportunities open.

## 3. World assumptions

| Design item | Working target | Status |
|---|---|---|
| World footprint | Approximately 5–7 km² | Provisional |
| Regions | Approximately 8 distinct areas | Provisional |
| World state | Toxic/mostly lifeless at the start; gradual local recovery | Planned |
| Time structure | Daily weather/events, seasonal calendar, long-term restoration | Planned |
| Narrative delivery | Environmental story, recordings, remnants, old technology | Planned |
| First visual quality target | Simple placeholder geometry and readable silhouettes | Planned |

Do not hard-code provisional world size or region count into foundational systems. Region definitions must be data-driven so the scope can change without rewriting core logic.

## 4. Core gameplay loop

1. Read conditions: time, weather, season, contamination, terrain, resources, and threat signals.
2. Choose a self-directed goal: secure shelter, dig a room, gather a resource, grow food, explore, repair old equipment, or improve a region.
3. Transform the world: cut, split, quarry, dig, shape, craft, plant, repair, connect, or clean.
4. Manage risk and opportunity cost: time, tool wear, carrying capacity, weather exposure, noise, light, supplies, and possible raids.
5. Gain a persistent advantage: safer shelter, a reliable route, useful equipment, new production, food supply, or an improving region.
6. Reassess the changed world and choose the next goal.

Waiting is not a reward by itself. Any time-consuming process must create a planning choice or a meaningful state change.

## 5. Pillars and system depth

| Pillar | Target depth | Required characteristics |
|---|---|---|
| Wood lifecycle and physical building | Deep | State-correct cutting/splitting, physical carry/place, contextual snap, joins, construction rules |
| Voxel terrain | Medium | Digging, material hardness, saved edits, tunnels/rooms, limited support/collapse rules |
| Raids and defense | Simple but meaningful | Telegraphing, barriers, traps, repair, predictable damage rules |
| Materials and recipes | Medium | Several useful outputs per core resource; input/process/output/cost data |
| Agriculture and seasons | Data-driven medium | Prepare soil, sow, growth states, water, harvest, seasonal windows |
| Weather and toxic exposure | Medium | Visible/aural warning, preparation, equipment durability/consumables, route trade-offs |
| Restoration | Regional simulation | Region-level progress and unlocks; do not simulate every plant globally |
| Story and old technology | Progressive | Discoveries, records, repairable old-world systems; no mandatory cinematic dependency |
| Advanced industry / player-made robots | Deferred | Not required for the initial target |

## 6. Core world systems

### 6.1 Wood lifecycle
Tree/source → fall or harvest → whole log → measured cut → split half/quarter or shaped variants → construction/fuel/tool/processing use. A piece’s processing state is authoritative and persistent. A half-log must never silently become a full straight log. A groove/hollow operation is valid only for the documented parent state. Use pre-authored mesh variants initially; procedural geometry is not a prerequisite.

### 6.2 Physical construction
A shared placement kernel solves geometry, compatibility, anchor occupancy, and commit validity. Material-specific providers define socket/anchor rules; data profiles define dimensions and spacing. Preview and committed placement must use the same solution. Do not create separate snap kernels for logs, stone, sticks, or rope.

### 6.3 Voxel terrain
Treat voxel technology as an explicit engine integration decision. Required capabilities include runtime edits, saved edits, terrain/material types, interaction with placed structures, bounded memory, and UE 5.8 compatibility. Scope is tunnels and practical underground spaces, not infinite-depth voxel simulation. Initially, enemies do not dig through terrain.

### 6.4 Survival and exposure
Exposure has readable warning signs and countermeasures. Filters and protective equipment are consumable/durable resources. Clean air should first be achieved through protective gear, sealed spaces, and local remediation; planting a few seeds must not instantly restore a globally toxic atmosphere.

### 6.5 Farming and seasons
Plants use data-defined states: seed/seedling/growing/mature/harvested/dead or dormant, as appropriate. Each crop has valid sowing windows, growth requirements, water/soil conditions, outputs, and recovery rules. The seasonal calendar affects selected systems meaningfully without forcing every system to change every day.

### 6.6 Raids
Raids test prior choices, not reflexes alone. Signals provide a preparation window. Defenses, concealment, barricades, traps, escape routes, and repairs must be understandable. Damage should disrupt or cost resources, not casually erase many hours of progress. Keep AI, wave structure, and destruction scope bounded.

### 6.7 Regional restoration
Represent contamination, soil/water recovery, vegetation return, and regional opportunities as regional state variables. Local actions can improve the region over time. Later-life flora/fauna or fungal threats remain content additions, not assumptions required by the core architecture.

## 7. First integrated vertical slice

The first complete slice should contain one bounded test area and the following loop:

- Player can move and interact.
- Player can pick up and carry one whole log; drop/place it safely.
- Log processing respects the valid state-transition graph.
- At least one reliable contextual snap connection works and persists.
- One simple stone/terrain resource can be gathered or broken in stages.
- A small diggable voxel area can be edited and saved, after the voxel technology gate passes.
- A small shelter can be assembled from placeholders.
- One contamination hazard and one protective countermeasure can be experienced.
- One simple crop completes a clear growth cycle, or farming is explicitly scheduled for the next slice if dependencies are not ready.
- A simple threat signal and one defense interaction demonstrate the raid contract.
- Basic sound/VFX/UI feedback explains valid and invalid actions.
- Save/load persists the tested world changes.

The slice is not complete simply because every class exists. Use the acceptance gates in the production plan.

## 8. Scope discipline

- Prefer placeholder meshes, simple collision, and readable states until mechanics stabilize.
- Avoid high-poly art, expensive materials, final music production, and large content packs during core-system implementation.
- Do not add a resource unless it has a source, acquisition method, processing path (if any), at least one useful payoff, and a reason to compete with another use.
- Do not add full simulation merely because a reference game does so. Copy the design principle, then constrain its implementation.
- Keep daily weather, seasonal time, technological progression, and long-term restoration as separate time scales.
- Never claim compile, PIE, save/load, performance, or multiplayer success without evidence from the relevant target.
