#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldCalendar.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldHudDataSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FWorldHudDataSnapshot
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") bool bValid = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") FWorldCalendarDate Calendar;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") EWorldWeatherState Weather = EWorldWeatherState::Clear;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") float WeatherSeverity = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") float AirTemperatureCelsius = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") double SimulationTimeSeconds = 0.0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") int64 ClockRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") TArray<FString> EventNotifications;
    /** The next signalled event and the game seconds left until it starts (P1.4 countdown). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") FName NextEventId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|HUD") double SecondsUntilNextEvent = 0.0;
};

/** Read-only aggregation contract for date, climate and active event notifications. */
UCLASS()
class MYPROJECT3_API UWorldHudDataSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="World|HUD") bool GetSnapshot(FWorldHudDataSnapshot& OutSnapshot) const;
};
