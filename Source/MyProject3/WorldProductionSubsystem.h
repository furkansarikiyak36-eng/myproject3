#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldProductionSubsystem.generated.h"

class ALogBuildPiece;

USTRUCT(BlueprintType)
struct FProductionIngredient
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production", meta=(ClampMin="1")) int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FWorldProductionRecipe
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") FName RecipeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") TArray<FProductionIngredient> Inputs;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") TArray<FProductionIngredient> Outputs;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production", meta=(ClampMin="0.01")) double DurationSeconds = 10.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") FName RequiredStationTag = NAME_None;
};

UENUM(BlueprintType)
enum class EProductionJobState : uint8 { Queued, Running, Completed, Blocked };

USTRUCT(BlueprintType)
struct FWorldProductionJob
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") FGuid JobId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") FName RecipeId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") EProductionJobState State = EProductionJobState::Queued;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") double ProgressSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") FName BlockedReason = NAME_None;
};

/** Durable idempotency receipt for converting one physical world item into station stock. */
USTRUCT(BlueprintType)
struct FWorldProductionPhysicalIntakeReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid SourceItemId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ExpectedItemRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ExpectedStationRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName SourceDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName SourceMeshVariantSetId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName StockItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ResultingItemRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ResultingStationRevision = 0;
};

/** Durable receipt for ingesting one non-actor world resource into recipe-addressable stock. */
USTRUCT(BlueprintType)
struct FWorldProductionItemIntakeReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid SourceItemId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ExpectedItemRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ExpectedStationRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName SourceDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName StockItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ResultingItemRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ResultingStationRevision = 0;
};

/** Durable receipt for idempotent recipe enqueue requests. */
USTRUCT(BlueprintType)
struct FWorldProductionEnqueueReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName RecipeId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid JobId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ExpectedStationRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ResultingStationRevision = 0;
};

/** Durable receipt for idempotent completed-job collection requests. */
USTRUCT(BlueprintType)
struct FWorldProductionCollectReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid JobId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ExpectedStationRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ResultingStationRevision = 0;
};

/** Durable receipt for materializing one aggregate stock unit as a physical world log. */
USTRUCT(BlueprintType)
struct FWorldProductionMaterializationReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName StockItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName DefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName BaseDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FName MeshVariantSetId = NAME_None;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") FGuid WorldItemId;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ExpectedStationRevision = 0;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") int64 ResultingStationRevision = 0;
};

USTRUCT(BlueprintType)
struct FWorldProductionStation
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") FGuid StationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") FName StationTag = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production", meta=(ClampMin="1")) int32 StorageCapacityUnits = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") TMap<FName, int32> Stock;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") TArray<FWorldProductionJob> Jobs;
    /** Receipts prevent one physical source actor from becoming station stock more than once. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") TArray<FWorldProductionPhysicalIntakeReceipt> PhysicalIntakeReceipts;
    /** Receipts for unmaterialized world resources entering recipe-addressable stock. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") TArray<FWorldProductionItemIntakeReceipt> ItemIntakeReceipts;
    /** Durable request results; retained after a job is collected to make retries idempotent. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") TArray<FWorldProductionEnqueueReceipt> EnqueueReceipts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") TArray<FWorldProductionCollectReceipt> CollectReceipts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") TArray<FWorldProductionMaterializationReceipt> MaterializationReceipts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") int64 Revision = 1;
};

/** Recipe-driven, bounded production queues with explicit blocked reasons. */
UCLASS()
class MYPROJECT3_API UWorldProductionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Production|Limits", meta=(ClampMin="1", ClampMax="2048")) int32 MaxStations = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Production|Limits", meta=(ClampMin="1", ClampMax="128")) int32 MaxJobsPerStation = 32;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Production|Limits", meta=(ClampMin="1", ClampMax="256")) int32 MaxRecipes = 128;

    UFUNCTION(BlueprintCallable, Category="World|Production") bool RegisterRecipe(const FWorldProductionRecipe& Recipe);
    UFUNCTION(BlueprintCallable, Category="World|Production") bool RegisterStation(const FWorldProductionStation& Station);
    UFUNCTION(BlueprintCallable, Category="World|Production") bool AddStock(const FGuid& StationId, FName ItemId, int32 Quantity, int64 ExpectedRevision);
    /** Takes stock out of a station (into a player's hands). Job inputs already left the stock when the job was queued. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool TryTakeStock(const FGuid& StationId, FName ItemId, int32 Quantity, int64 ExpectedRevision);
    /** Converts one loose, world-owned physical log into one station stock unit. OperationId must be reused for retries. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool TryIngestWorldOwnedLogPiece(FGuid StationId, FGuid OperationId, FGuid ItemId, int64 ExpectedItemRevision, int64 ExpectedStationRevision, FName& OutStockItemId);
    /** Ingests a uniquely owned ledger-only world resource; rejects items with a physical actor adapter. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool TryIngestWorldOwnedItem(
        FGuid StationId, FGuid OperationId, FGuid ItemId, int64 ExpectedItemRevision,
        int64 ExpectedStationRevision, FName& OutStockItemId);
    /** Stable recipe key for physical resource stock; preserves the nominal mesh family when one is authored. */
    UFUNCTION(BlueprintPure, Category="World|Production") static FName MakePhysicalStockItemId(FName DefinitionId, FName MeshVariantSetId);
    /** Converts one aggregate stock unit into a real world-owned physical log. Callers must reuse OperationId for retries. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool TryMaterializeStockAsWorldLog(FGuid StationId, FGuid OperationId, FName StockItemId, FName DefinitionId, FName BaseDefinitionId, FName MeshVariantSetId, int64 ExpectedStationRevision, const FTransform& SpawnTransform, TSubclassOf<ALogBuildPiece> LogActorClass, FGuid& OutWorldItemId);
    /** Retry-safe enqueue; caller must reuse OperationId when retrying the same request. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool TryEnqueueJob(FGuid OperationId, FGuid StationId, FName RecipeId, int64 ExpectedStationRevision, FGuid& OutJobId);
    /** Player-bound enqueue. Any recipe/input/output listed in discovery unlocks is gated by the player's saved knowledge. */
    UFUNCTION(BlueprintCallable, Category="World|Production|Progression") bool TryEnqueueJobForPlayer(FGuid OperationId, FGuid StationId, FName RecipeId, int64 ExpectedStationRevision, FGuid PlayerEntityId, FGuid& OutJobId);
    /** Legacy convenience wrapper. Use TryEnqueueJob for retry-safe callers. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool EnqueueJob(const FGuid& StationId, FName RecipeId, FGuid& OutJobId);
    UFUNCTION(BlueprintPure, Category="World|Production") bool CanAdvanceProductionStep(double DeltaSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Production") bool AdvanceProductionStep(double DeltaSeconds);
    /** Retry-safe collection; successful retries return true from the durable receipt. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool TryCollectCompletedJob(FGuid OperationId, FGuid StationId, FGuid JobId, int64 ExpectedStationRevision);
    /** Legacy convenience wrapper. Use TryCollectCompletedJob for retry-safe callers. */
    UFUNCTION(BlueprintCallable, Category="World|Production") bool CollectCompletedJob(const FGuid& StationId, const FGuid& JobId);
    UFUNCTION(BlueprintPure, Category="World|Production") bool GetStation(const FGuid& StationId, FWorldProductionStation& OutStation) const;
    void ExportSnapshot(TArray<FWorldProductionStation>& OutStations, TArray<FWorldProductionRecipe>& OutRecipes) const;
    bool RestoreSnapshot(const TArray<FWorldProductionStation>& StationSnapshot, const TArray<FWorldProductionRecipe>& RecipeSnapshot);
private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") TMap<FGuid, FWorldProductionStation> Stations;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") TArray<FWorldProductionRecipe> Recipes;
    int32 FindRecipe(FName RecipeId) const;
    int64 CountStockUnits(const TMap<FName, int32>& Stock) const;
    bool IsRecipeAvailableForPlayer(FName RecipeId, const FGuid& PlayerEntityId) const;
    bool TryEnqueueJobInternal(FGuid OperationId, FGuid StationId, FName RecipeId,
        int64 ExpectedStationRevision, FGuid& OutJobId);
};
