# Implementation Task Template

Copy this for every phase task. Keep one clear owner per interface or source file.

## Task
- **ID:** TASK-XXX
- **Phase:** Pxx
- **Mechanic IDs:** Mxxx...
- **Status:** PLANNED
- **Owner/branch:**
- **Target files:**

## Intent
Describe the player-visible outcome and why it matters. Do not define the task as “add a class” unless that class itself is the complete, testable outcome.

## Scope
### Included
- ...

### Excluded
- ...

## Contracts and dependencies
- Required data IDs/schemas:
- Systems this task depends on:
- Systems that depend on this output:
- Existing branch/implementation to reuse:
- Unresolved decisions:

## Implementation sequence
1. ...
2. ...
3. ...

## Failure and persistence behavior
- Invalid inputs/preconditions:
- Atomic rollback expectation:
- Ownership changes:
- Save/load and version migration:
- Performance/memory limit:

## Feedback
- UI state/message:
- Animation/event:
- SFX/VFX IDs:
- Accessibility concerns:

## Acceptance
- [ ] Preconditions and successful path work.
- [ ] Invalid/failure path is deterministic and consumes/changes nothing unless designed otherwise.
- [ ] Existing neighboring systems still work.
- [ ] Automated validation passes.
- [ ] Target UE 5.8 build evidence is recorded when code changes require it.
- [ ] Editor/PIE evidence is recorded for player-facing behavior.
- [ ] Save/load evidence is recorded if persistent state changes.
- [ ] Docs, status register, and session note are updated.

## Evidence and limitations
- Commit/PR:
- Test commands and results:
- Engine/editor version:
- Reproduction steps:
- Known limitations / next dependency:
