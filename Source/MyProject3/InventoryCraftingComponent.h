#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryCraftingTypes.h"
#include "InventoryCraftingComponent.generated.h"

class AActor;
class UHeatSourceComponent;

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
 /** Prototype item catalog. Keep ItemId unique; a shared DataAsset/registry can populate it later. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Definitions") TArray<FInventoryItemDefinition> ItemDefinitions;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory") TMap<FName, int32> ItemWeightGrams;
 /** Explicit class map used when a unique ledger-owned item is materialized in the world. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|World Items") TMap<FName, TSubclassOf<AActor>> PhysicalItemActorClasses;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory") FName QuickAccessItemId = NAME_None;
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryAddItem(const FInventoryStack& Incoming);
 /** Moves one registered physical actor into this inventory and destroys its world representation after commit. */
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryStorePhysicalActor(AActor* WorldActor);
 /** Transfers a unique owned item from inventory into a storage ledger owner. */
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryMoveOwnedItemToStorage(FGuid ItemId, FGuid StorageId);
 /** Moves a stored unique item to this inventory without duplicating the ledger record. */
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryWithdrawStoredItem(FGuid ItemId, FGuid StorageId);
 /** Spawns a configured physical actor and atomically transfers its unique item to world ownership. */
 UFUNCTION(BlueprintCallable, Category="Inventory|World Items") bool TryDropOwnedItem(FGuid ItemId, FVector SpawnLocation, AActor*& OutActor);
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryRemoveItem(FName ItemId, int32 Quantity);
 /** Consumes an unowned stack and applies its authored nutrition/water effect to a survival profile. */
 UFUNCTION(BlueprintCallable, Category="Inventory|Consumable") bool TryConsumeFood(FName ItemId, FGuid SurvivalEntityId, int32 Quantity = 1);
 /** Consumes authored feed from this inventory and applies its effect to one animal. */
 UFUNCTION(BlueprintCallable, Category="Inventory|Animals") bool TryFeedAnimal(FName ItemId, FGuid AnimalId, int32 Quantity = 1);
 /** Consumes authored fuel items into the heat source's single burn-time counter. */
 UFUNCTION(BlueprintCallable, Category="Inventory|Fuel") bool TryFuelHeatSource(FName ItemId, UHeatSourceComponent* HeatSource, int32 Quantity = 1);
 /** Equips a single authored equipment item, replacing anything in the same slot. */
 UFUNCTION(BlueprintCallable, Category="Inventory|Equipment") bool TryEquipItem(FName ItemId);
 UFUNCTION(BlueprintCallable, Category="Inventory|Equipment") bool TryUnequipItem(FName EquipmentSlotId);
 UFUNCTION(BlueprintPure, Category="Inventory|Equipment") float GetEquippedClothingInsulation() const;
 /** Called by the shared fixed-step coordinator; perishable stacks age in logical seconds. */
 bool CanAdvanceItemAging(double DeltaSeconds) const;
 bool AdvanceItemAging(double DeltaSeconds);
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
 const FInventoryItemDefinition* FindItemDefinition(FName ItemId) const;
 int32 GetConfiguredWeightGrams(FName ItemId) const;
 bool BuildRemovalPreview(FName ItemId, int32 Quantity, TArray<FInventoryStack>& OutItems) const;
 bool CalculateClothingInsulation(const TArray<FInventoryStack>& SourceItems, float& OutInsulation) const;
 bool HasCapacityFor(const TArray<FInventoryStack>& Additions) const;
 UPROPERTY(VisibleAnywhere, SaveGame, Category="Inventory") FGuid InventoryOwnerId;
};
