#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldSurvivalSubsystem.generated.h"

class UHeatSourceComponent;

/** Runtime-only state needed to undo heat fuel changes after a rejected fixed step. */
struct FWorldHeatSourceRuntimeSnapshot
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

/** An active status effect resolved using shared in-game seconds. Rates apply at authored severity. */
USTRUCT(BlueprintType)
struct FWorldSurvivalAffliction
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival|Afflictions") FName AfflictionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival|Afflictions", meta=(ClampMin="0", ClampMax="1")) float Severity = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival|Afflictions", meta=(ClampMin="0")) double RemainingSeconds = 3600.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival|Afflictions", meta=(ClampMin="0")) float HealthLossPerHour = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival|Afflictions", meta=(ClampMin="0")) float ThirstGainPerHour = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival|Afflictions", meta=(ClampMin="0")) float FatigueGainPerHour = 0.0f;
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
    /** Logical rest action; physical bed/shelter eligibility is validated by its actor adapter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival") bool bResting = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival|Afflictions") TArray<FWorldSurvivalAffliction> Afflictions;
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
};

USTRUCT(BlueprintType)
struct FWorldDiscoveryDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") FName DiscoveryId = NAME_None;
    /** If set, the caller must prove possession/equipment of this authored tag. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") FName RequiredEquipmentTag = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration", meta=(ClampMin="10", ClampMax="10000")) float InteractionRadiusCm = 300.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") TArray<FName> UnlockedContentIds;
};

USTRUCT(BlueprintType)
struct FWorldDiscoveryRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") FName DiscoveryId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") double DiscoveredAtTime = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Exploration") TArray<FName> UnlockedContentIds;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Rest", meta=(ClampMin="0", ClampMax="100")) float RestFatigueRecoveryPerHour = 12.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Rest", meta=(ClampMin="0", ClampMax="100")) float RestStaminaRecoveryPerHour = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration|Limits", meta=(ClampMin="1", ClampMax="4096")) int32 MaxDiscoveryRecords = 4096;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Exploration") TArray<FWorldDiscoveryDefinition> DiscoveryDefinitions;
    /** Health lost per hour per degree of core temperature below the hypothermia threshold. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float ColdDamagePerHour = 4.0f;

    // Authored weather temperature deltas in degrees Celsius at severity 1.0.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float RainCoolingCelsius = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float WindCoolingCelsius = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float SnowCoolingCelsius = 14.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float StormCoolingCelsius = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float HeatwaveWarmingCelsius = 18.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float WindChillWindCelsius = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float WindChillStormCelsius = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="60")) float WindChillSnowCelsius = 4.0f;

    // Rates are expressed per in-game hour, not per rendered frame.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="1")) float RainWetnessPerHour = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="1")) float StormWetnessPerHour = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="1")) float BaseDryingPerHour = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="1")) float ShelterDryingBonusPerHour = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="1")) float WarmthDryingPerDegree = 0.01f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="0", ClampMax="10")) float HeatThirstPerDegreeHour = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival|Climate", meta=(ClampMin="-20", ClampMax="60")) float HeatThirstThresholdCelsius = 32.0f;

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

    UFUNCTION(BlueprintCallable, Category="World|Survival") bool RegisterProfile(const FWorldSurvivalProfile& Profile);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool UpdateProtection(const FGuid& EntityId, float ClothingInsulation, float ShelterProtection, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool SetWeather(EWorldWeatherState NewWeather, float Severity);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool UpdateLocation(const FGuid& EntityId, FVector NewLocation);
    /** Adds the calorie and stamina cost of an activity; the energy burns on the next step. */
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool RecordActivity(const FGuid& EntityId, ESurvivalActivity Activity, float DurationSeconds, float CarriedMassKg);
    /** Restores food energy and water; item-bound eating calls this after consuming inventory. */
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool ConsumeFood(const FGuid& EntityId, float Kcal, float ThirstRelief);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Rest") bool SetResting(const FGuid& EntityId, bool bResting, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Afflictions") bool TryApplyAffliction(
        const FGuid& EntityId, const FWorldSurvivalAffliction& Affliction, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Afflictions") bool TryRemoveAffliction(
        const FGuid& EntityId, FName AfflictionId, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Survival|Medical") bool TryApplyMedicalTreatment(
        const FGuid& EntityId, float HealthRestore, FName AfflictionId, int64 ExpectedRevision);
    UFUNCTION(BlueprintPure, Category="World|Survival") float GetActivityKcalPerHour(ESurvivalActivity Activity) const;
    /** The single felt-temperature function every system reads. */
    UFUNCTION(BlueprintPure, Category="World|Survival") float ComputeFeltTemperature(const FWorldSurvivalProfile& Profile) const;
    UFUNCTION(BlueprintPure, Category="World|Survival") float GetHeatAt(const FVector& Location) const;
    UFUNCTION(BlueprintPure, Category="World|Survival") bool CanAdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool AdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds);
    UFUNCTION(BlueprintCallable, Category="World|Exploration") bool RecordDiscovery(FName DiscoveryId, FVector Location, double LogicalTimeSeconds);
    UFUNCTION(BlueprintCallable, Category="World|Exploration") bool TryDiscover(
        FName DiscoveryId, FVector PlayerLocation, FVector DiscoveryLocation, double LogicalTimeSeconds,
        const TArray<FName>& AvailableEquipmentTags, TArray<FName>& OutUnlockedContentIds);
    UFUNCTION(BlueprintPure, Category="World|Exploration") bool IsContentUnlocked(FName ContentId) const;
    UFUNCTION(BlueprintPure, Category="World|Survival") bool GetProfile(const FGuid& EntityId, FWorldSurvivalProfile& OutProfile) const;
    UFUNCTION(BlueprintPure, Category="World|Exploration") TArray<FWorldDiscoveryRecord> GetDiscoveries() const { return Discoveries; }
    void ExportSnapshot(TArray<FWorldSurvivalProfile>& OutProfiles, TArray<FWorldDiscoveryRecord>& OutDiscoveries, EWorldWeatherState& OutWeather, float& OutSeverity) const;
    bool RestoreSnapshot(const TArray<FWorldSurvivalProfile>& ProfileSnapshot, const TArray<FWorldDiscoveryRecord>& DiscoverySnapshot, EWorldWeatherState InWeather, float InSeverity);

    void RegisterHeatSource(UHeatSourceComponent* Source);
    void UnregisterHeatSource(UHeatSourceComponent* Source);
    void ExportHeatSourceRuntimeSnapshot(TArray<FWorldHeatSourceRuntimeSnapshot>& OutSnapshot) const;
    bool RestoreHeatSourceRuntimeSnapshot(const TArray<FWorldHeatSourceRuntimeSnapshot>& Snapshot);

private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Survival") TMap<FGuid, FWorldSurvivalProfile> Profiles;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Exploration") TArray<FWorldDiscoveryRecord> Discoveries;
    UPROPERTY(Transient) TArray<TWeakObjectPtr<UHeatSourceComponent>> HeatSources;

    bool IsValidProfile(const FWorldSurvivalProfile& Profile) const;
    float GetWeatherTemperatureDelta() const;
    float GetWindChill() const;
};
