#include "WorldZoneDiscoveryTriggerActor.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "LogSnapCharacter.h"
#include "WorldGameDataSubsystem.h"

AWorldZoneDiscoveryTriggerActor::AWorldZoneDiscoveryTriggerActor()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneDiscoveryTrigger"));
    SetRootComponent(TriggerBox);
    TriggerBox->SetBoxExtent(FVector(250.0, 250.0, 150.0));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);
}

void AWorldZoneDiscoveryTriggerActor::BeginPlay()
{
    Super::BeginPlay();
    if (TriggerBox)
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AWorldZoneDiscoveryTriggerActor::HandleZoneOverlap);
}

void AWorldZoneDiscoveryTriggerActor::HandleZoneOverlap(
    UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComponent;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    ALogSnapCharacter* Player = Cast<ALogSnapCharacter>(OtherActor);
    if (!Player)
        return;

    const FGuid PlayerEntityId = Player->SurvivalEntityId;
    if (!PlayerEntityId.IsValid())
    {
        LastEntryResult = TEXT("Zone discovery refused: player has no valid survival entity ID.");
        return;
    }

    if (bRecordOnlyOncePerPlayer && SuccessfullyRecordedPlayers.Contains(PlayerEntityId))
        return;

    UWorld* World = GetWorld();
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!GameData || ZoneId.IsNone())
    {
        LastEntryResult = TEXT("Zone discovery refused: game data or ZoneId is missing.");
        return;
    }

    FString Reason;
    if (!GameData->TryRecordZoneEntry(FGuid::NewGuid(), PlayerEntityId, ZoneId,
        Player->GetActorLocation(), Reason))
    {
        LastEntryResult = Reason;
        return;
    }

    SuccessfullyRecordedPlayers.Add(PlayerEntityId);
    LastEntryResult = FString::Printf(TEXT("Zone discovery recorded: %s"), *ZoneId.ToString());
}
