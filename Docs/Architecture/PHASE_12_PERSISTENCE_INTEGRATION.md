# Phase 12 — Versioned Persistence and Integration Boundary

## Purpose
Create a safe, versioned snapshot container that later subsystem adapters can use. This phase follows Phase 11 and does not merge any earlier branch into main.

## Source foundation
- `FWorldSaveManifest`: save ID, format version, world seed, world time, and named subsystem sections.
- `FWorldSaveSection`: section schema version, opaque serialized payload, and CRC32 integrity check.
- `UWorldSaveGame`: Unreal `USaveGame` container for the manifest.
- `UWorldPersistenceSubsystem`: snapshot creation, duplicate/size/schema/checksum validation, and slot save/load.
- A load validates the complete candidate manifest before replacing the active in-memory snapshot.

## Critical scope boundary
This is a **snapshot transport/container foundation**, not integrated world persistence yet. No subsystem currently exports or imports its authoritative state through this coordinator. `PutSection` accepts opaque bytes; it does not serialize C++ subsystem maps automatically.

## Registry relationship
- M007: versioned save/load boundary is only partially scaffolded; migration is not implemented.
- M076: terrain edit export/import is not connected.
- M109: integrated save/load across terrain, structures, inventory, production, farming and raids is not implemented.
- M119: discovery persistence has a container path available but is not wired to the discovery ledger or unlock context.
- Phase 12 does not change registry statuses. All M001–M119 entries remain `unreviewed` until source mapping and agreed UE 5.8 runtime evidence exist.

## Required adapter sequence
1. Define a stable section ID and schema version per authoritative owner.
2. Export each owner into a deterministic byte representation.
3. Validate every section and dependency before applying any section.
4. Stage imports so a failure cannot leave half the world restored.
5. Commit world state and domain events only after every participant is ready.
6. Add migration policy, corruption recovery, slot naming policy and autosave policy.
7. Verify round-trip, bad checksum, duplicate section, unsupported version and partial-failure cases in UE 5.8.

## Explicit non-goals
- No change to the 119-mechanic registry.
- No assertion that production, terrain, farming, inventory, structural graph, or raid state now survives reload.
- No editor map, UI, autosave scheduler, or runtime test claim.
