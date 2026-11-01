#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTerrainChunkComponent.generated.h"

UENUM(BlueprintType)
enum class EVoxelSoilClass : uint8
{
    SoftSoil,
    HardSoil,
    Rock
};

/** Durable receipt for an accepted voxel dig. Replaying an identical operation ID never edits twice. */
USTRUCT()
struct FVoxelTerrainDigReceipt
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid OperationId;

    UPROPERTY(SaveGame)
    FVector LocalCenter = FVector::ZeroVector;

    UPROPERTY(SaveGame)
    float RadiusCm = 0.0f;

    UPROPERTY(SaveGame)
    float ToolStrength = 0.0f;

    UPROPERTY(SaveGame)
    int64 ExpectedRevision = 0;

    UPROPERTY(SaveGame)
    int64 ResultingRevision = 0;

    UPROPERTY(SaveGame)
    float RemovedVolumeCubicMetres = 0.0f;
};

/**
 * Smooth voxel terrain chunk. One component is one bounded excavation pocket:
 * signed density is negative inside solid, and Marching Cubes generates the
 * visible/collidable surface. Whole-world voxel terrain, cross-chunk seams,
 * streaming and asynchronous mesh generation are deliberately out of scope.
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

    /** Material resistance is per pocket; tool strength thresholds are project data, not visual materials. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging")
    EVoxelSoilClass SoilClass = EVoxelSoilClass::SoftSoil;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging", meta=(ClampMin="0.01"))
    float SoftSoilToolStrength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging", meta=(ClampMin="0.01"))
    float HardSoilToolStrength = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging", meta=(ClampMin="0.01"))
    float RockToolStrength = 5.0f;

    /** Maximum radius of one gameplay edit; each edit must also fit inside EditableBoundsLocal. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging", meta=(ClampMin="1.0"))
    float MaxDigRadiusCm = 200.0f;

    /** Local-space region allowed to change. Default is the 16 m cube for a 32 x 50 cm chunk. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging")
    FBox EditableBoundsLocal = FBox(FVector::ZeroVector, FVector(1600.0f, 1600.0f, 1600.0f));

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging", meta=(ClampMin="1", ClampMax="100000"))
    int32 MaxRememberedDigOperations = 4096;

    /** Conservative 3D clearance around grounded structural graph nodes. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging", meta=(ClampMin="0.0", ClampMax="1000.0"))
    float SupportSafetyMarginCm = 200.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Terrain|Voxel")
    int64 DensityRevision = 0;

    /** World ground chunks: a dig sphere may reach past this chunk into its neighbours; only the
     *  part inside this chunk is applied (the ground actor applies the rest to the neighbours). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Digging")
    bool bAllowPartialEdits = false;

    /** Vertex colours by depth: grass on the original surface, soil below, rock at the floor
     *  (or everywhere below the surface for Rock chunks). Needs InitializeFromColumns. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Look")
    bool bGroundColours = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Look")
    FLinearColor GrassColour = FLinearColor(0.10f, 0.22f, 0.05f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Look")
    FLinearColor SoilColour = FLinearColor(0.22f, 0.15f, 0.08f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Terrain|Voxel|Look")
    FLinearColor RockColour = FLinearColor(0.20f, 0.19f, 0.18f);

    /**
     * Fills the grid from per-column heights (local cm, (CellsPerAxis+1)^2 values, X fastest).
     * Surface: solid below it, air above (bSolidAboveSurface makes the whole chunk solid, for
     * chunks under a landscape that is still shown). Floor: digging never removes ground at or
     * below it (empty array = no floor).
     */
    void InitializeFromColumns(const TArray<float>& InSurfaceLocalZ, const TArray<float>& InFloorLocalZ,
        bool bSolidAboveSurface);

    /** Opens the ground above the surface again (a chunk whose column became dug), keeping earlier carving. */
    void OpenAboveSurface();

    /** Carves a sphere for world generation (caves); ignores the floor and tool rules. */
    void CarveIgnoringFloor(const FVector& LocalCenter, float RadiusCm);

    /** Fills the grid with flat ground at a local height (cm above the chunk origin). */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel")
    void InitializeFlatGround(float GroundHeightCm);

    /**
     * Low-level geometry operation retained for development tools/tests.
     * Gameplay should use TryApplyBoundedDig so bounds, material resistance,
     * revisions and operation idempotency are enforced.
     */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel")
    float ApplySphereEdit(FVector LocalCenter, float RadiusCm, bool bAdd);

    /** Validated, bounded and idempotent gameplay dig. Removed volume is positive. */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel|Digging")
    bool TryApplyBoundedDig(const FGuid& OperationId, FVector LocalCenter, float RadiusCm,
        float ToolStrength, int64 ExpectedRevision, float& OutRemovedVolumeCubicMetres,
        int64& OutResultingRevision);

    /** Extracts the surface and rebuilds render and collision geometry. Returns triangle count. */
    UFUNCTION(BlueprintCallable, Category="Terrain|Voxel")
    int32 RebuildMesh();

    /** Approximate solid volume from the density grid, in cubic metres. */
    UFUNCTION(BlueprintPure, Category="Terrain|Voxel")
    float GetSolidVolumeCubicMetres() const;

    UFUNCTION(BlueprintPure, Category="Terrain|Voxel")
    float SampleDensity(const FVector& LocalPosition) const;

    UFUNCTION(BlueprintPure, Category="Terrain|Voxel")
    bool HasInitializedDensity() const;

    UFUNCTION(BlueprintPure, Category="Terrain|Voxel")
    int64 GetDensityRevision() const { return DensityRevision; }

    /** Explicit payload API used by the world save coordinator; validates before replacing live data. */
    bool TryGetDigReceipt(const FGuid& OperationId, FVoxelTerrainDigReceipt& OutReceipt) const;
    bool ExportDensitySnapshot(TArray<float>& OutDensity, int64& OutRevision,
        TArray<FVoxelTerrainDigReceipt>& OutReceipts) const;
    bool ValidateDensitySnapshot(int32 InCellsPerAxis, float InVoxelSizeCm, int64 InRevision,
        const TArray<float>& InDensity, const TArray<FVoxelTerrainDigReceipt>& InReceipts) const;
    bool RestoreDensitySnapshot(int32 InCellsPerAxis, float InVoxelSizeCm, int64 InRevision,
        const TArray<float>& InDensity, const TArray<FVoxelTerrainDigReceipt>& InReceipts);

private:
    /** Density per grid point, (CellsPerAxis + 1)^3 samples, negative inside solid. */
    UPROPERTY(SaveGame)
    TArray<float> Density;

    TMap<FGuid, FVoxelTerrainDigReceipt> DigReceipts;

    /** Per-column heights in local cm ((CellsPerAxis+1)^2, X fastest); rebuilt by the owner, not saved. */
    TArray<float> SurfaceLocalZ;
    TArray<float> FloorLocalZ;
    bool bIgnoreFloor = false;

    float ColumnValue(const TArray<float>& Values, float LocalX, float LocalY) const;

    int32 PointsPerAxis() const { return CellsPerAxis + 1; }
    int32 PointIndex(int32 X, int32 Y, int32 Z) const;
    float GetPoint(int32 X, int32 Y, int32 Z) const;
    float GetRequiredToolStrength() const;
};
