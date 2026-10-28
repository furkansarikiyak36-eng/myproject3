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
    if (SplitReceipts.Contains(OperationId))
    {
        return false;
    }

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

    if (SplitReceipts.Contains(OperationId))
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
    TArray<FWorldItemSplitReceipt> IgnoredSplitReceipts;
    ExportSnapshot(OutItems, IgnoredReceipts, IgnoredSplitReceipts);
}

void UWorldItemOwnershipSubsystem::ExportSnapshot(
    TArray<FWorldOwnedItemRecord>& OutItems,
    TArray<FWorldItemTransferReceipt>& OutReceipts) const
{
    TArray<FWorldItemSplitReceipt> IgnoredSplitReceipts;
    ExportSnapshot(OutItems, OutReceipts, IgnoredSplitReceipts);
}

void UWorldItemOwnershipSubsystem::ExportSnapshot(
    TArray<FWorldOwnedItemRecord>& OutItems,
    TArray<FWorldItemTransferReceipt>& OutReceipts,
    TArray<FWorldItemSplitReceipt>& OutSplitReceipts) const
{
    OutItems.Reset();
    Items.GenerateValueArray(OutItems);
    OutItems.Sort([](const FWorldOwnedItemRecord& A, const FWorldOwnedItemRecord& B)
        { return A.ItemId.ToString() < B.ItemId.ToString(); });

    OutReceipts.Reset();
    Receipts.GenerateValueArray(OutReceipts);
    OutReceipts.Sort([](const FWorldItemTransferReceipt& A, const FWorldItemTransferReceipt& B)
        { return A.OperationId.ToString() < B.OperationId.ToString(); });

    OutSplitReceipts.Reset();
    SplitReceipts.GenerateValueArray(OutSplitReceipts);
    OutSplitReceipts.Sort([](const FWorldItemSplitReceipt& A, const FWorldItemSplitReceipt& B)
        { return A.OperationId.ToString() < B.OperationId.ToString(); });
}

bool UWorldItemOwnershipSubsystem::TrySplitItem(
    const FGuid& OperationId,
    const FGuid& SourceItemId,
    int64 ExpectedRevision,
    EWorldItemOwnerKind ExpectedOwner,
    const TArray<FName>& OutputDefinitionIds,
    const TArray<FVector>& OutputWorldLocations,
    TArray<FWorldOwnedItemRecord>& OutRecords)
{
    OutRecords.Reset();
    if (!OperationId.IsValid() || !SourceItemId.IsValid() || ExpectedRevision <= 0 ||
        ExpectedOwner != EWorldItemOwnerKind::World ||
        OutputDefinitionIds.Num() < 2 ||
        OutputDefinitionIds.Num() != OutputWorldLocations.Num())
    {
        return false;
    }

    for (int32 Index = 0; Index < OutputDefinitionIds.Num(); ++Index)
    {
        const FVector& Location = OutputWorldLocations[Index];
        if (OutputDefinitionIds[Index].IsNone() ||
            !FMath::IsFinite(Location.X) ||
            !FMath::IsFinite(Location.Y) ||
            !FMath::IsFinite(Location.Z))
        {
            return false;
        }

        for (int32 Other = 0; Other < Index; ++Other)
        {
            if (OutputWorldLocations[Other].Equals(Location, 0.1f))
                return false;
        }
    }

    if (const FWorldItemSplitReceipt* Existing = SplitReceipts.Find(OperationId))
    {
        if (Existing->SourceItemId != SourceItemId ||
            Existing->ExpectedRevision != ExpectedRevision ||
            Existing->ExpectedOwner != ExpectedOwner ||
            Existing->OutputDefinitionIds != OutputDefinitionIds ||
            Existing->OutputWorldLocations.Num() != OutputWorldLocations.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < OutputWorldLocations.Num(); ++Index)
        {
            if (!Existing->OutputWorldLocations[Index].Equals(OutputWorldLocations[Index], 0.001f))
                return false;
        }
        OutRecords = Existing->ResultRecords;
        return OutRecords.Num() == OutputDefinitionIds.Num();
    }

    // Operation IDs are globally unique across ownership mutation receipt kinds.
    if (Receipts.Contains(OperationId))
        return false;

    // Capacity applies only to a new split. An exact retry above must return its
    // immutable prior result even when the item ledger is now at capacity.
    if (MaxTrackedItems <= 0 || Items.Num() > MaxTrackedItems ||
        OutputDefinitionIds.Num() - 1 > MaxTrackedItems - Items.Num())
        return false;

    const FWorldOwnedItemRecord* Source = Items.Find(SourceItemId);
    if (!Source || Source->Revision != ExpectedRevision ||
        Source->OwnerKind != ExpectedOwner || Source->Quantity != 1 ||
        Source->Revision == MAX_int64 ||
        Source->DefinitionId == OutputDefinitionIds[0])
    {
        return false;
    }

    TArray<FWorldOwnedItemRecord> ProposedOutputs;
    ProposedOutputs.Reserve(OutputDefinitionIds.Num());
    const FName BaseDefinition = Source->BaseDefinitionId.IsNone()
        ? Source->DefinitionId : Source->BaseDefinitionId;

    FWorldOwnedItemRecord FirstOutput = *Source;
    FirstOutput.DefinitionId = OutputDefinitionIds[0];
    FirstOutput.BaseDefinitionId = BaseDefinition;
    FirstOutput.OwnerKind = EWorldItemOwnerKind::World;
    FirstOutput.OwnerId.Invalidate();
    FirstOutput.WorldLocation = OutputWorldLocations[0];
    FirstOutput.Revision = ExpectedRevision + 1;
    ProposedOutputs.Add(FirstOutput);

    TSet<FGuid> ProposedIds;
    ProposedIds.Add(SourceItemId);
    for (int32 Index = 1; Index < OutputDefinitionIds.Num(); ++Index)
    {
        FGuid NewId;
        do
        {
            NewId = FGuid::NewGuid();
        }
        while (!NewId.IsValid() || Items.Contains(NewId) || ProposedIds.Contains(NewId));
        ProposedIds.Add(NewId);

        FWorldOwnedItemRecord Output;
        Output.ItemId = NewId;
        Output.DefinitionId = OutputDefinitionIds[Index];
        Output.BaseDefinitionId = BaseDefinition;
        Output.MeshVariantSetId = Source->MeshVariantSetId;
        Output.Quantity = 1;
        Output.OwnerKind = EWorldItemOwnerKind::World;
        Output.OwnerId.Invalidate();
        Output.WorldLocation = OutputWorldLocations[Index];
        Output.Revision = 1;
        ProposedOutputs.Add(Output);
    }

    FWorldItemSplitReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.SourceItemId = SourceItemId;
    Receipt.ExpectedRevision = ExpectedRevision;
    Receipt.ExpectedOwner = ExpectedOwner;
    Receipt.OutputDefinitionIds = OutputDefinitionIds;
    Receipt.OutputWorldLocations = OutputWorldLocations;
    Receipt.ResultRecords = ProposedOutputs;

    // No authoritative mutation occurs before every request field and output is valid.
    Items.FindChecked(SourceItemId) = ProposedOutputs[0];
    for (int32 Index = 1; Index < ProposedOutputs.Num(); ++Index)
    {
        Items.Add(ProposedOutputs[Index].ItemId, ProposedOutputs[Index]);
    }
    SplitReceipts.Add(OperationId, Receipt);
    OutRecords = MoveTemp(ProposedOutputs);
    return true;
}

bool UWorldItemOwnershipSubsystem::TryGetSplitResult(
    const FGuid& OperationId,
    TArray<FWorldOwnedItemRecord>& OutRecords) const
{
    OutRecords.Reset();
    const FWorldItemSplitReceipt* Receipt = SplitReceipts.Find(OperationId);
    if (!Receipt)
        return false;
    OutRecords = Receipt->ResultRecords;
    return !OutRecords.IsEmpty();
}

bool UWorldItemOwnershipSubsystem::RestoreSnapshot(const TArray<FWorldOwnedItemRecord>& Snapshot)
{
    TArray<FWorldItemTransferReceipt> EmptyReceipts;
    TArray<FWorldItemSplitReceipt> EmptySplitReceipts;
    return RestoreSnapshot(Snapshot, EmptyReceipts, EmptySplitReceipts);
}

bool UWorldItemOwnershipSubsystem::RestoreSnapshot(
    const TArray<FWorldOwnedItemRecord>& Snapshot,
    const TArray<FWorldItemTransferReceipt>& ReceiptSnapshot)
{
    TArray<FWorldItemSplitReceipt> EmptySplitReceipts;
    return RestoreSnapshot(Snapshot, ReceiptSnapshot, EmptySplitReceipts);
}

bool UWorldItemOwnershipSubsystem::RestoreSnapshot(
    const TArray<FWorldOwnedItemRecord>& Snapshot,
    const TArray<FWorldItemTransferReceipt>& ReceiptSnapshot,
    const TArray<FWorldItemSplitReceipt>& SplitReceiptSnapshot)
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

    TMap<FGuid, FWorldItemSplitReceipt> ProposedSplitReceipts;
    for (const FWorldItemSplitReceipt& Receipt : SplitReceiptSnapshot)
    {
        const int32 OutputCount = Receipt.ResultRecords.Num();
        if (!Receipt.OperationId.IsValid() || !Receipt.SourceItemId.IsValid() ||
            Receipt.ExpectedOwner != EWorldItemOwnerKind::World ||
            Receipt.ExpectedRevision <= 0 || Receipt.ExpectedRevision >= MAX_int64 ||
            OutputCount < 2 || Receipt.OutputDefinitionIds.Num() != OutputCount ||
            Receipt.OutputWorldLocations.Num() != OutputCount ||
            !ProposedItems.Contains(Receipt.SourceItemId) ||
            ProposedSplitReceipts.Contains(Receipt.OperationId) ||
            ProposedReceipts.Contains(Receipt.OperationId))
        {
            return false;
        }

        TSet<FGuid> ResultIds;
        for (int32 Index = 0; Index < OutputCount; ++Index)
        {
            const FWorldOwnedItemRecord& Output = Receipt.ResultRecords[Index];
            const FVector& Location = Receipt.OutputWorldLocations[Index];
            if (!IsValidItem(Output) || Output.ItemId.IsValid() == false ||
                !ProposedItems.Contains(Output.ItemId) || ResultIds.Contains(Output.ItemId) ||
                Output.DefinitionId != Receipt.OutputDefinitionIds[Index] ||
                !Output.WorldLocation.Equals(Location, 0.001f) ||
                Output.OwnerKind != EWorldItemOwnerKind::World || Output.OwnerId.IsValid() ||
                Output.Quantity != 1 ||
                !FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) ||
                !FMath::IsFinite(Location.Z) ||
                (Index == 0 &&
                    (Output.ItemId != Receipt.SourceItemId ||
                     Output.Revision != Receipt.ExpectedRevision + 1)) ||
                (Index > 0 && Output.Revision != 1))
            {
                return false;
            }

            ResultIds.Add(Output.ItemId);
        }

        ProposedSplitReceipts.Add(Receipt.OperationId, Receipt);
    }

    Items = MoveTemp(ProposedItems);
    Receipts = MoveTemp(ProposedReceipts);
    SplitReceipts = MoveTemp(ProposedSplitReceipts);
    return true;
}
