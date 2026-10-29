#pragma once
#include "CoreMinimal.h"
#include "InventoryCraftingTypes.generated.h"

UENUM(BlueprintType)
enum class EInventoryItemCategory : uint8 { Resource, Tool, Weapon, BuildingPart, Consumable, Protection, Quest, Other };

/** Authored item rules. These are data contracts; a later asset/catalog adapter can populate them. */
USTRUCT(BlueprintType)
struct FInventoryItemDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Definitions") FName ItemId = NAME_None;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Definitions") EInventoryItemCategory Category = EInventoryItemCategory::Other;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Definitions", meta=(ClampMin="1")) int32 UnitWeightGrams = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Definitions", meta=(ClampMin="1")) int32 MaxStack = 99;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Definitions") bool bIsTool = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Consumable") bool bConsumable = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Consumable", meta=(ClampMin="0.0", ClampMax="10000.0")) float CaloriesKcal = 0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Consumable", meta=(ClampMin="0.0", ClampMax="100.0")) float HydrationRelief = 0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Medical", meta=(ClampMin="0.0", ClampMax="100.0")) float HealthRestore = 0.0f;
 /** Optional exact affliction ID this item removes. Empty means no targeted cure. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Medical") FName TreatAfflictionId = NAME_None;
 /** Optional crop ID planted by this seed. Empty means this item is not a plantable seed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Farm") FName SeedCropDefinitionId = NAME_None;
 /** Soil moisture fraction restored per use; 0 means not a watering item. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Farm", meta=(ClampMin="0.0", ClampMax="1.0")) float MoistureGain = 0.0f;
 /** 0 means non-perishable. Rates use the shared in-game simulation clock. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Spoilage", meta=(ClampMin="0.0")) double ShelfLifeSeconds = 0.0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Spoilage") FName SpoiledDefinitionId = NAME_None;
 /** Fuel duration contributed to the single authoritative heat-source fuel counter. 0 means not fuel. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Fuel", meta=(ClampMin="0.0", ClampMax="86400.0")) float FuelBurnSeconds = 0.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Animals", meta=(ClampMin="0.0", ClampMax="100.0")) float AnimalFeedHungerReduction = 0.0f;
 /** Empty means not equipable. Slot IDs must be stable and unique per equipped item. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Equipment") FName EquipmentSlotId = NAME_None;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Equipment", meta=(ClampMin="0.0", ClampMax="1.0")) float ClothingInsulation = 0.0f;
};

/** Item stack state shared by inventory, save/load, and the fixed-step aging model. */
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
 /** Age in logical simulation seconds. Stacks with different ages are never merged. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Inventory", meta=(ClampMin="0.0")) double AgeSeconds = 0.0;
 /** Spoiled stacks cannot be consumed as fresh food or equipped as fresh supplies. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Inventory") bool bSpoiled = false;
 /** Non-empty only when this unique stack is equipped in the specified authored slot. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Inventory") FName EquippedSlotId = NAME_None;
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
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") TArray<FRecipeIngredient> Ingredients;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") FInventoryStack Output;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") TArray<FInventoryStack> Byproducts;
 /** Optional unlock token committed by discovery; NAME_None means always available. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") FName RequiredUnlockId = NAME_None;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crafting") bool bEnabled = true;
};
