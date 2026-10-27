# P00 — Candidate Source Symbol Map

**Source inspected:** `feature/simple-intent` headers and existing architecture/coverage notes.  
**Not inspected/verified here:** a fresh compiled target, full caller graph for every mechanic, Unreal Editor, PIE, save/load, or runtime behavior.  
**Per-mechanic ledger:** [PHASE_00_M001_M119_TRACEABILITY_MATRIX.csv](PHASE_00_M001_M119_TRACEABILITY_MATRIX.csv).

This map distinguishes a declared method from a complete mechanic. An existing method can be only one part of a mechanic, a wrapper, an inactive code path, or an untested candidate.

## Source symbol families

| Domain | Candidate source symbols / entry points | Interpretation and remaining gap |
|---|---|---|
| Generic identity/lifecycle | `UWorldObjectIdentityComponent::EnsureIdentity/TryTransitionLifecycle/IsLifecycleTransitionAllowed`; `UWorldObjectIdentitySubsystem::RegisterIdentity` | Reuse this identity/lifecycle layer; opt-in migration across domain actors and full actor restore still require verification. |
| Operation/request/result | `FWorldOperationRequest::IsStructurallyValid`; `FWorldOperationResult::Success/Failure/IsSuccess`; `UWorldOperationLedgerSubsystem::TryBeginOperation/CompleteOperation` | Result codes and receipts exist as a source contract. The ledger does not make arbitrary cross-subsystem changes atomic by itself. |
| Committed events | `UWorldCommittedEventSubsystem::PublishCommittedEvent` | Committed facts should publish after the authoritative domain result; this is a transient notification, not a durable event log. |
| Logical clock/coordinator | `UWorldSimulationClockSubsystem::InitializeClock/CanAdvanceSimulation/AdvanceSimulation/ExportSnapshot/RestoreSnapshot`; `UWorldSimulationCoordinatorSubsystem::InitializeWorldSimulation/AdvanceFixedStep` | Systems not wired into this coordinator do not advance just because the clock exists. |
| Save coordinator | `UWorldSaveCoordinatorSubsystem::SaveWorldState/LoadWorldState/CaptureCurrentState/ApplySnapshot` | Logical snapshots exist; full actor transforms, attachment graph and voxel mesh/collision restoration are not established. |
| Wood pickup/carry | `ULogCarryComponent::TryPickupTarget/PlaceHeldPiece/DropHeldPiece/TryShoulderLog/BeginDragAimedLog/TryPushOrRollAimedLog`; `ALogBuildPiece::SetHeldState/TryTransferOwnership/CanBePickedUp` | These are principal candidates for physical item interactions; exact owner transfers and retry/failure paths need per-task tracing. |
| Wood cutting/processing | `ULogCarryComponent::ResolveAimedCutChoice/CanMakeCut/CutLogAtShare/ProcessAimedPiece`; `ALogBuildPiece::TryAdvanceCutProgress/TryApplyCutAction/CommitCutAction/TrySplitOutputPieces/TryCommitPhysicalSplitOutputs/TryResolveGeometryVariant` | A method is not proof of correct mesh/state transition. Cross-cut half-length and longitudinal split-half geometry must stay distinct. |
| Wood snapping/joins | `ULogCarryComponent::FindBestSnapCandidate/CommitHeldSolution/CommitHeldLogAt/SolveStackOn`; `ALogBuildPiece::AddCustomSnapPoint/TryReserveSnapPoints/AttachToSnapParent/DetachFromSnapParent/TryJoinById`; `Config/Placement/PlacementProfiles.json` | One shared, material-neutral placement kernel is the design authority. Preview/commit parity, mirrored crossings, deck path and authored notch sockets remain acceptance items. |
| Stone primitives | `ULogCarryComponent::TryPickupMaterialPiece`; `AStoneBuildingPiece::TryCommitGroundPlacement/TryConnectToAnchor/CanConnectToAnchor/AddCustomAnchor/RemoveConnection` | Stone placement/anchors exist in the candidate source. No dedicated staged raw-boulder fracture/shaping operation was confirmed in the reviewed API. |
| Stick/traps | `AStickPrimitivePiece::TryProcessTo/TryCommitGroundPlacement/TryArmTrap/TryTriggerTrap/DisarmTrap/TryConnectToAnchor/RemoveConnection` | Logical trap states and primitive placement methods exist. Ground-contact requirements, trap persistence, trigger effect and salvage behavior need explicit acceptance. |
| Inventory/crafting | `UInventoryCraftingComponent::TryAddItem/GetItemCount/GetCurrentWeightGrams/HasCapacityFor/RegisterRecipe/CanCraft/TryCraft`; `UWorldItemOwnershipSubsystem::TryTransferItem/TryConsumeItem` | Item-definition IDs and unique world-item GUIDs are different namespaces. Atomic rollback, physical actor materialization and complete inventory UI remain open. |
| Structural graph | `UWorldStructuralGraphSubsystem::RegisterNode/TryConnect/TryDisconnect/TryAddSecondarySupport/SetGroundSupported/HasValidSupportPath/CanSafelyRemove/BuildDependentRemovalOrder/GetAggregateLoad/ApplyDamage/RepairDamage` | Shared logical graph methods exist. Actor attachment, graph changes, inventory ownership and damage/salvage must be reconciled as one bounded transaction. |
| Terrain cells/chunk prototype | `UWorldTerrainStateSubsystem::TryApplyCellEdit/TryGetCell/ExportSnapshot/RestoreSnapshot`; `UVoxelTerrainChunkComponent::InitializeFlatGround/ApplySphereEdit/RebuildMesh/SampleDensity` | Logical edited-cell state and one Marching Cubes chunk prototype are separate layers. Tunnels, neighboring chunks, material yields, support links, collision/navigation and persistent density edits are not confirmed end-to-end. |
| Clocked agriculture | `UWorldAgricultureSubsystem::RegisterCrop/SetCropMoisture/AdvanceAgricultureStep/GetCrop/RegisterAnimal/FeedAnimal/GetAnimal` | Logical crop/animal state exists. Player plant/harvest item transactions, output materialization, soil quality/season windows and animal product/reproduction paths need further work. |
| Threat scheduling | `UWorldThreatDirectorSubsystem::InitializeDirector/AdvanceDirectorAtTime/ResolveCurrentRaid/CancelCurrentRaid/GetCurrentRaid`; `UWorldRaidAdapterSubsystem::AdvanceRaidWorldAdapter/ResolveCollapsedSubtree` | Scheduling and a raid adapter are not a full attack AI. Enemy composition, spawn selection, targeting/navigation and context-based escalation are not confirmed. |
| Logistics/storage/production | `UWorldLogisticsRouteSubsystem::UpsertNode/UpsertEdge/FindRoute`; `UWorldCargoTransportSubsystem::StartTransport/AdvanceCargoStep/RetryBlockedDelivery`; `UWorldStorageSubsystem::TryReserveIncoming/CommitIncomingReservation`; `UWorldProductionSubsystem::TryIngestWorldOwnedLogPiece/TryEnqueueJob/AdvanceProductionStep/TryCollectCompletedJob/TryMaterializeStockAsWorldLog` | Logical route/cargo/queue records exist; physical transport, dynamic route cost and real machine/fuel/heat outputs remain incomplete. |
| Survival/health/shelter/discovery | `UWorldSurvivalSubsystem::UpdateProtection/SetWeather/UpdateLocation/RecordActivity/ConsumeFood/ComputeFeltTemperature/AdvanceSurvivalStep/RecordDiscovery/GetDiscoveries`; `UPhysicalHealthComponent::ApplyPhysicalDamage`; `UShelterSensorComponent::MeasureNow/GetShelterProtection` | State and interfaces exist, but poisoning, sleep/rest action, spoilage, equipment/inventory coupling and full shelter/exposure runtime need review. |

## Direct-owner gaps found in the inspected candidate API

The matrix marks 30 IDs as having no dedicated direct owner confirmed in the reviewed candidate headers/status notes: **M009, M010, M014, M028, M030, M033, M034, M042, M043, M051, M056, M057, M058, M064, M069, M074, M077, M079, M085, M088, M089, M090, M093, M094, M095, M096, M100, M105, M114, M116**.

“Not found” means the reviewed candidate source/docs did not confirm a dedicated owning API. It is not a proof the feature is absent from every content asset or a user's local unpushed workspace. These entries need exact source/content searches before being finally classified.

## Next actions

1. Trace each M001–M119 row through the request entry point, owning mutation, ownership transfer, commit event and save snapshot.
2. Replace broad file families with exact owning function/caller references and rejection-path tests.
3. Preserve the main-only commits after the candidate branch's merge base.
4. Do not merge `feature/simple-intent` or resolve snap PRs by commit count alone.
5. Re-run UE 5.8 UHT/UBT and Editor/PIE on the deliberate integration base and record it separately from historic evidence.
