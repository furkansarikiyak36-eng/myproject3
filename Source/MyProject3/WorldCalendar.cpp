#include "WorldCalendar.h"

namespace WorldCalendar
{
    bool IsValidConfig(const FWorldCalendarConfig& Config)
    {
        return FMath::IsFinite(Config.SecondsPerDay) && Config.SecondsPerDay >= 60.0 &&
            Config.DaysPerMonth >= 1 && Config.DaysPerMonth <= 31 &&
            Config.StartMonth >= 1 && Config.StartMonth <= 12 &&
            Config.StartDayOfMonth >= 1 && Config.StartDayOfMonth <= Config.DaysPerMonth &&
            FMath::IsFinite(Config.StartHourOfDay) &&
            Config.StartHourOfDay >= 0.0f && Config.StartHourOfDay < 24.0f;
    }

    EWorldSeason SeasonOfMonth(int32 Month)
    {
        switch (Month)
        {
        case 3: case 4: case 5: return EWorldSeason::Spring;
        case 6: case 7: case 8: return EWorldSeason::Summer;
        case 9: case 10: case 11: return EWorldSeason::Autumn;
        default: return EWorldSeason::Winter;
        }
    }

    bool DateFromTime(double SimulationTimeSeconds, const FWorldCalendarConfig& Config, FWorldCalendarDate& OutDate)
    {
        if (!IsValidConfig(Config) || !FMath::IsFinite(SimulationTimeSeconds) || SimulationTimeSeconds < 0.0)
            return false;

        // Time measured from midnight of the start day.
        const double SinceStartMidnight =
            SimulationTimeSeconds + Config.SecondsPerDay * Config.StartHourOfDay / 24.0;
        const double WholeDays = FMath::FloorToDouble(SinceStartMidnight / Config.SecondsPerDay);
        if (WholeDays > static_cast<double>(MAX_int32) * 64.0)
            return false;
        const int64 DaysSinceStartDay = static_cast<int64>(WholeDays);
        const double SecondsIntoDay = SinceStartMidnight - WholeDays * Config.SecondsPerDay;

        const int64 DaysPerYear = 12 * static_cast<int64>(Config.DaysPerMonth);
        const int64 StartIndex =
            static_cast<int64>(Config.StartMonth - 1) * Config.DaysPerMonth + (Config.StartDayOfMonth - 1);
        const int64 DayIndex = StartIndex + DaysSinceStartDay;
        const int64 IndexInYear = DayIndex % DaysPerYear;

        OutDate.Year = static_cast<int32>(1 + DayIndex / DaysPerYear);
        OutDate.Month = static_cast<int32>(1 + IndexInYear / Config.DaysPerMonth);
        OutDate.DayOfMonth = static_cast<int32>(1 + IndexInYear % Config.DaysPerMonth);
        OutDate.DayOfYear = static_cast<int32>(1 + IndexInYear);
        // The start hour is part of day 0, so DaysElapsed counts midnights passed.
        OutDate.DaysElapsed = DaysSinceStartDay;
        OutDate.HourOfDay = static_cast<float>(24.0 * SecondsIntoDay / Config.SecondsPerDay);
        OutDate.Season = SeasonOfMonth(OutDate.Month);
        return true;
    }
}
