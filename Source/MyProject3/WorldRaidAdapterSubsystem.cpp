#include "WorldRaidAdapterSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldRegionalEventSubsystem.h"
#include "WorldGameDataSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldOperationLedgerSubsystem.h"
#include "InventoryCraftingComponent.h"
#include "EngineUtils.h"
#include "WorldSimulationCoordinatorSubsystem.h"

UWorldThreatDirectorSubsystem* UWorldRaidAdapterSubsystem::GetThreatDirector() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldThreatDirectorSubsystem>() : nullptr;
}

UWorldStructuralGraphSubsystem* UWorldRaidAdapterSubsystem::GetStructuralGraph() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
}

bool UWorldRaidAdapterSubsystem::AdvanceRaidWorldAdapter()
{
    UWorld* World = GetWorld();
    UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!World || !Threat || !Clock || !Threat->IsDirectorInitialized() ||
        !Clock->IsClockInitialized() || !FMath::IsFinite(SpawnHeightOffset) ||
        !FMath::IsFinite(DamagePerPulse) || DamagePerPulse <= 0.0f ||
        !FMath::IsFinite(AttackIntervalSeconds) || AttackIntervalSeconds <= 0.0 ||
        !FMath::IsFinite(DamageReductionPerDefenseRating) ||
        DamageReductionPerDefenseRating < 0.0f || DamageReductionPerDefenseRating > 1.0f ||
        !FMath::IsFinite(MinimumRaidDamageFraction) ||
        MinimumRaidDamageFraction <= 0.0f || MinimumRaidDamageFraction > 1.0f)
        return false;

    FThreatRaidRecord Raid;
    if (!Threat->GetCurrentRaid(Raid) ||
        Raid.State == EThreatRaidState::Repelled ||
        Raid.State == EThreatRaidState::Failed ||
        Raid.State == EThreatRaidState::Cancelled)
    {
        if (ActiveRaidPawn.IsValid()) ActiveRaidPawn->Destroy();
        ActiveRaidPawn.Reset();
        MaterializedRaidId.Invalidate();
        NextDamageTimeSeconds = 0.0;
        return true;
    }

    if ((Raid.State == EThreatRaidState::Warning || Raid.State == EThreatRaidState::Active) &&
        (!ActiveRaidPawn.IsValid() || !MaterializedRaidId.IsValid() || MaterializedRaidId != Raid.RaidId))
    {
        if (!RaidPawnClass) return false;
        const FVector SpawnLocation = Raid.TargetLocation + FVector(0.0f, 0.0f, SpawnHeightOffset);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        APawn* Spawned = World->SpawnActor<APawn>(RaidPawnClass, SpawnLocation, FRotator::ZeroRotator, Params);
        if (!Spawned) return false;
        Spawned->SpawnDefaultController();
        ActiveRaidPawn = Spawned;
        MaterializedRaidId = Raid.RaidId;
        if (Raid.LastAppliedDamagePulseIndex >= MAX_int64 - 1)
        {
            Spawned->Destroy();
            ActiveRaidPawn.Reset();
            MaterializedRaidId.Invalidate();
            return false;
        }
        NextDamageTimeSeconds = Raid.ActiveStartTime +
            (static_cast<double>(Raid.LastAppliedDamagePulseIndex) + 1.0) * AttackIntervalSeconds;
        if (!FMath::IsFinite(NextDamageTimeSeconds))
        {
            Spawned->Destroy();
            ActiveRaidPawn.Reset();
            MaterializedRaidId.Invalidate();
            return false;
        }
    }

    const double Now = Clock->GetSimulationTimeSeconds();
    if (Raid.State != EThreatRaidState::Active || Now < NextDamageTimeSeconds)
        return true;

    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    if (!Graph) return false;

    const double Elapsed = FMath::Max(0.0, Now - Raid.ActiveStartTime);
    const double PulseAsDouble = FMath::FloorToDouble(Elapsed / AttackIntervalSeconds);
    if (!FMath::IsFinite(PulseAsDouble) || PulseAsDouble < 0.0 ||
        PulseAsDouble >= static_cast<double>(MAX_int64 - 1))
        return false;
    const int64 PulseIndex = static_cast<int64>(PulseAsDouble);
    const double NextPulseTime = Raid.ActiveStartTime +
        (static_cast<double>(PulseIndex) + 1.0) * AttackIntervalSeconds;
    if (!FMath::IsFinite(NextPulseTime) || NextPulseTime <= Now)
        return false;

    if (!Threat->TryClaimDamagePulse(Raid.RaidId, PulseIndex))
    {
        FThreatRaidRecord LatestRaid;
        if (!Threat->GetCurrentRaid(LatestRaid) || LatestRaid.RaidId != Raid.RaidId ||
            LatestRaid.LastAppliedDamagePulseIndex >= MAX_int64 - 1)
            return false;
        NextDamageTimeSeconds = LatestRaid.ActiveStartTime +
            (static_cast<double>(LatestRaid.LastAppliedDamagePulseIndex) + 1.0) * AttackIntervalSeconds;
        return FMath::IsFinite(NextDamageTimeSeconds);
    }

    // The receipt and next deadline are committed before damage; a failed
    // collapse resolution cannot replay this pulse on the next call.
    NextDamageTimeSeconds = NextPulseTime;
    TArray<FGuid> SortedTargets = DefenseTargetNodeIds;
    SortedTargets.Sort([](const FGuid& A, const FGuid& B)
        { return A.ToString() < B.ToString(); });
    for (int32 Index = SortedTargets.Num() - 1; Index > 0; --Index)
        if (SortedTargets[Index] == SortedTargets[Index - 1])
            SortedTargets.RemoveAt(Index, 1, EAllowShrinking::No);

    float RegionalDefenseMultiplier = 1.0f;
    UWorldRegionalEventSubsystem* RegionalEvents = World->GetSubsystem<UWorldRegionalEventSubsystem>();
    FWorldRegionalEventEvaluation RegionalEvaluation;
    if (RegionalEvents && !Raid.TargetRegionId.IsNone() &&
        RegionalEvents->EvaluateRegion(Raid.TargetRegionId, RegionalEvaluation))
    {
        RegionalDefenseMultiplier = RegionalEvaluation.RaidDefenseMultiplier;
    }
    const float RegionDefenseRating = !Raid.TargetRegionId.IsNone()
        ? Graph->GetRegionDefenseRating(Raid.TargetRegionId) : 0.0f;
    const float DamageFraction = FMath::Clamp(
        1.0f - RegionDefenseRating * DamageReductionPerDefenseRating * RegionalDefenseMultiplier,
        MinimumRaidDamageFraction, 1.0f);
    const float EffectiveDamage = DamagePerPulse * DamageFraction;

    for (const FGuid& NodeId : SortedTargets)
    {
        FStructuralNodeRecord Node;
        if (!Graph->GetNode(NodeId, Node)) continue;
        bool bThresholdExceeded = false;
        if (!Graph->ApplyDamage(NodeId, EffectiveDamage, bThresholdExceeded)) continue;
        if (bThresholdExceeded && !ResolveCollapsedSubtree(NodeId))
        {
            return false;
        }
    }
    return FMath::IsFinite(NextDamageTimeSeconds);
}

bool UWorldRaidAdapterSubsystem::ResolveCollapsedSubtree(FGuid RootNodeId)
{
    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    if (!Graph) return false;
    TArray<FGuid> LeafFirstOrder;
    if (!Graph->BuildDependentRemovalOrder(RootNodeId, LeafFirstOrder)) return false;

    // Validate every actor before the first destructive action.
    TArray<TWeakObjectPtr<AActor>> Actors;
    Actors.Reserve(LeafFirstOrder.Num());
    for (const FGuid& NodeId : LeafFirstOrder)
    {
        FStructuralNodeRecord Node;
        if (!Graph->GetNode(NodeId, Node) || !Node.Actor.IsValid()) return false;
        Actors.Add(Node.Actor);
    }

    // Leaf-first order means each actor's EndPlay cleanup sees no remaining
    // dependents. Unreal actor destruction itself cannot be rolled back, so this
    // operation is preflighted and stops on the first unexpected failure.
    for (int32 Index = 0; Index < LeafFirstOrder.Num(); ++Index)
    {
        const FGuid NodeId = LeafFirstOrder[Index];
        AActor* Actor = Actors[Index].Get();
        if (IsValid(Actor) && !Actor->Destroy()) return false;

        FStructuralNodeRecord Remaining;
        if (Graph->GetNode(NodeId, Remaining) && !Graph->UnregisterNode(NodeId, true))
            return false;
    }
    return true;
}


bool UWorldRaidAdapterSubsystem::AdvanceRegionalHazardDamage(double DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0)
        return false;

    UWorld* World = GetWorld();
    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    if (!World || !Graph || !Events || !Events->AreDefinitionsLoaded())
        return false;

    const double Hours = DeltaSeconds / 3600.0;
    const TArray<FName> RegionIds = Events->GetRegionIds();
    const TArray<FGuid> SortedNodeIds = Graph->GetNodeIds();
    TMap<FName, float> DamagePerRegion;
    for (const FName RegionId : RegionIds)
    {
        FWorldRegionalEventEvaluation Evaluation;
        if (!Events->EvaluateRegion(RegionId, Evaluation))
            continue;
        const double Damage = static_cast<double>(Evaluation.StructureDamagePerHour) * Hours;
        if (!FMath::IsFinite(Damage) || Damage < 0.0 || Damage > static_cast<double>(MAX_flt))
            return false;
        if (Damage > 0.0)
            DamagePerRegion.Add(RegionId, static_cast<float>(Damage));
    }

    // Deterministic node ordering makes simultaneous storm/fire damage independent
    // of TMap iteration order. The evaluator has combined concurrently active rates.
    for (const FGuid& NodeId : SortedNodeIds)
    {
        FStructuralNodeRecord Node;
        if (!Graph->GetNode(NodeId, Node))
            continue;
        const float* Damage = DamagePerRegion.Find(Node.RegionId);
        if (!Damage || *Damage <= 0.0f)
            continue;

        bool bThresholdExceeded = false;
        if (!Graph->ApplyDamage(NodeId, *Damage, bThresholdExceeded))
            return false;
        if (bThresholdExceeded && !ResolveCollapsedSubtree(NodeId))
            return false;
    }

    return true;
}


bool UWorldRaidAdapterSubsystem::TryRepairStructureWithMaterial(
    const FGuid& OperationId, const FGuid& NodeId, int64 ExpectedNodeRevision,
    const FGuid& MaterialItemId, int64 ExpectedItemRevision,
    const FGuid& InventoryOwnerId, float RepairAmount)
{
    if (!OperationId.IsValid() || !NodeId.IsValid() || ExpectedNodeRevision <= 0 ||
        !MaterialItemId.IsValid() || ExpectedItemRevision <= 0 ||
        !InventoryOwnerId.IsValid() || !FMath::IsFinite(RepairAmount) ||
        RepairAmount <= 0.0f || RepairAmount > 100000.0f)
        return false;

    UWorld* World = GetWorld();
    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldOperationLedgerSubsystem* Ledger =
        World ? World->GetSubsystem<UWorldOperationLedgerSubsystem>() : nullptr;
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !Graph || !Ownership || !Ledger || !GameData || !GameData->IsLoaded())
        return false;

    const FString ReceiptText = FString::Printf(
        TEXT("StructuralRepair|%s|%s|%lld|%lld|%s|%.6f"),
        *NodeId.ToString(), *MaterialItemId.ToString(),
        static_cast<long long>(ExpectedNodeRevision),
        static_cast<long long>(ExpectedItemRevision),
        *InventoryOwnerId.ToString(), RepairAmount);

    FWorldOperationResult ExistingResult;
    if (Ledger->TryGetCompletedResult(OperationId, ExistingResult))
    {
        return ExistingResult.IsSuccess() && ExistingResult.Message.ToString() == ReceiptText &&
            ExistingResult.AffectedObjectIds.Contains(NodeId) &&
            ExistingResult.AffectedObjectIds.Contains(MaterialItemId);
    }

    FStructuralNodeRecord Node;
    FWorldOwnedItemRecord Material;
    if (!Graph->GetNode(NodeId, Node) || Node.Revision != ExpectedNodeRevision ||
        Node.Revision >= MAX_int64 - 1 || Node.Damage <= 0.0f ||
        !Ownership->TryGetItem(MaterialItemId, Material) ||
        Material.Revision != ExpectedItemRevision || Material.Revision >= MAX_int64 - 1 ||
        Material.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Material.OwnerId != InventoryOwnerId || Material.Quantity != 1 ||
        Material.DefinitionId.IsNone())
        return false;

    const FGameDataItem* MaterialDefinition = GameData->GetItems().Find(Material.DefinitionId);
    if (!MaterialDefinition ||
        (MaterialDefinition->Category != TEXT("Resource") &&
         MaterialDefinition->Category != TEXT("BuildingPart")))
        return false;

    UInventoryCraftingComponent* Inventory = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        UInventoryCraftingComponent* Candidate =
            IsValid(*It) ? (*It)->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
        if (!IsValid(Candidate) || Candidate->GetInventoryOwnerId() != InventoryOwnerId)
            continue;
        if (Inventory)
            return false;
        Inventory = Candidate;
    }
    if (!Inventory)
        return false;

    FGuid OwnerActorId;
    FGuid PreviousInventoryOwnerId;
    TArray<FInventoryStack> PreviousInventory;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousInventoryOwnerId,
        PreviousInventory, PreviousQuickAccess);
    if (PreviousInventoryOwnerId != InventoryOwnerId ||
        !PreviousInventory.ContainsByPredicate([&MaterialItemId](const FInventoryStack& Stack)
        {
            return Stack.OwnedWorldItemId == MaterialItemId &&
                Stack.Quantity == 1 && !Stack.ItemId.IsNone();
        }))
        return false;

    TArray<FWorldOwnedItemRecord> PreviousItems;
    TArray<FWorldItemTransferReceipt> PreviousTransferReceipts;
    TArray<FWorldItemSplitReceipt> PreviousSplitReceipts;
    Ownership->ExportSnapshot(PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);

    if (!Ledger->TryBeginOperation(OperationId))
        return false;

    auto Rollback = [&]() -> bool
    {
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);
        const bool bLedgerCancelled = Ledger->CancelUncommittedOperation(OperationId);
        const bool bSucceeded = bInventoryRestored && bOwnershipRestored && bLedgerCancelled;
        if (!bSucceeded)
        {
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Structural repair rollback failed; stop simulation and recover/reset the world."));
            }
        }
        return bSucceeded;
    };

    FInventoryStack RemovedStack;
    if (!Inventory->TryRemoveOwnedItemReference(MaterialItemId, RemovedStack))
    {
        Ledger->CancelUncommittedOperation(OperationId);
        return false;
    }

    if (!Ownership->TryConsumeItem(OperationId, MaterialItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Inventory))
    {
        Rollback();
        return false;
    }

    FWorldOwnedItemRecord ConsumedMaterial;
    if (!Ownership->TryGetItem(MaterialItemId, ConsumedMaterial) ||
        ConsumedMaterial.OwnerKind != EWorldItemOwnerKind::Consumed ||
        !Graph->RepairDamage(NodeId, RepairAmount))
    {
        Rollback();
        return false;
    }

    FWorldOperationResult Result;
    Result.bSucceeded = true;
    Result.Code = EWorldOperationResultCode::Success;
    Result.OperationId = OperationId;
    Result.Message = FText::FromString(ReceiptText);
    Result.AffectedObjectIds.Add(NodeId);
    Result.AffectedObjectIds.Add(MaterialItemId);
    if (!Ledger->CompleteOperation(Result))
    {
        const float AppliedRepair = FMath::Min(Node.Damage, RepairAmount);
        bool bThresholdExceeded = false;
        const bool bNodeRestored = Graph->ApplyDamage(NodeId, AppliedRepair, bThresholdExceeded);
        const bool bOtherStateRestored = Rollback();
        if (!bNodeRestored || !bOtherStateRestored)
        {
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Structural repair completion rollback failed; stop simulation and recover/reset the world."));
            }
        }
        return false;
    }

    return true;
}
