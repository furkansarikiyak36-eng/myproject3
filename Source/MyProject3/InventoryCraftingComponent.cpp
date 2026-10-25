#include "InventoryCraftingComponent.h"

UInventoryCraftingComponent::UInventoryCraftingComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
}

int32 UInventoryCraftingComponent::GetItemCount(FName ItemId) const
{
 int32 Total = 0;
 if (ItemId.IsNone()) return Total;
 for (const FInventoryStack& Stack : Items) if (Stack.ItemId == ItemId) Total += FMath::Max(0, Stack.Quantity);
 return Total;
}

int32 UInventoryCraftingComponent::GetCurrentWeightGrams() const
{
 int64 Total = 0;
 for (const FInventoryStack& Stack : Items)
 {
  const int32* Weight = ItemWeightGrams.Find(Stack.ItemId);
  if (Weight && *Weight > 0) Total += static_cast<int64>(*Weight) * FMath::Max(0, Stack.Quantity);
 }
 return static_cast<int32>(FMath::Clamp<int64>(Total, 0, MAX_int32));
}

bool UInventoryCraftingComponent::HasCapacityFor(const TArray<FInventoryStack>& Additions) const
{
 TArray<FInventoryStack> Simulated = Items;
 for (const FInventoryStack& Addition : Additions)
 {
  if (Addition.ItemId.IsNone() || Addition.Quantity <= 0 || Addition.MaxStack <= 0) return false;
  int32 Remaining = Addition.Quantity;
  for (FInventoryStack& Existing : Simulated)
  {
   if (Existing.ItemId == Addition.ItemId && Existing.Quantity < Existing.MaxStack)
   {
    const int32 Delta = FMath::Min(Remaining, Existing.MaxStack - Existing.Quantity);
    Existing.Quantity += Delta; Remaining -= Delta;
    if (Remaining == 0) break;
   }
  }
  while (Remaining > 0)
  {
   if (Simulated.Num() >= MaxSlots) return false;
   FInventoryStack NewStack = Addition;
   NewStack.Quantity = FMath::Min(Remaining, Addition.MaxStack);
   Remaining -= NewStack.Quantity; Simulated.Add(NewStack);
  }
 }
 int64 Weight = 0;
 for (const FInventoryStack& Stack : Simulated)
 {
  const int32* ItemWeight = ItemWeightGrams.Find(Stack.ItemId);
  if (ItemWeight && *ItemWeight > 0) Weight += static_cast<int64>(*ItemWeight) * Stack.Quantity;
 }
 return Weight <= MaxCarryWeightGrams;
}

bool UInventoryCraftingComponent::TryAddItem(const FInventoryStack& Incoming)
{
 if (Incoming.ItemId.IsNone() || Incoming.Quantity <= 0 || Incoming.MaxStack <= 0 || !HasCapacityFor({Incoming})) return false;
 int32 Remaining = Incoming.Quantity;
 for (FInventoryStack& Existing : Items)
 {
  if (Existing.ItemId == Incoming.ItemId && Existing.Quantity < Existing.MaxStack)
  {
   const int32 Delta = FMath::Min(Remaining, Existing.MaxStack - Existing.Quantity);
   Existing.Quantity += Delta; Remaining -= Delta;
   if (Remaining == 0) return true;
  }
 }
 while (Remaining > 0)
 {
  FInventoryStack NewStack = Incoming;
  NewStack.Quantity = FMath::Min(Remaining, Incoming.MaxStack);
  Remaining -= NewStack.Quantity; Items.Add(NewStack);
 }
 return true;
}

bool UInventoryCraftingComponent::TryRemoveItem(FName ItemId, int32 Quantity)
{
 if (ItemId.IsNone() || Quantity <= 0 || GetItemCount(ItemId) < Quantity) return false;
 int32 Remaining = Quantity;
 for (int32 Index = Items.Num() - 1; Index >= 0 && Remaining > 0; --Index)
 {
  if (Items[Index].ItemId != ItemId) continue;
  const int32 Taken = FMath::Min(Remaining, Items[Index].Quantity);
  Items[Index].Quantity -= Taken; Remaining -= Taken;
  if (Items[Index].Quantity <= 0) Items.RemoveAt(Index);
 }
 if (QuickAccessItemId == ItemId && GetItemCount(ItemId) == 0) QuickAccessItemId = NAME_None;
 return Remaining == 0;
}

bool UInventoryCraftingComponent::SetQuickAccessItem(FName ItemId)
{
 if (ItemId.IsNone() || GetItemCount(ItemId) <= 0) return false;
 QuickAccessItemId = ItemId; return true;
}

int32 UInventoryCraftingComponent::FindRecipeIndex(FName RecipeId) const
{
 return RecipeId.IsNone() ? INDEX_NONE : Recipes.IndexOfByPredicate([RecipeId](const FCraftingRecipe& R){ return R.RecipeId == RecipeId; });
}

bool UInventoryCraftingComponent::RegisterRecipe(const FCraftingRecipe& Recipe)
{
 if (Recipe.RecipeId.IsNone() || Recipe.Output.ItemId.IsNone() || Recipe.Output.Quantity <= 0 ||
     Recipe.Output.MaxStack <= 0 || Recipe.Ingredients.IsEmpty() || FindRecipeIndex(Recipe.RecipeId) != INDEX_NONE) return false;
 for (const FRecipeIngredient& I : Recipe.Ingredients) if (I.ItemId.IsNone() || I.Quantity <= 0) return false;
 Recipes.Add(Recipe); return true;
}

bool UInventoryCraftingComponent::CanCraft(FName RecipeId, TArray<FInventoryStack>& OutMissing) const
{
 OutMissing.Reset();
 const int32 Index = FindRecipeIndex(RecipeId);
 if (!Recipes.IsValidIndex(Index) || !Recipes[Index].bEnabled) return false;
 const FCraftingRecipe& Recipe = Recipes[Index];
 TMap<FName,int32> Required;
 for (const FRecipeIngredient& I : Recipe.Ingredients) if (I.bConsume) Required.FindOrAdd(I.ItemId) += I.Quantity;
 for (const TPair<FName,int32>& P : Required)
 {
  const int32 Missing = P.Value - GetItemCount(P.Key);
  if (Missing > 0) { FInventoryStack S; S.ItemId=P.Key; S.Quantity=Missing; OutMissing.Add(S); }
 }
 if (!OutMissing.IsEmpty()) return false;
 TArray<FInventoryStack> Outputs; Outputs.Add(Recipe.Output); Outputs.Append(Recipe.Byproducts);
 return HasCapacityFor(Outputs);
}

bool UInventoryCraftingComponent::TryCraft(FName RecipeId)
{
 TArray<FInventoryStack> Missing;
 if (!CanCraft(RecipeId, Missing)) return false;
 const int32 Index = FindRecipeIndex(RecipeId);
 if (!Recipes.IsValidIndex(Index)) return false;
 const FCraftingRecipe Recipe = Recipes[Index];
 TMap<FName,int32> Required;
 for (const FRecipeIngredient& I : Recipe.Ingredients) if (I.bConsume) Required.FindOrAdd(I.ItemId) += I.Quantity;
 for (const TPair<FName,int32>& P : Required) if (GetItemCount(P.Key) < P.Value) return false;
 // Preflight the entire output set before consuming any ingredient.
 TArray<FInventoryStack> Outputs; Outputs.Add(Recipe.Output); Outputs.Append(Recipe.Byproducts);
 if (!HasCapacityFor(Outputs)) return false;
 // Snapshot allows rollback if an unexpected mutation failure occurs.
 const TArray<FInventoryStack> BeforeItems = Items;
 const FName BeforeQuickAccess = QuickAccessItemId;
 for (const TPair<FName,int32>& P : Required)
 {
  if (!TryRemoveItem(P.Key, P.Value)) { Items = BeforeItems; QuickAccessItemId = BeforeQuickAccess; return false; }
 }
 if (!TryAddItem(Recipe.Output)) { Items = BeforeItems; QuickAccessItemId = BeforeQuickAccess; return false; }
 for (const FInventoryStack& Byproduct : Recipe.Byproducts)
 {
  if (!TryAddItem(Byproduct)) { Items = BeforeItems; QuickAccessItemId = BeforeQuickAccess; return false; }
 }
 return true;
}
