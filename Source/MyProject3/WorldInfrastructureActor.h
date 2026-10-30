#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldGameDataSubsystem.h"
#include "WorldInfrastructureActor.generated.h"


/**
 * Generic route-linked primitive infrastructure actor.
 * Materials are paid from fungible inventory stacks; tools are required but kept.
 * Route condition is authoritative and persisted on the route edge snapshot.
 */
UCLASS()
class MYPROJECT3_API AWorldInfrastructureActor : public AActor
{
    GENERATED_BODY()

public:
    AWorldInfrastructureActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Infrastructure")
    FName InfrastructureId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Infrastructure")
    bool bAffectsRouteGraph = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Infrastructure")
    bool bConstructed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Infrastructure")
    float LocalCondition = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Infrastructure|Limits", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxOperationReceipts = 32;

    UFUNCTION(BlueprintCallable, Category="Infrastructure")
    bool BuildFromInventory(const FGuid& OperationId, const FGuid& InventoryOwnerId);

    UFUNCTION(BlueprintCallable, Category="Infrastructure")
    bool RepairFromInventory(const FGuid& OperationId, const FGuid& InventoryOwnerId);

    UFUNCTION(BlueprintPure, Category="Infrastructure")
    bool IsInfrastructureBuilt() const;

    UFUNCTION(BlueprintPure, Category="Infrastructure")
    float GetInfrastructureCondition() const;

    /** Applies event wear to non-route infrastructure such as forward camps. */
    UFUNCTION(BlueprintCallable, Category="Infrastructure|Maintenance")
    bool AdvanceLocalWeatherWear(double DeltaSeconds);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="Infrastructure|Receipts")
    TArray<FGuid> CompletedOperationIds;

    bool FindInventory(const FGuid& OwnerId, UInventoryCraftingComponent*& OutInventory) const;
    bool ConsumeMaterialsAtomically(UInventoryCraftingComponent* Inventory,
        const FGuid& OwnerId, const TArray<struct FGameDataItemAmount>& Materials,
        const TArray<FName>& RequiredTools);
    bool HasCompletedOperation(const FGuid& OperationId) const;
    void RecordCompletedOperation(const FGuid& OperationId);
};
