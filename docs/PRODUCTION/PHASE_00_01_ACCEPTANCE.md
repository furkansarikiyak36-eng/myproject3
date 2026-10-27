# Phase 00–01 Acceptance Checklist

## Phase 00 — Repository and scope baseline

- [x] Confirm repository is furkansarikiyak36-eng/myproject3, not the older unrealgame repository.
- [x] Record current main SHA and prior source inventory baseline.
- [x] Locate and count the canonical registry: 119 unique IDs M001–M119, all initially unreviewed.
- [x] Identify the phase branch stack for core/wood/stone/stick/inventory work.
- [x] Record snap branch divergence and major integration warning.
- [x] Produce per-mechanic candidate source-path map without marking candidate files as implemented.
- [x] List potential gaps: regional restoration, local air/water remediation, later fungal/spore mechanics.
- [ ] Review every mechanic's exact code/data implementation on the selected integration base.
- [ ] Audit local/untracked Blueprint references and the actual Unreal asset tree.
- [ ] Select and pin the future implementation branch's parent commit.

## Phase 01 — Shared contracts

- [x] Write identity/definition/instance separation.
- [x] Define atomic operation request/result/event semantics.
- [x] Define resource quantities with units and finite/nonnegative validation.
- [x] Define unique ownership and processing/recipe transaction expectations.
- [x] Define snap/structural graph invariants and preview/commit equality.
- [x] Define save schema/version and proposed restore order.
- [x] Map candidate implementations from core contracts, operation ledger, clock, save coordinator and domain branches.
- [ ] Decide which candidate API is retained after code review.
- [ ] Validate registry JSON against its schema and verify CSV/JSON/Markdown IDs agree.
- [ ] Add/execute automated contract tests on the selected branch.
- [ ] Build UE 5.8 Editor target and record UHT/compiler results.
- [ ] Run target Editor/PIE scenarios, including fail/retry and save/load.

## Allowed status labels

- DOCUMENTED: contract or inventory is written.
- CANDIDATE_IN_BRANCH: code exists on a non-main branch but is not integrated/verified.
- PARTIAL_IN_MAIN: prototype code is present, but the mechanic acceptance scenario is not proven.
- NEEDS_REVIEW: source, data, or callers have not been inspected enough to classify.
- BLOCKED: a decision/dependency prevents work.
- VERIFIED: only after the relevant static, automated, build, and runtime evidence is recorded separately.

Don't turn DOCUMENTED or CANDIDATE_IN_BRANCH into VERIFIED just because a file exists.
