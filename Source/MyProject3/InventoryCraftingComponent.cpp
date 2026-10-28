#include "InventoryCraftingComponent.h"

UInventoryCraftingComponent::UInventoryCraftingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UInventoryCraftingComponent::IsValidStack(const FInventoryStack& Stack) const
{
    return !Stack.ItemId.IsNone() && Stack.Quantity > 0 && Stack.MaxStack > 0;
}

int32 UInventoryCraftingComponent::GetItemCount(FName ItemId) const
{
    if (ItemId.IsNone())
    {
        return 0;
    }

    int64 Total = 0;
    for (const FInventoryStack& Stack : Items)
    {
        if (Stack.ItemId == ItemId)
        {
            Total += FMath::Max(0, Stack.Quantity);
        }
    }
    return static_cast<int32>(FMath::Clamp<int64>(Total, 0, MAX_int32));
}

int32 UInventoryCraftingComponent::GetCurrentWeightGrams() const
{
    int64 Total = 0;
    for (const FInventoryStack& Stack : Items)
    {
        const int32* Weight = ItemWeightGrams.Find(Stack.ItemId);
        if (Weight && *Weight > 0)
        {
            Total += static_cast<int64>(*Weight) * FMath::Max(0, Stack.Quantity);
        }
    }
    return static_cast<int32>(FMath::Clamp<int64>(Total, 0, MAX_int32));
}

bool UInventoryCraftingComponent::HasCapacityFor(
    const TArray<FInventoryStack>& BaseItems,
    const TArray<FInventoryStack>& Additions) const
{
    if (MaxSlots <= 0 || MaxCarryWeightGrams <= 0)
    {
        return false;
    }

    TArray<FInventoryStack> Simulated = BaseItems;
    for (const FInventoryStack& Addition : Additions)
    {
        if (!IsValidStack(Addition))
        {
            return false;
        }

        const int32* AddedWeight = ItemWeightGrams.Find(Addition.ItemId);
        if (!AddedWeight || *AddedWeight <= 0)
        {
            return false;
        }

        int32 Remaining = Addition.Quantity;
        for (FInventoryStack& Existing : Simulated)
        {
            if (Existing.ItemId != Addition.ItemId)
            {
                continue;
            }
            // One item definition must have a single consistent stack policy.
            if (Existing.MaxStack != Addition.MaxStack ||
                Existing.Category != Addition.Category ||
                Existing.bIsTool != Addition.bIsTool ||
                Existing.Quantity < 0 || Existing.Quantity > Existing.MaxStack)
            {
                return false;
            }

            if (Existing.Quantity < Existing.MaxStack)
            {
                const int32 Delta = FMath::Min(Remaining, Existing.MaxStack - Existing.Quantity);
                Existing.Quantity += Delta;
                Remaining -= Delta;
                if (Remaining == 0)
                {
                    break;
                }
            }
        }

        while (Remaining > 0)
        {
            if (Simulated.Num() >= MaxSlots)
            {
                return false;
            }
            FInventoryStack NewStack = Addition;
            NewStack.Quantity = FMath::Min(Remaining, Addition.MaxStack);
            Remaining -= NewStack.Quantity;
            Simulated.Add(NewStack);
        }
    }

    if (Simulated.Num() > MaxSlots)
    {
        return false;
    }

    int64 TotalWeight = 0;
    for (const FInventoryStack& Stack : Simulated)
    {
        if (!IsValidStack(Stack))
        {
            return false;
        }
        const int32* Weight = ItemWeightGrams.Find(Stack.ItemId);
        if (!Weight || *Weight <= 0)
        {
            return false;
        }
        TotalWeight += static_cast<int64>(*Weight) * Stack.Quantity;
        if (TotalWeight > MaxCarryWeightGrams)
        {
            return false;
        }
    }
    return true;
}

bool UInventoryCraftingComponent::TryAddItemInternal(const FInventoryStack& Incoming)
{
    if (!IsValidStack(Incoming) || !HasCapacityFor(Items, { Incoming }))
    {
        return false;
    }

    int32 Remaining = Incoming.Quantity;
    for (FInventoryStack& Existing : Items)
    {
        if (Existing.ItemId != Incoming.ItemId)
        {
            continue;
        }
        if (Existing.MaxStack != Incoming.MaxStack ||
            Existing.Category != Incoming.Category ||
            Existing.bIsTool != Incoming.bIsTool)
        {
            return false;
        }

        const int32 Delta = FMath::Min(Remaining, Existing.MaxStack - Existing.Quantity);
        Existing.Quantity += Delta;
        Remaining -= Delta;
        if (Remaining == 0)
        {
            return true;
        }
    }

    while (Remaining > 0)
    {
        FInventoryStack NewStack = Incoming;
        NewStack.Quantity = FMath::Min(Remaining, Incoming.MaxStack);
        Remaining -= NewStack.Quantity;
        Items.Add(NewStack);
    }
    return true;
}

bool UInventoryCraftingComponent::TryRemoveItemInternal(FName ItemId, int32 Quantity)
{
    if (ItemId.IsNone() || Quantity <= 0 || GetItemCount(ItemId) < Quantity)
    {
        return false;
    }

    int32 Remaining = Quantity;
    for (int32 Index = Items.Num() - 1; Index >= 0 && Remaining > 0; --Index)
    {
        if (Items[Index].ItemId != ItemId)
        {
            continue;
        }
        const int32 Taken = FMath::Min(Remaining, Items[Index].Quantity);
        Items[Index].Quantity -= Taken;
        Remaining -= Taken;
        if (Items[Index].Quantity == 0)
        {
            Items.RemoveAt(Index);
        }
    }

    if (QuickAccessItemId == ItemId && GetItemCount(ItemId) == 0)
    {
        QuickAccessItemId = NAME_None;
    }
    return Remaining == 0;
}

bool UInventoryCraftingComponent::TryAddItem(const FInventoryStack& Incoming)
{
    return TryAddItemResult(Incoming, FGuid::NewGuid()).IsSuccess();
}

bool UInventoryCraftingComponent::TryRemoveItem(FName ItemId, int32 Quantity)
{
    return TryRemoveItemResult(ItemId, Quantity, FGuid::NewGuid()).IsSuccess();
}

bool UInventoryCraftingComponent::SetQuickAccessItem(FName ItemId)
{
    if (ItemId.IsNone() || GetItemCount(ItemId) <= 0)
    {
        return false;
    }
    QuickAccessItemId = ItemId;
    return true;
}

int32 UInventoryCraftingComponent::FindRecipeIndex(FName RecipeId) const
{
    return RecipeId.IsNone()
        ? INDEX_NONE
        : Recipes.IndexOfByPredicate([RecipeId](const FCraftingRecipe& Recipe)
        {
            return Recipe.RecipeId == RecipeId;
        });
}

bool UInventoryCraftingComponent::RegisterRecipe(const FCraftingRecipe& Recipe)
{
    if (Recipe.RecipeId.IsNone() || !IsValidStack(Recipe.Output) ||
        Recipe.Ingredients.IsEmpty() || FindRecipeIndex(Recipe.RecipeId) != INDEX_NONE)
    {
        return false;
    }

    for (const FRecipeIngredient& Ingredient : Recipe.Ingredients)
    {
        if (Ingredient.ItemId.IsNone() || Ingredient.Quantity <= 0)
        {
            return false;
        }
    }
    for (const FInventoryStack& Byproduct : Recipe.Byproducts)
    {
        if (!IsValidStack(Byproduct))
        {
            return false;
        }
    }

    Recipes.Add(Recipe);
    return true;
}

bool UInventoryCraftingComponent::CanCraftAgainstInventory(
    const FCraftingRecipe& Recipe,
    const TArray<FInventoryStack>& BaseItems,
    TArray<FInventoryStack>& OutMissing,
    TArray<FInventoryStack>& OutPostConsumptionItems) const
{
    OutMissing.Reset();
    OutPostConsumptionItems = BaseItems;

    if (!Recipe.bEnabled || !IsValidStack(Recipe.Output) || Recipe.Ingredients.IsEmpty())
    {
        return false;
    }

    TMap<FName, int64> RequiredAvailable;
    TMap<FName, int64> RequiredConsumed;
    for (const FRecipeIngredient& Ingredient : Recipe.Ingredients)
    {
        if (Ingredient.ItemId.IsNone() || Ingredient.Quantity <= 0)
        {
            return false;
        }
        RequiredAvailable.FindOrAdd(Ingredient.ItemId) += Ingredient.Quantity;
        if (Ingredient.bConsume)
        {
            RequiredConsumed.FindOrAdd(Ingredient.ItemId) += Ingredient.Quantity;
        }
    }

    for (const TPair<FName, int64>& Requirement : RequiredAvailable)
    {
        const int64 Have = GetItemCount(Requirement.Key);
        const int64 Missing = Requirement.Value - Have;
        if (Missing > 0)
        {
            FInventoryStack MissingStack;
            MissingStack.ItemId = Requirement.Key;
            MissingStack.Quantity = static_cast<int32>(FMath::Min<int64>(Missing, MAX_int32));
            OutMissing.Add(MissingStack);
        }
    }
    if (!OutMissing.IsEmpty())
    {
        return false;
    }

    // Remove only consumed ingredients from the simulation, preserving tools.
    for (const TPair<FName, int64>& Consumed : RequiredConsumed)
    {
        int32 Remaining = static_cast<int32>(FMath::Min<int64>(Consumed.Value, MAX_int32));
        for (int32 Index = OutPostConsumptionItems.Num() - 1; Index >= 0 && Remaining > 0; --Index)
        {
            if (OutPostConsumptionItems[Index].ItemId != Consumed.Key)
            {
                continue;
            }
            const int32 Taken = FMath::Min(Remaining, OutPostConsumptionItems[Index].Quantity);
            OutPostConsumptionItems[Index].Quantity -= Taken;
            Remaining -= Taken;
            if (OutPostConsumptionItems[Index].Quantity == 0)
            {
                OutPostConsumptionItems.RemoveAt(Index);
            }
        }
        if (Remaining > 0)
        {
            return false;
        }
    }
    return true;
}

bool UInventoryCraftingComponent::CanCraft(FName RecipeId, TArray<FInventoryStack>& OutMissing) const
{
    OutMissing.Reset();
    const int32 Index = FindRecipeIndex(RecipeId);
    if (!Recipes.IsValidIndex(Index))
    {
        return false;
    }

    TArray<FInventoryStack> PostConsumption;
    if (!CanCraftAgainstInventory(Recipes[Index], Items, OutMissing, PostConsumption))
    {
        return false;
    }

    TArray<FInventoryStack> Outputs;
    Outputs.Add(Recipes[Index].Output);
    Outputs.Append(Recipes[Index].Byproducts);
    return HasCapacityFor(PostConsumption, Outputs);
}

FWorldOperationResult UInventoryCraftingComponent::TryAddItemResult(
    const FInventoryStack& Incoming,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }
    if (!IsValidStack(Incoming))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidTarget, OperationId,
            FText::FromString(TEXT("The incoming inventory stack is invalid.")));
    }
    if (!HasCapacityFor(Items, { Incoming }))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::CapacityExceeded, OperationId,
            FText::FromString(TEXT("Inventory capacity, item weight, or item definition prevents pickup.")));
    }
    if (!TryAddItemInternal(Incoming))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Conflict, OperationId,
            FText::FromString(TEXT("The item stack conflicts with the existing item definition.")));
    }
    return FWorldOperationResult::Success(
        OperationId, FText::FromString(TEXT("Item added to inventory.")), TArray<FGuid>());
}

FWorldOperationResult UInventoryCraftingComponent::TryRemoveItemResult(
    FName ItemId,
    int32 Quantity,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }
    if (ItemId.IsNone() || Quantity <= 0)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidTarget, OperationId,
            FText::FromString(TEXT("A valid item ID and positive quantity are required.")));
    }
    if (GetItemCount(ItemId) < Quantity)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InsufficientResources, OperationId,
            FText::FromString(TEXT("The inventory does not contain enough items.")));
    }
    if (!TryRemoveItemInternal(ItemId, Quantity))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InternalError, OperationId,
            FText::FromString(TEXT("Inventory removal failed without committing a partial result.")));
    }
    return FWorldOperationResult::Success(
        OperationId, FText::FromString(TEXT("Items removed from inventory.")), TArray<FGuid>());
}

FWorldOperationResult UInventoryCraftingComponent::TryCraftResult(
    FName RecipeId,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }
    const int32 Index = FindRecipeIndex(RecipeId);
    if (!Recipes.IsValidIndex(Index) || !Recipes[Index].bEnabled)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidTarget, OperationId,
            FText::FromString(TEXT("The recipe is missing or disabled.")));
    }

    TArray<FInventoryStack> Missing;
    TArray<FInventoryStack> PostConsumption;
    const FCraftingRecipe Recipe = Recipes[Index];
    if (!CanCraftAgainstInventory(Recipe, Items, Missing, PostConsumption))
    {
        return FWorldOperationResult::Failure(
            Missing.IsEmpty() ? EWorldOperationResultCode::PrerequisiteNotMet
                              : EWorldOperationResultCode::MissingMaterial,
            OperationId,
            Missing.IsEmpty()
                ? FText::FromString(TEXT("Recipe prerequisites are not satisfied."))
                : FText::FromString(TEXT("Required ingredients or tools are missing.")));
    }

    TArray<FInventoryStack> Outputs;
    Outputs.Add(Recipe.Output);
    Outputs.Append(Recipe.Byproducts);
    if (!HasCapacityFor(PostConsumption, Outputs))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::CapacityExceeded, OperationId,
            FText::FromString(TEXT("Crafting output would exceed slot or carry-weight limits.")));
    }

    const TArray<FInventoryStack> BeforeItems = Items;
    const FName BeforeQuickAccess = QuickAccessItemId;
    for (const FRecipeIngredient& Ingredient : Recipe.Ingredients)
    {
        if (Ingredient.bConsume && !TryRemoveItemInternal(Ingredient.ItemId, Ingredient.Quantity))
        {
            Items = BeforeItems;
            QuickAccessItemId = BeforeQuickAccess;
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::InternalError, OperationId,
                FText::FromString(TEXT("Crafting rolled back after ingredient consumption failed.")));
        }
    }
    if (!TryAddItemInternal(Recipe.Output))
    {
        Items = BeforeItems;
        QuickAccessItemId = BeforeQuickAccess;
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InternalError, OperationId,
            FText::FromString(TEXT("Crafting rolled back because output insertion failed.")));
    }
    for (const FInventoryStack& Byproduct : Recipe.Byproducts)
    {
        if (!TryAddItemInternal(Byproduct))
        {
            Items = BeforeItems;
            QuickAccessItemId = BeforeQuickAccess;
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::InternalError, OperationId,
                FText::FromString(TEXT("Crafting rolled back because byproduct insertion failed.")));
        }
    }

    return FWorldOperationResult::Success(
        OperationId, FText::FromString(TEXT("Crafting completed.")), TArray<FGuid>());
}

bool UInventoryCraftingComponent::TryCraft(FName RecipeId)
{
    return TryCraftResult(RecipeId, FGuid::NewGuid()).IsSuccess();
}
