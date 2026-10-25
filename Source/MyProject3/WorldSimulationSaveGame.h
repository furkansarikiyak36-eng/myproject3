#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldTerrainStateSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldCargoTransportSubsystem.h"
#include "WorldStorageSubsystem.h"
#include "InventoryCraftingTypes.h"
#include "WorldSimulationSaveGame.generated.h"

USTRUCT()
struct FWorldInventorySnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid OwnerActorId;

    UPROPERTY(SaveGame)
    FGuid InventoryOwnerId;

    UPROPERTY(SaveGame)
    TArray<FInventoryStack> Items;

    UPROPERTY(SaveGame)
    FName QuickAccessItemId = NAME_None;
};

UCLASS()
class MYPROJECT3_API UWorldSimulationSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    static constexpr int32 CurrentSchemaVersion = 4;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") int32 SchemaVersion = CurrentSchemaVersion;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") FWorldSimulationClockSnapshot Clock;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") FWorldThreatDirectorSnapshot Threat;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldOwnedItemRecord> Items;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldInventorySnapshot> Inventories;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldTerrainCellState> TerrainCells;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldCropState> Crops;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldAnimalState> Animals;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldSurvivalProfile> SurvivalProfiles;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldDiscoveryRecord> Discoveries;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") EWorldWeatherState Weather = EWorldWeatherState::Clear;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") float WeatherSeverity = 0.0f;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldProductionStation> ProductionStations;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldProductionRecipe> ProductionRecipes;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldCargoTransportRecord> Cargo;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldStorageContainerRecord> StorageContainers;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldStorageIncomingReservation> StorageReservations;
};
