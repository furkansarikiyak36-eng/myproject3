#pragma once

#include "CoreMinimal.h"
#include "TerrainWorldStateTypes.generated.h"

UENUM(BlueprintType)
enum class ETerrainEditLayer : uint8
{
    Protected UMETA(DisplayName = "Protected"),
    SurfaceSoil UMETA(DisplayName = "Surface Soil"),
    SoftSubsoil UMETA(DisplayName = "Soft Subsoil"),
    HardSubsoil UMETA(DisplayName = "Hard Subsoil"),
    Rock UMETA(DisplayName = "Rock")
};

UENUM(BlueprintType)
enum class ETerrainEditFailure : uint8
{
    None UMETA(DisplayName = "None"),
    InvalidRequest UMETA(DisplayName = "Invalid Request"),
    MissingConfiguration UMETA(DisplayName = "Missing Configuration"),
    ProtectedLayer UMETA(DisplayName = "Protected Layer"),
    DepthLimit UMETA(DisplayName = "Depth Limit"),
    VolumeLimit UMETA(DisplayName = "Volume Limit"),
    Unsupported UMETA(DisplayName = "Unsupported")
};

USTRUCT(BlueprintType)
struct FTerrainChunkKey
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") int32 Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") int32 Z = 0;
    bool operator==(const FTerrainChunkKey& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
    friend uint32 GetTypeHash(const FTerrainChunkKey& Key) { return HashCombine(HashCombine(::GetTypeHash(Key.X), ::GetTypeHash(Key.Y)), ::GetTypeHash(Key.Z)); }
};

USTRUCT(BlueprintType)
struct FTerrainEditPolicy
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Digging") TSet<ETerrainEditLayer> DiggableLayers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Digging", meta = (ClampMin = "0.0")) float MaxDepthCm = 300.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Digging", meta = (ClampMin = "0.0")) float MaxExcavationVolumeM3 = 20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Digging", meta = (ClampMin = "1.0")) float CellSizeCm = 100.0f;
};

USTRUCT(BlueprintType)
struct FTerrainCellState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") ETerrainEditLayer Layer = ETerrainEditLayer::Protected;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") bool bExcavated = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") float WaterLitres = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") float SoilMoisture = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") float NutrientLevel = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") float RotExposure = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") float FireExposure = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain") int64 Revision = 0;
};
