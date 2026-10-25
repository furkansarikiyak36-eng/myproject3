#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldProductionSubsystem.generated.h"

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

USTRUCT(BlueprintType)
struct FWorldProductionStation
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") FGuid StationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") FName StationTag = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production", meta=(ClampMin="1")) int32 StorageCapacityUnits = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Production") TMap<FName, int32> Stock;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Production") TArray<FWorldProductionJob> Jobs;
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
    UFUNCTION(BlueprintCallable, Category="World|Production") bool EnqueueJob(const FGuid& StationId, FName RecipeId, FGuid& OutJobId);
    UFUNCTION(BlueprintPure, Category="World|Production") bool CanAdvanceProductionStep(double DeltaSeconds) const;
    UFUNCTION(BlueprintCallable, Category="World|Production") bool AdvanceProductionStep(double DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="World|Production") bool CollectCompletedJob(const FGuid& StationId, const FGuid& JobId);
    UFUNCTION(BlueprintPure, Category="World|Production") bool GetStation(const FGuid& StationId, FWorldProductionStation& OutStation) const;
private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") TMap<FGuid, FWorldProductionStation> Stations;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Production") TArray<FWorldProductionRecipe> Recipes;
    int32 FindRecipe(FName RecipeId) const;
    int64 CountStockUnits(const TMap<FName, int32>& Stock) const;
};
