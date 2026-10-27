# VXL-02A — DeepSeek implementation task packet

**Purpose:** copy this task to a coding model after giving it access to the repository and the exact candidate branch. This is a bounded first code task, not an instruction to implement the entire voxel engine.

## Target repository and branches

- Repository: furkansarikiyak36-eng/myproject3
- Candidate source baseline: feature/simple-intent
- New implementation branch: feature/voxel-terrain-policy-spike, created from the latest inspected feature/simple-intent head.
- Do not edit main or the documentation branch.
- Before editing, state the actual base SHA and inspect the current versions of every referenced file. If the branch/file is different, stop and report the mismatch before guessing.

## Context you must read

1. Docs/Architecture/VOXEL_TERRAIN_DECISION.md
2. Docs/Architecture/PHASE_01_SHARED_DATA_CONTRACTS_RECONCILIATION.md
3. Docs/Architecture/Schemas/terrain-stratum-profile.schema.json
4. Docs/Architecture/Samples/terrain-stratum-profile-forest-foothills.sample.json
5. Source/MyProject3/VoxelTerrainChunkComponent.h/.cpp
6. Source/MyProject3/WorldTerrainStateSubsystem.h/.cpp
7. Source/MyProject3/GameplayCoreTypes.h/.cpp and MyProject3.Build.cs

The current voxel component is a one-chunk smooth-density/Marching-Cubes prototype. The logical terrain subsystem stores bounded cell records and operation receipts. The reviewed source did not show one transaction joining those two representations. Do not treat source presence as proof of working runtime excavation.

## Goal

Implement a **pure, deterministic terrain-stratum profile and excavation-policy query** that answers whether an excavation request is permitted at a given depth beneath the original surface. It must be testable without changing live terrain.

The query needs enough information to return:
- selected profile and stratum IDs;
- material blend IDs and normalized weights;
- permission result: Allowed, MissingTool, TooHard/Blocked, InvalidProfile, InvalidDepth or OutOfProfile;
- required tool tags;
- resistance/time multipliers and support-risk class;
- optional resource-yield profile ID.

All queries using the same profile revision, world seed and coordinates must return the same answer.

## Required behavior

1. Use Docs/Architecture/Schemas/terrain-stratum-profile.schema.json as the design-time data contract and the sample profile as a fixture.
2. Keep geology policy separate from mesh rendering. Do not use material color or mesh identity to decide whether digging succeeds.
3. Measure depth beneath the **original base surface**, not a pit floor or freshly carved tunnel roof. Do not recompute a new zero-depth surface from the edited density.
4. Make stratum intervals ordered and non-overlapping. Validate the first interval starts at zero, the last ends at the profile depth, and there are no gaps. Validate per-stratum material weights sum to 1 within a small documented tolerance.
5. A Blocked stratum always fails. A ToolGated stratum fails unless all required tier/tool tags are present. Missing/invalid profile, NaN/Inf/negative depth and depth beyond the profile fail closed.
6. A deterministic boundary-variation query may use world seed + horizontal position + profile/version to vary the interface. Never use a fresh random draw on each call. If the first implementation leaves boundary variation unimplemented, return the exact configured strata boundary and explicitly document that limitation rather than faking support.
7. Return a stable machine-readable reason code and a descriptive message separately.
8. Do not mutate terrain density, chunk mesh, collision, inventory, item ownership, receipts or world events in this task.
9. Do not replace UVoxelTerrainChunkComponent, UWorldTerrainStateSubsystem, the save coordinator, material schema or any existing voxel plugin.
10. Do not add a runtime JSON file loader solely because the schemas are JSON. The schema is a design-time contract; use a small runtime-native profile representation appropriate to the existing module, and state the migration path to a data asset or validated imported definition.
11. Keep the change scoped. New files for profile data/policy and focused automation tests are expected. Modify existing files only if required and explain each one. No unrelated refactor, no new third-party dependency and no mass rewrite.

## Suggested public contract (adapt only with a concrete reason)

- A profile definition with a stable FName/ID, version, profile depth and ordered stratum entries.
- Each stratum stores stable ID, depth interval, material blend, permission, required tool tags, resistance/time multipliers, support risk, optional yield-profile ID and visual transition width.
- A pure evaluator takes a profile, DepthBelowOriginalSurfaceCm, horizontal position/world seed if variation is implemented, and available tool tags; it returns a value result with a stable code.
- Avoid creating a second generic item/material/world-state owner. Reuse definition IDs and result conventions from existing core contracts where compatible.

## Required tests

Add focused automation tests for:
- 0 cm maps to topsoil and permits a shovel.
- A depth inside the compacted stratum requires the basic pickaxe.
- The stone-rich layer rejects the basic pickaxe and accepts the configured advanced tag.
- Bedrock is always blocked.
- Negative, NaN and infinite depths fail.
- Out-of-profile depth fails.
- Missing profile, missing material IDs, duplicate stratum IDs, gaps, overlaps, invalid intervals and blend weights not summing to 1 fail profile validation.
- Repeated query yields exactly the same result for the same inputs.
- Query does not mutate the profile or world.
- Boundary cases (just below/at/above each interval boundary) follow one explicit, documented convention and cannot select two strata.

Tests must be compiled and run only in an actual compatible Unreal environment; do not fabricate output. If the environment is unavailable, provide the tests and mark them not run.

## Deliverables

1. A focused diff with exact changed files.
2. New/updated automation tests.
3. A short design note explaining policy/data ownership and any limitations.
4. Actual commands and test outputs, or an explicit NOT RUN — Unreal environment unavailable status.
5. A list of remaining integration tasks. Do not integrate the policy into live ApplySphereEdit yet.

## Acceptance gate

This task is complete when the data structure and pure query are reviewable, failure cases are explicit, tests cover the policy, no live terrain mutation was added, and the diff contains no unrelated changes. It is not complete merely because new C++ classes exist.
