#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldSurvivalSubsystem.generated.h"

class UHeatSourceComponent;
class UInventoryCraftingComponent;

/** Runtime-only rollback record; weak references do not keep components alive. */
struct FWorldHeatSourceRuntimeState
{
    TWeakObjectPtr<UHeatSourceComponent> Source;
    float FuelSeconds = 0.0f;
    bool bLit = false;
};

UENUM(BlueprintType)
enum class EWorldWeatherState : uint8 { Clear, Rain, Wind, Snow, Storm, Heatwave };

/** Activities with a calorie and stamina cost (RISK-B01 activity table). */
UENUM(BlueprintType)
enum class ESurvivalActivity : uint8 { Walk, Run, Chop, Dig, Build, Carry };

USTRUCT(BlueprintType)
struct FWorldSurvivalConsumableDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Consumables") FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Consumables", meta=(ClampMin="0", ClampMax="10000")) float CaloriesKcal = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Consumables", meta=(ClampMin="0", ClampMax="100")) float ThirstRelief = 0.0f;
    /** 0 disables spoilage; otherwise food freshness is measured against the shared world clock. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Consumables", meta=(ClampMin="0")) double FreshnessDurationSeconds = 0.0;
    /** Energy retained after the freshness duration expires; 0 makes spoiled food inedible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Consumables", meta=(ClampMin="0", ClampMax="1")) float SpoiledEnergyFraction = 0.0f;
};

UENUM(BlueprintType)
enum class EWorldSurvivalConditionKind : uint8 { Injury, Illness, Poisoning };

UENUM(BlueprintType)
enum class EWorldEquipmentSlot : uint8 { Head, Body, Hands, Legs, Feet };

USTRUCT(BlueprintType)
struct FWorldSurvivalConditionDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions") FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions") EWorldSurvivalConditionKind Kind = EWorldSurvivalConditionKind::Injury;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions", meta=(ClampMin="0", ClampMax="100")) float HealthLossPerHour = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions", meta=(ClampMin="0", ClampMax="100")) float FatiguePerHour = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions", meta=(ClampMin="0", ClampMax="100")) float StaminaLossPerHour = 0.0f;
    /** 0 means the condition persists until treated. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions", meta=(ClampMin="0", ClampMax="604800")) double DurationSeconds = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions") FName TreatmentItemDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions", meta=(ClampMin="0", ClampMax="100")) float TreatmentHealthRecovery = 0.0f;
};

USTRUCT(BlueprintType)
struct FWorldSurvivalConditionState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions") FGuid ConditionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions") EWorldSurvivalConditionKind Kind = EWorldSurvivalConditionKind::Injury;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions") double StartedAtWorldTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions") double EndsAtWorldTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions", meta=(ClampMin="0", ClampMax="1")) float Severity = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions") int64 Revision = 1;
};

USTRUCT(BlueprintType)
struct FWorldSurvivalEquipmentDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment") FName ItemDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment") EWorldEquipmentSlot Slot = EWorldEquipmentSlot::Body;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment", meta=(ClampMin="0", ClampMax="1")) float Insulation = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment", meta=(ClampMin="0", ClampMax="1")) float WaterResistance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment") TArray<FName> GrantedCapabilityIds;
};

USTRUCT(BlueprintType)
struct FWorldEquippedItemState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Equipment") FGuid ItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Equipment") FName ItemDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Equipment") EWorldEquipmentSlot Slot = EWorldEquipmentSlot::Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Equipment") int64 ItemRevision = 0;
};

USTRUCT(BlueprintType)
struct FWorldDiscoveryDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") FName DiscoveryId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") TArray<FName> PrerequisiteDiscoveryIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") TArray<FName> RequiredEquipmentDefinitionIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") TArray<FName> UnlockedDefinitionIds;
};

UENUM(BlueprintType)
enum class EWorldSurvivalActionKind : uint8
{
    ConsumeFoodItem,
    BeginRest,\n    ApplyCondition,\n    TreatCondition,\n    EquipItem,\n    UnequipItem,\n    RecordDiscovery
};

/** Persistent idempotency receipt stored with the affected survival profile. */
USTRUCT(BlueprintType)
struct FWorldSurvivalActionReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FGuid EntityId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") EWorldSurvivalActionKind Action = EWorldSurvivalActionKind::ConsumeFoodItem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FGuid ItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FGuid ExpectedItemOwnerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") int64 ExpectedProfileRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") int64 ExpectedItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") int64 ResultingProfileRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") int64 ResultingItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FName ItemDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") float CaloriesKcal = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") float ThirstRelief = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") double RequestedStartTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") double RequestedDurationSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") double ResultingRestUntilTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FGuid RelatedConditionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FName ConditionDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FName DiscoveryId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") EWorldEquipmentSlot EquipmentSlot = EWorldEquipmentSlot::Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") double ConditionEndTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts", meta=(ClampMin="0", ClampMax="1")) float ConditionSeverity = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") FVector RequestedLocation = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") double RequestedDiscoveryTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct FWorldSurvivalProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival") FGuid EntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Health = 100.0f;
    /** Derived from Calories each step: 0 = full, 100 = empty store. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Hunger = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Thirst = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Stamina = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Fatigue = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float ClothingInsulation = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float ShelterProtection = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float Wetness = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival") int64 Revision = 1;

    /** Stored food energy in kilocalories. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0")) float CaloriesKcal = 2500.0f;
    /** Body core temperature in degrees Celsius; below 35 is hypothermia. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival") float CoreTemperatureCelsius = 37.0f;
    /** Where the entity stands; heat sources warm it by distance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival") FVector Location = FVector::ZeroVector;
    /** Activity energy reported since the last step; burned on the next step. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival") float PendingActivityKcal = 0.0f;
    /** Last computed felt temperature, for HUD and AI; the formula is authoritative. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival") float FeltTemperatureCelsius = 20.0f;
    /** A rest action is an interval on the shared world timeline, not a private timer. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival") double RestUntilTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Receipts") TArray<FWorldSurvivalActionReceipt> ActionReceipts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Conditions") TArray<FWorldSurvivalConditionState> Conditions;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Equipment") TArray<FWorldEquippedItemState> EquippedItems;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival|Equipment", meta=(ClampMin="0", ClampMax="1")) float ClothingWaterResistance = 0.0f;
};

USTRUCT(BlueprintType)
struct FWorldDiscoveryRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") FName DiscoveryId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") double DiscoveredAtTime = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Exploration") FGuid EntityId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Exploration") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Exploration") TArray<FName> UnlockedDefinitionIds;
};

/**
 * Bounded survival/exposure model and persistent discovery registry.
 * Temperature is one felt-temperature function (RISK-H01); food is a calorie
 * store burned by metabolism, activity and shivering (RISK-B01).
 */
UCLASS()
class MYPROJECT3_API UWorldSurvivalSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") EWorldWeatherState Weather = EWorldWeatherState::Clear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float WeatherSeverity = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float HungerPerHour = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float ThirstPerHour = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float FatiguePerHour = 1.0f;
    /** Health lost per hour per degree of core temperature below the hypothermia threshold. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float ColdDamagePerHour = 4.0f;

    // Felt temperature inputs, degrees Celsius.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float AmbientTemperatureCelsius = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float ClothingWarmthCelsius = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float ShelterWarmthCelsius = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float WetnessPenaltyCelsius = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float ComfortTemperatureCelsius = 18.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float HypothermiaThresholdCelsius = 35.0f;
    /** Core temperature change per hour per degree of felt temperature away from comfort. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float CoreCoolingPerDegreeHour = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Temperature") float CoreRecoveryPerHour = 1.0f;

    // Calorie model.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Calories") float MaxCaloriesKcal = 2500.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Calories") float BasalKcalPerHour = 80.0f;
    /** Extra burn per hour per degree below comfort (shivering). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Calories") float ShiverKcalPerDegreeHour = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Calories") float CarryKcalPerKgHour = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Consumables", meta=(ClampMin="1", ClampMax="4096")) int32 MaxConsumableDefinitions = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Receipts", meta=(ClampMin="1", ClampMax="4096")) int32 MaxActionReceiptsPerProfile = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Rest", meta=(ClampMin="0", ClampMax="12")) float RestFatigueRecoveryMultiplier = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Rest", meta=(ClampMin="0")) float RestStaminaRecoveryPerHour = 25.0f;

    UFUNCTION(BlueprintCallable, Category="World|Survival") bool RegisterProfile(const FWorldSurvivalProfile& Profile);
    /** Legacy import helper. Gameplay adapters should update authored shelter protection separately. */
    bool UpdateProtection(const FGuid& EntityId, float ClothingInsulation, float ShelterProtection, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Survival")
    bool UpdateShelterProtection(const FGuid& EntityId, float ShelterProtection, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool SetWeather(EWorldWeatherState NewWeather, float Severity);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool UpdateLocation(const FGuid& EntityId, FVector NewLocation);
    /** Adds the calorie and stamina cost of an activity; the energy burns on the next step. */
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool RecordActivity(const FGuid& EntityId, ESurvivalActivity Activity, float DurationSeconds, float CarriedMassKg);
    /** Data-driven consumable registry. Unknown definitions fail closed. */
    UFUNCTION(BlueprintCallable, Category="World|Survival|Consumables")
    bool RegisterConsumableDefinition(const FWorldSurvivalConsumableDefinition& Definition);
    /** Consumes one uniquely owned inventory item and applies its registered effects once. */
    UFUNCTION(BlueprintCallable, Category="World|Survival|Consumables")
    bool TryConsumeFoodItem(const FGuid& OperationId, const FGuid& EntityId,
        int64 ExpectedProfileRevision, const FGuid& ItemId, int64 ExpectedItemRevision,
        const FGuid& ExpectedInventoryOwnerId);
    /** Begins a bounded rest interval on the common world clock; it does not create a second clock. */
    UFUNCTION(BlueprintCallable, Category="World|Survival")
    bool TryBeginRest(const FGuid& OperationId, const FGuid& EntityId,
        int64 ExpectedProfileRevision, double DurationSeconds);
    /** Legacy numerical helper retained for controlled systems; player food interactions should use TryConsumeFoodItem. */
    bool ConsumeFood(const FGuid& EntityId, float Kcal, float ThirstRelief);

    UFUNCTION(BlueprintCallable, Category="World|Survival|Conditions")
    bool RegisterConditionDefinition(const FWorldSurvivalConditionDefinition& Definition);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Conditions")
    bool TryApplyCondition(const FGuid& OperationId, const FGuid& EntityId,
        int64 ExpectedProfileRevision, FName ConditionDefinitionId, float Severity = 1.0f);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Conditions")
    bool TryTreatCondition(const FGuid& OperationId, const FGuid& EntityId,
        int64 ExpectedProfileRevision, const FGuid& ConditionId,
        const FGuid& TreatmentItemId, int64 ExpectedItemRevision,
        const FGuid& ExpectedInventoryOwnerId);

    UFUNCTION(BlueprintCallable, Category="World|Survival|Equipment")
    bool RegisterEquipmentDefinition(const FWorldSurvivalEquipmentDefinition& Definition);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Equipment")
    bool TryEquipItem(const FGuid& OperationId, const FGuid& EntityId,
        int64 ExpectedProfileRevision, const FGuid& ItemId, int64 ExpectedItemRevision,
        const FGuid& ExpectedInventoryOwnerId);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Equipment")
    bool TryUnequipItem(const FGuid& OperationId, const FGuid& EntityId,
        int64 ExpectedProfileRevision, EWorldEquipmentSlot Slot,
        const FGuid& ExpectedInventoryOwnerId);

    UFUNCTION(BlueprintCallable, Category="World|Exploration")
    bool RegisterDiscoveryDefinition(const FWorldDiscoveryDefinition& Definition);
    UFUNCTION(BlueprintCallable, Category="World|Exploration")
    bool TryRecordConfiguredDiscovery(const FGuid& OperationId, const FGuid& EntityId,
        int64 ExpectedProfileRevision, FName DiscoveryId, FVector Location,
        double LogicalTimeSeconds);
    UFUNCTION(BlueprintPure, Category="World|Exploration")
    bool IsDefinitionUnlocked(const FGuid& EntityId, FName DefinitionId) const;
    UFUNCTION(BlueprintPure, Category="World|Survival|Equipment")
    bool HasEquippedCapability(const FGuid& EntityId, FName CapabilityId) const;
    UFUNCTION(BlueprintPure, Category="World|Survival") float GetActivityKcalPerHour(ESurvivalActivity Activity) const;
    /** The single felt-temperature function every system reads. */
    UFUNCTION(BlueprintPure, Category="World|Survival") float ComputeFeltTemperature(const FWorldSurvivalProfile& Profile) const;
    UFUNCTION(BlueprintPure, Category="World|Survival") float GetHeatAt(const FVector& Location) const;
    UFUNCTION(BlueprintPure, Category="World|Survival") bool CanAdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool AdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds);
    /** Legacy import/dev helper. Player interactions must use TryRecordConfiguredDiscovery. */
    bool RecordDiscovery(FName DiscoveryId, FVector Location, double LogicalTimeSeconds);
    UFUNCTION(BlueprintPure, Category="World|Survival") bool GetProfile(const FGuid& EntityId, FWorldSurvivalProfile& OutProfile) const;
    UFUNCTION(BlueprintPure, Category="World|Exploration") TArray<FWorldDiscoveryRecord> GetDiscoveries() const { return Discoveries; }
    void ExportSnapshot(TArray<FWorldSurvivalProfile>& OutProfiles, TArray<FWorldDiscoveryRecord>& OutDiscoveries, EWorldWeatherState& OutWeather, float& OutSeverity) const;
    bool RestoreSnapshot(const TArray<FWorldSurvivalProfile>& ProfileSnapshot, const TArray<FWorldDiscoveryRecord>& DiscoverySnapshot, EWorldWeatherState InWeather, float InSeverity);

    void RegisterHeatSource(UHeatSourceComponent* Source);
    void UnregisterHeatSource(UHeatSourceComponent* Source);

    /** Captures transient heat state so a failed fixed step can roll back fuel and ignition state. */
    void CaptureHeatSourceRuntimeState(TArray<FWorldHeatSourceRuntimeState>& OutStates) const;
    bool RestoreHeatSourceRuntimeState(const TArray<FWorldHeatSourceRuntimeState>& Snapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Survival") TMap<FGuid, FWorldSurvivalProfile> Profiles;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Exploration") TArray<FWorldDiscoveryRecord> Discoveries;
    UPROPERTY(Transient) TArray<TWeakObjectPtr<UHeatSourceComponent>> HeatSources;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Consumables") TArray<FWorldSurvivalConsumableDefinition> ConsumableDefinitions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions", meta=(ClampMin="1", ClampMax="1024")) int32 MaxConditionDefinitions = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Conditions") TArray<FWorldSurvivalConditionDefinition> ConditionDefinitions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment", meta=(ClampMin="1", ClampMax="1024")) int32 MaxEquipmentDefinitions = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment") TArray<FWorldSurvivalEquipmentDefinition> EquipmentDefinitions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Equipment", meta=(ClampMin="1", ClampMax="1024")) int32 MaxEquippedItemsPerProfile = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration", meta=(ClampMin="1", ClampMax="4096")) int32 MaxDiscoveryDefinitions = 512;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") TArray<FWorldDiscoveryDefinition> DiscoveryDefinitions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration", meta=(ClampMin="1", ClampMax="16384")) int32 MaxDiscoveryRecords = 4096;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Receipts", meta=(ClampMin="1", ClampMax="8192")) int32 MaxDiscoveryReceiptsPerProfile = 512;

    bool IsValidProfile(const FWorldSurvivalProfile& Profile) const;
    const FWorldSurvivalConditionDefinition* FindConditionDefinition(FName DefinitionId) const;
    const FWorldSurvivalEquipmentDefinition* FindEquipmentDefinition(FName ItemDefinitionId) const;
    const FWorldDiscoveryDefinition* FindDiscoveryDefinition(FName DiscoveryId) const;
    bool FindInventoryForOwner(const FGuid& InventoryOwnerId, class UInventoryCraftingComponent*& OutInventory) const;
    void RecomputeEquipmentProtection(FWorldSurvivalProfile& Profile) const;
    const FWorldSurvivalConsumableDefinition* FindConsumableDefinition(FName DefinitionId) const;
    const FWorldSurvivalActionReceipt* FindActionReceipt(const FGuid& OperationId) const;
    float GetWeatherTemperatureDelta() const;
    float GetWindChill() const;
};
