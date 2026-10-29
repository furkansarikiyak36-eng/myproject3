#pragma once

#include "CoreMinimal.h"
#include "WorldDemoBootstrapActor.h"
#include "LogBuildingTypes.h"
#include "VerticalSliceCWorldActor.generated.h"

class ADirectionalLight;
class AHarvestableTreeActor;
class AHelperCompanionActor;
class ALogBuildPiece;
class AProductionStationActor;
class AWorldDiscoveryMarkerActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * VerticalSlice_C: one small, continuous creek-side forest clearing. Nothing
 * is laid out as test stations; the place itself asks for the mechanics:
 * an abandoned camp, a half-built cabin without door or roof, a creek too
 * deep to wade except at a far ford, the remains of a rope bridge, a cliff
 * terrace on the hill with old stair stringers, a lookout with a zipline
 * down to camp, a broken stake wall to the east, animals, bushes and a
 * stranger in the woods. Day turns to a cold night.
 */
UCLASS()
class MYPROJECT3_API AVerticalSliceCWorldActor : public AWorldDemoBootstrapActor
{
    GENERATED_BODY()

public:
    AVerticalSliceCWorldActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Demo C")
    bool BuildWorld();

    UFUNCTION(BlueprintPure, Category = "Demo C")
    float GetGroundHeight(float X, float Y) const;

    /** Centre line of the creek (world Y) at X. */
    float GetCreekCentreY(float X) const;

    /** Direction and speed (cm/s) the creek water flows at X; it runs east to the ford. */
    FVector GetCreekFlow(float X) const;

    /**
     * Water in the creek: loose logs float and drift downstream, a sled rides the
     * surface, and now and then a log comes floating down from upstream.
     */
    void UpdateCreek(float DeltaSeconds);

    /** True when a point lies over the creek bed below the water line. */
    bool IsInCreek(const FVector& Point) const;

    AHelperCompanionActor* GetHelper() const { return Helper; }
    int32 GetDriftSpawnCount() const { return DriftSpawned; }

    int32 GetTreeCount() const { return Trees.Num(); }
    int32 GetPlacedLogCount() const { return PlacedLogs.Num(); }

    static constexpr float HalfExtent = 3600.0f;
    static constexpr float WaterLevel = -165.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo C")
    bool bDayNightCycle = true;

    /** Hour of day when play starts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo C", meta = (ClampMin = "0.0", ClampMax = "24.0"))
    float StartHour = 13.5f;

private:
    UPROPERTY(VisibleAnywhere, Category = "Demo C")
    TObjectPtr<UProceduralMeshComponent> Ground;

    UPROPERTY(VisibleAnywhere, Category = "Demo C")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassClumps;

    UPROPERTY(VisibleAnywhere, Category = "Demo C")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Pebbles;

    UPROPERTY(VisibleAnywhere, Category = "Demo C")
    TObjectPtr<UStaticMeshComponent> Water;

    /** Hills and a far forest around the clearing so the world has no edge. */
    UPROPERTY(VisibleAnywhere, Category = "Demo C")
    TObjectPtr<UProceduralMeshComponent> Backdrop;

    UPROPERTY(VisibleAnywhere, Category = "Demo C")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FarForest;

    UPROPERTY(VisibleAnywhere, Category = "Demo C")
    TObjectPtr<class UPostProcessComponent> Grade;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AHarvestableTreeActor>> Trees;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ALogBuildPiece>> PlacedLogs;

    UPROPERTY(Transient)
    TObjectPtr<AHelperCompanionActor> Helper;

    float DriftSpawnTimer = 6.0f;
    int32 DriftSpawned = 0;
    FString LastHelperStatus;

    FLogProcessingMeshSet Family;
    bool bBuilt = false;
    bool bInputBound = false;
    bool bStarterToolsGranted = false;
    bool bDiscoveryMarkersSpawned = false;
    bool bGatherableZonesAssigned = false;
    bool bFarmPlotBuilt = false;
    float FarmPlotVisualRefreshTimer = 0.0f;
    int32 SelectedFieldActionIndex = INDEX_NONE;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> FarmPlotTiles;
    TArray<FLinearColor> FarmPlotColours;
    int32 ChopSwingCount = 0;
    int32 SuccessfulChopCount = 0;
    FGuid PlayerSurvivalId;
    float Hours = 13.5f;
    float TimeToSurvivalUpdate = 0.0f;
    /** Last user-notified wetness band: 0 dry, 1 damp, 2 wet, 3 soaked; -1 uninitialized. */
    int32 LastReportedWetnessBand = INDEX_NONE;
    bool bIntroShown = false;
    TWeakObjectPtr<ADirectionalLight> Sun;

    void BuildGround();
    void BuildWater();
    void BuildBackdrop();
    void BuildForest();
    void BuildRocksAndGrass();
    void BuildCamp();
    void BuildCabin();
    void BuildOldBridge();
    void BuildHill();
    void BuildPalisade();
    void BuildLife();
    void BuildFarmPlot();
    void RefreshFarmPlotVisuals();
    void BuildCreekDam();
    void BuildHelper();
    void HelperOrderInput();

    bool IsOpenGround(float X, float Y, float Margin) const;
    UMaterialInterface* Tint(const FLinearColor& Colour);
    UStaticMeshComponent* AddProp(UStaticMesh* Mesh, const FTransform& Transform, const FLinearColor& Colour, bool bCollision);
    AHarvestableTreeActor* SpawnTree(const FVector& Base, float Height, float TrunkScale, int32 Seed);
    /** Spawns a log from a named demo mesh with its centre at Centre lying along Axis. */
    ALogBuildPiece* SpawnLog(const TCHAR* MeshName, const FVector& Centre, const FVector& Axis,
        bool bPlaced, float LengthCm = 0.0f);
    /** Builds a log in through the placement kernel at its transform, resting on Primary (and Secondary) or the ground. */
    bool CommitBuilt(ALogBuildPiece* Log, AActor* Primary = nullptr, AActor* Secondary = nullptr);
    /** Builds Log onto Lower as the player's stacking would (a station crossing), aimed at AimPoint. */
    bool CommitStacked(ALogBuildPiece* Log, ALogBuildPiece* Lower, const FVector& AimPoint, float CarryYaw);

    void SetupPlayer();
    void EnsureStarterTools();
    void EnsureDiscoveryMarkers();
    void AssignGatherableZones();
    void ChopInput();
    void GatherInput();
    void CycleFieldActionInput();
    void ApplyFieldActionInput();
    void PlantOrHarvestCropInput();
    bool FindNearestFarmPlotCell(const FVector& WorldLocation, FIntVector& OutCell,
        float MaxDistanceCm = 135.0f) const;
    void FillCreekWaterInput();
    void DrinkWaterInput();
    void BoilWaterInput();
    void RestInput();
    void ShowSurvivalStatusInput();
    void RefuelFireInput();
    void StationStartCollectInput();
    void StationDepositInput();
    void StationTakeInput();
    AProductionStationActor* FindNearestStation(const FVector& Location, float MaxDistanceCm) const;
    void UpdateSun();
};
