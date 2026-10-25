#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryCraftingTypes.h"
#include "InventoryCraftingComponent.generated.h"

UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UInventoryCraftingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UInventoryCraftingComponent();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory", meta=(ClampMin="1")) int32 MaxSlots = 24;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory", meta=(ClampMin="1")) int32 MaxCarryWeightGrams = 30000;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory") TArray<FInventoryStack> Items;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") TArray<FCraftingRecipe> Recipes;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory") TMap<FName, int32> ItemWeightGrams;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory") FName QuickAccessItemId = NAME_None;
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryAddItem(const FInventoryStack& Incoming);
 UFUNCTION(BlueprintCallable, Category="Inventory") bool TryRemoveItem(FName ItemId, int32 Quantity);
 UFUNCTION(BlueprintCallable, Category="Inventory") bool SetQuickAccessItem(FName ItemId);
 UFUNCTION(BlueprintPure, Category="Inventory") int32 GetItemCount(FName ItemId) const;
 UFUNCTION(BlueprintPure, Category="Inventory") int32 GetCurrentWeightGrams() const;
 UFUNCTION(BlueprintPure, Category="Crafting") bool CanCraft(FName RecipeId, TArray<FInventoryStack>& OutMissing) const;
 UFUNCTION(BlueprintCallable, Category="Crafting") bool TryCraft(FName RecipeId);
 UFUNCTION(BlueprintCallable, Category="Crafting") bool RegisterRecipe(const FCraftingRecipe& Recipe);
private:
 int32 FindRecipeIndex(FName RecipeId) const;
 bool HasCapacityFor(const TArray<FInventoryStack>& Additions) const;
};
