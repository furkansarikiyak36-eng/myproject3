#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldGroundActor.generated.h"

class ALandscapeProxy;
class UBoxComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class UTexture2D;
class UVoxelTerrainChunkComponent;

/** Landscape collision uses its own object channel so dug ground can be walked through (DefaultEngine.ini "Ground"). */
#define ECC_WorldGround ECC_GameTraceChannel1

/** One generated cave: spheres along its tunnel (world cm in XYZ, radius cm in W). */
USTRUCT()
struct FWorldCaveSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="World|Ground")
    FName Name;

    UPROPERTY(EditAnywhere, Category="World|Ground")
    TArray<FVector4f> Spheres;
};

/**
 * The diggable ground of the world map. The landscape carries the whole terrain; where the player
 * digs, that 16 x 16 m column turns into voxel chunks: the landscape is hidden there (material mask)
 * and pawns and physics bodies inside the column ignore its collision, so the voxel surface takes
 * over. Soil can be dug down to the rock floor (15-35 m, from the soil depth plan map); rock ground
 * needs a rock-strength tool. Caves are carved into voxel chunks at play start, with the landscape
 * opened at their entrances.
 */
UCLASS()
class MYPROJECT3_API AWorldGroundActor : public AActor
{
    GENERATED_BODY()

public:
    AWorldGroundActor();

    UPROPERTY(EditAnywhere, Category="World|Ground")
    TObjectPtr<ALandscapeProxy> Landscape;

    /** Soil depth in metres on a grid (0 = rock, not diggable). Index = Row * SoilGridSize + Column. */
    UPROPERTY(EditAnywhere, Category="World|Ground")
    TArray<uint8> SoilDepthM;

    UPROPERTY(EditAnywhere, Category="World|Ground")
    int32 SoilGridSize = 0;

    UPROPERTY(EditAnywhere, Category="World|Ground")
    float SoilCellCm = 400.0f;

    /** World XY of soil grid cell (0, 0). */
    UPROPERTY(EditAnywhere, Category="World|Ground")
    FVector2D SoilOriginCm = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, Category="World|Ground")
    TArray<FWorldCaveSpec> Caves;

    /** Vertex-coloured material for the voxel ground. */
    UPROPERTY(EditAnywhere, Category="World|Ground")
    TObjectPtr<UMaterialInterface> VoxelMaterial;

    UPROPERTY(EditAnywhere, Category="World|Ground", meta=(ClampMin="8", ClampMax="64"))
    int32 ChunkCells = 32;

    UPROPERTY(EditAnywhere, Category="World|Ground", meta=(ClampMin="10.0"))
    float VoxelSizeCm = 50.0f;

    /** Landscape material mask: one texel per column, covering MaskSizeCm from MaskOriginCm. */
    UPROPERTY(EditAnywhere, Category="World|Ground")
    int32 MaskResolution = 256;

    UPROPERTY(EditAnywhere, Category="World|Ground")
    float MaskOriginCm = -204800.0f;

    /** Gives the player's pawn a dig tool (N) that needs no inventory item: for world play tests. */
    UPROPERTY(EditAnywhere, Category="World|Ground|Test")
    bool bGivePlayerTestDigTool = true;

    UPROPERTY(EditAnywhere, Category="World|Ground|Test")
    float TestDigReachCm = 1500.0f;

    UPROPERTY(EditAnywhere, Category="World|Ground|Test")
    float TestDigRadiusCm = 90.0f;

    static constexpr const TCHAR* MaskParameterName = TEXT("GroundDigMask");

    /**
     * Digs a sphere of world ground. Turns the touched soil columns into voxels first.
     * Returns false with a reason when nothing could be dug (rock, floor reached, too weak a tool).
     */
    bool DigAt(const FVector& WorldCenter, float RadiusCm, float ToolStrength, FString& OutStatus);

    /** True where the landscape is hidden and the voxel ground has taken over. */
    bool IsDugColumn(const FVector& WorldLocation) const;

    /** Soil depth at a world location in cm; 0 = rock. */
    float SoilDepthCmAt(double X, double Y) const;

    int32 GetChunkCount() const { return Chunks.Num(); }
    int32 GetDugColumnCount() const { return DugColumns.Num(); }

    /** Landscape surface height (world cm) or false if there is no landscape there. */
    bool SurfaceZ(double X, double Y, float& OutZ) const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    struct FColumnData
    {
        TArray<float> SurfaceZ; // world cm, (ChunkCells+1)^2
        TArray<float> FloorZ;   // world cm, empty = rock column
        float MinSurface = 0.0f, MaxSurface = 0.0f, MinFloor = 0.0f;
    };

    TMap<FIntPoint, FColumnData> ColumnCache;

    UPROPERTY(Transient)
    TMap<FIntVector, TObjectPtr<UVoxelTerrainChunkComponent>> Chunks;

    UPROPERTY(Transient)
    TMap<FIntPoint, TObjectPtr<UBoxComponent>> DugColumns;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> MaskTexture;

    TArray<uint8> MaskData;

    /** Primitives currently set to ignore the landscape, with their previous response. */
    TMap<TWeakObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionResponse>> Bypassing;

    float ChunkSizeCm() const { return ChunkCells * VoxelSizeCm; }
    FIntPoint ColumnOf(double X, double Y) const;
    FIntVector ChunkOf(const FVector& World) const;
    const FColumnData& Column(const FIntPoint& Key);
    UVoxelTerrainChunkComponent* EnsureChunk(const FIntVector& Key, bool bOpenColumn);
    void DigColumn(const FIntPoint& Key);
    void CarveCaves();
    void CreateMask();
    void SetMask(const FIntPoint& Key);
    void UpdateBypass();
    void GiveTestDigTool();
};
