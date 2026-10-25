#include "WorldCommittedEventSubsystem.h"

#include "Engine/World.h"

bool UWorldCommittedEventSubsystem::PublishCommittedEvent(
    FWorldCommittedEvent Event)
{
    UWorld* World = GetWorld();
    if (!World ||
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

    // The world clock is authoritative for this transient notification.
    Event.WorldTimeSeconds = static_cast<double>(World->GetTimeSeconds());

    OnCommittedEvent.Broadcast(Event);
    return true;
}
