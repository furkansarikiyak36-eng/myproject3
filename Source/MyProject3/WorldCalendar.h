#pragma once

#include "CoreMinimal.h"
#include "WorldCalendar.generated.h"

/** Season of the year, fixed by the month (northern mountain valley). */
UENUM(BlueprintType)
enum class EWorldSeason : uint8
{
    Spring, // March, April, May
    Summer, // June, July, August
    Autumn, // September, October, November
    Winter  // December, January, February
};

/** How simulation time maps to a date. Defaults follow the game plan (5 days a month). */
USTRUCT(BlueprintType)
struct FWorldCalendarConfig
{
    GENERATED_BODY()

    /** Simulation seconds in one game day (survival rates are per simulation hour). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Calendar", meta=(ClampMin="60"))
    double SecondsPerDay = 86400.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Calendar", meta=(ClampMin="1", ClampMax="31"))
    int32 DaysPerMonth = 5;

    /** The date at simulation time zero: the end of March of year 1, in the morning. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Calendar", meta=(ClampMin="1", ClampMax="12"))
    int32 StartMonth = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Calendar", meta=(ClampMin="1", ClampMax="31"))
    int32 StartDayOfMonth = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Calendar", meta=(ClampMin="0", ClampMax="23.99"))
    float StartHourOfDay = 8.0f;
};

USTRUCT(BlueprintType)
struct FWorldCalendarDate
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Calendar") int32 Year = 1;
    /** 1 = January ... 12 = December. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Calendar") int32 Month = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Calendar") int32 DayOfMonth = 1;
    /** 1 .. 12 * DaysPerMonth. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Calendar") int32 DayOfYear = 1;
    /** Days since the game started (0 on the first day). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Calendar") int64 DaysElapsed = 0;
    /** 0 .. 24. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Calendar") float HourOfDay = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Calendar") EWorldSeason Season = EWorldSeason::Winter;
};

/** Pure date math. The date is derived from the saved simulation time, so it needs no save data of its own. */
namespace WorldCalendar
{
    MYPROJECT3_API bool IsValidConfig(const FWorldCalendarConfig& Config);
    MYPROJECT3_API EWorldSeason SeasonOfMonth(int32 Month);
    /** Fails on an invalid config or a negative / non-finite time. */
    MYPROJECT3_API bool DateFromTime(double SimulationTimeSeconds, const FWorldCalendarConfig& Config, FWorldCalendarDate& OutDate);
}
