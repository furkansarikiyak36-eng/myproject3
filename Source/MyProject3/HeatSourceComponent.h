#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HeatSourceComponent.generated.h"

enum class EWorldWeatherState : uint8;

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

    /** 0 = open fire, 1 = fully sheltered hearth; rain and storms put out poorly protected fires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat", meta=(ClampMin="0.0", ClampMax="1.0"))
    float WeatherProtection = 0.0f;

    UFUNCTION(BlueprintPure, Category="Heat")
    bool IsLit() const { return bLit; }

    UFUNCTION(BlueprintPure, Category="Heat")
    float GetFuelSeconds() const { return FuelSeconds; }

    /** Adds burn time; the total never exceeds MaxFuelSeconds. */
    UFUNCTION(BlueprintCallable, Category="Heat")
    bool AddFuel(float BurnSeconds);

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

private:
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Heat")
    float FuelSeconds = 0.0f;

    UPROPERTY(SaveGame, VisibleAnywhere, Category="Heat")
    bool bLit = false;
};
