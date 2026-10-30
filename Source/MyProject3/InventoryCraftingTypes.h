#pragma once
#include "CoreMinimal.h"
#include "InventoryCraftingTypes.generated.h"

UENUM(BlueprintType)
enum class EInventoryItemCategory : uint8 { Resource, Tool, Weapon, BuildingPart, Consumable, Protection, Quest, Other };

USTRUCT(BlueprintType)
struct FInventoryStack
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Inventory") FName ItemId = NAME_None;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Inventory", meta=(ClampMin="1")) int32 Quantity = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Inventory") EInventoryItemCategory Category = EInventoryItemCategory::Resource;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Inventory", meta=(ClampMin="1")) int32 MaxStack = 99;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Inventory") bool bIsTool = false;
 /** Valid only when this stack represents one unique physical world item. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Inventory") FGuid OwnedWorldItemId;
};

USTRUCT(BlueprintType)
struct FRecipeIngredient
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") FName ItemId = NAME_None;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting", meta=(ClampMin="1")) int32 Quantity = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") bool bConsume = true;
};

USTRUCT(BlueprintType)
struct FCraftingRecipe
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") FName RecipeId = NAME_None;
 /** Non-empty when this recipe is granted by a recorded discovery. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") FName RequiredUnlockDefinitionId = NAME_None;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") TArray<FRecipeIngredient> Ingredients;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") FInventoryStack Output;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") TArray<FInventoryStack> Byproducts;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") bool bEnabled = true;
};
