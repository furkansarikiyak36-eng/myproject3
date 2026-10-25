#include "WorldOperationLedgerSubsystem.h"

bool UWorldOperationLedgerSubsystem::TryBeginOperation(const FGuid& OperationId)
{
    if (!OperationId.IsValid() ||
        Entries.Contains(OperationId) ||
        MaxEntries <= 0 ||
        Entries.Num() >= MaxEntries)
    {
        return false;
    }

    FWorldOperationLedgerEntry Entry;
    Entry.OperationId = OperationId;
    Entry.State = EWorldOperationLedgerEntryState::Pending;
    Entries.Add(OperationId, Entry);
    return true;
}

bool UWorldOperationLedgerSubsystem::CompleteOperation(
    const FWorldOperationResult& Result)
{
    if (!Result.OperationId.IsValid())
    {
        return false;
    }

    FWorldOperationLedgerEntry* Entry = Entries.Find(Result.OperationId);
    if (!Entry || Entry->State != EWorldOperationLedgerEntryState::Pending)
    {
        return false;
    }

    Entry->Result = Result;
    Entry->State = EWorldOperationLedgerEntryState::Completed;
    return true;
}

bool UWorldOperationLedgerSubsystem::TryGetCompletedResult(
    const FGuid& OperationId,
    FWorldOperationResult& OutResult) const
{
    const FWorldOperationLedgerEntry* Entry = Entries.Find(OperationId);
    if (!Entry || Entry->State != EWorldOperationLedgerEntryState::Completed)
    {
        return false;
    }

    OutResult = Entry->Result;
    return true;
}

bool UWorldOperationLedgerSubsystem::CancelUncommittedOperation(
    const FGuid& OperationId)
{
    FWorldOperationLedgerEntry* Entry = Entries.Find(OperationId);
    if (!Entry || Entry->State != EWorldOperationLedgerEntryState::Pending)
    {
        return false;
    }

    Entries.Remove(OperationId);
    return true;
}
