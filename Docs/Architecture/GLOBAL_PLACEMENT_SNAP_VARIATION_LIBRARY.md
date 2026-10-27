# Global Placement & Snap Variation Library

**Status:** Architecture and machine-readable catalogue only; runtime migration is not yet implemented.  
**Branch:** `architecture/global-snap-variation-library`  
**Baseline:** `sotf-wood-core-integration`

## Purpose

Build one reusable placement/attachment vocabulary for logs, stones, fire fixtures, and future voxel placement. The kernel should not contain object-specific branches such as "if log", "if stone", or "if fire". Each object family contributes geometry, anchor/contact providers, compatibility tags, and supported placement profiles.

The machine-readable seed catalogue is `Config/Placement/SnapVariationLibrary.json`.

## Core distinction: station, anchor, contact, cut, and cell

These concepts are deliberately separate:

- **Anchor** — a named, typed connection feature that can participate in a structural or receiver connection.
- **Surface station** — a reusable longitudinal position on a shape. A station is not itself a set of duplicated physical actors; the geometry provider resolves its contact frame for the current connection direction.
- **Contact point** — a point discovered from collision/geometry queries. It may support physical resting without creating a structural connection.
- **Cut target** — a position where a cut operation may be requested. It does not become a snap point just because it lies on the same grid.
- **Voxel cell/face** — a discrete grid address or neighbor relation, not a traditional actor socket.

Profiles can combine these concepts, but must not conflate them.

## Log station convention

For a standard log whose length is an integer multiple of 75 cm:

- The maximum standard length is 600 cm.
- Create two end anchors from the actual end planes.
- Create one logical surface station per 75 cm longitudinal module, located at the center of that module.
- A 600 cm log therefore has 8 surface stations at 37.5, 112.5, 187.5, 262.5, 337.5, 412.5, 487.5, and 562.5 cm, plus 2 end anchors: 10 logical anchor/station records in total.
- The surface station's radial contact frame is resolved around the log's local longitudinal axis at query time. Do not spawn a duplicate snap actor for every rotation or radial direction.
- A 450 cm log has 6 surface stations plus 2 end anchors; 300 cm has 4 plus 2; 150 cm has 2 plus 2; 75 cm has 1 plus 2.
- If a piece has a non-standard physical length, end anchors still follow the actual geometry. Only complete modules receive standard surface stations unless a profile explicitly enables another strategy.

### Grid boundary versus station center

A surface station is at the **center of its 75 cm module**. A modular cut target is at an **internal 75 cm boundary** (75, 150, 225 cm, etc.). These coordinate sets are intentionally different. A future configuration may allow a cut-created end to become an anchor, but a cut target is not automatically an anchor before the cut is completed.

The current log source has a free-length-cut path described as using 25 cm steps. The catalogue retains it as the optional `log.free_cut_25cm` compatibility profile rather than silently redefining every cutting action to 75 cm. Choose whether the 25 cm option remains available through design/acceptance review; do not change existing gameplay behavior as an accidental side effect of changing snap spacing.

## Variation dimensions

A variation should be described by a combination of independent dimensions instead of a new hard-coded solver function for every visual case.

| Dimension | Values/examples | Purpose |
|---|---|---|
| Source feature | end, surface station, face, edge, corner, named receiver | Which part of the held piece participates |
| Target feature/provider | end anchor, surface station, support region, surface, cell/face | Which target feature can receive it |
| Relationship | end-to-end, end-to-side, side crossing, parallel stack, multi-support, ground rest, face alignment, cell adjacency | Topology of the placement |
| Orientation | parallel, perpendicular, configured angle set, surface-normal alignment, free physics orientation | How axes/normals are aligned |
| Constraints | distance, angle, collision, clearance, slope, material tags, support count, reservation | Whether a candidate is legal |
| Commit behavior | structural connection, receiver transfer, ground contact, voxel edit, physics settling | What state change occurs |
| Failure policy | reject, show invalid preview, or explicitly selected physical hinge/rest | Prevent unintended fallback behavior |

The catalogue currently includes these representative combinations:

### Log
- end-to-end
- end-to-surface T joint
- 90-degree surface crossing
- configured angled surface crossing
- parallel interlocked stacking
- beam whose two ends must solve against two distinct supports
- diagonal brace between compatible post/beam regions
- explicitly selected one-end hinge / swing
- free ground rest with physical settling
- 75 cm modular cut targets
- optional 25 cm fine-cut compatibility profile

### Stone
- face-to-face alignment
- edge/corner connection
- irregular surface resting with physics

### Fire
- ground-surface placement with surface tags and clearance
- named fuel receiver transfer (not a structural connection)

### Voxel
- cell edit by grid coordinate
- face-neighbor adjacency
- later extensions may add edge/corner adjacency and shape-specific block profiles

This is a starter catalogue, not a claim that these behaviors already exist or are verified in the game.

## Resolution pipeline

1. Discover candidate providers near the aim, held item, or edited cell.
2. Expand candidate pairs through the requested/allowed profiles.
3. Solve geometry and all required anchor constraints.
4. Validate family/profile/material compatibility.
5. Validate collision, surface, slope, and clearance.
6. Ask the structural graph to validate support/load/removal implications when relevant.
7. Score legal candidates; hard-invalid candidates are never rescued by a high score.
8. Show one complete preview of the proposed solution and its connection/contact points.
9. Reserve every required anchor/resource.
10. Commit all required connections in one transaction, or roll back all reservations and transforms.
11. Enable physical simulation only when the chosen profile calls for physical settling or an explicit hinge behavior.

## Candidate scoring

Hard rejection comes before scoring. Examples: incompatible anchor role, missing required second support, occupied reservation, blocking collision, invalid ground/cell, or structural constraint failure.

Among valid candidates, score by:
- proximity to the point the player is aiming at;
- orientation error;
- declared profile preference;
- distance from the standard modular station;
- smallest unexpected change from the previewed transform.

A candidate must not win merely because it is close if it violates a hard constraint.

## Explicit failure rules

- A failed two-support beam must not be silently committed to only one support.
- A failed beam must not silently become a diagonal brace.
- Ground contact must not be recorded as a structural attachment unless the selected profile defines that relationship.
- A one-end hinge/swing is allowed only through its explicit profile; it does not become the universal fallback for every failed snap.
- A failed reservation or commit must leave no partial structural links and no leaked reservations.
- A partial cut changes processing state only. A completed cut creates the new pieces, then recalculates actual endpoint anchors and surface stations for each resulting length.

## Runtime contract (implementation target)

A future C++ contract should expose equivalents of:

- `IPlacementGeometryProvider`: local features, surface queries, bounds and contact frames.
- `IPlacementAnchorProvider`: named anchors/stations and compatibility metadata.
- `FPlacementProfile`: relationship type, allowed feature roles, constraints, scoring preferences and commit policy.
- `FPlacementCandidate`: source/target feature references plus profile ID.
- `FPlacementSolution`: final transform(s), required anchors, contacts, support references and explanatory validity result.
- `UPlacementKernelSubsystem`: candidate discovery, solving, scoring, preview and atomic commit orchestration.
- `IPlacementTransactionParticipant`: reservations, ownership/structural updates, rollback.
- Separate structural graph and physics adapters; neither is folded into geometric solving.

These are interface goals, not existing class names to assume are already implemented.

## Incremental migration plan

1. **Lock the vocabulary and data schema.** Validate `SnapVariationLibrary.json` and agree on station/cut semantics.
2. **Implement the provider and profile types.** Keep the first pass independent of the player input component.
3. **Build the log geometry provider.** Generate two true end anchors and surface stations from actual piece length; make station radial frames query-driven.
4. **Add solver/preview tests.** Exercise end-to-end, T, surface crossing at 90 degrees, angled cross, parallel stack, two-support beam, diagonal brace, ground rest, invalid candidate, and reservation rollback.
5. **Bridge legacy log routes.** Route existing candidate search, stack placement, strut placement, and ground placement through adapters in small steps. Keep a temporary compatibility path only while equivalence tests are run.
6. **Add material-family providers.** Stone and sticks use the same contract with different profiles; fire uses surface placement plus named receivers.
7. **Add voxel adapter.** Grid/cell edits use the same orchestration and transaction model but do not create a physical anchor actor per cell.
8. **Validate in Unreal 5.8.** Build/UHT and PIE tests remain a separate acceptance gate; source presence or JSON validity is not game validation.

## Acceptance checklist

- [ ] 600 cm log generates 8 surface stations + 2 end anchors.
- [ ] 450/300/150/75 cm logs generate 6/4/2/1 surface stations + 2 end anchors.
- [ ] Surface station radial frames support varied connection directions without spawning radial duplicates.
- [ ] A two-support beam requires both support constraints and commits atomically.
- [ ] End-to-side T and surface-to-surface crossing are distinct profiles.
- [ ] Angled joins obey configured angle tolerances and collision rules.
- [ ] Ground-rest contact does not create a structural socket by itself.
- [ ] Explicit one-end hinge can swing; failed ordinary beams do not auto-convert.
- [ ] Partial cut does not split the piece; completed cut regenerates endpoints/stations.
- [ ] Existing 25 cm free-cut behavior is either retained via its named profile or intentionally changed after acceptance review.
- [ ] Stone surface/face behavior uses the same core contract without log-specific branches.
- [ ] Fire placement and fuel receiver transfer remain separate operation types.
- [ ] Voxel cell changes use grid coordinates and transactions, not per-cell snap actors.
- [ ] Failed placement leaves no partial links, reservations, or ownership changes.
- [ ] Unreal build/UHT and manual PIE outcomes are recorded separately from static/catalogue tests.

## Current implementation status

This branch adds the design and data catalogue only. The placement kernel has **not** been implemented, existing log code paths have **not** been migrated, and Unreal build/PIE testing has **not** been performed by this change.
