#include "WorldGameDataSubsystem.h"

#include "HeatSourceComponent.h"
#include "InventoryCraftingComponent.h"
#include "WorldProductionSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldTerrainStateSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
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

    bool IsFiniteRange(double Value, double Minimum, double Maximum)
    {
        return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
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
    FJson ItemsJson, StationsJson, TransformationsJson, ConditionsJson, CropsJson, AnimalsJson, FieldActionsJson, InfrastructuresJson, StructureRepairsJson, RaidDefensesJson, KnowledgeJson, ZonesJson, RoutesJson, DiscoveriesJson, CalendarJson, ClimateJson, LayersJson;
    if (!ReadJson(Directory, TEXT("items.json"), ItemsJson, Error) ||
        !ReadJson(Directory, TEXT("stations.json"), StationsJson, Error) ||
        !ReadJson(Directory, TEXT("transformations.json"), TransformationsJson, Error) ||
        !ReadJson(Directory, TEXT("conditions.json"), ConditionsJson, Error) ||
        !ReadJson(Directory, TEXT("crops.json"), CropsJson, Error) ||
        !ReadJson(Directory, TEXT("animals.json"), AnimalsJson, Error) ||
        !ReadJson(Directory, TEXT("field_actions.json"), FieldActionsJson, Error) ||
        !ReadJson(Directory, TEXT("infrastructure.json"), InfrastructuresJson, Error) ||
        !ReadJson(Directory, TEXT("structure_repairs.json"), StructureRepairsJson, Error) ||
        !ReadJson(Directory, TEXT("raid_defenses.json"), RaidDefensesJson, Error) ||
        !ReadJson(Directory, TEXT("knowledge.json"), KnowledgeJson, Error) ||
        !ReadJson(Directory, TEXT("zones.json"), ZonesJson, Error) ||
        !ReadJson(Directory, TEXT("routes.json"), RoutesJson, Error) ||
        !ReadJson(Directory, TEXT("discoveries.json"), DiscoveriesJson, Error) ||
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
        if (Item.Id.IsNone() || NewItems.Contains(Item.Id) || Item.WeightGrams <= 0)
            return Fail(FString::Printf(TEXT("Bad or duplicate item %s"), *Item.Id.ToString()));
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
            FEquipment EquipmentDefinition;
            EquipmentDefinition.ItemId = Item.Id;
            EquipmentDefinition.Slot = NameField(*Sub, TEXT("slot"));
            EquipmentDefinition.Insulation = static_cast<float>(NumberField(*Sub, TEXT("insulation")));
            EquipmentDefinition.WaterResistance = static_cast<float>(NumberField(*Sub, TEXT("water_resistance")));

            const TArray<TSharedPtr<FJsonValue>>* CapabilityValues = ArrayField(*Sub, TEXT("capabilities"));
            TSet<FName> SeenCapabilities;
            if (CapabilityValues)
            {
                for (const TSharedPtr<FJsonValue>& CapabilityValue : *CapabilityValues)
                {
                    const FName CapabilityId(*CapabilityValue->AsString());
                    if (CapabilityId.IsNone() || SeenCapabilities.Contains(CapabilityId))
                        return Fail(FString::Printf(TEXT("Equipment %s has an invalid/repeated capability"),
                            *Item.Id.ToString()));
                    SeenCapabilities.Add(CapabilityId);
                    EquipmentDefinition.GrantedCapabilities.Add(CapabilityId);
                }
            }
            NewEquipment.Add(MoveTemp(EquipmentDefinition));
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

    TSet<FName> TransformationIdsSet = TransformationIds;

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

    TArray<FGameDataCrop> NewCrops;
    const TArray<TSharedPtr<FJsonValue>>* CropValues = ArrayField(CropsJson, TEXT("crops"));
    if (!CropValues || CropValues->IsEmpty())
        return Fail(TEXT("crops.json has no crops"));
    TSet<FName> SeenCropIds;
    for (const TSharedPtr<FJsonValue>& Value : *CropValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataCrop Crop;
        Crop.Id = NameField(Entry, TEXT("id"));
        Crop.SeedItemId = NameField(Entry, TEXT("seed"));
        Crop.HarvestItemId = NameField(Entry, TEXT("harvest"));
        const double HarvestUnitsValue = NumberField(Entry, TEXT("units"), 1.0);
        const double HoursPerStage = NumberField(Entry, TEXT("hours_per_stage"), 1.0);
        const double SeedReturnValue = NumberField(Entry, TEXT("seed_return_units"), 1.0);
        const double FoundInZoneValue = NumberField(Entry, TEXT("found_in_zone"), 0.0);
        if (!FMath::IsFinite(HarvestUnitsValue) || FMath::FloorToDouble(HarvestUnitsValue) != HarvestUnitsValue ||
            HarvestUnitsValue < 1.0 || HarvestUnitsValue > 64.0 ||
            !FMath::IsFinite(HoursPerStage) || HoursPerStage <= 0.0 ||
            !FMath::IsFinite(SeedReturnValue) || FMath::FloorToDouble(SeedReturnValue) != SeedReturnValue ||
            SeedReturnValue < 0.0 || SeedReturnValue > 64.0 ||
            !FMath::IsFinite(FoundInZoneValue) || FMath::FloorToDouble(FoundInZoneValue) != FoundInZoneValue ||
            FoundInZoneValue < 0.0 || FoundInZoneValue > static_cast<double>(MAX_int32))
            return Fail(FString::Printf(TEXT("Crop %s has invalid numeric fields"), *Crop.Id.ToString()));
        Crop.HarvestUnits = static_cast<int32>(HarvestUnitsValue);
        Crop.SecondsPerStage = HoursPerStage * 3600.0;
        Crop.SeedReturnUnits = static_cast<int32>(SeedReturnValue);
        Crop.SoilExhaustionPerHarvest = static_cast<float>(NumberField(Entry, TEXT("soil_exhaustion_per_harvest"), 10.0));
        Crop.MinimumFertility = static_cast<float>(NumberField(Entry, TEXT("minimum_fertility"), 10.0));
        Crop.MaximumPlantWeedPressure = static_cast<float>(NumberField(Entry, TEXT("maximum_plant_weed_pressure"), 0.75));
        Crop.FoundInZone = static_cast<int32>(FoundInZoneValue);

        const TArray<TSharedPtr<FJsonValue>>* Months = ArrayField(Entry, TEXT("plant_months"));
        TSet<int32> SeenPlantMonths;
        if (Months)
        {
            for (const TSharedPtr<FJsonValue>& Month : *Months)
            {
                const double MonthValue = Month->AsNumber();
                if (!FMath::IsFinite(MonthValue) || FMath::FloorToDouble(MonthValue) != MonthValue)
                    return Fail(FString::Printf(TEXT("Crop %s has a non-integer planting month"), *Crop.Id.ToString()));
                const int32 MonthNumber = static_cast<int32>(MonthValue);
                if (MonthNumber < 1 || MonthNumber > 12 || SeenPlantMonths.Contains(MonthNumber))
                    return Fail(FString::Printf(TEXT("Crop %s has an invalid or repeated planting month"), *Crop.Id.ToString()));
                SeenPlantMonths.Add(MonthNumber);
                Crop.PlantMonths.Add(MonthNumber);
            }
        }

        if (Crop.Id.IsNone() || SeenCropIds.Contains(Crop.Id) ||
            !HasItem(Crop.SeedItemId) || !HasItem(Crop.HarvestItemId) ||
            !FMath::IsFinite(HarvestUnitsValue) || FMath::FloorToDouble(HarvestUnitsValue) != HarvestUnitsValue ||
            Crop.HarvestUnits <= 0 || Crop.HarvestUnits > 64 ||
            !FMath::IsFinite(HoursPerStage) || HoursPerStage <= 0.0 ||
            !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0 ||
            !FMath::IsFinite(SeedReturnValue) || FMath::FloorToDouble(SeedReturnValue) != SeedReturnValue ||
            Crop.SeedReturnUnits < 0 || Crop.SeedReturnUnits > 64 ||
            !FMath::IsFinite(Crop.SoilExhaustionPerHarvest) ||
            Crop.SoilExhaustionPerHarvest < 0.0f || Crop.SoilExhaustionPerHarvest > 100.0f ||
            !FMath::IsFinite(Crop.MinimumFertility) || Crop.MinimumFertility < 0.0f || Crop.MinimumFertility > 100.0f ||
            !FMath::IsFinite(Crop.MaximumPlantWeedPressure) ||
            Crop.MaximumPlantWeedPressure < 0.0f || Crop.MaximumPlantWeedPressure > 1.0f ||
            Crop.FoundInZone < 0 || Crop.PlantMonths.IsEmpty())
            return Fail(FString::Printf(TEXT("Bad crop %s"), *Crop.Id.ToString()));

        Crop.PlantMonths.Sort();
        SeenCropIds.Add(Crop.Id);
        NewCrops.Add(Crop);
    }

    TArray<FGameDataAnimal> NewAnimals;
    const TArray<TSharedPtr<FJsonValue>>* AnimalValues = ArrayField(AnimalsJson, TEXT("animals"));
    if (!AnimalValues || AnimalValues->IsEmpty())
        return Fail(TEXT("animals.json has no animals"));
    TSet<FName> SeenAnimalIds;
    for (const TSharedPtr<FJsonValue>& Value : *AnimalValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataAnimal Animal;
        Animal.Id = NameField(Entry, TEXT("id"));
        Animal.FeedItemId = NameField(Entry, TEXT("feed"));
        Animal.ProductItemId = NameField(Entry, TEXT("product"));
        const double ProductIntervalHours = NumberField(Entry, TEXT("product_interval_hours"), 1.0);
        const double ProductUnitsValue = NumberField(Entry, TEXT("product_units"), 1.0);
        if (Animal.Id.IsNone() || SeenAnimalIds.Contains(Animal.Id) ||
            !HasItem(Animal.FeedItemId) ||
            (!Animal.ProductItemId.IsNone() && !HasItem(Animal.ProductItemId)) ||
            !FMath::IsFinite(ProductIntervalHours) || ProductIntervalHours < 1.0 ||
            !FMath::IsFinite(ProductUnitsValue) || FMath::FloorToDouble(ProductUnitsValue) != ProductUnitsValue ||
            ProductUnitsValue < 1.0 || ProductUnitsValue > 64.0)
            return Fail(FString::Printf(TEXT("Bad animal %s"), *Animal.Id.ToString()));

        Animal.ProductIntervalSeconds = ProductIntervalHours * 3600.0;
        if (!FMath::IsFinite(Animal.ProductIntervalSeconds) || Animal.ProductIntervalSeconds < 3600.0)
            return Fail(FString::Printf(TEXT("Animal %s has an invalid product interval"), *Animal.Id.ToString()));
        Animal.ProductUnitsPerInterval = static_cast<int32>(ProductUnitsValue);
        const TArray<TSharedPtr<FJsonValue>>* CaptureTools = ArrayField(Entry, TEXT("capture_tools"));
        if (CaptureTools)
        {
            TSet<FName> SeenTools;
            for (const TSharedPtr<FJsonValue>& ToolValue : *CaptureTools)
            {
                const FString ToolString = ToolValue->AsString();
                const FName ToolId(*ToolString);
                if (ToolId.IsNone() || !HasItem(ToolId) || SeenTools.Contains(ToolId))
                    return Fail(FString::Printf(TEXT("Animal %s has an invalid capture tool"), *Animal.Id.ToString()));
                SeenTools.Add(ToolId);
                Animal.CaptureToolItemIds.Add(ToolId);
            }
        }
        if (Animal.CaptureToolItemIds.IsEmpty())
            return Fail(FString::Printf(TEXT("Animal %s requires at least one capture tool"), *Animal.Id.ToString()));

        const TArray<TSharedPtr<FJsonValue>>* Yields = ArrayField(Entry, TEXT("slaughter_yields"));
        if (!Yields || Yields->IsEmpty())
            return Fail(FString::Printf(TEXT("Animal %s has no slaughter yields"), *Animal.Id.ToString()));
        TSet<FName> SeenYieldItems;
        int32 TotalYieldUnits = 0;
        for (const TSharedPtr<FJsonValue>& YieldValue : *Yields)
        {
            const FJson Yield = YieldValue->AsObject();
            const FName ItemId = NameField(Yield, TEXT("item"));
            const double UnitsValue = NumberField(Yield, TEXT("units"), 1.0);
            if (ItemId.IsNone() || !HasItem(ItemId) ||
                !FMath::IsFinite(UnitsValue) || FMath::FloorToDouble(UnitsValue) != UnitsValue ||
                UnitsValue < 1.0 || UnitsValue > 64.0 ||
                SeenYieldItems.Contains(ItemId) || TotalYieldUnits > 64 - static_cast<int32>(UnitsValue))
                return Fail(FString::Printf(TEXT("Animal %s has invalid slaughter yields"), *Animal.Id.ToString()));
            FGameDataItemAmount& YieldAmount = Animal.SlaughterYields.AddDefaulted_GetRef();
            YieldAmount.ItemId = ItemId;
            YieldAmount.Quantity = static_cast<int32>(UnitsValue);
            TotalYieldUnits += YieldAmount.Quantity;
            SeenYieldItems.Add(ItemId);
        }

        SeenAnimalIds.Add(Animal.Id);
        NewAnimals.Add(Animal);
    }

    TArray<FGameDataFieldAction> NewFieldActions;
    const TArray<TSharedPtr<FJsonValue>>* FieldActionValues = ArrayField(FieldActionsJson, TEXT("actions"));
    if (!FieldActionValues || FieldActionValues->Num() != 15)
        return Fail(TEXT("field_actions.json must define exactly 15 field actions"));
    TSet<FName> SeenFieldActionIds;
    const TSet<FName> SupportedFieldOperations = {
        TEXT("Clear"), TEXT("Till"), TEXT("Fertilize"), TEXT("Plant"), TEXT("Water"),
        TEXT("Weed"), TEXT("Mulch"), TEXT("Harvest"), TEXT("Dry"), TEXT("Thresh"),
        TEXT("Winnow"), TEXT("Grind"), TEXT("Store"), TEXT("SeparateSeeds"), TEXT("Fence")
    };
    for (const TSharedPtr<FJsonValue>& Value : *FieldActionValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataFieldAction Action;
        Action.Id = NameField(Entry, TEXT("id"));
        Action.Operation = NameField(Entry, TEXT("operation"));
        Action.WorkSeconds = NumberField(Entry, TEXT("work_seconds"));
        Action.WorkAmount = static_cast<float>(NumberField(Entry, TEXT("work_amount"), 1.0));
        Action.StationId = NameField(Entry, TEXT("station"));
        Action.StoragePolicy = NameField(Entry, TEXT("storage_policy"));
        if (const TArray<TSharedPtr<FJsonValue>>* Tools = ArrayField(Entry, TEXT("tools")))
        {
            TSet<FName> SeenTools;
            for (const TSharedPtr<FJsonValue>& Tool : *Tools)
            {
                const FName ToolId(*Tool->AsString());
                if (ToolId.IsNone() || !HasItem(ToolId) || SeenTools.Contains(ToolId))
                    return Fail(FString::Printf(TEXT("Field action %s has an invalid/repeated tool"),
                        *Action.Id.ToString()));
                SeenTools.Add(ToolId);
                Action.Tools.Add(ToolId);
            }
        }
        Action.Inputs = Amounts(Entry, TEXT("inputs"));
        for (const FGameDataItemAmount& Input : Action.Inputs)
            if (!HasItem(Input.ItemId) || Input.Quantity <= 0)
                return Fail(FString::Printf(TEXT("Field action %s has an unknown/invalid input %s"),
                    *Action.Id.ToString(), *Input.ItemId.ToString()));
        if (const TArray<TSharedPtr<FJsonValue>>* TransformationIds = ArrayField(Entry, TEXT("transformation_ids")))
            for (const TSharedPtr<FJsonValue>& TransformationValue : *TransformationIds)
            {
                const FName TransformationId(*TransformationValue->AsString());
                if (TransformationId.IsNone() || !TransformationIdsSet.Contains(TransformationId))
                    return Fail(FString::Printf(TEXT("Field action %s references unknown transformation %s"),
                        *Action.Id.ToString(), *TransformationId.ToString()));
                Action.TransformationIds.Add(TransformationId);
            }
        if (Action.Id.IsNone() || SeenFieldActionIds.Contains(Action.Id) ||
            !SupportedFieldOperations.Contains(Action.Operation) ||
            !FMath::IsFinite(Action.WorkSeconds) || Action.WorkSeconds <= 0.0 || Action.WorkSeconds > 31536000.0 ||
            !FMath::IsFinite(Action.WorkAmount) || Action.WorkAmount <= 0.0f || Action.WorkAmount > 100.0f ||
            (!Action.StationId.IsNone() && !NewStations.Contains(Action.StationId)) ||
            (Action.Operation == TEXT("Store") && Action.StoragePolicy.IsNone()))
            return Fail(FString::Printf(TEXT("Bad field action %s"), *Action.Id.ToString()));
        SeenFieldActionIds.Add(Action.Id);
        NewFieldActions.Add(Action);
    }

    TArray<FGameDataInfrastructure> NewInfrastructures;
    const TArray<TSharedPtr<FJsonValue>>* InfrastructureValues = ArrayField(InfrastructuresJson, TEXT("infrastructures"));
    if (!InfrastructureValues || InfrastructureValues->IsEmpty())
        return Fail(TEXT("infrastructure.json has no infrastructure definitions"));
    TSet<FName> SeenInfrastructureIds;
    for (const TSharedPtr<FJsonValue>& Value : *InfrastructureValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataInfrastructure Definition;
        Definition.Id = NameField(Entry, TEXT("id"));
        Entry->TryGetStringField(TEXT("name"), Definition.Name);
        Definition.BuildWorkSeconds = NumberField(Entry, TEXT("build_work_seconds"));
        Definition.MaxCondition = static_cast<float>(NumberField(Entry, TEXT("max_condition"), 100.0));
        Definition.RepairConditionPerJob = static_cast<float>(NumberField(Entry, TEXT("repair_condition_per_job"), 25.0));
        Definition.BuiltRouteCostMultiplier = static_cast<float>(NumberField(Entry, TEXT("route_cost_multiplier_built"), 1.0));
        Definition.BuildMaterials = Amounts(Entry, TEXT("build_materials"));
        Definition.RepairMaterials = Amounts(Entry, TEXT("repair_materials"));
        auto ParseTools = [&Entry, &HasItem](const TCHAR* Field, TArray<FName>& OutTools) -> bool
        {
            if (const TArray<TSharedPtr<FJsonValue>>* Tools = ArrayField(Entry, Field))
            {
                TSet<FName> Seen;
                for (const TSharedPtr<FJsonValue>& Tool : *Tools)
                {
                    const FName ToolId(*Tool->AsString());
                    if (ToolId.IsNone() || !HasItem(ToolId) || Seen.Contains(ToolId))
                        return false;
                    Seen.Add(ToolId);
                    OutTools.Add(ToolId);
                }
            }
            return true;
        };
        if (!ParseTools(TEXT("tools"), Definition.Tools))
            return Fail(FString::Printf(TEXT("Infrastructure %s has an invalid tool"), *Definition.Id.ToString()));
        for (const FGameDataItemAmount& Amount : Definition.BuildMaterials)
            if (!HasItem(Amount.ItemId) || Amount.Quantity <= 0)
                return Fail(FString::Printf(TEXT("Infrastructure %s has invalid build material"), *Definition.Id.ToString()));
        for (const FGameDataItemAmount& Amount : Definition.RepairMaterials)
            if (!HasItem(Amount.ItemId) || Amount.Quantity <= 0)
                return Fail(FString::Printf(TEXT("Infrastructure %s has invalid repair material"), *Definition.Id.ToString()));
        if (const TArray<TSharedPtr<FJsonValue>>* WearValues = ArrayField(Entry, TEXT("wear_events")))
            for (const TSharedPtr<FJsonValue>& WearValue : *WearValues)
            {
                const FJson Wear = WearValue->AsObject();
                FGameDataInfrastructureWearRule& Rule = Definition.WearRules.AddDefaulted_GetRef();
                Rule.EventId = NameField(Wear, TEXT("event_id"));
                Rule.WearPerHour = static_cast<float>(NumberField(Wear, TEXT("wear_per_hour")));
                if (Rule.EventId.IsNone() || !FMath::IsFinite(Rule.WearPerHour) ||
                    Rule.WearPerHour <= 0.0f || Rule.WearPerHour > 100000.0f)
                    return Fail(FString::Printf(TEXT("Infrastructure %s has invalid wear rule"), *Definition.Id.ToString()));
            }
        if (Definition.Id.IsNone() || SeenInfrastructureIds.Contains(Definition.Id) ||
            Definition.BuildMaterials.IsEmpty() || Definition.RepairMaterials.IsEmpty() ||
            !FMath::IsFinite(Definition.BuildWorkSeconds) || Definition.BuildWorkSeconds <= 0.0 ||
            !FMath::IsFinite(Definition.MaxCondition) || Definition.MaxCondition <= 0.0f ||
            !FMath::IsFinite(Definition.RepairConditionPerJob) || Definition.RepairConditionPerJob <= 0.0f || Definition.RepairConditionPerJob > Definition.MaxCondition ||
            !FMath::IsFinite(Definition.BuiltRouteCostMultiplier) ||
            Definition.BuiltRouteCostMultiplier <= 0.0f || Definition.BuiltRouteCostMultiplier > 100.0f)
            return Fail(FString::Printf(TEXT("Bad infrastructure definition %s"), *Definition.Id.ToString()));
        SeenInfrastructureIds.Add(Definition.Id);
        NewInfrastructures.Add(Definition);
    }

    TArray<FGameDataStructureRepair> NewStructureRepairs;
    const TArray<TSharedPtr<FJsonValue>>* RepairValues = ArrayField(StructureRepairsJson, TEXT("repairs"));
    if (!RepairValues || RepairValues->IsEmpty())
        return Fail(TEXT("structure_repairs.json has no repair recipes"));
    TSet<FName> SeenRepairIds;
    for (const TSharedPtr<FJsonValue>& Value : *RepairValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataStructureRepair Repair;
        Repair.Id = NameField(Entry, TEXT("id"));
        Repair.StructureKind = NameField(Entry, TEXT("structure_kind"));
        Repair.Materials = Amounts(Entry, TEXT("materials"));
        Repair.RepairAmount = static_cast<float>(NumberField(Entry, TEXT("repair_amount")));
        Repair.WorkSeconds = NumberField(Entry, TEXT("work_seconds"));
        if (const TArray<TSharedPtr<FJsonValue>>* Tools = ArrayField(Entry, TEXT("tools")))
            for (const TSharedPtr<FJsonValue>& Tool : *Tools)
            {
                const FName ToolId(*Tool->AsString());
                if (ToolId.IsNone() || !HasItem(ToolId) || Repair.Tools.Contains(ToolId))
                    return Fail(FString::Printf(TEXT("Repair %s has an invalid/repeated tool"), *Repair.Id.ToString()));
                Repair.Tools.Add(ToolId);
            }
        for (const FGameDataItemAmount& Amount : Repair.Materials)
            if (!HasItem(Amount.ItemId) || Amount.Quantity <= 0)
                return Fail(FString::Printf(TEXT("Repair %s has invalid material"), *Repair.Id.ToString()));
        if (Repair.Id.IsNone() || SeenRepairIds.Contains(Repair.Id) ||
            (Repair.StructureKind != TEXT("Wood") && Repair.StructureKind != TEXT("Stone")) ||
            Repair.Materials.IsEmpty() || Repair.Tools.IsEmpty() ||
            !FMath::IsFinite(Repair.RepairAmount) || Repair.RepairAmount <= 0.0f ||
            !FMath::IsFinite(Repair.WorkSeconds) || Repair.WorkSeconds <= 0.0)
            return Fail(FString::Printf(TEXT("Bad structure repair %s"), *Repair.Id.ToString()));
        SeenRepairIds.Add(Repair.Id);
        NewStructureRepairs.Add(Repair);
    }

    TArray<FGameDataRaidDefense> NewRaidDefenses;
    const TArray<TSharedPtr<FJsonValue>>* DefenseValues = ArrayField(RaidDefensesJson, TEXT("defenses"));
    if (!DefenseValues || DefenseValues->IsEmpty())
        return Fail(TEXT("raid_defenses.json has no countermeasures"));
    TSet<FName> SeenDefenseIds;
    for (const TSharedPtr<FJsonValue>& Value : *DefenseValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataRaidDefense Defense;
        Defense.Id = NameField(Entry, TEXT("id"));
        Entry->TryGetStringField(TEXT("name"), Defense.Name);
        Defense.BuildMaterials = Amounts(Entry, TEXT("build_materials"));
        Defense.DefenseRating = static_cast<float>(NumberField(Entry, TEXT("defense_rating")));
        Defense.WorkSeconds = NumberField(Entry, TEXT("work_seconds"));
        if (const TArray<TSharedPtr<FJsonValue>>* Tools = ArrayField(Entry, TEXT("tools")))
            for (const TSharedPtr<FJsonValue>& Tool : *Tools)
            {
                const FName ToolId(*Tool->AsString());
                if (ToolId.IsNone() || !HasItem(ToolId) || Defense.Tools.Contains(ToolId))
                    return Fail(FString::Printf(TEXT("Defense %s has invalid/repeated tool"), *Defense.Id.ToString()));
                Defense.Tools.Add(ToolId);
            }
        for (const FGameDataItemAmount& Amount : Defense.BuildMaterials)
            if (!HasItem(Amount.ItemId) || Amount.Quantity <= 0)
                return Fail(FString::Printf(TEXT("Defense %s has invalid material"), *Defense.Id.ToString()));
        const TSharedPtr<FJsonObject>* Suppression = nullptr;
        if (Entry->TryGetObjectField(TEXT("signal_suppression"), Suppression))
        {
            Defense.StorageSuppression = static_cast<float>(NumberField(*Suppression, TEXT("Storage")));
            Defense.SmellSuppression = static_cast<float>(NumberField(*Suppression, TEXT("Smell")));
            Defense.LightSuppression = static_cast<float>(NumberField(*Suppression, TEXT("Light")));
            Defense.SmokeSuppression = static_cast<float>(NumberField(*Suppression, TEXT("Smoke")));
        }
        if (Defense.Id.IsNone() || SeenDefenseIds.Contains(Defense.Id) ||
            Defense.BuildMaterials.IsEmpty() ||
            !FMath::IsFinite(Defense.DefenseRating) || Defense.DefenseRating < 0.0f ||
            Defense.DefenseRating > 100.0f ||
            !IsFiniteRange(Defense.StorageSuppression, 0.0f, 1.0f) ||
            !IsFiniteRange(Defense.SmellSuppression, 0.0f, 1.0f) ||
            !IsFiniteRange(Defense.LightSuppression, 0.0f, 1.0f) ||
            !IsFiniteRange(Defense.SmokeSuppression, 0.0f, 1.0f) ||
            !FMath::IsFinite(Defense.WorkSeconds) || Defense.WorkSeconds <= 0.0)
            return Fail(FString::Printf(TEXT("Bad raid defense definition %s"), *Defense.Id.ToString()));
        SeenDefenseIds.Add(Defense.Id);
        NewRaidDefenses.Add(Defense);
    }

    TArray<FGameDataZone> NewZones;
    const TArray<TSharedPtr<FJsonValue>>* ZoneValues = ArrayField(ZonesJson, TEXT("zones"));
    if (!ZoneValues || ZoneValues->Num() != 5)
        return Fail(TEXT("zones.json must define exactly the five zone IDs 0–4"));
    TSet<FName> SeenZoneIds;
    TSet<int32> SeenZoneNumbers;
    for (const TSharedPtr<FJsonValue>& Value : *ZoneValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataZone Zone;
        Zone.Id = NameField(Entry, TEXT("id"));
        const double ZoneNumberValue = NumberField(Entry, TEXT("number"), -1.0);
        if (!FMath::IsFinite(ZoneNumberValue) || FMath::FloorToDouble(ZoneNumberValue) != ZoneNumberValue ||
            ZoneNumberValue < 0.0 || ZoneNumberValue > 4.0)
            return Fail(FString::Printf(TEXT("Zone %s has an invalid number"), *Zone.Id.ToString()));
        Zone.Number = static_cast<int32>(ZoneNumberValue);
        Entry->TryGetStringField(TEXT("name"), Zone.Name);
        Entry->TryGetStringField(TEXT("description"), Zone.Description);
        Zone.DiscoveryId = NameField(Entry, TEXT("discovery_id"));
        Zone.BarrierId = NameField(Entry, TEXT("barrier_id"));
        Entry->TryGetStringField(TEXT("barrier_description"), Zone.BarrierDescription);

        const TArray<TSharedPtr<FJsonValue>>* ResourceIds = ArrayField(Entry, TEXT("resource_item_ids"));
        TSet<FName> SeenResourceIds;
        if (ResourceIds)
        {
            for (const TSharedPtr<FJsonValue>& ResourceValue : *ResourceIds)
            {
                const FName ItemId(*ResourceValue->AsString());
                if (ItemId.IsNone() || !HasItem(ItemId) || SeenResourceIds.Contains(ItemId))
                    return Fail(FString::Printf(TEXT("Zone %s has an invalid resource item"), *Zone.Id.ToString()));
                SeenResourceIds.Add(ItemId);
                Zone.ResourceItemIds.Add(ItemId);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* SolutionIds = ArrayField(Entry, TEXT("permanent_solution_ids"));
        TSet<FName> SeenSolutions;
        if (SolutionIds)
        {
            for (const TSharedPtr<FJsonValue>& SolutionValue : *SolutionIds)
            {
                const FName SolutionId(*SolutionValue->AsString());
                if (SolutionId.IsNone() || SeenSolutions.Contains(SolutionId))
                    return Fail(FString::Printf(TEXT("Zone %s has an invalid/repeated permanent solution"), *Zone.Id.ToString()));
                SeenSolutions.Add(SolutionId);
                Zone.PermanentSolutionIds.Add(SolutionId);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* SeasonalMonths = ArrayField(Entry, TEXT("seasonal_months"));
        TSet<int32> SeenSeasonalMonths;
        if (SeasonalMonths)
        {
            for (const TSharedPtr<FJsonValue>& MonthValue : *SeasonalMonths)
            {
                const double Number = MonthValue->AsNumber();
                if (!FMath::IsFinite(Number) || FMath::FloorToDouble(Number) != Number ||
                    Number < 1.0 || Number > 12.0 || SeenSeasonalMonths.Contains(static_cast<int32>(Number)))
                    return Fail(FString::Printf(TEXT("Zone %s has an invalid/repeated seasonal month"), *Zone.Id.ToString()));
                SeenSeasonalMonths.Add(static_cast<int32>(Number));
                Zone.SeasonalMonths.Add(static_cast<int32>(Number));
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* RequiredCapabilities = ArrayField(Entry, TEXT("required_capabilities"));
        TSet<FName> SeenRequiredCapabilities;
        if (RequiredCapabilities)
        {
            for (const TSharedPtr<FJsonValue>& CapabilityValue : *RequiredCapabilities)
            {
                const FName CapabilityId(*CapabilityValue->AsString());
                const bool bKnownCapability = NewEquipment.ContainsByPredicate(
                    [CapabilityId](const FEquipment& Equipment)
                    {
                        return Equipment.GrantedCapabilities.Contains(CapabilityId);
                    });
                if (CapabilityId.IsNone() || SeenRequiredCapabilities.Contains(CapabilityId) || !bKnownCapability)
                    return Fail(FString::Printf(TEXT("Zone %s has an unknown/repeated capability"),
                        *Zone.Id.ToString()));
                SeenRequiredCapabilities.Add(CapabilityId);
                Zone.RequiredCapabilityIds.Add(CapabilityId);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* RequiredDiscoveries = ArrayField(Entry, TEXT("required_discovery_ids"));
        TSet<FName> SeenRequiredDiscoveries;
        if (RequiredDiscoveries)
        {
            for (const TSharedPtr<FJsonValue>& DiscoveryValue : *RequiredDiscoveries)
            {
                const FName DiscoveryId(*DiscoveryValue->AsString());
                if (DiscoveryId.IsNone() || SeenRequiredDiscoveries.Contains(DiscoveryId))
                    return Fail(FString::Printf(TEXT("Zone %s has an invalid/repeated discovery gate"),
                        *Zone.Id.ToString()));
                SeenRequiredDiscoveries.Add(DiscoveryId);
                Zone.RequiredDiscoveryIds.Add(DiscoveryId);
            }
        }

        if (Zone.Id.IsNone() || SeenZoneIds.Contains(Zone.Id) ||
            Zone.Name.IsEmpty() || Zone.DiscoveryId.IsNone() || Zone.BarrierId.IsNone() ||
            SeenZoneNumbers.Contains(Zone.Number) ||
            (Zone.Number == 4 && Zone.SeasonalMonths.IsEmpty()))
            return Fail(FString::Printf(TEXT("Bad/duplicate zone definition %s"), *Zone.Id.ToString()));
        SeenZoneIds.Add(Zone.Id);
        SeenZoneNumbers.Add(Zone.Number);
        NewZones.Add(Zone);
    }
    if (SeenZoneNumbers.Num() != 5)
        return Fail(TEXT("zones.json must cover every zone number from 0 through 4"));

    TArray<FGameDataRouteNode> NewRouteNodes;
    const TArray<TSharedPtr<FJsonValue>>* RouteNodeValues = ArrayField(RoutesJson, TEXT("nodes"));
    if (!RouteNodeValues || RouteNodeValues->IsEmpty() || RouteNodeValues->Num() > 512)
        return Fail(TEXT("routes.json must define between 1 and 512 route nodes"));
    TSet<FName> SeenRouteNodeIds;
    for (const TSharedPtr<FJsonValue>& Value : *RouteNodeValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataRouteNode Node;
        Node.NodeId = NameField(Entry, TEXT("id"));
        Node.ZoneId = NameField(Entry, TEXT("zone"));
        Node.Location.X = NumberField(Entry, TEXT("x"), TNumericLimits<double>::QuietNaN());
        Node.Location.Y = NumberField(Entry, TEXT("y"), TNumericLimits<double>::QuietNaN());
        Node.Location.Z = NumberField(Entry, TEXT("z"), TNumericLimits<double>::QuietNaN());
        Node.bEnabled = !Entry.IsValid() || !Entry->HasField(TEXT("enabled")) ? true : BoolField(Entry, TEXT("enabled"));
        if (Node.NodeId.IsNone() || SeenRouteNodeIds.Contains(Node.NodeId) ||
            !SeenZoneIds.Contains(Node.ZoneId) || Node.Location.ContainsNaN() ||
            !FMath::IsFinite(Node.Location.X) || !FMath::IsFinite(Node.Location.Y) || !FMath::IsFinite(Node.Location.Z))
            return Fail(FString::Printf(TEXT("Bad route node %s"), *Node.NodeId.ToString()));
        SeenRouteNodeIds.Add(Node.NodeId);
        NewRouteNodes.Add(Node);
    }

    TArray<FGameDataRouteEdge> NewRouteEdges;
    const TArray<TSharedPtr<FJsonValue>>* RouteEdgeValues = ArrayField(RoutesJson, TEXT("edges"));
    if (!RouteEdgeValues || RouteEdgeValues->IsEmpty() || RouteEdgeValues->Num() > 16384)
        return Fail(TEXT("routes.json must define between 1 and 16384 route edges"));
    TSet<FString> SeenRoutePairs;
    for (const TSharedPtr<FJsonValue>& Value : *RouteEdgeValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataRouteEdge Edge;
        Edge.FromNode = NameField(Entry, TEXT("from"));
        Edge.ToNode = NameField(Entry, TEXT("to"));
        Edge.RegionId = NameField(Entry, TEXT("region_id"));
        const double Cost = NumberField(Entry, TEXT("cost"), 1.0);
        const double Slope = NumberField(Entry, TEXT("slope_ratio"), 0.0);
        const double Surface = NumberField(Entry, TEXT("surface_multiplier"), 1.0);
        const double Weather = NumberField(Entry, TEXT("weather_sensitivity"), 0.0);
        const double Load = NumberField(Entry, TEXT("load_sensitivity"), 0.0);
        const double MissingInfrastructure = NumberField(Entry, TEXT("missing_infrastructure_penalty"), 0.0);
        Edge.Cost = static_cast<float>(Cost);
        Edge.SlopeRatio = static_cast<float>(Slope);
        Edge.SurfaceCostMultiplier = static_cast<float>(Surface);
        Edge.WeatherSensitivity = static_cast<float>(Weather);
        Edge.LoadSensitivity = static_cast<float>(Load);
        Edge.MissingInfrastructurePenalty = static_cast<float>(MissingInfrastructure);
        Edge.InfrastructureId = NameField(Entry, TEXT("infrastructure_id"));
        Edge.bHasBuiltInfrastructure = BoolField(Entry, TEXT("built"));
        Edge.bRequiresInfrastructure = BoolField(Entry, TEXT("requires_infrastructure"));
        Edge.bEnabled = !Entry.IsValid() || !Entry->HasField(TEXT("enabled")) ? true : BoolField(Entry, TEXT("enabled"));

        const FString PairKey = Edge.FromNode.ToString() + TEXT("->") + Edge.ToNode.ToString();
        if (Edge.FromNode.IsNone() || Edge.ToNode.IsNone() || Edge.FromNode == Edge.ToNode ||
            !SeenRouteNodeIds.Contains(Edge.FromNode) || !SeenRouteNodeIds.Contains(Edge.ToNode) ||
            SeenRoutePairs.Contains(PairKey) ||
            !FMath::IsFinite(Cost) || Cost < 0.0 || Cost > TNumericLimits<float>::Max() ||
            !FMath::IsFinite(Slope) || Slope < 0.0 || Slope > 10.0 ||
            !FMath::IsFinite(Surface) || Surface < 0.01 || Surface > 100.0 ||
            !FMath::IsFinite(Weather) || Weather < 0.0 || Weather > 100.0 ||
            !FMath::IsFinite(Load) || Load < 0.0 || Load > 100.0 ||
            !FMath::IsFinite(MissingInfrastructure) || MissingInfrastructure < 0.0 || MissingInfrastructure > 100.0 ||
            (Edge.bRequiresInfrastructure && Edge.InfrastructureId.IsNone()) ||
            (!Edge.RegionId.IsNone() && !NewZones.ContainsByPredicate(
                [&Edge](const FGameDataZone& Zone) { return Zone.Id == Edge.RegionId; })))
            return Fail(FString::Printf(TEXT("Bad route edge %s -> %s"), *Edge.FromNode.ToString(), *Edge.ToNode.ToString()));
        SeenRoutePairs.Add(PairKey);
        NewRouteEdges.Add(Edge);
    }

    TArray<FGameDataKnowledge> NewKnowledge;
    const TArray<TSharedPtr<FJsonValue>>* KnowledgeValues = ArrayField(KnowledgeJson, TEXT("knowledge"));
    if (!KnowledgeValues || KnowledgeValues->IsEmpty() || KnowledgeValues->Num() > 256)
        return Fail(TEXT("knowledge.json must define between 1 and 256 knowledge entries"));
    TSet<FName> SeenKnowledgeIds;
    for (const TSharedPtr<FJsonValue>& Value : *KnowledgeValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataKnowledge KnowledgeEntry;
        KnowledgeEntry.Id = NameField(Entry, TEXT("id"));
        Entry->TryGetStringField(TEXT("name"), KnowledgeEntry.Name);
        Entry->TryGetStringField(TEXT("description"), KnowledgeEntry.Description);
        if (KnowledgeEntry.Id.IsNone() || !KnowledgeEntry.Id.ToString().StartsWith(TEXT("Knowledge.")) ||
            KnowledgeEntry.Name.IsEmpty() || KnowledgeEntry.Description.IsEmpty() ||
            SeenKnowledgeIds.Contains(KnowledgeEntry.Id))
            return Fail(FString::Printf(TEXT("Bad or duplicate knowledge definition %s"),
                *KnowledgeEntry.Id.ToString()));
        SeenKnowledgeIds.Add(KnowledgeEntry.Id);
        NewKnowledge.Add(KnowledgeEntry);
    }

    TArray<FGameDataDiscovery> NewDiscoveries;
    const TArray<TSharedPtr<FJsonValue>>* DiscoveryValues = ArrayField(DiscoveriesJson, TEXT("discoveries"));
    if (!DiscoveryValues || DiscoveryValues->IsEmpty() || DiscoveryValues->Num() > 512)
        return Fail(TEXT("discoveries.json must define between 1 and 512 discoveries"));

    TSet<FName> SeenDiscoveryIds;
    auto IsKnownUnlockId = [&NewItems, &NewStations, &NewTransformations, &NewCrops,
        &NewAnimals, &NewKnowledge, &NewZones, &NewRouteEdges](FName Id)
    {
        if (Id.IsNone())
            return false;
        if (NewItems.Contains(Id) || NewStations.Contains(Id) ||
            NewTransformations.ContainsByPredicate([Id](const FGameDataTransformation& T) { return T.Id == Id; }) ||
            NewCrops.ContainsByPredicate([Id](const FGameDataCrop& C) { return C.Id == Id; }) ||
            NewAnimals.ContainsByPredicate([Id](const FGameDataAnimal& A) { return A.Id == Id; }) ||
            NewKnowledge.ContainsByPredicate([Id](const FGameDataKnowledge& K) { return K.Id == Id; }) ||
            NewZones.ContainsByPredicate([Id](const FGameDataZone& Z) { return Z.Id == Id; }) ||
            NewZones.ContainsByPredicate([Id](const FGameDataZone& Z) { return Z.PermanentSolutionIds.Contains(Id); }) ||
            NewRouteEdges.ContainsByPredicate([Id](const FGameDataRouteEdge& E) { return E.InfrastructureId == Id; }))
            return true;
        return false;
    };

    for (const TSharedPtr<FJsonValue>& Value : *DiscoveryValues)
    {
        const FJson Entry = Value->AsObject();
        FGameDataDiscovery Discovery;
        Discovery.DiscoveryId = NameField(Entry, TEXT("id"));

        const TArray<TSharedPtr<FJsonValue>>* Prerequisites = ArrayField(Entry, TEXT("prerequisites"));
        TSet<FName> SeenPrerequisites;
        if (Prerequisites)
        {
            for (const TSharedPtr<FJsonValue>& PrerequisiteValue : *Prerequisites)
            {
                const FName PrerequisiteId(*PrerequisiteValue->AsString());
                // Enforce topological file order: a prerequisite must be declared before its dependent.
                if (PrerequisiteId.IsNone() || PrerequisiteId == Discovery.DiscoveryId ||
                    SeenPrerequisites.Contains(PrerequisiteId) || !SeenDiscoveryIds.Contains(PrerequisiteId))
                    return Fail(FString::Printf(TEXT("Discovery %s has an unknown, forward or repeated prerequisite"),
                        *Discovery.DiscoveryId.ToString()));
                SeenPrerequisites.Add(PrerequisiteId);
                Discovery.PrerequisiteDiscoveryIds.Add(PrerequisiteId);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* RequiredEquipment = ArrayField(Entry, TEXT("required_equipment"));
        TSet<FName> SeenRequiredEquipment;
        if (RequiredEquipment)
        {
            for (const TSharedPtr<FJsonValue>& EquipmentValue : *RequiredEquipment)
            {
                const FName EquipmentId(*EquipmentValue->AsString());
                const bool bConfiguredEquipment = NewEquipment.ContainsByPredicate(
                    [EquipmentId](const FEquipment& Equipment) { return Equipment.ItemId == EquipmentId; });
                if (EquipmentId.IsNone() || !bConfiguredEquipment || SeenRequiredEquipment.Contains(EquipmentId))
                    return Fail(FString::Printf(TEXT("Discovery %s has unknown or repeated required equipment"),
                        *Discovery.DiscoveryId.ToString()));
                SeenRequiredEquipment.Add(EquipmentId);
                Discovery.RequiredEquipmentDefinitionIds.Add(EquipmentId);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Unlocked = ArrayField(Entry, TEXT("unlocks"));
        TSet<FName> SeenUnlocks;
        if (Unlocked)
        {
            for (const TSharedPtr<FJsonValue>& UnlockValue : *Unlocked)
            {
                const FName UnlockId(*UnlockValue->AsString());
                if (!IsKnownUnlockId(UnlockId) || SeenUnlocks.Contains(UnlockId))
                    return Fail(FString::Printf(TEXT("Discovery %s has an unknown or repeated unlock ID"),
                        *Discovery.DiscoveryId.ToString()));
                SeenUnlocks.Add(UnlockId);
                Discovery.UnlockedDefinitionIds.Add(UnlockId);
            }
        }

        if (Discovery.DiscoveryId.IsNone() || SeenDiscoveryIds.Contains(Discovery.DiscoveryId) ||
            Discovery.UnlockedDefinitionIds.IsEmpty())
            return Fail(FString::Printf(TEXT("Bad or duplicate discovery %s"), *Discovery.DiscoveryId.ToString()));

        SeenDiscoveryIds.Add(Discovery.DiscoveryId);
        NewDiscoveries.Add(Discovery);
    }

    TSet<FName> SeenZoneDiscoveryIds;
    for (const FGameDataZone& Zone : NewZones)
    {
        if (!SeenDiscoveryIds.Contains(Zone.DiscoveryId) ||
            SeenZoneDiscoveryIds.Contains(Zone.DiscoveryId))
            return Fail(FString::Printf(TEXT("Zone %s has an unknown or reused discovery ID %s"),
                *Zone.Id.ToString(), *Zone.DiscoveryId.ToString()));
        SeenZoneDiscoveryIds.Add(Zone.DiscoveryId);

        for (const FName RequiredDiscoveryId : Zone.RequiredDiscoveryIds)
        {
            if (!SeenDiscoveryIds.Contains(RequiredDiscoveryId) ||
                RequiredDiscoveryId == Zone.DiscoveryId)
                return Fail(FString::Printf(TEXT("Zone %s requires an unknown/invalid discovery %s"),
                    *Zone.Id.ToString(), *RequiredDiscoveryId.ToString()));
        }
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
    Animals = MoveTemp(NewAnimals);
    FieldActions = MoveTemp(NewFieldActions);
    Infrastructures = MoveTemp(NewInfrastructures);
    StructureRepairs = MoveTemp(NewStructureRepairs);
    RaidDefenses = MoveTemp(NewRaidDefenses);
    Zones = MoveTemp(NewZones);
    Discoveries = MoveTemp(NewDiscoveries);
    Knowledge = MoveTemp(NewKnowledge);
    RouteNodes = MoveTemp(NewRouteNodes);
    RouteEdges = MoveTemp(NewRouteEdges);
    Calendar = NewCalendar;
    ClimateMonths = MoveTemp(NewMonths);
    Layers = MoveTemp(NewLayers);
    LastError.Reset();
    bLoaded = true;
    bApplied = false;
    return true;
}

const FGameDataKnowledge* UWorldGameDataSubsystem::FindKnowledge(FName Id) const
{
    return Knowledge.FindByPredicate([Id](const FGameDataKnowledge& Entry) { return Entry.Id == Id; });
}

const FGameDataFieldAction* UWorldGameDataSubsystem::FindFieldAction(FName Id) const
{
    return FieldActions.FindByPredicate([Id](const FGameDataFieldAction& Action) { return Action.Id == Id; });
}

const FGameDataInfrastructure* UWorldGameDataSubsystem::FindInfrastructure(FName Id) const
{
    return Infrastructures.FindByPredicate([Id](const FGameDataInfrastructure& Definition) { return Definition.Id == Id; });
}

const FGameDataStructureRepair* UWorldGameDataSubsystem::FindStructureRepair(FName Id) const
{
    return StructureRepairs.FindByPredicate([Id](const FGameDataStructureRepair& Repair) { return Repair.Id == Id; });
}

const FGameDataRaidDefense* UWorldGameDataSubsystem::FindRaidDefense(FName Id) const
{
    return RaidDefenses.FindByPredicate([Id](const FGameDataRaidDefense& Defense) { return Defense.Id == Id; });
}

const FGameDataTransformation* UWorldGameDataSubsystem::FindTransformation(FName Id) const
{
    return Transformations.FindByPredicate([Id](const FGameDataTransformation& T) { return T.Id == Id; });
}

bool UWorldGameDataSubsystem::CanEnterZone(
    const FGuid& PlayerEntityId, FName ZoneId, FString& OutReason) const
{
    OutReason.Reset();
    if (!bLoaded || !bApplied || !PlayerEntityId.IsValid() || ZoneId.IsNone())
    {
        OutReason = TEXT("Zone access needs loaded/applied game data and a registered player profile.");
        return false;
    }

    const FGameDataZone* Zone = Zones.FindByPredicate([ZoneId](const FGameDataZone& Candidate)
    {
        return Candidate.Id == ZoneId;
    });
    if (!Zone)
    {
        OutReason = FString::Printf(TEXT("Unknown zone %s."), *ZoneId.ToString());
        return false;
    }

    UWorld* World = GetWorld();
    const UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    const UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!Clock || !Survival)
    {
        OutReason = TEXT("Zone access requires the simulation clock and survival subsystem.");
        return false;
    }
    FWorldSurvivalProfile PlayerProfile;
    if (!Survival->GetProfile(PlayerEntityId, PlayerProfile))
    {
        OutReason = TEXT("Zone access requires a registered player survival profile.");
        return false;
    }

    if (!Zone->SeasonalMonths.IsEmpty())
    {
        FWorldCalendarDate Date;
        if (!Clock->IsClockInitialized() || !Clock->GetCalendarDate(Date))
        {
            OutReason = TEXT("Zone access requires an initialized world calendar.");
            return false;
        }
        if (!Zone->SeasonalMonths.Contains(Date.Month))
        {
            OutReason = FString::Printf(TEXT("%s is seasonally closed in month %d."),
                *Zone->Name, Date.Month);
            return false;
        }
    }

    for (const FName CapabilityId : Zone->RequiredCapabilityIds)
    {
        if (!Survival->HasEquippedCapability(PlayerEntityId, CapabilityId))
        {
            OutReason = FString::Printf(TEXT("%s requires equipped capability %s."),
                *Zone->Name, *CapabilityId.ToString());
            return false;
        }
    }

    const TArray<FWorldDiscoveryRecord> RecordedDiscoveries = Survival->GetDiscoveries();
    for (const FName DiscoveryId : Zone->RequiredDiscoveryIds)
    {
        if (!RecordedDiscoveries.ContainsByPredicate([DiscoveryId, PlayerEntityId](const FWorldDiscoveryRecord& Record)
            {
                return Record.DiscoveryId == DiscoveryId &&
                    (!Record.EntityId.IsValid() || Record.EntityId == PlayerEntityId);
            }))
        {
            OutReason = FString::Printf(TEXT("%s requires discovery %s."),
                *Zone->Name, *DiscoveryId.ToString());
            return false;
        }
    }

    return true;
}

bool UWorldGameDataSubsystem::TryRecordZoneEntry(
    const FGuid& OperationId, const FGuid& PlayerEntityId, FName ZoneId,
    FVector EntryLocation, FString& OutReason)
{
    OutReason.Reset();
    if (!OperationId.IsValid() || !PlayerEntityId.IsValid() ||
        ZoneId.IsNone() || EntryLocation.ContainsNaN())
    {
        OutReason = TEXT("Zone entry needs a valid operation, player and finite entry location.");
        return false;
    }

    if (!CanEnterZone(PlayerEntityId, ZoneId, OutReason))
        return false;

    const FGameDataZone* Zone = Zones.FindByPredicate([ZoneId](const FGameDataZone& Candidate)
    {
        return Candidate.Id == ZoneId;
    });
    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    FWorldSurvivalProfile Profile;
    if (!Zone || Zone->DiscoveryId.IsNone() || !Clock || !Clock->IsClockInitialized() ||
        !Survival || !Survival->GetProfile(PlayerEntityId, Profile))
    {
        OutReason = TEXT("Zone entry could not resolve its discovery, initialized clock or player profile.");
        return false;
    }

    if (!Survival->TryRecordConfiguredDiscovery(OperationId, PlayerEntityId,
        Profile.Revision, Zone->DiscoveryId, EntryLocation, Clock->GetSimulationTimeSeconds()))
    {
        OutReason = FString::Printf(TEXT("Zone %s could not be discovered: verify the prerequisite chain and operation identity."),
            *ZoneId.ToString());
        return false;
    }
    return true;
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
    UWorldLogisticsRouteSubsystem* Routes = World->GetSubsystem<UWorldLogisticsRouteSubsystem>();
    if (!Production || !Survival || !Clock || !Climate || !Terrain || !Agriculture || !Routes)
        return Fail(TEXT("A world subsystem is missing"));

    // Station recipes. A tool is put into the station and comes back with the output.
    for (const FGameDataTransformation& T : Transformations)
    {
        if (IsSpecialPlace(T.Where) || T.Inputs.Num() == 0)
            continue;
        FWorldProductionRecipe Recipe;
        Recipe.RecipeId = T.Id;
        for (const FGameDataDiscovery& Discovery : Discoveries)
        {
            if (Discovery.UnlockedDefinitionIds.Contains(T.Id))
            {
                Recipe.RequiredUnlockDefinitionId = T.Id;
                break;
            }
        }
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
        Definition.GrantedCapabilityIds = E.GrantedCapabilities;
        if (!Survival->RegisterEquipmentDefinition(Definition))
            return Fail(FString::Printf(TEXT("Survival refused equipment %s"), *E.ItemId.ToString()));
    }

    Clock->CalendarConfig = Calendar;
    Climate->Months = ClimateMonths;

    for (const FGameDataDiscovery& Discovery : Discoveries)
    {
        FWorldDiscoveryDefinition Definition;
        Definition.DiscoveryId = Discovery.DiscoveryId;
        Definition.PrerequisiteDiscoveryIds = Discovery.PrerequisiteDiscoveryIds;
        Definition.RequiredEquipmentDefinitionIds = Discovery.RequiredEquipmentDefinitionIds;
        Definition.UnlockedDefinitionIds = Discovery.UnlockedDefinitionIds;
        if (!Survival->RegisterDiscoveryDefinition(Definition))
            return Fail(FString::Printf(TEXT("Survival refused discovery %s"), *Discovery.DiscoveryId.ToString()));
    }

    for (const FGameDataRouteNode& Node : RouteNodes)
    {
        FLogisticsRouteNode RouteNode;
        RouteNode.NodeId = Node.NodeId;
        RouteNode.Location = Node.Location;
        RouteNode.bEnabled = Node.bEnabled;
        if (!Routes->UpsertNode(RouteNode))
            return Fail(FString::Printf(TEXT("Route graph refused node %s"), *Node.NodeId.ToString()));
    }
    for (const FGameDataRouteEdge& Edge : RouteEdges)
    {
        FLogisticsRouteEdge RouteEdge;
        RouteEdge.FromNode = Edge.FromNode;
        RouteEdge.ToNode = Edge.ToNode;
        RouteEdge.RegionId = Edge.RegionId;
        RouteEdge.Cost = Edge.Cost;
        RouteEdge.SlopeRatio = Edge.SlopeRatio;
        RouteEdge.SurfaceCostMultiplier = Edge.SurfaceCostMultiplier;
        RouteEdge.WeatherSensitivity = Edge.WeatherSensitivity;
        RouteEdge.LoadSensitivity = Edge.LoadSensitivity;
        RouteEdge.MissingInfrastructurePenalty = Edge.MissingInfrastructurePenalty;
        RouteEdge.bHasBuiltInfrastructure = Edge.bHasBuiltInfrastructure;
        RouteEdge.bRequiresInfrastructure = Edge.bRequiresInfrastructure;
        RouteEdge.InfrastructureId = Edge.InfrastructureId;
        RouteEdge.bEnabled = Edge.bEnabled;
        if (!Routes->UpsertEdge(RouteEdge))
            return Fail(FString::Printf(TEXT("Route graph refused edge %s -> %s"),
                *Edge.FromNode.ToString(), *Edge.ToNode.ToString()));
    }

    for (const FGameDataAnimal& Animal : Animals)
    {
        FWorldAnimalDefinition Definition;
        Definition.AnimalDefinitionId = Animal.Id;
        Definition.FeedDefinitionId = Animal.FeedItemId;
        Definition.ProductDefinitionId = Animal.ProductItemId;
        Definition.ProductUnitsPerInterval = Animal.ProductUnitsPerInterval;
        Definition.ProductIntervalSeconds = Animal.ProductIntervalSeconds;
        Definition.CaptureToolDefinitionIds = Animal.CaptureToolItemIds;
        for (const FGameDataItemAmount& Yield : Animal.SlaughterYields)
        {
            FWorldAnimalYieldDefinition OutYield;
            OutYield.ItemDefinitionId = Yield.ItemId;
            OutYield.Units = Yield.Quantity;
            Definition.SlaughterYields.Add(OutYield);
        }
        if (!Agriculture->RegisterAnimalDefinition(Definition))
            return Fail(FString::Printf(TEXT("Agriculture refused animal %s"), *Animal.Id.ToString()));
    }

    for (const FGameDataCrop& Crop : Crops)
    {
        FWorldCropDefinition Definition;
        Definition.CropDefinitionId = Crop.Id;
        Definition.SeedDefinitionId = Crop.SeedItemId;
        Definition.HarvestDefinitionId = Crop.HarvestItemId;
        Definition.HarvestUnits = Crop.HarvestUnits;
        Definition.SecondsPerStage = Crop.SecondsPerStage;
        Definition.PlantMonths = Crop.PlantMonths;
        Definition.SeedReturnUnits = Crop.SeedReturnUnits;
        Definition.SoilExhaustionPerHarvest = Crop.SoilExhaustionPerHarvest;
        Definition.MinimumFertility = Crop.MinimumFertility;
        Definition.MaximumPlantWeedPressure = Crop.MaximumPlantWeedPressure;
        if (!Agriculture->RegisterCropDefinition(Definition))
            return Fail(FString::Printf(TEXT("Agriculture refused crop %s"), *Crop.Id.ToString()));
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
        bAll = Inventory->RegisterRecipe(Recipe) && bAll;
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
