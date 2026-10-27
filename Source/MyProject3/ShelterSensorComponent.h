#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShelterSensorComponent.generated.h"

/**
 * Measures how covered the owner is by built pieces: a roof overhead keeps
 * rain and snow off, walls around block wind. Feeds survival shelter value.
 */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class MYPROJECT3_API UShelterSensorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShelterSensorComponent();

    /** Survival profile to update; leave empty to only measure. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelter")
    FGuid SurvivalEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelter", meta = (ClampMin = "0.1"))
    float UpdateInterval = 1.0f;

    UFUNCTION(BlueprintPure, Category = "Shelter")
    float GetRoofCover() const { return RoofCover; }

    UFUNCTION(BlueprintPure, Category = "Shelter")
    float GetWallCover() const { return WallCover; }

    /** 0..1 protection from weather: mostly roof, partly walls. */
    UFUNCTION(BlueprintPure, Category = "Shelter")
    float GetShelterProtection() const { return Protection; }

    /** Measures cover at a point; usable without a component. */
    static void MeasureAt(const UWorld* World, const FVector& Location, const AActor* Ignore,
        float& OutRoof, float& OutWalls);

    void MeasureNow();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    float RoofCover = 0.0f;
    float WallCover = 0.0f;
    float Protection = 0.0f;
    float TimeToUpdate = 0.0f;
};
