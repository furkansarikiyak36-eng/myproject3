#pragma once

#include "CoreMinimal.h"
#include "WorldDemoBootstrapActor.h"
#include "LogBuildingTypes.h"
#include "VerticalSliceCWorldActor.generated.h"

class ADirectionalLight;
class AHarvestableTreeActor;
class ALogBuildPiece;
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

    FLogProcessingMeshSet Family;
    bool bBuilt = false;
    bool bInputBound = false;
    FGuid PlayerSurvivalId;
    float Hours = 13.5f;
    float TimeToSurvivalUpdate = 0.0f;
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

    bool IsOpenGround(float X, float Y, float Margin) const;
    UMaterialInterface* Tint(const FLinearColor& Colour);
    UStaticMeshComponent* AddProp(UStaticMesh* Mesh, const FTransform& Transform, const FLinearColor& Colour, bool bCollision);
    AHarvestableTreeActor* SpawnTree(const FVector& Base, float Height, float TrunkScale, int32 Seed);
    /** Spawns a log from a named demo mesh with its centre at Centre lying along Axis. */
    ALogBuildPiece* SpawnLog(const TCHAR* MeshName, const FVector& Centre, const FVector& Axis,
        bool bPlaced, float LengthCm = 0.0f);
    void Connect(ALogBuildPiece* Parent, ALogBuildPiece* Child, ALogBuildPiece* SecondParent = nullptr);

    void SetupPlayer();
    void ChopInput();
    void UpdateSun();
};
