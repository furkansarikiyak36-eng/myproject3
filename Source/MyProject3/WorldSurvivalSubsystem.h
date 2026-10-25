#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldSurvivalSubsystem.generated.h"

UENUM(BlueprintType)
enum class EWorldWeatherState : uint8 { Clear, Rain, Wind, Snow, Storm, Heatwave };

USTRUCT(BlueprintType)
struct FWorldSurvivalProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival") FGuid EntityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Hunger = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Thirst = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Stamina = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="100")) float Fatigue = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float ClothingInsulation = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float ShelterProtection = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float Wetness = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="World|Survival") int64 Revision = 1;
};

USTRUCT(BlueprintType)
struct FWorldDiscoveryRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") FName DiscoveryId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Exploration") double DiscoveredAtTime = 0.0;
};

/** Bounded survival/exposure model and persistent discovery registry. */
UCLASS()
class MYPROJECT3_API UWorldSurvivalSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") EWorldWeatherState Weather = EWorldWeatherState::Clear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival", meta=(ClampMin="0", ClampMax="1")) float WeatherSeverity = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float HungerPerHour = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float ThirstPerHour = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float FatiguePerHour = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Survival") float ColdDamagePerHour = 4.0f;

    UFUNCTION(BlueprintCallable, Category="World|Survival") bool RegisterProfile(const FWorldSurvivalProfile& Profile);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool UpdateProtection(const FGuid& EntityId, float ClothingInsulation, float ShelterProtection, int64 ExpectedRevision);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool SetWeather(EWorldWeatherState NewWeather, float Severity);
    UFUNCTION(BlueprintCallable, Category="World|Survival") bool AdvanceSurvivalStep(double DeltaSeconds, double LogicalTimeSeconds);
    UFUNCTION(BlueprintCallable, Category="World|Exploration") bool RecordDiscovery(FName DiscoveryId, FVector Location, double LogicalTimeSeconds);
    UFUNCTION(BlueprintPure, Category="World|Survival") bool GetProfile(const FGuid& EntityId, FWorldSurvivalProfile& OutProfile) const;
    UFUNCTION(BlueprintPure, Category="World|Exploration") TArray<FWorldDiscoveryRecord> GetDiscoveries() const { return Discoveries; }
private:
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Survival") TMap<FGuid, FWorldSurvivalProfile> Profiles;
    UPROPERTY(VisibleAnywhere, SaveGame, Category="World|Exploration") TArray<FWorldDiscoveryRecord> Discoveries;
};
