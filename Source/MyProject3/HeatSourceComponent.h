#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HeatSourceComponent.generated.h"

enum class EWorldWeatherState : uint8;

USTRUCT(BlueprintType)
struct FHeatFuelDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Fuel") FName ItemDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Fuel", meta=(ClampMin="0.01", ClampMax="86400")) float BurnSecondsPerItem = 900.0f;
    /** Heat of this fuel: dry wood 1, wet wood ~0.6 (smoky, short), charcoal ~1.5 (hot enough for pottery). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Fuel", meta=(ClampMin="0.1", ClampMax="3")) float HeatOutputMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FHeatFuelOperationReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Heat|Fuel") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Heat|Fuel") FGuid ItemId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Heat|Fuel") FGuid InventoryOwnerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Heat|Fuel") int64 ExpectedItemRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Heat|Fuel") FName ItemDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Heat|Fuel") float AddedBurnSeconds = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Heat|Fuel") float ResultingFuelSeconds = 0.0f;
};

/**
 * One heat source model for campfires, hearths, fireplaces and kilns
 * (RISK-H02). Fuel burns on the shared simulation clock through the survival
 * subsystem; warmth falls off with distance inside the radius.
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UHeatSourceComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UHeatSourceComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Temperature added at the centre of the source, in degrees Celsius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat", meta=(ClampMin="0.0"))
    float HeatOutputCelsius = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat", meta=(ClampMin="10.0"))
    float RadiusCm = 450.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat", meta=(ClampMin="1.0"))
    float MaxFuelSeconds = 4.0f * 3600.0f;

    /** Visibility signature used by the raid/threat model while this source is lit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Threat", meta=(ClampMin="0.0", ClampMax="10.0"))
    float RaidLightSignature = 1.0f;

    /** Baseline smoke signature; the current fuel blend scales this in the raid model. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Threat", meta=(ClampMin="0.0", ClampMax="10.0"))
    float RaidSmokeSignature = 0.65f;

    /** 0 = open fire, 1 = fully sheltered hearth; rain and storms put out poorly protected fires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat", meta=(ClampMin="0.0", ClampMax="1.0"))
    float WeatherProtection = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Fuel", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxFuelReceipts = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Fuel", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaxFuelDefinitions = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Fuel")
    TArray<FHeatFuelDefinition> FuelDefinitions;

    UFUNCTION(BlueprintPure, Category="Heat")
    bool IsLit() const { return bLit; }

    UFUNCTION(BlueprintPure, Category="Heat")
    float GetFuelSeconds() const { return FuelSeconds; }

    /** Heat of the fuel now in the fire, weighted by its burn time (P3.1). */
    UFUNCTION(BlueprintPure, Category="Heat")
    float GetFuelHeatMultiplier() const { return FuelHeatMultiplier; }

    /** Lit and burning charcoal-hot: needed to fire clay (P4). */
    UFUNCTION(BlueprintPure, Category="Heat")
    bool IsBurningHot() const { return bLit && FuelHeatMultiplier >= 1.3f; }

    UFUNCTION(BlueprintPure, Category="Heat|Threat")
    float GetRaidLightSignature() const { return RaidLightSignature; }

    UFUNCTION(BlueprintPure, Category="Heat|Threat")
    float GetRaidSmokeSignature() const { return RaidSmokeSignature; }

    /** Internal numerical helper. Player-facing refuelling must consume an owned fuel item. */
    bool AddFuel(float BurnSeconds);

    UFUNCTION(BlueprintCallable, Category="Heat|Fuel")
    bool RegisterFuelDefinition(const FHeatFuelDefinition& Definition);

    /** Consumes one uniquely owned inventory fuel item and adds its authored burn duration once. */
    UFUNCTION(BlueprintCallable, Category="Heat|Fuel")
    bool TryAddFuelItem(const FGuid& OperationId, const FGuid& ItemId,
        int64 ExpectedItemRevision, const FGuid& InventoryOwnerId);

    /** Lights the source if it has fuel. */
    UFUNCTION(BlueprintCallable, Category="Heat")
    bool Ignite();

    UFUNCTION(BlueprintCallable, Category="Heat")
    void Extinguish();

    /** Warmth this source adds at a world location, in degrees Celsius. */
    UFUNCTION(BlueprintPure, Category="Heat")
    float GetHeatAt(const FVector& WorldLocation) const;

    /** Burns fuel for one simulation step; called by the survival subsystem only. */
    void AdvanceFuel(double DeltaSeconds, EWorldWeatherState Weather, float WeatherSeverity);

    /** Internal transaction rollback hook; restores only fuel/lit runtime state. */
    bool RestoreSimulationFuelState(float InFuelSeconds, bool bInLit, float InFuelHeatMultiplier = 1.0f);

private:
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Heat")
    float FuelSeconds = 0.0f;

    UPROPERTY(SaveGame, VisibleAnywhere, Category="Heat")
    bool bLit = false;

    UPROPERTY(SaveGame, VisibleAnywhere, Category="Heat")
    float FuelHeatMultiplier = 1.0f;

    UPROPERTY(SaveGame, VisibleAnywhere, Category="Heat|Fuel")
    TArray<FHeatFuelOperationReceipt> FuelReceipts;
};
