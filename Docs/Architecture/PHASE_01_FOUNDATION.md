# Phase 1 — Shared Technical Foundation

Status: **architecture baseline drafted; Unreal build and runtime validation pending**  
Target engine: Unreal Engine 5.8  
Working branch: `phase-01-foundation` (branched from `Chatgpt`)

## Goals

Create a stable contract for the 16 development phases without replacing existing gameplay systems before their current code and ownership are verified.

## Non-negotiable rules

1. Preserve the current project and existing mechanics. New code must not silently replace or duplicate an existing authoritative system.
2. Keep gameplay domains modular, but share object identity, definitions, operation outcomes, world events, and persistence contracts.
3. A gameplay operation must validate prerequisites before mutating state.
4. State changes must be explicit and traceable. A visual mesh alone is never the source of truth for a mechanic.
5. Models are project-owned simple/procedural assets; no dependency on third-party asset packs is assumed.
6. No Unreal build, editor test, or in-game validation is considered passed until it is actually run and its result recorded.

## Domain boundaries

- **World Core:** world time, region identity, world-state coordination.
- **Object & Definition Catalog:** stable definition IDs, tags, material/resource properties.
- **Interaction:** requests, prerequisites, validation, operation outcomes.
- **Resource & Material:** quantities, units, capacity, consumption and transformation.
- **Processing & Crafting:** domain-specific processing states and recipes.
- **Construction:** snap points, support, attachment, integrity and damage.
- **Terrain Modification:** permitted terrain layers, edits, support and water interactions.
- **Traversal:** movement constraints, load, ropes, ladders, bridges and paths.
- **Survival:** player needs, exposure, fatigue, injury and shelter effects.
- **Farming & Animals:** soil, crops, animal needs, lifecycle and outputs.
- **Environment:** time, seasons, weather, hazards and regional effects.
- **AI & Threat:** perception, wildlife, combat, raids and defense.
- **Exploration & Progression:** discoveries, routes and persistent regional change.
- **Persistence:** save/load contracts for durable world state.

These are logical ownership boundaries, not a mandate to create one Unreal module per domain. The implementation should fit the current single-module project until a real build or dependency need justifies module splitting.

## Shared contracts

### Identity and definitions
- Every persistent world instance has a stable instance ID.
- Every instance references a definition ID; display names are not identifiers.
- Definition data describes what an object is; instance state describes what has happened to it.
- Material/resource quantities use explicit units and validated non-negative values.
- Domain-specific state is not flattened into one universal enum. For example, a log's cut/split/groove state belongs to wood processing, while installation and damage belong to construction.

### Operations
Every state-changing interaction follows this order:

1. Resolve actor, target and operation.
2. Check target validity, range, ownership/permission if relevant, required tool/material, and domain prerequisites.
3. Calculate the proposed result without mutating the world.
4. Apply the change atomically only if validation succeeds.
5. Emit a result/event and mark affected state dirty for persistence.
6. On failure, return a stable reason code and leave state unchanged.

### Operation result categories
- Succeeded
- InvalidTarget
- OutOfRange
- MissingTool
- MissingMaterial
- InvalidState
- PrerequisiteNotMet
- BlockedByPlacement
- UnsupportedSurface
- CapacityExceeded
- NotPermitted
- InternalError

A domain may add more specific codes, but it must preserve the shared success/failure semantics.

### Events
Events report completed facts, not speculative intentions. Examples:
- ObjectProcessed
- ResourceConsumed
- ConstructionAttached
- ConstructionDamaged
- TerrainChanged
- CropStateChanged
- AnimalStateChanged
- WeatherChanged
- WorldHazardStarted
- WorldHazardEnded

Events should carry stable IDs and enough context to update UI, AI, audio/visual feedback and persistence without each system directly reaching into unrelated systems.

### Persistence
Save durable state, not transient presentation:
- world/region identity and simulation time
- stable instance IDs and definition IDs
- domain-specific object state
- construction links and support relationships
- terrain edits and their bounds
- crop/animal lifecycle state
- persistent environment/hazard changes
- schema version

Derived visuals, temporary UI state and rebuildable caches should not be authoritative save data. Loaders must tolerate missing optional fields through explicit defaults and reject incompatible schema versions safely.

## Ownership rules

- One authoritative system owns each state transition.
- Other systems request changes through its public interface; they do not write its internal fields.
- No duplicate legacy implementation should be activated merely because it is convenient.
- Existing classes are retained until their ownership and call sites are confirmed.
- Cross-domain dependencies point to contracts/interfaces, not private implementation details.

## Model and collision standards

- Use centimetres as Unreal world units.
- Every asset specification records intended dimensions, pivot convention, collision intent and snap/attachment points.
- Processing variants must represent actual geometry when geometry affects gameplay; a material swap alone cannot impersonate a groove, split or cut face.
- Collision should be simple and purpose-built for the mechanic.
- Initial assets may be low-poly/procedural and must be replaceable without changing gameplay identity or rules.

## Phase 1 deliverables

- [x] Establish a non-destructive architecture baseline.
- [x] Define shared identity, operation, event and persistence rules.
- [x] Define domain ownership boundaries.
- [ ] Inventory the actual current source tree and authoritative implementations.
- [ ] Map all 119 mechanics to owning system, prerequisites, failure cases, persistence and verification.
- [ ] Add code-level shared types only after checking existing types and naming conflicts.
- [ ] Run static checks and Unreal 5.8 build when a suitable environment is available.
- [ ] Record Editor/gameplay verification separately from code/build verification.

## Acceptance gate

Phase 1 is not marked fully complete until existing code is inventoried, the 119-mechanic registry is mapped without invented entries, shared types are reconciled with existing classes, and build/test results are explicitly recorded. This document is the baseline, not proof of a successful Unreal build.
