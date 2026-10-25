#include "WorldRaidAdapterSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"

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

    if (Raid.State == EThreatRaidState::Warning &&
        (!MaterializedRaidId.IsValid() || MaterializedRaidId != Raid.RaidId))
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
        NextDamageTimeSeconds = Raid.ActiveStartTime;
    }

    if (Raid.State != EThreatRaidState::Active || Clock->GetSimulationTimeSeconds() < NextDamageTimeSeconds)
        return true;

    UWorldStructuralGraphSubsystem* Graph = GetStructuralGraph();
    if (!Graph) return false;
    for (const FGuid& NodeId : DefenseTargetNodeIds)
    {
        FStructuralNodeRecord Node;
        if (!Graph->GetNode(NodeId, Node)) continue;
        bool bThresholdExceeded = false;
        if (!Graph->ApplyDamage(NodeId, DamagePerPulse, bThresholdExceeded)) continue;
        if (bThresholdExceeded && !ResolveCollapsedSubtree(NodeId))
        {
            // Damage remains authoritative; the structure stays in the graph if
            // a safe leaf-first removal plan cannot be executed.
            return false;
        }
    }
    NextDamageTimeSeconds = Clock->GetSimulationTimeSeconds() + AttackIntervalSeconds;
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
