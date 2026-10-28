#pragma once

#include "CoreMinimal.h"
#include "InventoryCraftingTypes.generated.h"

UENUM(BlueprintType)
enum class EInventoryItemCategory : uint8
{
    Resource UMETA(DisplayName = "Resource"),
    Tool UMETA(DisplayName = "Tool"),
    Weapon UMETA(DisplayName = "Weapon"),
    BuildingPart UMETA(DisplayName = "Building Part"),
    Consumable UMETA(DisplayName = "Consumable"),
    Protection UMETA(DisplayName = "Protection"),
    Quest UMETA(DisplayName = "Quest"),
    Other UMETA(DisplayName = "Other")
};

USTRUCT(BlueprintType)
struct FInventoryStack
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1"))
    int32 Quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    EInventoryItemCategory Category = EInventoryItemCategory::Resource;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1"))
    int32 MaxStack = 99;

    /** Tools are item definitions too; recipe ingredients control consumption. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    bool bIsTool = false;
};

USTRUCT(BlueprintType)
struct FRecipeIngredient
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "1"))
    int32 Quantity = 1;

    /** False means the ingredient is a requirement/tool and is not consumed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    bool bConsume = true;
};

USTRUCT(BlueprintType)
struct FCraftingRecipe
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    FName RecipeId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    TArray<FRecipeIngredient> Ingredients;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    FInventoryStack Output;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    TArray<FInventoryStack> Byproducts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    bool bEnabled = true;
};
