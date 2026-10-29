#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TerrainDigToolComponent.generated.h"

class UInputComponent;
class UVoxelTerrainChunkComponent;

/**
 * Player-facing bounded excavation tool.
 * N targets a voxel chunk under the crosshair; the terrain/world subsystems remain
 * authoritative for bounds, tool resistance, structure clearance, revisions and rollback.
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UTerrainDigToolComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTerrainDigToolComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain Tool", meta=(ClampMin="50.0"))
    float TraceDistanceCm = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain Tool", meta=(ClampMin="10.0", ClampMax="200.0"))
    float DigRadiusCm = 75.0f;

    /** Multiplier applied to the authored inventory tool strength. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain Tool", meta=(ClampMin="0.01", ClampMax="10.0"))
    float ToolStrength = 1.0f;

    /** World-space grid used to assign the logical excavation ledger cell. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain Tool", meta=(ClampMin="50.0"))
    float LogicalCellSizeCm = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain Tool", meta=(ClampMin="0"))
    int32 DepthLayer = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain Tool")
    FName MaterialId = NAME_None;

    void BindDefaultInput(UInputComponent* InputComponent);

private:
    void DigInput();
    bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
    void SetStatus(const FString& Message) const;
};
