# System Dependencies and Acceptance Rules

## 1. Dependency map

| System | Depends on | Must provide to others | Main risk |
|---|---|---|---|
| Item IDs and data contracts | Scope lock | Stable item/recipe/material IDs | IDs drift between C++ and data |
| Placement kernel | Units, axes, profile contracts | Preview/commit result, anchor occupancy | Separate kernels diverge |
| Wood lifecycle | Item/state schema, tool action contract | Explicit piece state and output variant | Illegal transitions or mesh/state mismatch |
| Structural graph | Placement kernel, build-part data | Parent/child links, support/removal rules | Orphans or clipping |
| Inventory/carry | Item schema, ownership contract | Atomic ownership transitions | Duplication/loss on failed actions |
| Crafting/processing | Item schema, recipes, tool/station data | Deterministic input/output transaction | Partial consumption |
| Voxel runtime | Engine compatibility gate, save contract | Edited terrain state, hardness, excavation output | Plugin/engine conflict and save cost |
| Underground support | Voxel runtime, build-part schema | Bounded room/tunnel support rules | Unsupported spaces or unpredictable collapse |
| Survival/exposure | Time, region, equipment data | Exposure state and clear warnings | Opaque unavoidable damage |
| Weather | Time system, region data | Weather state and environmental modifiers | Many systems update inconsistently |
| Farming | Calendar, soil, water/contamination | Crop states and harvest outputs | Save/load timing mismatch |
| Raids | Threat state, structure damage, audio/UI | Warning, target selection, repairable outcomes | Unfair/unrecoverable damage |
| Restoration | Region state, discovery/resource systems | Persistent regional changes | Global simulation cost or superficial visual-only recovery |
| VFX/SFX/music/UI | Gameplay event registry | Perceptible action/state feedback | Feedback contradicts authoritative result |
| Save/load | Stable IDs and version policy across systems | Persistent world/inventory/terrain state | Partial saves corrupt ownership or world state |

## 2. Cross-system contracts that must not be violated

1. **One placement kernel:** material-specific rules belong in providers/profiles, not duplicated kernels.
2. **Preview equals commit:** the accepted preview solution must be the one committed; don't recompute a different solution at commit.
3. **State-owned geometry:** a log's processing state and its mesh variant must agree. Half-length and split half-round are distinct.
4. **Atomic transactions:** pickup, drop, craft, place, process, and consume either succeed completely or leave state unchanged.
5. **Unique ownership:** world actor, carried object, inventory slot, and placed graph cannot own the same item simultaneously.
6. **Persistent identity:** save uses stable IDs/handles, not transient actor names or array indices.
7. **Voxel boundaries:** excavated cells, yield, collision, support, and save policy must agree. No unlimited voxel↔structure simulation in the first scope.
8. **Feedback is not authority:** audio/VFX/animation are reactions to accepted gameplay events, never the source of truth.
9. **Time-scale separation:** weather, crop calendar, technology, and region restoration use clear update rates and saved state.
10. **No invisible failure:** invalid action, insufficient tool/material, occupied anchor, insufficient capacity, or unsafe exposure gets a readable response.

## 3. Definition of done

A task can be marked VERIFIED only when all applicable checks are recorded:

- Code or content is on the intended branch and reviewed.
- Data/config IDs resolve and schema validation passes.
- Preconditions and failure path are defined.
- No duplication/loss on failed actions.
- Save/load behavior is tested where state persists.
- Automated tests pass for deterministic rules.
- Target UE 5.8 editor build is recorded.
- Relevant Editor/PIE scenario is executed and evidence/logs are recorded.
- Performance impact is measured for voxel, world scans, VFX, AI and other bounded-cost systems.
- Documentation and current state/session notes are updated.

A static code test, Python script, unit test, or successful compilation is not proof that the in-game loop works. Record each evidence type separately.

## 4. Traceability rows

Use one row per mechanic/catalogue item:

| Field | Meaning |
|---|---|
| MechanicId | M001–M119 or a new approved ID |
| SystemId | Owning system |
| Status | Proposed, planned, partial, implemented-unverified, verified, deferred |
| SourcePaths | Exact owning source/content/config paths |
| DataIds | Item/recipe/material/event IDs |
| Dependencies | Required systems and phases |
| AcceptanceCase | Reproducible case that must pass |
| Evidence | Commit, test output, screenshot/video/log and target engine version |
| KnownLimitations | Honest boundaries that remain |

Do not renumber M001–M119 to make new requirements fit. If landscape/voxel needs create extra mechanics, append new IDs only after cross-reference and duplicate review.
