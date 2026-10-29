#include "WorldRaidAdapterSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "StoneBuildingPiece.h"

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
        !FMath::IsFinite(AttackIntervalSeconds) || AttackIntervalSeconds <= 0.0)
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

    for (const FGuid& NodeId : SortedTargets)
    {
        FStructuralNodeRecord Node;
        if (!Graph->GetNode(NodeId, Node)) continue;

        // Open doors and gates, and authored wall openings, are not raid
        // obstacles: do not spend a damage pulse on a passage that is already clear.
        if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Node.Actor.Get()))
            if (!Stone->IsRaidBlocking())
                continue;

        bool bThresholdExceeded = false;
        if (!Graph->ApplyDamage(NodeId, DamagePerPulse, bThresholdExceeded)) continue;
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
    FStructuralNodeRecord Root;
    if (!Graph->GetNode(RootNodeId, Root))
        return false;
    // The graph owns the destructive transaction: it preflights every child,
    // consumes unique source ownership, clears actor anchors and removes all
    // graph nodes before any actor is destroyed. Raid collapse yields no salvage.
    return Graph->TryCollapseConnectedStructure(RootNodeId, Root.Revision);
}
