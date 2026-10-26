#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTerrainChunkComponent.generated.h"

/**
 * Smooth voxel terrain chunk prototype (7D-V01/V02 measurement, start of
 * 7D-V03..V05). Holds a signed density grid (negative = solid), extracts a
 * smooth surface with the engine's Marching Cubes and builds a collidable
 * runtime mesh. One component is one chunk; neighbouring chunks, streaming
 * and persistence are later tasks.
 */
UCLASS(ClassGroup=(Terrain), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UVoxelTerrainChunkComponent : public UProceduralMeshComponent
{
    GENERATED_BODY()

public:
    UVoxelTerrainChunkComponent(const FObjectInitializer& ObjectInitializer);

    /** Cells per axis; the chunk spans CellsPerAxis * VoxelSizeCm. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel", meta=(ClampMin="4", ClampMax="128"))
    int32 CellsPerAxis = 32;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel", meta=(ClampMin="5.0"))
    float VoxelSizeCm = 50.0f;

    /** Fills the grid with flat ground at a local height (cm above the chunk origin). */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel")
    void InitializeFlatGround(float GroundHeightCm);

    /**
     * Removes (bAdd = false) or adds material inside a sphere, in local space.
     * Returns the change in solid volume in cubic metres (negative when digging).
     */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel")
    float ApplySphereEdit(FVector LocalCenter, float RadiusCm, bool bAdd);

    /** Extracts the surface and rebuilds render and collision geometry. Returns the triangle count. */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel")
    int32 RebuildMesh();

    /** Approximate solid volume from the density grid, in cubic metres. */
    UFUNCTION(BlueprintPure, Category="Terrain|Voxel")
    float GetSolidVolumeCubicMetres() const;

    UFUNCTION(BlueprintPure, Category="Terrain|Voxel")
    float SampleDensity(const FVector& LocalPosition) const;

private:
    /** Density per grid point, (CellsPerAxis + 1)^3 samples, negative inside solid. */
    UPROPERTY()
    TArray<float> Density;

    int32 PointsPerAxis() const { return CellsPerAxis + 1; }
    int32 PointIndex(int32 X, int32 Y, int32 Z) const;
    float GetPoint(int32 X, int32 Y, int32 Z) const;
};
