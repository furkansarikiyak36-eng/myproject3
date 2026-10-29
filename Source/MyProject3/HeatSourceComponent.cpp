#include "HeatSourceComponent.h"

#include "WorldSurvivalSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "InventoryCraftingComponent.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#include "Engine/World.h"

UHeatSourceComponent::UHeatSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHeatSourceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr)
    {
        Survival->RegisterHeatSource(this);
    }
}

void UHeatSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr)
    {
        Survival->UnregisterHeatSource(this);
    }
    Super::EndPlay(EndPlayReason);
}

bool UHeatSourceComponent::RegisterFuelDefinition(const FHeatFuelDefinition& Definition)
{
    if (Definition.ItemDefinitionId.IsNone() ||
        !FMath::IsFinite(Definition.BurnSecondsPerItem) ||
        Definition.BurnSecondsPerItem <= 0.0f || Definition.BurnSecondsPerItem > 86400.0f ||
        MaxFuelDefinitions <= 0 || MaxFuelDefinitions > 1024 ||
        FuelDefinitions.Num() >= MaxFuelDefinitions ||
        FuelDefinitions.ContainsByPredicate([&Definition](const FHeatFuelDefinition& Existing)
            { return Existing.ItemDefinitionId == Definition.ItemDefinitionId; }))
        return false;
    FuelDefinitions.Add(Definition);
    return true;
}

bool UHeatSourceComponent::TryAddFuelItem(
    const FGuid& OperationId, const FGuid& ItemId, int64 ExpectedItemRevision,
    const FGuid& InventoryOwnerId)
{
    if (!OperationId.IsValid() || !ItemId.IsValid() ||
        ExpectedItemRevision <= 0 || ExpectedItemRevision >= MAX_int64 - 1 ||
        !InventoryOwnerId.IsValid())
        return false;

    // Resolve retries first so receipt capacity or later fuel burning cannot
    // cause a previously accepted refuel to be applied a second time.
    for (const FHeatFuelOperationReceipt& Receipt : FuelReceipts)
    {
        if (Receipt.OperationId != OperationId)
            continue;
        return Receipt.ItemId == ItemId &&
            Receipt.ExpectedItemRevision == ExpectedItemRevision &&
            Receipt.InventoryOwnerId == InventoryOwnerId;
    }

    UWorld* World = GetWorld();
    if (!World)
        return false;
    // Operation IDs are global across heat-source components. Without this
    // guard, the ownership ledger's idempotent consume receipt could be replayed
    // into a second fire and award the same burn time twice.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UHeatSourceComponent*> OtherSources;
        if (IsValid(*It))
            It->GetComponents<UHeatSourceComponent>(OtherSources);
        for (UHeatSourceComponent* Other : OtherSources)
        {
            if (!IsValid(Other) || Other == this)
                continue;
            for (const FHeatFuelOperationReceipt& Receipt : Other->FuelReceipts)
                if (Receipt.OperationId == OperationId)
                    return false;
        }
    }

    if (MaxFuelReceipts <= 0 || MaxFuelReceipts > 4096 ||
        FuelReceipts.Num() >= MaxFuelReceipts ||
        !FMath::IsFinite(FuelSeconds) || !FMath::IsFinite(MaxFuelSeconds) ||
        FuelSeconds < 0.0f || MaxFuelSeconds <= 0.0f || FuelSeconds > MaxFuelSeconds)
        return false;

    UWorldItemOwnershipSubsystem* Ownership =
        World->GetSubsystem<UWorldItemOwnershipSubsystem>();
    if (!Ownership)
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(ItemId, Item) ||
        Item.Revision != ExpectedItemRevision || Item.Revision >= MAX_int64 - 1 ||
        Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Item.OwnerId != InventoryOwnerId || Item.Quantity != 1)
        return false;

    const FHeatFuelDefinition* Definition = nullptr;
    for (const FHeatFuelDefinition& Candidate : FuelDefinitions)
    {
        if (Candidate.ItemDefinitionId != Item.DefinitionId)
            continue;
        if (Definition)
            return false; // Duplicate authored definitions are ambiguous.
        Definition = &Candidate;
    }
    if (!Definition || !FMath::IsFinite(Definition->BurnSecondsPerItem) ||
        Definition->BurnSecondsPerItem <= 0.0f || Definition->BurnSecondsPerItem > 86400.0f ||
        Definition->BurnSecondsPerItem > MaxFuelSeconds - FuelSeconds)
        return false;

    UInventoryCraftingComponent* Inventory = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        UInventoryCraftingComponent* Candidate =
            IsValid(*It) ? It->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
        if (IsValid(Candidate) && Candidate->GetInventoryOwnerId() == InventoryOwnerId)
        {
            if (Inventory)
                return false;
            Inventory = Candidate;
        }
    }
    if (!Inventory)
        return false;

    FGuid OwnerActorId;
    FGuid PreviousInventoryOwnerId;
    TArray<FInventoryStack> PreviousInventory;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousInventoryOwnerId,
        PreviousInventory, PreviousQuickAccess);
    if (PreviousInventoryOwnerId != InventoryOwnerId)
        return false;

    bool bHasExactStack = false;
    for (const FInventoryStack& Stack : PreviousInventory)
    {
        if (Stack.OwnedWorldItemId == ItemId)
        {
            if (Stack.Quantity != 1 || Stack.ItemId != Item.DefinitionId)
                return false;
            bHasExactStack = true;
            break;
        }
    }
    if (!bHasExactStack)
        return false;

    TArray<FWorldOwnedItemRecord> PreviousItems;
    TArray<FWorldItemTransferReceipt> PreviousTransfers;
    TArray<FWorldItemSplitReceipt> PreviousSplits;
    Ownership->ExportSnapshot(PreviousItems, PreviousTransfers, PreviousSplits);
    auto Rollback = [&]() -> bool
    {
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
        const bool bItemsRestored = Ownership->RestoreSnapshot(
            PreviousItems, PreviousTransfers, PreviousSplits);
        if (!bInventoryRestored || !bItemsRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Fuel-item rollback failed for item %s."),
                *ItemId.ToString());
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Fuel refuelling rollback failed; stop simulation and recover/reset the world."));
            }
            return false;
        }
        return true;
    };

    FInventoryStack RemovedStack;
    if (!Inventory->TryRemoveOwnedItemReference(ItemId, RemovedStack))
        return false;
    if (!Ownership->TryConsumeItem(OperationId, ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Inventory))
    {
        Rollback();
        return false;
    }

    FuelSeconds += Definition->BurnSecondsPerItem;
    FHeatFuelOperationReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.ItemId = ItemId;
    Receipt.InventoryOwnerId = InventoryOwnerId;
    Receipt.ExpectedItemRevision = ExpectedItemRevision;
    Receipt.ItemDefinitionId = Item.DefinitionId;
    Receipt.AddedBurnSeconds = Definition->BurnSecondsPerItem;
    Receipt.ResultingFuelSeconds = FuelSeconds;
    FuelReceipts.Add(Receipt);
    return true;
}

bool UHeatSourceComponent::AddFuel(float BurnSeconds)
{
    if (!FMath::IsFinite(BurnSeconds) || BurnSeconds <= 0.0f || FuelSeconds >= MaxFuelSeconds)
        return false;
    FuelSeconds = FMath::Min(MaxFuelSeconds, FuelSeconds + BurnSeconds);
    return true;
}

bool UHeatSourceComponent::Ignite()
{
    if (FuelSeconds <= 0.0f)
        return false;
    bLit = true;
    return true;
}

void UHeatSourceComponent::Extinguish()
{
    bLit = false;
}

float UHeatSourceComponent::GetHeatAt(const FVector& WorldLocation) const
{
    if (!bLit || RadiusCm <= 0.0f || WorldLocation.ContainsNaN())
        return 0.0f;
    const float Distance = FVector::Dist(GetComponentLocation(), WorldLocation);
    if (Distance >= RadiusCm)
        return 0.0f;
    // Quadratic falloff: warm next to the fire, little at the edge.
    const float Closeness = 1.0f - Distance / RadiusCm;
    return HeatOutputCelsius * Closeness * Closeness;
}

bool UHeatSourceComponent::RestoreSimulationFuelState(float InFuelSeconds, bool bInLit)
{
    if (!FMath::IsFinite(InFuelSeconds) || InFuelSeconds < 0.0f ||
        !FMath::IsFinite(MaxFuelSeconds) || MaxFuelSeconds <= 0.0f ||
        InFuelSeconds > MaxFuelSeconds || (bInLit && InFuelSeconds <= 0.0f))
        return false;
    FuelSeconds = InFuelSeconds;
    bLit = bInLit;
    return true;
}

void UHeatSourceComponent::AdvanceFuel(double DeltaSeconds, EWorldWeatherState Weather, float WeatherSeverity)
{
    if (!bLit || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
        return;

    // Heavy rain and storms drown a fire that is not sheltered enough.
    const bool bWetWeather = Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm;
    if (bWetWeather && WeatherSeverity > WeatherProtection)
    {
        bLit = false;
        return;
    }

    FuelSeconds = FMath::Max(0.0f, FuelSeconds - static_cast<float>(DeltaSeconds));
    if (FuelSeconds <= 0.0f)
        bLit = false;
}
