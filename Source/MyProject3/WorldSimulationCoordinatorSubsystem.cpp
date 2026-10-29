#include "WorldSimulationCoordinatorSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "InventoryCraftingComponent.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldCargoTransportSubsystem.h"
#include "WorldRaidAdapterSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldStorageSubsystem.h"
#include "WorldOperationLedgerSubsystem.h"

UWorldSimulationClockSubsystem* UWorldSimulationCoordinatorSubsystem::GetClock() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
}

UWorldThreatDirectorSubsystem* UWorldSimulationCoordinatorSubsystem::GetThreatDirector() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldThreatDirectorSubsystem>() : nullptr;
}

UWorldAgricultureSubsystem* UWorldSimulationCoordinatorSubsystem::GetAgriculture() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
}

UWorldSurvivalSubsystem* UWorldSimulationCoordinatorSubsystem::GetSurvival() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
}

UWorldProductionSubsystem* UWorldSimulationCoordinatorSubsystem::GetProduction() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
}

UWorldCargoTransportSubsystem* UWorldSimulationCoordinatorSubsystem::GetCargo() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldCargoTransportSubsystem>() : nullptr;
}

bool UWorldSimulationCoordinatorSubsystem::InitializeWorldSimulation(
    int32 RandomSeed, double InitialTimeSeconds, double FirstRaidDelaySeconds)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    if (!Clock || !Threat || Clock->IsClockInitialized() || Threat->IsDirectorInitialized() ||
        !FMath::IsFinite(InitialTimeSeconds) || InitialTimeSeconds < 0.0 ||
        !FMath::IsFinite(FirstRaidDelaySeconds) || FirstRaidDelaySeconds < 0.0 ||
        !FMath::IsFinite(InitialTimeSeconds + FirstRaidDelaySeconds) ||
        !FMath::IsFinite(Threat->MinimumIntervalSeconds) ||
        !FMath::IsFinite(Threat->MaximumIntervalSeconds) ||
        Threat->MinimumIntervalSeconds < 1.0 ||
        Threat->MaximumIntervalSeconds < Threat->MinimumIntervalSeconds ||
        !FMath::IsFinite(Threat->WarningDurationSeconds) || Threat->WarningDurationSeconds <= 0.0 ||
        !FMath::IsFinite(Threat->RaidDurationSeconds) || Threat->RaidDurationSeconds <= 0.0 ||
        Threat->MaxHistoryEntries < 1)
    {
        return false;
    }

    if (!Clock->InitializeClock(RandomSeed, InitialTimeSeconds))
    {
        return false;
    }

    // Configuration and time arithmetic are validated before either subsystem
    // is initialized, preventing predictable partial bootstrap failures.
    if (!Threat->InitializeDirector(FirstRaidDelaySeconds))
    {
        return false; // Defensive only; the same conditions were preflighted above.
    }

    CompletedCoordinatorSteps = 0;
    return true;
}

bool UWorldSimulationCoordinatorSubsystem::AdvanceFixedStep(double DeltaSeconds)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    UWorldAgricultureSubsystem* Agriculture = GetAgriculture();
    UWorldSurvivalSubsystem* Survival = GetSurvival();
    UWorldProductionSubsystem* Production = GetProduction();
    UWorldCargoTransportSubsystem* Cargo = GetCargo();
    UWorld* World = GetWorld();
    UWorldRaidAdapterSubsystem* RaidAdapter =
        World ? World->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
    if (bSimulationFaulted || !Clock || !Threat || !Agriculture || !Survival || !Production || !Cargo ||
        !World || !RaidAdapter ||
        !Clock->IsClockInitialized() || !Threat->IsDirectorInitialized() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 ||
        CompletedCoordinatorSteps == MAX_int64 || !Clock->CanAdvanceSimulation(DeltaSeconds))
    {
        return false;
    }

    // Inventories join the shared fixed-step model so perishable stacks do not
    // age on actor frame ticks or use a clock different from crops and production.
    TArray<TWeakObjectPtr<UInventoryCraftingComponent>> InventoryComponents;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UInventoryCraftingComponent*> Components;
        It->GetComponents<UInventoryCraftingComponent>(Components);
        for (UInventoryCraftingComponent* Component : Components)
        {
            if (!IsValid(Component) || !Component->CanAdvanceItemAging(DeltaSeconds))
                return false;
            InventoryComponents.Add(Component);
        }
    }

    const double TargetTime = Clock->GetSimulationTimeSeconds() + DeltaSeconds;
    const int64 TargetTick = Clock->GetTickIndex() + 1;
    if (!Agriculture->CanAdvanceAgricultureStep(DeltaSeconds) ||
        !Survival->CanAdvanceSurvivalStep(DeltaSeconds, TargetTime) ||
        !Production->CanAdvanceProductionStep(DeltaSeconds) ||
        !Cargo->CanAdvanceCargoStep(DeltaSeconds) ||
        !Threat->CanAdvanceDirectorAtTime(TargetTime, TargetTick) ||
        !RaidAdapter->CanAdvanceRaidWorldAdapterAtTime(TargetTime))
    {
        return false;
    }

    // Capture every logical participant before the first commit. Cargo delivery
    // also mutates item ownership, so the ledger must be part of the same rollback.
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldStorageSubsystem* Storage =
        World ? World->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    UWorldOperationLedgerSubsystem* OperationLedger =
        World ? World->GetSubsystem<UWorldOperationLedgerSubsystem>() : nullptr;
    if (!Ownership || !Storage || !OperationLedger) return false;

    const FWorldSimulationClockSnapshot BeforeClock = Clock->ExportSnapshot();
    const FWorldThreatDirectorSnapshot BeforeThreat = Threat->ExportSnapshot();
    TArray<FWorldCropState> BeforeCrops;
    TArray<FWorldAnimalState> BeforeAnimals;
    Agriculture->ExportSnapshot(BeforeCrops, BeforeAnimals);
    TArray<FWorldSurvivalProfile> BeforeProfiles;
    TArray<FWorldDiscoveryRecord> BeforeDiscoveries;
    EWorldWeatherState BeforeWeather = EWorldWeatherState::Clear;
    float BeforeWeatherSeverity = 0.0f;
    Survival->ExportSnapshot(BeforeProfiles, BeforeDiscoveries, BeforeWeather, BeforeWeatherSeverity);
    TArray<FWorldHeatSourceRuntimeSnapshot> BeforeHeatSources;
    Survival->ExportHeatSourceRuntimeSnapshot(BeforeHeatSources);
    TArray<FWorldProductionStation> BeforeStations;
    TArray<FWorldProductionRecipe> BeforeRecipes;
    Production->ExportSnapshot(BeforeStations, BeforeRecipes);
    TArray<FWorldCargoTransportRecord> BeforeCargo;
    Cargo->ExportSnapshot(BeforeCargo);
    TArray<FWorldOwnedItemRecord> BeforeItems;
    TArray<FWorldItemTransferReceipt> BeforeItemTransferReceipts;
    TArray<FWorldItemSplitReceipt> BeforeItemSplitReceipts;
    Ownership->ExportSnapshot(BeforeItems, BeforeItemTransferReceipts, BeforeItemSplitReceipts);
    TArray<FWorldStorageContainerRecord> BeforeStorages;
    TArray<FWorldStorageIncomingReservation> BeforeReservations;
    Storage->ExportSnapshot(BeforeStorages, BeforeReservations);
    TArray<FWorldOperationLedgerEntry> BeforeOperationLedgerEntries;
    OperationLedger->ExportSnapshot(BeforeOperationLedgerEntries);

    struct FInventoryAgingBeforeState
    {
        TWeakObjectPtr<UInventoryCraftingComponent> Component;
        TArray<FInventoryStack> Items;
        FName QuickAccessItemId = NAME_None;
    };
    TArray<FInventoryAgingBeforeState> BeforeInventoryStates;
    BeforeInventoryStates.Reserve(InventoryComponents.Num());
    for (const TWeakObjectPtr<UInventoryCraftingComponent>& WeakComponent : InventoryComponents)
    {
        UInventoryCraftingComponent* Component = WeakComponent.Get();
        if (!IsValid(Component)) return false;
        FInventoryAgingBeforeState& State = BeforeInventoryStates.AddDefaulted_GetRef();
        State.Component = Component;
        State.Items = Component->Items;
        State.QuickAccessItemId = Component->QuickAccessItemId;
    }

    auto RollbackStep = [&]() -> bool
    {
        bool bRollbackSucceeded = true;
        // Restore ownership before cargo because cargo snapshot validation is
        // intentionally cross-domain and checks the ledger's current owner/revision.
        bRollbackSucceeded = Ownership->RestoreSnapshot(
            BeforeItems, BeforeItemTransferReceipts, BeforeItemSplitReceipts) && bRollbackSucceeded;
        bRollbackSucceeded = OperationLedger->RestoreSnapshot(BeforeOperationLedgerEntries) && bRollbackSucceeded;
        // Cargo restore validates its incoming reservations against the storage
        // subsystem; restore the storage manifest before attempting cargo.
        bRollbackSucceeded = Storage->RestoreSnapshot(BeforeStorages, BeforeReservations) && bRollbackSucceeded;
        bRollbackSucceeded = Cargo->RestoreSnapshot(BeforeCargo) && bRollbackSucceeded;
        bRollbackSucceeded = Production->RestoreSnapshot(BeforeStations, BeforeRecipes) && bRollbackSucceeded;
        bRollbackSucceeded = Survival->RestoreSnapshot(BeforeProfiles, BeforeDiscoveries,
            BeforeWeather, BeforeWeatherSeverity) && bRollbackSucceeded;
        bRollbackSucceeded = Survival->RestoreHeatSourceRuntimeSnapshot(BeforeHeatSources) && bRollbackSucceeded;
        bRollbackSucceeded = Agriculture->RestoreSnapshot(BeforeCrops, BeforeAnimals) && bRollbackSucceeded;
        bRollbackSucceeded = Threat->RestoreSnapshot(BeforeThreat) && bRollbackSucceeded;
        bRollbackSucceeded = Clock->RestoreSnapshot(BeforeClock) && bRollbackSucceeded;
        for (const FInventoryAgingBeforeState& State : BeforeInventoryStates)
        {
            UInventoryCraftingComponent* Component = State.Component.Get();
            if (!IsValid(Component))
            {
                bRollbackSucceeded = false;
                continue;
            }
            Component->Items = State.Items;
            Component->QuickAccessItemId = State.QuickAccessItemId;
        }
        if (!bRollbackSucceeded)
        {
            // A partially restored world must never continue consuming fixed
            // steps. Fail closed until the owning world is reset/reloaded.
            bSimulationFaulted = true;
            SimulationFaultReason = TEXT(
                "Fixed-step rollback failed; authoritative world state may be inconsistent. Stop simulation and recover/reset the world.");
            UE_LOG(LogTemp, Error, TEXT("World simulation entered fail-closed state: %s"),
                *SimulationFaultReason);
        }
        return bRollbackSucceeded;
    };

    // Every participant completed preflight above. If a commit still rejects,
    // restore all participating logical state instead of leaving a mixed tick.
    if (!Agriculture->AdvanceAgricultureStep(DeltaSeconds) ||
        !Survival->AdvanceSurvivalStep(DeltaSeconds, TargetTime) ||
        !Production->AdvanceProductionStep(DeltaSeconds) ||
        !Cargo->AdvanceCargoStep(DeltaSeconds) ||
        !Threat->AdvanceDirectorAtTime(TargetTime, TargetTick) ||
        !Clock->AdvanceSimulation(DeltaSeconds))
    {
        RollbackStep();
        return false;
    }

    for (const TWeakObjectPtr<UInventoryCraftingComponent>& WeakComponent : InventoryComponents)
    {
        UInventoryCraftingComponent* Component = WeakComponent.Get();
        if (!IsValid(Component) || !Component->AdvanceItemAging(DeltaSeconds))
        {
            RollbackStep();
            return false;
        }
    }

    ++CompletedCoordinatorSteps;

    // The raid adapter may spawn/destroy actors and apply structural damage,
    // which cannot be rolled back by the logical subsystem snapshots above.
    // Run it only after the shared clock and all logical domains commit. Any
    // unexpected adapter failure freezes the world instead of replaying a tick
    // whose effects may be partial.
    if (!RaidAdapter->AdvanceRaidWorldAdapter())
    {
        MarkSimulationFaulted(
            TEXT("Raid world adapter failed after the fixed-step logical commit. Actor/structure effects may be partial; recover from a durable save or reset the world."));
        return false;
    }
    return true;
}

void UWorldSimulationCoordinatorSubsystem::MarkSimulationFaulted(const FString& Reason)
{
    bSimulationFaulted = true;
    if (SimulationFaultReason.IsEmpty())
    {
        SimulationFaultReason = Reason.IsEmpty()
            ? TEXT("An unrecoverable world-state failure occurred. Stop simulation and recover/reset the world.")
            : Reason;
    }
    UE_LOG(LogTemp, Error, TEXT("World simulation entered fail-closed state: %s"), *SimulationFaultReason);
}

bool UWorldSimulationCoordinatorSubsystem::IsWorldSimulationReady() const
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    const UWorldThreatDirectorSubsystem* Threat = GetThreatDirector();
    const UWorldAgricultureSubsystem* Agriculture = GetAgriculture();
    const UWorldSurvivalSubsystem* Survival = GetSurvival();
    const UWorldProductionSubsystem* Production = GetProduction();
    const UWorldCargoTransportSubsystem* Cargo = GetCargo();
    const UWorld* World = GetWorld();
    const UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    const UWorldStorageSubsystem* Storage =
        World ? World->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    const UWorldOperationLedgerSubsystem* OperationLedger =
        World ? World->GetSubsystem<UWorldOperationLedgerSubsystem>() : nullptr;
    return !bSimulationFaulted && Clock && Threat && Agriculture && Survival &&
        Production && Cargo && Ownership && Storage && OperationLedger &&
        Clock->IsClockInitialized() && Threat->IsDirectorInitialized();
}
