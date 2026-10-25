#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldItemOwnershipSubsystem.generated.h"

UENUM(BlueprintType)
enum class EWorldItemOwnerKind : uint8
{
    World,
    Held,
    Inventory,
    Storage,
    InTransit,
    Consumed,
    Transformed
};

USTRUCT(BlueprintType)
struct FWorldOwnedItemRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items")
    FGuid ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items")
    FName DefinitionId = NAME_None;

    /** Stable root definition retained as the item changes processing state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items")
    FName BaseDefinitionId = NAME_None;

    /** Authored nominal mesh-family selector; this is not inferred from filename digits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items")
    FName MeshVariantSetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items", meta=(ClampMin="1"))
    int32 Quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items")
    EWorldItemOwnerKind OwnerKind = EWorldItemOwnerKind::World;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items")
    FGuid OwnerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Items")
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Items")
    int64 Revision = 1;
};

USTRUCT()
struct FWorldItemTransferReceipt
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid OperationId;

    UPROPERTY(SaveGame)
    FGuid ItemId;

    UPROPERTY(SaveGame)
    int64 ResultingRevision = 0;

    UPROPERTY(SaveGame)
    EWorldItemOwnerKind ResultingOwner = EWorldItemOwnerKind::World;

    // Immutable result snapshot returned by idempotent retries.
    UPROPERTY(SaveGame)
    FWorldOwnedItemRecord ResultRecord;

    UPROPERTY(SaveGame)
    int64 ExpectedRevision = 0;

    UPROPERTY(SaveGame)
    EWorldItemOwnerKind ExpectedOwner = EWorldItemOwnerKind::World;

    UPROPERTY(SaveGame)
    EWorldItemOwnerKind RequestedOwner = EWorldItemOwnerKind::World;

    UPROPERTY(SaveGame)
    FGuid RequestedOwnerId;

    UPROPERTY(SaveGame)
    FVector RequestedWorldLocation = FVector::ZeroVector;

    UPROPERTY(SaveGame)
    bool bDefinitionChange = false;

    UPROPERTY(SaveGame)
    FName RequestedDefinitionId = NAME_None;
};

USTRUCT()
struct FWorldItemSplitReceipt
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid OperationId;

    UPROPERTY(SaveGame)
    FGuid SourceItemId;

    UPROPERTY(SaveGame)
    int64 ExpectedRevision = 0;

    UPROPERTY(SaveGame)
    EWorldItemOwnerKind ExpectedOwner = EWorldItemOwnerKind::World;

    UPROPERTY(SaveGame)
    TArray<FName> OutputDefinitionIds;

    UPROPERTY(SaveGame)
    TArray<FVector> OutputWorldLocations;

    /** First record retains SourceItemId; all following records are new unique items. */
    UPROPERTY(SaveGame)
    TArray<FWorldOwnedItemRecord> ResultRecords;
};

/** Single authoritative ownership ledger for world objects and resource stacks. */
UCLASS()
class MYPROJECT3_API UWorldItemOwnershipSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World|Items", meta=(ClampMin="1", ClampMax="100000"))
    int32 MaxTrackedItems = 16384;

    UFUNCTION(BlueprintCallable, Category="World|Items")
    bool RegisterItem(const FWorldOwnedItemRecord& Record);

    UFUNCTION(BlueprintCallable, Category="World|Items")
    bool TryTransferItem(const FGuid& OperationId, const FGuid& ItemId,
        int64 ExpectedRevision, EWorldItemOwnerKind ExpectedOwner,
        EWorldItemOwnerKind NewOwner, const FGuid& NewOwnerId,
        FVector NewWorldLocation, FWorldOwnedItemRecord& OutRecord);

    UFUNCTION(BlueprintCallable, Category="World|Items")
    bool TryConsumeItem(const FGuid& OperationId, const FGuid& ItemId,
        int64 ExpectedRevision, EWorldItemOwnerKind ExpectedOwner);

    UFUNCTION(BlueprintCallable, Category="World|Items")
    bool TryUpdateItemDefinition(const FGuid& OperationId, const FGuid& ItemId,
        int64 ExpectedRevision, EWorldItemOwnerKind ExpectedOwner,
        FName NewDefinitionId, FWorldOwnedItemRecord& OutRecord);

    UFUNCTION(BlueprintPure, Category="World|Items")
    bool TryGetItem(const FGuid& ItemId, FWorldOwnedItemRecord& OutRecord) const;

    UFUNCTION(BlueprintPure, Category="World|Items")
    int32 GetTrackedItemCount() const { return Items.Num(); }

    /** Exports authoritative item records only (legacy callers intentionally omit retry receipts). */
    void ExportSnapshot(TArray<FWorldOwnedItemRecord>& OutItems) const;

    /** Exports item records and idempotency receipts for rollback/save checkpoints. */
    void ExportSnapshot(TArray<FWorldOwnedItemRecord>& OutItems,
        TArray<FWorldItemTransferReceipt>& OutReceipts) const;

    /** Restores item records and clears in-memory retry receipts. */
    bool RestoreSnapshot(const TArray<FWorldOwnedItemRecord>& Snapshot);

    /** Exports item records, transfer receipts and physical split receipts. */
    void ExportSnapshot(TArray<FWorldOwnedItemRecord>& OutItems,
        TArray<FWorldItemTransferReceipt>& OutReceipts,
        TArray<FWorldItemSplitReceipt>& OutSplitReceipts) const;

    /** Atomically restores item records and their associated transfer idempotency receipts. */
    bool RestoreSnapshot(const TArray<FWorldOwnedItemRecord>& Snapshot,
        const TArray<FWorldItemTransferReceipt>& ReceiptSnapshot);

    /** Atomically restores item records, transfer receipts and split receipts. */
    bool RestoreSnapshot(const TArray<FWorldOwnedItemRecord>& Snapshot,
        const TArray<FWorldItemTransferReceipt>& ReceiptSnapshot,
        const TArray<FWorldItemSplitReceipt>& SplitReceiptSnapshot);

    /**
     * Atomically changes a world-owned physical item into multiple individually
     * owned world items. The first output keeps SourceItemId; subsequent IDs
     * are generated by this ledger. Reusing OperationId with the same request
     * returns the original output records without splitting again.
     */
    bool TrySplitItem(const FGuid& OperationId, const FGuid& SourceItemId,
        int64 ExpectedRevision, EWorldItemOwnerKind ExpectedOwner,
        const TArray<FName>& OutputDefinitionIds,
        const TArray<FVector>& OutputWorldLocations,
        TArray<FWorldOwnedItemRecord>& OutRecords);

    /** Returns a previously committed split result, if the operation ID matches. */
    bool TryGetSplitResult(const FGuid& OperationId,
        TArray<FWorldOwnedItemRecord>& OutRecords) const;

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Items")
    TMap<FGuid, FWorldOwnedItemRecord> Items;

    UPROPERTY(Transient)
    TMap<FGuid, FWorldItemTransferReceipt> Receipts;

    UPROPERTY(Transient)
    TMap<FGuid, FWorldItemSplitReceipt> SplitReceipts;
};
