#include "InventoryCraftingComponent.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStorageSubsystem.h"
#include "LogBuildPiece.h"
#include "StoneBuildingPiece.h"
#include "StickPrimitivePiece.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "HeatSourceComponent.h"

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

const FInventoryItemDefinition* UInventoryCraftingComponent::FindItemDefinition(FName ItemId) const
{
 if (ItemId.IsNone()) return nullptr;
 const FInventoryItemDefinition* Found = nullptr;
 for (const FInventoryItemDefinition& Definition : ItemDefinitions)
 {
  if (Definition.ItemId != ItemId) continue;
  if (Found) return nullptr;
  Found = &Definition;
 }
 return Found;
}

int32 UInventoryCraftingComponent::GetConfiguredWeightGrams(FName ItemId) const
{
 if (const int32* Configured = ItemWeightGrams.Find(ItemId))
  return *Configured > 0 ? *Configured : 0;
 const FInventoryItemDefinition* Definition = FindItemDefinition(ItemId);
 return Definition && Definition->UnitWeightGrams > 0 ? Definition->UnitWeightGrams : 0;
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
  const int32 Weight = GetConfiguredWeightGrams(Stack.ItemId);
  // Unknown/invalid weight must never make an item silently weightless.
  if (Weight <= 0 || Stack.Quantity < 0) return MAX_int32;
  Total += static_cast<int64>(Weight) * Stack.Quantity;
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
      !FMath::IsFinite(Addition.AgeSeconds) || Addition.AgeSeconds < 0.0 ||
      (!Addition.EquippedSlotId.IsNone() &&
       (Addition.Quantity != 1 || Addition.MaxStack != 1 || Addition.OwnedWorldItemId.IsValid() || Addition.bSpoiled)) ||
      (Addition.OwnedWorldItemId.IsValid() && (Addition.Quantity != 1 || Addition.MaxStack != 1 ||
       Simulated.ContainsByPredicate([&Addition](const FInventoryStack& S) { return S.OwnedWorldItemId == Addition.OwnedWorldItemId; })))) return false;
  int32 Remaining = Addition.Quantity;
  for (FInventoryStack& Existing : Simulated)
  {
   if (Existing.ItemId == Addition.ItemId && !Existing.OwnedWorldItemId.IsValid() &&
       !Addition.OwnedWorldItemId.IsValid() && Existing.EquippedSlotId.IsNone() &&
       Addition.EquippedSlotId.IsNone() && Existing.bSpoiled == Addition.bSpoiled &&
       FMath::IsNearlyEqual(Existing.AgeSeconds, Addition.AgeSeconds, 1.0e-6) &&
       Existing.Quantity < Existing.MaxStack)
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
  const int32 ItemWeight = GetConfiguredWeightGrams(Stack.ItemId);
  // Fail closed until every item has a positive, configured weight.
  if (ItemWeight <= 0 || Stack.Quantity <= 0) return false;
  Weight += static_cast<int64>(ItemWeight) * Stack.Quantity;
  if (Weight > MaxCarryWeightGrams) return false;
 }
 return true;
}

bool UInventoryCraftingComponent::TryAddItem(const FInventoryStack& Incoming)
{
 if (Incoming.ItemId.IsNone() || Incoming.Quantity <= 0 || Incoming.MaxStack <= 0 ||
     !FMath::IsFinite(Incoming.AgeSeconds) || Incoming.AgeSeconds < 0.0 ||
     (!Incoming.EquippedSlotId.IsNone() &&
      (Incoming.Quantity != 1 || Incoming.MaxStack != 1 || Incoming.OwnedWorldItemId.IsValid() || Incoming.bSpoiled)) ||
     (Incoming.OwnedWorldItemId.IsValid() && (Incoming.Quantity != 1 || Incoming.MaxStack != 1 ||
      Items.ContainsByPredicate([&Incoming](const FInventoryStack& S) { return S.OwnedWorldItemId == Incoming.OwnedWorldItemId; }))) ||
     !HasCapacityFor({Incoming})) return false;
 int32 Remaining = Incoming.Quantity;
 for (FInventoryStack& Existing : Items)
 {
  if (Existing.ItemId == Incoming.ItemId && !Existing.OwnedWorldItemId.IsValid() &&
      !Incoming.OwnedWorldItemId.IsValid() && Existing.EquippedSlotId.IsNone() &&
      Incoming.EquippedSlotId.IsNone() && Existing.bSpoiled == Incoming.bSpoiled &&
      FMath::IsNearlyEqual(Existing.AgeSeconds, Incoming.AgeSeconds, 1.0e-6) &&
      Existing.Quantity < Existing.MaxStack)
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
 TSet<FName> EquippedSlots;
 int64 TotalWeight = 0;
 for (const FInventoryStack& Stack : Snapshot)
 {
  if (Stack.ItemId.IsNone() || Stack.Quantity <= 0 || Stack.MaxStack <= 0 ||
      Stack.Quantity > Stack.MaxStack || !FMath::IsFinite(Stack.AgeSeconds) ||
      Stack.AgeSeconds < 0.0 ||
      (!Stack.EquippedSlotId.IsNone() &&
       (Stack.Quantity != 1 || Stack.MaxStack != 1 || Stack.OwnedWorldItemId.IsValid() ||
        Stack.bSpoiled || EquippedSlots.Contains(Stack.EquippedSlotId))))
   return false;
  if (!Stack.EquippedSlotId.IsNone())
  {
   const FInventoryItemDefinition* Def = FindItemDefinition(Stack.ItemId);
   if (!Def || Def->EquipmentSlotId != Stack.EquippedSlotId) return false;
   EquippedSlots.Add(Stack.EquippedSlotId);
  }
  const int32 Weight = GetConfiguredWeightGrams(Stack.ItemId);
  if (Weight <= 0) return false;
  TotalWeight += static_cast<int64>(Weight) * Stack.Quantity;
  if (TotalWeight > MaxCarryWeightGrams) return false;
  if (Stack.OwnedWorldItemId.IsValid())
  {
   if (!Stack.EquippedSlotId.IsNone() || Stack.bSpoiled ||
       !Ownership || Stack.Quantity != 1 || Stack.MaxStack != 1 ||
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
  if (LogPiece->HasCustomLengthScale())
  {
   UE_LOG(LogTemp, Warning, TEXT("Inventory rejected scaled log %s because it has no canonical length definition."),
    *GetNameSafe(LogPiece));
   return false;
  }
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

 // Stone/stick domains own their placement constraints. Their held-state
 // preflight rejects placed pieces, connected supports and pieces with children.
 bool bTemporarilyHeldStone = false;
 bool bTemporarilyHeldStick = false;
 if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(WorldActor))
 {
  if (!Stone->SetHeldState(true)) return false;
  bTemporarilyHeldStone = true;
 }
 if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(WorldActor))
 {
  if (!Stick->SetHeldState(true))
  {
   if (bTemporarilyHeldStone)
    if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(WorldActor)) Stone->SetHeldState(false);
   return false;
  }
  bTemporarilyHeldStick = true;
 }

 const FVector CurrentWorldLocation = WorldActor->GetActorLocation();
 FWorldOwnedItemRecord Stored;
 if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
     EWorldItemOwnerKind::World, EWorldItemOwnerKind::Inventory,
     InventoryOwnerId, CurrentWorldLocation, Stored))
 {
  if (bTemporarilyHeldStone)
   if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(WorldActor)) Stone->SetHeldState(false);
  if (bTemporarilyHeldStick)
   if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(WorldActor)) Stick->SetHeldState(false);
  return false;
 }

 if (!TryAddItem(Incoming))
 {
  FWorldOwnedItemRecord RolledBack;
  const bool bLedgerRollback = Ownership->TryTransferItem(FGuid::NewGuid(), Stored.ItemId, Stored.Revision,
      EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::World,
      FGuid(), CurrentWorldLocation, RolledBack);
  FWorldOwnedItemRecord Actual;
  const bool bWorldOwnedAgain = Ownership->TryGetItem(Item.ItemId, Actual) &&
      Actual.OwnerKind == EWorldItemOwnerKind::World && !Actual.OwnerId.IsValid();
  if (!bWorldOwnedAgain)
  {
   WorldActor->SetActorHiddenInGame(true);
   WorldActor->SetActorEnableCollision(false);
   UE_LOG(LogTemp, Error, TEXT("Inventory rollback left physical item %s outside its world owner."),
    *Item.ItemId.ToString());
   if (UWorldSimulationCoordinatorSubsystem* Coordinator =
       GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
    Coordinator->MarkSimulationFaulted(TEXT("Physical-item pickup failed and ownership could not be restored to World; stop simulation and recover/reset the world."));
  }
  if (bWorldOwnedAgain || bLedgerRollback)
  {
   if (bTemporarilyHeldStone)
    if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(WorldActor)) Stone->SetHeldState(false);
   if (bTemporarilyHeldStick)
    if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(WorldActor)) Stick->SetHeldState(false);
  }
  return false;
 }

 if (!Identity->TryTransitionLifecycle(PreviousLifecycle, EWorldObjectLifecycleState::Destroyed))
 {
  const int32 StoredIndex = Items.IndexOfByPredicate([ItemId = Item.ItemId](const FInventoryStack& Stack)
   { return Stack.OwnedWorldItemId == ItemId; });
  if (Items.IsValidIndex(StoredIndex)) Items.RemoveAt(StoredIndex);
  FWorldOwnedItemRecord RolledBack;
  const bool bLedgerRollback = Ownership->TryTransferItem(FGuid::NewGuid(), Stored.ItemId, Stored.Revision,
      EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::World,
      FGuid(), CurrentWorldLocation, RolledBack);
  FWorldOwnedItemRecord Actual;
  const bool bWorldOwnedAgain = Ownership->TryGetItem(Item.ItemId, Actual) &&
      Actual.OwnerKind == EWorldItemOwnerKind::World && !Actual.OwnerId.IsValid();
  const bool bStillInventoryOwned = Ownership->TryGetItem(Item.ItemId, Actual) &&
      Actual.OwnerKind == EWorldItemOwnerKind::Inventory && Actual.OwnerId == InventoryOwnerId;
  if (!bWorldOwnedAgain && !bStillInventoryOwned)
  {
   WorldActor->SetActorHiddenInGame(true);
   WorldActor->SetActorEnableCollision(false);
   UE_LOG(LogTemp, Error, TEXT("Lifecycle rollback left physical item %s in an unexpected owner state."),
    *Item.ItemId.ToString());
   if (UWorldSimulationCoordinatorSubsystem* Coordinator =
       GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
    Coordinator->MarkSimulationFaulted(TEXT("Physical-item lifecycle rollback could not reconcile the world and inventory owner; stop simulation and recover/reset the world."));
  }
  if (bStillInventoryOwned)
  {
   // Restore the logical inventory representation if the ledger never rolled back.
   if (!Items.ContainsByPredicate([ItemId = Item.ItemId](const FInventoryStack& Stack)
       { return Stack.OwnedWorldItemId == ItemId; }) && !TryAddItem(Incoming))
   {
    UE_LOG(LogTemp, Error, TEXT("Could not restore inventory row for physical item %s after lifecycle rollback."),
     *Item.ItemId.ToString());
    if (UWorldSimulationCoordinatorSubsystem* Coordinator =
        GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
     Coordinator->MarkSimulationFaulted(TEXT("Physical-item lifecycle rollback retained Inventory ownership but could not restore its inventory row; stop simulation and recover/reset the world."));
   }
   WorldActor->SetActorHiddenInGame(true);
   WorldActor->SetActorEnableCollision(false);
  }
  else if (bWorldOwnedAgain || bLedgerRollback)
  {
   if (bTemporarilyHeldStone)
    if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(WorldActor)) Stone->SetHeldState(false);
   if (bTemporarilyHeldStick)
    if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(WorldActor)) Stick->SetHeldState(false);
  }
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
 if (!Storage || !Storage->IsStorageAvailable(StorageId) ||
     !Storage->CanAcceptItem(StorageId, ItemId)) return false;
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
 if (!Storage->GetStorage(StorageId, StorageRecord) ||
     !Storage->IsStorageAvailable(StorageId)) return false;

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
  const bool bRollbackSucceeded = Ownership->TryTransferItem(FGuid::NewGuid(), ItemId, Transferred.Revision,
      EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::Storage,
      StorageId, Item.WorldLocation, RolledBack);

  if (!bRollbackSucceeded)
  {
   // Do not assume a failed compensation left the ledger unchanged. Inspect the
   // authoritative record before deciding whether the world can keep running.
   FWorldOwnedItemRecord ActualItem;
   const bool bRecordExists = Ownership->TryGetItem(ItemId, ActualItem);
   const bool bBackInStorage = bRecordExists &&
       ActualItem.OwnerKind == EWorldItemOwnerKind::Storage &&
       ActualItem.OwnerId == StorageId;
   if (!bBackInStorage)
   {
    UE_LOG(LogTemp, Error, TEXT("Storage withdrawal rollback left item %s in an inconsistent owner state."),
     *ItemId.ToString());
    if (UWorldSimulationCoordinatorSubsystem* Coordinator =
        GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
    {
     Coordinator->MarkSimulationFaulted(
      TEXT("Storage withdrawal rollback failed to reconcile the inventory and storage owner; stop simulation and recover/reset the world."));
    }
   }
  }
  return false;
 }
 return true;
}

bool UInventoryCraftingComponent::TryDropOwnedItem(
 FGuid ItemId, FVector SpawnLocation, AActor*& OutActor)
{
 OutActor = nullptr;
 if (!ItemId.IsValid() || !GetWorld() || !InventoryOwnerId.IsValid() ||
     !FMath::IsFinite(SpawnLocation.X) || !FMath::IsFinite(SpawnLocation.Y) ||
     !FMath::IsFinite(SpawnLocation.Z))
  return false;

 const int32 StackIndex = Items.IndexOfByPredicate([ItemId](const FInventoryStack& Stack)
  { return Stack.OwnedWorldItemId == ItemId; });
 if (!Items.IsValidIndex(StackIndex)) return false;

 UWorldItemOwnershipSubsystem* Ownership =
  GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
 if (!Ownership) return false;

 FWorldOwnedItemRecord Item;
 if (!Ownership->TryGetItem(ItemId, Item) ||
     Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
     Item.OwnerId != InventoryOwnerId || Item.Revision == MAX_int64)
  return false;

 const TSubclassOf<AActor>* ActorClass = PhysicalItemActorClasses.Find(Item.DefinitionId);
 bool bApplyLogVariantFromBaseClass = false;
 if ((!ActorClass || !*ActorClass) &&
     !Item.BaseDefinitionId.IsNone() && Item.BaseDefinitionId != Item.DefinitionId)
 {
  const TSubclassOf<AActor>* BaseActorClass = PhysicalItemActorClasses.Find(Item.BaseDefinitionId);
  if (BaseActorClass && *BaseActorClass &&
      (*BaseActorClass)->IsChildOf(ALogBuildPiece::StaticClass()))
  {
   ActorClass = BaseActorClass;
   bApplyLogVariantFromBaseClass = true;
  }
 }
 if (!ActorClass || !*ActorClass) return false;

 const UClass* RequestedClass = *ActorClass;
 const bool bStructuralActor =
  RequestedClass->IsChildOf(ALogBuildPiece::StaticClass()) ||
  RequestedClass->IsChildOf(AStoneBuildingPiece::StaticClass()) ||
  RequestedClass->IsChildOf(AStickPrimitivePiece::StaticClass());
 if (bStructuralActor)
 {
  UWorldStructuralGraphSubsystem* Graph =
   GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
  FStructuralNodeRecord ExistingNode;
  // The original actor should have unregistered when it entered inventory.
  if (!Graph || Graph->GetNode(ItemId, ExistingNode))
   return false;
 }

 const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
 AActor* Spawned = GetWorld()->SpawnActorDeferred<AActor>(
  *ActorClass, SpawnTransform, GetOwner(), nullptr,
  ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if (!IsValid(Spawned)) return false;

 // Hide the actor before FinishSpawning runs BeginPlay and Blueprint construction.
 Spawned->SetActorHiddenInGame(true);
 Spawned->SetActorEnableCollision(false);

 UWorldObjectIdentityComponent* Identity =
  Spawned->FindComponentByClass<UWorldObjectIdentityComponent>();
 if (!Identity)
 {
  Spawned->Destroy();
  return false;
 }

 // Reuse the ledger's ID before BeginPlay so the actor cannot register as a
 // second, unrelated item while the inventory still owns the unique item.
 Identity->InstanceId = Item.ItemId;
 Identity->SetDefinitionId(Item.DefinitionId);
 if (ALogBuildPiece* LogPiece = Cast<ALogBuildPiece>(Spawned))
 {
  LogPiece->bStartsPlaced = false;
  LogPiece->BaseDefinitionId = Item.BaseDefinitionId.IsNone()
   ? Item.DefinitionId : Item.BaseDefinitionId;
  LogPiece->SelectedProcessingMeshSetId = Item.MeshVariantSetId;
 }
 if (AStoneBuildingPiece* StonePiece = Cast<AStoneBuildingPiece>(Spawned))
  StonePiece->DefinitionId = Item.DefinitionId;
 if (AStickPrimitivePiece* StickPiece = Cast<AStickPrimitivePiece>(Spawned))
  StickPiece->DefinitionId = Item.DefinitionId;

 UGameplayStatics::FinishSpawningActor(Spawned, SpawnTransform);
 if (!IsValid(Spawned))
  return false;

 // Construction scripts may initialize Blueprint defaults; reapply the canonical
 // ledger identity and variant metadata after construction has completed.
 Identity->InstanceId = Item.ItemId;
 Identity->SetDefinitionId(Item.DefinitionId);
 if (ALogBuildPiece* LogPiece = Cast<ALogBuildPiece>(Spawned))
 {
  LogPiece->bStartsPlaced = false;
  LogPiece->BaseDefinitionId = Item.BaseDefinitionId.IsNone()
   ? Item.DefinitionId : Item.BaseDefinitionId;
  LogPiece->SelectedProcessingMeshSetId = Item.MeshVariantSetId;
 }
 if (AStoneBuildingPiece* StonePiece = Cast<AStoneBuildingPiece>(Spawned))
  StonePiece->DefinitionId = Item.DefinitionId;
 if (AStickPrimitivePiece* StickPiece = Cast<AStickPrimitivePiece>(Spawned))
  StickPiece->DefinitionId = Item.DefinitionId;

 if (bApplyLogVariantFromBaseClass)
 {
  ALogBuildPiece* LogPiece = Cast<ALogBuildPiece>(Spawned);
  if (!LogPiece || !LogPiece->ApplyDefinitionVariant(Item.DefinitionId))
  {
   UE_LOG(LogTemp, Error, TEXT("No configured physical mesh variant for item definition %s."),
    *Item.DefinitionId.ToString());
   Spawned->Destroy();
   return false;
  }
 }

 Spawned->SetActorHiddenInGame(true);
 Spawned->SetActorEnableCollision(false);

 FWorldOwnedItemRecord WorldItem;
 if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
     EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::World,
     FGuid(), SpawnLocation, WorldItem))
 {
  Spawned->Destroy();
  return false;
 }

 bool bActivated = true;
 if (ALogBuildPiece* LogPiece = Cast<ALogBuildPiece>(Spawned))
 {
  bActivated = LogPiece->ActivateAfterOwnershipTransfer();
 }
 else if (AStoneBuildingPiece* StonePiece = Cast<AStoneBuildingPiece>(Spawned))
 {
  bActivated = StonePiece->ActivateAfterOwnershipTransfer();
 }
 else if (AStickPrimitivePiece* StickPiece = Cast<AStickPrimitivePiece>(Spawned))
 {
  bActivated = StickPiece->ActivateAfterOwnershipTransfer();
 }
 else
 {
  // Generic physical-item classes are only supported if their actor/identity
  // component is self-contained; domain actors must use their typed activation.
  Spawned->SetActorHiddenInGame(false);
  Spawned->SetActorEnableCollision(true);
 }

 if (!bActivated)
 {
  FWorldOwnedItemRecord RolledBack;
  const bool bRollbackTransferSucceeded = Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId,
      WorldItem.Revision, EWorldItemOwnerKind::World, EWorldItemOwnerKind::Inventory,
      InventoryOwnerId, Item.WorldLocation, RolledBack);

  FWorldOwnedItemRecord ActualItem;
  const bool bCurrentItemExists = Ownership->TryGetItem(Item.ItemId, ActualItem);
  const bool bSafelyInventoryOwned = bCurrentItemExists &&
      ActualItem.OwnerKind == EWorldItemOwnerKind::Inventory &&
      ActualItem.OwnerId == InventoryOwnerId;

  Spawned->SetActorHiddenInGame(true);
  Spawned->SetActorEnableCollision(false);
  if (bSafelyInventoryOwned)
  {
   // The ledger is authoritative: keep the inventory row and destroy this
   // failed world representation, whether compensation returned true or false.
   Spawned->Destroy();
  }
  else
  {
   // There is no verified inventory owner. Never leave the stale inventory row
   // alongside a world-owned, missing, or unexpectedly owned ledger record.
   const int32 StaleIndex = Items.IndexOfByPredicate([ItemId](const FInventoryStack& S)
    { return S.OwnedWorldItemId == ItemId; });
   if (Items.IsValidIndex(StaleIndex)) Items.RemoveAt(StaleIndex);

   if (bCurrentItemExists && ActualItem.OwnerKind == EWorldItemOwnerKind::World)
   {
    // Preserve the one world-owned representation, but quarantine it hidden.
    UE_LOG(LogTemp, Error, TEXT("Physical item %s remains world-owned but activation failed; actor quarantined hidden."),
     *Item.ItemId.ToString());
   }
   else
   {
    // The ledger no longer confirms this actor as world-owned; it must not
    // remain as a visible or collidable representation.
    Spawned->Destroy();
   }

   if (UWorldSimulationCoordinatorSubsystem* Coordinator =
       GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
   {
    Coordinator->MarkSimulationFaulted(
     TEXT("Physical-item drop activation failed and ownership could not be restored to the inventory; stop simulation and recover/reset the world."));
   }
   else
   {
    UE_LOG(LogTemp, Error, TEXT("Drop activation failure left item %s without verified inventory ownership; simulation coordinator unavailable."),
     *Item.ItemId.ToString());
   }
  }

  UE_LOG(LogTemp, Error, TEXT("Dropped item activation failed for %s (rollback-transfer=%d, inventory-owned=%d)."),
      *Item.ItemId.ToString(), bRollbackTransferSucceeded ? 1 : 0, bSafelyInventoryOwned ? 1 : 0);
  return false;
 }

 Items.RemoveAt(StackIndex);
 if (!QuickAccessItemId.IsNone() && GetItemCount(QuickAccessItemId) <= 0)
  QuickAccessItemId = NAME_None;

 OutActor = Spawned;
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
  if (!Stack.EquippedSlotId.IsNone()) return false;
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
    {
     FWorldOwnedItemRecord Actual;
     const bool bAlreadyRestored = Ownership->TryGetItem(Committed.ItemId, Actual) &&
         Actual.OwnerKind == EWorldItemOwnerKind::Inventory && Actual.OwnerId == InventoryOwnerId;
     if (!bAlreadyRestored)
     {
      UE_LOG(LogTemp, Error, TEXT("Inventory consume rollback left item %s outside its inventory owner."),
       *Committed.ItemId.ToString());
      if (UWorldSimulationCoordinatorSubsystem* Coordinator =
          GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
       Coordinator->MarkSimulationFaulted(TEXT("Craft ingredient rollback failed to restore inventory ownership; stop simulation and recover/reset the world."));
     }
    }
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
    {
     FWorldOwnedItemRecord Actual;
     const bool bAlreadyRestored = Ownership->TryGetItem(Previous.ItemId, Actual) &&
         Actual.OwnerKind == EWorldItemOwnerKind::Inventory && Actual.OwnerId == InventoryOwnerId;
     if (!bAlreadyRestored)
     {
      UE_LOG(LogTemp, Error, TEXT("Inventory consume rollback left item %s outside its inventory owner."),
       *Previous.ItemId.ToString());
      if (UWorldSimulationCoordinatorSubsystem* Coordinator =
          GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
       Coordinator->MarkSimulationFaulted(TEXT("Craft ingredient rollback failed to restore inventory ownership; stop simulation and recover/reset the world."));
     }
    }
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

bool UInventoryCraftingComponent::BuildRemovalPreview(
 FName ItemId, int32 Quantity, TArray<FInventoryStack>& OutItems) const
{
 OutItems.Reset();
 if (ItemId.IsNone() || Quantity <= 0 || GetItemCount(ItemId) < Quantity) return false;
 OutItems = Items;
 int32 Remaining = Quantity;
 while (Remaining > 0)
 {
  int32 BestIndex = INDEX_NONE;
  double OldestAge = -1.0;
  for (int32 Index = 0; Index < OutItems.Num(); ++Index)
  {
   const FInventoryStack& Stack = OutItems[Index];
   if (Stack.ItemId != ItemId || Stack.OwnedWorldItemId.IsValid() ||
       !Stack.EquippedSlotId.IsNone() || Stack.bSpoiled) continue;
   if (BestIndex == INDEX_NONE || Stack.AgeSeconds > OldestAge)
   { BestIndex = Index; OldestAge = Stack.AgeSeconds; }
  }
  if (!OutItems.IsValidIndex(BestIndex)) { OutItems.Reset(); return false; }
  FInventoryStack& Stack = OutItems[BestIndex];
  const int32 Taken = FMath::Min(Remaining, Stack.Quantity);
  Stack.Quantity -= Taken;
  Remaining -= Taken;
  if (Stack.Quantity <= 0) OutItems.RemoveAt(BestIndex);
 }
 return true;
}

bool UInventoryCraftingComponent::TryConsumeFood(
 FName ItemId, FGuid SurvivalEntityId, int32 Quantity)
{
 if (!GetWorld() || ItemId.IsNone() || !SurvivalEntityId.IsValid() ||
     Quantity <= 0 || Quantity > 10000) return false;
 const FInventoryItemDefinition* Def = FindItemDefinition(ItemId);
 if (!Def || !Def->bConsumable || Def->Category != EInventoryItemCategory::Consumable ||
     !FMath::IsFinite(Def->CaloriesKcal) || Def->CaloriesKcal < 0.0f ||
     !FMath::IsFinite(Def->HydrationRelief) || Def->HydrationRelief < 0.0f ||
     (Def->CaloriesKcal <= 0.0f && Def->HydrationRelief <= 0.0f)) return false;
 const double Kcal = static_cast<double>(Def->CaloriesKcal) * Quantity;
 const double Hydration = static_cast<double>(Def->HydrationRelief) * Quantity;
 if (!FMath::IsFinite(Kcal) || Kcal > 10000.0 ||
     !FMath::IsFinite(Hydration) || Hydration > 100.0) return false;
 UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
 FWorldSurvivalProfile Profile;
 if (!Survival || !Survival->GetProfile(SurvivalEntityId, Profile) ||
     Profile.Revision == MAX_int64) return false;
 TArray<FInventoryStack> ProposedItems;
 if (!BuildRemovalPreview(ItemId, Quantity, ProposedItems)) return false;
 if (!Survival->ConsumeFood(SurvivalEntityId, static_cast<float>(Kcal),
     static_cast<float>(Hydration))) return false;
 Items = MoveTemp(ProposedItems);
 if (QuickAccessItemId == ItemId && GetItemCount(ItemId) <= 0) QuickAccessItemId = NAME_None;
 return true;
}

bool UInventoryCraftingComponent::TryFeedAnimal(FName ItemId, FGuid AnimalId, int32 Quantity)
{
 if (!GetWorld() || ItemId.IsNone() || !AnimalId.IsValid() ||
     Quantity <= 0 || Quantity > 10000) return false;
 const FInventoryItemDefinition* Def = FindItemDefinition(ItemId);
 if (!Def || !FMath::IsFinite(Def->AnimalFeedHungerReduction) ||
     Def->AnimalFeedHungerReduction <= 0.0f) return false;
 const double Relief = static_cast<double>(Def->AnimalFeedHungerReduction) * Quantity;
 if (!FMath::IsFinite(Relief) || Relief <= 0.0 || Relief > 100.0) return false;
 UWorldAgricultureSubsystem* Agriculture = GetWorld()->GetSubsystem<UWorldAgricultureSubsystem>();
 FWorldAnimalState Animal;
 if (!Agriculture || !Agriculture->GetAnimal(AnimalId, Animal) ||
     Animal.Revision == MAX_int64 || Animal.Health <= 0.0f || Animal.Hunger <= 0.0f) return false;
 TArray<FInventoryStack> ProposedItems;
 if (!BuildRemovalPreview(ItemId, Quantity, ProposedItems)) return false;
 if (!Agriculture->FeedAnimal(AnimalId, static_cast<float>(Relief), Animal.Revision)) return false;
 Items = MoveTemp(ProposedItems);
 if (QuickAccessItemId == ItemId && GetItemCount(ItemId) <= 0) QuickAccessItemId = NAME_None;
 return true;
}

bool UInventoryCraftingComponent::CalculateClothingInsulation(
 const TArray<FInventoryStack>& SourceItems, float& OutInsulation) const
{
 OutInsulation = 0.0f;
 double Remaining = 1.0;
 TSet<FName> SeenSlots;
 for (const FInventoryStack& Stack : SourceItems)
 {
  if (Stack.EquippedSlotId.IsNone()) continue;
  if (Stack.Quantity != 1 || Stack.MaxStack != 1 || Stack.OwnedWorldItemId.IsValid() ||
      Stack.bSpoiled || SeenSlots.Contains(Stack.EquippedSlotId)) return false;
  const FInventoryItemDefinition* Def = FindItemDefinition(Stack.ItemId);
  if (!Def || Def->EquipmentSlotId != Stack.EquippedSlotId ||
      !FMath::IsFinite(Def->ClothingInsulation) ||
      Def->ClothingInsulation < 0.0f || Def->ClothingInsulation > 1.0f) return false;
  SeenSlots.Add(Stack.EquippedSlotId);
  Remaining *= 1.0 - static_cast<double>(Def->ClothingInsulation);
 }
 const double Combined = 1.0 - Remaining;
 if (!FMath::IsFinite(Combined) || Combined < 0.0 || Combined > 1.0) return false;
 OutInsulation = static_cast<float>(Combined);
 return true;
}

float UInventoryCraftingComponent::GetEquippedClothingInsulation() const
{
 float Value = 0.0f;
 return CalculateClothingInsulation(Items, Value) ? Value : 0.0f;
}

bool UInventoryCraftingComponent::TryFuelHeatSource(
 FName ItemId, UHeatSourceComponent* HeatSource, int32 Quantity)
{
 if (!GetWorld() || !IsValid(HeatSource) || HeatSource->GetWorld() != GetWorld() ||
     ItemId.IsNone() || Quantity <= 0 || Quantity > 10000)
  return false;
 const FInventoryItemDefinition* Def = FindItemDefinition(ItemId);
 if (!Def || !FMath::IsFinite(Def->FuelBurnSeconds) ||
     Def->FuelBurnSeconds <= 0.0f || Def->FuelBurnSeconds > 86400.0f)
  return false;
 const double BurnSeconds = static_cast<double>(Def->FuelBurnSeconds) * Quantity;
 const double ProjectedFuel = static_cast<double>(HeatSource->GetFuelSeconds()) + BurnSeconds;
 if (!FMath::IsFinite(BurnSeconds) || BurnSeconds <= 0.0 ||
     !FMath::IsFinite(ProjectedFuel) || ProjectedFuel > HeatSource->MaxFuelSeconds)
  return false;
 TArray<FInventoryStack> ProposedItems;
 if (!BuildRemovalPreview(ItemId, Quantity, ProposedItems)) return false;
 if (!HeatSource->AddFuel(static_cast<float>(BurnSeconds))) return false;
 Items = MoveTemp(ProposedItems);
 if (QuickAccessItemId == ItemId && GetItemCount(ItemId) <= 0)
  QuickAccessItemId = NAME_None;
 return true;
}

bool UInventoryCraftingComponent::TryEquipItem(FName ItemId)
{
 if (ItemId.IsNone() || !GetWorld() || !IsValid(GetOwner())) return false;
 const FInventoryItemDefinition* Def = FindItemDefinition(ItemId);
 if (!Def || Def->EquipmentSlotId.IsNone() ||
     !FMath::IsFinite(Def->ClothingInsulation) ||
     Def->ClothingInsulation < 0.0f || Def->ClothingInsulation > 1.0f) return false;
 const int32 StackIndex = Items.IndexOfByPredicate([ItemId](const FInventoryStack& Stack)
 {
  return Stack.ItemId == ItemId && Stack.Quantity == 1 && Stack.MaxStack == 1 &&
      !Stack.OwnedWorldItemId.IsValid() && !Stack.bSpoiled;
 });
 if (!Items.IsValidIndex(StackIndex)) return false;
 if (Items[StackIndex].EquippedSlotId == Def->EquipmentSlotId) return true;
 UWorldObjectIdentityComponent* Identity = GetOwner()->FindComponentByClass<UWorldObjectIdentityComponent>();
 UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
 FWorldSurvivalProfile Profile;
 if (!Identity || !Identity->HasValidIdentity() || !Survival ||
     !Survival->GetProfile(Identity->GetInstanceId(), Profile) || Profile.Revision == MAX_int64)
  return false;
 TArray<FInventoryStack> Proposed = Items;
 for (FInventoryStack& Stack : Proposed)
  if (Stack.EquippedSlotId == Def->EquipmentSlotId) Stack.EquippedSlotId = NAME_None;
 Proposed[StackIndex].EquippedSlotId = Def->EquipmentSlotId;
 float NewInsulation = 0.0f;
 if (!CalculateClothingInsulation(Proposed, NewInsulation) ||
     !Survival->UpdateProtection(Identity->GetInstanceId(), NewInsulation,
        Profile.ShelterProtection, Profile.Revision)) return false;
 Items = MoveTemp(Proposed);
 return true;
}

bool UInventoryCraftingComponent::TryUnequipItem(FName EquipmentSlotId)
{
 if (EquipmentSlotId.IsNone() || !GetWorld() || !IsValid(GetOwner())) return false;
 const int32 StackIndex = Items.IndexOfByPredicate([EquipmentSlotId](const FInventoryStack& Stack)
  { return Stack.EquippedSlotId == EquipmentSlotId; });
 if (!Items.IsValidIndex(StackIndex)) return true;
 UWorldObjectIdentityComponent* Identity = GetOwner()->FindComponentByClass<UWorldObjectIdentityComponent>();
 UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
 FWorldSurvivalProfile Profile;
 if (!Identity || !Identity->HasValidIdentity() || !Survival ||
     !Survival->GetProfile(Identity->GetInstanceId(), Profile) || Profile.Revision == MAX_int64)
  return false;
 TArray<FInventoryStack> Proposed = Items;
 Proposed[StackIndex].EquippedSlotId = NAME_None;
 float NewInsulation = 0.0f;
 if (!CalculateClothingInsulation(Proposed, NewInsulation) ||
     !Survival->UpdateProtection(Identity->GetInstanceId(), NewInsulation,
        Profile.ShelterProtection, Profile.Revision)) return false;
 Items = MoveTemp(Proposed);
 return true;
}

bool UInventoryCraftingComponent::CanAdvanceItemAging(double DeltaSeconds) const
{
 if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
     MaxSlots <= 0 || Items.Num() > MaxSlots || ItemDefinitions.Num() > 4096) return false;
 TSet<FName> SeenDefinitions;
 for (const FInventoryItemDefinition& Def : ItemDefinitions)
 {
  if (Def.ItemId.IsNone() || SeenDefinitions.Contains(Def.ItemId) ||
      Def.UnitWeightGrams <= 0 || Def.MaxStack <= 0 ||
      !FMath::IsFinite(Def.CaloriesKcal) || Def.CaloriesKcal < 0.0f || Def.CaloriesKcal > 10000.0f ||
      !FMath::IsFinite(Def.HydrationRelief) || Def.HydrationRelief < 0.0f || Def.HydrationRelief > 100.0f ||
      !FMath::IsFinite(Def.ShelfLifeSeconds) || Def.ShelfLifeSeconds < 0.0 ||
      !FMath::IsFinite(Def.FuelBurnSeconds) || Def.FuelBurnSeconds < 0.0f || Def.FuelBurnSeconds > 86400.0f ||
      !FMath::IsFinite(Def.AnimalFeedHungerReduction) ||
      Def.AnimalFeedHungerReduction < 0.0f || Def.AnimalFeedHungerReduction > 100.0f ||
      !FMath::IsFinite(Def.ClothingInsulation) || Def.ClothingInsulation < 0.0f ||
      Def.ClothingInsulation > 1.0f ||
      (Def.bConsumable && (Def.Category != EInventoryItemCategory::Consumable ||
       (Def.CaloriesKcal <= 0.0f && Def.HydrationRelief <= 0.0f))) ||
      (!Def.EquipmentSlotId.IsNone() && Def.MaxStack != 1) ||
      (Def.AnimalFeedHungerReduction > 0.0f && Def.Category != EInventoryItemCategory::Consumable))
   return false;
  SeenDefinitions.Add(Def.ItemId);
 }
 for (const FInventoryItemDefinition& Def : ItemDefinitions)
  if (!Def.SpoiledDefinitionId.IsNone() && !SeenDefinitions.Contains(Def.SpoiledDefinitionId))
   return false;
 TSet<FName> EquippedSlots;
 for (const FInventoryStack& Stack : Items)
 {
  if (Stack.ItemId.IsNone() || Stack.Quantity <= 0 || Stack.MaxStack <= 0 ||
      Stack.Quantity > Stack.MaxStack || !FMath::IsFinite(Stack.AgeSeconds) || Stack.AgeSeconds < 0.0)
   return false;
  if (!Stack.EquippedSlotId.IsNone())
  {
   if (Stack.Quantity != 1 || Stack.MaxStack != 1 || Stack.OwnedWorldItemId.IsValid() ||
       Stack.bSpoiled || EquippedSlots.Contains(Stack.EquippedSlotId)) return false;
   const FInventoryItemDefinition* Def = FindItemDefinition(Stack.ItemId);
   if (!Def || Def->EquipmentSlotId != Stack.EquippedSlotId) return false;
   EquippedSlots.Add(Stack.EquippedSlotId);
  }
  if (Stack.OwnedWorldItemId.IsValid() || Stack.bSpoiled) continue;
  const FInventoryItemDefinition* Def = FindItemDefinition(Stack.ItemId);
  if (!Def || Def->ShelfLifeSeconds <= 0.0) continue;
  const double NextAge = Stack.AgeSeconds + DeltaSeconds;
  if (!FMath::IsFinite(NextAge)) return false;
  if (NextAge >= Def->ShelfLifeSeconds && !Def->SpoiledDefinitionId.IsNone())
  {
   const FInventoryItemDefinition* Spoiled = FindItemDefinition(Def->SpoiledDefinitionId);
   if (!Spoiled || Spoiled->MaxStack < Stack.Quantity ||
       GetConfiguredWeightGrams(Spoiled->ItemId) <= 0) return false;
  }
 }
 return true;
}

bool UInventoryCraftingComponent::AdvanceItemAging(double DeltaSeconds)
{
 if (!CanAdvanceItemAging(DeltaSeconds)) return false;
 for (FInventoryStack& Stack : Items)
 {
  if (Stack.OwnedWorldItemId.IsValid() || Stack.bSpoiled) continue;
  const FInventoryItemDefinition* Def = FindItemDefinition(Stack.ItemId);
  if (!Def || Def->ShelfLifeSeconds <= 0.0) continue;
  Stack.AgeSeconds += DeltaSeconds;
  if (Stack.AgeSeconds < Def->ShelfLifeSeconds) continue;
  const FName SpoiledId = Def->SpoiledDefinitionId;
  Stack.bSpoiled = true;
  if (!SpoiledId.IsNone())
  {
   const FInventoryItemDefinition* Spoiled = FindItemDefinition(SpoiledId);
   if (!Spoiled) return false;
   Stack.ItemId = Spoiled->ItemId;
   Stack.Category = Spoiled->Category;
   Stack.MaxStack = Spoiled->MaxStack;
   Stack.bIsTool = Spoiled->bIsTool;
  }
 }
 return true;
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
 if (!Recipe.RequiredUnlockId.IsNone())
 {
  const UWorldSurvivalSubsystem* Survival =
      GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
  if (!Survival || !Survival->IsContentUnlocked(Recipe.RequiredUnlockId)) return false;
 }
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
  // Restore the inventory view, then verify that every unique physical item
  // represented by that view is still owned by this inventory in the ledger.
  // If any ledger repair fails, block world simulation instead of continuing
  // with two conflicting sources of truth.
  Items = BeforeItems;
  QuickAccessItemId = BeforeQuickAccess;

  bool bRollbackConsistent = true;
  UWorld* World = GetWorld();
  UWorldItemOwnershipSubsystem* Ownership =
   World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;

  for (const FInventoryStack& Stack : BeforeItems)
  {
   if (!Stack.OwnedWorldItemId.IsValid()) continue;

   if (!Ownership)
   {
    bRollbackConsistent = false;
    UE_LOG(LogTemp, Error, TEXT("Craft rollback has no ownership subsystem for physical item %s."),
     *Stack.OwnedWorldItemId.ToString());
    continue;
   }

   FWorldOwnedItemRecord Item;
   if (!Ownership->TryGetItem(Stack.OwnedWorldItemId, Item))
   {
    bRollbackConsistent = false;
    UE_LOG(LogTemp, Error, TEXT("Craft rollback cannot find physical item %s in ownership ledger."),
     *Stack.OwnedWorldItemId.ToString());
    continue;
   }

   if (Item.OwnerKind == EWorldItemOwnerKind::Inventory &&
       Item.OwnerId == InventoryOwnerId)
   {
    continue; // TryRemoveItem may have failed before consuming this item.
   }

   if (Item.OwnerKind != EWorldItemOwnerKind::Consumed ||
       Item.Revision == MAX_int64)
   {
    bRollbackConsistent = false;
    UE_LOG(LogTemp, Error, TEXT("Craft rollback found unexpected ownership for physical item %s."),
     *Item.ItemId.ToString());
    continue;
   }

   FWorldOwnedItemRecord Restored;
   if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
       EWorldItemOwnerKind::Consumed, EWorldItemOwnerKind::Inventory,
       InventoryOwnerId, Item.WorldLocation, Restored))
   {
    FWorldOwnedItemRecord Actual;
    const bool bAlreadyRestored = Ownership->TryGetItem(Item.ItemId, Actual) &&
        Actual.OwnerKind == EWorldItemOwnerKind::Inventory && Actual.OwnerId == InventoryOwnerId;
    if (!bAlreadyRestored)
    {
     bRollbackConsistent = false;
     UE_LOG(LogTemp, Error, TEXT("Craft rollback could not restore physical item %s."),
      *Item.ItemId.ToString());
    }
   }
  }

  if (!bRollbackConsistent && World)
  {
   if (UWorldSimulationCoordinatorSubsystem* Coordinator =
       World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
   {
    Coordinator->MarkSimulationFaulted(
     TEXT("Craft rollback failed to reconcile inventory and physical-item ownership; stop simulation and recover/reset the world."));
   }
   else
   {
    UE_LOG(LogTemp, Error, TEXT("Craft rollback is inconsistent and no simulation coordinator is available."));
   }
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
