#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldWaterActor.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * Simple water surfaces of the world map: the lake as a flat disc at its water level and the river as a
 * ribbon that follows its bed downhill. Data comes from the heightmap builder (valley_import.json "water");
 * the terrain hides the parts of the surface that lie under higher ground. A stand-in until the Water plugin
 * test (step 7); no swimming or flow.
 */
UCLASS()
class MYPROJECT3_API AWorldWaterActor : public AActor
{
    GENERATED_BODY()

public:
    AWorldWaterActor();

    /** River centre line: world cm XY, water surface Z in Z, half width cm in W. */
    UPROPERTY(EditAnywhere, Category="World|Water")
    TArray<FVector4f> RiverPoints;

    /** Lake: centre XY, water level Z, radius W (world cm). Radius 0 = no lake. */
    UPROPERTY(EditAnywhere, Category="World|Water")
    FVector4f Lake = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, Category="World|Water")
    TObjectPtr<UMaterialInterface> WaterMaterial;

    /** Builds the surfaces from RiverPoints and Lake. */
    void BuildSurfaces();

    virtual void OnConstruction(const FTransform& Transform) override;

private:
    UPROPERTY(VisibleAnywhere, Category="World|Water")
    TObjectPtr<UProceduralMeshComponent> Surface;
};
