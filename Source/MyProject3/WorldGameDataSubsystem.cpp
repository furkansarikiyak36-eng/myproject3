#include "WorldGameDataSubsystem.h"

#include "WorldAgricultureSubsystem.h"

#include "HeatSourceComponent.h"
#include "InventoryCraftingComponent.h"
#include "WorldProductionSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldTerrainStateSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    using FJson = TSharedPtr<FJsonObject>;

    bool ReadJson(const FString& Directory, const TCHAR* FileName, FJson& Out, FString& Error)
    {
        FString Text;
        const FString Path = FPaths::Combine(Directory, FileName);
        if (!FFileHelper::LoadFileToString(Text, *Path))
        {
            Error = FString::Printf(TEXT("Cannot read %s"), *Path);
            return false;
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
        {
            Error = FString::Printf(TEXT("Bad JSON in %s"), FileName);
            return false;
        }
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ArrayField(const FJson& Object, const TCHAR* Name)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        return Object.IsValid() && Object->TryGetArrayField(Name, Values) ? Values : nullptr;
    }

    FName NameField(const FJson& Object, const TCHAR* Name)
    {
        FString Value;
        return Object.IsValid() && Object->TryGetStringField(Name, Value) && !Value.IsEmpty() ? FName(*Value) : NAME_None;
    }

    double NumberField(const FJson& Object, const TCHAR* Name, double Default = 0.0)
    {
        double Value = Default;
        return Object.IsValid() && Object->TryGetNumberField(Name, Value) ? Value : Default;
    }

    bool BoolField(const FJson& Object, const TCHAR* Name)
    {
        bool Value = false;
        return Object.IsValid() && Object->TryGetBoolField(Name, Value) && Value;
    }

    TArray<FGameDataItemAmount> Amounts(const FJson& Object, const TCHAR* Name)
    {
        TArray<FGameDataItemAmount> Result;
        if (const TArray<TSharedPtr<FJsonValue>>* Values = ArrayField(Object, Name))
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const FJson Entry = Value->AsObject();
                FGameDataItemAmount Amount;
                Amount.ItemId = NameField(Entry, TEXT("item"));
                Amount.Quantity = static_cast<int32>(NumberField(Entry, TEXT("qty"), 1.0));
                Result.Add(Amount);
            }
        return Result;
    }

    bool IsSpecialPlace(FName Where)
    {
        return Where == TEXT("hand") || Where == TEXT("world") || Where == TEXT("log_system") || Where == TEXT("terrain");
    }

    bool ToSlot(FName Slot, EWorldEquipmentSlot& Out)
    {
        static const TMap<FName, EWorldEquipmentSlot> Slots = {
            { TEXT("Head"), EWorldEquipmentSlot::Head }, { TEXT("Body"), EWorldEquipmentSlot::Body },
            { TEXT("Hands"), EWorldEquipmentSlot::Hands }, { TEXT("Legs"), EWorldEquipmentSlot::Legs },
            { TEXT("Feet"), EWorldEquipmentSlot::Feet } };
        const EWorldEquipmentSlot* Found = Slots.Find(Slot);
        if (Found)
            Out = *Found;
        return Found != nullptr;
    }

    bool ToConditionKind(FName Kind, EWorldSurvivalConditionKind& Out)
    {
        if (Kind == TEXT("Injury")) { Out = EWorldSurvivalConditionKind::Injury; return true; }
        if (Kind == TEXT("Illness")) { Out = EWorldSurvivalConditionKind::Illness; return true; }
        if (Kind == TEXT("Poisoning")) { Out = EWorldSurvivalConditionKind::Poisoning; return true; }
        return false;
    }

    EInventoryItemCategory ToCategory(FName Category)
    {
        if (Category == TEXT("Tool")) return EInventoryItemCategory::Tool;
        if (Category == TEXT("Weapon")) return EInventoryItemCategory::Weapon;
        if (Category == TEXT("BuildingPart")) return EInventoryItemCategory::BuildingPart;
        if (Category == TEXT("Consumable")) return EInventoryItemCategory::Consumable;
        if (Category == TEXT("Protection")) return EInventoryItemCategory::Protection;
        return EInventoryItemCategory::Resource;
    }
}

bool UWorldGameDataSubsystem::Fail(const FString& Message)
{
    LastError = Message;
    UE_LOG(LogTemp, Warning, TEXT("Game data: %s"), *Message);
    return false;
}

bool UWorldGameDataSubsystem::LoadDefault()
{
    return LoadFromDirectory(FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("GameData")));
}

bool UWorldGameDataSubsystem::LoadFromDirectory(const FString& Directory)
{
    FString Error;
    FJson ItemsJson, StationsJson, TransformationsJson, ConditionsJson, CropsJson, ZonesJson, DiscoveriesJson, EventsJson, CalendarJson, ClimateJson, LayersJson;
    if (!ReadJson(Directory, TEXT("items.json"), ItemsJson, Error) ||
        !ReadJson(Directory, TEXT("stations.json"), StationsJson, Error) ||
        !ReadJson(Directory, TEXT("transformations.json"), TransformationsJson, Error) ||
        !ReadJson(Directory, TEXT("conditions.json"), ConditionsJson, Error) ||
        !ReadJson(Directory, TEXT("crops.json"), CropsJson, Error) ||
        !ReadJson(Directory, TEXT("zones.json"), ZonesJson, Error) ||
        !ReadJson(Directory, TEXT("discoveries.json"), DiscoveriesJson, Error) ||
        !ReadJson(Directory, TEXT("events.json"), EventsJson, Error) ||
        !ReadJson(Directory, TEXT("calendar.json"), CalendarJson, Error) ||
        !ReadJson(Directory, TEXT("climate.json"), ClimateJson, Error) ||
        !ReadJson(Directory, TEXT("layers.json"), LayersJson, Error))
        return Fail(Error);

    // Parse into locals; the subsystem changes only when everything is valid.
    TMap<FName, FGameDataItem> NewItems;
    TArray<FConsumable> NewConsumables;
    TArray<FFuel> NewFuels;
    TArray<FEquipment> NewEquipment;
    const TArray<TSharedPtr<FJsonValue>>* ItemValues = ArrayField(ItemsJson, TEXT("items"));
    if (!ItemValues)
        return Fail(TEXT("items.json has no items"));
    for (const TSharedPtr<FJsonValue>& Value : *ItemValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataItem Item;
        Item.Id = NameField(Entry, TEXT("id"));
        Item.Group = NameField(Entry, TEXT("group"));
        Entry->TryGetStringField(TEXT("name"), Item.Name);
        Item.WeightGrams = static_cast<int32>(NumberField(Entry, TEXT("weight_g")));
        Item.Category = NameField(Entry, TEXT("category"));
        Item.bPhysical = BoolField(Entry, TEXT("physical"));
        Item.ToolStrength = Item.Category == FName(TEXT("Tool"))
            ? static_cast<float>(NumberField(Entry, TEXT("tool_strength"), 1.0))
            : 0.0f;
        if (Item.Id.IsNone() || NewItems.Contains(Item.Id) || Item.WeightGrams <= 0)
            return Fail(FString::Printf(TEXT("Bad or duplicate item %s"), *Item.Id.ToString()));
        if (Item.Category == FName(TEXT("Tool")) &&
            (!FMath::IsFinite(Item.ToolStrength) ||
             Item.ToolStrength < 0.1f || Item.ToolStrength > 10.0f))
            return Fail(FString::Printf(TEXT("Tool %s has invalid tool_strength (expected 0.1..10)."),
                *Item.Id.ToString()));
        const TSharedPtr<FJsonObject>* Sub = nullptr;
        if (Entry->TryGetObjectField(TEXT("consumable"), Sub))
        {
            FConsumable C;
            C.ItemId = Item.Id;
            C.Kcal = NumberField(*Sub, TEXT("kcal"));
            C.Thirst = NumberField(*Sub, TEXT("thirst"));
            C.FreshnessSeconds = NumberField(*Sub, TEXT("freshness_h")) * 3600.0;
            C.SpoiledFraction = NumberField(*Sub, TEXT("spoiled_fraction"));
            C.Condition = NameField(*Sub, TEXT("condition"));
            C.SpoiledCondition = NameField(*Sub, TEXT("spoiled_condition"));
            C.Severity = NumberField(*Sub, TEXT("severity"), 0.5);
            NewConsumables.Add(C);
            Item.bConsumable = true;
        }
        if (Entry->TryGetObjectField(TEXT("fuel"), Sub))
        {
            NewFuels.Add({ Item.Id, static_cast<float>(NumberField(*Sub, TEXT("burn_s"))), static_cast<float>(NumberField(*Sub, TEXT("heat"), 1.0)) });
            Item.bFuel = true;
        }
        if (Entry->TryGetObjectField(TEXT("equipment"), Sub))
        {
            NewEquipment.Add({ Item.Id, NameField(*Sub, TEXT("slot")),
                static_cast<float>(NumberField(*Sub, TEXT("insulation"))), static_cast<float>(NumberField(*Sub, TEXT("water_resistance"))) });
            Item.bEquipment = true;
        }
        NewItems.Add(Item.Id, Item);
    }

    TMap<FName, FGameDataStation> NewStations;
    if (const TArray<TSharedPtr<FJsonValue>>* Values = ArrayField(StationsJson, TEXT("stations")))
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const FJson Entry = Value->AsObject();
            FGameDataStation Station;
            Station.Id = NameField(Entry, TEXT("id"));
            Entry->TryGetStringField(TEXT("name"), Station.Name);
            Station.Capacity = static_cast<int32>(NumberField(Entry, TEXT("capacity"), 16.0));
            Station.bNeedsFire = BoolField(Entry, TEXT("needs_fire"));
            Station.bNeedsHotFire = BoolField(Entry, TEXT("needs_hot_fire"));
            Station.SpoilageRate = NumberField(Entry, TEXT("spoilage_rate"), 1.0);
            if (Station.Id.IsNone() || NewStations.Contains(Station.Id) || IsSpecialPlace(Station.Id) ||
                Station.Capacity <= 0 || Station.SpoilageRate < 0.0f || Station.SpoilageRate > 4.0f)
                return Fail(FString::Printf(TEXT("Bad or duplicate station %s"), *Station.Id.ToString()));
            NewStations.Add(Station.Id, Station);
        }

    auto HasItem = [&NewItems](FName Id) { return NewItems.Contains(Id); };

    TArray<FGameDataTransformation> NewTransformations;
    TSet<FName> TransformationIds;
    const TArray<TSharedPtr<FJsonValue>>* TransformationValues = ArrayField(TransformationsJson, TEXT("transformations"));
    if (!TransformationValues)
        return Fail(TEXT("transformations.json has no transformations"));
    for (const TSharedPtr<FJsonValue>& Value : *TransformationValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataTransformation T;
        T.PlanNumber = static_cast<int32>(NumberField(Entry, TEXT("n")));
        T.Id = NameField(Entry, TEXT("id"));
        Entry->TryGetStringField(TEXT("name"), T.Name);
        T.bPhysical = NameField(Entry, TEXT("kind")) == TEXT("F");
        T.Where = NameField(Entry, TEXT("where"));
        T.Inputs = Amounts(Entry, TEXT("inputs"));
        T.Outputs = Amounts(Entry, TEXT("outputs"));
        if (const TArray<TSharedPtr<FJsonValue>>* Tools = ArrayField(Entry, TEXT("tools")))
            for (const TSharedPtr<FJsonValue>& Tool : *Tools)
                T.Tools.Add(FName(*Tool->AsString()));
        T.DurationSeconds = NumberField(Entry, TEXT("duration_s"));
        if (T.Id.IsNone() || TransformationIds.Contains(T.Id) || T.PlanNumber < 1 || T.DurationSeconds < 0.0 ||
            (!IsSpecialPlace(T.Where) && !NewStations.Contains(T.Where)))
            return Fail(FString::Printf(TEXT("Bad transformation %s (where %s)"), *T.Id.ToString(), *T.Where.ToString()));
        for (const FGameDataItemAmount& Amount : T.Inputs)
            if (!HasItem(Amount.ItemId) || Amount.Quantity <= 0)
                return Fail(FString::Printf(TEXT("Transformation %s uses unknown input %s"), *T.Id.ToString(), *Amount.ItemId.ToString()));
        for (const FGameDataItemAmount& Amount : T.Outputs)
            if (!HasItem(Amount.ItemId) || Amount.Quantity <= 0)
                return Fail(FString::Printf(TEXT("Transformation %s makes unknown output %s"), *T.Id.ToString(), *Amount.ItemId.ToString()));
        for (const FName Tool : T.Tools)
            if (!HasItem(Tool))
                return Fail(FString::Printf(TEXT("Transformation %s needs unknown tool %s"), *T.Id.ToString(), *Tool.ToString()));
        TransformationIds.Add(T.Id);
        NewTransformations.Add(T);
    }

    TArray<FCondition> NewConditions;
    TSet<FName> ConditionIds;
    if (const TArray<TSharedPtr<FJsonValue>>* Values = ArrayField(ConditionsJson, TEXT("conditions")))
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const FJson Entry = Value->AsObject();
            FCondition C;
            C.Id = NameField(Entry, TEXT("id"));
            C.Kind = NameField(Entry, TEXT("kind"));
            C.HealthLossPerHour = NumberField(Entry, TEXT("health_loss_h"));
            C.FatiguePerHour = NumberField(Entry, TEXT("fatigue_h"));
            C.StaminaLossPerHour = NumberField(Entry, TEXT("stamina_loss_h"));
            C.DurationSeconds = NumberField(Entry, TEXT("duration_h")) * 3600.0;
            C.Treatment = NameField(Entry, TEXT("treatment"));
            C.TreatmentRecovery = NumberField(Entry, TEXT("treatment_recovery"));
            EWorldSurvivalConditionKind Kind;
            if (C.Id.IsNone() || ConditionIds.Contains(C.Id) || !ToConditionKind(C.Kind, Kind) ||
                (!C.Treatment.IsNone() && !HasItem(C.Treatment)))
                return Fail(FString::Printf(TEXT("Bad condition %s"), *C.Id.ToString()));
            ConditionIds.Add(C.Id);
            NewConditions.Add(C);
        }
    for (const FConsumable& C : NewConsumables)
        if ((!C.Condition.IsNone() && !ConditionIds.Contains(C.Condition)) ||
            (!C.SpoiledCondition.IsNone() && !ConditionIds.Contains(C.SpoiledCondition)))
            return Fail(FString::Printf(TEXT("Consumable %s names an unknown condition"), *C.ItemId.ToString()));
    for (const FEquipment& E : NewEquipment)
    {
        EWorldEquipmentSlot Slot;
        if (!ToSlot(E.Slot, Slot))
            return Fail(FString::Printf(TEXT("Equipment %s has an unknown slot"), *E.ItemId.ToString()));
    }

    TArray<FGameDataZone> NewZones;
    const TArray<TSharedPtr<FJsonValue>>* ZoneValues = ArrayField(ZonesJson, TEXT("zones"));
    if (!ZoneValues || ZoneValues->IsEmpty())
        return Fail(TEXT("zones.json needs at least one zone"));
    TSet<FName> ZoneIds;
    TSet<int32> ZoneIndices;
    for (const TSharedPtr<FJsonValue>& Value : *ZoneValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataZone Zone;
        Zone.Id = NameField(Entry, TEXT("id"));
        Entry->TryGetStringField(TEXT("name"), Zone.Name);
        Zone.Index = static_cast<int32>(NumberField(Entry, TEXT("index"), -1.0));
        Zone.Center.X = NumberField(Entry, TEXT("center_x"));
        Zone.Center.Y = NumberField(Entry, TEXT("center_y"));
        Zone.Center.Z = NumberField(Entry, TEXT("center_z"));
        Zone.RadiusCm = static_cast<float>(NumberField(Entry, TEXT("radius_cm"), 1000.0));
        Entry->TryGetStringField(TEXT("obstacle"), Zone.Obstacle);
        Entry->TryGetStringField(TEXT("solution"), Zone.PermanentSolution);
        if (const TArray<TSharedPtr<FJsonValue>>* Resources = ArrayField(Entry, TEXT("resources")))
        {
            for (const TSharedPtr<FJsonValue>& Resource : *Resources)
            {
                if (!Resource.IsValid() || Resource->Type != EJson::String)
                    return Fail(FString::Printf(TEXT("Zone %s contains a non-string resource"),
                        *Zone.Id.ToString()));
                const FName ResourceId(*Resource->AsString());
                if (ResourceId.IsNone() || Zone.ResourceItemIds.Contains(ResourceId))
                    return Fail(FString::Printf(TEXT("Zone %s has an empty or duplicate resource"),
                        *Zone.Id.ToString()));
                if (!NewItems.Contains(ResourceId))
                    return Fail(FString::Printf(TEXT("Zone %s names unknown resource item %s"),
                        *Zone.Id.ToString(), *ResourceId.ToString()));
                Zone.ResourceItemIds.Add(ResourceId);
            }
        }
        if (Zone.Id.IsNone() || ZoneIds.Contains(Zone.Id) || Zone.Name.IsEmpty() ||
            Zone.Index < 0 || Zone.Index > 4 || ZoneIndices.Contains(Zone.Index) ||
            !FMath::IsFinite(Zone.Center.X) || !FMath::IsFinite(Zone.Center.Y) ||
            !FMath::IsFinite(Zone.Center.Z) ||
            !FMath::IsFinite(Zone.RadiusCm) || Zone.RadiusCm < 100.0f ||
            Zone.RadiusCm > 50000.0f || Zone.ResourceItemIds.IsEmpty() ||
            Zone.Obstacle.IsEmpty() || Zone.PermanentSolution.IsEmpty())
            return Fail(FString::Printf(TEXT("Bad or duplicate zone %s"), *Zone.Id.ToString()));
        ZoneIds.Add(Zone.Id);
        ZoneIndices.Add(Zone.Index);
        NewZones.Add(Zone);
    }

    TArray<FGameDataCrop> NewCrops;
    if (const TArray<TSharedPtr<FJsonValue>>* Values = ArrayField(CropsJson, TEXT("crops")))
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const FJson Entry = Value->AsObject();
            FGameDataCrop Crop;
            Crop.Id = NameField(Entry, TEXT("id"));
            Crop.SeedItemId = NameField(Entry, TEXT("seed"));
            Crop.HarvestItemId = NameField(Entry, TEXT("harvest"));
            Crop.HarvestUnits = static_cast<int32>(NumberField(Entry, TEXT("units"), 1.0));
            Crop.SeedReturnUnits = static_cast<int32>(NumberField(Entry, TEXT("seed_return_units"), 1.0));
            Crop.SecondsPerStage = NumberField(Entry, TEXT("hours_per_stage"), 1.0) * 3600.0;
            Crop.FoundInZone = static_cast<int32>(NumberField(Entry, TEXT("found_in_zone")));
            if (const TArray<TSharedPtr<FJsonValue>>* Months = ArrayField(Entry, TEXT("plant_months")))
                for (const TSharedPtr<FJsonValue>& Month : *Months)
                    Crop.PlantMonths.Add(static_cast<int32>(Month->AsNumber()));
            if (Crop.Id.IsNone() || !HasItem(Crop.SeedItemId) || !HasItem(Crop.HarvestItemId) ||
                Crop.HarvestUnits <= 0 || Crop.HarvestUnits > 64 ||
                Crop.SeedReturnUnits < 0 || Crop.SeedReturnUnits > 64 ||
                Crop.HarvestUnits + Crop.SeedReturnUnits > 64 ||
                Crop.SecondsPerStage <= 0.0 || Crop.PlantMonths.Num() == 0)
                return Fail(FString::Printf(TEXT("Bad crop %s"), *Crop.Id.ToString()));
            NewCrops.Add(Crop);
        }

    for (const FGameDataCrop& Crop : NewCrops)
    {
        if (!ZoneIndices.Contains(Crop.FoundInZone))
            return Fail(FString::Printf(TEXT("Crop %s names unknown zone index %d"),
                *Crop.Id.ToString(), Crop.FoundInZone));
    }

    // Natural-event contracts are data-driven and validated before replacing live data.
    TArray<FGameDataWorldEvent> NewWorldEvents;
    TSet<FName> WorldEventIds;
    const TArray<TSharedPtr<FJsonValue>>* EventValues = ArrayField(EventsJson, TEXT("events"));
    if (!EventValues)
        return Fail(TEXT("events.json has no events"));
    for (const TSharedPtr<FJsonValue>& Value : *EventValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataWorldEvent Event;
        Event.Id = NameField(Entry, TEXT("id"));
        Entry->TryGetStringField(TEXT("name"), Event.Name);
        Event.Trigger = NameField(Entry, TEXT("trigger"));
        Event.Signal = NameField(Entry, TEXT("signal"));
        Event.Effect = NameField(Entry, TEXT("effect"));
        Event.WarningSeconds = NumberField(Entry, TEXT("warning_h")) * 3600.0;
        Event.DurationSeconds = NumberField(Entry, TEXT("duration_h")) * 3600.0;
        Event.TriggerMinimum = static_cast<float>(NumberField(Entry, TEXT("trigger_min"), 0.0));
        Event.TriggerMaximum = static_cast<float>(NumberField(Entry, TEXT("trigger_max"), 1.0));
        Event.MinimumSeverity = static_cast<float>(NumberField(Entry, TEXT("min_severity")));
        Event.MaximumSeverity = static_cast<float>(NumberField(Entry, TEXT("max_severity"), 1.0));
        Event.CounterItemId = NameField(Entry, TEXT("counter_item"));
        Event.CounterQuantity = static_cast<int32>(NumberField(Entry, TEXT("counter_qty")));
        if (Event.Id.IsNone() || WorldEventIds.Contains(Event.Id) || Event.Name.IsEmpty() ||
            Event.Trigger.IsNone() || Event.Signal.IsNone() || Event.Effect.IsNone() ||
            !FMath::IsFinite(Event.WarningSeconds) || Event.WarningSeconds < 0.0 ||
            !FMath::IsFinite(Event.DurationSeconds) || Event.DurationSeconds <= 0.0 ||
            !FMath::IsFinite(Event.TriggerMinimum) || !FMath::IsFinite(Event.TriggerMaximum) ||
            !FMath::IsFinite(Event.MinimumSeverity) || !FMath::IsFinite(Event.MaximumSeverity) ||
            Event.TriggerMinimum < 0.0f || Event.TriggerMaximum > 1.0f ||
            Event.TriggerMinimum > Event.TriggerMaximum ||
            Event.MinimumSeverity < 0.0f || Event.MaximumSeverity > 1.0f ||
            Event.MinimumSeverity > Event.MaximumSeverity ||
            ((!Event.CounterItemId.IsNone()) &&
                (!HasItem(Event.CounterItemId) || Event.CounterQuantity <= 0)) ||
            (Event.CounterItemId.IsNone() && Event.CounterQuantity != 0))
        {
            return Fail(FString::Printf(TEXT("Bad world event %s"), *Event.Id.ToString()));
        }
        WorldEventIds.Add(Event.Id);
        NewWorldEvents.Add(Event);
    }
    if (NewWorldEvents.IsEmpty())
        return Fail(TEXT("events.json needs at least one event"));

    TArray<FGameDataDiscovery> NewDiscoveries;
    TSet<FName> DiscoveryIds;
    const TArray<TSharedPtr<FJsonValue>>* DiscoveryValues = ArrayField(DiscoveriesJson, TEXT("discoveries"));
    if (!DiscoveryValues || DiscoveryValues->IsEmpty())
        return Fail(TEXT("discoveries.json needs at least one discovery"));

    for (const TSharedPtr<FJsonValue>& Value : *DiscoveryValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataDiscovery Discovery;
        Discovery.Id = NameField(Entry, TEXT("id"));
        Entry->TryGetStringField(TEXT("name"), Discovery.Name);
        Discovery.ZoneId = NameField(Entry, TEXT("zone"));
        Discovery.Location.X = NumberField(Entry, TEXT("location_x"));
        Discovery.Location.Y = NumberField(Entry, TEXT("location_y"));
        Discovery.Location.Z = NumberField(Entry, TEXT("location_z"));
        Discovery.RevealRadiusCm = static_cast<float>(NumberField(Entry, TEXT("reveal_radius_cm"), 300.0));
        Discovery.RewardItemId = NameField(Entry, TEXT("reward_item"));
        Discovery.RewardQuantity = static_cast<int32>(NumberField(Entry, TEXT("reward_qty"), 0.0));

        auto ReadNameArray = [&Entry](const TCHAR* FieldName, TArray<FName>& OutNames) -> bool
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Entry->TryGetArrayField(FieldName, Values))
                return false;
            TSet<FName> Seen;
            for (const TSharedPtr<FJsonValue>& ArrayValue : *Values)
            {
                if (!ArrayValue.IsValid() || ArrayValue->Type != EJson::String)
                    return false;
                const FName Id(*ArrayValue->AsString());
                if (Id.IsNone() || Seen.Contains(Id))
                    return false;
                Seen.Add(Id);
                OutNames.Add(Id);
            }
            return true;
        };
        if (!ReadNameArray(TEXT("prerequisites"), Discovery.PrerequisiteDiscoveryIds) ||
            !ReadNameArray(TEXT("required_equipment"), Discovery.RequiredEquipmentDefinitionIds) ||
            !ReadNameArray(TEXT("unlocks"), Discovery.UnlockedDefinitionIds))
            return Fail(FString::Printf(TEXT("Discovery %s has malformed name arrays"),
                *Discovery.Id.ToString()));

        const FGameDataZone* Zone = NewZones.FindByPredicate(
            [&Discovery](const FGameDataZone& Candidate) { return Candidate.Id == Discovery.ZoneId; });
        const double DistanceSquared = Zone
            ? FVector::DistSquared2D(Zone->Center, Discovery.Location)
            : TNumericLimits<double>::Max();
        if (Discovery.Id.IsNone() || DiscoveryIds.Contains(Discovery.Id) ||
            Discovery.Name.IsEmpty() || !Zone ||
            !FMath::IsFinite(Discovery.Location.X) || !FMath::IsFinite(Discovery.Location.Y) ||
            !FMath::IsFinite(Discovery.Location.Z) ||
            !FMath::IsFinite(Discovery.RevealRadiusCm) ||
            Discovery.RevealRadiusCm < 50.0f || Discovery.RevealRadiusCm > 2500.0f ||
            DistanceSquared > FMath::Square(static_cast<double>(Zone->RadiusCm)) ||
            Discovery.RewardQuantity < 0 || Discovery.RewardQuantity > 64 ||
            (Discovery.RewardItemId.IsNone() != (Discovery.RewardQuantity == 0)) ||
            (!Discovery.RewardItemId.IsNone() && !NewItems.Contains(Discovery.RewardItemId)))
            return Fail(FString::Printf(TEXT("Bad or duplicate discovery %s"), *Discovery.Id.ToString()));

        for (const FName EquipmentId : Discovery.RequiredEquipmentDefinitionIds)
        {
            if (!NewEquipment.ContainsByPredicate(
                [&EquipmentId](const FEquipment& Equipment) { return Equipment.ItemId == EquipmentId; }))
                return Fail(FString::Printf(TEXT("Discovery %s requires unknown equipment %s"),
                    *Discovery.Id.ToString(), *EquipmentId.ToString()));
        }
        for (const FName UnlockId : Discovery.UnlockedDefinitionIds)
        {
            if (!NewItems.Contains(UnlockId) && !TransformationIds.Contains(UnlockId))
                return Fail(FString::Printf(TEXT("Discovery %s unlocks unknown definition %s"),
                    *Discovery.Id.ToString(), *UnlockId.ToString()));
        }
        DiscoveryIds.Add(Discovery.Id);
        NewDiscoveries.Add(Discovery);
    }

    // Require every prerequisite to exist and order definitions topologically,
    // because UWorldSurvivalSubsystem registers prerequisite definitions first.
    for (const FGameDataDiscovery& Discovery : NewDiscoveries)
    {
        for (const FName Required : Discovery.PrerequisiteDiscoveryIds)
        {
            if (Required == Discovery.Id || !DiscoveryIds.Contains(Required))
                return Fail(FString::Printf(TEXT("Discovery %s has unknown/self prerequisite %s"),
                    *Discovery.Id.ToString(), *Required.ToString()));
        }
    }
    TArray<FGameDataDiscovery> TopologicalDiscoveries;
    TSet<FName> RegisteredDiscoveryIds;
    while (TopologicalDiscoveries.Num() < NewDiscoveries.Num())
    {
        bool bAddedInPass = false;
        for (const FGameDataDiscovery& Discovery : NewDiscoveries)
        {
            if (RegisteredDiscoveryIds.Contains(Discovery.Id))
                continue;
            bool bPrerequisitesMet = true;
            for (const FName Required : Discovery.PrerequisiteDiscoveryIds)
                if (!RegisteredDiscoveryIds.Contains(Required))
                    bPrerequisitesMet = false;
            if (!bPrerequisitesMet)
                continue;
            RegisteredDiscoveryIds.Add(Discovery.Id);
            TopologicalDiscoveries.Add(Discovery);
            bAddedInPass = true;
        }
        if (!bAddedInPass)
            return Fail(TEXT("discoveries.json contains a prerequisite cycle"));
    }

    FWorldCalendarConfig NewCalendar;
    NewCalendar.SecondsPerDay = NumberField(CalendarJson, TEXT("seconds_per_day"), NewCalendar.SecondsPerDay);
    NewCalendar.DaysPerMonth = static_cast<int32>(NumberField(CalendarJson, TEXT("days_per_month"), NewCalendar.DaysPerMonth));
    NewCalendar.StartMonth = static_cast<int32>(NumberField(CalendarJson, TEXT("start_month"), NewCalendar.StartMonth));
    NewCalendar.StartDayOfMonth = static_cast<int32>(NumberField(CalendarJson, TEXT("start_day_of_month"), NewCalendar.StartDayOfMonth));
    NewCalendar.StartHourOfDay = NumberField(CalendarJson, TEXT("start_hour"), NewCalendar.StartHourOfDay);
    if (!WorldCalendar::IsValidConfig(NewCalendar))
        return Fail(TEXT("calendar.json is not a valid calendar"));

    TArray<FWorldMonthClimate> NewMonths;
    NewMonths.SetNum(12);
    TSet<int32> SeenMonths;
    if (const TArray<TSharedPtr<FJsonValue>>* Values = ArrayField(ClimateJson, TEXT("months")))
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const FJson Entry = Value->AsObject();
            const int32 Month = static_cast<int32>(NumberField(Entry, TEXT("month")));
            if (Month < 1 || Month > 12 || SeenMonths.Contains(Month))
                return Fail(TEXT("climate.json has a bad or repeated month"));
            FWorldMonthClimate& M = NewMonths[Month - 1];
            M.MinTemperatureCelsius = NumberField(Entry, TEXT("min_c"));
            M.MaxTemperatureCelsius = NumberField(Entry, TEXT("max_c"));
            M.PrecipitationChance = NumberField(Entry, TEXT("wet"));
            M.StormChance = NumberField(Entry, TEXT("storm"));
            M.WindChance = NumberField(Entry, TEXT("wind"));
            M.HeatwaveChance = NumberField(Entry, TEXT("heatwave"));
            if (M.MinTemperatureCelsius > M.MaxTemperatureCelsius)
                return Fail(TEXT("climate.json has min above max"));
            SeenMonths.Add(Month);
        }
    if (SeenMonths.Num() != 12)
        return Fail(TEXT("climate.json needs all 12 months"));

    TArray<FLayer> NewLayers;
    if (const TArray<TSharedPtr<FJsonValue>>* Values = ArrayField(LayersJson, TEXT("layers")))
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const FJson Entry = Value->AsObject();
            FLayer L;
            L.Material = NameField(Entry, TEXT("material"));
            L.ItemId = NameField(Entry, TEXT("item"));
            L.MinLayer = static_cast<int32>(NumberField(Entry, TEXT("min_layer")));
            L.MaxLayer = static_cast<int32>(NumberField(Entry, TEXT("max_layer")));
            L.UnitsPerCubicMetre = NumberField(Entry, TEXT("units_per_m3"), 1.0);
            if (L.Material.IsNone() || !HasItem(L.ItemId) || L.MinLayer < 0 || L.MaxLayer < L.MinLayer || L.UnitsPerCubicMetre <= 0.0f)
                return Fail(FString::Printf(TEXT("Bad layer %s"), *L.Material.ToString()));
            NewLayers.Add(L);
        }

    Items = MoveTemp(NewItems);
    Stations = MoveTemp(NewStations);
    Transformations = MoveTemp(NewTransformations);
    Conditions = MoveTemp(NewConditions);
    Consumables = MoveTemp(NewConsumables);
    Fuels = MoveTemp(NewFuels);
    Equipment = MoveTemp(NewEquipment);
    Crops = MoveTemp(NewCrops);
    Zones = MoveTemp(NewZones);
    Discoveries = MoveTemp(TopologicalDiscoveries);
    WorldEvents = MoveTemp(NewWorldEvents);
    Calendar = NewCalendar;
    ClimateMonths = MoveTemp(NewMonths);
    Layers = MoveTemp(NewLayers);
    LastError.Reset();
    bLoaded = true;
    bApplied = false;
    return true;
}


const FGameDataZone* UWorldGameDataSubsystem::FindZone(FName ZoneId) const
{
    if (ZoneId.IsNone())
        return nullptr;
    return Zones.FindByPredicate([ZoneId](const FGameDataZone& Zone)
    {
        return Zone.Id == ZoneId;
    });
}

const FGameDataZone* UWorldGameDataSubsystem::FindZoneAtLocation(const FVector& WorldLocation) const
{
    if (!FMath::IsFinite(WorldLocation.X) || !FMath::IsFinite(WorldLocation.Y) ||
        !FMath::IsFinite(WorldLocation.Z))
        return nullptr;

    const FGameDataZone* Best = nullptr;
    double BestNormalizedDistance = TNumericLimits<double>::Max();
    for (const FGameDataZone& Zone : Zones)
    {
        const double DistanceSquared = FVector::DistSquared2D(WorldLocation, Zone.Center);
        const double RadiusSquared = FMath::Square(static_cast<double>(Zone.RadiusCm));
        if (DistanceSquared > RadiusSquared)
            continue;
        const double NormalizedDistance = DistanceSquared / RadiusSquared;
        if (NormalizedDistance < BestNormalizedDistance)
        {
            Best = &Zone;
            BestNormalizedDistance = NormalizedDistance;
        }
    }
    return Best;
}

bool UWorldGameDataSubsystem::GetToolStrength(FName ItemId, float& OutStrength) const
{
    OutStrength = 0.0f;
    if (!bLoaded || ItemId.IsNone())
        return false;
    const FGameDataItem* Item = Items.Find(ItemId);
    if (!Item || Item->Category != FName(TEXT("Tool")) ||
        !FMath::IsFinite(Item->ToolStrength) || Item->ToolStrength < 0.1f ||
        Item->ToolStrength > 10.0f)
        return false;
    OutStrength = Item->ToolStrength;
    return true;
}

const FGameDataTransformation* UWorldGameDataSubsystem::FindTransformation(FName Id) const
{
    return Transformations.FindByPredicate([Id](const FGameDataTransformation& T) { return T.Id == Id; });
}

bool UWorldGameDataSubsystem::ApplyToWorld()
{
    UWorld* World = GetWorld();
    if (!bLoaded || bApplied || !World)
        return false;
    UWorldProductionSubsystem* Production = World->GetSubsystem<UWorldProductionSubsystem>();
    UWorldSurvivalSubsystem* Survival = World->GetSubsystem<UWorldSurvivalSubsystem>();
    UWorldSimulationClockSubsystem* Clock = World->GetSubsystem<UWorldSimulationClockSubsystem>();
    UWorldClimateSubsystem* Climate = World->GetSubsystem<UWorldClimateSubsystem>();
    UWorldTerrainStateSubsystem* Terrain = World->GetSubsystem<UWorldTerrainStateSubsystem>();
    UWorldAgricultureSubsystem* Agriculture = World->GetSubsystem<UWorldAgricultureSubsystem>();
    if (!Production || !Survival || !Clock || !Climate || !Terrain || !Agriculture)
        return Fail(TEXT("A world subsystem is missing"));

    // Station recipes. A tool is put into the station and comes back with the output.
    for (const FGameDataTransformation& T : Transformations)
    {
        if (IsSpecialPlace(T.Where) || T.Inputs.Num() == 0)
            continue;
        FWorldProductionRecipe Recipe;
        Recipe.RecipeId = T.Id;
        Recipe.RequiredStationTag = T.Where;
        Recipe.DurationSeconds = FMath::Max(1.0, T.DurationSeconds);
        auto Ingredient = [](FName ItemId, int32 Quantity)
        {
            FProductionIngredient Result;
            Result.ItemId = ItemId;
            Result.Quantity = Quantity;
            return Result;
        };
        for (const FGameDataItemAmount& Amount : T.Inputs)
            Recipe.Inputs.Add(Ingredient(Amount.ItemId, Amount.Quantity));
        for (const FGameDataItemAmount& Amount : T.Outputs)
            Recipe.Outputs.Add(Ingredient(Amount.ItemId, Amount.Quantity));
        for (const FName Tool : T.Tools)
        {
            Recipe.Inputs.Add(Ingredient(Tool, 1));
            Recipe.Outputs.Add(Ingredient(Tool, 1));
        }
        if (!Production->RegisterRecipe(Recipe))
            return Fail(FString::Printf(TEXT("Production refused recipe %s"), *T.Id.ToString()));
    }

    for (const FCondition& C : Conditions)
    {
        FWorldSurvivalConditionDefinition Definition;
        Definition.DefinitionId = C.Id;
        ToConditionKind(C.Kind, Definition.Kind);
        Definition.HealthLossPerHour = C.HealthLossPerHour;
        Definition.FatiguePerHour = C.FatiguePerHour;
        Definition.StaminaLossPerHour = C.StaminaLossPerHour;
        Definition.DurationSeconds = C.DurationSeconds;
        Definition.TreatmentItemDefinitionId = C.Treatment;
        Definition.TreatmentHealthRecovery = C.TreatmentRecovery;
        if (!Survival->RegisterConditionDefinition(Definition))
            return Fail(FString::Printf(TEXT("Survival refused condition %s"), *C.Id.ToString()));
    }
    for (const FConsumable& C : Consumables)
    {
        FWorldSurvivalConsumableDefinition Definition;
        Definition.DefinitionId = C.ItemId;
        Definition.CaloriesKcal = C.Kcal;
        Definition.ThirstRelief = C.Thirst;
        Definition.FreshnessDurationSeconds = C.FreshnessSeconds;
        Definition.SpoiledEnergyFraction = C.SpoiledFraction;
        Definition.ConditionOnConsumeId = C.Condition;
        Definition.ConditionWhenSpoiledId = C.SpoiledCondition;
        Definition.ConditionSeverity = C.Severity;
        if (!Survival->RegisterConsumableDefinition(Definition))
            return Fail(FString::Printf(TEXT("Survival refused consumable %s"), *C.ItemId.ToString()));
    }
    for (const FEquipment& E : Equipment)
    {
        FWorldSurvivalEquipmentDefinition Definition;
        Definition.ItemDefinitionId = E.ItemId;
        ToSlot(E.Slot, Definition.Slot);
        Definition.Insulation = E.Insulation;
        Definition.WaterResistance = E.WaterResistance;
        if (!Survival->RegisterEquipmentDefinition(Definition))
            return Fail(FString::Printf(TEXT("Survival refused equipment %s"), *E.ItemId.ToString()));
    }

    for (const FGameDataDiscovery& Discovery : Discoveries)
    {
        FWorldDiscoveryDefinition Definition;
        Definition.DiscoveryId = Discovery.Id;
        Definition.PrerequisiteDiscoveryIds = Discovery.PrerequisiteDiscoveryIds;
        Definition.RequiredEquipmentDefinitionIds = Discovery.RequiredEquipmentDefinitionIds;
        Definition.UnlockedDefinitionIds = Discovery.UnlockedDefinitionIds;
        if (!Survival->RegisterDiscoveryDefinition(Definition))
            return Fail(FString::Printf(TEXT("Survival refused discovery %s"),
                *Discovery.Id.ToString()));
    }

    Clock->CalendarConfig = Calendar;
    Climate->Months = ClimateMonths;

    // The crop list is data, not a placeholder registration hidden in a demo actor.
    for (const FGameDataCrop& C : Crops)
    {
        FWorldCropDefinition Definition;
        Definition.CropDefinitionId = C.Id;
        Definition.SeedDefinitionId = C.SeedItemId;
        Definition.HarvestDefinitionId = C.HarvestItemId;
        Definition.HarvestUnits = C.HarvestUnits;
        Definition.SeedReturnUnits = C.SeedReturnUnits;
        Definition.SecondsPerStage = C.SecondsPerStage;
        Definition.PlantMonths = C.PlantMonths;
        Definition.FoundInZone = C.FoundInZone;
        if (!Agriculture->RegisterCropDefinition(Definition))
            return Fail(FString::Printf(TEXT("Agriculture refused crop %s"), *C.Id.ToString()));
    }

    for (const FLayer& L : Layers)
    {
        FWorldTerrainResourceYieldRule Rule;
        Rule.SourceMaterialId = L.Material;
        Rule.OutputDefinitionId = L.ItemId;
        Rule.MinimumDepthLayer = L.MinLayer;
        Rule.MaximumDepthLayer = L.MaxLayer;
        Rule.UnitsPerCubicMetre = L.UnitsPerCubicMetre;
        Terrain->ResourceYieldRules.Add(Rule);
    }

    // Heat sources may have begun play before the data loader applied. Apply
    // authored fuel definitions to all existing sources; newly spawned sources
    // pick them up from UHeatSourceComponent::BeginPlay.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (!IsValid(*It))
            continue;
        TArray<UHeatSourceComponent*> HeatSources;
        It->GetComponents<UHeatSourceComponent>(HeatSources);
        for (UHeatSourceComponent* HeatSource : HeatSources)
        {
            if (IsValid(HeatSource) && !ApplyToHeatSource(HeatSource))
                return Fail(FString::Printf(TEXT("Heat source %s refused fuel definitions"),
                    *GetNameSafe(HeatSource)));
        }
    }

    // Inventories on the player or level may have begun play before this
    // bootstrap loaded game data. Backfill their weights and hand recipes now.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
            continue;
        UInventoryCraftingComponent* Inventory = Actor->FindComponentByClass<UInventoryCraftingComponent>();
        if (IsValid(Inventory) && !ApplyToInventory(Inventory))
            return Fail(FString::Printf(TEXT("Inventory %s refused game-data weights/recipes"),
                *GetNameSafe(Inventory)));
    }

    bApplied = true;
    return true;
}

bool UWorldGameDataSubsystem::ApplyToInventory(UInventoryCraftingComponent* Inventory) const
{
    if (!bLoaded || !IsValid(Inventory))
        return false;
    for (const TPair<FName, FGameDataItem>& Pair : Items)
        if (!Inventory->ItemWeightGrams.Contains(Pair.Key))
            Inventory->ItemWeightGrams.Add(Pair.Key, Pair.Value.WeightGrams);
    bool bAll = true;
    for (const FGameDataTransformation& T : Transformations)
    {
        // Hand-made item recipes only; physical parts wait for P10.
        if (T.Where != TEXT("hand") || T.bPhysical || T.Outputs.Num() == 0)
            continue;
        FCraftingRecipe Recipe;
        Recipe.RecipeId = T.Id;
        for (const FGameDataItemAmount& Amount : T.Inputs)
        {
            FRecipeIngredient Ingredient;
            Ingredient.ItemId = Amount.ItemId;
            Ingredient.Quantity = Amount.Quantity;
            Ingredient.bConsume = true;
            Recipe.Ingredients.Add(Ingredient);
        }
        for (const FName Tool : T.Tools)
        {
            FRecipeIngredient Ingredient;
            Ingredient.ItemId = Tool;
            Ingredient.Quantity = 1;
            Ingredient.bConsume = false; // A tool is needed, not used up (P4.4).
            Recipe.Ingredients.Add(Ingredient);
        }
        auto MakeStack = [this](const FGameDataItemAmount& Amount)
        {
            FInventoryStack Stack;
            Stack.ItemId = Amount.ItemId;
            Stack.Quantity = Amount.Quantity;
            const FGameDataItem* Item = Items.Find(Amount.ItemId);
            Stack.Category = Item ? ToCategory(Item->Category) : EInventoryItemCategory::Resource;
            Stack.bIsTool = Stack.Category == EInventoryItemCategory::Tool;
            Stack.MaxStack = Stack.bIsTool ? 1 : 99;
            return Stack;
        };
        Recipe.Output = MakeStack(T.Outputs[0]);
        for (int32 Index = 1; Index < T.Outputs.Num(); ++Index)
            Recipe.Byproducts.Add(MakeStack(T.Outputs[Index]));
        const FCraftingRecipe* ExistingRecipe = Inventory->Recipes.FindByPredicate(
            [&Recipe](const FCraftingRecipe& Existing) { return Existing.RecipeId == Recipe.RecipeId; });
        if (!ExistingRecipe)
        {
            bAll = Inventory->RegisterRecipe(Recipe) && bAll;
            continue;
        }

        // Applying game data to an inventory that began play before the loader
        // must be safe to repeat. An authored duplicate ID is acceptable only
        // when it is exactly the same recipe contract; do not silently override it.
        auto SameStack = [](const FInventoryStack& A, const FInventoryStack& B)
        {
            return A.ItemId == B.ItemId && A.Quantity == B.Quantity &&
                A.Category == B.Category && A.MaxStack == B.MaxStack &&
                A.bIsTool == B.bIsTool && A.OwnedWorldItemId == B.OwnedWorldItemId;
        };
        bool bSameRecipe = ExistingRecipe->bEnabled == Recipe.bEnabled &&
            ExistingRecipe->Ingredients.Num() == Recipe.Ingredients.Num() &&
            ExistingRecipe->Byproducts.Num() == Recipe.Byproducts.Num() &&
            SameStack(ExistingRecipe->Output, Recipe.Output);
        for (int32 Index = 0; bSameRecipe && Index < Recipe.Ingredients.Num(); ++Index)
        {
            const FRecipeIngredient& A = ExistingRecipe->Ingredients[Index];
            const FRecipeIngredient& B = Recipe.Ingredients[Index];
            bSameRecipe = A.ItemId == B.ItemId && A.Quantity == B.Quantity && A.bConsume == B.bConsume;
        }
        for (int32 Index = 0; bSameRecipe && Index < Recipe.Byproducts.Num(); ++Index)
            bSameRecipe = SameStack(ExistingRecipe->Byproducts[Index], Recipe.Byproducts[Index]);
        bAll = bSameRecipe && bAll;
    }
    return bAll;
}

bool UWorldGameDataSubsystem::ApplyToHeatSource(UHeatSourceComponent* HeatSource) const
{
    if (!bLoaded || !IsValid(HeatSource))
        return false;
    bool bAll = true;
    for (const FFuel& Fuel : Fuels)
    {
        if (HeatSource->FuelDefinitions.ContainsByPredicate([&Fuel](const FHeatFuelDefinition& Existing)
            { return Existing.ItemDefinitionId == Fuel.ItemId; }))
            continue;
        FHeatFuelDefinition Definition;
        Definition.ItemDefinitionId = Fuel.ItemId;
        Definition.BurnSecondsPerItem = Fuel.BurnSeconds;
        Definition.HeatOutputMultiplier = Fuel.Heat;
        bAll = HeatSource->RegisterFuelDefinition(Definition) && bAll;
    }
    return bAll;
}
