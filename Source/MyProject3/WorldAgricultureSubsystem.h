#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldAgricultureSubsystem.generated.h"

UENUM(BlueprintType)
enum class ECropGrowthStage : uint8 { Seed, Sprout, Growing, Mature, Withered };

USTRUCT(BlueprintType)
struct FWorldCropState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") FGuid CropId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") FName CropDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") FIntVector PlotCell = FIntVector::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm") ECropGrowthStage Stage = ECropGrowthStage::Seed;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm", meta=(ClampMin="0", ClampMax="1")) float Moisture = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm", meta=(ClampMin="0")) double GrowthProgressSeconds = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Farm", meta=(ClampMin="1")) double SecondsPerStage = 3600.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Farm") int64 Revision = 1;
};

USTRUCT(BlueprintType)
struct FWorldAnimalState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid AnimalId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName AnimalDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FGuid OwnerSettlementId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Hunger = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0", ClampMax="100")) float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals") FName ProductDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Animals", meta=(ClampMin="0")) int32 ProductUnits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Animals") int64 Revision = 1;
};

/** Bounded logical crop/animal lifecycle; visuals and physical actors are adapters. */
UCLASS()
class MYPROJECT3_API UWorldAgricultureSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxCrops = 2048;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals|Limits", meta=(ClampMin="1", ClampMax="10000")) int32 MaxAnimals = 512;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Farm") float MinimumGrowthMoisture = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float HungerPerHour = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Animals") float ProductHungerLimit = 55.0f;

    UFUNCTION(BlueprintCallable, Category="World|Farm") bool RegisterCrop(const FWorldCropState& Crop);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool RegisterAnimal(const FWorldAnimalState& Animal);
    UFUNCTION(BlueprintCallable, Category="World|Farm") bool SetCropMoisture(const FGuid& CropId, float Moisture, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Animals") bool FeedAnimal(const FGuid& AnimalId, float HungerReduction, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Simulation") bool AdvanceAgricultureStep(double DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="World|Farm") bool GetCrop(const FGuid& CropId, FWorldCropState& OutCrop) const;
    UFUNCTION(BlueprintPure, Category="World|Animals") bool GetAnimal(const FGuid& AnimalId, FWorldAnimalState& OutAnimal) const;
private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Farm") TMap<FGuid, FWorldCropState> Crops;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Animals") TMap<FGuid, FWorldAnimalState> Animals;
};
