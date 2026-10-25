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

    UPROPERTY()
    FGuid OperationId;

    UPROPERTY()
    FGuid ItemId;

    UPROPERTY()
    int64 ResultingRevision = 0;

    UPROPERTY()
    EWorldItemOwnerKind ResultingOwner = EWorldItemOwnerKind::World;

    // Immutable result snapshot returned by idempotent retries.
    UPROPERTY()
    FWorldOwnedItemRecord ResultRecord;

    UPROPERTY()
    int64 ExpectedRevision = 0;

    UPROPERTY()
    EWorldItemOwnerKind ExpectedOwner = EWorldItemOwnerKind::World;

    UPROPERTY()
    EWorldItemOwnerKind RequestedOwner = EWorldItemOwnerKind::World;

    UPROPERTY()
    FGuid RequestedOwnerId;

    UPROPERTY()
    FVector RequestedWorldLocation = FVector::ZeroVector;
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

    UFUNCTION(BlueprintPure, Category="World|Items")
    bool TryGetItem(const FGuid& ItemId, FWorldOwnedItemRecord& OutRecord) const;

    UFUNCTION(BlueprintPure, Category="World|Items")
    int32 GetTrackedItemCount() const { return Items.Num(); }
    void ExportSnapshot(TArray<FWorldOwnedItemRecord>& OutItems) const;
    bool RestoreSnapshot(const TArray<FWorldOwnedItemRecord>& Snapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Items")
    TMap<FGuid, FWorldOwnedItemRecord> Items;

    UPROPERTY(Transient)
    TMap<FGuid, FWorldItemTransferReceipt> Receipts;
};
