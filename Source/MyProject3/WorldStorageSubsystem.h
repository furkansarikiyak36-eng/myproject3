#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldStorageSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FWorldStorageContainerRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Storage")
    FGuid StorageId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Storage", meta=(ClampMin="1"))
    int32 CapacityUnits = 128;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Storage")
    int64 Revision = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Storage")
    bool bAvailable = true;
};

USTRUCT(BlueprintType)
struct FWorldStorageIncomingReservation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Storage")
    FGuid CargoId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Storage")
    FGuid ItemId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Storage")
    FGuid DestinationStorageId;
};

/**
 * Storage capacity and incoming-slot reservations. Item ownership remains
 * authoritative in UWorldItemOwnershipSubsystem; this subsystem owns only
 * container configuration and reservations for items currently in transit.
 */
UCLASS()
class MYPROJECT3_API UWorldStorageSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Storage|Limits",
        meta=(ClampMin="1", ClampMax="4096"))
    int32 MaxStorages = 256;

    UFUNCTION(BlueprintCallable, Category="World|Storage")
    bool RegisterStorage(const FGuid& StorageId, int32 CapacityUnits);

    UFUNCTION(BlueprintPure, Category="World|Storage")
    bool GetStorage(const FGuid& StorageId, FWorldStorageContainerRecord& OutStorage) const;

    UFUNCTION(BlueprintPure, Category="World|Storage")
    bool IsStorageAvailable(const FGuid& StorageId) const;

    UFUNCTION(BlueprintCallable, Category="World|Storage")
    bool SetStorageAvailable(const FGuid& StorageId, bool bAvailable);

    UFUNCTION(BlueprintPure, Category="World|Storage")
    bool CanAcceptItem(const FGuid& StorageId, const FGuid& ItemId) const;

    UFUNCTION(BlueprintCallable, Category="World|Storage")
    bool TryReserveIncoming(const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId);

    UFUNCTION(BlueprintPure, Category="World|Storage")
    bool HasIncomingReservation(const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId) const;

    UFUNCTION(BlueprintPure, Category="World|Storage")
    bool CanCommitIncomingReservation(const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId) const;

    UFUNCTION(BlueprintCallable, Category="World|Storage")
    bool CommitIncomingReservation(const FGuid& StorageId, const FGuid& CargoId, const FGuid& ItemId);

    UFUNCTION(BlueprintCallable, Category="World|Storage")
    bool ReleaseIncomingReservation(const FGuid& CargoId);

    UFUNCTION(BlueprintPure, Category="World|Storage")
    bool GetStoredItemIds(const FGuid& StorageId, TArray<FGuid>& OutItemIds) const;

    void ExportSnapshot(TArray<FWorldStorageContainerRecord>& OutStorages,
        TArray<FWorldStorageIncomingReservation>& OutReservations) const;

    bool RestoreSnapshot(const TArray<FWorldStorageContainerRecord>& StorageSnapshot,
        const TArray<FWorldStorageIncomingReservation>& ReservationSnapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Storage")
    TMap<FGuid, FWorldStorageContainerRecord> Storages;

    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Storage")
    TMap<FGuid, FWorldStorageIncomingReservation> IncomingReservations;

    UPROPERTY(Transient)
    TSet<FGuid> LiveStorageIds;

    class UWorldItemOwnershipSubsystem* GetOwnership() const;
    int32 CountReservationsForStorage(const FGuid& StorageId) const;
};
