#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BuildValleyWorldCommandlet.generated.h"

/**
 * Builds the game world map from the generated heightmap (editor only).
 *
 *   UnrealEditor-Cmd MyProject3.uproject -run=BuildValleyWorld [-Map=/Game/World/Valley]
 *
 * Reads SourceArt/World/valley.r16 and valley_import.json (made by
 * tools/heightmaps/build_world_heightmap.py), creates a blank (non World Partition) map,
 * imports the heights as one Landscape, adds sun, sky, fog, a player start at the valley
 * centre and the simulation bootstrap, and saves the map. Running it again replaces the map.
 */
UCLASS()
class MYPROJECT3_API UBuildValleyWorldCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UBuildValleyWorldCommandlet();
    virtual int32 Main(const FString& Params) override;
};
