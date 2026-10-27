#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTerrainChunkComponent.generated.h"

/**
 * Smooth voxel terrain chunk prototype (7D-V01/V02 measurement, start of
 * 7D-V03..V05). Holds a signed density grid (negative = solid), extracts a
 * smooth surface with the engine's Marching Cubes and builds a collidable
 * runtime mesh. Sphere edits are profile-gated and revision checked; edit
 * receipts are kept in memory for this component's lifetime.
 */
struct FVoxelEditOperationReceipt
{
    int64 ExpectedRevision = INDEX_NONE;
    FVector LocalCenter = FVector::ZeroVector;
    float RadiusCm = 0.0f;
    bool bAdd = false;
    float DeltaVolumeCubicMetres = 0.0f;
    int64 CommittedRevision = INDEX_NONE;
};
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

    /** Current authoritative density-grid revision. */
    UFUNCTION(BlueprintPure, Category="Terrain|Voxel")
    int64 GetVoxelRevision() const { return VoxelRevision; }

    /**
     * Applies an edit through the global voxel.multi_cell_edit profile.
     * Reusing an operation ID with identical arguments returns its prior receipt;
     * stale expected revisions and conflicting ID reuse are rejected.
     */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel")
    bool TryApplySphereEdit(FGuid OperationId, int64 ExpectedRevision,
        FVector LocalCenter, float RadiusCm, bool bAdd,
        float& OutDeltaVolumeCubicMetres, int64& OutCommittedRevision,
        FString& OutFailureReason);

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Terrain|Voxel", meta=(AllowPrivateAccess="true"))
    int64 VoxelRevision = 0;

    TMap<FGuid, FVoxelEditOperationReceipt> EditReceipts;
    TArray<FGuid> EditReceiptOrder;

    float ApplySphereEditInternal(FVector LocalCenter, float RadiusCm, bool bAdd);
    void RememberEditReceipt(FGuid OperationId, const FVoxelEditOperationReceipt& Receipt);

    int32 PointsPerAxis() const { return CellsPerAxis + 1; }
    int32 PointIndex(int32 X, int32 Y, int32 Z) const;
    float GetPoint(int32 X, int32 Y, int32 Z) const;
};
