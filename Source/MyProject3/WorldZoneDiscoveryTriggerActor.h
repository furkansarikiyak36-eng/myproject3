#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldZoneDiscoveryTriggerActor.generated.h"

class UBoxComponent;

/**
 * Reusable overlap adapter for data-driven zone discovery.
 * It records entry only after UWorldGameDataSubsystem validates the zone gates.
 * It does not create a physical barrier or place itself into any level.
 */
UCLASS()
class MYPROJECT3_API AWorldZoneDiscoveryTriggerActor : public AActor
{
    GENERATED_BODY()

public:
    AWorldZoneDiscoveryTriggerActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Zones")
    TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="World|Zones")
    FName ZoneId = NAME_None;

    /** Last accepted/denied result for a UI or Blueprint adapter to present. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Zones")
    FString LastEntryResult;

    /** Repeated overlaps from the same player are suppressed after a successful discovery. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Zones")
    bool bRecordOnlyOncePerPlayer = true;

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleZoneOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    TSet<FGuid> SuccessfullyRecordedPlayers;
};
