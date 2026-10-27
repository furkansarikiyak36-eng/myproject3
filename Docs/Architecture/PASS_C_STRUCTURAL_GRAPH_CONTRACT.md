# Pass C — Shared Structural Graph Contract

## Objective

Introduce a world-local graph contract that can represent wood, stone, stick, foundation and terrain-anchor nodes in one support/load graph. Domain actors remain owners of their mesh geometry and authored anchors.

## Implemented

- Node registration with a stable caller-provided GUID, domain kind, actor reference, local load, maximum supported load and damage threshold.
- Explicit parent-child edges carrying both anchor IDs.
- Secondary supports (`TryAddSecondarySupport`, `TryRemoveSecondarySupport`): a node keeps one primary parent and any number of extra load-bearing contacts. A node stands while any support path reaches the ground. Floors, beams on two posts, struts and props use this.
- Cycle rejection and support-path validation over primary and secondary links.
- Recursive aggregate-load calculation; a connection or secondary support that overloads any node is rejected.
- Safe-removal query (`CanSafelyRemove`) and a leaf-first removal order (`BuildDependentRemovalOrder`).
- Damage accumulation with a threshold signal, and `RepairDamage`.
- Revision increments for graph mutations.
- Persistence: `ExportSnapshot` / `RestoreSnapshot` store node IDs, primary edges, secondary supports, damage and revisions. The save coordinator saves and loads them. `RebuildLogActorAttachmentsFromSnapshot` rebuilds log actor attachments from the restored edges.

## Who writes edges

- **Placement kernel** (`Source/MyProject3/Placement`, see `GLOBAL_SNAP_SYSTEM_PLAN.md`): every placement by the player, the helper or a plan slot goes through `UPlacementKernelSubsystem::CommitPlacement`. It adds the primary edge, secondary supports and "carried" links (a beam resting on a new strut) in one transaction. If any one is refused, all are undone.
- **Material joints**: log snap points (`ALogBuildPiece::TryReserveSnapPoints`), stone and stick anchors (`TryConnectToAnchor`) and cross-material joints (`UMaterialConnectionSubsystem`) add their own edges. The kernel calls them through its feature providers.
- **Follow-up reactions** after a placement (end contacts, beams raised onto a new post, lifted ends) add secondary supports from `ULogCarryComponent::HandlePlacementCommitted`. These are best-effort: a refused contact does not undo the placement.

## Important boundaries

- It does not automatically spawn collapse physics or salvage resources when damage reaches a threshold. It reports the threshold crossing so the owning orchestration layer can resolve it.
- Anchor geometry, compatibility tags and anchor occupancy are validated by the domain adapters and the placement kernel, not by the graph.
- Load is a configurable scalar, not a structural physics solver: there is no stress, bending or load sharing between supports.

## Required follow-up

1. Define the removal policy beyond "refuse": detach dependents in stable order, or resolve collapse.
2. Tune load and damage values against gameplay.
3. Keep mechanic registry statuses unchanged until the final verification gate.
