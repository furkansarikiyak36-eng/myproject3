#pragma once

#include "CoreMinimal.h"
#include "WorldStructuralGraphSubsystem.h"

class AActor;
class UWorld;

/**
 * Placement-local mutation journal. Records actor transforms/attachment,
 * the canonical structural graph snapshot, and reversible ledger/lifecycle
 * operations. The caller must call Commit() or Rollback(); the destructor
 * rolls back an open transaction as a last-resort safety net.
 */
class MYPROJECT3_API FPlacementTransactionJournal
{
public:
    explicit FPlacementTransactionJournal(UWorld* InWorld);
    ~FPlacementTransactionJournal();

    bool RecordActor(AActor* Actor);
    bool CaptureStructuralGraph();
    FVector GetRecordedWorldLocation(const AActor* Actor) const;
    void AddUndo(TFunction<bool()> UndoAction);
    void Commit();
    bool Rollback();
    bool IsOpen() const { return !bClosed; }

private:
    struct FActorSnapshot
    {
        TWeakObjectPtr<AActor> Actor;
        FTransform WorldTransform = FTransform::Identity;
        TWeakObjectPtr<AActor> AttachParent;
        FTransform RelativeTransform = FTransform::Identity;
        bool bWasAttached = false;
    };

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UWorldStructuralGraphSubsystem> Graph;
    TArray<FWorldStructuralNodeSnapshot> GraphSnapshot;
    TArray<FActorSnapshot> ActorSnapshots;
    TArray<TFunction<bool()>> UndoActions;
    bool bHasGraphSnapshot = false;
    bool bClosed = false;

    bool RestoreActorSnapshot(const FActorSnapshot& Snapshot) const;
};
