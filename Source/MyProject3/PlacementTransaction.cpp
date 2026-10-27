#include "PlacementTransaction.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "WorldSimulationCoordinatorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlacementTransaction, Log, All);

FPlacementTransactionJournal::FPlacementTransactionJournal(UWorld* InWorld)
    : World(InWorld)
{
    if (InWorld)
        Graph = InWorld->GetSubsystem<UWorldStructuralGraphSubsystem>();
}

FPlacementTransactionJournal::~FPlacementTransactionJournal()
{
    if (!bClosed)
        Rollback();
}

bool FPlacementTransactionJournal::RecordActor(AActor* Actor)
{
    if (bClosed || !IsValid(Actor))
        return false;
    for (const FActorSnapshot& Snapshot : ActorSnapshots)
    {
        if (Snapshot.Actor.Get() == Actor)
            return true;
    }

    FActorSnapshot Snapshot;
    Snapshot.Actor = Actor;
    Snapshot.WorldTransform = Actor->GetActorTransform();
    Snapshot.AttachParent = Actor->GetAttachParentActor();
    Snapshot.bWasAttached = Snapshot.AttachParent.IsValid();
    Snapshot.RelativeTransform = Actor->GetActorRelativeTransform();
    ActorSnapshots.Add(MoveTemp(Snapshot));
    return true;
}

bool FPlacementTransactionJournal::CaptureStructuralGraph()
{
    if (bClosed)
        return false;
    if (!Graph.IsValid())
    {
        bHasGraphSnapshot = false;
        return false;
    }
    if (!Graph->ExportSnapshot(GraphSnapshot))
        return false;
    bHasGraphSnapshot = true;
    return true;
}

FVector FPlacementTransactionJournal::GetRecordedWorldLocation(const AActor* Actor) const
{
    for (const FActorSnapshot& Snapshot : ActorSnapshots)
    {
        if (Snapshot.Actor.Get() == Actor)
            return Snapshot.WorldTransform.GetLocation();
    }
    return IsValid(Actor) ? Actor->GetActorLocation() : FVector::ZeroVector;
}

void FPlacementTransactionJournal::AddUndo(TFunction<bool()> UndoAction)
{
    if (!bClosed && UndoAction)
        UndoActions.Add(MoveTemp(UndoAction));
}

void FPlacementTransactionJournal::Commit()
{
    bClosed = true;
    UndoActions.Reset();
    ActorSnapshots.Reset();
    GraphSnapshot.Reset();
    bHasGraphSnapshot = false;
}

bool FPlacementTransactionJournal::RestoreActorSnapshot(const FActorSnapshot& Snapshot) const
{
    AActor* Actor = Snapshot.Actor.Get();
    if (!IsValid(Actor))
        return false;

    AActor* Parent = Snapshot.AttachParent.Get();
    if (Parent)
    {
        if (Actor->GetAttachParentActor() != Parent)
        {
            Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            if (!Actor->AttachToActor(Parent, FAttachmentTransformRules::KeepRelativeTransform))
                return false;
        }
        Actor->SetActorRelativeTransform(Snapshot.RelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }
    else
    {
        if (Actor->GetAttachParentActor())
            Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Actor->SetActorTransform(Snapshot.WorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }
    return true;
}

bool FPlacementTransactionJournal::Rollback()
{
    if (bClosed)
        return true;

    bool bSuccess = true;
    if (bHasGraphSnapshot && Graph.IsValid())
    {
        if (!Graph->RestoreSnapshot(GraphSnapshot))
        {
            bSuccess = false;
        }
        else if (!Graph->RebuildLogActorAttachmentsFromSnapshot())
        {
            bSuccess = false;
        }
    }

    for (int32 Index = UndoActions.Num() - 1; Index >= 0; --Index)
    {
        if (!UndoActions[Index]())
            bSuccess = false;
    }
    for (int32 Index = ActorSnapshots.Num() - 1; Index >= 0; --Index)
    {
        if (!RestoreActorSnapshot(ActorSnapshots[Index]))
            bSuccess = false;
    }

    if (!bSuccess)
    {
        UE_LOG(LogPlacementTransaction, Error,
            TEXT("Placement rollback failed. The world simulation is being marked faulted."));
        if (UWorld* StrongWorld = World.Get())
        {
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                StrongWorld->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Global placement transaction could not restore actor, ownership or structural graph state."));
            }
        }
    }

    bClosed = true;
    return bSuccess;
}
