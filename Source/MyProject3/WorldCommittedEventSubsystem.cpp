#include "WorldCommittedEventSubsystem.h"

#include "Engine/World.h"
#include "WorldSimulationClockSubsystem.h"

bool UWorldCommittedEventSubsystem::PublishCommittedEvent(
    FWorldCommittedEvent Event)
{
    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;

    if (!World || !Clock || !Clock->IsClockInitialized() ||
        Event.Type == EWorldCommittedEventType::None ||
        static_cast<uint8>(Event.Type) > static_cast<uint8>(EWorldCommittedEventType::DiscoveryRecorded) ||
        Event.SchemaVersion != FWorldCommittedEvent::CurrentSchemaVersion ||
        !Event.OperationId.IsValid() || MaxRetainedEvents <= 0 ||
        MaxRetainedEvents > 65536)
    {
        return false;
    }

    if (!Event.EventId.IsValid())
        Event.EventId = FGuid::NewGuid();
    if (!Event.EventId.IsValid())
        return false;

    // Canonicalize the affected-object set so reordered retries are still the same
    // committed fact. Multiple event types (or object sets) may belong to one operation.
    TArray<FGuid> CanonicalIds = Event.AffectedObjectIds;
    for (const FGuid& Id : CanonicalIds)
        if (!Id.IsValid())
            return false;
    CanonicalIds.Sort([](const FGuid& A, const FGuid& B)
    {
        return A.ToString() < B.ToString();
    });
    for (int32 Index = CanonicalIds.Num() - 1; Index > 0; --Index)
        if (CanonicalIds[Index] == CanonicalIds[Index - 1])
            CanonicalIds.RemoveAt(Index);

    FString OperationKey = Event.OperationId.ToString() + TEXT(":") +
        LexToString(static_cast<uint8>(Event.Type));
    for (const FGuid& Id : CanonicalIds)
        OperationKey += TEXT(":") + Id.ToString();

    // Reuse of an event ID for a different fact is a conflict. An identical
    // operation/type/object-set retry is accepted but never broadcast twice.
    if (const FWorldCommittedEvent* Existing = PublishedEvents.Find(Event.EventId))
    {
        TArray<FGuid> ExistingIds = Existing->AffectedObjectIds;
        ExistingIds.Sort([](const FGuid& A, const FGuid& B)
        {
            return A.ToString() < B.ToString();
        });
        for (int32 Index = ExistingIds.Num() - 1; Index > 0; --Index)
            if (ExistingIds[Index] == ExistingIds[Index - 1])
                ExistingIds.RemoveAt(Index);
        if (Existing->OperationId != Event.OperationId || Existing->Type != Event.Type ||
            ExistingIds != CanonicalIds)
            return false;
        return true;
    }
    if (PublishedOperationKeys.Contains(OperationKey))
        return true;
    if (PublishedEvents.Num() >= MaxRetainedEvents)
        return false;

    Event.AffectedObjectIds = MoveTemp(CanonicalIds);
    // Use explicit logical simulation time, never frame/world elapsed time.
    Event.WorldTimeSeconds = Clock->GetSimulationTimeSeconds();

    PublishedOperationKeys.Add(OperationKey, Event.EventId);
    PublishedEvents.Add(Event.EventId, Event);
    OnCommittedEvent.Broadcast(Event);
    return true;
}
