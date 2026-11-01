#include "TerrainDigToolComponent.h"

#include "VoxelTerrainChunkComponent.h"
#include "WorldTerrainStateSubsystem.h"
#include "LogCarryComponent.h"
#include "LogBuildPiece.h"
#include "InventoryCraftingComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "ShelterSensorComponent.h"
#include "WorldGroundActor.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

UTerrainDigToolComponent::UTerrainDigToolComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTerrainDigToolComponent::BindDefaultInput(UInputComponent* InputComponent)
{
    if (InputComponent)
        InputComponent->BindKey(EKeys::N, IE_Pressed, this, &UTerrainDigToolComponent::DigInput);
}

bool UTerrainDigToolComponent::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Pawn && Pawn->GetController())
    {
        Pawn->GetController()->GetPlayerViewPoint(OutLocation, OutRotation);
        return true;
    }
    if (GetOwner())
    {
        GetOwner()->GetActorEyesViewPoint(OutLocation, OutRotation);
        return true;
    }
    return false;
}

void UTerrainDigToolComponent::SetStatus(const FString& Message) const
{
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Message);
}

void UTerrainDigToolComponent::DigInput()
{
    UWorld* World = GetWorld();
    if (!World || !FMath::IsFinite(TraceDistanceCm) || TraceDistanceCm < 50.0f ||
        !FMath::IsFinite(DigRadiusCm) || DigRadiusCm <= 0.0f ||
        !FMath::IsFinite(ToolStrength) || ToolStrength <= 0.0f ||
        !FMath::IsFinite(LogicalCellSizeCm) || LogicalCellSizeCm < 50.0f ||
        DepthLayer < 0 || RegionId.IsNone())
    {
        SetStatus(TEXT("Kazı aracı ayarları geçersiz."));
        return;
    }

    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    const UInventoryCraftingComponent* Inventory = OwnerPawn
        ? OwnerPawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    const UWorldGameDataSubsystem* GameData = World->GetSubsystem<UWorldGameDataSubsystem>();
    float DiggingStickStrength = bRequireInventoryTool ? 0.0f : 1.0f;
    if (bRequireInventoryTool && (!Inventory || Inventory->GetItemCount(TEXT("Tool.DiggingStick")) <= 0 ||
        !GameData || !GameData->IsApplied() ||
        !GameData->GetToolStrength(TEXT("Tool.DiggingStick"), DiggingStickStrength)))
    {
        SetStatus(TEXT("Kazmak için envanterde Tool.DiggingStick gerekir."));
        return;
    }
    float EffectiveToolStrength = FMath::Clamp(
        ToolStrength * DiggingStickStrength, 0.01f, 100.0f);

    FVector ViewLocation;
    FRotator ViewRotation;
    if (!GetView(ViewLocation, ViewRotation))
    {
        SetStatus(TEXT("Kazı için oyuncu görüşü bulunamadı."));
        return;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(TerrainDigToolTrace), false, GetOwner());
    // A carried log is in front of the camera in this first-person prototype;
    // it must not mask the terrain hit test.
    if (const ULogCarryComponent* Carry = GetOwner()
        ? GetOwner()->FindComponentByClass<ULogCarryComponent>() : nullptr)
    {
        if (Carry->HeldPiece)
            Params.AddIgnoredActor(Carry->HeldPiece);
        if (Carry->HeldMaterialPiece)
            Params.AddIgnoredActor(Carry->HeldMaterialPiece);
    }
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, ViewLocation,
        ViewLocation + ViewRotation.Vector() * TraceDistanceCm, ECC_Visibility, Params))
    {
        SetStatus(TEXT("Kazılabilir voxel yüzeyi hedeflenmedi."));
        return;
    }

    // World map ground: the landscape and its voxel columns are dug through the ground actor.
    AWorldGroundActor* Ground = nullptr;
    for (TActorIterator<AWorldGroundActor> It(World); It; ++It)
    {
        Ground = *It;
        break;
    }
    if (Ground && Ground->Landscape && Hit.GetActor() == Ground->Landscape && Ground->IsDugColumn(Hit.ImpactPoint))
    {
        // The landscape is hidden over dug columns; look through it to the voxel ground.
        Params.AddIgnoredActor(Ground->Landscape.Get());
        if (!World->LineTraceSingleByChannel(Hit, ViewLocation,
            ViewLocation + ViewRotation.Vector() * TraceDistanceCm, ECC_Visibility, Params))
        {
            SetStatus(TEXT("Kazılabilir zemin hedeflenmedi."));
            return;
        }
    }
    const bool bWorldGround = Ground && (Hit.GetActor() == Ground || (Ground->Landscape && Hit.GetActor() == Ground->Landscape));

    UVoxelTerrainChunkComponent* Chunk = Cast<UVoxelTerrainChunkComponent>(Hit.GetComponent());
    UWorldTerrainStateSubsystem* TerrainState = World->GetSubsystem<UWorldTerrainStateSubsystem>();
    if (bWorldGround)
    {
        const UShelterSensorComponent* GroundSensor = OwnerPawn
            ? OwnerPawn->FindComponentByClass<UShelterSensorComponent>() : nullptr;
        UWorldSurvivalSubsystem* GroundSurvival = World->GetSubsystem<UWorldSurvivalSubsystem>();
        if (GroundSurvival && GroundSensor && GroundSensor->SurvivalEntityId.IsValid())
        {
            if (!GroundSurvival->RecordActivity(GroundSensor->SurvivalEntityId, ESurvivalActivity::Dig, 1.0f, 0.0f))
            {
                SetStatus(TEXT("Çok yorgunsun veya dinleniyorsun; kazmak için toparlan."));
                return;
            }
            EffectiveToolStrength *= GroundSurvival->GetWorkEffectiveness(GroundSensor->SurvivalEntityId);
        }
        FString Status;
        Ground->DigAt(Hit.ImpactPoint, DigRadiusCm, EffectiveToolStrength, Status);
        SetStatus(Status);
        return;
    }
    if (!IsValid(Chunk) || !TerrainState)
    {
        SetStatus(TEXT("Hedef bir voxel kazı cebi değil."));
        return;
    }

    // The physical density grid uses chunk-local centimetres; the logical ledger
    // uses stable world-space cells so adjacent chunks don't alias by local index.
    const FVector LocalHit = Chunk->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
    const FIntVector LogicalCell(
        FMath::FloorToInt(Hit.ImpactPoint.X / LogicalCellSizeCm),
        FMath::FloorToInt(Hit.ImpactPoint.Y / LogicalCellSizeCm),
        FMath::FloorToInt(Hit.ImpactPoint.Z / LogicalCellSizeCm));

    FWorldTerrainCellState ExistingCell;
    const int64 ExpectedCellRevision = TerrainState->TryGetCell(LogicalCell, ExistingCell)
        ? ExistingCell.Revision : 0;

    // Digging is strenuous work: resting and exhaustion block the attempt,
    // while fatigue reduces the effective strength of each shovel/stick stroke.
    const UShelterSensorComponent* SurvivalSensor = OwnerPawn
        ? OwnerPawn->FindComponentByClass<UShelterSensorComponent>() : nullptr;
    UWorldSurvivalSubsystem* Survival = World->GetSubsystem<UWorldSurvivalSubsystem>();
    if (Survival && SurvivalSensor && SurvivalSensor->SurvivalEntityId.IsValid())
    {
        if (!Survival->RecordActivity(
            SurvivalSensor->SurvivalEntityId, ESurvivalActivity::Dig, 1.0f, 0.0f))
        {
            SetStatus(TEXT("Çok yorgunsun veya dinleniyorsun; kazmak için toparlan."));
            return;
        }
        EffectiveToolStrength *= Survival->GetWorkEffectiveness(SurvivalSensor->SurvivalEntityId);
    }

    const UWorldSimulationClockSubsystem* Clock = World->GetSubsystem<UWorldSimulationClockSubsystem>();
    if (!Clock || !Clock->IsClockInitialized())
    {
        SetStatus(TEXT("Kazı için ortak dünya saati hazır değil."));
        return;
    }

    FWorldTerrainCellState CommittedState;
    TArray<FGuid> ResourceItemIds;
    const bool bCommitted = TerrainState->TryExcavateForGeology(
        FGuid::NewGuid(), Chunk, LocalHit, DigRadiusCm, EffectiveToolStrength,
        Chunk->GetDensityRevision(), LogicalCell, DepthLayer, ExpectedCellRevision,
        RegionId, Clock->GetRandomSeed(), CommittedState, ResourceItemIds);

    if (bCommitted)
        SetStatus(FString::Printf(TEXT("Kazı tamamlandı. Hücre revizyonu: %lld · Kaynak: %d"),
            CommittedState.Revision, ResourceItemIds.Num()));
    else
        SetStatus(TEXT("Kazı reddedildi: jeoloji/bölge kuralı, kimlik, sınır, destek veya revizyon kontrolü."));
}
