#include "WorldCommittedEventSubsystem.h"

#include "Engine/World.h"
#include "WorldSimulationClockSubsystem.h"

bool UWorldCommittedEventSubsystem::PublishCommittedEvent(
    FWorldCommittedEvent Event)
{
    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;

    if (!World ||
        !Clock ||
        !Clock->IsClockInitialized() ||
        Event.Type == EWorldCommittedEventType::None ||
        Event.SchemaVersion != FWorldCommittedEvent::CurrentSchemaVersion ||
        !Event.OperationId.IsValid())
    {
        return false;
    }

    if (!Event.EventId.IsValid())
    {
        Event.EventId = FGuid::NewGuid();
    }

    // Use explicit logical simulation time, never frame/world elapsed time.
    Event.WorldTimeSeconds = Clock->GetSimulationTimeSeconds();

    OnCommittedEvent.Broadcast(Event);
    return true;
}
