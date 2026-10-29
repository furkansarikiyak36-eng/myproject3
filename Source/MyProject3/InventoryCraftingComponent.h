#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryCraftingTypes.h"
#include "InventoryCraftingComponent.generated.h"

class AActor;

UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UInventoryCraftingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UInventoryCraftingComponent();
 virtual void BeginPlay() override;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory", meta=(ClampMin="1")) int32 MaxSlots = 24;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory", meta=(ClampMin="1")) int32 MaxCarryWeightGrams = 30000;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory") TArray<FInventoryStack> Items;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") TArray<FCraftingRecipe> Recipes;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory") TMap<FName, int32> ItemWeightGrams;
 /** Explicit class map used when a unique ledger-owned item is materialized in the world. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|World Items") TMap<FName, TSubclassOf<AActor>> PhysicalItemActorClasses;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory") FName QuickAccessItemId = NAME_None;
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryAddItem(const FInventoryStack& Incoming);
 /** Moves one registered physical actor into this inventory and destroys its world representation after commit. */
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryStorePhysicalActor(AActor* WorldActor);
 /** Removes the exact unique-item inventory reference without changing the world ownership ledger; transaction coordinators must commit/rollback both together. */
 bool TryRemoveOwnedItemReference(const FGuid& ItemId, FInventoryStack& OutRemovedStack);
 /** Transfers a unique owned item from inventory into a storage ledger owner. */
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryMoveOwnedItemToStorage(FGuid ItemId, FGuid StorageId);
 /** Moves a stored unique item to this inventory without duplicating the ledger record. */
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryWithdrawStoredItem(FGuid ItemId, FGuid StorageId);
 /** Spawns a configured physical actor and atomically transfers its unique item to world ownership. */
 UFUNCTION(BlueprintCallable, Category="Inventory|World Items") bool TryDropOwnedItem(FGuid ItemId, FVector SpawnLocation, AActor*& OutActor);
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryRemoveItem(FName ItemId, int32 Quantity);
 UFUNCTION(BlueprintCallable, Category="Inventory") bool SetQuickAccessItem(FName ItemId);
 UFUNCTION(BlueprintPure, Category="Inventory") int32 GetItemCount(FName ItemId) const;
 UFUNCTION(BlueprintPure, Category="Inventory") int32 GetCurrentWeightGrams() const;
 UFUNCTION(BlueprintPure, Category="Crafting") bool CanCraft(FName RecipeId, TArray<FInventoryStack>& OutMissing) const;
 UFUNCTION(BlueprintCallable, Category="Crafting") bool TryCraft(FName RecipeId);
 UFUNCTION(BlueprintCallable, Category="Crafting") bool RegisterRecipe(const FCraftingRecipe& Recipe);
 void ExportSnapshot(FGuid& OutOwnerActorId, FGuid& OutInventoryOwnerId,
  TArray<FInventoryStack>& OutItems, FName& OutQuickAccessItemId) const;
 bool RestoreSnapshot(const FGuid& InInventoryOwnerId, const TArray<FInventoryStack>& Snapshot,
  FName InQuickAccessItemId);
 FGuid GetInventoryOwnerId() const { return InventoryOwnerId; }
private:
 int32 FindRecipeIndex(FName RecipeId) const;
 bool HasCapacityFor(const TArray<FInventoryStack>& Additions) const;
 UPROPERTY(VisibleAnywhere, SaveGame, Category="Inventory") FGuid InventoryOwnerId;
};
