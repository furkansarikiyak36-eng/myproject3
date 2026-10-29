#include "WorldSurvivalSubsystem.h"

#include "HeatSourceComponent.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "InventoryCraftingComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace
{
    bool InRange(float Value, float Min, float Max)
    {
        return FMath::IsFinite(Value) && Value >= Min && Value <= Max;
    }
}

const FWorldSurvivalConditionDefinition*
UWorldSurvivalSubsystem::FindConditionDefinition(FName DefinitionId) const
{
    return DefinitionId.IsNone() ? nullptr :
        ConditionDefinitions.FindByPredicate([DefinitionId](const FWorldSurvivalConditionDefinition& Definition)
        { return Definition.DefinitionId == DefinitionId; });
}

const FWorldSurvivalEquipmentDefinition*
UWorldSurvivalSubsystem::FindEquipmentDefinition(FName ItemDefinitionId) const
{
    return ItemDefinitionId.IsNone() ? nullptr :
        EquipmentDefinitions.FindByPredicate([ItemDefinitionId](const FWorldSurvivalEquipmentDefinition& Definition)
        { return Definition.ItemDefinitionId == ItemDefinitionId; });
}

const FWorldDiscoveryDefinition*
UWorldSurvivalSubsystem::FindDiscoveryDefinition(FName DiscoveryId) const
{
    return DiscoveryId.IsNone() ? nullptr :
        DiscoveryDefinitions.FindByPredicate([DiscoveryId](const FWorldDiscoveryDefinition& Definition)
        { return Definition.DiscoveryId == DiscoveryId; });
}

bool UWorldSurvivalSubsystem::FindInventoryForOwner(
    const FGuid& InventoryOwnerId, UInventoryCraftingComponent*& OutInventory) const
{
    OutInventory = nullptr;
    UWorld* World = GetWorld();
    if (!World || !InventoryOwnerId.IsValid())
        return false;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        UInventoryCraftingComponent* Candidate =
            IsValid(*It) ? It->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
        if (!IsValid(Candidate) || Candidate->GetInventoryOwnerId() != InventoryOwnerId)
            continue;
        if (OutInventory)
        {
            OutInventory = nullptr;
            return false; // Duplicate owner identities are ambiguous and fail closed.
        }
        OutInventory = Candidate;
    }
    return OutInventory != nullptr;
}

void UWorldSurvivalSubsystem::RecomputeEquipmentProtection(FWorldSurvivalProfile& Profile) const
{
    double Insulation = 0.0;
    double WaterResistance = 0.0;
    for (const FWorldEquippedItemState& Equipped : Profile.EquippedItems)
    {
        const FWorldSurvivalEquipmentDefinition* Definition =
            FindEquipmentDefinition(Equipped.ItemDefinitionId);
        if (!Definition || Definition->Slot != Equipped.Slot)
            continue;
        Insulation += Definition->Insulation;
        WaterResistance += Definition->WaterResistance;
    }
    Profile.ClothingInsulation = static_cast<float>(FMath::Clamp(Insulation, 0.0, 1.0));
    Profile.ClothingWaterResistance = static_cast<float>(FMath::Clamp(WaterResistance, 0.0, 1.0));
}

bool UWorldSurvivalSubsystem::IsValidProfile(const FWorldSurvivalProfile& Profile) const
{
    if (!Profile.EntityId.IsValid() || Profile.Revision <= 0 ||
        !InRange(Profile.Health, 0.0f, 100.0f) || !InRange(Profile.Hunger, 0.0f, 100.0f) ||
        !InRange(Profile.Thirst, 0.0f, 100.0f) || !InRange(Profile.Stamina, 0.0f, 100.0f) ||
        !InRange(Profile.Fatigue, 0.0f, 100.0f) || !InRange(Profile.ClothingInsulation, 0.0f, 1.0f) ||
        !InRange(Profile.ClothingWaterResistance, 0.0f, 1.0f) ||
        !InRange(Profile.ShelterProtection, 0.0f, 1.0f) || !InRange(Profile.Wetness, 0.0f, 1.0f) ||
        !InRange(Profile.CaloriesKcal, 0.0f, MaxCaloriesKcal) ||
        !InRange(Profile.CoreTemperatureCelsius, 20.0f, 45.0f) ||
        !InRange(Profile.PendingActivityKcal, 0.0f, 100000.0f) ||
        !FMath::IsFinite(Profile.RestUntilTimeSeconds) || Profile.RestUntilTimeSeconds < 0.0 ||
        MaxActionReceiptsPerProfile <= 0 || Profile.ActionReceipts.Num() > MaxActionReceiptsPerProfile ||
        Profile.EquippedItems.Num() > MaxEquippedItemsPerProfile ||
        MaxEquippedItemsPerProfile <= 0 || MaxEquippedItemsPerProfile > 5 ||
        Profile.Conditions.Num() > 128 || Profile.Location.ContainsNaN())
        return false;

    TSet<FGuid> SeenConditionIds;
    for (const FWorldSurvivalConditionState& Condition : Profile.Conditions)
    {
        const FWorldSurvivalConditionDefinition* Definition =
            FindConditionDefinition(Condition.DefinitionId);
        if (!Condition.ConditionId.IsValid() || SeenConditionIds.Contains(Condition.ConditionId) ||
            !Definition || Definition->Kind != Condition.Kind ||
            static_cast<uint8>(Condition.Kind) > static_cast<uint8>(EWorldSurvivalConditionKind::Poisoning) ||
            !FMath::IsFinite(Condition.StartedAtWorldTimeSeconds) ||
            Condition.StartedAtWorldTimeSeconds < 0.0 ||
            !FMath::IsFinite(Condition.EndsAtWorldTimeSeconds) ||
            Condition.EndsAtWorldTimeSeconds < 0.0 ||
            (Condition.EndsAtWorldTimeSeconds > 0.0 &&
             Condition.EndsAtWorldTimeSeconds < Condition.StartedAtWorldTimeSeconds) ||
            !InRange(Condition.Severity, 0.0f, 1.0f) || Condition.Severity <= 0.0f ||
            Condition.Revision <= 0)
            return false;
        SeenConditionIds.Add(Condition.ConditionId);
    }

    TSet<FGuid> SeenEquippedItems;
    TSet<uint8> SeenSlots;
    double DerivedInsulation = 0.0;
    double DerivedWaterResistance = 0.0;
    for (const FWorldEquippedItemState& Equipped : Profile.EquippedItems)
    {
        const FWorldSurvivalEquipmentDefinition* Definition =
            FindEquipmentDefinition(Equipped.ItemDefinitionId);
        const uint8 Slot = static_cast<uint8>(Equipped.Slot);
        if (!Equipped.ItemId.IsValid() || SeenEquippedItems.Contains(Equipped.ItemId) ||
            SeenSlots.Contains(Slot) || Slot > static_cast<uint8>(EWorldEquipmentSlot::Feet) ||
            !Definition || Definition->Slot != Equipped.Slot ||
            Definition->ItemDefinitionId != Equipped.ItemDefinitionId ||
            Equipped.ItemRevision <= 0)
            return false;
        SeenEquippedItems.Add(Equipped.ItemId);
        SeenSlots.Add(Slot);
        DerivedInsulation += Definition->Insulation;
        DerivedWaterResistance += Definition->WaterResistance;
    }

    // Profiles from legacy saves may have authored protection fields but no
    // equipment ledger yet. Once any item is equipped, the equipment records
    // are authoritative for both clothing protection values.
    if (!Profile.EquippedItems.IsEmpty() &&
        (!FMath::IsNearlyEqual(Profile.ClothingInsulation,
            static_cast<float>(FMath::Clamp(DerivedInsulation, 0.0, 1.0)), 0.001f) ||
         !FMath::IsNearlyEqual(Profile.ClothingWaterResistance,
            static_cast<float>(FMath::Clamp(DerivedWaterResistance, 0.0, 1.0)), 0.001f)))
        return false;

    TSet<FGuid> SeenOperations;
    for (const FWorldSurvivalActionReceipt& Receipt : Profile.ActionReceipts)
    {
        if (!Receipt.OperationId.IsValid() || SeenOperations.Contains(Receipt.OperationId) ||
            Receipt.EntityId != Profile.EntityId ||
            static_cast<uint8>(Receipt.Action) > static_cast<uint8>(EWorldSurvivalActionKind::RecordDiscovery) ||
            Receipt.ExpectedProfileRevision <= 0 ||
            Receipt.ResultingProfileRevision != Receipt.ExpectedProfileRevision + 1 ||
            Receipt.ResultingProfileRevision > Profile.Revision)
            return false;
        SeenOperations.Add(Receipt.OperationId);

        switch (Receipt.Action)
        {
        case EWorldSurvivalActionKind::ConsumeFoodItem:
            if (!Receipt.ItemId.IsValid() || !Receipt.ExpectedItemOwnerId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ItemDefinitionId.IsNone() ||
                !InRange(Receipt.CaloriesKcal, 0.0f, 10000.0f) ||
                !InRange(Receipt.ThirstRelief, 0.0f, 100.0f))
                return false;
            break;
        case EWorldSurvivalActionKind::BeginRest:
            if (!FMath::IsFinite(Receipt.RequestedStartTimeSeconds) ||
                Receipt.RequestedStartTimeSeconds < 0.0 ||
                !FMath::IsFinite(Receipt.RequestedDurationSeconds) ||
                Receipt.RequestedDurationSeconds <= 0.0 ||
                Receipt.RequestedDurationSeconds > 43200.0 ||
                !FMath::IsFinite(Receipt.ResultingRestUntilTimeSeconds) ||
                !FMath::IsNearlyEqual(Receipt.ResultingRestUntilTimeSeconds,
                    Receipt.RequestedStartTimeSeconds + Receipt.RequestedDurationSeconds, 0.001))
                return false;
            break;
        case EWorldSurvivalActionKind::ApplyCondition:
            if (!Receipt.RelatedConditionId.IsValid() || Receipt.ConditionDefinitionId.IsNone() ||
                !FMath::IsFinite(Receipt.ConditionEndTimeSeconds) ||
                Receipt.ConditionEndTimeSeconds < 0.0)
                return false;
            break;
        case EWorldSurvivalActionKind::TreatCondition:
            if (!Receipt.RelatedConditionId.IsValid() || Receipt.ConditionDefinitionId.IsNone() ||
                !Receipt.ItemId.IsValid() || !Receipt.ExpectedItemOwnerId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ItemDefinitionId.IsNone())
                return false;
            break;
        case EWorldSurvivalActionKind::EquipItem:
        case EWorldSurvivalActionKind::UnequipItem:
            if (!Receipt.ItemId.IsValid() || !Receipt.ExpectedItemOwnerId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ItemDefinitionId.IsNone() ||
                static_cast<uint8>(Receipt.EquipmentSlot) > static_cast<uint8>(EWorldEquipmentSlot::Feet))
                return false;
            break;
        case EWorldSurvivalActionKind::RecordDiscovery:
            if (Receipt.DiscoveryId.IsNone() || Receipt.RequestedLocation.ContainsNaN() ||
                !FMath::IsFinite(Receipt.RequestedDiscoveryTimeSeconds) ||
                Receipt.RequestedDiscoveryTimeSeconds < 0.0)
                return false;
            break;
        default:
            return false;
        }
    }
    return true;
}

bool UWorldSurvivalSubsystem::RegisterProfile(const FWorldSurvivalProfile& Profile)
{
    if (Profiles.Contains(Profile.EntityId) || !IsValidProfile(Profile))
        return false;
    Profiles.Add(Profile.EntityId, Profile);
    return true;
}

const FWorldSurvivalConsumableDefinition*
UWorldSurvivalSubsystem::FindConsumableDefinition(FName DefinitionId) const
{
    if (DefinitionId.IsNone())
        return nullptr;
    return ConsumableDefinitions.FindByPredicate([DefinitionId](
        const FWorldSurvivalConsumableDefinition& Definition)
    {
        return Definition.DefinitionId == DefinitionId;
    });
}

const FWorldSurvivalActionReceipt*
UWorldSurvivalSubsystem::FindActionReceipt(const FGuid& OperationId) const
{
    if (!OperationId.IsValid())
        return nullptr;
    for (const TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
    {
        const FWorldSurvivalActionReceipt* Found = Pair.Value.ActionReceipts.FindByPredicate(
            [OperationId](const FWorldSurvivalActionReceipt& Receipt)
            {
                return Receipt.OperationId == OperationId;
            });
        if (Found)
            return Found;
    }
    return nullptr;
}

bool UWorldSurvivalSubsystem::RegisterConsumableDefinition(
    const FWorldSurvivalConsumableDefinition& Definition)
{
    if (Definition.DefinitionId.IsNone() ||
        !InRange(Definition.CaloriesKcal, 0.0f, 10000.0f) ||
        !InRange(Definition.ThirstRelief, 0.0f, 100.0f) ||
        (Definition.CaloriesKcal <= 0.0f && Definition.ThirstRelief <= 0.0f) ||
        !FMath::IsFinite(Definition.FreshnessDurationSeconds) ||
        Definition.FreshnessDurationSeconds < 0.0 ||
        !InRange(Definition.SpoiledEnergyFraction, 0.0f, 1.0f) ||
        MaxConsumableDefinitions <= 0 || MaxConsumableDefinitions > 4096)
    {
        return false;
    }
    if (FindConsumableDefinition(Definition.DefinitionId) ||
        ConsumableDefinitions.Num() >= MaxConsumableDefinitions)
    {
        return false;
    }
    ConsumableDefinitions.Add(Definition);
    return true;
}

bool UWorldSurvivalSubsystem::TryConsumeFoodItem(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    const FGuid& ItemId, int64 ExpectedItemRevision, const FGuid& ExpectedInventoryOwnerId)
{
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        !ItemId.IsValid() || ExpectedItemRevision <= 0 || !ExpectedInventoryOwnerId.IsValid())
    {
        return false;
    }

    // A committed retry must be resolved before stale-revision checks: after the
    // first commit both the profile and item revisions have advanced.
    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        return Existing->Action == EWorldSurvivalActionKind::ConsumeFoodItem &&
            Existing->EntityId == EntityId && Existing->ItemId == ItemId &&
            Existing->ExpectedItemOwnerId == ExpectedInventoryOwnerId &&
            Existing->ExpectedProfileRevision == ExpectedProfileRevision &&
            Existing->ExpectedItemRevision == ExpectedItemRevision;
    }

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!Profile || !Ownership || !Clock || !Clock->IsClockInitialized() ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        MaxActionReceiptsPerProfile <= 0 ||
        Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile)
    {
        return false;
    }

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(ItemId, Item) ||
        Item.Revision != ExpectedItemRevision || Item.Revision >= MAX_int64 - 1 ||
        Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Item.OwnerId != ExpectedInventoryOwnerId || Item.Quantity != 1)
    {
        return false;
    }
    const FWorldSurvivalConsumableDefinition* Definition =
        FindConsumableDefinition(Item.DefinitionId);
    if (!Definition)
        return false;

    const double Now = Clock->GetSimulationTimeSeconds();
    if (!FMath::IsFinite(Now) || Now < 0.0 ||
        !FMath::IsFinite(Item.CreatedAtWorldTimeSeconds) ||
        Item.CreatedAtWorldTimeSeconds < 0.0)
        return false;

    const double Age = FMath::Max(0.0, Now - Item.CreatedAtWorldTimeSeconds);
    const bool bSpoiled = Definition->FreshnessDurationSeconds > 0.0 &&
        Age >= Definition->FreshnessDurationSeconds;
    if (bSpoiled && Definition->SpoiledEnergyFraction <= 0.0f)
        return false;
    const float AppliedCalories = Definition->CaloriesKcal *
        (bSpoiled ? Definition->SpoiledEnergyFraction : 1.0f);
    const float AppliedThirstRelief = Definition->ThirstRelief;
    if (AppliedCalories <= 0.0f && AppliedThirstRelief <= 0.0f)
        return false;

    UInventoryCraftingComponent* Inventory = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        UInventoryCraftingComponent* Candidate =
            IsValid(*It) ? It->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
        if (IsValid(Candidate) && Candidate->GetInventoryOwnerId() == ExpectedInventoryOwnerId)
        {
            if (Inventory)
                return false; // Duplicate inventory owners are ambiguous and fail closed.
            Inventory = Candidate;
        }
    }
    if (!Inventory)
        return false;

    FGuid OwnerActorId;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
    if (InventoryOwnerBefore != ExpectedInventoryOwnerId)
        return false;

    bool bHasExactStack = false;
    for (const FInventoryStack& Stack : InventoryItemsBefore)
    {
        if (Stack.OwnedWorldItemId == ItemId)
        {
            if (Stack.Quantity != 1 || Stack.ItemId.IsNone())
                return false;
            bHasExactStack = true;
            break;
        }
    }
    if (!bHasExactStack)
        return false;

    TArray<FWorldOwnedItemRecord> ItemsBefore;
    TArray<FWorldItemTransferReceipt> TransferReceiptsBefore;
    TArray<FWorldItemSplitReceipt> SplitReceiptsBefore;
    Ownership->ExportSnapshot(ItemsBefore, TransferReceiptsBefore, SplitReceiptsBefore);

    auto RollbackOwnershipAndInventory = [&]() -> bool
    {
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            ItemsBefore, TransferReceiptsBefore, SplitReceiptsBefore);
        if (!bInventoryRestored || !bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Food-consumption rollback failed for item %s."),
                *ItemId.ToString());
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Food consumption rollback failed; stop simulation and recover/reset the world."));
            }
            return false;
        }
        return true;
    };

    FInventoryStack RemovedStack;
    if (!Inventory->TryRemoveOwnedItemReference(ItemId, RemovedStack))
    {
        return false;
    }

    if (!Ownership->TryConsumeItem(OperationId, ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Inventory))
    {
        RollbackOwnershipAndInventory();
        return false;
    }

    FWorldOwnedItemRecord ConsumedItem;
    if (!Ownership->TryGetItem(ItemId, ConsumedItem) ||
        ConsumedItem.OwnerKind != EWorldItemOwnerKind::Consumed)
    {
        RollbackOwnershipAndInventory();
        return false;
    }

    // From here on, all validation/capacity checks have passed. Mutate the
    // survival profile once, then persist a receipt bound to the item identity.
    Profile->CaloriesKcal = FMath::Min(MaxCaloriesKcal, Profile->CaloriesKcal + AppliedCalories);
    Profile->Hunger = 100.0f * (1.0f - Profile->CaloriesKcal / MaxCaloriesKcal);
    Profile->Thirst = FMath::Max(0.0f, Profile->Thirst - AppliedThirstRelief);
    ++Profile->Revision;

    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::ConsumeFoodItem;
    Receipt.ItemId = ItemId;
    Receipt.ExpectedItemOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ExpectedItemRevision = ExpectedItemRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.ResultingItemRevision = ConsumedItem.Revision;
    Receipt.ItemDefinitionId = Item.DefinitionId;
    Receipt.CaloriesKcal = AppliedCalories;
    Receipt.ThirstRelief = AppliedThirstRelief;
    Profile->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldSurvivalSubsystem::TryBeginRest(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    double DurationSeconds)
{
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0 ||
        DurationSeconds > 43200.0)
    {
        return false;
    }
    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        return Existing->Action == EWorldSurvivalActionKind::BeginRest &&
            Existing->EntityId == EntityId &&
            Existing->ExpectedProfileRevision == ExpectedProfileRevision &&
            FMath::IsNearlyEqual(Existing->RequestedDurationSeconds, DurationSeconds, 0.001);
    }

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorldSimulationClockSubsystem* Clock =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!Profile || !Clock || !Clock->IsClockInitialized() ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        MaxActionReceiptsPerProfile <= 0 ||
        Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile)
    {
        return false;
    }
    const double Now = Clock->GetSimulationTimeSeconds();
    const double RestUntil = Now + DurationSeconds;
    if (!FMath::IsFinite(Now) || Now < 0.0 || !FMath::IsFinite(RestUntil) ||
        Profile->RestUntilTimeSeconds > Now)
    {
        return false;
    }

    Profile->RestUntilTimeSeconds = RestUntil;
    ++Profile->Revision;
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::BeginRest;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.RequestedStartTimeSeconds = Now;
    Receipt.RequestedDurationSeconds = DurationSeconds;
    Receipt.ResultingRestUntilTimeSeconds = RestUntil;
    Profile->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldSurvivalSubsystem::UpdateProtection(
    const FGuid& EntityId, float ClothingInsulation, float ShelterProtection, int64 ExpectedRevision)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision != ExpectedRevision || Profile->Revision == MAX_int64 ||
        !InRange(ClothingInsulation, 0.0f, 1.0f) || !InRange(ShelterProtection, 0.0f, 1.0f))
    {
        return false;
    }
    Profile->ClothingInsulation = ClothingInsulation;
    Profile->ShelterProtection = ShelterProtection;
    ++Profile->Revision;
    return true;
}

bool UWorldSurvivalSubsystem::SetWeather(EWorldWeatherState NewWeather, float Severity)
{
    if (static_cast<uint8>(NewWeather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        !InRange(Severity, 0.0f, 1.0f)) return false;
    Weather = NewWeather;
    WeatherSeverity = Severity;
    return true;
}

bool UWorldSurvivalSubsystem::UpdateLocation(const FGuid& EntityId, FVector NewLocation)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || NewLocation.ContainsNaN())
        return false;
    Profile->Location = NewLocation;
    return true;
}

float UWorldSurvivalSubsystem::GetActivityKcalPerHour(ESurvivalActivity Activity) const
{
    switch (Activity)
    {
    case ESurvivalActivity::Walk:  return 200.0f;
    case ESurvivalActivity::Run:   return 600.0f;
    case ESurvivalActivity::Chop:  return 450.0f;
    case ESurvivalActivity::Dig:   return 500.0f;
    case ESurvivalActivity::Build: return 300.0f;
    case ESurvivalActivity::Carry: return 250.0f;
    default:                       return 0.0f;
    }
}

bool UWorldSurvivalSubsystem::RecordActivity(
    const FGuid& EntityId, ESurvivalActivity Activity, float DurationSeconds, float CarriedMassKg)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || static_cast<uint8>(Activity) > static_cast<uint8>(ESurvivalActivity::Carry) ||
        !InRange(DurationSeconds, 0.0f, 3600.0f) || DurationSeconds == 0.0f ||
        !InRange(CarriedMassKg, 0.0f, 1000.0f) || Profile->Revision >= MAX_int64 - 1)
    {
        return false;
    }
    const float Hours = DurationSeconds / 3600.0f;
    const float Kcal = (GetActivityKcalPerHour(Activity) + CarriedMassKg * CarryKcalPerKgHour) * Hours;
    Profile->PendingActivityKcal = FMath::Min(100000.0f, Profile->PendingActivityKcal + Kcal);
    // Hard work drains stamina now; rest recovers it during simulation steps.
    float StaminaPerHour = 30.0f;
    switch (Activity)
    {
    case ESurvivalActivity::Run:   StaminaPerHour = 600.0f; break;
    case ESurvivalActivity::Chop:
    case ESurvivalActivity::Dig:   StaminaPerHour = 300.0f; break;
    case ESurvivalActivity::Build: StaminaPerHour = 150.0f; break;
    case ESurvivalActivity::Carry: StaminaPerHour = 120.0f; break;
    default: break;
    }
    const float StaminaCost = (StaminaPerHour + CarriedMassKg * 6.0f) * Hours;
    Profile->Stamina = FMath::Clamp(Profile->Stamina - StaminaCost, 0.0f, 100.0f);
    ++Profile->Revision;
    return true;
}

bool UWorldSurvivalSubsystem::ConsumeFood(const FGuid& EntityId, float Kcal, float ThirstRelief)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || !InRange(Kcal, 0.0f, 10000.0f) || !InRange(ThirstRelief, 0.0f, 100.0f) ||
        (Kcal == 0.0f && ThirstRelief == 0.0f) || Profile->Revision == MAX_int64)
    {
        return false;
    }
    Profile->CaloriesKcal = FMath::Min(MaxCaloriesKcal, Profile->CaloriesKcal + Kcal);
    Profile->Hunger = 100.0f * (1.0f - Profile->CaloriesKcal / MaxCaloriesKcal);
    Profile->Thirst = FMath::Max(0.0f, Profile->Thirst - ThirstRelief);
    ++Profile->Revision;
    return true;
}

float UWorldSurvivalSubsystem::GetWeatherTemperatureDelta() const
{
    switch (Weather)
    {
    case EWorldWeatherState::Rain:     return -4.0f * WeatherSeverity;
    case EWorldWeatherState::Wind:     return -2.0f * WeatherSeverity;
    case EWorldWeatherState::Snow:     return -14.0f * WeatherSeverity;
    case EWorldWeatherState::Storm:    return -10.0f * WeatherSeverity;
    case EWorldWeatherState::Heatwave: return 18.0f * WeatherSeverity;
    case EWorldWeatherState::Clear:
    default:                           return 0.0f;
    }
}

float UWorldSurvivalSubsystem::GetWindChill() const
{
    switch (Weather)
    {
    case EWorldWeatherState::Wind:  return 8.0f * WeatherSeverity;
    case EWorldWeatherState::Storm: return 10.0f * WeatherSeverity;
    case EWorldWeatherState::Snow:  return 4.0f * WeatherSeverity;
    default:                        return 0.0f;
    }
}

float UWorldSurvivalSubsystem::GetHeatAt(const FVector& Location) const
{
    float Heat = 0.0f;
    for (const TWeakObjectPtr<UHeatSourceComponent>& Source : HeatSources)
    {
        if (Source.IsValid())
            Heat += Source->GetHeatAt(Location);
    }
    return Heat;
}

float UWorldSurvivalSubsystem::ComputeFeltTemperature(const FWorldSurvivalProfile& Profile) const
{
    const float Air = AmbientTemperatureCelsius + GetWeatherTemperatureDelta();
    const float WindChill = GetWindChill() * (1.0f - Profile.ShelterProtection);
    return Air - WindChill + GetHeatAt(Profile.Location) +
        Profile.ClothingInsulation * ClothingWarmthCelsius +
        Profile.ShelterProtection * ShelterWarmthCelsius -
        Profile.Wetness * WetnessPenaltyCelsius;
}

void UWorldSurvivalSubsystem::RegisterHeatSource(UHeatSourceComponent* Source)
{
    if (IsValid(Source))
        HeatSources.AddUnique(Source);
}

void UWorldSurvivalSubsystem::UnregisterHeatSource(UHeatSourceComponent* Source)
{
    HeatSources.RemoveAll([Source](const TWeakObjectPtr<UHeatSourceComponent>& Item)
    {
        return !Item.IsValid() || Item.Get() == Source;
    });
}

bool UWorldSurvivalSubsystem::CanAdvanceSurvivalStep(
    double DeltaSeconds, double LogicalTimeSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        !FMath::IsFinite(LogicalTimeSeconds) || LogicalTimeSeconds < 0.0 ||
        !InRange(WeatherSeverity, 0.0f, 1.0f) ||
        !FMath::IsFinite(HungerPerHour) || HungerPerHour < 0.0f ||
        !FMath::IsFinite(ThirstPerHour) || ThirstPerHour < 0.0f ||
        !FMath::IsFinite(FatiguePerHour) || FatiguePerHour < 0.0f ||
        !FMath::IsFinite(ColdDamagePerHour) || ColdDamagePerHour < 0.0f ||
        !FMath::IsFinite(MaxCaloriesKcal) || MaxCaloriesKcal <= 0.0f ||
        !FMath::IsFinite(BasalKcalPerHour) || BasalKcalPerHour < 0.0f ||
        !FMath::IsFinite(AmbientTemperatureCelsius) ||
        !FMath::IsFinite(RestFatigueRecoveryMultiplier) ||
        RestFatigueRecoveryMultiplier < 0.0f || RestFatigueRecoveryMultiplier > 12.0f ||
        !FMath::IsFinite(RestStaminaRecoveryPerHour) ||
        RestStaminaRecoveryPerHour < 0.0f || RestStaminaRecoveryPerHour > 200.0f ||
        MaxActionReceiptsPerProfile <= 0 || MaxActionReceiptsPerProfile > 4096 ||
        MaxConsumableDefinitions <= 0 || MaxConsumableDefinitions > 4096 ||
        ConsumableDefinitions.Num() > MaxConsumableDefinitions)
        return false;
    TSet<FName> SeenDefinitions;
    for (const FWorldSurvivalConsumableDefinition& Definition : ConsumableDefinitions)
    {
        if (Definition.DefinitionId.IsNone() || SeenDefinitions.Contains(Definition.DefinitionId) ||
            !InRange(Definition.CaloriesKcal, 0.0f, 10000.0f) ||
            !InRange(Definition.ThirstRelief, 0.0f, 100.0f) ||
            (Definition.CaloriesKcal <= 0.0f && Definition.ThirstRelief <= 0.0f) ||
            !FMath::IsFinite(Definition.FreshnessDurationSeconds) ||
            Definition.FreshnessDurationSeconds < 0.0 ||
            !InRange(Definition.SpoiledEnergyFraction, 0.0f, 1.0f))
            return false;
        SeenDefinitions.Add(Definition.DefinitionId);
    }
    if (LogicalTimeSeconds < DeltaSeconds) return false;
    for (const TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
        if (Pair.Value.Revision >= MAX_int64 - 1) return false;
    return true;
}

bool UWorldSurvivalSubsystem::AdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds)
{
    if (!CanAdvanceSurvivalStep(DeltaSeconds, LogicalTimeSeconds)) return false;

    // Fuel burns on the same clock that drives the bodies it warms.
    for (const TWeakObjectPtr<UHeatSourceComponent>& Source : HeatSources)
    {
        if (Source.IsValid())
            Source->AdvanceFuel(DeltaSeconds, Weather, WeatherSeverity);
    }

    const float Hours = static_cast<float>(DeltaSeconds / 3600.0);
    const double PreviousLogicalTimeSeconds = LogicalTimeSeconds - DeltaSeconds;
    if (!FMath::IsFinite(PreviousLogicalTimeSeconds) || PreviousLogicalTimeSeconds < 0.0)
        return false;
    for (TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
    {
        FWorldSurvivalProfile& P = Pair.Value;
        const double RestOverlapSeconds = FMath::Max(0.0,
            FMath::Min(LogicalTimeSeconds, P.RestUntilTimeSeconds) - PreviousLogicalTimeSeconds);
        const float RestHours = static_cast<float>(RestOverlapSeconds / 3600.0);
        P.Thirst = FMath::Clamp(P.Thirst + ThirstPerHour * Hours, 0.0f, 100.0f);
        P.Fatigue = FMath::Clamp(P.Fatigue + FatiguePerHour * Hours -
            FatiguePerHour * RestFatigueRecoveryMultiplier * RestHours, 0.0f, 100.0f);

        // Exposure: one felt temperature drives core temperature and shivering.
        P.FeltTemperatureCelsius = ComputeFeltTemperature(P);
        const float ColdDegrees = FMath::Max(0.0f, ComfortTemperatureCelsius - P.FeltTemperatureCelsius);
        if (ColdDegrees > 0.0f)
        {
            P.CoreTemperatureCelsius = FMath::Max(20.0f,
                P.CoreTemperatureCelsius - ColdDegrees * CoreCoolingPerDegreeHour * Hours);
        }
        else
        {
            P.CoreTemperatureCelsius = FMath::Min(37.0f, P.CoreTemperatureCelsius + CoreRecoveryPerHour * Hours);
        }
        const float HypothermiaDegrees = FMath::Max(0.0f, HypothermiaThresholdCelsius - P.CoreTemperatureCelsius);
        P.Health = FMath::Max(0.0f, P.Health - ColdDamagePerHour * HypothermiaDegrees * Hours);
        if (P.FeltTemperatureCelsius > 32.0f)
            P.Thirst = FMath::Clamp(P.Thirst + (P.FeltTemperatureCelsius - 32.0f) * 0.2f * Hours, 0.0f, 100.0f);

        // Calories: basal metabolism, reported activity and shivering.
        const float BurnKcal = BasalKcalPerHour * Hours + P.PendingActivityKcal +
            ColdDegrees * ShiverKcalPerDegreeHour * Hours;
        P.PendingActivityKcal = 0.0f;
        P.CaloriesKcal = FMath::Clamp(P.CaloriesKcal - BurnKcal, 0.0f, MaxCaloriesKcal);
        P.Hunger = 100.0f * (1.0f - P.CaloriesKcal / MaxCaloriesKcal);

        if (Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm)
            P.Wetness = FMath::Clamp(P.Wetness + WeatherSeverity * (1.0f - P.ShelterProtection) * Hours * 0.2f, 0.0f, 1.0f);
        // Warmth dries clothing faster.
        const float DryingBoost = FMath::Max(0.0f, P.FeltTemperatureCelsius - ComfortTemperatureCelsius) * 0.01f;
        P.Wetness = FMath::Max(0.0f, P.Wetness - Hours * (0.05f + P.ShelterProtection * 0.25f + DryingBoost));

        if (P.CaloriesKcal <= 0.0f || P.Thirst >= 98.0f)
            P.Health = FMath::Max(0.0f, P.Health - Hours * 2.0f);
        P.Stamina = FMath::Clamp(P.Stamina +
            (P.Fatigue < 50.0f ? 2.0f : -2.0f) * Hours +
            RestStaminaRecoveryPerHour * RestHours, 0.0f, 100.0f);
        ++P.Revision;
    }
    return true;
}

bool UWorldSurvivalSubsystem::RecordDiscovery(
    FName DiscoveryId, FVector Location, double LogicalTimeSeconds)
{
    if (DiscoveryId.IsNone() || Location.ContainsNaN() || !FMath::IsFinite(LogicalTimeSeconds) ||
        LogicalTimeSeconds < 0.0)
    {
        return false;
    }
    if (Discoveries.ContainsByPredicate([DiscoveryId](const FWorldDiscoveryRecord& D)
        { return D.DiscoveryId == DiscoveryId; }))
    {
        return true; // discovery is idempotent
    }
    FWorldDiscoveryRecord Record;
    Record.DiscoveryId = DiscoveryId;
    Record.Location = Location;
    Record.DiscoveredAtTime = LogicalTimeSeconds;
    Discoveries.Add(Record);
    return true;
}

bool UWorldSurvivalSubsystem::GetProfile(
    const FGuid& EntityId, FWorldSurvivalProfile& OutProfile) const
{
    const FWorldSurvivalProfile* Found = Profiles.Find(EntityId);
    if (!Found) return false;
    OutProfile = *Found;
    return true;
}

void UWorldSurvivalSubsystem::ExportSnapshot(
    TArray<FWorldSurvivalProfile>& OutProfiles, TArray<FWorldDiscoveryRecord>& OutDiscoveries,
    EWorldWeatherState& OutWeather, float& OutSeverity) const
{
    Profiles.GenerateValueArray(OutProfiles);
    OutProfiles.Sort([](const FWorldSurvivalProfile& A, const FWorldSurvivalProfile& B)
        { return A.EntityId.ToString() < B.EntityId.ToString(); });
    OutDiscoveries = Discoveries;
    OutWeather = Weather;
    OutSeverity = WeatherSeverity;
}

bool UWorldSurvivalSubsystem::RestoreSnapshot(
    const TArray<FWorldSurvivalProfile>& ProfileSnapshot,
    const TArray<FWorldDiscoveryRecord>& DiscoverySnapshot,
    EWorldWeatherState InWeather, float InSeverity)
{
    if (!InRange(InSeverity, 0.0f, 1.0f)) return false;
    TMap<FGuid, FWorldSurvivalProfile> Proposed;
    for (const FWorldSurvivalProfile& P : ProfileSnapshot)
    {
        if (Proposed.Contains(P.EntityId) || !IsValidProfile(P)) return false;
        Proposed.Add(P.EntityId, P);
    }
    TSet<FGuid> SeenActionOperationIds;
    for (const FWorldSurvivalProfile& P : ProfileSnapshot)
    {
        for (const FWorldSurvivalActionReceipt& Receipt : P.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || SeenActionOperationIds.Contains(Receipt.OperationId))
                return false;
            SeenActionOperationIds.Add(Receipt.OperationId);
        }
    }
    TSet<FName> SeenDiscoveries;
    for (const FWorldDiscoveryRecord& D : DiscoverySnapshot)
    {
        if (D.DiscoveryId.IsNone() || SeenDiscoveries.Contains(D.DiscoveryId) ||
            D.Location.ContainsNaN() || !FMath::IsFinite(D.DiscoveredAtTime) ||
            D.DiscoveredAtTime < 0.0) return false;
        SeenDiscoveries.Add(D.DiscoveryId);
    }
    Profiles = MoveTemp(Proposed);
    Discoveries = DiscoverySnapshot;
    Weather = InWeather;
    WeatherSeverity = InSeverity;
    return true;
}
