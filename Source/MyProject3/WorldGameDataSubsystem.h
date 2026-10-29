#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldCalendar.h"
#include "WorldClimateSubsystem.h"
#include "WorldGameDataSubsystem.generated.h"

class UInventoryCraftingComponent;
class UHeatSourceComponent;

USTRUCT(BlueprintType)
struct FGameDataItemAmount
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName ItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") int32 Quantity = 1;
};

/** One item of the primitive era (Config/GameData/items.json). */
USTRUCT(BlueprintType)
struct FGameDataItem
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName Id = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName Group = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") int32 WeightGrams = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName Category = NAME_None;
    /** A physical building part (mesh + snap); its geometry work is frozen until P10. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") bool bPhysical = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") bool bConsumable = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") bool bFuel = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") bool bEquipment = false;
    /** Relative work strength used by player-facing axe/dig tool actions; zero for non-tools. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData", meta=(ClampMin="0.0", ClampMax="10.0")) float ToolStrength = 0.0f;
};

/** One transformation of the plan (Config/GameData/transformations.json). */
USTRUCT(BlueprintType)
struct FGameDataTransformation
{
    GENERATED_BODY()
    /** Number in the plan's list (1..38); several rows can share one. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") int32 PlanNumber = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName Id = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FString Name;
    /** True = physical part (F), false = item recipe (E). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") bool bPhysical = false;
    /** "hand", a station id, "world", "log_system" or "terrain". */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName Where = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") TArray<FGameDataItemAmount> Inputs;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") TArray<FGameDataItemAmount> Outputs;
    /** Needed but not used up. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") TArray<FName> Tools;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") double DurationSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct FGameDataStation
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName Id = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") int32 Capacity = 16;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") bool bNeedsFire = false;
    /** Needs a charcoal-hot fire (pottery). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") bool bNeedsHotFire = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") float SpoilageRate = 1.0f;
};

USTRUCT(BlueprintType)
struct FGameDataCrop
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName Id = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName SeedItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") FName HarvestItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") int32 HarvestUnits = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") int32 SeedReturnUnits = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") double SecondsPerStage = 3600.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") TArray<int32> PlantMonths;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData") int32 FoundInZone = 0;
};

/** Authored trigger/effect contract for natural world events (P1.3/P8.1). */
USTRUCT(BlueprintType)
struct FGameDataWorldEvent
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") FName Id = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") FName Trigger = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") FName Signal = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") FName Effect = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") double WarningSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") double DurationSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") float TriggerMinimum = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") float TriggerMaximum = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") float MinimumSeverity = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") float MaximumSeverity = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") FName CounterItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GameData|Events") int32 CounterQuantity = 0;
};

/**
 * Loads and checks the primitive-era game data (Config/GameData/*.json, P0.3/P0.4) and
 * hands it to the systems that use it: production recipes and stations, survival
 * consumables / conditions / equipment, heat fuels, climate months, the calendar, natural-event
 * contracts, and terrain resource layers. Hand recipes and fuels are per component (ApplyToInventory,
 * ApplyToHeatSource); components call those themselves once the data is applied.
 */
UCLASS()
class MYPROJECT3_API UWorldGameDataSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Reads every file from a folder (default Config/GameData) and checks all references. */
    UFUNCTION(BlueprintCallable, Category="GameData") bool LoadFromDirectory(const FString& Directory);
    UFUNCTION(BlueprintCallable, Category="GameData") bool LoadDefault();
    UFUNCTION(BlueprintPure, Category="GameData") bool IsLoaded() const { return bLoaded; }

    /** Registers world-level data: recipes, survival definitions, climate, calendar, terrain layers. */
    UFUNCTION(BlueprintCallable, Category="GameData") bool ApplyToWorld();
    UFUNCTION(BlueprintPure, Category="GameData") bool IsApplied() const { return bApplied; }
    /** Hand recipes (tools are kept, not used up) and item weights. */
    UFUNCTION(BlueprintCallable, Category="GameData") bool ApplyToInventory(UInventoryCraftingComponent* Inventory) const;
    UFUNCTION(BlueprintCallable, Category="GameData") bool ApplyToHeatSource(UHeatSourceComponent* HeatSource) const;

    /** Why the last load failed (empty after a good load). */
    UFUNCTION(BlueprintPure, Category="GameData") FString GetLastError() const { return LastError; }

    const TMap<FName, FGameDataItem>& GetItems() const { return Items; }
    /** Returns the authored strength for a loaded item categorized as Tool. */
    bool GetToolStrength(FName ItemId, float& OutStrength) const;
    const TArray<FGameDataTransformation>& GetTransformations() const { return Transformations; }
    const TMap<FName, FGameDataStation>& GetStations() const { return Stations; }
    const TArray<FGameDataCrop>& GetCrops() const { return Crops; }
    const TArray<FGameDataWorldEvent>& GetWorldEvents() const { return WorldEvents; }
    const FGameDataTransformation* FindTransformation(FName Id) const;

private:
    bool Fail(const FString& Message);

    bool bLoaded = false;
    bool bApplied = false;
    FString LastError;

    TMap<FName, FGameDataItem> Items;
    TArray<FGameDataTransformation> Transformations;
    TMap<FName, FGameDataStation> Stations;
    TArray<FGameDataCrop> Crops;
    TArray<FGameDataWorldEvent> WorldEvents;
    FWorldCalendarConfig Calendar;
    TArray<FWorldMonthClimate> ClimateMonths;

    // Kept as parsed JSON pieces that map 1:1 to existing definition structs.
    struct FConsumable { FName ItemId; float Kcal = 0; float Thirst = 0; double FreshnessSeconds = 0; float SpoiledFraction = 0; FName Condition; FName SpoiledCondition; float Severity = 0.5f; };
    struct FFuel { FName ItemId; float BurnSeconds = 0; float Heat = 1; };
    struct FEquipment { FName ItemId; FName Slot; float Insulation = 0; float WaterResistance = 0; };
    struct FCondition { FName Id; FName Kind; float HealthLossPerHour = 0; float FatiguePerHour = 0; float StaminaLossPerHour = 0; double DurationSeconds = 0; FName Treatment; float TreatmentRecovery = 0; };
    struct FLayer { FName Material; FName ItemId; int32 MinLayer = 0; int32 MaxLayer = 0; float UnitsPerCubicMetre = 1; };
    TArray<FConsumable> Consumables;
    TArray<FFuel> Fuels;
    TArray<FEquipment> Equipment;
    TArray<FCondition> Conditions;
    TArray<FLayer> Layers;
};
