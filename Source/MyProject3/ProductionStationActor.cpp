#include "ProductionStationActor.h"

#include "HeatSourceComponent.h"
#include "InventoryCraftingComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AProductionStationActor::AProductionStationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.5f;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
}

void AProductionStationActor::BeginPlay()
{
    Super::BeginPlay();
    EnsureRegistered();
}

void AProductionStationActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    const FGameDataStation* Data = GameData && GameData->IsApplied() && !StationDataId.IsNone()
        ? GameData->GetStations().Find(StationDataId) : nullptr;
    if (Data && (Data->bNeedsFire || Data->bNeedsHotFire))
        EnsureRegistered(); // Refresh the condition only when this station type needs fire.
}

bool AProductionStationActor::EnsureRegistered()
{
    UWorld* World = GetWorld();
    UWorldProductionSubsystem* Production = World ? World->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    const UWorldGameDataSubsystem* GameData = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!Production || !GameData || !GameData->IsApplied() || StationDataId.IsNone())
        return false;

    const FGameDataStation* Data = GameData->GetStations().Find(StationDataId);
    if (!Data)
        return false; // Never register a station with an invented fallback capacity.
    if (!StationId.IsValid())
        StationId = FGuid::NewGuid();
    const bool bFireSatisfied = HasRequiredFire();
    FWorldProductionStation Existing;
    if (Production->GetStation(StationId, Existing))
    {
        if (Existing.StationTag != StationDataId || Existing.StorageCapacityUnits != Data->Capacity)
            return false;
        return Production->ConfigureStationFireRequirement(
            StationId, Data->bNeedsFire, Data->bNeedsHotFire, bFireSatisfied);
    }

    FWorldProductionStation Station;
    Station.StationId = StationId;
    Station.StationTag = StationDataId;
    Station.StorageCapacityUnits = Data->Capacity;
    Station.bRequiresFire = Data->bNeedsFire;
    Station.bRequiresHotFire = Data->bNeedsHotFire;
    Station.bFireRequirementSatisfied = (!Data->bNeedsFire && !Data->bNeedsHotFire) || bFireSatisfied;
    return Production->RegisterStation(Station);
}

bool AProductionStationActor::HasRequiredFire() const
{
    const UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !GameData || !GameData->IsApplied() || StationDataId.IsNone())
        return false;

    const FGameDataStation* Data = GameData->GetStations().Find(StationDataId);
    // Fail closed for missing/invalid station definitions; otherwise a typo in
    // StationDataId could silently bypass the station's fuel/fire requirements.
    if (!Data)
        return false;
    if (!Data->bNeedsFire && !Data->bNeedsHotFire)
        return true;

    const FVector Here = GetActorLocation();
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TInlineComponentArray<UHeatSourceComponent*> Fires(*It);
        for (const UHeatSourceComponent* Fire : Fires)
        {
            if (!Fire->IsLit() || FVector::Dist(Fire->GetComponentLocation(), Here) > FireSearchRadiusCm)
                continue;
            if (!Data->bNeedsHotFire || Fire->IsBurningHot())
                return true;
        }
    }
    return false;
}

bool AProductionStationActor::TryDeposit(UInventoryCraftingComponent* Inventory, FName ItemId, int32 Quantity)
{
    UWorldProductionSubsystem* Production = GetWorld() ? GetWorld()->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    FWorldProductionStation Station;
    if (!IsValid(Inventory) || !Production || Quantity <= 0 || !EnsureRegistered() ||
        !Production->GetStation(StationId, Station) || Inventory->GetItemCount(ItemId) < Quantity)
        return false;

    // Station stock is aggregate quantity, not a unique-item owner. Do not
    // route a ledger-identified physical item through generic remove/add stacks.
    if (Inventory->Items.ContainsByPredicate([ItemId](const FInventoryStack& Stack)
        { return Stack.ItemId == ItemId && Stack.OwnedWorldItemId.IsValid(); }))
        return false;

    if (!Production->AddStock(StationId, ItemId, Quantity, Station.Revision))
        return false; // No room in the station.
    if (!Inventory->TryRemoveItem(ItemId, Quantity))
    {
        Production->GetStation(StationId, Station);
        Production->TryTakeStock(StationId, ItemId, Quantity, Station.Revision);
        return false;
    }
    return true;
}

bool AProductionStationActor::TryStartRecipe(FName RecipeId, FGuid& OutJobId)
{
    UWorldProductionSubsystem* Production = GetWorld() ? GetWorld()->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    FWorldProductionStation Station;
    if (!Production || !EnsureRegistered() || !HasRequiredFire() || !Production->GetStation(StationId, Station))
        return false;
    return Production->TryEnqueueJob(FGuid::NewGuid(), StationId, RecipeId, Station.Revision, OutJobId);
}

int32 AProductionStationActor::CollectFinishedJobs()
{
    UWorldProductionSubsystem* Production = GetWorld() ? GetWorld()->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    FWorldProductionStation Station;
    if (!Production || !Production->GetStation(StationId, Station))
        return 0;
    int32 Collected = 0;
    for (const FWorldProductionJob& Job : Station.Jobs)
    {
        if (Job.State != EProductionJobState::Completed)
            continue;
        FWorldProductionStation Current;
        if (Production->GetStation(StationId, Current) &&
            Production->TryCollectCompletedJob(FGuid::NewGuid(), StationId, Job.JobId, Current.Revision))
            ++Collected;
    }
    return Collected;
}

bool AProductionStationActor::TryTake(UInventoryCraftingComponent* Inventory, FName ItemId, int32 Quantity)
{
    UWorldProductionSubsystem* Production = GetWorld() ? GetWorld()->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    FWorldProductionStation Station;
    if (!IsValid(Inventory) || !Production || Quantity <= 0 || !Production->GetStation(StationId, Station) ||
        Station.Stock.FindRef(ItemId) < Quantity)
        return false;
    FInventoryStack Stack;
    Stack.ItemId = ItemId;
    Stack.Quantity = Quantity;
    if (const UWorldGameDataSubsystem* GameData = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldGameDataSubsystem>() : nullptr)
    {
        if (const FGameDataItem* Item = GameData->GetItems().Find(ItemId))
        {
            if (Item->Category == FName(TEXT("Tool")))
                Stack.Category = EInventoryItemCategory::Tool;
            else if (Item->Category == FName(TEXT("Weapon")))
                Stack.Category = EInventoryItemCategory::Weapon;
            else if (Item->Category == FName(TEXT("BuildingPart")))
                Stack.Category = EInventoryItemCategory::BuildingPart;
            else if (Item->Category == FName(TEXT("Consumable")))
                Stack.Category = EInventoryItemCategory::Consumable;
            else if (Item->Category == FName(TEXT("Protection")))
                Stack.Category = EInventoryItemCategory::Protection;
            else if (Item->Category == FName(TEXT("Quest")))
                Stack.Category = EInventoryItemCategory::Quest;
            else
                Stack.Category = EInventoryItemCategory::Resource;
        }
    }
    Stack.bIsTool = Stack.Category == EInventoryItemCategory::Tool;
    Stack.MaxStack = Stack.bIsTool ? 1 : 99;
    if (!Inventory->CanAddItems({ Stack }) ||
        !Production->TryTakeStock(StationId, ItemId, Quantity, Station.Revision))
        return false;

    if (Inventory->TryAddItem(Stack))
        return true;

    // Stock was deducted first; restore it if inventory insertion unexpectedly
    // fails after the capacity preflight.
    FWorldProductionStation AfterTake;
    if (!Production->GetStation(StationId, AfterTake) ||
        !Production->AddStock(StationId, ItemId, Quantity, AfterTake.Revision))
    {
        UE_LOG(LogTemp, Error, TEXT("Could not roll back station stock for item %s."), *ItemId.ToString());
    }
    return false;
}

int32 AProductionStationActor::GetStockCount(FName ItemId) const
{
    const UWorldProductionSubsystem* Production = GetWorld() ? GetWorld()->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    FWorldProductionStation Station;
    return Production && Production->GetStation(StationId, Station) ? Station.Stock.FindRef(ItemId) : 0;
}
