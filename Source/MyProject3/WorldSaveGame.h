#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "WorldPersistenceTypes.h"
#include "WorldSaveGame.generated.h"

UCLASS()
class MYPROJECT3_API UWorldSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Persistence")
    FWorldSaveManifest Manifest;
};
