#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldStorageSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldLogisticsRouteSubsystem.h"
#include "WorldDemoScenarioActor.generated.h"

/**
 * Level-placed registration adapter for the integrated demo.
 *
 * This actor registers real records in the existing world subsystems. It does
 * not spawn pretend meshes or claim a visual/interaction adapter exists.
 * Keep options disabled until the matching level prop/interaction is wired.
 */
UCLASS(Blueprintable)
class MYPROJECT3_API AWorldDemoScenarioActor : public AActor
{
    GENERATED_BODY()

public:
    AWorldDemoScenarioActor();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="Demo|Scenario")
    bool InitializeScenario();

    UFUNCTION(BlueprintPure, Category="Demo|Scenario")
    bool IsScenarioReady() const { return bScenarioReady; }

    UFUNCTION(BlueprintPure, Category="Demo|Scenario")
    FString GetScenarioStatus() const { return ScenarioStatus; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Storage")
    bool bRegisterStorehouse = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Storage")
    FGuid StorehouseId = FGuid(0xD3A00001, 0x00000001, 0x00000001, 0x00000001);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Storage", meta=(ClampMin="1", ClampMax="4096"))
    int32 StorehouseCapacity = 128;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Routes")
    bool bRegisterSettlementRoutes = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Routes")
    FVector CampRouteLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Routes")
    FVector TimberRouteLocation = FVector(3500.0, 1500.0, 0.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Routes")
    FVector WorkshopRouteLocation = FVector(2500.0, -2000.0, 0.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Routes", meta=(ClampMin="0.0"))
    float CampToTimberRouteCost = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Routes", meta=(ClampMin="0.0"))
    float TimberToWorkshopRouteCost = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Farm")
    bool bRegisterLogicalTestCrop = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Farm")
    FGuid TestCropId = FGuid(0xD3A00002, 0x00000002, 0x00000002, 0x00000002);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Farm")
    FName TestCropDefinitionId = TEXT("DemoCrop_Unbound");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Farm", meta=(ClampMin="1.0"))
    double TestCropSecondsPerStage = 60.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Farm", meta=(ClampMin="0.0", ClampMax="1.0"))
    float TestCropMoisture = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Survival")
    bool bSetDemoWeather = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Survival")
    EWorldWeatherState DemoWeather = EWorldWeatherState::Rain;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Survival", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DemoWeatherSeverity = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Production")
    bool bRegisterConfiguredProduction = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Production")
    FGuid ProductionStationId = FGuid(0xD3A00003, 0x00000003, 0x00000003, 0x00000003);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Production")
    FName ProductionStationTag = TEXT("DemoWorkshop");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Production", meta=(ClampMin="1"))
    int32 ProductionCapacity = 64;

    /** Selects an existing non-physical production transformation from Config/GameData/transformations.json. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Scenario|Production")
    FName ProductionRecipeId = NAME_None;

private:
    UPROPERTY(Transient)
    bool bScenarioReady = false;

    UPROPERTY(Transient)
    FString ScenarioStatus;

    bool EnsureStorage(class UWorldStorageSubsystem* Storage) const;
    bool EnsureRoutes(class UWorldLogisticsRouteSubsystem* Routes) const;
    bool EnsureCrop(class UWorldAgricultureSubsystem* Agriculture) const;
    bool EnsureProduction(class UWorldProductionSubsystem* Production) const;
};
