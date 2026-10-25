#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayCoreTypes.h"
#include "WorldOperationLedgerSubsystem.generated.h"

UENUM(BlueprintType)
enum class EWorldOperationLedgerEntryState : uint8
{
    Pending,
    Completed
};

USTRUCT(BlueprintType)
struct FWorldOperationLedgerEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    FGuid OperationId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    EWorldOperationLedgerEntryState State = EWorldOperationLedgerEntryState::Pending;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    FWorldOperationResult Result;
};

/**
 * World-local in-memory deduplication ledger.
 *
 * Call TryBeginOperation before a mutation. A caller must only commit a
 * mutation after successfully reserving the operation ID, then complete the
 * entry with the final result. This ledger does not itself make mutations
 * atomic and is not durable across world teardown/restart.
 */
UCLASS()
class MYPROJECT3_API UWorldOperationLedgerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay|Operation",
        meta = (ClampMin = "1"))
    int32 MaxEntries = 8192;

    UFUNCTION(BlueprintCallable, Category = "Gameplay|Operation")
    bool TryBeginOperation(const FGuid& OperationId);

    UFUNCTION(BlueprintCallable, Category = "Gameplay|Operation")
    bool CompleteOperation(const FWorldOperationResult& Result);

    UFUNCTION(BlueprintPure, Category = "Gameplay|Operation")
    bool TryGetCompletedResult(const FGuid& OperationId,
        FWorldOperationResult& OutResult) const;

    /** Only for an operation known not to have committed any mutation. */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Operation")
    bool CancelUncommittedOperation(const FGuid& OperationId);

    UFUNCTION(BlueprintPure, Category = "Gameplay|Operation")
    int32 GetEntryCount() const { return Entries.Num(); }

private:
    UPROPERTY(Transient)
    TMap<FGuid, FWorldOperationLedgerEntry> Entries;
};
