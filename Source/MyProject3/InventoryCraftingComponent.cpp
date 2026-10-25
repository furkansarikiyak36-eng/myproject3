#include "InventoryCraftingComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStorageSubsystem.h"
#include "LogBuildPiece.h"

UInventoryCraftingComponent::UInventoryCraftingComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
 InventoryOwnerId = FGuid::NewGuid();
}

void UInventoryCraftingComponent::BeginPlay()
{
 Super::BeginPlay();
 if (!InventoryOwnerId.IsValid()) InventoryOwnerId = FGuid::NewGuid();
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
  // Unknown/invalid weight must never make an item silently weightless.
  if (!Weight || *Weight <= 0 || Stack.Quantity < 0) return MAX_int32;
  Total += static_cast<int64>(*Weight) * Stack.Quantity;
  if (Total > MAX_int32) return MAX_int32;
 }
 return static_cast<int32>(Total);
}

bool UInventoryCraftingComponent::HasCapacityFor(const TArray<FInventoryStack>& Additions) const
{
 TArray<FInventoryStack> Simulated = Items;
 for (const FInventoryStack& Addition : Additions)
 {
  if (Addition.ItemId.IsNone() || Addition.Quantity <= 0 || Addition.MaxStack <= 0 ||
      (Addition.OwnedWorldItemId.IsValid() && (Addition.Quantity != 1 || Addition.MaxStack != 1 ||
       Simulated.ContainsByPredicate([&Addition](const FInventoryStack& S) { return S.OwnedWorldItemId == Addition.OwnedWorldItemId; })))) return false;
  int32 Remaining = Addition.Quantity;
  for (FInventoryStack& Existing : Simulated)
  {
   if (Existing.ItemId == Addition.ItemId && !Existing.OwnedWorldItemId.IsValid() && !Addition.OwnedWorldItemId.IsValid() && Existing.Quantity < Existing.MaxStack)
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
  // Fail closed until every item has a positive, configured weight.
  if (!ItemWeight || *ItemWeight <= 0 || Stack.Quantity <= 0) return false;
  Weight += static_cast<int64>(*ItemWeight) * Stack.Quantity;
  if (Weight > MaxCarryWeightGrams) return false;
 }
 return true;
}

bool UInventoryCraftingComponent::TryAddItem(const FInventoryStack& Incoming)
{
 if (Incoming.ItemId.IsNone() || Incoming.Quantity <= 0 || Incoming.MaxStack <= 0 ||
     (Incoming.OwnedWorldItemId.IsValid() && (Incoming.Quantity != 1 || Incoming.MaxStack != 1 ||
      Items.ContainsByPredicate([&Incoming](const FInventoryStack& S) { return S.OwnedWorldItemId == Incoming.OwnedWorldItemId; }))) ||
     !HasCapacityFor({Incoming})) return false;
 int32 Remaining = Incoming.Quantity;
 for (FInventoryStack& Existing : Items)
 {
  if (Existing.ItemId == Incoming.ItemId && !Existing.OwnedWorldItemId.IsValid() && !Incoming.OwnedWorldItemId.IsValid() && Existing.Quantity < Existing.MaxStack)
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

void UInventoryCraftingComponent::ExportSnapshot(
 FGuid& OutOwnerActorId, FGuid& OutInventoryOwnerId,
 TArray<FInventoryStack>& OutItems, FName& OutQuickAccessItemId) const
{
 OutOwnerActorId.Invalidate();
 if (AActor* Owner = GetOwner())
 {
  if (UWorldObjectIdentityComponent* Identity =
      Owner->FindComponentByClass<UWorldObjectIdentityComponent>())
   OutOwnerActorId = Identity->GetInstanceId();
 }
 OutInventoryOwnerId = InventoryOwnerId;
 OutItems = Items;
 OutQuickAccessItemId = QuickAccessItemId;
}

bool UInventoryCraftingComponent::RestoreSnapshot(
 const FGuid& InInventoryOwnerId, const TArray<FInventoryStack>& Snapshot,
 FName InQuickAccessItemId)
{
 if (!InInventoryOwnerId.IsValid() || Snapshot.Num() > MaxSlots || MaxSlots <= 0 ||
     MaxCarryWeightGrams <= 0) return false;

 UWorldItemOwnershipSubsystem* Ownership =
  GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
 TSet<FGuid> OwnedIds;
 int64 TotalWeight = 0;
 for (const FInventoryStack& Stack : Snapshot)
 {
  if (Stack.ItemId.IsNone() || Stack.Quantity <= 0 || Stack.MaxStack <= 0 ||
      Stack.Quantity > Stack.MaxStack) return false;
  const int32* Weight = ItemWeightGrams.Find(Stack.ItemId);
  if (!Weight || *Weight <= 0) return false;
  TotalWeight += static_cast<int64>(*Weight) * Stack.Quantity;
  if (TotalWeight > MaxCarryWeightGrams) return false;

  if (Stack.OwnedWorldItemId.IsValid())
  {
   if (!Ownership || Stack.Quantity != 1 || Stack.MaxStack != 1 ||
       OwnedIds.Contains(Stack.OwnedWorldItemId)) return false;
   OwnedIds.Add(Stack.OwnedWorldItemId);
   FWorldOwnedItemRecord Item;
   if (!Ownership->TryGetItem(Stack.OwnedWorldItemId, Item) ||
       Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
       Item.OwnerId != InInventoryOwnerId || Item.DefinitionId != Stack.ItemId)
    return false;
  }
 }
 if (!InQuickAccessItemId.IsNone() &&
     !Snapshot.ContainsByPredicate([InQuickAccessItemId](const FInventoryStack& Stack)
      { return Stack.ItemId == InQuickAccessItemId && Stack.Quantity > 0; }))
  return false;

 InventoryOwnerId = InInventoryOwnerId;
 Items = Snapshot;
 QuickAccessItemId = InQuickAccessItemId;
 return true;
}

bool UInventoryCraftingComponent::TryStorePhysicalActor(AActor* WorldActor)
{
 if (!IsValid(WorldActor) || !GetWorld() || !InventoryOwnerId.IsValid()) return false;
 UWorldObjectIdentityComponent* Identity =
  WorldActor->FindComponentByClass<UWorldObjectIdentityComponent>();
 UWorldItemOwnershipSubsystem* Ownership =
  GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
 if (!Identity || !Identity->HasValidIdentity() || !Ownership) return false;
 const EWorldObjectLifecycleState PreviousLifecycle = Identity->GetLifecycleState();
 if (PreviousLifecycle != EWorldObjectLifecycleState::World &&
     PreviousLifecycle != EWorldObjectLifecycleState::Placed) return false;
 if (ALogBuildPiece* LogPiece = Cast<ALogBuildPiece>(WorldActor))
 {
  if (!LogPiece->CanBePickedUp()) return false;
 }

 FWorldOwnedItemRecord Item;
 if (!Ownership->TryGetItem(Identity->GetInstanceId(), Item) ||
     Item.OwnerKind != EWorldItemOwnerKind::World ||
     Item.DefinitionId.IsNone() || Item.Quantity != 1) return false;

 FInventoryStack Incoming;
 Incoming.ItemId = Item.DefinitionId;
 Incoming.Quantity = 1;
 Incoming.MaxStack = 1;
 Incoming.OwnedWorldItemId = Item.ItemId;
 if (!HasCapacityFor({Incoming})) return false;

 FWorldOwnedItemRecord Stored;
 if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
     EWorldItemOwnerKind::World, EWorldItemOwnerKind::Inventory,
     InventoryOwnerId, Item.WorldLocation, Stored)) return false;

 if (!TryAddItem(Incoming))
 {
  FWorldOwnedItemRecord RolledBack;
  if (!Ownership->TryTransferItem(FGuid::NewGuid(), Stored.ItemId, Stored.Revision,
      EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::World,
      FGuid(), WorldActor->GetActorLocation(), RolledBack))
  {
   UE_LOG(LogTemp, Error, TEXT("Inventory rollback failed for physical item %s."),
    *Item.ItemId.ToString());
  }
  return false;
 }

 if (!Identity->TryTransitionLifecycle(PreviousLifecycle, EWorldObjectLifecycleState::Destroyed))
 {
  const int32 StoredIndex = Items.IndexOfByPredicate([ItemId = Item.ItemId](const FInventoryStack& Stack)
   { return Stack.OwnedWorldItemId == ItemId; });
  if (Items.IsValidIndex(StoredIndex)) Items.RemoveAt(StoredIndex);
  FWorldOwnedItemRecord RolledBack;
  if (!Ownership->TryTransferItem(FGuid::NewGuid(), Stored.ItemId, Stored.Revision,
      EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::World,
      FGuid(), WorldActor->GetActorLocation(), RolledBack))
   UE_LOG(LogTemp, Error, TEXT("Physical item lifecycle rollback failed for item %s."), *Item.ItemId.ToString());
  return false;
 }

 WorldActor->SetActorHiddenInGame(true);
 WorldActor->SetActorEnableCollision(false);
 if (!WorldActor->Destroy())
 {
  // The item is already committed to inventory; keep its world representation
  // inert rather than creating a duplicate world-owned copy.
  UE_LOG(LogTemp, Warning, TEXT("Stored item %s is owned by inventory but its actor did not destroy."),
   *Item.ItemId.ToString());
 }
 return true;
}

bool UInventoryCraftingComponent::TryMoveOwnedItemToStorage(FGuid ItemId, FGuid StorageId)
{
 if (!ItemId.IsValid() || !StorageId.IsValid() || !GetWorld() || !InventoryOwnerId.IsValid())
  return false;
 UWorldStorageSubsystem* Storage = GetWorld()->GetSubsystem<UWorldStorageSubsystem>();
 if (!Storage || !Storage->CanAcceptItem(StorageId, ItemId)) return false;
 const int32 Index = Items.IndexOfByPredicate([ItemId](const FInventoryStack& Stack)
  { return Stack.OwnedWorldItemId == ItemId; });
 if (!Items.IsValidIndex(Index)) return false;

 UWorldItemOwnershipSubsystem* Ownership =
  GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
 if (!Ownership) return false;
 FWorldOwnedItemRecord Item;
 if (!Ownership->TryGetItem(ItemId, Item) ||
     Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
     Item.OwnerId != InventoryOwnerId || Item.Revision == MAX_int64)
  return false;

 FWorldOwnedItemRecord Stored;
 if (!Ownership->TryTransferItem(FGuid::NewGuid(), ItemId, Item.Revision,
     EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::Storage,
     StorageId, Item.WorldLocation, Stored))
  return false;

 Items.RemoveAt(Index);
 if (!QuickAccessItemId.IsNone() && GetItemCount(QuickAccessItemId) <= 0)
  QuickAccessItemId = NAME_None;
 return true;
}

bool UInventoryCraftingComponent::TryWithdrawStoredItem(FGuid ItemId, FGuid StorageId)
{
 if (!ItemId.IsValid() || !StorageId.IsValid() || !GetWorld() || !InventoryOwnerId.IsValid())
  return false;

 UWorldStorageSubsystem* Storage = GetWorld()->GetSubsystem<UWorldStorageSubsystem>();
 UWorldItemOwnershipSubsystem* Ownership = GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
 if (!Storage || !Ownership) return false;

 FWorldStorageContainerRecord StorageRecord;
 if (!Storage->GetStorage(StorageId, StorageRecord)) return false;

 FWorldOwnedItemRecord Item;
 if (!Ownership->TryGetItem(ItemId, Item) ||
     Item.OwnerKind != EWorldItemOwnerKind::Storage || Item.OwnerId != StorageId)
  return false;

 FInventoryStack Incoming;
 Incoming.ItemId = Item.DefinitionId;
 Incoming.Quantity = 1;
 Incoming.MaxStack = 1;
 Incoming.OwnedWorldItemId = ItemId;
 if (!HasCapacityFor({Incoming})) return false;

 FWorldOwnedItemRecord Transferred;
 if (!Ownership->TryTransferItem(FGuid::NewGuid(), ItemId, Item.Revision,
     EWorldItemOwnerKind::Storage, EWorldItemOwnerKind::Inventory,
     InventoryOwnerId, Item.WorldLocation, Transferred))
  return false;

 if (!TryAddItem(Incoming))
 {
  FWorldOwnedItemRecord RolledBack;
  if (!Ownership->TryTransferItem(FGuid::NewGuid(), ItemId, Transferred.Revision,
      EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::Storage,
      StorageId, Item.WorldLocation, RolledBack))
   UE_LOG(LogTemp, Error, TEXT("Storage withdrawal rollback failed for item %s."), *ItemId.ToString());
  return false;
 }
 return true;
}

bool UInventoryCraftingComponent::TryRemoveItem(FName ItemId, int32 Quantity)
{
 if (ItemId.IsNone() || Quantity <= 0 || GetItemCount(ItemId) < Quantity) return false;

 // Validate every unique physical item first; generic resource stacks remain
 // quantity-based, while physical items must transition out of inventory ownership.
 int32 Remaining = Quantity;
 TArray<FGuid> OwnedToConsume;
 for (int32 Index = Items.Num() - 1; Index >= 0 && Remaining > 0; --Index)
 {
  const FInventoryStack& Stack = Items[Index];
  if (Stack.ItemId != ItemId) continue;
  const int32 Taken = FMath::Min(Remaining, Stack.Quantity);
  if (Stack.OwnedWorldItemId.IsValid())
  {
   if (Taken != 1 || Stack.Quantity != 1 || !GetWorld() || !InventoryOwnerId.IsValid())
    return false;
   OwnedToConsume.Add(Stack.OwnedWorldItemId);
  }
  Remaining -= Taken;
 }
 if (Remaining != 0) return false;

 UWorldItemOwnershipSubsystem* Ownership =
  GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
 if (!OwnedToConsume.IsEmpty() && !Ownership) return false;

 TArray<FWorldOwnedItemRecord> ConsumedRecords;
 for (const FGuid& OwnedId : OwnedToConsume)
 {
  FWorldOwnedItemRecord Item;
  if (!Ownership->TryGetItem(OwnedId, Item) ||
      Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
      Item.OwnerId != InventoryOwnerId || Item.Revision == MAX_int64)
  {
   for (int32 RollbackIndex = ConsumedRecords.Num() - 1; RollbackIndex >= 0; --RollbackIndex)
   {
    const FWorldOwnedItemRecord& Committed = ConsumedRecords[RollbackIndex];
    FWorldOwnedItemRecord RolledBack;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), Committed.ItemId, Committed.Revision,
        EWorldItemOwnerKind::Consumed, EWorldItemOwnerKind::Inventory,
        InventoryOwnerId, Committed.WorldLocation, RolledBack))
     UE_LOG(LogTemp, Error, TEXT("Inventory consume rollback failed for item %s."),
      *Committed.ItemId.ToString());
   }
   return false;
  }

  FWorldOwnedItemRecord Committed;
  if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
      EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::Consumed,
      FGuid(), Item.WorldLocation, Committed))
  {
   for (int32 RollbackIndex = ConsumedRecords.Num() - 1; RollbackIndex >= 0; --RollbackIndex)
   {
    const FWorldOwnedItemRecord& Previous = ConsumedRecords[RollbackIndex];
    FWorldOwnedItemRecord RolledBack;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), Previous.ItemId, Previous.Revision,
        EWorldItemOwnerKind::Consumed, EWorldItemOwnerKind::Inventory,
        InventoryOwnerId, Previous.WorldLocation, RolledBack))
     UE_LOG(LogTemp, Error, TEXT("Inventory consume rollback failed for item %s."),
      *Previous.ItemId.ToString());
   }
   return false;
  }
  ConsumedRecords.Add(Committed);
 }

 Remaining = Quantity;
 for (int32 Index = Items.Num() - 1; Index >= 0 && Remaining > 0; --Index)
 {
  if (Items[Index].ItemId != ItemId) continue;
  const int32 Taken = FMath::Min(Remaining, Items[Index].Quantity);
  Items[Index].Quantity -= Taken;
  Remaining -= Taken;
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
 // Snapshot allows rollback if an unexpected mutation failure occurs. Physical
 // ingredients consumed by TryRemoveItem must be restored in the ownership ledger too.
 const TArray<FInventoryStack> BeforeItems = Items;
 const FName BeforeQuickAccess = QuickAccessItemId;
 auto RollbackCraft = [&]()
 {
  Items = BeforeItems;
  QuickAccessItemId = BeforeQuickAccess;
  UWorldItemOwnershipSubsystem* Ownership =
   GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
  if (!Ownership) return;
  for (const FInventoryStack& Stack : BeforeItems)
  {
   if (!Stack.OwnedWorldItemId.IsValid()) continue;
   FWorldOwnedItemRecord Item;
   if (!Ownership->TryGetItem(Stack.OwnedWorldItemId, Item) ||
       Item.OwnerKind != EWorldItemOwnerKind::Consumed || Item.Revision == MAX_int64)
    continue;
   FWorldOwnedItemRecord Restored;
   if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
       EWorldItemOwnerKind::Consumed, EWorldItemOwnerKind::Inventory,
       InventoryOwnerId, Item.WorldLocation, Restored))
    UE_LOG(LogTemp, Error, TEXT("Craft rollback could not restore physical item %s."),
     *Item.ItemId.ToString());
  }
 };
 for (const TPair<FName,int32>& P : Required)
 {
  if (!TryRemoveItem(P.Key, P.Value)) { RollbackCraft(); return false; }
 }
 if (!TryAddItem(Recipe.Output)) { RollbackCraft(); return false; }
 for (const FInventoryStack& Byproduct : Recipe.Byproducts)
 {
  if (!TryAddItem(Byproduct)) { RollbackCraft(); return false; }
 }
 return true;
}
