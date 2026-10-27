# Constrained Coding-Model Workflow for MyProject3

This workflow permits a fast coding model to draft implementation changes while preserving one architecture authority and a verifiable source history.

## 1. Roles

- Architecture/task owner: defines behavior, interfaces, non-goals, dependencies and acceptance criteria; preserves IDs and source ownership.
- Coding model: inspects the exact candidate branch, implements only the assigned task, returns a diff and reports tests honestly.
- Reviewer/integrator: independently reviews the diff, fixes defects, runs available static/automated checks, reconciles it against the target branch and records what remains unverified.
- Unreal verification: the actual UE 5.8 environment performs UHT/UBT, Editor/PIE, save/load and performance checks when required. Static review is never presented as a substitute.

## 2. Never pass an unbounded prompt

Every task packet must include:
- repository and exact base branch/SHA;
- required files and source contracts to read;
- one player-visible or data-contract outcome;
- explicit inclusions and exclusions;
- source ownership/API contracts to preserve;
- failure/rollback/idempotency behavior;
- persistence expectations;
- exact acceptance cases;
- allowed file list (or strict justification for extra files);
- expected diff, test report and known-limitations format.

The full GDD is context, not permission to modify everything. The coding model should read the GDD plus the smallest necessary source subset.

## 3. Required coding-model response

Before editing, it must report its base SHA, inspected files and implementation plan. After editing, it must return:

1. exact files changed and why;
2. unified diff or commit/PR URL;
3. tests it actually ran, exact commands and exit codes;
4. tests not run and why;
5. assumptions and unresolved decisions;
6. any API or data migration;
7. a self-review of failure paths and potential regressions.

A verbal claim such as “compiled successfully” without reproducible command/output is not evidence. If it cannot access the repo or execute the target engine, it must say so.

## 4. Review and integration gate

The reviewer must:
- compare the diff against the exact base revision and task packet;
- reject unrequested rewrites, duplicated authorities, guessed APIs and silent public signature changes;
- inspect success, failure, retry/idempotency, state ownership and save/load paths;
- check schema/data IDs and cross-references;
- run relevant available static/automated validation;
- make necessary fixes in a reviewable commit;
- keep Unreal build/UHT/Editor/PIE/save-load/performance statuses separate.

Only after this gate should code be integrated into the selected candidate branch. Never merge a broad stack into main because one small test or catalogue CI passed.

## 5. One task, one branch, one owner

- Start implementation work from the explicitly selected source branch, not automatically from main.
- Give every branch and PR one bounded responsibility.
- Do not allow two agents to simultaneously rewrite the same public header or shared API.
- Do not ask a coding model to recreate code that already exists; require it to inspect and extend the current implementation.
- Preserve existing M001–M119 IDs. New mechanic IDs need a separate duplicate/scope review.
- The documentation/schema branch must not be mistaken for the gameplay implementation branch.

## 6. First recommended task

Use the VXL-02A DeepSeek task packet in Docs/Architecture/VXL_02A_DEEPSEEK_TASK_PACKET.md for the first smooth-voxel task. It implements only the depth-stratum policy contract and tests. It deliberately does not connect to live terrain edits; that requires a subsequent transaction and persistence task after the policy diff is reviewed.
