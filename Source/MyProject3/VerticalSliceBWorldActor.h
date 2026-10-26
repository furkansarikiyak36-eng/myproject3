#pragma once

#include "CoreMinimal.h"
#include "WorldDemoBootstrapActor.h"
#include "LogBuildingTypes.h"
#include "VerticalSliceBWorldActor.generated.h"

class AHarvestableTreeActor;
class ALogBuildPiece;
class AStoneBuildingPiece;
class AStickPrimitivePiece;
class UHeatSourceComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;

/**
 * A larger, self-contained playable settlement demo. This is deliberately a
 * new actor for VerticalSlice_B; it never edits or replaces VerticalSlice_A.
 *
 * The map uses a deterministic heightfield prototype, clear work zones, real
 * log mesh families, harvestable trees, cross-material footing/log attachment,
 * carryable stone/stick pieces, a fueled heat source and bounded pit/fill edits.
 * It does not claim native Landscape, underground tunnels, farm simulation or
 * save-persistent terrain edits.
 */
UCLASS()
class MYPROJECT3_API AVerticalSliceBWorldActor : public AWorldDemoBootstrapActor
{
    GENERATED_BODY()

public:
    AVerticalSliceBWorldActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Build the world once. Exposed for controlled editor/automation use. */
    UFUNCTION(BlueprintCallable, Category="Demo B")
    bool BuildWorld();

    UFUNCTION(BlueprintPure, Category="Demo B|Terrain")
    float GetTerrainHeightAt(float Xcm, float Ycm) const;

    UFUNCTION(BlueprintPure, Category="Demo B")
    int32 GetSpawnedLogCount() const { return SpawnedLogs.Num(); }

    /** The 600 cm demo log family with every authored length/profile mesh and cut rule. */
    static FLogProcessingMeshSet BuildLogMeshFamily();
    static UStaticMesh* LoadLogMesh(const FString& Name);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo B|Terrain", meta=(ClampMin="8000.0", ClampMax="30000.0"))
    float WorldHalfExtentCm = 12000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo B|Terrain", meta=(ClampMin="32", ClampMax="160"))
    int32 TerrainQuadsPerAxis = 96;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo B|Terrain", meta=(ClampMin="100.0", ClampMax="600.0"))
    float DigRadiusCm = 300.0f;

private:
    UPROPERTY(VisibleAnywhere, Category="Demo B|Terrain")
    TObjectPtr<UProceduralMeshComponent> TerrainMesh;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Terrain")
    TObjectPtr<UProceduralMeshComponent> TrailMesh;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Runtime Actors")
    TArray<TObjectPtr<ALogBuildPiece>> SpawnedLogs;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Runtime Actors")
    TArray<TObjectPtr<AHarvestableTreeActor>> HarvestableTrees;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Runtime Actors")
    TObjectPtr<UHeatSourceComponent> Fire;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Terrain")
    TArray<float> TerrainHeights;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Terrain")
    int32 TerrainPointCount = 0;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Terrain")
    float TerrainSpacingCm = 250.0f;

    UPROPERTY(VisibleAnywhere, Category="Demo B")
    bool bWorldBuilt = false;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Construction")
    TObjectPtr<AStoneBuildingPiece> DemoFooting;

    UPROPERTY(VisibleAnywhere, Category="Demo B|Construction")
    TObjectPtr<ALogBuildPiece> DemoFootingLog;

    bool bFootingConnectionReady = false;
    bool bInputBound = false;
    FGuid PlayerSurvivalId;
    FString LastActionMessage = TEXT("Explore the settlement. V: chop tree.");

    bool BuildTerrain();
    void RebuildTerrainMesh();
    void BuildTrailMesh();
    float InitialHeightAt(float Xcm, float Ycm) const;
    float RawHeightAt(float Xcm, float Ycm) const;
    int32 TerrainIndex(int32 X, int32 Y) const;
    float EditTerrainAt(const FVector& WorldPoint, float RadiusCm, bool bAdd);

    UStaticMesh* LoadBasicMesh(const TCHAR* Name) const;
    UMaterialInterface* MakeTintedMaterial(const FLinearColor& Tint) const;
    UStaticMeshComponent* AddProp(const TCHAR* MeshName, const FVector& Location,
        const FVector& Scale, const FRotator& Rotation, const FLinearColor& Tint,
        bool bCollision = false);

    ALogBuildPiece* SpawnLog(const FLogProcessingMeshSet& Family,
        const FVector& Location, const FRotator& Rotation);
    void SpawnTree(const FLogProcessingMeshSet& Family, const FVector& GroundLocation);
    AStoneBuildingPiece* SpawnLooseStone(const FVector& Location, const FVector& Scale);
    AStickPrimitivePiece* SpawnLooseStick(const FVector& Location, const FRotator& Rotation);

    void BuildCamp(const FLogProcessingMeshSet& Family);
    void BuildTimberStand(const FLogProcessingMeshSet& Family);
    void BuildWoodYard(const FLogProcessingMeshSet& Family);
    void BuildStoneYard();
    void BuildConstructionYard(const FLogProcessingMeshSet& Family);
    void BuildFutureZones();
    void BuildConstructionFeatures(const FLogProcessingMeshSet& Family);
    void BuildFire();
    void TryConnectFooting();

    void AddZoneSign(const FString& Label, const FVector2D& Centre,
        const FLinearColor& Tint, bool bActive);
    void SetupPlayer();
    void BindDemoInput();
    bool AimTrace(FHitResult& OutHit, float Range) const;
    void ChopInput();
    void DigInput();
    void FillInput();
    void TakeInput();
    void DropInput();
    void TendFireInput();
    void UpdatePlayerSurvival(float DeltaSeconds);
    void DrawStatus() const;
};
