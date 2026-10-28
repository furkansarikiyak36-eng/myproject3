#include "WorldRaidAdapterSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayCoreTypes.h"
#include "RaidPlaceholderPawn.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldOperationLedgerSubsystem.h"

namespace
{
bool IsTerminalRaidState(EThreatRaidState State)
{
    return State == EThreatRaidState::Repelled ||
        State == EThreatRaidState::Failed ||
        State == EThreatRaidState::Cancelled;
}

FGuid MakeRaidPulseOperationId(const FGuid& RaidId, int64 PulseIndex)
{
    const uint64 Index = static_cast<uint64>(PulseIndex);
    FGuid OperationId(
        RaidId.A ^ 0xBADA551Du,
        RaidId.B ^ 0x7A1DCAFEu,
        RaidId.C ^ static_cast<uint32>(Index),
        RaidId.D ^ static_cast<uint32>(Index >> 32));
    if (!OperationId.IsValid())
        OperationId.D = 1;
    return OperationId;
}
}

UWorldRaidAdapterSubsystem::UWorldRaidAdapterSubsystem()
{
    // A primitive-only pawn keeps the demo usable until a project-specific
    // enemy pawn and AI behavior are authored.
    RaidPawnClass = ARaidPlaceholderPawn::StaticClass();
}

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

bool UWorldRaidAdapterSubsystem::CanAdvanceRaidWorldAdapterAtTime(double TargetTimeSeconds) const
{
    UWorld* World = GetWorld();
    const UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    const UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    const UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    const UWorldOperationLedgerSubsystem* OperationLedger =
        World ? World->GetSubsystem<UWorldOperationLedgerSubsystem>() : nullptr;
    if (!World || !Threat || !Clock || !OperationLedger ||
        !Threat->IsDirectorInitialized() || !Clock->IsClockInitialized() ||
        !FMath::IsFinite(TargetTimeSeconds) ||
        TargetTimeSeconds < Clock->GetSimulationTimeSeconds() ||
        !FMath::IsFinite(SpawnHeightOffset) || SpawnHeightOffset < 0.0f ||
        !FMath::IsFinite(DamagePerPulse) || DamagePerPulse <= 0.0f ||
        !FMath::IsFinite(AttackIntervalSeconds) || AttackIntervalSeconds <= 0.0 ||
        !Graph)
    {
        return false;
    }

    FThreatRaidRecord CurrentRaid;
    const bool bHasRaid = Threat->GetCurrentRaid(CurrentRaid);
    const bool bRaidWillFinish =
        bHasRaid && !IsTerminalRaidState(CurrentRaid.State) &&
        CurrentRaid.State != EThreatRaidState::None &&
        TargetTimeSeconds >= CurrentRaid.PlannedEndTime;
    const bool bCanScheduleNext =
        !bHasRaid || IsTerminalRaidState(CurrentRaid.State) || bRaidWillFinish;
    const bool bRaidWillStart =
        bCanScheduleNext && TargetTimeSeconds >= Threat->GetNextRaidTimeSeconds();
    const bool bRaidNeedsPawn =
        (bHasRaid && (CurrentRaid.State == EThreatRaidState::Warning ||
                      CurrentRaid.State == EThreatRaidState::Active) && !bRaidWillFinish) ||
        bRaidWillStart;
    if (bRaidNeedsPawn && (!RaidPawnClass || RaidPawnClass->HasAnyClassFlags(
        CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)))
        return false;

    if (!bHasRaid || CurrentRaid.State == EThreatRaidState::None ||
        IsTerminalRaidState(CurrentRaid.State) || bRaidWillFinish)
    {
        return true;
    }

    const bool bWarningWillActivate =
        CurrentRaid.State == EThreatRaidState::Warning &&
        TargetTimeSeconds >= CurrentRaid.ActiveStartTime;
    const bool bWillBeActive =
        (CurrentRaid.State == EThreatRaidState::Active && !bRaidWillFinish) ||
        (CurrentRaid.State == EThreatRaidState::Warning &&
         TargetTimeSeconds >= CurrentRaid.ActiveStartTime &&
         TargetTimeSeconds < CurrentRaid.PlannedEndTime);

    // A newly created warning freezes the authored interval into its own raid
    // record. After that, the save's value wins even if tuning changes.
    double EffectiveIntervalSeconds = CurrentRaid.AttackIntervalSeconds;
    const bool bNeedsIntervalInitialization =
        CurrentRaid.State == EThreatRaidState::Warning &&
        (!FMath::IsFinite(EffectiveIntervalSeconds) || EffectiveIntervalSeconds <= 0.0);
    if (bNeedsIntervalInitialization)
        EffectiveIntervalSeconds = AttackIntervalSeconds;

    int64 RequiredRevisionIncrements = 0;
    if (CurrentRaid.State == EThreatRaidState::Warning && !bRaidWillFinish)
    {
        if (bWarningWillActivate)
            ++RequiredRevisionIncrements;
        if (bNeedsIntervalInitialization)
            ++RequiredRevisionIncrements;
    }

    if (bWillBeActive)
    {
        if (!FMath::IsFinite(EffectiveIntervalSeconds) || EffectiveIntervalSeconds <= 0.0)
            return false;

        const double Elapsed = TargetTimeSeconds - CurrentRaid.ActiveStartTime;
        const double PulseIndexAsDouble = FMath::FloorToDouble(Elapsed / EffectiveIntervalSeconds);
        if (!FMath::IsFinite(Elapsed) || Elapsed < 0.0 ||
            !FMath::IsFinite(PulseIndexAsDouble) || PulseIndexAsDouble < 0.0 ||
            PulseIndexAsDouble >= static_cast<double>(MAX_int64 - 1))
        {
            return false;
        }

        const int64 PulseIndex = static_cast<int64>(PulseIndexAsDouble);
        const double NextPulseTime = CurrentRaid.ActiveStartTime +
            (static_cast<double>(PulseIndex) + 1.0) * EffectiveIntervalSeconds;
        if (!FMath::IsFinite(NextPulseTime))
            return false;

        if (PulseIndex > CurrentRaid.LastCommittedAttackPulseIndex)
        {
            ++RequiredRevisionIncrements;
            const FGuid PulseOperationId = MakeRaidPulseOperationId(CurrentRaid.RaidId, PulseIndex);
            FWorldOperationResult PreviousResult;
            const bool bHasCompletedReceipt =
                OperationLedger->TryGetCompletedResult(PulseOperationId, PreviousResult);
            if (bHasCompletedReceipt)
            {
                if (!PreviousResult.IsSuccess())
                    return false;
            }
            else
            {
                if (!OperationLedger->CanBeginOperation(PulseOperationId))
                    return false;

                TArray<FGuid> DamageTargets;
                if (!BuildDamageTargetList(CurrentRaid, DamageTargets))
                    return false;

                TSet<FGuid> UniqueTargets;
                for (const FGuid& NodeId : DamageTargets)
                {
                    if (!NodeId.IsValid() || UniqueTargets.Contains(NodeId))
                        return false;
                    UniqueTargets.Add(NodeId);

                    FStructuralNodeRecord Node;
                    if (!Graph->GetNode(NodeId, Node) || !Node.Actor.IsValid() ||
                        Node.Revision == MAX_int64 || !FMath::IsFinite(Node.Damage) ||
                        !FMath::IsFinite(Node.DamageThreshold) ||
                        !FMath::IsFinite(Node.Damage + DamagePerPulse))
                    {
                        return false;
                    }

                    if (Node.Damage + DamagePerPulse >= Node.DamageThreshold)
                    {
                        TArray<FGuid> RemovalOrder;
                        if (!Graph->BuildDependentRemovalOrder(NodeId, RemovalOrder))
                            return false;
                        for (const FGuid& RemovalId : RemovalOrder)
                        {
                            FStructuralNodeRecord RemovalNode;
                            if (!Graph->GetNode(RemovalId, RemovalNode) ||
                                !RemovalNode.Actor.IsValid())
                            {
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }

    // State transition, first interval freeze and attack-pulse commit can each
    // advance Revision in the same fixed step; reserve room for all of them.
    return CurrentRaid.Revision >= 0 &&
        CurrentRaid.Revision <= MAX_int64 - RequiredRevisionIncrements;
}

bool UWorldRaidAdapterSubsystem::ResolvePrimaryTargetNode(
    const FThreatRaidRecord& Raid, FGuid& OutNodeId, FVector& OutLocation) const
{
    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    if (!Graph)
        return false;

    TArray<FGuid> Candidates = DefenseTargetNodeIds;
    if (Candidates.IsEmpty())
    {
        TArray<FWorldStructuralNodeSnapshot> Nodes;
        if (!Graph->ExportSnapshot(Nodes))
            return false;
        Candidates.Reserve(Nodes.Num());
        for (const FWorldStructuralNodeSnapshot& Node : Nodes)
            Candidates.Add(Node.NodeId);
    }

    bool bFound = false;
    double BestDistanceSquared = TNumericLimits<double>::Max();
    FString BestIdKey;
    for (const FGuid& CandidateId : Candidates)
    {
        FStructuralNodeRecord Node;
        if (!CandidateId.IsValid() || !Graph->GetNode(CandidateId, Node))
            continue;

        AActor* Actor = Node.Actor.Get();
        if (!IsValid(Actor))
            continue;

        const FVector Location = Actor->GetActorLocation();
        if (Location.ContainsNaN())
            continue;
        const double DistanceSquared = FVector::DistSquared(Location, Raid.TargetLocation);
        if (!FMath::IsFinite(DistanceSquared))
            continue;

        const FString CandidateKey = CandidateId.ToString();
        if (!bFound || DistanceSquared < BestDistanceSquared ||
            (DistanceSquared == BestDistanceSquared && CandidateKey < BestIdKey))
        {
            bFound = true;
            BestDistanceSquared = DistanceSquared;
            BestIdKey = CandidateKey;
            OutNodeId = CandidateId;
            OutLocation = Location;
        }
    }
    return bFound;
}

bool UWorldRaidAdapterSubsystem::BuildDamageTargetList(
    const FThreatRaidRecord& Raid, TArray<FGuid>& OutTargetIds) const
{
    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    if (!Graph)
        return false;

    OutTargetIds.Reset();
    if (DefenseTargetNodeIds.IsEmpty())
    {
        FGuid TargetNodeId;
        FVector TargetLocation;
        if (MaterializedTargetNodeId.IsValid())
        {
            FStructuralNodeRecord ExistingTarget;
            if (Graph->GetNode(MaterializedTargetNodeId, ExistingTarget) &&
                ExistingTarget.Actor.IsValid())
            {
                TargetNodeId = MaterializedTargetNodeId;
                TargetLocation = ExistingTarget.Actor->GetActorLocation();
            }
        }
        if (!TargetNodeId.IsValid() &&
            !ResolvePrimaryTargetNode(Raid, TargetNodeId, TargetLocation))
        {
            // A world with no registered structures has nothing for the raid to damage.
            return true;
        }
        OutTargetIds.Add(TargetNodeId);
        return true;
    }

    TSet<FGuid> UniqueTargets;
    OutTargetIds = DefenseTargetNodeIds;
    for (const FGuid& TargetId : OutTargetIds)
    {
        FStructuralNodeRecord Node;
        if (!TargetId.IsValid() || UniqueTargets.Contains(TargetId) ||
            !Graph->GetNode(TargetId, Node) || !Node.Actor.IsValid())
        {
            return false;
        }
        UniqueTargets.Add(TargetId);
    }
    OutTargetIds.Sort([](const FGuid& A, const FGuid& B)
    {
        return A.ToString() < B.ToString();
    });
    return true;
}

bool UWorldRaidAdapterSubsystem::AdvanceRaidWorldAdapter()
{
    UWorld* World = GetWorld();
    UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    UWorldOperationLedgerSubsystem* OperationLedger =
        World ? World->GetSubsystem<UWorldOperationLedgerSubsystem>() : nullptr;
    if (!World || !Threat || !Clock || !Graph || !OperationLedger ||
        !Threat->IsDirectorInitialized() || !Clock->IsClockInitialized() ||
        !FMath::IsFinite(SpawnHeightOffset) || SpawnHeightOffset < 0.0f ||
        !FMath::IsFinite(DamagePerPulse) || DamagePerPulse <= 0.0f ||
        !FMath::IsFinite(AttackIntervalSeconds) || AttackIntervalSeconds <= 0.0)
    {
        return false;
    }

    FThreatRaidRecord Raid;
    if (!Threat->GetCurrentRaid(Raid) || IsTerminalRaidState(Raid.State))
    {
        if (ActiveRaidPawn.IsValid() && !ActiveRaidPawn->Destroy())
            return false;
        ActiveRaidPawn.Reset();
        MaterializedRaidId.Invalidate();
        MaterializedTargetNodeId.Invalidate();
        NextDamageTimeSeconds = 0.0;
        return true;
    }

    if (Raid.State == EThreatRaidState::Warning &&
        (!FMath::IsFinite(Raid.AttackIntervalSeconds) || Raid.AttackIntervalSeconds <= 0.0))
    {
        if (!Threat->InitializeRaidAttackInterval(Raid.RaidId, AttackIntervalSeconds) ||
            !Threat->GetCurrentRaid(Raid))
        {
            return false;
        }
    }
    if (!FMath::IsFinite(Raid.AttackIntervalSeconds) || Raid.AttackIntervalSeconds <= 0.0)
        return false;
    const double EffectiveIntervalSeconds = Raid.AttackIntervalSeconds;

    const bool bSameRaid = MaterializedRaidId.IsValid() &&
        MaterializedRaidId == Raid.RaidId;
    if ((Raid.State == EThreatRaidState::Warning || Raid.State == EThreatRaidState::Active) &&
        (!bSameRaid || !ActiveRaidPawn.IsValid()))
    {
        // The pawn is a transient visual adapter and is recreated from the
        // authoritative raid record after load. Do not orphan a pawn belonging
        // to the raid that was active before a successful snapshot restore.
        if (!bSameRaid && ActiveRaidPawn.IsValid())
        {
            if (!ActiveRaidPawn->Destroy())
                return false;
            ActiveRaidPawn.Reset();
            MaterializedRaidId.Invalidate();
            MaterializedTargetNodeId.Invalidate();
        }

        if (!RaidPawnClass || RaidPawnClass->HasAnyClassFlags(
            CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
            return false;

        FVector BaseLocation = Raid.TargetLocation;
        FGuid TargetNodeId;
        FVector TargetLocation;
        if (ResolvePrimaryTargetNode(Raid, TargetNodeId, TargetLocation))
        {
            BaseLocation = TargetLocation;
            MaterializedTargetNodeId = TargetNodeId;
        }
        else
        {
            MaterializedTargetNodeId.Invalidate();
        }

        const FVector SpawnLocation = BaseLocation + FVector(0.0f, 0.0f, SpawnHeightOffset);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        APawn* Spawned = World->SpawnActor<APawn>(
            RaidPawnClass, SpawnLocation, FRotator::ZeroRotator, Params);
        if (!Spawned)
            return false;

        Spawned->SpawnDefaultController();
        ActiveRaidPawn = Spawned;
        MaterializedRaidId = Raid.RaidId;
        if (!bSameRaid || !FMath::IsFinite(NextDamageTimeSeconds) ||
            NextDamageTimeSeconds <= 0.0)
        {
            NextDamageTimeSeconds = Raid.ActiveStartTime;
        }
    }

    const double Now = Clock->GetSimulationTimeSeconds();
    if (Raid.State != EThreatRaidState::Active || Now < Raid.ActiveStartTime)
        return true;

    const double Elapsed = Now - Raid.ActiveStartTime;
    const double PulseIndexAsDouble = FMath::FloorToDouble(Elapsed / EffectiveIntervalSeconds);
    if (!FMath::IsFinite(Elapsed) || Elapsed < 0.0 ||
        !FMath::IsFinite(PulseIndexAsDouble) || PulseIndexAsDouble < 0.0 ||
        PulseIndexAsDouble >= static_cast<double>(MAX_int64 - 1))
    {
        return false;
    }

    // Pulse identity is a pure function of the raid and its scheduled pulse
    // index. The persisted operation ledger prevents a save/load from applying
    // the same structural damage pulse twice.
    const int64 PulseIndex = static_cast<int64>(PulseIndexAsDouble);
    const FGuid PulseOperationId = MakeRaidPulseOperationId(Raid.RaidId, PulseIndex);

    // The watermark is authoritative and persisted with the raid record. If a
    // save/load or retry reaches an already committed pulse, never re-apply it.
    if (PulseIndex <= Raid.LastCommittedAttackPulseIndex)
    {
        NextDamageTimeSeconds = Raid.ActiveStartTime +
            (static_cast<double>(PulseIndex) + 1.0) * EffectiveIntervalSeconds;
        return FMath::IsFinite(NextDamageTimeSeconds);
    }

    FWorldOperationResult PreviousResult;
    if (OperationLedger->TryGetCompletedResult(PulseOperationId, PreviousResult))
    {
        if (!PreviousResult.IsSuccess())
            return false;
        // Compatibility guard for a receipt produced by an earlier adapter
        // revision: upgrade the durable watermark before skipping the pulse.
        if (!Threat->CommitRaidAttackPulse(Raid.RaidId, PulseIndex) ||
            !OperationLedger->ForgetCompletedOperationWithExternalReceipt(PulseOperationId))
        {
            return false;
        }
        NextDamageTimeSeconds = Raid.ActiveStartTime +
            (static_cast<double>(PulseIndex) + 1.0) * AttackIntervalSeconds;
        return FMath::IsFinite(NextDamageTimeSeconds);
    }

    TArray<FGuid> DamageTargets;
    if (DefenseTargetNodeIds.IsEmpty())
    {
        FGuid NearestNodeId;
        FVector NearestLocation;
        if (MaterializedTargetNodeId.IsValid())
        {
            FStructuralNodeRecord Node;
            if (Graph->GetNode(MaterializedTargetNodeId, Node) && Node.Actor.IsValid())
                NearestNodeId = MaterializedTargetNodeId;
        }
        if (!NearestNodeId.IsValid() &&
            ResolvePrimaryTargetNode(Raid, NearestNodeId, NearestLocation))
        {
            MaterializedTargetNodeId = NearestNodeId;
        }
        if (NearestNodeId.IsValid())
            DamageTargets.Add(NearestNodeId);
    }
    else
    {
        DamageTargets = DefenseTargetNodeIds;
        DamageTargets.Sort([](const FGuid& A, const FGuid& B)
        {
            return A.ToString() < B.ToString();
        });
    }

    // Preflight every threshold-crossing collapse before the first damage
    // mutation. ResolveCollapsedSubtree repeats this guard at commit time.
    TSet<FGuid> UniqueTargets;
    for (const FGuid& NodeId : DamageTargets)
    {
        if (!NodeId.IsValid() || UniqueTargets.Contains(NodeId))
            continue;
        UniqueTargets.Add(NodeId);

        FStructuralNodeRecord Node;
        if (!Graph->GetNode(NodeId, Node))
            continue;
        if (Node.Damage + DamagePerPulse >= Node.DamageThreshold)
        {
            TArray<FGuid> RemovalOrder;
            if (!Graph->BuildDependentRemovalOrder(NodeId, RemovalOrder))
                return false;
            for (const FGuid& RemovalId : RemovalOrder)
            {
                FStructuralNodeRecord RemovalNode;
                if (!Graph->GetNode(RemovalId, RemovalNode) || !RemovalNode.Actor.IsValid())
                    return false;
            }
        }
    }

    // Reserve the durable operation only after all predictable graph work has
    // passed preflight. From this point a failure leaves a Pending receipt and
    // must stop the simulation rather than replay potentially partial damage.
    if (!OperationLedger->TryBeginOperation(PulseOperationId))
        return false;

    TArray<FGuid> AffectedNodeIds;
    for (const FGuid& NodeId : DamageTargets)
    {
        if (!NodeId.IsValid() || AffectedNodeIds.Contains(NodeId))
            continue;

        FStructuralNodeRecord Node;
        if (!Graph->GetNode(NodeId, Node))
            continue;

        bool bThresholdExceeded = false;
        if (!Graph->ApplyDamage(NodeId, DamagePerPulse, bThresholdExceeded))
        {
            // The operation remains Pending. The coordinator will fail closed
            // and durable save refuses to serialize this partial transaction.
            return false;
        }
        AffectedNodeIds.Add(NodeId);

        if (bThresholdExceeded && !ResolveCollapsedSubtree(NodeId))
            return false;
    }

    // Commit the persistent watermark only after the structural damage work
    // has succeeded. If this commit fails, leave the pending operation marker;
    // the coordinator will fault closed and save/load will reject the partial tick.
    if (!Threat->CommitRaidAttackPulse(Raid.RaidId, PulseIndex))
        return false;

    const FWorldOperationResult PulseResult = FWorldOperationResult::Success(
        PulseOperationId, FText::FromString(TEXT("Raid attack pulse committed")), AffectedNodeIds);
    if (!OperationLedger->CompleteOperation(PulseResult))
        return false;

    // The just-committed watermark now durably represents this pulse. Drop the
    // temporary completed row only after that authoritative receipt has changed.
    if (!OperationLedger->ForgetCompletedOperationWithExternalReceipt(PulseOperationId))
        return false;

    NextDamageTimeSeconds = Raid.ActiveStartTime +
        (static_cast<double>(PulseIndex) + 1.0) * AttackIntervalSeconds;
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
