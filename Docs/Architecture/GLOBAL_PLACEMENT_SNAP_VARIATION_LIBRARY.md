# Global Placement & Snap Variation Library — Runtime Contract v2

**Status:** Source implementation added for the profile registry, anchor-pair/multi-anchor solver and transaction journal. Full Unreal build/PIE verification remains open.

## Design boundaries

The global system is a shared placement orchestrator, not one universal snap function. It distinguishes:

- **Connection:** feature-to-feature structural connection.
- **Surface placement:** physical rest or placement on a validated surface.
- **Receiver interaction:** inventory/ownership transaction against a named receiver.
- **Voxel operation:** grid/cell mutation with no per-cell actor anchors.
- **Geometry processing:** cutting/reshaping/output generation, separate from placement.

The version 2 catalog stores these families in separate arrays and is consumed at runtime by `UPlacementKernelSubsystem`. Duplicate IDs, malformed required fields, invalid tolerances and an invalid two-support beam contract reject the entire catalog; placement is unavailable rather than guessing when the catalog cannot load.

## Log stations

The common station spacing is 75 cm. For actual measured length (L), produce `floor(L / 75)` logical surface stations at 37.5 cm plus 75 cm increments from the negative-axis endpoint, and two end-plane anchors derived from the actual mesh bounds. Thus a nominal 600 cm log has stations at 37.5, 112.5, 187.5, 262.5, 337.5, 412.5, 487.5 and 562.5 cm, plus two endpoint features.

One station record does not spawn four radial actor/socket variants. Round logs resolve the radial contact frame from the aim hint; hewn/square pieces resolve to the closest actual side face. The legacy `SideSnapSpacing` property remains serialized for Blueprint compatibility but no longer controls the global station interval.

Cut boundaries are not station centers. The 75 cm modular cut and existing 25 cm free cut remain separate geometry-processing profiles. A partial cut must not change topology; a committed cut must replace the physical piece set and rebuild all generated features from the actual new geometry.

## Runtime kernel

`UPlacementKernelSubsystem` loads `Config/Placement/SnapVariationLibrary.json` from the project config folder, rejects duplicate profile IDs, checks required profile fields and performs paired-anchor and rigid two-anchor transform solves. Candidate scoring is run only after hard constraints. A conservative broad-phase bounds query is available to profiles requiring collision validation.

`FPlacementTransactionJournal` captures actor transforms/attachments and the structural graph snapshot, records explicit undo actions for lifecycle/ownership mutations, and rolls back on scope exit unless committed. Canonical graph rehydration now uses a domain-neutral `IStructuralPieceInterface` adapter to clear and rebuild primary same-material links, cross-material links, secondary anchor occupancy and root physics state for logs, stones and sticks. If graph/actor/ownership restoration fails, it marks the world simulation faulted. This journal is a coordinated rollback mechanism; it does not make every Unreal physics side effect inherently reversible, so callers must use it around staged operations and revalidate after commit.

## Multi-support and failure policy

A two-support beam must define at least two required connections and two distinct supports, with atomic commit and no single-end downgrade or physics fallback. The solver does not scale a beam to fit between supports; it rejects a span mismatch above the profile's limit. Strut placement now commits the strut-to-post edge and the beam's secondary support edge inside the same transaction.

An ordinary unsupported end does not automatically start hinge physics. Only an anchor explicitly marked to allow hinge behavior may do so. A failed secondary support is never ignored: the candidate is rejected or the previous transform/graph state is restored.

## Material families

- Wood uses actual length/cross-section geometry and explicit joinery profiles.
- Stone uses face/edge/corner features; irregular resting contact is not a structural socket.
- Sticks use typed end/body features and explicit cross-material compatibility.
- Fire surface placement, fuel receiver, and heat/interaction regions are different capability types.
- Voxel edits and neighbor relations use integer grid/cell coordinates, never one physical snap actor per cell.

The catalog includes the initial planned families and variation profiles. Some operations (especially irregular stone surface fitting, joinery-specific collision allowances and voxel-world mutation) still depend on their own provider/operation adapters; their presence in the data catalog is a contract, not proof those domain runtimes already perform every behavior.

## Asset source of truth

`LOG_ASSET_MANIFEST` remains an FBX-source manifest. `BuildUnrealPlacementAssetCatalog.py` enumerates actual Unreal Content assets into a separate editor-generated catalog. Asset file names are hints only: dimensions are measured from loaded meshes; family, material profile, source FBX and runtime verification remain explicit fields. Generated .uasset variants are not labelled missing merely because the FBX scanner does not see them.

## Migration boundaries

Existing primary log snap now uses the kernel's paired-anchor solver, profile selection and runtime station transform. Existing stack placement remains a domain-specific grid strategy using the 75 cm modular span set; its commit path uses the transaction journal rather than pretending the preview grid is a pre-authored socket. Strut-to-post plus beam-secondary-support commits are grouped under one transaction. Material-specific primary links remain implemented by their domain providers, but graph snapshot restore now shares the same adapter lifecycle for log/stone/stick. Fire receivers and voxel mutation still need their world-operation adapters; the catalog alone does not implement those domain actions.

## Explicit verification status

- The runtime catalogue, paired-anchor solver, rigid two-anchor constraint solver and placement rollback journal exist in source. Current player-driven wood single-anchor snap is routed through the kernel; material and domain-specific paths are still being migrated and are not all globally solved.
- Two-support atomic helpers now check graph insertion return values; full two-anchor candidate discovery and all support-rest mutation paths are still an integration requirement, not claimed complete.
- Runtime code: added, not yet Unreal-compiled in this change.
- Unit/automation/PIE tests: intentionally not run, per scope.
- Asset catalog script: added; it has not been run in Unreal Editor.
- No gameplay-completion claim is inferred from source files alone.
