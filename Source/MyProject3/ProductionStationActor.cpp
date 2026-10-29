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
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
}

void AProductionStationActor::BeginPlay()
{
    Super::BeginPlay();
    EnsureRegistered();
}

bool AProductionStationActor::EnsureRegistered()
{
    UWorld* World = GetWorld();
    UWorldProductionSubsystem* Production = World ? World->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    if (!Production || StationDataId.IsNone())
        return false;
    if (!StationId.IsValid())
        StationId = FGuid::NewGuid();
    FWorldProductionStation Existing;
    if (Production->GetStation(StationId, Existing))
        return true;
    FWorldProductionStation Station;
    Station.StationId = StationId;
    Station.StationTag = StationDataId;
    if (const UWorldGameDataSubsystem* GameData = World->GetSubsystem<UWorldGameDataSubsystem>())
        if (const FGameDataStation* Data = GameData->GetStations().Find(StationDataId))
            Station.StorageCapacityUnits = Data->Capacity;
    return Production->RegisterStation(Station);
}

bool AProductionStationActor::HasRequiredFire() const
{
    const UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    const FGameDataStation* Data = GameData ? GameData->GetStations().Find(StationDataId) : nullptr;
    if (!Data || (!Data->bNeedsFire && !Data->bNeedsHotFire))
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
    if (!IsValid(Inventory) || !Production || Quantity <= 0 ||
        !Inventory->IsDefinitionAvailableForProgression(ItemId) || !EnsureRegistered() ||
        !Production->GetStation(StationId, Station) || Inventory->GetItemCount(ItemId) < Quantity)
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
    // The legacy call has no player identity; the production subsystem rejects any locked recipe.
    return Production->TryEnqueueJob(FGuid::NewGuid(), StationId, RecipeId, Station.Revision, OutJobId);
}

bool AProductionStationActor::TryStartRecipeForPlayer(
    FName RecipeId, FGuid PlayerEntityId, FGuid& OutJobId)
{
    OutJobId.Invalidate();
    UWorldProductionSubsystem* Production = GetWorld() ? GetWorld()->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    FWorldProductionStation Station;
    if (!Production || !PlayerEntityId.IsValid() || !EnsureRegistered() ||
        !HasRequiredFire() || !Production->GetStation(StationId, Station))
        return false;
    return Production->TryEnqueueJobForPlayer(
        FGuid::NewGuid(), StationId, RecipeId, Station.Revision, PlayerEntityId, OutJobId);
}

bool AProductionStationActor::TryStartRecipeForInventory(
    FName RecipeId, UInventoryCraftingComponent* Inventory, FGuid& OutJobId)
{
    if (!IsValid(Inventory) || !Inventory->ProgressionEntityId.IsValid())
    {
        OutJobId.Invalidate();
        return false;
    }
    return TryStartRecipeForPlayer(RecipeId, Inventory->ProgressionEntityId, OutJobId);
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
    if (!IsValid(Inventory) || !Production || Quantity <= 0 ||
        !Inventory->IsDefinitionAvailableForProgression(ItemId) ||
        !Production->GetStation(StationId, Station) || Station.Stock.FindRef(ItemId) < Quantity)
        return false;
    FInventoryStack Stack;
    Stack.ItemId = ItemId;
    Stack.Quantity = Quantity;
    if (!Inventory->CanAddItems({ Stack }) || !Production->TryTakeStock(StationId, ItemId, Quantity, Station.Revision))
        return false;
    return Inventory->TryAddItem(Stack);
}

int32 AProductionStationActor::GetStockCount(FName ItemId) const
{
    const UWorldProductionSubsystem* Production = GetWorld() ? GetWorld()->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    FWorldProductionStation Station;
    return Production && Production->GetStation(StationId, Station) ? Station.Stock.FindRef(ItemId) : 0;
}
