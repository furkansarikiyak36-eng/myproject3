# 119 Mechanic Coverage Audit — Pass 01

**Audit ref:** `phase-06-inventory-crafting` source tree, compared with canonical registry v1.0  
**Working branch:** `mechanic-detailing-pass-01`  
**Purpose:** First dependency-ordered source inventory before detailed implementation.  
**Important:** This is a static inventory of the source currently present in `Source/MyProject3`; it is not an Unreal build or runtime result. Registry statuses are intentionally unchanged.

## Summary

The current source tree contains initial layers for shared operation/result types, identity/lifecycle, transient committed-event broadcast, log processing/snap placement, stone anchors/placement, stick/trap states, and inventory/recipe transactions. These are **partial layers**, not 119 completed mechanics.

No complete implementation was found in the current source tree for deterministic world clock/seed, versioned world save/load, shared survival simulation, tree-felling/generation, shared cross-domain structural graph/collapse, constrained terrain digging, farming/animal simulation, raid director, logistics/production queue, weather/exposure, or persistent exploration discovery.

The classifications below are implementation-coverage estimates based on visible source, not verified status:
- `present-in-layer`: a reusable source element exists for the mechanic's core contract.
- `contract-only`: data/request/result structures exist, but no authoritative operation owner.
- `transient-only`: event notification exists without durable delivery/persistence.
- `partial`: some behavior exists, with material gaps remaining.
- `not-found`: no owning-system implementation was found in the current source tree reviewed.

## Canonical per-mechanic coverage

| ID | Mechanic | Dependencies | Coverage classification | Source-level observation |
|---|---|---|---|---|
| M001 | Stable identity for physical world objects | — | partial | Identity component and world-local uniqueness registry exist; no durable world save/restore coordinator. |
| M002 | Exclusive object lifecycle state | M001 | partial | Generic lifecycle enum and guarded transitions exist; not integrated consistently into all domain actors. |
| M003 | Atomic world-changing operation | M002 | contract-only | Request/result types exist; no shared transaction coordinator or generic rollback boundary. |
| M004 | Structured operation result and failure reason | M003 | present-in-layer | Structured result codes/envelope exist; engine behavior not validated. |
| M005 | Committed domain events | M003 | transient-only | Committed-event broadcaster exists; not durable, no replay/dedup store; timestamp uses UWorld time. |
| M006 | Deterministic world clock and random seed | — | not-found | No deterministic simulation clock/seed owner in the current source tree. |
| M007 | Versioned save/load and migration boundary | M001; M006 | partial | SaveGame-marked fields exist, but no versioned world save/load/migration system. |
| M008 | Shared survival state: health, stamina, warmth and needs | M006 | not-found | No authoritative shared survival-state subsystem/component in current source tree. |
| M009 | Fell a tree with tool hits | — | not-found | No tree-felling actor/tool-hit and log-generation implementation found in current source tree. |
| M010 | Generate logs from tree dimensions | M009 | not-found | No tree-felling actor/tool-hit and log-generation implementation found in current source tree. |
| M011 | Pick up and carry a loose log | M010 | partial | Log carry component exists; source layer does not establish full generic world-item lifecycle or tested carry/drop integration. |
| M012 | Drop or quick-release a carried log | M011 | partial | Log carry component exists; source layer does not establish full generic world-item lifecycle or tested carry/drop integration. |
| M013 | Roll, drag and push a loose log | M011 | not-found | No explicit roll/drag/push interaction or impact-damage resolver found; physics-enabled mesh alone is insufficient. |
| M014 | Log collision and impact damage | M013 | not-found | No explicit roll/drag/push interaction or impact-damage resolver found; physics-enabled mesh alone is insufficient. |
| M015 | Cross-cut log to a valid length | M010 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M016 | Split a whole log longitudinally | M015 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M017 | Quarter a split-half log | M016 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M018 | Hew or square a log | M015 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M019 | Cut a longitudinal groove only after splitting | M016 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M020 | Make a trough or channel | M016 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M021 | Produce a plank from a split log | M016 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M022 | Cut notches and interlocking joints | M018 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M023 | Shape a grounded spike | M015 | partial | Log cut-action state/output-mesh path exists for a subset of processing transitions; each registry action still needs exact mapping, source review and failure-path coverage. |
| M024 | Generate and validate snap anchors | M001 | partial | Log snap anchors, placement and parent-child attachment exist; generic cross-domain graph is absent. |
| M025 | Place logs horizontally, vertically and diagonally | M024 | partial | Log snap anchors, placement and parent-child attachment exist; generic cross-domain graph is absent. |
| M026 | Maintain explicit parent-child construction links | M024 | partial | Log snap anchors, placement and parent-child attachment exist; generic cross-domain graph is absent. |
| M027 | Remove a support and resolve collapse | M026 | not-found | No shared support-collapse resolver, repair/salvage/burn lifecycle found in current source tree. |
| M028 | Repair, salvage and burn wood | M027 | not-found | No shared support-collapse resolver, repair/salvage/burn lifecycle found in current source tree. |
| M029 | Collect, hold and throw loose stone | — | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M030 | Shape raw stone into building pieces | M029 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M031 | Place and align individual stones | M030 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M032 | Stack stones horizontally and vertically | M031 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M033 | Build a stone wall and corner | M032 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M034 | Build stone columns, beams and openings | M033 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M035 | Resolve stone support, balance and collapse | M034 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M036 | Damage, repair and salvage stone structures | M035 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M037 | Store and haul stone in bulk | M029 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M038 | Connect mixed wood-stone construction and stone defenses | M033; M026 | partial | Stone piece/anchor/carry/place connection layer exists; harvesting, shaping, full masonry forms, support/damage/salvage and mixed-material integration are not present as a complete system. |
| M039 | Gather and break sticks | — | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M040 | Sharpen a stick or make a double-ended stake | M039 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M041 | Craft primitive tools and melee weapons | M039 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M042 | Craft spear, bow and arrows | M041 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M043 | Make torches and simple light sources | M039 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M044 | Build stakes, fences and simple barriers | M040 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M045 | Build primitive traps and trigger lines | M044 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M046 | Build stick storage, racks and furniture | M039 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M047 | Build simple garden and animal-support structures | M046 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M048 | Dismantle stick structures and recover materials | M044 | partial | Stick piece states, anchors and trap arming prerequisites exist; harvest, tool/weapon behavior, projectile combat, fuel, trigger sensing, structures and salvage are incomplete. |
| M049 | Pick up a world item into inventory | — | not-found | No world item actor pickup/drop transaction integration. |
| M050 | Stack items and enforce carry limits | M049 | partial | Stack, slot and weight capacity functions exist; unknown item weights currently count as zero, and edge-case/source review remains. |
| M051 | Show item category, properties and quick access | M049 | partial | Item categories and quick-access ID exist; no player-facing category/property UI. |
| M052 | Place ingredients into a crafting area | M049 | not-found | No crafting-area ingredient placement/session ownership. |
| M053 | Resolve recipes and alternate valid recipes | M052 | partial | Recipe registry/resolution exists for one recipe ID; alternate valid recipe selection is not implemented. |
| M054 | Show missing ingredients and required quantities | M053 | partial | Missing consumed ingredients can be reported; no complete player-facing required-quantity UI. |
| M055 | Confirm crafting and consume materials atomically | M053 | partial | Craft preflight and local inventory snapshot rollback exist; no cross-system transaction/operation ID/revision protection. |
| M056 | Preserve tools and return unused materials | M055 | partial | Non-consumed recipe ingredients are preserved; explicit tool durability and returned-unused-material flows are not implemented. |
| M057 | Cancel crafting and collect the result | M055 | not-found | No owned crafting session cancellation or result-collection lifecycle. |
| M058 | Craft repair kits, protection and trap components | M055 | not-found | Domain repair/protection/trap-component recipes are not integrated. |
| M059 | Use one authoritative structural graph | — | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M060 | Validate support path from ground to load | M059 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M061 | Enforce span, overhang and load limits | M060 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M062 | Apply damage thresholds to structural links | M059 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M063 | Collapse dependent pieces in a stable order | M060 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M064 | Create and remove doors, windows and wall openings | M059 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M065 | Build floors, roofs, ramps and stairs | M060 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M066 | Build bridges, rope crossings and elevated walkways | M065 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M067 | Build gates, fences, railings and defensive walls | M059 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M068 | Damage and repair a structure without duplicating state | M062 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M069 | Dismantle structures and recover resources | M059 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M070 | Prevent unsafe pickup/removal of supporting pieces | M060 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M071 | Divide world state into persistent chunks | — | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M072 | Edit only configured diggable terrain layers | M071 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M073 | Dig a pit with valid terrain geometry and collision | M072 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M074 | Dig a horizontal tunnel and small room | M073 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M075 | Prevent unsupported underground collapse | M074 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M076 | Preserve terrain edits across save/load | M071 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M077 | Simulate water level, flow and local flooding | M071 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M078 | Simulate soil moisture, nutrients and surface runoff | M077 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M079 | Track fire spread, rot and material exposure | M071 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M080 | Place and discover environmental ruins and clues | M071 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M081 | Advance world time with deterministic simulation ticks | — | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M082 | Plant seeds and validate soil/season requirements | M078; M081 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M083 | Grow crops through defined stages | M082 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M084 | Apply watering, nutrients and plant stress | M083 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M085 | Harvest crops and calculate yield | M083 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M086 | Keep and feed farm animals | M081 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M087 | Advance animal growth and health | M086 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M088 | Produce milk, eggs, wool or other configured goods | M086 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M089 | Reproduce animals within habitat capacity | M087 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M090 | Connect feed, manure and soil nutrient cycles | M085; M086 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M091 | Schedule and gate threat events | M006 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M092 | Evaluate difficulty from player/world context | M091 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M093 | Plan enemy groups, waves and resource budget | M092 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M094 | Enforce spawn capacity and queue limits | M093 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M095 | Choose valid spawn locations | M094 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M096 | Select targets and compute navigation paths | M095 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M097 | Resolve attack, bypass and structure damage | M096; M062 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M098 | Advance wave state machine | M093 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M099 | Cancel, interrupt and recover a failed raid | M098 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M100 | Reassess pressure from settlement noise, stock and defenses | M091; M086 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M101 | Load and unload carts, sleds and storage racks | — | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M102 | Transport materials by rope line or zipline | M101 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M103 | Create primitive saw/production chains | M101 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M104 | Queue production and expose blocked reasons | M103 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M105 | Manage fuel, heat and cooking stations | M043 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M106 | Use roads and bridges to reduce route cost | M066 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M107 | Manage shelter, temperature and daily survival choices | M008 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M108 | Expose resource, durability and storage status to the player | M051 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M109 | Demonstrate integrated save/load across world and production | M076; M104; M090; M098 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M110 | Simulate weather and seasonal transitions | M006 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M111 | Apply cold, heat and wetness exposure | M110 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M112 | Track hunger and thirst consumption | M006 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M113 | Recover stamina through rest and activity limits | M008 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M114 | Apply sleep and fatigue effects | M113 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M115 | Apply injury, illness and poisoning state | M008 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M116 | Preserve food quality and spoilage over time | M006 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M117 | Apply clothing insulation and protective equipment | M111 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M118 | Make shelter quality affect environmental exposure | M107; M111 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |
| M119 | Make exploration discoveries persist and unlock world context | M080 | not-found | No owning-system implementation found in the current Source/MyProject3 tree; only registry/architecture scope exists. |

## High-priority source issues found during first pass

### P0 — Resolve before expanding dependent systems

1. **Inventory weight bypass:** `GetCurrentWeightGrams` and capacity simulation only count an item when its weight is configured and positive. An item with no weight definition effectively weighs zero, which can bypass carry limits. Decide whether unknown weight means rejection or a safe configured fallback; do not silently treat it as weightless.
2. **No authoritative transaction coordinator:** shared operation/request/result types do not make multi-domain mutations atomic by themselves. Crafting rollback only snapshots inventory arrays; it cannot roll back a world actor, structure anchor, terrain change or event already committed elsewhere.
3. **No deterministic world clock/save coordinator:** world events use `UWorld::GetTimeSeconds()`, which is not a persisted deterministic simulation clock. SaveGame flags alone do not implement world snapshot ordering, schema migration, or durable actor/graph relationships.
4. **Domain state is not unified:** log, stone and stick pieces each maintain their own placement/connection state. The shared structural graph must be added before support propagation, mixed-material construction, raid damage and safe support removal can be completed.

### P1 — Correctness and integration gaps

5. **Event semantics:** `WorldCommittedEventSubsystem` is a transient multicast notification, not a durable event log. Listener delivery does not provide replay, deduplication or save/load semantics.
6. **Inventory data integrity:** stack merging is keyed primarily by item ID; the system needs a canonical item-definition source so category, stack maximum and weight cannot conflict across incoming stacks.
7. **Crafting diagnostics:** `CanCraft` returns false both for missing ingredients and output capacity, but its output only describes missing ingredients. A structured blocked reason is needed for the UI and production queue.
8. **Log operation retry safety:** the result-bearing cut API accepts an operation ID but no durable deduplication store was found. Repeated calls with the same ID can potentially apply a second transition only if state permits; idempotency is not guaranteed by the ID alone.
9. **Construction consistency:** per-domain anchor reservations need a single cross-domain graph adapter and audited cleanup semantics before collapse/removal logic is added.

## Dependency-ordered implementation passes

1. **Pass A — Core invariants:** deterministic clock/seed; operation ID/deduplication and revision checks; lifecycle identity integration; transaction boundary/result reasons; save schema/version baseline.
2. **Pass B — Physical resource lifecycle:** tree felling/log generation; carry/drop; exact wood processing state machine; stone and stick resource transitions; connect all world pickup/drop to inventory without duplicate ownership.
3. **Pass C — Structural graph:** one graph spanning logs, stones and sticks; anchor reservation; support/load limits; damage, repair, collapse and safe removal.
4. **Pass D — World simulation:** bounded terrain layers/chunks/digging, water/moisture/fire, then farming and animals on the shared deterministic clock.
5. **Pass E — Threat and logistics:** raid director and bounded AI; carts/zipline/resource movement; production chains and queues; route-cost integration.
6. **Pass F — Survival and persistence:** weather, exposure, needs, rest, injuries, food spoilage, clothing/shelter, discoveries; integrated save/load across all domains.
7. **Pass G — Coverage/demo:** one practical setup/action/expected result/failure-path/cleanup record per M001–M119; demo scene only after source integration and the final user-authorized engine gate.

## Explicit non-claims

- No source was compiled.
- No Unreal Header Tool (UHT), Unreal Editor, PIE or gameplay test was run.
- No registry status was upgraded.
- No existing PR was merged.
- The 119-item registry remains the canonical forward scope; it is not represented as the recovered historical 119-test list.
