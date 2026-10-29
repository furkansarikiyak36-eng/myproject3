#include "WorldSurvivalSubsystem.h"
#include "WorldGameDataSubsystem.h"

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
            static_cast<uint8>(Receipt.Action) > static_cast<uint8>(EWorldSurvivalActionKind::BoilWater) ||
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
                !FindConditionDefinition(Receipt.ConditionDefinitionId) ||
                !FMath::IsFinite(Receipt.ConditionEndTimeSeconds) ||
                Receipt.ConditionEndTimeSeconds < 0.0 ||
                !InRange(Receipt.ConditionSeverity, 0.001f, 1.0f))
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
        case EWorldSurvivalActionKind::FillWaterFromCreek:
            if (!Receipt.ItemId.IsValid() || !Receipt.ResultingItemId.IsValid() ||
                Receipt.ItemId == Receipt.ResultingItemId ||
                !Receipt.ExpectedItemOwnerId.IsValid() || Receipt.ExpectedItemRevision <= 0 ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision ||
                Receipt.ItemDefinitionId != FName(TEXT("Pot.Fired")) ||
                Receipt.RequestedLocation.ContainsNaN())
                return false;
            break;
        case EWorldSurvivalActionKind::BoilWater:
        {
            const bool bStartingBoil =
                Receipt.ItemDefinitionId == FName(TEXT("Water.Dirty")) &&
                Receipt.ResultingItemDefinitionId == FName(TEXT("Water.Boiling"));
            const bool bFinishingBoil =
                Receipt.ItemDefinitionId == FName(TEXT("Water.Boiling")) &&
                Receipt.ResultingItemDefinitionId == FName(TEXT("Water.Boiled"));
            if (!Receipt.ItemId.IsValid() || !Receipt.RelatedItemId.IsValid() ||
                Receipt.ItemId == Receipt.RelatedItemId ||
                !Receipt.ExpectedItemOwnerId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ExpectedRelatedItemRevision <= 0 ||
                (!bStartingBoil && !bFinishingBoil) ||
                !FMath::IsFinite(Receipt.RequestedStartTimeSeconds) ||
                Receipt.RequestedStartTimeSeconds < 0.0 ||
                !FMath::IsFinite(Receipt.RequestedDurationSeconds) ||
                Receipt.RequestedDurationSeconds <= 0.0 ||
                Receipt.RequestedDurationSeconds > 86400.0)
                return false;
            break;
        }
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
        !InRange(Definition.ConditionSeverity, 0.001f, 1.0f) ||
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

    // Age follows where the item was kept and how warm it was (P2.4).
    const double Age = Ownership->GetSpoilageAgeSeconds(ItemId);
    if (Age < 0.0)
        return false;
    const bool bSpoiled = Definition->FreshnessDurationSeconds > 0.0 &&
        Age >= Definition->FreshnessDurationSeconds;
    if (bSpoiled && Definition->SpoiledEnergyFraction <= 0.0f)
        return false;
    const float AppliedCalories = Definition->CaloriesKcal *
        (bSpoiled ? Definition->SpoiledEnergyFraction : 1.0f);
    const float AppliedThirstRelief = Definition->ThirstRelief;
    if (AppliedCalories <= 0.0f && AppliedThirstRelief <= 0.0f)
        return false;
    // Dirty water or spoiled food makes the eater ill. Check it before anything
    // changes, so the whole action either happens with its condition or not at all.
    const FName CausedConditionId = (bSpoiled && !Definition->ConditionWhenSpoiledId.IsNone())
        ? Definition->ConditionWhenSpoiledId : Definition->ConditionOnConsumeId;
    const FWorldSurvivalConditionDefinition* CausedCondition = nullptr;
    double CausedConditionEndsAt = 0.0;
    if (!CausedConditionId.IsNone())
    {
        CausedCondition = FindConditionDefinition(CausedConditionId);
        if (!CausedCondition || Profile->Conditions.Num() >= 128)
            return false;
        CausedConditionEndsAt = CausedCondition->DurationSeconds > 0.0 ? Now + CausedCondition->DurationSeconds : 0.0;
        if (CausedCondition->DurationSeconds > 0.0 && (!FMath::IsFinite(CausedConditionEndsAt) || CausedConditionEndsAt <= Now))
            return false;
    }

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
    FWorldSurvivalConditionState CausedState;
    if (CausedCondition)
    {
        CausedState.ConditionId = FGuid::NewGuid();
        CausedState.DefinitionId = CausedCondition->DefinitionId;
        CausedState.Kind = CausedCondition->Kind;
        CausedState.StartedAtWorldTimeSeconds = Now;
        CausedState.EndsAtWorldTimeSeconds = CausedConditionEndsAt;
        CausedState.Severity = Definition->ConditionSeverity;
        CausedState.Revision = 1;
        Profile->Conditions.Add(CausedState);
    }
    ++Profile->Revision;

    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::ConsumeFoodItem;
    if (CausedCondition)
    {
        Receipt.RelatedConditionId = CausedState.ConditionId;
        Receipt.ConditionDefinitionId = CausedState.DefinitionId;
        Receipt.ConditionEndTimeSeconds = CausedConditionEndsAt;
        Receipt.ConditionSeverity = CausedState.Severity;
    }
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

bool UWorldSurvivalSubsystem::TryFillWaterFromCreek(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    const FGuid& ContainerItemId, int64 ExpectedContainerRevision,
    const FGuid& ExpectedInventoryOwnerId, const FVector& CreekLocation, FGuid& OutWaterItemId)
{
    OutWaterItemId.Invalidate();
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        !ContainerItemId.IsValid() || ExpectedContainerRevision <= 0 ||
        !ExpectedInventoryOwnerId.IsValid() || CreekLocation.ContainsNaN())
        return false;

    // A retry resolves to its committed output before current revisions are checked.
    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldSurvivalActionKind::FillWaterFromCreek ||
            Existing->EntityId != EntityId || Existing->ItemId != ContainerItemId ||
            Existing->ExpectedItemOwnerId != ExpectedInventoryOwnerId ||
            Existing->ExpectedProfileRevision != ExpectedProfileRevision ||
            Existing->ExpectedItemRevision != ExpectedContainerRevision ||
            Existing->ItemDefinitionId != FName(TEXT("Pot.Fired")) ||
            FVector::DistSquared(Existing->RequestedLocation, CreekLocation) > 100.0f ||
            !Existing->ResultingItemId.IsValid())
            return false;

        UWorldItemOwnershipSubsystem* ExistingOwnership =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
        FWorldOwnedItemRecord ExistingOutput;
        if (!ExistingOwnership || !ExistingOwnership->TryGetItem(Existing->ResultingItemId, ExistingOutput) ||
            ExistingOutput.BaseDefinitionId != FName(TEXT("Water.Dirty")))
            return false;
        OutWaterItemId = Existing->ResultingItemId;
        return true;
    }

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!Profile || !Ownership || !Clock || !Clock->IsClockInitialized() ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile ||
        MaxActionReceiptsPerProfile <= 0)
        return false;

    // The authored fired pot is the reusable vessel. Filling never consumes it.
    FWorldOwnedItemRecord Container;
    if (!Ownership->TryGetItem(ContainerItemId, Container) ||
        Container.Revision != ExpectedContainerRevision || Container.Revision >= MAX_int64 - 1 ||
        Container.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Container.OwnerId != ExpectedInventoryOwnerId || Container.Quantity != 1 ||
        Container.DefinitionId != FName(TEXT("Pot.Fired")))
        return false;

    // One reusable pot holds one uniquely owned water item at a time. A new
    // request cannot duplicate a filled vessel, but retrying the original
    // operation above still resolves to the same committed water identity.
    for (const FWorldSurvivalActionReceipt& Receipt : Profile->ActionReceipts)
    {
        if (Receipt.Action != EWorldSurvivalActionKind::FillWaterFromCreek ||
            Receipt.ItemId != ContainerItemId || !Receipt.ResultingItemId.IsValid())
            continue;
        FWorldOwnedItemRecord ExistingWater;
        if (Ownership->TryGetItem(Receipt.ResultingItemId, ExistingWater) &&
            ExistingWater.OwnerKind == EWorldItemOwnerKind::Inventory &&
            ExistingWater.OwnerId == ExpectedInventoryOwnerId &&
            ExistingWater.Quantity == 1 &&
            (ExistingWater.DefinitionId == FName(TEXT("Water.Dirty")) ||
             ExistingWater.DefinitionId == FName(TEXT("Water.Boiling")) ||
             ExistingWater.DefinitionId == FName(TEXT("Water.Boiled"))))
            return false;
    }

    UInventoryCraftingComponent* Inventory = nullptr;
    if (!FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory))
        return false;

    FGuid OwnerActorBefore;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(OwnerActorBefore, InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
    if (InventoryOwnerBefore != ExpectedInventoryOwnerId)
        return false;

    bool bFoundReusablePot = false;
    for (const FInventoryStack& Stack : InventoryItemsBefore)
    {
        if (Stack.OwnedWorldItemId != ContainerItemId)
            continue;
        if (Stack.ItemId != FName(TEXT("Pot.Fired")) || Stack.Quantity != 1 || Stack.MaxStack != 1)
            return false;
        bFoundReusablePot = true;
        break;
    }
    FInventoryStack WaterCapacityProbe;
    WaterCapacityProbe.ItemId = FName(TEXT("Water.Dirty"));
    WaterCapacityProbe.Quantity = 1;
    WaterCapacityProbe.MaxStack = 1;
    WaterCapacityProbe.bIsTool = false;
    WaterCapacityProbe.Category = EInventoryItemCategory::Consumable;
    // A filled vessel is a uniquely owned item, not a mergeable stack. Use
    // a temporary unique identity during preflight so an ordinary water stack
    // cannot make a full inventory appear to have spare capacity.
    WaterCapacityProbe.OwnedWorldItemId = FGuid::NewGuid();
    if (!WaterCapacityProbe.OwnedWorldItemId.IsValid())
        return false;
    TArray<FInventoryStack> WaterCapacityProbeItems;
    WaterCapacityProbeItems.Add(WaterCapacityProbe);
    if (!bFoundReusablePot || !Inventory->CanAddItems(WaterCapacityProbeItems))
        return false;

    const double Now = Clock->GetSimulationTimeSeconds();
    if (!FMath::IsFinite(Now) || Now < 0.0 ||
        FVector::DistSquared(Profile->Location, CreekLocation) > FMath::Square(600.0))
        return false;

    TArray<FWorldOwnedItemRecord> ItemsBefore;
    TArray<FWorldItemTransferReceipt> TransferReceiptsBefore;
    TArray<FWorldItemSplitReceipt> SplitReceiptsBefore;
    Ownership->ExportSnapshot(ItemsBefore, TransferReceiptsBefore, SplitReceiptsBefore);

    FWorldOwnedItemRecord WaterItem;
    WaterItem.ItemId = FGuid::NewGuid();
    WaterItem.DefinitionId = FName(TEXT("Water.Dirty"));
    WaterItem.BaseDefinitionId = WaterItem.DefinitionId;
    WaterItem.Quantity = 1;
    WaterItem.OwnerKind = EWorldItemOwnerKind::Inventory;
    WaterItem.OwnerId = ExpectedInventoryOwnerId;
    WaterItem.WorldLocation = CreekLocation;
    WaterItem.CreatedAtWorldTimeSeconds = Now;
    WaterItem.SpoilageAgeSeconds = 0.0;
    WaterItem.SpoilageAgeUpdatedAtSeconds = Now;
    WaterItem.Revision = 1;
    if (!WaterItem.ItemId.IsValid() || !Ownership->RegisterItem(WaterItem))
        return false;

    FInventoryStack WaterStack;
    WaterStack.ItemId = WaterItem.DefinitionId;
    WaterStack.Quantity = 1;
    WaterStack.MaxStack = 1; // A water-filled vessel is a unique physical item.
    WaterStack.bIsTool = false;
    WaterStack.Category = EInventoryItemCategory::Consumable;
    WaterStack.OwnedWorldItemId = WaterItem.ItemId;

    auto Rollback = [&]() -> bool
    {
        // Restore the ownership ledger first; inventory restore validates against it.
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            ItemsBefore, TransferReceiptsBefore, SplitReceiptsBefore);
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        if (!bOwnershipRestored || !bInventoryRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Creek fill rollback failed for output item %s."),
                *WaterItem.ItemId.ToString());
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Creek water fill rollback failed; stop simulation and recover/reset the world."));
            }
            return false;
        }
        return true;
    };

    if (!Inventory->TryAddItem(WaterStack))
    {
        Rollback();
        return false;
    }

    ++Profile->Revision;
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::FillWaterFromCreek;
    Receipt.ItemId = ContainerItemId;
    Receipt.ResultingItemId = WaterItem.ItemId;
    Receipt.ExpectedItemOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ExpectedItemRevision = ExpectedContainerRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    // The reusable pot is unchanged; this field records its unchanged revision.
    Receipt.ResultingItemRevision = ExpectedContainerRevision;
    Receipt.ItemDefinitionId = Container.DefinitionId;
    Receipt.RequestedLocation = CreekLocation;
    Profile->ActionReceipts.Add(Receipt);
    OutWaterItemId = WaterItem.ItemId;
    return true;
}

bool UWorldSurvivalSubsystem::TryBoilWaterAtFire(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    const FGuid& WaterItemId, int64 ExpectedWaterItemRevision,
    const FGuid& ContainerItemId, int64 ExpectedContainerRevision,
    const FGuid& ExpectedInventoryOwnerId, bool& bOutStarted)
{
    bOutStarted = false;
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        !WaterItemId.IsValid() || ExpectedWaterItemRevision <= 0 ||
        ExpectedWaterItemRevision >= MAX_int64 - 1 ||
        !ContainerItemId.IsValid() || ExpectedContainerRevision <= 0 ||
        ExpectedContainerRevision >= MAX_int64 - 1 ||
        !ExpectedInventoryOwnerId.IsValid())
        return false;

    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        const bool bMatches = Existing->Action == EWorldSurvivalActionKind::BoilWater &&
            Existing->EntityId == EntityId && Existing->ItemId == WaterItemId &&
            Existing->RelatedItemId == ContainerItemId &&
            Existing->ExpectedItemOwnerId == ExpectedInventoryOwnerId &&
            Existing->ExpectedProfileRevision == ExpectedProfileRevision &&
            Existing->ExpectedItemRevision == ExpectedWaterItemRevision &&
            Existing->ExpectedRelatedItemRevision == ExpectedContainerRevision;
        bOutStarted = bMatches &&
            Existing->ResultingItemDefinitionId == FName(TEXT("Water.Boiling"));
        return bMatches;
    }

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    if (!Profile || !World || !Ownership || !Clock || !Clock->IsClockInitialized() ||
        !GameData || !GameData->IsApplied() ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        MaxActionReceiptsPerProfile <= 0 ||
        Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile ||
        !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        !GameData->GetItems().Contains(FName(TEXT("Water.Boiling"))))
        return false;

    const FGameDataTransformation* BoilRecipe =
        GameData->FindTransformation(FName(TEXT("boil_water")));
    const FGameDataItem* DirtyItemDefinition =
        GameData->GetItems().Find(FName(TEXT("Water.Dirty")));
    const FGameDataItem* BoiledItemDefinition =
        GameData->GetItems().Find(FName(TEXT("Water.Boiled")));
    const bool bHasDirtyWaterInput = BoilRecipe && BoilRecipe->Inputs.ContainsByPredicate(
        [](const FGameDataItemAmount& Amount)
        {
            return Amount.ItemId == FName(TEXT("Water.Dirty")) && Amount.Quantity == 1;
        });
    const bool bHasBoiledWaterOutput = BoilRecipe && BoilRecipe->Outputs.ContainsByPredicate(
        [](const FGameDataItemAmount& Amount)
        {
            return Amount.ItemId == FName(TEXT("Water.Boiled")) && Amount.Quantity == 1;
        });
    const bool bRequiresReusablePot = BoilRecipe &&
        BoilRecipe->Tools.Contains(FName(TEXT("Pot.Fired")));
    if (!BoilRecipe || BoilRecipe->Where != FName(TEXT("campfire")) ||
        !FMath::IsFinite(BoilRecipe->DurationSeconds) ||
        BoilRecipe->DurationSeconds <= 0.0 || BoilRecipe->DurationSeconds > 86400.0 ||
        !bHasDirtyWaterInput || !bHasBoiledWaterOutput || !bRequiresReusablePot ||
        !DirtyItemDefinition || !BoiledItemDefinition ||
        !FindConsumableDefinition(FName(TEXT("Water.Dirty"))) ||
        !FindConsumableDefinition(FName(TEXT("Water.Boiled"))))
        return false;

    const double Now = Clock->GetSimulationTimeSeconds();
    if (!FMath::IsFinite(Now) || Now < 0.0)
        return false;

    FWorldOwnedItemRecord Water;
    FWorldOwnedItemRecord Container;
    if (!Ownership->TryGetItem(WaterItemId, Water) ||
        Water.Revision != ExpectedWaterItemRevision ||
        Water.Revision >= MAX_int64 - 1 ||
        Water.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Water.OwnerId != ExpectedInventoryOwnerId || Water.Quantity != 1 ||
        !Ownership->TryGetItem(ContainerItemId, Container) ||
        Container.Revision != ExpectedContainerRevision ||
        Container.Revision >= MAX_int64 - 1 ||
        Container.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Container.OwnerId != ExpectedInventoryOwnerId || Container.Quantity != 1 ||
        Container.DefinitionId != FName(TEXT("Pot.Fired")))
        return false;

    int32 WaterStackIndex = INDEX_NONE;
    int32 ContainerStackIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Inventory->Items.Num(); ++Index)
    {
        const FInventoryStack& Stack = Inventory->Items[Index];
        if (Stack.OwnedWorldItemId == WaterItemId)
        {
            if (WaterStackIndex != INDEX_NONE || Stack.ItemId != Water.DefinitionId ||
                Stack.Quantity != 1 || Stack.MaxStack != 1)
                return false;
            WaterStackIndex = Index;
        }
        if (Stack.OwnedWorldItemId == ContainerItemId)
        {
            if (ContainerStackIndex != INDEX_NONE || Stack.ItemId != FName(TEXT("Pot.Fired")) ||
                Stack.Quantity != 1 || Stack.MaxStack != 1)
                return false;
            ContainerStackIndex = Index;
        }
    }
    if (WaterStackIndex == INDEX_NONE || ContainerStackIndex == INDEX_NONE)
        return false;

    bool bAtHotFire = false;
    for (TActorIterator<AActor> It(World); It && !bAtHotFire; ++It)
    {
        if (!IsValid(*It))
            continue;
        TInlineComponentArray<UHeatSourceComponent*> Sources(*It);
        for (const UHeatSourceComponent* Source : Sources)
        {
            if (IsValid(Source) && Source->IsLit() &&
                Source->GetHeatAt(Profile->Location) >= 12.0f)
            {
                bAtHotFire = true;
                break;
            }
        }
    }
    if (!bAtHotFire)
        return false;

    FName ResultDefinitionId = NAME_None;
    double BoilStartTime = Now;
    double BoilDuration = BoilRecipe->DurationSeconds;
    if (Water.DefinitionId == FName(TEXT("Water.Dirty")))
    {
        // This is the beginning of the authored transformation interval.
        ResultDefinitionId = FName(TEXT("Water.Boiling"));
    }
    else if (Water.DefinitionId == FName(TEXT("Water.Boiling")))
    {
        const FWorldSurvivalActionReceipt* StartReceipt =
            Profile->ActionReceipts.FindByPredicate(
                [&WaterItemId, &ExpectedInventoryOwnerId, &ContainerItemId](
                    const FWorldSurvivalActionReceipt& Receipt)
                {
                    return Receipt.Action == EWorldSurvivalActionKind::BoilWater &&
                        Receipt.ItemId == WaterItemId &&
                        Receipt.RelatedItemId == ContainerItemId &&
                        Receipt.ExpectedItemOwnerId == ExpectedInventoryOwnerId &&
                        Receipt.ItemDefinitionId == FName(TEXT("Water.Dirty")) &&
                        Receipt.ResultingItemDefinitionId == FName(TEXT("Water.Boiling"));
                });
        if (!StartReceipt)
            return false; // A transient boiling item without its start receipt is invalid.
        BoilStartTime = StartReceipt->RequestedStartTimeSeconds;
        BoilDuration = StartReceipt->RequestedDurationSeconds;
        if (Now + 0.001 < BoilStartTime + BoilDuration)
            return false;
        ResultDefinitionId = FName(TEXT("Water.Boiled"));
    }
    else
    {
        return false; // Already boiled or not a water state supported by this action.
    }

    FGuid OwnerActorId;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
    if (InventoryOwnerBefore != ExpectedInventoryOwnerId)
        return false;

    TArray<FWorldOwnedItemRecord> OwnershipItemsBefore;
    TArray<FWorldItemTransferReceipt> OwnershipReceiptsBefore;
    TArray<FWorldItemSplitReceipt> OwnershipSplitReceiptsBefore;
    Ownership->ExportSnapshot(OwnershipItemsBefore, OwnershipReceiptsBefore, OwnershipSplitReceiptsBefore);

    auto Rollback = [&]() -> bool
    {
        const bool bInventoryRestored =
            Inventory->RestoreSnapshot(InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        const bool bOwnershipRestored =
            Ownership->RestoreSnapshot(OwnershipItemsBefore, OwnershipReceiptsBefore, OwnershipSplitReceiptsBefore);
        if (!bInventoryRestored || !bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Water-boiling rollback failed for item %s."), *WaterItemId.ToString());
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(
                    TEXT("Water boiling rollback failed; stop simulation and recover/reset the world."));
            return false;
        }
        return true;
    };

    FWorldOwnedItemRecord UpdatedWater;
    if (!Ownership->TryUpdateItemDefinition(OperationId, WaterItemId, ExpectedWaterItemRevision,
        EWorldItemOwnerKind::Inventory, ResultDefinitionId, UpdatedWater))
        return false;

    if (!Inventory->Items.IsValidIndex(WaterStackIndex) ||
        Inventory->Items[WaterStackIndex].OwnedWorldItemId != WaterItemId ||
        Inventory->Items[WaterStackIndex].ItemId != Water.DefinitionId)
    {
        Rollback();
        return false;
    }
    Inventory->Items[WaterStackIndex].ItemId = ResultDefinitionId;

    ++Profile->Revision;
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::BoilWater;
    Receipt.ItemId = WaterItemId;
    Receipt.RelatedItemId = ContainerItemId;
    Receipt.ExpectedItemOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ExpectedItemRevision = ExpectedWaterItemRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.ResultingItemRevision = UpdatedWater.Revision;
    Receipt.ItemDefinitionId = Water.DefinitionId;
    Receipt.ResultingItemDefinitionId = ResultDefinitionId;
    Receipt.ExpectedRelatedItemRevision = ExpectedContainerRevision;
    Receipt.RequestedStartTimeSeconds = BoilStartTime;
    Receipt.RequestedDurationSeconds = BoilDuration;
    Profile->ActionReceipts.Add(Receipt);
    bOutStarted = ResultDefinitionId == FName(TEXT("Water.Boiling"));
    return true;
}

bool UWorldSurvivalSubsystem::TryBoilWaterAtFire(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    const FGuid& WaterItemId, int64 ExpectedWaterItemRevision,
    const FGuid& ContainerItemId, int64 ExpectedContainerRevision,
    const FGuid& ExpectedInventoryOwnerId, bool& bOutStarted)
{
    bOutStarted = false;
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        !WaterItemId.IsValid() || ExpectedWaterItemRevision <= 0 ||
        !ContainerItemId.IsValid() || ExpectedContainerRevision <= 0 ||
        WaterItemId == ContainerItemId || !ExpectedInventoryOwnerId.IsValid())
        return false;

    // A retry after commit must not advance the item a second time.
    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        if (Existing->Action != EWorldSurvivalActionKind::BoilWater ||
            Existing->EntityId != EntityId || Existing->ItemId != WaterItemId ||
            Existing->RelatedItemId != ContainerItemId ||
            Existing->ExpectedItemOwnerId != ExpectedInventoryOwnerId ||
            Existing->ExpectedProfileRevision != ExpectedProfileRevision ||
            Existing->ExpectedItemRevision != ExpectedWaterItemRevision ||
            Existing->ExpectedRelatedItemRevision != ExpectedContainerRevision)
            return false;
        bOutStarted = Existing->ResultingItemDefinitionId == FName(TEXT("Water.Boiling"));
        return true;
    }

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!Profile || !Ownership || !Clock || !Clock->IsClockInitialized() ||
        !GameData || !GameData->IsApplied() ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        MaxActionReceiptsPerProfile <= 0 ||
        Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile)
        return false;

    const FGameDataTransformation* Recipe =
        GameData->FindTransformation(FName(TEXT("boil_water")));
    if (!Recipe || Recipe->Where != FName(TEXT("campfire")) ||
        !FMath::IsFinite(Recipe->DurationSeconds) ||
        Recipe->DurationSeconds <= 0.0 || Recipe->DurationSeconds > 43200.0)
        return false;

    // A heat source must be lit and actually warm the player at the point of use.
    bool bNearLitFire = false;
    for (TActorIterator<AActor> It(World); It && !bNearLitFire; ++It)
    {
        if (!IsValid(*It))
            continue;
        TArray<UHeatSourceComponent*> Sources;
        It->GetComponents<UHeatSourceComponent>(Sources);
        for (const UHeatSourceComponent* Source : Sources)
        {
            if (IsValid(Source) && Source->IsLit() && Source->GetHeatAt(Profile->Location) >= 8.0f)
            {
                bNearLitFire = true;
                break;
            }
        }
    }
    if (!bNearLitFire)
        return false;

    FWorldOwnedItemRecord Water;
    FWorldOwnedItemRecord Container;
    if (!Ownership->TryGetItem(WaterItemId, Water) ||
        Water.Revision != ExpectedWaterItemRevision || Water.Revision >= MAX_int64 - 1 ||
        Water.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Water.OwnerId != ExpectedInventoryOwnerId || Water.Quantity != 1 ||
        (Water.DefinitionId != FName(TEXT("Water.Dirty")) &&
         Water.DefinitionId != FName(TEXT("Water.Boiling"))) ||
        !Ownership->TryGetItem(ContainerItemId, Container) ||
        Container.Revision != ExpectedContainerRevision || Container.Revision >= MAX_int64 - 1 ||
        Container.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Container.OwnerId != ExpectedInventoryOwnerId || Container.Quantity != 1 ||
        Container.DefinitionId != FName(TEXT("Pot.Fired")))
        return false;

    UInventoryCraftingComponent* Inventory = nullptr;
    if (!FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory))
        return false;

    int32 WaterStackIndex = INDEX_NONE;
    bool bFoundContainerStack = false;
    for (int32 Index = 0; Index < Inventory->Items.Num(); ++Index)
    {
        const FInventoryStack& Stack = Inventory->Items[Index];
        if (Stack.OwnedWorldItemId == WaterItemId)
        {
            if (WaterStackIndex != INDEX_NONE || Stack.ItemId != Water.DefinitionId ||
                Stack.Quantity != 1 || Stack.MaxStack != 1)
                return false;
            WaterStackIndex = Index;
        }
        if (Stack.OwnedWorldItemId == ContainerItemId)
        {
            if (Stack.ItemId != FName(TEXT("Pot.Fired")) ||
                Stack.Quantity != 1 || Stack.MaxStack != 1)
                return false;
            bFoundContainerStack = true;
        }
    }
    if (WaterStackIndex == INDEX_NONE || !bFoundContainerStack)
        return false;

    const double Now = Clock->GetSimulationTimeSeconds();
    if (!FMath::IsFinite(Now) || Now < 0.0)
        return false;

    FName NewDefinitionId = NAME_None;
    bool bStarting = false;
    if (Water.DefinitionId == FName(TEXT("Water.Dirty")))
    {
        // First activation starts a persistent processing interval; it does not
        // create another item or consume the reusable pot.
        NewDefinitionId = FName(TEXT("Water.Boiling"));
        bStarting = true;
    }
    else
    {
        if (!FMath::IsFinite(Water.ProcessingStartedAtWorldTimeSeconds) ||
            Water.ProcessingStartedAtWorldTimeSeconds < 0.0 ||
            Now - Water.ProcessingStartedAtWorldTimeSeconds < Recipe->DurationSeconds)
            return false; // UI can tell the player to wait for the authored duration.
        NewDefinitionId = FName(TEXT("Water.Boiled"));
    }

    FWorldOwnedItemRecord UpdatedWater;
    if (!Ownership->TryUpdateItemDefinition(
        OperationId, WaterItemId, Water.Revision, EWorldItemOwnerKind::Inventory,
        NewDefinitionId, UpdatedWater, bStarting ? Now : -1.0))
        return false;

    // The stack is a view of the authoritative ledger definition. Keep the same
    // unique ID, category, slot and weight, merely synchronizing the definition.
    FInventoryStack& WaterStack = Inventory->Items[WaterStackIndex];
    WaterStack.ItemId = NewDefinitionId;
    WaterStack.Category = EInventoryItemCategory::Consumable;
    WaterStack.bIsTool = false;
    WaterStack.Quantity = 1;
    WaterStack.MaxStack = 1;

    ++Profile->Revision;
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::BoilWater;
    Receipt.ItemId = WaterItemId;
    Receipt.ResultingItemId = WaterItemId;
    Receipt.RelatedItemId = ContainerItemId;
    Receipt.ExpectedItemOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ExpectedItemRevision = ExpectedWaterItemRevision;
    Receipt.ExpectedRelatedItemRevision = ExpectedContainerRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.ResultingItemRevision = UpdatedWater.Revision;
    Receipt.ItemDefinitionId = Water.DefinitionId;
    Receipt.ResultingItemDefinitionId = NewDefinitionId;
    Profile->ActionReceipts.Add(Receipt);
    bOutStarted = bStarting;
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

bool UWorldSurvivalSubsystem::UpdateShelterProtection(
    const FGuid& EntityId, float NewShelterProtection, int64 ExpectedRevision)
{
    if (!InRange(NewShelterProtection, 0.0f, 1.0f))
        return false;
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || Profile->Revision != ExpectedRevision ||
        Profile->Revision >= MAX_int64 - 1)
        return false;
    Profile->ShelterProtection = NewShelterProtection;
    ++Profile->Revision;
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

bool UWorldSurvivalSubsystem::ApplyWetnessExposure(const FGuid& EntityId, float WetnessDelta)
{
    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile || !FMath::IsFinite(WetnessDelta) || WetnessDelta <= 0.0f || WetnessDelta > 1.0f)
        return false;
    Profile->Wetness = FMath::Clamp(Profile->Wetness + WetnessDelta, 0.0f, 1.0f);
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

    // Player adapters call this before a costly action. Sprinting stops earlier
    // than ordinary work; severe exhaustion also blocks strenuous actions.
    if (Activity == ESurvivalActivity::Run &&
        (Profile->Stamina <= 20.0f || Profile->Fatigue >= 85.0f))
        return false;
    const bool bStrenuous = Activity == ESurvivalActivity::Chop ||
        Activity == ESurvivalActivity::Dig || Activity == ESurvivalActivity::Build ||
        Activity == ESurvivalActivity::Carry;
    if (bStrenuous && (Profile->Stamina <= 5.0f || Profile->Fatigue >= 95.0f))
        return false;

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

bool UWorldSurvivalSubsystem::RegisterConditionDefinition(
    const FWorldSurvivalConditionDefinition& Definition)
{
    if (Definition.DefinitionId.IsNone() ||
        static_cast<uint8>(Definition.Kind) > static_cast<uint8>(EWorldSurvivalConditionKind::Poisoning) ||
        !InRange(Definition.HealthLossPerHour, 0.0f, 100.0f) ||
        !InRange(Definition.FatiguePerHour, 0.0f, 100.0f) ||
        !InRange(Definition.StaminaLossPerHour, 0.0f, 100.0f) ||
        !FMath::IsFinite(Definition.DurationSeconds) ||
        Definition.DurationSeconds < 0.0 || Definition.DurationSeconds > 604800.0 ||
        !InRange(Definition.TreatmentHealthRecovery, 0.0f, 100.0f) ||
        MaxConditionDefinitions <= 0 || MaxConditionDefinitions > 1024 ||
        ConditionDefinitions.Num() >= MaxConditionDefinitions ||
        FindConditionDefinition(Definition.DefinitionId))
        return false;
    ConditionDefinitions.Add(Definition);
    return true;
}

bool UWorldSurvivalSubsystem::TryApplyCondition(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    FName ConditionDefinitionId, float Severity)
{
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        ConditionDefinitionId.IsNone() || !InRange(Severity, 0.001f, 1.0f))
        return false;

    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        return Existing->Action == EWorldSurvivalActionKind::ApplyCondition &&
            Existing->EntityId == EntityId &&
            Existing->ExpectedProfileRevision == ExpectedProfileRevision &&
            Existing->ConditionDefinitionId == ConditionDefinitionId &&
            FMath::IsNearlyEqual(Existing->ConditionSeverity, Severity, 0.0001f);
    }

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorldSimulationClockSubsystem* Clock =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    const FWorldSurvivalConditionDefinition* Definition =
        FindConditionDefinition(ConditionDefinitionId);
    if (!Profile || !Clock || !Clock->IsClockInitialized() || !Definition ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        Profile->Conditions.Num() >= 128 || Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile)
        return false;

    const double Now = Clock->GetSimulationTimeSeconds();
    const double EndsAt = Definition->DurationSeconds > 0.0 ? Now + Definition->DurationSeconds : 0.0;
    if (!FMath::IsFinite(Now) || Now < 0.0 ||
        (Definition->DurationSeconds > 0.0 && (!FMath::IsFinite(EndsAt) || EndsAt <= Now)))
        return false;

    FWorldSurvivalConditionState Condition;
    Condition.ConditionId = FGuid::NewGuid();
    Condition.DefinitionId = Definition->DefinitionId;
    Condition.Kind = Definition->Kind;
    Condition.StartedAtWorldTimeSeconds = Now;
    Condition.EndsAtWorldTimeSeconds = EndsAt;
    Condition.Severity = Severity;
    Condition.Revision = 1;
    if (!Condition.ConditionId.IsValid())
        return false;

    ++Profile->Revision;
    Profile->Conditions.Add(Condition);
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::ApplyCondition;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.RelatedConditionId = Condition.ConditionId;
    Receipt.ConditionDefinitionId = Definition->DefinitionId;
    Receipt.ConditionEndTimeSeconds = EndsAt;
    Receipt.ConditionSeverity = Severity;
    Profile->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldSurvivalSubsystem::TryTreatCondition(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    const FGuid& ConditionId, const FGuid& TreatmentItemId, int64 ExpectedItemRevision,
    const FGuid& ExpectedInventoryOwnerId)
{
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        !ConditionId.IsValid() || !TreatmentItemId.IsValid() || ExpectedItemRevision <= 0 ||
        !ExpectedInventoryOwnerId.IsValid())
        return false;

    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
    {
        return Existing->Action == EWorldSurvivalActionKind::TreatCondition &&
            Existing->EntityId == EntityId &&
            Existing->ExpectedProfileRevision == ExpectedProfileRevision &&
            Existing->RelatedConditionId == ConditionId &&
            Existing->ItemId == TreatmentItemId &&
            Existing->ExpectedItemRevision == ExpectedItemRevision &&
            Existing->ExpectedItemOwnerId == ExpectedInventoryOwnerId;
    }

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!Profile || !Ownership || !Clock || !Clock->IsClockInitialized() ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile)
        return false;

    const int32 ConditionIndex = Profile->Conditions.IndexOfByPredicate(
        [&ConditionId](const FWorldSurvivalConditionState& Condition)
        { return Condition.ConditionId == ConditionId; });
    if (!Profile->Conditions.IsValidIndex(ConditionIndex))
        return false;
    const FWorldSurvivalConditionState Condition = Profile->Conditions[ConditionIndex];
    const FWorldSurvivalConditionDefinition* Definition =
        FindConditionDefinition(Condition.DefinitionId);
    const double Now = Clock->GetSimulationTimeSeconds();
    if (!Definition || Definition->TreatmentItemDefinitionId.IsNone() ||
        !FMath::IsFinite(Now) || Now < 0.0 ||
        (Condition.EndsAtWorldTimeSeconds > 0.0 && Now >= Condition.EndsAtWorldTimeSeconds))
        return false;

    FWorldOwnedItemRecord TreatmentItem;
    if (!Ownership->TryGetItem(TreatmentItemId, TreatmentItem) ||
        TreatmentItem.Revision != ExpectedItemRevision || TreatmentItem.Revision >= MAX_int64 - 1 ||
        TreatmentItem.OwnerKind != EWorldItemOwnerKind::Inventory ||
        TreatmentItem.OwnerId != ExpectedInventoryOwnerId || TreatmentItem.Quantity != 1 ||
        TreatmentItem.DefinitionId != Definition->TreatmentItemDefinitionId)
        return false;

    UInventoryCraftingComponent* Inventory = nullptr;
    if (!FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory))
        return false;

    FGuid OwnerActorId;
    FGuid PreviousInventoryOwnerId;
    TArray<FInventoryStack> PreviousInventory;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
    if (PreviousInventoryOwnerId != ExpectedInventoryOwnerId ||
        !PreviousInventory.ContainsByPredicate([&TreatmentItemId](const FInventoryStack& Stack)
        { return Stack.OwnedWorldItemId == TreatmentItemId && Stack.Quantity == 1 && !Stack.ItemId.IsNone(); }))
        return false;

    TArray<FWorldOwnedItemRecord> PreviousItems;
    TArray<FWorldItemTransferReceipt> PreviousTransferReceipts;
    TArray<FWorldItemSplitReceipt> PreviousSplitReceipts;
    Ownership->ExportSnapshot(PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);
    auto Rollback = [&]() -> bool
    {
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);
        if (!bInventoryRestored || !bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Condition-treatment rollback failed for item %s."),
                *TreatmentItemId.ToString());
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(
                    TEXT("Treatment rollback failed; stop simulation and recover/reset the world."));
            return false;
        }
        return true;
    };

    FInventoryStack RemovedStack;
    if (!Inventory->TryRemoveOwnedItemReference(TreatmentItemId, RemovedStack))
        return false;
    if (!Ownership->TryConsumeItem(OperationId, TreatmentItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Inventory))
    {
        Rollback();
        return false;
    }

    const float PreviousHealth = Profile->Health;
    Profile->Conditions.RemoveAt(ConditionIndex);
    Profile->Health = FMath::Clamp(Profile->Health + Definition->TreatmentHealthRecovery, 0.0f, 100.0f);
    ++Profile->Revision;
    FWorldOwnedItemRecord ConsumedTreatment;
    if (!Ownership->TryGetItem(TreatmentItemId, ConsumedTreatment) ||
        ConsumedTreatment.OwnerKind != EWorldItemOwnerKind::Consumed)
    {
        // Roll back domain state too; a receipt must never claim a treatment
        // when the canonical item ledger did not commit its consumed state.
        Profile->Conditions.Insert(Condition, ConditionIndex);
        Profile->Health = PreviousHealth;
        --Profile->Revision;
        Rollback();
        return false;
    }

    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::TreatCondition;
    Receipt.ItemId = TreatmentItemId;
    Receipt.ExpectedItemOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ExpectedItemRevision = ExpectedItemRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.ResultingItemRevision = ConsumedTreatment.Revision;
    Receipt.ItemDefinitionId = TreatmentItem.DefinitionId;
    Receipt.RelatedConditionId = ConditionId;
    Receipt.ConditionDefinitionId = Condition.DefinitionId;
    Profile->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldSurvivalSubsystem::RegisterEquipmentDefinition(
    const FWorldSurvivalEquipmentDefinition& Definition)
{
    if (Definition.ItemDefinitionId.IsNone() ||
        static_cast<uint8>(Definition.Slot) > static_cast<uint8>(EWorldEquipmentSlot::Feet) ||
        !InRange(Definition.Insulation, 0.0f, 1.0f) ||
        !InRange(Definition.WaterResistance, 0.0f, 1.0f) ||
        MaxEquipmentDefinitions <= 0 || MaxEquipmentDefinitions > 1024 ||
        EquipmentDefinitions.Num() >= MaxEquipmentDefinitions ||
        FindEquipmentDefinition(Definition.ItemDefinitionId))
        return false;
    TSet<FName> SeenCapabilities;
    for (const FName Capability : Definition.GrantedCapabilityIds)
    {
        if (Capability.IsNone() || SeenCapabilities.Contains(Capability))
            return false;
        SeenCapabilities.Add(Capability);
    }
    EquipmentDefinitions.Add(Definition);
    return true;
}

bool UWorldSurvivalSubsystem::TryEquipItem(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    const FGuid& ItemId, int64 ExpectedItemRevision, const FGuid& ExpectedInventoryOwnerId)
{
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        !ItemId.IsValid() || ExpectedItemRevision <= 0 || !ExpectedInventoryOwnerId.IsValid())
        return false;
    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
        return Existing->Action == EWorldSurvivalActionKind::EquipItem &&
            Existing->EntityId == EntityId && Existing->ItemId == ItemId &&
            Existing->ExpectedItemOwnerId == ExpectedInventoryOwnerId &&
            Existing->ExpectedProfileRevision == ExpectedProfileRevision &&
            Existing->ExpectedItemRevision == ExpectedItemRevision;

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!Profile || !Ownership || Profile->Revision != ExpectedProfileRevision ||
        Profile->Revision >= MAX_int64 - 1 || Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile ||
        Profile->EquippedItems.Num() >= MaxEquippedItemsPerProfile)
        return false;

    FWorldOwnedItemRecord Item;
    const FWorldSurvivalEquipmentDefinition* Definition = nullptr;
    if (!Ownership->TryGetItem(ItemId, Item) || Item.Revision != ExpectedItemRevision ||
        Item.Revision >= MAX_int64 - 1 || Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
        Item.OwnerId != ExpectedInventoryOwnerId || Item.Quantity != 1 ||
        !Item.MeshVariantSetId.IsNone())
        return false;
    Definition = FindEquipmentDefinition(Item.DefinitionId);
    if (!Definition || Profile->EquippedItems.ContainsByPredicate(
        [&Definition](const FWorldEquippedItemState& Equipped) { return Equipped.Slot == Definition->Slot; }) ||
        Profile->EquippedItems.ContainsByPredicate(
        [&ItemId](const FWorldEquippedItemState& Equipped) { return Equipped.ItemId == ItemId; }))
        return false;

    UInventoryCraftingComponent* Inventory = nullptr;
    if (!FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory))
        return false;
    FGuid OwnerActorId;
    FGuid PreviousInventoryOwnerId;
    TArray<FInventoryStack> PreviousInventory;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
    if (PreviousInventoryOwnerId != ExpectedInventoryOwnerId ||
        !PreviousInventory.ContainsByPredicate([&ItemId](const FInventoryStack& Stack)
        { return Stack.OwnedWorldItemId == ItemId && Stack.Quantity == 1 && !Stack.ItemId.IsNone(); }))
        return false;

    TArray<FWorldOwnedItemRecord> PreviousItems;
    TArray<FWorldItemTransferReceipt> PreviousTransferReceipts;
    TArray<FWorldItemSplitReceipt> PreviousSplitReceipts;
    Ownership->ExportSnapshot(PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);

    FInventoryStack RemovedStack;
    if (!Inventory->TryRemoveOwnedItemReference(ItemId, RemovedStack))
        return false;
    FWorldOwnedItemRecord EquippedItem;
    if (!Ownership->TryTransferItem(OperationId, ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::Inventory, EWorldItemOwnerKind::Equipped,
        EntityId, Item.WorldLocation, EquippedItem))
    {
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            PreviousItems, PreviousTransferReceipts, PreviousSplitReceipts);
        if (!bInventoryRestored || !bOwnershipRestored)
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(TEXT("Equipment transfer rollback failed."));
        return false;
    }

    FWorldEquippedItemState Equipped;
    Equipped.ItemId = ItemId;
    Equipped.ItemDefinitionId = Item.DefinitionId;
    Equipped.Slot = Definition->Slot;
    Equipped.ItemRevision = EquippedItem.Revision;
    Profile->EquippedItems.Add(Equipped);
    RecomputeEquipmentProtection(*Profile);
    ++Profile->Revision;
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::EquipItem;
    Receipt.ItemId = ItemId;
    Receipt.ExpectedItemOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ExpectedItemRevision = ExpectedItemRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.ResultingItemRevision = EquippedItem.Revision;
    Receipt.ItemDefinitionId = Item.DefinitionId;
    Receipt.EquipmentSlot = Definition->Slot;
    Profile->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldSurvivalSubsystem::TryUnequipItem(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    EWorldEquipmentSlot Slot, const FGuid& ExpectedInventoryOwnerId)
{
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        static_cast<uint8>(Slot) > static_cast<uint8>(EWorldEquipmentSlot::Feet) ||
        !ExpectedInventoryOwnerId.IsValid())
        return false;
    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
        return Existing->Action == EWorldSurvivalActionKind::UnequipItem &&
            Existing->EntityId == EntityId && Existing->EquipmentSlot == Slot &&
            Existing->ExpectedItemOwnerId == ExpectedInventoryOwnerId &&
            Existing->ExpectedProfileRevision == ExpectedProfileRevision;

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    if (!Profile || !Ownership || !FindInventoryForOwner(ExpectedInventoryOwnerId, Inventory) ||
        Profile->Revision != ExpectedProfileRevision || Profile->Revision >= MAX_int64 - 1 ||
        Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile)
        return false;

    const int32 EquippedIndex = Profile->EquippedItems.IndexOfByPredicate([Slot](const FWorldEquippedItemState& Equipped)
        { return Equipped.Slot == Slot; });
    if (!Profile->EquippedItems.IsValidIndex(EquippedIndex))
        return false;
    const FWorldEquippedItemState Equipped = Profile->EquippedItems[EquippedIndex];

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(Equipped.ItemId, Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::Equipped || Item.OwnerId != EntityId ||
        Item.Revision != Equipped.ItemRevision || Item.Revision >= MAX_int64 - 1 ||
        Item.DefinitionId != Equipped.ItemDefinitionId || Item.Quantity != 1)
        return false;

    FGuid OwnerActorId;
    FGuid PreviousInventoryOwnerId;
    TArray<FInventoryStack> PreviousInventory;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
    if (PreviousInventoryOwnerId != ExpectedInventoryOwnerId)
        return false;
    TArray<FWorldOwnedItemRecord> PreviousItems;
    TArray<FWorldItemTransferReceipt> PreviousTransfers;
    TArray<FWorldItemSplitReceipt> PreviousSplits;
    Ownership->ExportSnapshot(PreviousItems, PreviousTransfers, PreviousSplits);

    FWorldOwnedItemRecord InventoryItem;
    if (!Ownership->TryTransferItem(OperationId, Item.ItemId, Item.Revision,
        EWorldItemOwnerKind::Equipped, EWorldItemOwnerKind::Inventory,
        ExpectedInventoryOwnerId, Item.WorldLocation, InventoryItem))
        return false;

    FInventoryStack Stack;
    Stack.ItemId = InventoryItem.DefinitionId;
    Stack.Quantity = 1;
    Stack.MaxStack = 1;
    Stack.OwnedWorldItemId = InventoryItem.ItemId;
    if (!Inventory->TryAddItem(Stack))
    {
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            PreviousInventoryOwnerId, PreviousInventory, PreviousQuickAccess);
        const bool bOwnershipRestored = Ownership->RestoreSnapshot(
            PreviousItems, PreviousTransfers, PreviousSplits);
        if (!bInventoryRestored || !bOwnershipRestored)
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(TEXT("Equipment unequip rollback failed."));
        return false;
    }

    Profile->EquippedItems.RemoveAt(EquippedIndex);
    RecomputeEquipmentProtection(*Profile);
    ++Profile->Revision;
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::UnequipItem;
    Receipt.ItemId = Item.ItemId;
    Receipt.ExpectedItemOwnerId = ExpectedInventoryOwnerId;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ExpectedItemRevision = Item.Revision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.ResultingItemRevision = InventoryItem.Revision;
    Receipt.ItemDefinitionId = Item.DefinitionId;
    Receipt.EquipmentSlot = Slot;
    Profile->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldSurvivalSubsystem::RegisterDiscoveryDefinition(
    const FWorldDiscoveryDefinition& Definition)
{
    if (Definition.DiscoveryId.IsNone() || MaxDiscoveryDefinitions <= 0 ||
        MaxDiscoveryDefinitions > 4096 || DiscoveryDefinitions.Num() >= MaxDiscoveryDefinitions ||
        MaxDiscoveryRecords <= 0 || MaxDiscoveryRecords > 16384 ||
        FindDiscoveryDefinition(Definition.DiscoveryId) ||
        Definition.PrerequisiteDiscoveryIds.Contains(Definition.DiscoveryId))
        return false;

    TSet<FName> SeenPrerequisites;
    for (const FName Required : Definition.PrerequisiteDiscoveryIds)
    {
        if (Required.IsNone() || SeenPrerequisites.Contains(Required) ||
            !FindDiscoveryDefinition(Required))
            return false;
        SeenPrerequisites.Add(Required);
    }
    TSet<FName> SeenEquipment;
    for (const FName Required : Definition.RequiredEquipmentDefinitionIds)
    {
        if (Required.IsNone() || SeenEquipment.Contains(Required) ||
            !FindEquipmentDefinition(Required))
            return false;
        SeenEquipment.Add(Required);
    }
    TSet<FName> SeenUnlocks;
    for (const FName Unlock : Definition.UnlockedDefinitionIds)
    {
        if (Unlock.IsNone() || SeenUnlocks.Contains(Unlock))
            return false;
        SeenUnlocks.Add(Unlock);
    }
    DiscoveryDefinitions.Add(Definition);
    return true;
}

bool UWorldSurvivalSubsystem::TryRecordConfiguredDiscovery(
    const FGuid& OperationId, const FGuid& EntityId, int64 ExpectedProfileRevision,
    FName DiscoveryId, FVector Location, double LogicalTimeSeconds)
{
    if (!OperationId.IsValid() || !EntityId.IsValid() || ExpectedProfileRevision <= 0 ||
        DiscoveryId.IsNone() || Location.ContainsNaN() || !FMath::IsFinite(LogicalTimeSeconds) ||
        LogicalTimeSeconds < 0.0)
        return false;

    if (const FWorldSurvivalActionReceipt* Existing = FindActionReceipt(OperationId))
        return Existing->Action == EWorldSurvivalActionKind::RecordDiscovery &&
            Existing->EntityId == EntityId && Existing->ExpectedProfileRevision == ExpectedProfileRevision &&
            Existing->DiscoveryId == DiscoveryId &&
            Existing->RequestedLocation.Equals(Location, 0.001f) &&
            FMath::IsNearlyEqual(Existing->RequestedDiscoveryTimeSeconds, LogicalTimeSeconds, 0.001);

    FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    const FWorldDiscoveryDefinition* Definition = FindDiscoveryDefinition(DiscoveryId);
    UWorldSimulationClockSubsystem* Clock =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!Profile || !Definition || !Clock || !Clock->IsClockInitialized() ||
        !FMath::IsNearlyEqual(Clock->GetSimulationTimeSeconds(), LogicalTimeSeconds, 0.001) ||
        Profile->Revision != ExpectedProfileRevision ||
        Profile->Revision >= MAX_int64 - 1 || Profile->ActionReceipts.Num() >= MaxActionReceiptsPerProfile ||
        MaxDiscoveryRecords <= 0 || MaxDiscoveryRecords > 16384)
        return false;

    for (const FName RequiredDiscovery : Definition->PrerequisiteDiscoveryIds)
    {
        if (!Discoveries.ContainsByPredicate([RequiredDiscovery](const FWorldDiscoveryRecord& Record)
            { return Record.DiscoveryId == RequiredDiscovery; }))
            return false;
    }
    for (const FName RequiredEquipment : Definition->RequiredEquipmentDefinitionIds)
    {
        if (!Profile->EquippedItems.ContainsByPredicate([RequiredEquipment](const FWorldEquippedItemState& Equipped)
            { return Equipped.ItemDefinitionId == RequiredEquipment; }))
            return false;
    }

    FWorldDiscoveryRecord* ExistingRecord = Discoveries.FindByPredicate([DiscoveryId](const FWorldDiscoveryRecord& Record)
        { return Record.DiscoveryId == DiscoveryId; });
    if (!ExistingRecord && Discoveries.Num() >= MaxDiscoveryRecords)
        return false;
    if (ExistingRecord)
    {
        // A single-player discovery is globally permanent. Reconciliation may
        // add the authored unlock set when migrating a legacy bare discovery.
        for (const FName Unlock : Definition->UnlockedDefinitionIds)
            if (!ExistingRecord->UnlockedDefinitionIds.Contains(Unlock))
                ExistingRecord->UnlockedDefinitionIds.Add(Unlock);
    }
    else
    {
        FWorldDiscoveryRecord& Record = Discoveries.AddDefaulted_GetRef();
        Record.DiscoveryId = DiscoveryId;
        Record.Location = Location;
        Record.DiscoveredAtTime = LogicalTimeSeconds;
        Record.EntityId = EntityId;
        Record.OperationId = OperationId;
        Record.UnlockedDefinitionIds = Definition->UnlockedDefinitionIds;
    }

    ++Profile->Revision;
    FWorldSurvivalActionReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.EntityId = EntityId;
    Receipt.Action = EWorldSurvivalActionKind::RecordDiscovery;
    Receipt.ExpectedProfileRevision = ExpectedProfileRevision;
    Receipt.ResultingProfileRevision = Profile->Revision;
    Receipt.DiscoveryId = DiscoveryId;
    Receipt.RequestedLocation = Location;
    Receipt.RequestedDiscoveryTimeSeconds = LogicalTimeSeconds;
    Profile->ActionReceipts.Add(Receipt);
    return true;
}

bool UWorldSurvivalSubsystem::IsDefinitionUnlocked(const FGuid& EntityId, FName DefinitionId) const
{
    if (!EntityId.IsValid() || DefinitionId.IsNone())
        return false;
    return Discoveries.ContainsByPredicate([EntityId, DefinitionId](const FWorldDiscoveryRecord& Record)
    {
        return (Record.EntityId.IsValid() ? Record.EntityId == EntityId : true) &&
            Record.UnlockedDefinitionIds.Contains(DefinitionId);
    });
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
    const float FireHeat = GetHeatAt(Profile.Location) *
        (1.0f + FMath::Clamp(ShelterHeatRetention, 0.0f, 3.0f) * FMath::Clamp(Profile.ShelterProtection, 0.0f, 1.0f));
    return Air - WindChill + FireHeat +
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

void UWorldSurvivalSubsystem::CaptureHeatSourceRuntimeState(
    TArray<FWorldHeatSourceRuntimeState>& OutStates) const
{
    OutStates.Reset();
    for (const TWeakObjectPtr<UHeatSourceComponent>& WeakSource : HeatSources)
    {
        UHeatSourceComponent* Source = WeakSource.Get();
        if (!IsValid(Source))
            continue;
        FWorldHeatSourceRuntimeState& State = OutStates.AddDefaulted_GetRef();
        State.Source = Source;
        State.FuelSeconds = Source->GetFuelSeconds();
        State.bLit = Source->IsLit();
        State.FuelHeatMultiplier = Source->GetFuelHeatMultiplier();
    }
}

bool UWorldSurvivalSubsystem::RestoreHeatSourceRuntimeState(
    const TArray<FWorldHeatSourceRuntimeState>& Snapshot)
{
    TSet<UHeatSourceComponent*> CapturedSources;
    for (const FWorldHeatSourceRuntimeState& State : Snapshot)
    {
        UHeatSourceComponent* Source = State.Source.Get();
        if (!IsValid(Source) || CapturedSources.Contains(Source) ||
            !FMath::IsFinite(State.FuelSeconds) || State.FuelSeconds < 0.0f)
            return false;
        CapturedSources.Add(Source);
    }

    int32 CurrentSourceCount = 0;
    for (const TWeakObjectPtr<UHeatSourceComponent>& WeakSource : HeatSources)
    {
        if (!WeakSource.IsValid())
            continue;
        ++CurrentSourceCount;
        if (!CapturedSources.Contains(WeakSource.Get()))
            return false;
    }
    if (CurrentSourceCount != Snapshot.Num())
        return false;

    for (const FWorldHeatSourceRuntimeState& State : Snapshot)
    {
        UHeatSourceComponent* Source = State.Source.Get();
        if (!Source->RestoreSimulationFuelState(State.FuelSeconds, State.bLit, State.FuelHeatMultiplier))
            return false;
    }
    return true;
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
        ConsumableDefinitions.Num() > MaxConsumableDefinitions ||
        MaxConditionDefinitions <= 0 || MaxConditionDefinitions > 1024 ||
        ConditionDefinitions.Num() > MaxConditionDefinitions ||
        MaxEquipmentDefinitions <= 0 || MaxEquipmentDefinitions > 1024 ||
        EquipmentDefinitions.Num() > MaxEquipmentDefinitions ||
        MaxEquippedItemsPerProfile <= 0 || MaxEquippedItemsPerProfile > 5 ||
        MaxDiscoveryDefinitions <= 0 || MaxDiscoveryDefinitions > 4096 ||
        DiscoveryDefinitions.Num() > MaxDiscoveryDefinitions ||
        MaxDiscoveryRecords <= 0 || MaxDiscoveryRecords > 16384 ||
        MaxDiscoveryReceiptsPerProfile <= 0 || MaxDiscoveryReceiptsPerProfile > 8192)
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
            !InRange(Definition.SpoiledEnergyFraction, 0.0f, 1.0f) ||
            !InRange(Definition.ConditionSeverity, 0.001f, 1.0f))
            return false;
        SeenDefinitions.Add(Definition.DefinitionId);
    }

    TSet<FName> SeenConditionDefinitions;
    for (const FWorldSurvivalConditionDefinition& Definition : ConditionDefinitions)
    {
        if (Definition.DefinitionId.IsNone() || SeenConditionDefinitions.Contains(Definition.DefinitionId) ||
            static_cast<uint8>(Definition.Kind) > static_cast<uint8>(EWorldSurvivalConditionKind::Poisoning) ||
            !InRange(Definition.HealthLossPerHour, 0.0f, 100.0f) ||
            !InRange(Definition.FatiguePerHour, 0.0f, 100.0f) ||
            !InRange(Definition.StaminaLossPerHour, 0.0f, 100.0f) ||
            !FMath::IsFinite(Definition.DurationSeconds) ||
            Definition.DurationSeconds < 0.0 || Definition.DurationSeconds > 604800.0 ||
            !InRange(Definition.TreatmentHealthRecovery, 0.0f, 100.0f))
            return false;
        SeenConditionDefinitions.Add(Definition.DefinitionId);
    }

    TSet<FName> SeenEquipmentDefinitions;
    for (const FWorldSurvivalEquipmentDefinition& Definition : EquipmentDefinitions)
    {
        if (Definition.ItemDefinitionId.IsNone() || SeenEquipmentDefinitions.Contains(Definition.ItemDefinitionId) ||
            static_cast<uint8>(Definition.Slot) > static_cast<uint8>(EWorldEquipmentSlot::Feet) ||
            !InRange(Definition.Insulation, 0.0f, 1.0f) ||
            !InRange(Definition.WaterResistance, 0.0f, 1.0f))
            return false;
        SeenEquipmentDefinitions.Add(Definition.ItemDefinitionId);
    }

    TSet<FName> SeenDiscoveryDefinitions;
    for (const FWorldDiscoveryDefinition& Definition : DiscoveryDefinitions)
    {
        if (Definition.DiscoveryId.IsNone() || SeenDiscoveryDefinitions.Contains(Definition.DiscoveryId))
            return false;
        TSet<FName> SeenPrerequisites;
        for (const FName Required : Definition.PrerequisiteDiscoveryIds)
        {
            if (Required.IsNone() || Required == Definition.DiscoveryId ||
                SeenPrerequisites.Contains(Required))
                return false;
            SeenPrerequisites.Add(Required);
        }
        TSet<FName> SeenUnlocks;
        for (const FName Unlock : Definition.UnlockedDefinitionIds)
        {
            if (Unlock.IsNone() || SeenUnlocks.Contains(Unlock))
                return false;
            SeenUnlocks.Add(Unlock);
        }
        SeenDiscoveryDefinitions.Add(Definition.DiscoveryId);
    }

    if (LogicalTimeSeconds < DeltaSeconds) return false;
    for (const TPair<FGuid, FWorldSurvivalProfile>& Pair : Profiles)
        if (Pair.Value.Revision >= MAX_int64 - 1 || !IsValidProfile(Pair.Value)) return false;
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

    // Only world-exposed fuel changes definition. Inventory/storage fuel is
    // protected from weather; current DefinitionId changes while BaseDefinitionId
    // remains the original dry-fuel identity for save/history purposes.
    if (UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr)
    {
        TArray<FWorldOwnedItemRecord> WorldItems;
        Ownership->ExportSnapshot(WorldItems);
        const bool bRainOrStorm = Weather == EWorldWeatherState::Rain ||
            Weather == EWorldWeatherState::Storm;
        for (const FWorldOwnedItemRecord& Item : WorldItems)
        {
            if (Item.OwnerKind != EWorldItemOwnerKind::World ||
                !Item.ItemId.IsValid() || Item.Revision <= 0)
                continue;

            FName DesiredDefinition = NAME_None;
            if (bRainOrStorm && WeatherSeverity >= 0.2f &&
                Item.DefinitionId == FName(TEXT("Firewood")))
            {
                DesiredDefinition = FName(TEXT("Firewood.Wet"));
            }
            else if (!bRainOrStorm && Item.DefinitionId == FName(TEXT("Firewood.Wet")))
            {
                const bool bNearDryingHeat = HeatSources.ContainsByPredicate(
                    [&Item](const TWeakObjectPtr<UHeatSourceComponent>& Source)
                    {
                        return Source.IsValid() &&
                            Source->GetHeatAt(Item.WorldLocation) >= 12.0f;
                    });
                if (bNearDryingHeat)
                    DesiredDefinition = FName(TEXT("Firewood"));
            }

            if (!DesiredDefinition.IsNone())
            {
                FWorldOwnedItemRecord UpdatedItem;
                Ownership->TryUpdateItemDefinition(
                    FGuid::NewGuid(), Item.ItemId, Item.Revision,
                    EWorldItemOwnerKind::World, DesiredDefinition, UpdatedItem);
            }
        }
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
        // Sleeping in the open, cold or wet recovers less than under a roof by a fire.
        const float RestHours = static_cast<float>(RestOverlapSeconds / 3600.0) * GetRestQuality(P);
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
            P.Wetness = FMath::Clamp(P.Wetness + WeatherSeverity *
                (1.0f - P.ShelterProtection) * (1.0f - P.ClothingWaterResistance) *
                Hours * 0.2f, 0.0f, 1.0f);
        // Warmth dries clothing faster.
        const float DryingBoost = FMath::Max(0.0f, P.FeltTemperatureCelsius - ComfortTemperatureCelsius) * 0.01f;
        P.Wetness = FMath::Max(0.0f, P.Wetness - Hours * (0.05f + P.ShelterProtection * 0.25f + DryingBoost));

        // Prolonged wet-cold exposure can cause the configured cold illness.
        // Core temperature must fall first, so a brief splash is not enough.
        const FName ColdIllnessId(TEXT("Illness.Cold"));
        const FWorldSurvivalConditionDefinition* ColdIllness = FindConditionDefinition(ColdIllnessId);
        const bool bAlreadyColdIll = P.Conditions.ContainsByPredicate(
            [LogicalTimeSeconds, &ColdIllnessId](const FWorldSurvivalConditionState& Condition)
            {
                return Condition.DefinitionId == ColdIllnessId &&
                    (Condition.EndsAtWorldTimeSeconds <= 0.0 ||
                     Condition.EndsAtWorldTimeSeconds > LogicalTimeSeconds);
            });
        if (ColdIllness && P.Wetness >= 0.8f &&
            P.FeltTemperatureCelsius < ComfortTemperatureCelsius - 8.0f &&
            P.CoreTemperatureCelsius <= 36.0f && !bAlreadyColdIll &&
            P.Conditions.Num() < 128)
        {
            FWorldSurvivalConditionState Condition;
            Condition.ConditionId = FGuid::NewGuid();
            Condition.DefinitionId = ColdIllness->DefinitionId;
            Condition.Kind = ColdIllness->Kind;
            Condition.StartedAtWorldTimeSeconds = LogicalTimeSeconds;
            Condition.EndsAtWorldTimeSeconds = ColdIllness->DurationSeconds > 0.0
                ? LogicalTimeSeconds + ColdIllness->DurationSeconds : 0.0;
            Condition.Severity = FMath::Clamp(
                (P.Wetness - 0.7f) * 2.0f +
                (ComfortTemperatureCelsius - P.FeltTemperatureCelsius) / 30.0f,
                0.25f, 1.0f);
            Condition.Revision = 1;
            if (Condition.ConditionId.IsValid() &&
                (Condition.EndsAtWorldTimeSeconds == 0.0 ||
                 FMath::IsFinite(Condition.EndsAtWorldTimeSeconds)))
                P.Conditions.Add(Condition);
        }

        if (P.CaloriesKcal <= 0.0f || P.Thirst >= 98.0f)
            P.Health = FMath::Max(0.0f, P.Health - Hours * 2.0f);
        P.Stamina = FMath::Clamp(P.Stamina +
            (P.Fatigue < 50.0f ? 2.0f : -2.0f) * Hours +
            RestStaminaRecoveryPerHour * RestHours, 0.0f, 100.0f);

        for (int32 ConditionIndex = P.Conditions.Num() - 1; ConditionIndex >= 0; --ConditionIndex)
        {
            const FWorldSurvivalConditionState& Condition = P.Conditions[ConditionIndex];
            const FWorldSurvivalConditionDefinition* Definition =
                FindConditionDefinition(Condition.DefinitionId);
            // CanAdvanceSurvivalStep validated the profile and every registered
            // definition before any heat/fuel/profile state changes.
            const double ActiveEnd = Condition.EndsAtWorldTimeSeconds > 0.0
                ? FMath::Min(LogicalTimeSeconds, Condition.EndsAtWorldTimeSeconds)
                : LogicalTimeSeconds;
            const double ActiveStart = FMath::Max(PreviousLogicalTimeSeconds, Condition.StartedAtWorldTimeSeconds);
            const double ActiveSeconds = FMath::Max(0.0, ActiveEnd - ActiveStart);
            const float ConditionHours = static_cast<float>(ActiveSeconds / 3600.0);
            if (Definition && ConditionHours > 0.0f)
            {
                const float Severity = Condition.Severity;
                P.Health = FMath::Max(0.0f,
                    P.Health - Definition->HealthLossPerHour * Severity * ConditionHours);
                P.Fatigue = FMath::Clamp(
                    P.Fatigue + Definition->FatiguePerHour * Severity * ConditionHours, 0.0f, 100.0f);
                P.Stamina = FMath::Clamp(
                    P.Stamina - Definition->StaminaLossPerHour * Severity * ConditionHours, 0.0f, 100.0f);
            }
            if (Condition.EndsAtWorldTimeSeconds > 0.0 &&
                Condition.EndsAtWorldTimeSeconds <= LogicalTimeSeconds)
                P.Conditions.RemoveAt(ConditionIndex);
        }
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

bool UWorldSurvivalSubsystem::HasEquippedCapability(
    const FGuid& EntityId, FName CapabilityId) const
{
    if (!EntityId.IsValid() || CapabilityId.IsNone())
        return false;
    const FWorldSurvivalProfile* Profile = Profiles.Find(EntityId);
    if (!Profile)
        return false;
    for (const FWorldEquippedItemState& Equipped : Profile->EquippedItems)
    {
        const FWorldSurvivalEquipmentDefinition* Definition =
            FindEquipmentDefinition(Equipped.ItemDefinitionId);
        if (Definition && Definition->Slot == Equipped.Slot &&
            Definition->GrantedCapabilityIds.Contains(CapabilityId))
            return true;
    }
    return false;
}

bool UWorldSurvivalSubsystem::RestoreSnapshot(
    const TArray<FWorldSurvivalProfile>& ProfileSnapshot,
    const TArray<FWorldDiscoveryRecord>& DiscoverySnapshot,
    EWorldWeatherState InWeather, float InSeverity)
{
    if (!InRange(InSeverity, 0.0f, 1.0f) ||
        static_cast<uint8>(InWeather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        DiscoverySnapshot.Num() > MaxDiscoveryRecords ||
        MaxDiscoveryRecords <= 0)
        return false;

    TMap<FGuid, FWorldSurvivalProfile> Proposed;
    for (const FWorldSurvivalProfile& P : ProfileSnapshot)
    {
        if (Proposed.Contains(P.EntityId) || !IsValidProfile(P))
            return false;
        Proposed.Add(P.EntityId, P);
    }

    TSet<FGuid> SeenActionOperationIds;
    bool bNeedsOwnershipValidation = false;
    for (const FWorldSurvivalProfile& P : ProfileSnapshot)
    {
        for (const FWorldSurvivalActionReceipt& Receipt : P.ActionReceipts)
        {
            if (!Receipt.OperationId.IsValid() || SeenActionOperationIds.Contains(Receipt.OperationId))
                return false;
            SeenActionOperationIds.Add(Receipt.OperationId);
            if (Receipt.Action == EWorldSurvivalActionKind::ConsumeFoodItem ||
                Receipt.Action == EWorldSurvivalActionKind::TreatCondition ||
                Receipt.Action == EWorldSurvivalActionKind::EquipItem ||
                Receipt.Action == EWorldSurvivalActionKind::UnequipItem ||
                Receipt.Action == EWorldSurvivalActionKind::FillWaterFromCreek)
                bNeedsOwnershipValidation = true;
        }
        if (!P.EquippedItems.IsEmpty())
            bNeedsOwnershipValidation = true;
    }

    TSet<FName> SeenDiscoveries;
    for (const FWorldDiscoveryRecord& D : DiscoverySnapshot)
    {
        if (D.DiscoveryId.IsNone() || SeenDiscoveries.Contains(D.DiscoveryId) ||
            D.Location.ContainsNaN() || !FMath::IsFinite(D.DiscoveredAtTime) ||
            D.DiscoveredAtTime < 0.0 ||
            (D.EntityId.IsValid() != D.OperationId.IsValid()))
            return false;

        TSet<FName> SeenUnlocks;
        for (const FName Unlock : D.UnlockedDefinitionIds)
        {
            if (Unlock.IsNone() || SeenUnlocks.Contains(Unlock))
                return false;
            SeenUnlocks.Add(Unlock);
        }

        if (D.OperationId.IsValid())
        {
            const FWorldDiscoveryDefinition* Definition = FindDiscoveryDefinition(D.DiscoveryId);
            if (!Definition || Definition->UnlockedDefinitionIds != D.UnlockedDefinitionIds)
                return false;
            for (const FName Prerequisite : Definition->PrerequisiteDiscoveryIds)
            {
                if (!DiscoverySnapshot.ContainsByPredicate([Prerequisite](const FWorldDiscoveryRecord& Candidate)
                    { return Candidate.DiscoveryId == Prerequisite; }))
                    return false;
            }
        }
        SeenDiscoveries.Add(D.DiscoveryId);
    }

    // Validate saved equipment ownership and all item-bound action receipts
    // before mutating the live profile/discovery maps.
    if (bNeedsOwnershipValidation)
    {
        UWorldItemOwnershipSubsystem* Ownership =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
        if (!Ownership)
            return false;

        for (const FWorldSurvivalProfile& P : ProfileSnapshot)
        {
            for (const FWorldEquippedItemState& Equipped : P.EquippedItems)
            {
                FWorldOwnedItemRecord Item;
                if (!Ownership->TryGetItem(Equipped.ItemId, Item) ||
                    Item.OwnerKind != EWorldItemOwnerKind::Equipped || Item.OwnerId != P.EntityId ||
                    Item.DefinitionId != Equipped.ItemDefinitionId ||
                    Item.Revision < Equipped.ItemRevision)
                    return false;
            }

            for (const FWorldSurvivalActionReceipt& Receipt : P.ActionReceipts)
            {
                if (Receipt.Action != EWorldSurvivalActionKind::ConsumeFoodItem &&
                    Receipt.Action != EWorldSurvivalActionKind::TreatCondition &&
                    Receipt.Action != EWorldSurvivalActionKind::EquipItem &&
                    Receipt.Action != EWorldSurvivalActionKind::UnequipItem &&
                    Receipt.Action != EWorldSurvivalActionKind::FillWaterFromCreek)
                    continue;
                FWorldOwnedItemRecord Item;
                if (!Ownership->TryGetItem(Receipt.ItemId, Item) ||
                    Item.DefinitionId != Receipt.ItemDefinitionId ||
                    Item.Revision < Receipt.ResultingItemRevision)
                    return false;
                if (Receipt.Action == EWorldSurvivalActionKind::FillWaterFromCreek)
                {
                    FWorldOwnedItemRecord Water;
                    if (!Receipt.ResultingItemId.IsValid() ||
                        !Ownership->TryGetItem(Receipt.ResultingItemId, Water) ||
                        Water.BaseDefinitionId != FName(TEXT("Water.Dirty")))
                        return false;
                }
                if ((Receipt.Action == EWorldSurvivalActionKind::ConsumeFoodItem ||
                     Receipt.Action == EWorldSurvivalActionKind::TreatCondition) &&
                    Item.OwnerKind != EWorldItemOwnerKind::Consumed)
                    return false;
                if (Receipt.Action == EWorldSurvivalActionKind::EquipItem &&
                    Item.Revision == Receipt.ResultingItemRevision &&
                    Item.OwnerKind != EWorldItemOwnerKind::Equipped)
                    return false;
            }
        }
    }

    for (const FWorldSurvivalProfile& P : ProfileSnapshot)
    {
        for (const FWorldSurvivalActionReceipt& Receipt : P.ActionReceipts)
        {
            if (Receipt.Action != EWorldSurvivalActionKind::RecordDiscovery)
                continue;
            const bool bDiscoveryPresent = DiscoverySnapshot.ContainsByPredicate(
                [&Receipt, &P](const FWorldDiscoveryRecord& D)
                {
                    return D.DiscoveryId == Receipt.DiscoveryId &&
                        (!D.EntityId.IsValid() || D.EntityId == P.EntityId);
                });
            if (!bDiscoveryPresent)
                return false;
        }
    }

    Profiles = MoveTemp(Proposed);
    Discoveries = DiscoverySnapshot;
    Weather = InWeather;
    WeatherSeverity = InSeverity;
    return true;
}

float UWorldSurvivalSubsystem::GetRestQuality(const FWorldSurvivalProfile& Profile) const
{
    const float Shelter = 0.5f + 0.5f * FMath::Clamp(Profile.ShelterProtection, 0.0f, 1.0f);
    // FeltTemperatureCelsius is the last step's value: what the sleeper actually felt.
    const float Warmth = Profile.FeltTemperatureCelsius >= ComfortTemperatureCelsius - 8.0f ? 1.0f : 0.5f;
    const float Dryness = Profile.Wetness > 0.5f ? 0.75f : 1.0f;
    return Shelter * Warmth * Dryness;
}
