#include "WorldOperationLedgerSubsystem.h"

bool UWorldOperationLedgerSubsystem::CanBeginOperation(const FGuid& OperationId) const
{
    return OperationId.IsValid() && !Entries.Contains(OperationId) &&
        MaxEntries > 0 && Entries.Num() < MaxEntries;
}

bool UWorldOperationLedgerSubsystem::TryBeginOperation(const FGuid& OperationId)
{
    if (!OperationId.IsValid() || Entries.Contains(OperationId))
        return false;
    if (MaxEntries <= 0 || Entries.Num() >= MaxEntries)
    {
        UE_LOG(LogTemp, Error,
            TEXT("World operation ledger is full (%d/%d); refusing operation %s rather than evicting an unreceipted idempotency result."),
            Entries.Num(), MaxEntries, *OperationId.ToString());
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
    if (!Entry || Entry->State != EWorldOperationLedgerEntryState::Pending ||
        static_cast<uint8>(Result.Code) > static_cast<uint8>(EWorldOperationResultCode::InternalError) ||
        Result.bSucceeded != (Result.Code == EWorldOperationResultCode::Success))
    {
        return false;
    }

    for (const FGuid& AffectedId : Result.AffectedObjectIds)
    {
        if (!AffectedId.IsValid())
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


bool UWorldOperationLedgerSubsystem::ForgetCompletedOperationWithExternalReceipt(
    const FGuid& OperationId)
{
    if (!OperationId.IsValid())
        return false;

    const FWorldOperationLedgerEntry* Entry = Entries.Find(OperationId);
    if (!Entry || Entry->State != EWorldOperationLedgerEntryState::Completed ||
        Entry->Result.OperationId != OperationId ||
        Entry->Result.bSucceeded != (Entry->Result.Code == EWorldOperationResultCode::Success))
    {
        return false;
    }

    // The caller is responsible for proving that a separately persisted,
    // domain-owned idempotency watermark/receipt already represents this result.
    return Entries.Remove(OperationId) == 1;
}

int32 UWorldOperationLedgerSubsystem::GetCompletedEntryCount() const
{
    int32 Count = 0;
    for (const TPair<FGuid, FWorldOperationLedgerEntry>& Pair : Entries)
        if (Pair.Value.State == EWorldOperationLedgerEntryState::Completed)
            ++Count;
    return Count;
}

bool UWorldOperationLedgerSubsystem::HasPendingOperations() const
{
    for (const TPair<FGuid, FWorldOperationLedgerEntry>& Pair : Entries)
    {
        if (Pair.Value.State == EWorldOperationLedgerEntryState::Pending)
            return true;
    }
    return false;
}

void UWorldOperationLedgerSubsystem::ExportSnapshot(
    TArray<FWorldOperationLedgerEntry>& OutEntries) const
{
    OutEntries.Reset();
    Entries.GenerateValueArray(OutEntries);
    OutEntries.Sort([](const FWorldOperationLedgerEntry& A, const FWorldOperationLedgerEntry& B)
        { return A.OperationId.ToString() < B.OperationId.ToString(); });
}

bool UWorldOperationLedgerSubsystem::RestoreSnapshot(
    const TArray<FWorldOperationLedgerEntry>& Snapshot)
{
    if (MaxEntries <= 0 || Snapshot.Num() > MaxEntries)
        return false;

    TMap<FGuid, FWorldOperationLedgerEntry> Proposed;
    for (const FWorldOperationLedgerEntry& Entry : Snapshot)
    {
        if (!Entry.OperationId.IsValid() || Proposed.Contains(Entry.OperationId) ||
            static_cast<uint8>(Entry.State) > static_cast<uint8>(EWorldOperationLedgerEntryState::Completed))
        {
            return false;
        }

        if (Entry.State == EWorldOperationLedgerEntryState::Completed)
        {
            if (Entry.Result.OperationId != Entry.OperationId ||
                static_cast<uint8>(Entry.Result.Code) > static_cast<uint8>(EWorldOperationResultCode::InternalError) ||
                Entry.Result.bSucceeded != (Entry.Result.Code == EWorldOperationResultCode::Success))
            {
                return false;
            }

            for (const FGuid& AffectedId : Entry.Result.AffectedObjectIds)
            {
                if (!AffectedId.IsValid())
                    return false;
            }
        }

        Proposed.Add(Entry.OperationId, Entry);
    }

    Entries = MoveTemp(Proposed);
    return true;
}
