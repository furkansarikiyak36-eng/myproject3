#include "WorldItemOwnershipSubsystem.h"

bool UWorldItemOwnershipSubsystem::RegisterItem(const FWorldOwnedItemRecord& Record)
{
    if (!Record.ItemId.IsValid() || Record.DefinitionId.IsNone() ||
        Record.Quantity <= 0 || Record.Revision <= 0 ||
        static_cast<uint8>(Record.OwnerKind) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
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

    FWorldOwnedItemRecord Normalized = Record;
    if (Normalized.BaseDefinitionId.IsNone())
        Normalized.BaseDefinitionId = Normalized.DefinitionId;
    Items.Add(Normalized.ItemId, Normalized);
    return true;
}

bool UWorldItemOwnershipSubsystem::TryTransferItem(
    const FGuid& OperationId, const FGuid& ItemId, int64 ExpectedRevision,
    EWorldItemOwnerKind ExpectedOwner, EWorldItemOwnerKind NewOwner,
    const FGuid& NewOwnerId, FVector NewWorldLocation, FWorldOwnedItemRecord& OutRecord)
{
    if (!OperationId.IsValid() || !ItemId.IsValid() || ExpectedRevision <= 0 ||
        static_cast<uint8>(ExpectedOwner) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
        static_cast<uint8>(NewOwner) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
        !FMath::IsFinite(NewWorldLocation.X) || !FMath::IsFinite(NewWorldLocation.Y) ||
        !FMath::IsFinite(NewWorldLocation.Z))
    {
        return false;
    }

    // Retries return the already-committed outcome; they never apply a second transfer.
    if (const FWorldItemTransferReceipt* Receipt = Receipts.Find(OperationId))
    {
        // An operation ID is idempotent only for the same complete request.
        if (Receipt->bDefinitionChange || !Receipt->RequestedDefinitionId.IsNone() ||
            Receipt->ItemId != ItemId || Receipt->ExpectedRevision != ExpectedRevision ||
            Receipt->ExpectedOwner != ExpectedOwner || Receipt->RequestedOwner != NewOwner ||
            Receipt->RequestedOwnerId != NewOwnerId ||
            !Receipt->RequestedWorldLocation.Equals(NewWorldLocation, 0.001f))
        {
            return false;
        }
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
    Receipt.ExpectedRevision = ExpectedRevision;
    Receipt.ExpectedOwner = ExpectedOwner;
    Receipt.RequestedOwner = NewOwner;
    Receipt.RequestedOwnerId = bRequiresOwnerId ? NewOwnerId : FGuid();
    Receipt.RequestedWorldLocation = NewWorldLocation;
    Receipts.Add(OperationId, Receipt);
    OutRecord = *Item;
    return true;
}

bool UWorldItemOwnershipSubsystem::TryUpdateItemDefinition(
    const FGuid& OperationId, const FGuid& ItemId, int64 ExpectedRevision,
    EWorldItemOwnerKind ExpectedOwner, FName NewDefinitionId,
    FWorldOwnedItemRecord& OutRecord)
{
    if (!OperationId.IsValid() || !ItemId.IsValid() || ExpectedRevision <= 0 ||
        ExpectedOwner != EWorldItemOwnerKind::World || NewDefinitionId.IsNone())
    {
        return false;
    }

    if (const FWorldItemTransferReceipt* Receipt = Receipts.Find(OperationId))
    {
        if (!Receipt->bDefinitionChange || Receipt->ItemId != ItemId ||
            Receipt->ExpectedRevision != ExpectedRevision ||
            Receipt->ExpectedOwner != ExpectedOwner ||
            Receipt->RequestedDefinitionId != NewDefinitionId)
        {
            return false;
        }
        OutRecord = Receipt->ResultRecord;
        return true;
    }

    FWorldOwnedItemRecord* Item = Items.Find(ItemId);
    if (!Item || Item->Revision != ExpectedRevision || Item->OwnerKind != ExpectedOwner ||
        Item->Revision == MAX_int64 || Item->DefinitionId == NewDefinitionId)
    {
        return false;
    }

    if (Item->BaseDefinitionId.IsNone())
        Item->BaseDefinitionId = Item->DefinitionId;

    Item->DefinitionId = NewDefinitionId;
    ++Item->Revision;

    FWorldItemTransferReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.ItemId = ItemId;
    Receipt.ResultingRevision = Item->Revision;
    Receipt.ResultingOwner = Item->OwnerKind;
    Receipt.ResultRecord = *Item;
    Receipt.ExpectedRevision = ExpectedRevision;
    Receipt.ExpectedOwner = ExpectedOwner;
    Receipt.RequestedOwner = Item->OwnerKind;
    Receipt.RequestedOwnerId = Item->OwnerId;
    Receipt.RequestedWorldLocation = Item->WorldLocation;
    Receipt.bDefinitionChange = true;
    Receipt.RequestedDefinitionId = NewDefinitionId;
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
    TArray<FWorldItemTransferReceipt> IgnoredReceipts;
    ExportSnapshot(OutItems, IgnoredReceipts);
}

void UWorldItemOwnershipSubsystem::ExportSnapshot(
    TArray<FWorldOwnedItemRecord>& OutItems,
    TArray<FWorldItemTransferReceipt>& OutReceipts) const
{
    OutItems.Reset();
    Items.GenerateValueArray(OutItems);
    OutItems.Sort([](const FWorldOwnedItemRecord& A, const FWorldOwnedItemRecord& B)
        { return A.ItemId.ToString() < B.ItemId.ToString(); });

    OutReceipts.Reset();
    Receipts.GenerateValueArray(OutReceipts);
    OutReceipts.Sort([](const FWorldItemTransferReceipt& A, const FWorldItemTransferReceipt& B)
        { return A.OperationId.ToString() < B.OperationId.ToString(); });
}

bool UWorldItemOwnershipSubsystem::RestoreSnapshot(const TArray<FWorldOwnedItemRecord>& Snapshot)
{
    TArray<FWorldItemTransferReceipt> EmptyReceipts;
    return RestoreSnapshot(Snapshot, EmptyReceipts);
}

bool UWorldItemOwnershipSubsystem::RestoreSnapshot(
    const TArray<FWorldOwnedItemRecord>& Snapshot,
    const TArray<FWorldItemTransferReceipt>& ReceiptSnapshot)
{
    if (Snapshot.Num() > MaxTrackedItems || MaxTrackedItems <= 0)
        return false;

    const auto IsValidItem = [](const FWorldOwnedItemRecord& Item) -> bool
    {
        if (!Item.ItemId.IsValid() || Item.DefinitionId.IsNone() || Item.Quantity <= 0 ||
            Item.Revision <= 0 ||
            static_cast<uint8>(Item.OwnerKind) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
            !FMath::IsFinite(Item.WorldLocation.X) ||
            !FMath::IsFinite(Item.WorldLocation.Y) ||
            !FMath::IsFinite(Item.WorldLocation.Z))
        {
            return false;
        }

        const bool bNeedsOwner = Item.OwnerKind == EWorldItemOwnerKind::Held ||
            Item.OwnerKind == EWorldItemOwnerKind::Inventory ||
            Item.OwnerKind == EWorldItemOwnerKind::Storage ||
            Item.OwnerKind == EWorldItemOwnerKind::InTransit;
        return !bNeedsOwner || Item.OwnerId.IsValid();
    };

    TMap<FGuid, FWorldOwnedItemRecord> ProposedItems;
    for (const FWorldOwnedItemRecord& Item : Snapshot)
    {
        if (!IsValidItem(Item) || ProposedItems.Contains(Item.ItemId))
            return false;

        FWorldOwnedItemRecord Normalized = Item;
        if (Normalized.BaseDefinitionId.IsNone())
            Normalized.BaseDefinitionId = Normalized.DefinitionId;
        ProposedItems.Add(Normalized.ItemId, Normalized);
    }

    TMap<FGuid, FWorldItemTransferReceipt> ProposedReceipts;
    for (const FWorldItemTransferReceipt& Receipt : ReceiptSnapshot)
    {
        if (!Receipt.OperationId.IsValid() || !Receipt.ItemId.IsValid() ||
            !ProposedItems.Contains(Receipt.ItemId) || ProposedReceipts.Contains(Receipt.OperationId) ||
            Receipt.ExpectedRevision <= 0 || Receipt.ExpectedRevision >= MAX_int64 ||
            Receipt.ResultingRevision != Receipt.ExpectedRevision + 1 ||
            static_cast<uint8>(Receipt.ExpectedOwner) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
            static_cast<uint8>(Receipt.RequestedOwner) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
            static_cast<uint8>(Receipt.ResultingOwner) > static_cast<uint8>(EWorldItemOwnerKind::Transformed) ||
            !IsValidItem(Receipt.ResultRecord) || Receipt.ResultRecord.ItemId != Receipt.ItemId ||
            Receipt.ResultRecord.Revision != Receipt.ResultingRevision ||
            Receipt.ResultRecord.OwnerKind != Receipt.ResultingOwner ||
            !FMath::IsFinite(Receipt.RequestedWorldLocation.X) ||
            !FMath::IsFinite(Receipt.RequestedWorldLocation.Y) ||
            !FMath::IsFinite(Receipt.RequestedWorldLocation.Z))
        {
            return false;
        }

        const bool bRequestedOwnerNeedsId = Receipt.RequestedOwner == EWorldItemOwnerKind::Held ||
            Receipt.RequestedOwner == EWorldItemOwnerKind::Inventory ||
            Receipt.RequestedOwner == EWorldItemOwnerKind::Storage ||
            Receipt.RequestedOwner == EWorldItemOwnerKind::InTransit;
        if (bRequestedOwnerNeedsId && !Receipt.RequestedOwnerId.IsValid())
            return false;

        if (Receipt.bDefinitionChange)
        {
            if (Receipt.ExpectedOwner != EWorldItemOwnerKind::World ||
                Receipt.RequestedDefinitionId.IsNone() ||
                Receipt.ResultRecord.DefinitionId != Receipt.RequestedDefinitionId)
            {
                return false;
            }
        }
        else if (!Receipt.RequestedDefinitionId.IsNone() ||
            Receipt.ResultRecord.OwnerKind != Receipt.RequestedOwner)
        {
            return false;
        }

        FWorldItemTransferReceipt NormalizedReceipt = Receipt;
        if (NormalizedReceipt.ResultRecord.BaseDefinitionId.IsNone())
            NormalizedReceipt.ResultRecord.BaseDefinitionId = NormalizedReceipt.ResultRecord.DefinitionId;
        ProposedReceipts.Add(NormalizedReceipt.OperationId, NormalizedReceipt);
    }

    // Commit both maps only after every item and receipt has passed validation.
    Items = MoveTemp(ProposedItems);
    Receipts = MoveTemp(ProposedReceipts);
    return true;
}
