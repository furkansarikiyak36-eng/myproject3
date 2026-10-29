#include "WorldHudDataSubsystem.h"
#include "Engine/World.h"
#include "WorldClimateSubsystem.h"
#include "WorldEventDirectorSubsystem.h"
#include "WorldSimulationClockSubsystem.h"

bool UWorldHudDataSubsystem::GetSnapshot(FWorldHudDataSnapshot& OutSnapshot) const
{
    OutSnapshot = FWorldHudDataSnapshot();
    const UWorld* World = GetWorld();
    const UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!Clock || !Clock->GetCalendarDate(OutSnapshot.Calendar)) return false;
    OutSnapshot.SimulationTimeSeconds = Clock->GetSimulationTimeSeconds();
    OutSnapshot.ClockRevision = Clock->GetStateRevision();
    if (const UWorldClimateSubsystem* Climate = World->GetSubsystem<UWorldClimateSubsystem>();
        Climate && Climate->IsClimateInitialized())
    {
        OutSnapshot.Weather = Climate->GetWeather();
        OutSnapshot.WeatherSeverity = Climate->GetWeatherSeverity();
        OutSnapshot.AirTemperatureCelsius = Climate->GetTemperatureCelsius();
    }
    if (const UWorldEventDirectorSubsystem* Events = World->GetSubsystem<UWorldEventDirectorSubsystem>();
        Events && Events->IsEventDirectorInitialized())
    {
        const FString Status = Events->GetEventStatusText();
        if (!Status.IsEmpty())
            OutSnapshot.EventNotifications.Add(Status);
    }
    OutSnapshot.bValid = true;
    return true;
}
