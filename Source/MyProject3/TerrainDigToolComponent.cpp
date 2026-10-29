#include "TerrainDigToolComponent.h"

#include "VoxelTerrainChunkComponent.h"
#include "WorldTerrainStateSubsystem.h"
#include "LogCarryComponent.h"
#include "InventoryCraftingComponent.h"
#include "WorldGameDataSubsystem.h"
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
        DepthLayer < 0)
    {
        SetStatus(TEXT("Kazı aracı ayarları geçersiz."));
        return;
    }

    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    const UInventoryCraftingComponent* Inventory = OwnerPawn
        ? OwnerPawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    const UWorldGameDataSubsystem* GameData = World->GetSubsystem<UWorldGameDataSubsystem>();
    float DiggingStickStrength = 0.0f;
    if (!Inventory || Inventory->GetItemCount(TEXT("Tool.DiggingStick")) <= 0 ||
        !GameData || !GameData->IsApplied() ||
        !GameData->GetToolStrength(TEXT("Tool.DiggingStick"), DiggingStickStrength))
    {
        SetStatus(TEXT("Kazmak için envanterde Tool.DiggingStick gerekir."));
        return;
    }
    const float EffectiveToolStrength = FMath::Clamp(
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

    UVoxelTerrainChunkComponent* Chunk = Cast<UVoxelTerrainChunkComponent>(Hit.GetComponent());
    UWorldTerrainStateSubsystem* TerrainState = World->GetSubsystem<UWorldTerrainStateSubsystem>();
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

    FWorldTerrainCellState CommittedState;
    const bool bCommitted = TerrainState->TryApplyVoxelExcavation(
        FGuid::NewGuid(), Chunk, LocalHit, DigRadiusCm, EffectiveToolStrength,
        Chunk->GetDensityRevision(), LogicalCell, DepthLayer, ExpectedCellRevision,
        MaterialId, CommittedState);

    if (bCommitted)
        SetStatus(FString::Printf(TEXT("Kazı tamamlandı. Hücre revizyonu: %lld"), CommittedState.Revision));
    else
        SetStatus(TEXT("Kazı reddedildi: sınır, zemin direnci, destek güvenliği veya revizyon kontrolü."));
}
