#pragma once

#include "CoreMinimal.h"
#include "WorldDemoBootstrapActor.h"
#include "LogBuildingTypes.h"
#include "VerticalSliceDemoActor.generated.h"

class ALogBuildPiece;
class UHeatSourceComponent;
class UStaticMesh;
class UVoxelTerrainChunkComponent;

/**
 * Builds the first playable vertical slice (DEMO-01) at BeginPlay: diggable
 * smooth voxel ground, logs with the real FBX mesh families, stones, sticks, a
 * stone footing carrying a log, a fueled fire, windy weather and a survival
 * profile for the player, plus an on-screen status read-out.
 *
 * Extra keys: X dig, Z fill, T take stone/stick, Y drop it, H tend the fire.
 * The character keeps E pick up log, G drop, F process, C select cut action.
 */
UCLASS()
class MYPROJECT3_API AVerticalSliceDemoActor : public AWorldDemoBootstrapActor
{
    GENERATED_BODY()

public:
    AVerticalSliceDemoActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Digs (bAdd = false) or fills at a world point in every overlapping chunk. */
    UFUNCTION(BlueprintCallable, Category="Demo|Terrain")
    float EditTerrainAt(FVector WorldPoint, float RadiusCm, bool bAdd);

    UFUNCTION(BlueprintPure, Category="Demo")
    int32 GetSpawnedLogCount() const { return SpawnedLogs.Num(); }

    UFUNCTION(BlueprintPure, Category="Demo")
    bool IsFootingJointBuilt() const { return bFootingJointBuilt; }

    UFUNCTION(BlueprintPure, Category="Demo")
    UHeatSourceComponent* GetFire() const { return Fire; }

    /** Builds the slice content; BeginPlay calls this, tests may call it directly. */
    bool BuildSlice();

protected:
    UPROPERTY(EditAnywhere, Category="Demo|Terrain", meta=(ClampMin="1", ClampMax="4"))
    int32 ChunksPerSide = 2;

    UPROPERTY(EditAnywhere, Category="Demo|Terrain")
    float DigRadiusCm = 90.0f;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UVoxelTerrainChunkComponent>> Chunks;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ALogBuildPiece>> SpawnedLogs;

    UPROPERTY(Transient)
    TObjectPtr<UHeatSourceComponent> Fire;

    FGuid PlayerSurvivalId;
    bool bSliceBuilt = false;
    bool bFootingJointBuilt = false;
    FString LastActionMessage;

    void BuildTerrain();
    FLogProcessingMeshSet BuildLogMeshFamily() const;
    ALogBuildPiece* SpawnLog(const FLogProcessingMeshSet& Family, const FVector& Location, const FRotator& Rotation);
    void SpawnStonesAndSticks();
    void BuildFootingWithLog(const FLogProcessingMeshSet& Family);
    void SpawnFire();
    void SetupPlayer();
    void BindDemoInput();

    bool AimTrace(FHitResult& OutHit, float Range) const;
    void DigInput();
    void FillInput();
    void TakeInput();
    void DropInput();
    void TendFireInput();
    void UpdatePlayerSurvival(float DeltaSeconds);
    void DrawStatus() const;
};
