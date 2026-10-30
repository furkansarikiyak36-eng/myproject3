#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldClimateSubsystem.h"
#include "WorldCombustibleComponent.generated.h"

/**
 * SaveGame-backed fuel and moisture state for a burnable world surface.
 * This is deliberately separate from UHeatSourceComponent: a campfire consumes
 * fuel to heat a room; this component represents a tree, thatch roof, log pile
 * or other environmental material that can catch and spread wildfire.
 */
UCLASS(ClassGroup=(World), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UWorldCombustibleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWorldCombustibleComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire")
    FGuid CombustibleId;

    /** Optional explicit region override; otherwise the owning actor's Region:<id> tag is used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire")
    FName RegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire|Fuel", meta=(ClampMin="0", ClampMax="100000"))
    float FuelUnitsRemaining = 100.0f;

    /** 0 = dry, 1 = saturated. Rain/snow alter this on the shared logical tick. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire|Fuel", meta=(ClampMin="0", ClampMax="1"))
    float Moisture = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire|Ignition", meta=(ClampMin="0", ClampMax="1"))
    float IgnitionMoistureThreshold = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire|Ignition", meta=(ClampMin="0", ClampMax="4"))
    float IgnitionSusceptibility = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire|Spread", meta=(ClampMin="0", ClampMax="50000"))
    float SpreadRadiusCm = 450.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire|Spread", meta=(ClampMin="0", ClampMax="4"))
    float SpreadChanceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Fire|Fuel", meta=(ClampMin="0", ClampMax="100000"))
    float BurnUnitsPerHour = 12.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Fire")
    bool bBurning = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Fire", meta=(ClampMin="0", ClampMax="1"))
    float FireIntensity = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Fire|Receipts")
    int64 LastBurnTickIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Fire|Receipts")
    int64 LastMoistureTickIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Fire|Receipts")
    TArray<FGuid> IgnitionOperationIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Fire|Limits", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxIgnitionReceipts = 128;

    UFUNCTION(BlueprintCallable, Category="World|Fire")
    bool EnsureCombustibleIdentity();

    UFUNCTION(BlueprintPure, Category="World|Fire")
    FName ResolveRegionId() const;

    UFUNCTION(BlueprintPure, Category="World|Fire")
    bool IsBurning() const { return bBurning; }

    UFUNCTION(BlueprintPure, Category="World|Fire")
    float GetFuelUnitsRemaining() const { return FuelUnitsRemaining; }

    UFUNCTION(BlueprintPure, Category="World|Fire")
    float GetMoisture() const { return Moisture; }

    UFUNCTION(BlueprintCallable, Category="World|Fire")
    bool TryIgnite(const FGuid& OperationId, float Intensity = 1.0f);

    /** Burns fuel at most once for a committed fixed-step tick. */
    bool AdvanceBurn(double DeltaSeconds, int64 TickIndex, float EventBurnRateMultiplier,
        float& OutBurnedUnits);

    /** Updates fuel moisture at most once for a committed fixed-step tick. */
    bool AdvanceWeatherMoisture(double DeltaSeconds, int64 TickIndex,
        EWorldWeatherState Weather, float WeatherSeverity);

    /** Runtime tags are distinct from authored Light/Smoke signal tags. */
    void SynchronizeRaidSignalTags();

private:
    bool HasIgnitionReceipt(const FGuid& OperationId) const;
    void RecordIgnitionReceipt(const FGuid& OperationId);
};
