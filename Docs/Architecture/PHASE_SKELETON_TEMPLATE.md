# Phase NN — Domain Name

**Status:** Skeleton / partial / implemented_unreviewed / reviewed_static / verified_engine / blocked  
**Registry scope:** Mxxx–Myyy  
**Prerequisites:** Explicit phase and mechanic IDs

## Responsibility
State the single authoritative responsibility of this phase and what it deliberately does not own.

## Proposed source boundaries
List candidate subsystem/components/types. Mark all names as proposed until confirmed in source.

## Inputs and contracts
List required data, prerequisites, result types, identity/revision behavior and transaction boundaries.

## State ownership and persistence
Name the authoritative state owner, what must persist, schema/version concerns and restore dependencies.

## Cross-system integrations
List provider/consumer systems and the direction of calls/events. Avoid duplicate state owners.

## Failure invariants
Document rejected states and guarantee no partial mutation, duplicated resources, stale links or double-applied events.

## Acceptance gates
- [ ] Positive path
- [ ] Invalid prerequisite path
- [ ] Capacity/configuration failure path
- [ ] Cross-system integration path
- [ ] Persistence path
- [ ] Static source review
- [ ] Unreal build/UHT/Editor/PIE/gameplay gate (record as not run until actually executed)
