#include "WorldDiscoveryMarkerActor.h"

#include "ConstructionHelpers.h"
#include "InventoryCraftingComponent.h"
#include "InventoryCraftingTypes.h"
#include "ShelterSensorComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"

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
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!World || !Pawn || !Shelter || !Shelter->SurvivalEntityId.IsValid() ||
        !Survival || !Clock || !Clock->IsClockInitialized() ||
        !GameData || !GameData->IsApplied() || DiscoveryId.IsNone() ||
        !FMath::IsFinite(RevealRadiusCm) || RevealRadiusCm < 50.0f ||
        !FMath::IsFinite(Clock->GetSimulationTimeSeconds()) ||
        RewardQuantity < 0 || RewardQuantity > 64)
        return false;

    // The actor is a view/trigger for authored discovery data, never an alternate
    // source of reward authority. A misplaced or hand-edited marker cannot grant
    // different items or unlocks than its validated GameData record.
    const FGameDataDiscovery* AuthoredDiscovery =
        GameData->GetDiscoveries().FindByPredicate([this](const FGameDataDiscovery& Candidate)
        {
            return Candidate.Id == DiscoveryId;
        });
    if (!AuthoredDiscovery ||
        AuthoredDiscovery->RewardItemId != RewardItemId ||
        AuthoredDiscovery->RewardQuantity != RewardQuantity ||
        AuthoredDiscovery->bRewardUniqueItems != bRewardUniqueItems ||
        !FMath::IsNearlyEqual(AuthoredDiscovery->RevealRadiusCm, RevealRadiusCm, 0.01f) ||
        FVector::Dist2D(GetActorLocation(), AuthoredDiscovery->Location) > 50.0f)
        return false;

    FWorldSurvivalProfile Profile;
    if (!Survival->GetProfile(Shelter->SurvivalEntityId, Profile))
        return false;

    UInventoryCraftingComponent* Inventory =
        Pawn->FindComponentByClass<UInventoryCraftingComponent>();
    const bool bHasReward = RewardQuantity > 0;
    if (bHasReward != !RewardItemId.IsNone() ||
        (bRewardUniqueItems && !bHasReward))
        return false;

    TArray<FInventoryStack> RewardStacks;
    FGuid InventoryActorBefore;
    FGuid InventoryOwnerBefore;
    TArray<FInventoryStack> InventoryItemsBefore;
    FName QuickAccessBefore = NAME_None;

    TArray<FWorldOwnedItemRecord> OwnershipItemsBefore;
    TArray<FWorldItemTransferReceipt> OwnershipTransfersBefore;
    TArray<FWorldItemSplitReceipt> OwnershipSplitsBefore;

    if (bHasReward)
    {
        const FGameDataItem* RewardDefinition =
            GameData->GetItems().Find(RewardItemId);
        if (!Inventory || !RewardDefinition || RewardDefinition->bConsumable ||
            RewardDefinition->WeightGrams <= 0)
            return false;

        if (!Inventory->ItemWeightGrams.Contains(RewardItemId))
            Inventory->ItemWeightGrams.Add(RewardItemId, RewardDefinition->WeightGrams);

        Inventory->ExportSnapshot(InventoryActorBefore, InventoryOwnerBefore,
            InventoryItemsBefore, QuickAccessBefore);
        if (!InventoryOwnerBefore.IsValid() ||
            InventoryOwnerBefore != Inventory->GetInventoryOwnerId())
            return false;
        (void)InventoryActorBefore;

        if (bRewardUniqueItems)
        {
            if (!Ownership || Ownership->MaxTrackedItems <= 0 ||
                RewardQuantity > Ownership->MaxTrackedItems - Ownership->GetTrackedItemCount())
                return false;
            RewardStacks.Reserve(RewardQuantity);
            for (int32 Index = 0; Index < RewardQuantity; ++Index)
            {
                FInventoryStack Stack;
                Stack.ItemId = RewardItemId;
                Stack.Quantity = 1;
                Stack.Category = EInventoryItemCategory::Resource;
                Stack.MaxStack = 1;
                Stack.OwnedWorldItemId = FGuid::NewGuid();
                if (!Stack.OwnedWorldItemId.IsValid())
                    return false;
                RewardStacks.Add(Stack);
            }
            Ownership->ExportSnapshot(OwnershipItemsBefore,
                OwnershipTransfersBefore, OwnershipSplitsBefore);
        }
        else
        {
            FInventoryStack Stack;
            Stack.ItemId = RewardItemId;
            Stack.Quantity = RewardQuantity;
            Stack.Category = EInventoryItemCategory::Resource;
            Stack.MaxStack = 99;
            RewardStacks.Add(Stack);
        }

        if (!Inventory->CanAddItems(RewardStacks))
            return false;
    }

    TArray<FWorldSurvivalProfile> ProfilesBefore;
    TArray<FWorldDiscoveryRecord> DiscoveriesBefore;
    EWorldWeatherState WeatherBefore = EWorldWeatherState::Clear;
    float WeatherSeverityBefore = 0.0f;
    Survival->ExportSnapshot(ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);

    const bool bDiscoveryCommitted = Survival->TryRecordConfiguredDiscovery(
        FGuid::NewGuid(), Shelter->SurvivalEntityId, Profile.Revision,
        DiscoveryId, GetActorLocation(), Clock->GetSimulationTimeSeconds());
    if (!bDiscoveryCommitted)
        return false;

    bool bRewardCommitted = true;
    if (bHasReward && bRewardUniqueItems)
    {
        for (const FInventoryStack& Stack : RewardStacks)
        {
            FWorldOwnedItemRecord Record;
            Record.ItemId = Stack.OwnedWorldItemId;
            Record.DefinitionId = RewardItemId;
            Record.BaseDefinitionId = RewardItemId;
            Record.Quantity = 1;
            Record.OwnerKind = EWorldItemOwnerKind::Inventory;
            Record.OwnerId = InventoryOwnerBefore;
            Record.WorldLocation = Pawn->GetActorLocation();
            if (!Ownership->RegisterItem(Record) || !Inventory->TryAddItem(Stack))
            {
                bRewardCommitted = false;
                break;
            }
        }
    }
    else if (bHasReward)
    {
        bRewardCommitted = Inventory->TryAddItem(RewardStacks[0]);
    }

    if (!bRewardCommitted)
    {
        const bool bOwnershipRestored = !bRewardUniqueItems ||
            Ownership->RestoreSnapshot(OwnershipItemsBefore,
                OwnershipTransfersBefore, OwnershipSplitsBefore);
        const bool bInventoryRestored = !bHasReward ||
            Inventory->RestoreSnapshot(InventoryOwnerBefore, InventoryItemsBefore, QuickAccessBefore);
        const bool bSurvivalRestored = Survival->RestoreSnapshot(
            ProfilesBefore, DiscoveriesBefore, WeatherBefore, WeatherSeverityBefore);
        if (!bOwnershipRestored || !bInventoryRestored || !bSurvivalRestored)
        {
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(
                    TEXT("Discovery reward rollback failed; stop simulation and recover/reset the world."));
            }
            UE_LOG(LogTemp, Error, TEXT("Discovery reward rollback failed for %s."),
                *DiscoveryId.ToString());
        }
        return false;
    }

    return true;
}
