#include "WorldDiscoveryMarkerActor.h"

#include "ConstructionHelpers.h"
#include "InventoryCraftingComponent.h"
#include "InventoryCraftingTypes.h"
#include "ShelterSensorComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AWorldDiscoveryMarkerActor::AWorldDiscoveryMarkerActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;

    MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiscoveryMarker"));
    SetRootComponent(MarkerMesh);
    MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerMesh->SetGenerateOverlapEvents(false);
}

void AWorldDiscoveryMarkerActor::BeginPlay()
{
    Super::BeginPlay();
    if (MarkerMesh)
    {
        MarkerMesh->SetStaticMesh(MyProject3Construction::LoadEngineShape(TEXT("Sphere")));
        MarkerMesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.42f));
        MarkerMesh->SetMaterial(0, MyProject3Construction::MakeTint(
            this, FLinearColor(0.72f, 0.48f, 0.16f)));
    }

    if (IsAlreadyDiscovered())
        HideAsResolved();
}

bool AWorldDiscoveryMarkerActor::IsAlreadyDiscovered() const
{
    const UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!Survival || DiscoveryId.IsNone())
        return false;
    return Survival->GetDiscoveries().ContainsByPredicate(
        [this](const FWorldDiscoveryRecord& Record) { return Record.DiscoveryId == DiscoveryId; });
}

void AWorldDiscoveryMarkerActor::HideAsResolved()
{
    bResolved = true;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
}

void AWorldDiscoveryMarkerActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bResolved || DiscoveryId.IsNone() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
        return;

    if (IsAlreadyDiscovered())
    {
        HideAsResolved();
        return;
    }

    TimeUntilScan -= DeltaSeconds;
    if (TimeUntilScan > 0.0f)
        return;
    TimeUntilScan = 0.5f;

    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const UShelterSensorComponent* Shelter =
        Pawn ? Pawn->FindComponentByClass<UShelterSensorComponent>() : nullptr;
    if (!Pawn || !Shelter || !Shelter->SurvivalEntityId.IsValid())
        return;

    if (FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation()) > RevealRadiusCm)
    {
        bRangeMessageShown = false;
        return;
    }

    if (TryCommitDiscovery())
    {
        HideAsResolved();
        if (GEngine)
        {
            const FString Message = RewardQuantity > 0
                ? FString::Printf(TEXT("KEŞİF: %s · %d x %s"),
                    *DisplayName, RewardQuantity, *RewardItemId.ToString())
                : FString::Printf(TEXT("KEŞİF: %s"), *DisplayName);
            GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor(130, 225, 170), Message);
        }
    }
    else if (!bRangeMessageShown && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor(240, 190, 90),
            FString::Printf(TEXT("%s: keşif için ön koşul, ekipman veya envanter kapasitesi yetersiz."),
                *DisplayName));
        bRangeMessageShown = true;
    }
}

bool AWorldDiscoveryMarkerActor::TryCommitDiscovery()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const UShelterSensorComponent* Shelter =
        Pawn ? Pawn->FindComponentByClass<UShelterSensorComponent>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !Pawn || !Shelter || !Shelter->SurvivalEntityId.IsValid() ||
        !Survival || !Clock || !Clock->IsClockInitialized() ||
        !GameData || !GameData->IsApplied() || DiscoveryId.IsNone() ||
        !FMath::IsFinite(RevealRadiusCm) || RevealRadiusCm < 50.0f ||
        !FMath::IsFinite(Clock->GetSimulationTimeSeconds()))
        return false;

    FWorldSurvivalProfile Profile;
    if (!Survival->GetProfile(Shelter->SurvivalEntityId, Profile))
        return false;

    UInventoryCraftingComponent* Inventory =
        Pawn->FindComponentByClass<UInventoryCraftingComponent>();
    FInventoryStack RewardStack;
    bool bHasReward = RewardQuantity > 0;
    if (bHasReward)
    {
        const FGameDataItem* RewardDefinition =
            GameData->GetItems().Find(RewardItemId);
        if (!Inventory || RewardItemId.IsNone() || !RewardDefinition ||
            RewardDefinition->bConsumable || RewardDefinition->WeightGrams <= 0)
            return false;

        if (!Inventory->ItemWeightGrams.Contains(RewardItemId))
            Inventory->ItemWeightGrams.Add(RewardItemId, RewardDefinition->WeightGrams);
        RewardStack.ItemId = RewardItemId;
        RewardStack.Quantity = RewardQuantity;
        RewardStack.Category = EInventoryItemCategory::Resource;
        RewardStack.MaxStack = 99;
        if (!Inventory->CanAddItems({ RewardStack }))
            return false;
    }
    else if (!RewardItemId.IsNone() || RewardQuantity != 0)
    {
        return false;
    }

    TArray<FWorldSurvivalProfile> ProfilesBefore;
    TArray<FWorldDiscoveryRecord> DiscoveriesBefore;
    EWorldWeatherState WeatherBefore = EWorldWeatherState::Clear;
    float WeatherSeverityBefore = 0.0f;
    Survival->ExportSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);

    FGuid InventoryActorId;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;
    if (bHasReward)
    {
        Inventory->ExportSnapshot(InventoryActorId, InventoryOwnerBefore,
            InventoryItemsBefore, QuickAccessBefore);
        if (!InventoryOwnerBefore.IsValid() ||
            InventoryOwnerBefore != Inventory->GetInventoryOwnerId())
            return false;
    }

    const bool bDiscoveryCommitted = Survival->TryRecordConfiguredDiscovery(
        FGuid::NewGuid(), Shelter->SurvivalEntityId, Profile.Revision,
        DiscoveryId, GetActorLocation(), Clock->GetSimulationTimeSeconds());
    if (!bDiscoveryCommitted)
        return false;

    if (bHasReward && !Inventory->TryAddItem(RewardStack))
    {
        const bool bSurvivalRestored = Survival->RestoreSnapshot(
            ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);
        const bool bInventoryRestored = Inventory->RestoreSnapshot(
            InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        if (!bSurvivalRestored || !bInventoryRestored)
        {
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(
                    TEXT("Discovery reward rollback failed; stop simulation and recover/reset the world."));
            UE_LOG(LogTemp, Error, TEXT("Discovery rollback failed for %s."),
                *DiscoveryId.ToString());
        }
        return false;
    }

    return true;
}
