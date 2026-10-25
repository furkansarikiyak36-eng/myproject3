#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayCoreTypes.h"
#include "WorldCommittedEventSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnWorldCommittedEvent,
    const FWorldCommittedEvent&, Event);

/**
 * Lightweight notification fan-out for committed world facts.
 *
 * This subsystem does not own domain state, queue durable events, or perform
 * mutations. Callers must invoke PublishCommittedEvent only after the domain
 * owner has successfully committed its state change.
 */
UCLASS()
class MYPROJECT3_API UWorldCommittedEventSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Gameplay|Events")
    FOnWorldCommittedEvent OnCommittedEvent;

    /**
     * Publishes a committed fact. Rejects invalid type, unsupported schema,
     * or missing operation identity. This is notification, not persistence.
     */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Events")
    bool PublishCommittedEvent(FWorldCommittedEvent Event);
};
