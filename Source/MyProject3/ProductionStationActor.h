#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProductionStationActor.generated.h"

class UInventoryCraftingComponent;
class UStaticMeshComponent;

/**
 * A work station in the world (P4.3): pit kiln, charcoal pit, smoke rack, hide frame,
 * grinding stone, drying rack, threshing floor, campfire, compost heap, log workbench.
 * StationDataId names its entry in Config/GameData/stations.json; the station's recipes
 * come from transformations.json. A station that needs fire (or a charcoal-hot fire)
 * only starts work with such a fire within reach. Work runs on the world clock in the
 * production subsystem; inputs are really used up and outputs need room.
 */
UCLASS()
class MYPROJECT3_API AProductionStationActor : public AActor
{
    GENERATED_BODY()

public:
    AProductionStationActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Station") FName StationDataId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Station", meta=(ClampMin="50")) float FireSearchRadiusCm = 300.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Station") UStaticMeshComponent* Mesh = nullptr;

    /** Registers the station with production (once); safe to call again. */
    UFUNCTION(BlueprintCallable, Category="Station") bool EnsureRegistered();
    UFUNCTION(BlueprintPure, Category="Station") FGuid GetStationId() const { return StationId; }

    /** True when the station needs no fire, or a lit (hot, if required) fire is within reach. */
    UFUNCTION(BlueprintPure, Category="Station") bool HasRequiredFire() const;

    /** Moves items from the inventory into the station. */
    UFUNCTION(BlueprintCallable, Category="Station") bool TryDeposit(UInventoryCraftingComponent* Inventory, FName ItemId, int32 Quantity);
    /** Starts a recipe if its inputs are in the station and the fire rule holds. */
    UFUNCTION(BlueprintCallable, Category="Station") bool TryStartRecipe(FName RecipeId, FGuid& OutJobId);
    /** Starts a knowledge-gated recipe only when this player has the required discoveries. */
    UFUNCTION(BlueprintCallable, Category="Station|Progression") bool TryStartRecipeForPlayer(FName RecipeId, FGuid PlayerEntityId, FGuid& OutJobId);
    /** Player inventory adapter. Requires the inventory to be bound to a registered player's progression profile. */
    UFUNCTION(BlueprintCallable, Category="Station|Progression") bool TryStartRecipeForInventory(FName RecipeId, UInventoryCraftingComponent* Inventory, FGuid& OutJobId);
    /** Collects every finished job into the station's stock. Returns how many were collected. */
    UFUNCTION(BlueprintCallable, Category="Station") int32 CollectFinishedJobs();
    /** Moves items from the station's stock into the inventory. */
    UFUNCTION(BlueprintCallable, Category="Station") bool TryTake(UInventoryCraftingComponent* Inventory, FName ItemId, int32 Quantity);
    UFUNCTION(BlueprintPure, Category="Station") int32 GetStockCount(FName ItemId) const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Station") FGuid StationId;
};
