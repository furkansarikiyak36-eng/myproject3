#include "WorldRegionalEventSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WorldCalendar.h"
#include "WorldSimulationClockSubsystem.h"

namespace
{
    using FJson = TSharedPtr<FJsonObject>;

    bool ReadJsonObject(const FString& Directory, const TCHAR* FileName, FJson& Out, FString& Error)
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

    FString StringField(const FJson& Object, const TCHAR* Name, const FString& Default = FString())
    {
        FString Value;
        return Object.IsValid() && Object->TryGetStringField(Name, Value) ? Value : Default;
    }

    double NumberField(const FJson& Object, const TCHAR* Name, double Default = 0.0)
    {
        double Value = Default;
        return Object.IsValid() && Object->TryGetNumberField(Name, Value) ? Value : Default;
    }

    const TArray<TSharedPtr<FJsonValue>>* ArrayField(const FJson& Object, const TCHAR* Name)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        return Object.IsValid() && Object->TryGetArrayField(Name, Values) ? Values : nullptr;
    }

    bool ParseWeather(const FString& Text, EWorldWeatherState& OutWeather)
    {
        if (Text.Equals(TEXT("Clear"), ESearchCase::IgnoreCase)) OutWeather = EWorldWeatherState::Clear;
        else if (Text.Equals(TEXT("Rain"), ESearchCase::IgnoreCase)) OutWeather = EWorldWeatherState::Rain;
        else if (Text.Equals(TEXT("Wind"), ESearchCase::IgnoreCase)) OutWeather = EWorldWeatherState::Wind;
        else if (Text.Equals(TEXT("Snow"), ESearchCase::IgnoreCase)) OutWeather = EWorldWeatherState::Snow;
        else if (Text.Equals(TEXT("Storm"), ESearchCase::IgnoreCase)) OutWeather = EWorldWeatherState::Storm;
        else if (Text.Equals(TEXT("Heatwave"), ESearchCase::IgnoreCase)) OutWeather = EWorldWeatherState::Heatwave;
        else return false;
        return true;
    }

    uint32 StableStringHash(const FString& Value)
    {
        uint32 Hash = 2166136261u;
        for (TCHAR Character : Value)
        {
            Hash ^= static_cast<uint32>(Character);
            Hash *= 16777619u;
        }
        return Hash;
    }

    float StableRoll01(int32 Seed, int64 DayIndex, FName RegionId, FName EventId, uint32 Salt)
    {
        uint64 Value = static_cast<uint64>(static_cast<uint32>(Seed)) * 0x9E3779B97F4A7C15ull;
        Value ^= static_cast<uint64>(DayIndex) + 0xBF58476D1CE4E5B9ull + (Value << 6) + (Value >> 2);
        Value ^= static_cast<uint64>(StableStringHash(RegionId.ToString())) * 0x94D049BB133111EBull;
        Value ^= static_cast<uint64>(StableStringHash(EventId.ToString())) * 0xD6E8FEB86659FD93ull;
        Value ^= static_cast<uint64>(Salt) * 0xA24BAED4963EE407ull;
        Value ^= Value >> 30;
        Value *= 0xBF58476D1CE4E5B9ull;
        Value ^= Value >> 27;
        Value *= 0x94D049BB133111EBull;
        Value ^= Value >> 31;
        return static_cast<float>((Value >> 40) & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    bool IsFiniteRange(float Value, float Minimum, float Maximum)
    {
        return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
    }
}

void UWorldRegionalEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (!LoadDefaultDefinitions())
    {
        UE_LOG(LogTemp, Warning, TEXT("Regional events are unavailable: %s"), *LastError);
    }
}

bool UWorldRegionalEventSubsystem::LoadDefaultDefinitions()
{
    return LoadDefinitionsFromDirectory(FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("GameData")));
}

bool UWorldRegionalEventSubsystem::LoadDefinitionsFromDirectory(const FString& Directory)
{
    FString Error;
    FJson Root;
    if (!ReadJsonObject(Directory, TEXT("events.json"), Root, Error))
    {
        LastError = Error;
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* RegionValues = ArrayField(Root, TEXT("regions"));
    const TArray<TSharedPtr<FJsonValue>>* EventValues = ArrayField(Root, TEXT("events"));
    if (!RegionValues || RegionValues->IsEmpty() || RegionValues->Num() > 128 ||
        !EventValues || EventValues->IsEmpty() || EventValues->Num() > 512)
    {
        LastError = TEXT("events.json must include 1–128 regions and 1–512 events");
        return false;
    }

    TMap<FName, FWorldRegionProfile> NewRegions;
    for (const TSharedPtr<FJsonValue>& Value : *RegionValues)
    {
        const FJson Entry = Value.IsValid() ? Value->AsObject() : nullptr;
        FWorldRegionProfile Region;
        Region.Id = FName(*StringField(Entry, TEXT("id")));
        Region.ElevationMeters = static_cast<float>(NumberField(Entry, TEXT("elevation_m")));
        Region.SlopeRatio = static_cast<float>(NumberField(Entry, TEXT("slope_ratio")));
        Region.FloodExposure = static_cast<float>(NumberField(Entry, TEXT("flood_exposure")));
        Region.VegetationFuelLoad = static_cast<float>(NumberField(Entry, TEXT("vegetation_fuel_load")));
        Region.SnowLoadUnitsPerCmM2 = static_cast<float>(NumberField(Entry, TEXT("snow_load_units_per_cm_m2")));
        const TArray<TSharedPtr<FJsonValue>>* Target = ArrayField(Entry, TEXT("raid_target"));
        if (!Entry.IsValid() || Region.Id.IsNone() || NewRegions.Contains(Region.Id) ||
            !FMath::IsFinite(Region.ElevationMeters) ||
            !IsFiniteRange(Region.SlopeRatio, 0.0f, 100.0f) ||
            !IsFiniteRange(Region.FloodExposure, 0.0f, 1.0f) ||
            !IsFiniteRange(Region.VegetationFuelLoad, 0.0f, 1.0f) ||
            !IsFiniteRange(Region.SnowLoadUnitsPerCmM2, 0.0f, 10000.0f) ||
            !Target || Target->Num() != 3)
        {
            LastError = FString::Printf(TEXT("Invalid or duplicate region definition %s"), *Region.Id.ToString());
            return false;
        }
        double Coordinates[3] = {0.0, 0.0, 0.0};
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            if (!(*Target)[Axis].IsValid() ||
                !(*Target)[Axis]->TryGetNumber(Coordinates[Axis]) ||
                !FMath::IsFinite(Coordinates[Axis]) ||
                FMath::Abs(Coordinates[Axis]) > 100000000.0)
            {
                LastError = FString::Printf(TEXT("Region %s has an invalid raid_target"), *Region.Id.ToString());
                return false;
            }
        }
        Region.RaidTargetLocation = FVector(Coordinates[0], Coordinates[1], Coordinates[2]);
        NewRegions.Add(Region.Id, Region);
    }

    TArray<FWorldRegionalEventDefinition> NewDefinitions;
    TMap<FName, int32> NewIndexById;
    for (const TSharedPtr<FJsonValue>& Value : *EventValues)
    {
        const FJson Entry = Value.IsValid() ? Value->AsObject() : nullptr;
        FWorldRegionalEventDefinition Definition;
        Definition.Id = FName(*StringField(Entry, TEXT("id")));
        Definition.Type = FName(*StringField(Entry, TEXT("type")));
        Definition.ChainParentEventId = FName(*StringField(Entry, TEXT("chain_parent_id")));
        Definition.MinTemperatureCelsius = static_cast<float>(NumberField(Entry, TEXT("min_temperature_c"), -100.0));
        Definition.MaxTemperatureCelsius = static_cast<float>(NumberField(Entry, TEXT("max_temperature_c"), 100.0));
        Definition.MinSnowDepthCm = static_cast<float>(NumberField(Entry, TEXT("min_snow_depth_cm")));
        Definition.MinGroundWetness = static_cast<float>(NumberField(Entry, TEXT("min_ground_wetness")));
        Definition.MaxGroundWetness = static_cast<float>(NumberField(Entry, TEXT("max_ground_wetness"), 1.0));
        Definition.MinSlopeRatio = static_cast<float>(NumberField(Entry, TEXT("min_slope_ratio")));
        Definition.MinElevationMeters = static_cast<float>(NumberField(Entry, TEXT("min_elevation_m"), -10000.0));
        Definition.MinFloodExposure = static_cast<float>(NumberField(Entry, TEXT("min_flood_exposure")));
        Definition.MinVegetationFuelLoad = static_cast<float>(NumberField(Entry, TEXT("min_vegetation_fuel_load")));
        Definition.ChancePerDay = static_cast<float>(NumberField(Entry, TEXT("chance_per_day")));
        Definition.DurationHours = static_cast<float>(NumberField(Entry, TEXT("duration_hours"), 1.0));
        Definition.StructureDamagePerHour = static_cast<float>(NumberField(Entry, TEXT("structure_damage_per_hour")));
        Definition.RouteCostMultiplier = static_cast<float>(NumberField(Entry, TEXT("route_cost_multiplier"), 1.0));
        Definition.RaidDefenseMultiplier = static_cast<float>(NumberField(Entry, TEXT("raid_defense_multiplier"), 1.0));
        Definition.SnowLoadMultiplier = static_cast<float>(NumberField(Entry, TEXT("snow_load_multiplier"), 1.0));
        Definition.SurvivalSeverityMultiplier = static_cast<float>(NumberField(Entry, TEXT("survival_severity_multiplier"), 1.0));

        const FString EventType = Definition.Type.ToString();
        const bool bSupportedType =
            EventType == TEXT("Flood") || EventType == TEXT("Frost") ||
            EventType == TEXT("Hail") || EventType == TEXT("Drought") ||
            EventType == TEXT("Wildfire") || EventType == TEXT("ToxicRain") ||
            EventType == TEXT("Epidemic") || EventType == TEXT("Snowstorm") ||
            EventType == TEXT("Avalanche") || EventType == TEXT("Fog") ||
            EventType == TEXT("WindStorm") || EventType == TEXT("Lightning");
        if (!Entry.IsValid() || Definition.Id.IsNone() || Definition.Type.IsNone() || !bSupportedType ||
            NewIndexById.Contains(Definition.Id) ||
            !FMath::IsFinite(Definition.MinTemperatureCelsius) ||
            !FMath::IsFinite(Definition.MaxTemperatureCelsius) ||
            Definition.MinTemperatureCelsius > Definition.MaxTemperatureCelsius ||
            !IsFiniteRange(Definition.MinSnowDepthCm, 0.0f, 100000.0f) ||
            !IsFiniteRange(Definition.MinGroundWetness, 0.0f, 1.0f) ||
            !IsFiniteRange(Definition.MaxGroundWetness, 0.0f, 1.0f) ||
            Definition.MinGroundWetness > Definition.MaxGroundWetness ||
            !IsFiniteRange(Definition.MinSlopeRatio, 0.0f, 100.0f) ||
            !FMath::IsFinite(Definition.MinElevationMeters) ||
            !IsFiniteRange(Definition.MinFloodExposure, 0.0f, 1.0f) ||
            !IsFiniteRange(Definition.MinVegetationFuelLoad, 0.0f, 1.0f) ||
            !IsFiniteRange(Definition.ChancePerDay, 0.0f, 1.0f) ||
            !IsFiniteRange(Definition.DurationHours, 0.01f, 72.0f) ||
            !IsFiniteRange(Definition.StructureDamagePerHour, 0.0f, 1000000.0f) ||
            !IsFiniteRange(Definition.RouteCostMultiplier, 0.01f, 1000.0f) ||
            !IsFiniteRange(Definition.RaidDefenseMultiplier, 0.01f, 100.0f) ||
            !IsFiniteRange(Definition.SnowLoadMultiplier, 0.01f, 100.0f) ||
            !IsFiniteRange(Definition.SurvivalSeverityMultiplier, 0.01f, 100.0f))
        {
            LastError = FString::Printf(TEXT("Invalid or duplicate event definition %s"), *Definition.Id.ToString());
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* RegionIds = ArrayField(Entry, TEXT("regions"));
        if (RegionIds)
        {
            TSet<FName> Seen;
            for (const TSharedPtr<FJsonValue>& RegionValue : *RegionIds)
            {
                const FName RegionId(*RegionValue->AsString());
                if (RegionId.IsNone() || !NewRegions.Contains(RegionId) || Seen.Contains(RegionId))
                {
                    LastError = FString::Printf(TEXT("Event %s references an unknown/repeated region"),
                        *Definition.Id.ToString());
                    return false;
                }
                Seen.Add(RegionId);
                Definition.RegionIds.Add(RegionId);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Months = ArrayField(Entry, TEXT("months"));
        if (Months)
        {
            TSet<int32> SeenMonths;
            for (const TSharedPtr<FJsonValue>& MonthValue : *Months)
            {
                double MonthNumber = 0.0;
                if (!MonthValue.IsValid() || !MonthValue->TryGetNumber(MonthNumber) ||
                    FMath::FloorToDouble(MonthNumber) != MonthNumber ||
                    MonthNumber < 1.0 || MonthNumber > 12.0 ||
                    SeenMonths.Contains(static_cast<int32>(MonthNumber)))
                {
                    LastError = FString::Printf(TEXT("Event %s has an invalid/repeated month"),
                        *Definition.Id.ToString());
                    return false;
                }
                SeenMonths.Add(static_cast<int32>(MonthNumber));
                Definition.Months.Add(static_cast<int32>(MonthNumber));
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* WeatherValues = ArrayField(Entry, TEXT("required_weather"));
        if (WeatherValues)
        {
            TSet<uint8> SeenWeather;
            for (const TSharedPtr<FJsonValue>& WeatherValue : *WeatherValues)
            {
                EWorldWeatherState Weather = EWorldWeatherState::Clear;
                if (!WeatherValue.IsValid() ||
                    !ParseWeather(WeatherValue->AsString(), Weather) ||
                    SeenWeather.Contains(static_cast<uint8>(Weather)))
                {
                    LastError = FString::Printf(TEXT("Event %s has an unknown/repeated weather state"),
                        *Definition.Id.ToString());
                    return false;
                }
                SeenWeather.Add(static_cast<uint8>(Weather));
                Definition.RequiredWeatherStates.Add(Weather);
            }
        }

        NewIndexById.Add(Definition.Id, NewDefinitions.Num());
        NewDefinitions.Add(Definition);
    }

    // Validate chain references and reject cycles before swapping any live definitions.
    for (const FWorldRegionalEventDefinition& Definition : NewDefinitions)
    {
        if (!Definition.ChainParentEventId.IsNone() &&
            (!NewIndexById.Contains(Definition.ChainParentEventId) ||
             Definition.ChainParentEventId == Definition.Id))
        {
            LastError = FString::Printf(TEXT("Event %s has an unknown or self-referencing chain parent"),
                *Definition.Id.ToString());
            return false;
        }
        TSet<FName> Visiting;
        FName Current = Definition.Id;
        while (!Current.IsNone())
        {
            if (Visiting.Contains(Current))
            {
                LastError = FString::Printf(TEXT("Event chain contains a cycle at %s"), *Current.ToString());
                return false;
            }
            Visiting.Add(Current);
            const int32* Index = NewIndexById.Find(Current);
            if (!Index || !NewDefinitions.IsValidIndex(*Index))
                break;
            Current = NewDefinitions[*Index].ChainParentEventId;
        }
    }

    NewDefinitions.Sort([](const FWorldRegionalEventDefinition& A, const FWorldRegionalEventDefinition& B)
    {
        return A.Id.ToString() < B.Id.ToString();
    });
    NewIndexById.Reset();
    for (int32 Index = 0; Index < NewDefinitions.Num(); ++Index)
        NewIndexById.Add(NewDefinitions[Index].Id, Index);

    Regions = MoveTemp(NewRegions);
    EventDefinitions = MoveTemp(NewDefinitions);
    EventIndexById = MoveTemp(NewIndexById);
    LastError.Reset();
    bDefinitionsLoaded = true;
    return true;
}

bool UWorldRegionalEventSubsystem::GetRegionProfile(FName RegionId, FWorldRegionProfile& OutProfile) const
{
    const FWorldRegionProfile* Found = Regions.Find(RegionId);
    if (!bDefinitionsLoaded || !Found)
        return false;
    OutProfile = *Found;
    return true;
}

TArray<FName> UWorldRegionalEventSubsystem::GetRegionIds() const
{
    TArray<FName> Result;
    if (!bDefinitionsLoaded)
        return Result;
    Regions.GetKeys(Result);
    Result.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
    return Result;
}

bool UWorldRegionalEventSubsystem::GetEventDefinition(
    FName EventId, FWorldRegionalEventDefinition& OutDefinition) const
{
    OutDefinition = FWorldRegionalEventDefinition();
    const int32* Index = EventIndexById.Find(EventId);
    if (!Index || !EventDefinitions.IsValidIndex(*Index))
        return false;
    OutDefinition = EventDefinitions[*Index];
    return true;
}

float UWorldRegionalEventSubsystem::GetSnowLoadUnitsPerCmM2(FName RegionId) const
{
    const FWorldRegionProfile* Found = bDefinitionsLoaded ? Regions.Find(RegionId) : nullptr;
    return Found ? Found->SnowLoadUnitsPerCmM2 : 0.0f;
}

bool UWorldRegionalEventSubsystem::EvaluateRegion(
    FName RegionId, FWorldRegionalEventEvaluation& OutEvaluation) const
{
    OutEvaluation = FWorldRegionalEventEvaluation();
    OutEvaluation.RegionId = RegionId;
    if (!bDefinitionsLoaded || RegionId.IsNone() || !Regions.Contains(RegionId))
        return false;

    const UWorld* World = GetWorld();
    const UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    const UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    FWorldCalendarDate Date;
    if (!Clock || !Clock->IsClockInitialized() || !Climate ||
        !Climate->IsClimateInitialized() || !Clock->GetCalendarDate(Date))
        return false;

    const FWorldRegionProfile& Region = Regions.FindChecked(RegionId);
    TMap<FName, bool> Memo;
    TSet<FName> Visiting;
    for (const FWorldRegionalEventDefinition& Definition : EventDefinitions)
    {
        if (!IsDefinitionActive(Definition, Region, Date.Month, Date.HourOfDay,
            Date.DaysElapsed, Clock->GetRandomSeed(), Climate->GetWeather(),
            Climate->GetTemperatureCelsius(), Climate->GetSnowDepthCm(),
            Climate->GetGroundWetness(), Memo, Visiting))
        {
            continue;
        }

        OutEvaluation.ActiveEventIds.Add(Definition.Id);
        OutEvaluation.RouteCostMultiplier = FMath::Clamp(
            OutEvaluation.RouteCostMultiplier * Definition.RouteCostMultiplier, 0.01f, 100000.0f);
        OutEvaluation.RaidDefenseMultiplier = FMath::Clamp(
            OutEvaluation.RaidDefenseMultiplier * Definition.RaidDefenseMultiplier, 0.01f, 100000.0f);
        OutEvaluation.SnowLoadMultiplier = FMath::Clamp(
            OutEvaluation.SnowLoadMultiplier * Definition.SnowLoadMultiplier, 0.01f, 100000.0f);
        OutEvaluation.StructureDamagePerHour = FMath::Clamp(
            OutEvaluation.StructureDamagePerHour + Definition.StructureDamagePerHour, 0.0f, 1000000.0f);
        OutEvaluation.SurvivalSeverityMultiplier = FMath::Clamp(
            OutEvaluation.SurvivalSeverityMultiplier * Definition.SurvivalSeverityMultiplier, 0.01f, 100000.0f);
    }
    return true;
}

bool UWorldRegionalEventSubsystem::IsDefinitionActive(
    const FWorldRegionalEventDefinition& Definition,
    const FWorldRegionProfile& Region, int32 Month, float HourOfDay,
    int64 DayIndex, int32 RandomSeed, EWorldWeatherState Weather,
    float TemperatureCelsius, float SnowDepthCm, float GroundWetness,
    TMap<FName, bool>& Memo, TSet<FName>& Visiting) const
{
    if (const bool* Cached = Memo.Find(Definition.Id))
        return *Cached;
    if (Visiting.Contains(Definition.Id))
        return false;
    Visiting.Add(Definition.Id);

    bool bMatches = Definition.RegionIds.IsEmpty() || Definition.RegionIds.Contains(Region.Id);
    bMatches &= Definition.Months.IsEmpty() || Definition.Months.Contains(Month);
    bMatches &= Definition.RequiredWeatherStates.IsEmpty() || Definition.RequiredWeatherStates.Contains(Weather);
    bMatches &= TemperatureCelsius >= Definition.MinTemperatureCelsius &&
        TemperatureCelsius <= Definition.MaxTemperatureCelsius;
    bMatches &= SnowDepthCm >= Definition.MinSnowDepthCm;
    bMatches &= GroundWetness >= Definition.MinGroundWetness &&
        GroundWetness <= Definition.MaxGroundWetness;
    bMatches &= Region.SlopeRatio >= Definition.MinSlopeRatio;
    bMatches &= Region.ElevationMeters >= Definition.MinElevationMeters;
    bMatches &= Region.FloodExposure >= Definition.MinFloodExposure;
    bMatches &= Region.VegetationFuelLoad >= Definition.MinVegetationFuelLoad;

    bool bScheduledNow = false;
    if (bMatches && Definition.ChancePerDay > 0.0f)
    {
        const double AbsoluteHour = static_cast<double>(DayIndex) * 24.0 + HourOfDay;
        const int32 DaysToCheck = FMath::Min(4, FMath::CeilToInt(Definition.DurationHours / 24.0f) + 1);
        for (int32 Offset = 0; Offset < DaysToCheck; ++Offset)
        {
            const int64 EventDay = DayIndex - Offset;
            if (EventDay < 0)
                break;
            if (StableRoll01(RandomSeed, EventDay, Region.Id, Definition.Id, 0x10u) >= Definition.ChancePerDay)
                continue;
            const double StartHour = StableRoll01(RandomSeed, EventDay, Region.Id, Definition.Id, 0x20u) * 24.0;
            const double StartAbsoluteHour = static_cast<double>(EventDay) * 24.0 + StartHour;
            if (AbsoluteHour >= StartAbsoluteHour &&
                AbsoluteHour < StartAbsoluteHour + Definition.DurationHours)
            {
                bScheduledNow = true;
                break;
            }
        }
    }

    bool bActive = bMatches && bScheduledNow;
    if (bActive && !Definition.ChainParentEventId.IsNone())
    {
        const int32* ParentIndex = EventIndexById.Find(Definition.ChainParentEventId);
        bActive = ParentIndex && EventDefinitions.IsValidIndex(*ParentIndex) &&
            IsDefinitionActive(EventDefinitions[*ParentIndex], Region, Month, HourOfDay, DayIndex,
                RandomSeed, Weather, TemperatureCelsius, SnowDepthCm, GroundWetness, Memo, Visiting);
    }

    Visiting.Remove(Definition.Id);
    Memo.Add(Definition.Id, bActive);
    return bActive;
}
