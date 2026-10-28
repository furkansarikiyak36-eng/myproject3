# Phase 12 Acceptance Checklist

## Source-level checks
- [x] Versioned manifest and opaque named section contract.
- [x] SaveGame slot container.
- [x] Duplicate section, payload size, format, and checksum validation.
- [x] Candidate manifest is validated before replacing active snapshot.
- [ ] Deterministic serialization contract for every subsystem.
- [ ] Schema migration and backward compatibility.
- [ ] Atomic multi-subsystem restore and rollback.
- [ ] Durable terrain, structure, inventory, farming, production, raid and discovery adapters.
- [ ] UE 5.8 UHT/build.
- [ ] Editor/PIE save-reload verification.
- [ ] Corrupt-save and partial-adapter-failure tests.

## Required round-trip scenarios
1. Save then load the same snapshot and compare every section byte-for-byte.
2. Reject duplicate section IDs without altering the current snapshot.
3. Reject a modified payload with a stale CRC.
4. Reject unsupported format versions.
5. Reject missing CoreWorld section when required.
6. Simulate one participant failing import and prove no other participant is committed.
7. Reload a world with terrain edits, structure links, inventory, production jobs, crop/animal state, raid state and discoveries.

## Exit gate
Phase 12 remains a draft foundation until the unchecked integration and UE validation items are completed. Source presence alone does not close M007, M076, M109 or M119.
