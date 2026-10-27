#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldOperationLedgerSubsystem.h"
#include "WorldTerrainStateSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldCargoTransportSubsystem.h"
#include "WorldStorageSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/EngineTypes.h"
#include "InventoryCraftingTypes.h"
#include "VoxelTerrainChunkComponent.h"
#include "WorldSimulationSaveGame.generated.h"

USTRUCT()
struct FWorldVoxelChunkSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid ActorIdentityId;

    UPROPERTY(SaveGame)
    int32 CellsPerAxis = 0;

    UPROPERTY(SaveGame)
    float VoxelSizeCm = 0.0f;

    UPROPERTY(SaveGame)
    int64 DensityRevision = 0;

    UPROPERTY(SaveGame)
    TArray<float> DensitySamples;

    UPROPERTY(SaveGame)
    TArray<FVoxelTerrainDigReceipt> DigReceipts;
};

USTRUCT()
struct FWorldPhysicalComponentSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FName ComponentName = NAME_None;

    UPROPERTY(SaveGame)
    FSoftClassPath ComponentClass;

    UPROPERTY(SaveGame)
    bool bIsPrimitive = false;

    UPROPERTY(SaveGame)
    bool bSimulatePhysics = false;

    UPROPERTY(SaveGame)
    TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::QueryAndPhysics;

    UPROPERTY(SaveGame)
    FVector LinearVelocity = FVector::ZeroVector;

    UPROPERTY(SaveGame)
    FVector AngularVelocityRadians = FVector::ZeroVector;

    UPROPERTY(SaveGame)
    FSoftObjectPath StaticMeshAsset;
};

USTRUCT()
struct FWorldPhysicalActorSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid IdentityId;

    UPROPERTY(SaveGame)
    FSoftClassPath ActorClass;

    UPROPERTY(SaveGame)
    FTransform ActorTransform = FTransform::Identity;

    UPROPERTY(SaveGame)
    bool bActorHiddenInGame = false;

    UPROPERTY(SaveGame)
    bool bActorCollisionEnabled = true;

    UPROPERTY(SaveGame)
    bool bActorTickEnabled = true;

    UPROPERTY(SaveGame)
    FName DefinitionId = NAME_None;

    UPROPERTY(SaveGame)
    EWorldObjectLifecycleState LifecycleState = EWorldObjectLifecycleState::World;

    UPROPERTY(SaveGame)
    int64 StateRevision = 0;

    UPROPERTY(SaveGame)
    TArray<FWorldPhysicalComponentSnapshot> Components;

    /** Actor-level UPROPERTY(SaveGame) payload for domain lifecycle fields. */
    UPROPERTY(SaveGame)
    TArray<uint8> ActorSaveGameData;
};

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
    static constexpr int32 CurrentSchemaVersion = 14;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") int32 SchemaVersion = CurrentSchemaVersion;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") FWorldSimulationClockSnapshot Clock;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") FWorldThreatDirectorSnapshot Threat;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldOwnedItemRecord> Items;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldItemTransferReceipt> ItemTransferReceipts;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldItemSplitReceipt> ItemSplitReceipts;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldOperationLedgerEntry> OperationLedgerEntries;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldInventorySnapshot> Inventories;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldTerrainCellState> TerrainCells;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldVoxelChunkSnapshot> VoxelChunks;
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
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldPhysicalActorSnapshot> PhysicalActors;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Save") TArray<FWorldStructuralNodeSnapshot> StructuralGraph;
};
