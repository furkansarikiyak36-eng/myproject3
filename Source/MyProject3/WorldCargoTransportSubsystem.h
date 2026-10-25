#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldCargoTransportSubsystem.generated.h"

class UWorldLogisticsRouteSubsystem;
class UWorldItemOwnershipSubsystem;

UENUM(BlueprintType)
enum class ECargoTransportState : uint8 { InTransit, Delivered, Blocked, Cancelled };

USTRUCT(BlueprintType)
struct FWorldCargoTransportRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") FGuid CargoId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") FGuid ItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") FGuid DestinationStorageId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") FName OriginNode = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") FName DestinationNode = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") TArray<FName> Route;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") float RouteCost = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") float Progress = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") float CostPerSecond = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") int64 ItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") ECargoTransportState State = ECargoTransportState::InTransit;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") FGuid DeliveryOperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Logistics") int64 Revision = 1;
};

/** Moves item ownership through a route graph; actor movement is a visual/physics adapter. */
UCLASS()
class MYPROJECT3_API UWorldCargoTransportSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Logistics|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxActiveCargo = 1024;
    UFUNCTION(BlueprintCallable, Category="World|Logistics") bool StartTransport(
        const FGuid& ItemId, int64 ExpectedItemRevision, const FGuid& SourceStorageId,
        FName OriginNode, FName DestinationNode, const FGuid& DestinationStorageId,
        float CostPerSecond, FGuid& OutCargoId);
    UFUNCTION(BlueprintCallable, Category="World|Logistics") bool AdvanceCargoStep(double DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="World|Logistics") bool RetryBlockedDelivery(const FGuid& CargoId);
    UFUNCTION(BlueprintPure, Category="World|Logistics") bool GetCargo(const FGuid& CargoId, FWorldCargoTransportRecord& OutCargo) const;
private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Logistics") TMap<FGuid, FWorldCargoTransportRecord> Cargo;
    UWorldLogisticsRouteSubsystem* GetRoutes() const;
    UWorldItemOwnershipSubsystem* GetOwnership() const;
    bool TryDeliver(FWorldCargoTransportRecord& Record);
};
