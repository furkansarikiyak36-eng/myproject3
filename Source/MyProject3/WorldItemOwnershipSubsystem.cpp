#include "WorldItemOwnershipSubsystem.h"

bool UWorldItemOwnershipSubsystem::RegisterItem(const FWorldOwnedItemRecord& Record)
{
    if (!Record.ItemId.IsValid() || Record.DefinitionId.IsNone() ||
        Record.Quantity <= 0 || Record.Revision <= 0 ||
        Items.Contains(Record.ItemId) || MaxTrackedItems <= 0 ||
        Items.Num() >= MaxTrackedItems ||
        !FMath::IsFinite(Record.WorldLocation.X) ||
        !FMath::IsFinite(Record.WorldLocation.Y) ||
        !FMath::IsFinite(Record.WorldLocation.Z))
    {
        return false;
    }

    if ((Record.OwnerKind == EWorldItemOwnerKind::Held ||
         Record.OwnerKind == EWorldItemOwnerKind::Inventory ||
         Record.OwnerKind == EWorldItemOwnerKind::Storage ||
         Record.OwnerKind == EWorldItemOwnerKind::InTransit) &&
        !Record.OwnerId.IsValid())
    {
        return false;
    }

    Items.Add(Record.ItemId, Record);
    return true;
}

bool UWorldItemOwnershipSubsystem::TryTransferItem(
    const FGuid& OperationId, const FGuid& ItemId, int64 ExpectedRevision,
    EWorldItemOwnerKind ExpectedOwner, EWorldItemOwnerKind NewOwner,
    const FGuid& NewOwnerId, FVector NewWorldLocation, FWorldOwnedItemRecord& OutRecord)
{
    if (!OperationId.IsValid() || !ItemId.IsValid() || ExpectedRevision <= 0 ||
        !FMath::IsFinite(NewWorldLocation.X) || !FMath::IsFinite(NewWorldLocation.Y) ||
        !FMath::IsFinite(NewWorldLocation.Z))
    {
        return false;
    }

    // Retries return the already-committed outcome; they never apply a second transfer.
    if (const FWorldItemTransferReceipt* Receipt = Receipts.Find(OperationId))
    {
        if (Receipt->ItemId != ItemId) return false;
        OutRecord = Receipt->ResultRecord;
        return true;
    }

    FWorldOwnedItemRecord* Item = Items.Find(ItemId);
    if (!Item || Item->Revision != ExpectedRevision || Item->OwnerKind != ExpectedOwner ||
        Item->Revision == MAX_int64)
    {
        return false;
    }

    const bool bRequiresOwnerId = NewOwner == EWorldItemOwnerKind::Held ||
        NewOwner == EWorldItemOwnerKind::Inventory || NewOwner == EWorldItemOwnerKind::Storage ||
        NewOwner == EWorldItemOwnerKind::InTransit;
    if (bRequiresOwnerId && !NewOwnerId.IsValid())
    {
        return false;
    }

    // All validation is complete before the record is changed.
    Item->OwnerKind = NewOwner;
    Item->OwnerId = bRequiresOwnerId ? NewOwnerId : FGuid();
    Item->WorldLocation = NewWorldLocation;
    ++Item->Revision;

    FWorldItemTransferReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.ItemId = ItemId;
    Receipt.ResultingRevision = Item->Revision;
    Receipt.ResultingOwner = Item->OwnerKind;
    Receipt.ResultRecord = *Item;
    Receipts.Add(OperationId, Receipt);
    OutRecord = *Item;
    return true;
}

bool UWorldItemOwnershipSubsystem::TryConsumeItem(
    const FGuid& OperationId, const FGuid& ItemId, int64 ExpectedRevision,
    EWorldItemOwnerKind ExpectedOwner)
{
    FWorldOwnedItemRecord Updated;
    return TryTransferItem(OperationId, ItemId, ExpectedRevision, ExpectedOwner,
        EWorldItemOwnerKind::Consumed, FGuid(), FVector::ZeroVector, Updated);
}

bool UWorldItemOwnershipSubsystem::TryGetItem(
    const FGuid& ItemId, FWorldOwnedItemRecord& OutRecord) const
{
    const FWorldOwnedItemRecord* Found = Items.Find(ItemId);
    if (!Found) return false;
    OutRecord = *Found;
    return true;
}

void UWorldItemOwnershipSubsystem::ExportSnapshot(TArray<FWorldOwnedItemRecord>& OutItems) const
{
    OutItems.Reset();
    Items.GenerateValueArray(OutItems);
    OutItems.Sort([](const FWorldOwnedItemRecord& A, const FWorldOwnedItemRecord& B)
        { return A.ItemId.ToString() < B.ItemId.ToString(); });
}

bool UWorldItemOwnershipSubsystem::RestoreSnapshot(const TArray<FWorldOwnedItemRecord>& Snapshot)
{
    if (Snapshot.Num() > MaxTrackedItems || MaxTrackedItems <= 0) return false;
    TMap<FGuid, FWorldOwnedItemRecord> Proposed;
    for (const FWorldOwnedItemRecord& Item : Snapshot)
    {
        if (!Item.ItemId.IsValid() || Item.DefinitionId.IsNone() || Item.Quantity <= 0 ||
            Item.Revision <= 0 || Proposed.Contains(Item.ItemId) ||
            !FMath::IsFinite(Item.WorldLocation.X) || !FMath::IsFinite(Item.WorldLocation.Y) ||
            !FMath::IsFinite(Item.WorldLocation.Z))
            return false;
        const bool bNeedsOwner = Item.OwnerKind == EWorldItemOwnerKind::Held ||
            Item.OwnerKind == EWorldItemOwnerKind::Inventory || Item.OwnerKind == EWorldItemOwnerKind::Storage ||
            Item.OwnerKind == EWorldItemOwnerKind::InTransit;
        if (bNeedsOwner && !Item.OwnerId.IsValid()) return false;
        Proposed.Add(Item.ItemId, Item);
    }
    Items = MoveTemp(Proposed);
    Receipts.Reset();
    return true;
}
