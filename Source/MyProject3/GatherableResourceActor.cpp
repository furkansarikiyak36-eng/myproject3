#include "GatherableResourceActor.h"

#include "InventoryCraftingComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

AGatherableResourceActor::AGatherableResourceActor()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
}

void AGatherableResourceActor::BeginPlay()
{
    Super::BeginPlay();
    if (AvailableUnits < 0)
    {
        AvailableUnits = MaxUnits;
        RefillClockSeconds = Now();
    }
}

double AGatherableResourceActor::Now() const
{
    const UWorld* World = GetWorld();
    const UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    return Clock && Clock->IsClockInitialized() ? Clock->GetSimulationTimeSeconds() : 0.0;
}

void AGatherableResourceActor::Refill()
{
    if (AvailableUnits < 0)
    {
        AvailableUnits = MaxUnits;
        RefillClockSeconds = Now();
        return;
    }
    AvailableUnits = FMath::Min(AvailableUnits, MaxUnits); // MaxUnits may be lowered after BeginPlay.
    const double Time = Now();
    if (AvailableUnits >= MaxUnits || RefillSecondsPerUnit <= 0.0 || Time <= RefillClockSeconds)
    {
        // Full or never refilling: the refill clock waits at "now".
        if (AvailableUnits >= MaxUnits)
            RefillClockSeconds = Time;
        return;
    }
    const int32 Grown = static_cast<int32>(FMath::FloorToDouble((Time - RefillClockSeconds) / RefillSecondsPerUnit));
    if (Grown <= 0)
        return;
    AvailableUnits = FMath::Min(MaxUnits, AvailableUnits + Grown);
    RefillClockSeconds = AvailableUnits >= MaxUnits ? Time : RefillClockSeconds + Grown * RefillSecondsPerUnit;
}

int32 AGatherableResourceActor::GetAvailableUnits()
{
    Refill();
    return AvailableUnits;
}

bool AGatherableResourceActor::TryGather(UInventoryCraftingComponent* Inventory, int32& OutGathered)
{
    OutGathered = 0;
    Refill();
    if (!IsValid(Inventory) || ItemDefinitionId.IsNone() || UnitsPerGather <= 0 || AvailableUnits <= 0)
        return false;
    if (!RequiredToolId.IsNone() && Inventory->GetItemCount(RequiredToolId) <= 0)
        return false;

    const int32 Units = FMath::Min(UnitsPerGather, AvailableUnits);
    UWorld* World = GetWorld();
    if (const UWorldGameDataSubsystem* GameData = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr)
    {
        const FGameDataItem* Item = GameData->GetItems().Find(ItemDefinitionId);
        if (Item && !Inventory->ItemWeightGrams.Contains(ItemDefinitionId))
            Inventory->ItemWeightGrams.Add(ItemDefinitionId, Item->WeightGrams);
    }

    if (!bGiveLedgerItems)
    {
        FInventoryStack Stack;
        Stack.ItemId = ItemDefinitionId;
        Stack.Quantity = Units;
        if (!Inventory->TryAddItem(Stack))
            return false;
    }
    else
    {
        UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
        if (!Ownership)
            return false;
        // Check room for all units first, so the gather is all or nothing.
        TArray<FInventoryStack> Incoming;
        for (int32 Index = 0; Index < Units; ++Index)
        {
            FInventoryStack Stack;
            Stack.ItemId = ItemDefinitionId;
            Stack.Quantity = 1;
            Stack.MaxStack = 1;
            Incoming.Add(Stack);
        }
        if (!Inventory->CanAddItems(Incoming))
            return false;
        for (FInventoryStack& Stack : Incoming)
        {
            FWorldOwnedItemRecord Record;
            Record.ItemId = FGuid::NewGuid();
            Record.DefinitionId = ItemDefinitionId;
            Record.OwnerKind = EWorldItemOwnerKind::Inventory;
            Record.OwnerId = Inventory->GetInventoryOwnerId();
            Record.CreatedAtWorldTimeSeconds = Now();
            Stack.OwnedWorldItemId = Record.ItemId;
            if (!Ownership->RegisterItem(Record) || !Inventory->TryAddItem(Stack))
                return false;
        }
    }
    // Refill() left the refill clock at "now" when the place was full, so regrowth starts here.
    AvailableUnits -= Units;
    OutGathered = Units;
    return true;
}
