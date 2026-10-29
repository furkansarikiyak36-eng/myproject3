#include "EarthworkActor.h"

#include "InventoryCraftingComponent.h"
#include "ShelterSensorComponent.h"
#include "WorldClimateSubsystem.h"
#include "WorldStorageSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldTerrainStateSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AEarthworkActor::AEarthworkActor()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 5.0f;
}

void AEarthworkActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bValid && !bCollapsed)
        CheckStability(); // A removed post makes the room cave in.
}

void AEarthworkActor::BeginPlay()
{
    Super::BeginPlay();
    if (!bCollapsed)
        Validate();
}

FIntVector AEarthworkActor::CellAt(const FVector& Location, int32 Layer, float CellSizeCm)
{
    const float Size = FMath::Max(1.0f, CellSizeCm);
    return FIntVector(FMath::FloorToInt(Location.X / Size), FMath::FloorToInt(Location.Y / Size), Layer);
}

bool AEarthworkActor::IsCellExcavated(int32 X, int32 Y, int32 Layer) const
{
    const UWorldTerrainStateSubsystem* Terrain = GetWorld() ? GetWorld()->GetSubsystem<UWorldTerrainStateSubsystem>() : nullptr;
    FWorldTerrainCellState Cell;
    return Terrain && Terrain->TryGetCell(FIntVector(X, Y, Layer), Cell) && Cell.bExcavated;
}

bool AEarthworkActor::Validate()
{
    UWorld* World = GetWorld();
    const UWorldTerrainStateSubsystem* Terrain = World ? World->GetSubsystem<UWorldTerrainStateSubsystem>() : nullptr;
    bValid = false;
    if (!Terrain || bCollapsed || SizeCells.X <= 0 || SizeCells.Y <= 0)
        return false;

    switch (Purpose)
    {
    case EEarthworkPurpose::Cellar:
    case EEarthworkPurpose::Cistern:
        // Every cell of the footprint is dug from the surface down to the room's layer.
        for (int32 X = OriginCell.X; X < OriginCell.X + SizeCells.X; ++X)
            for (int32 Y = OriginCell.Y; Y < OriginCell.Y + SizeCells.Y; ++Y)
                for (int32 Layer = 0; Layer <= DepthLayer; ++Layer)
                    if (!IsCellExcavated(X, Y, Layer))
                        return false;
        break;
    case EEarthworkPurpose::Trench:
    {
        // A line one cell wide and at least three long, dug at the surface.
        const int32 Length = FMath::Max(SizeCells.X, SizeCells.Y);
        if (FMath::Min(SizeCells.X, SizeCells.Y) != 1 || Length < 3)
            return false;
        for (int32 X = OriginCell.X; X < OriginCell.X + SizeCells.X; ++X)
            for (int32 Y = OriginCell.Y; Y < OriginCell.Y + SizeCells.Y; ++Y)
                if (!IsCellExcavated(X, Y, 0))
                    return false;
        break;
    }
    case EEarthworkPurpose::Embankment:
    {
        // Filled cells (edited, not dug) along the footprint.
        for (int32 X = OriginCell.X; X < OriginCell.X + SizeCells.X; ++X)
            for (int32 Y = OriginCell.Y; Y < OriginCell.Y + SizeCells.Y; ++Y)
            {
                FWorldTerrainCellState Cell;
                if (!Terrain->TryGetCell(FIntVector(X, Y, 0), Cell) || Cell.bExcavated)
                    return false;
            }
        break;
    }
    }

    if (Purpose == EEarthworkPurpose::Cellar)
    {
        // A cellar is roofed; open pits are not cool stores.
        float Roof = 0.0f, Walls = 0.0f;
        UShelterSensorComponent::MeasureAt(World, GetActorLocation() + FVector(0.0, 0.0, 30.0), this, Roof, Walls);
        if (Roof < 0.6f)
            return false;
        UWorldStorageSubsystem* Storage = World->GetSubsystem<UWorldStorageSubsystem>();
        if (!Storage)
            return false;
        if (!StorageId.IsValid())
            StorageId = FGuid::NewGuid();
        FWorldStorageContainerRecord Existing;
        if (!Storage->GetStorage(StorageId, Existing) &&
            !Storage->RegisterStorage(StorageId, 8 * SizeCells.X * SizeCells.Y))
            return false;
        Storage->SetStorageSpoilageRate(StorageId, CellarSpoilageRate);
    }
    if (Purpose == EEarthworkPurpose::Cistern && LastRainfallIndex < 0.0)
    {
        if (const UWorldClimateSubsystem* Climate = World->GetSubsystem<UWorldClimateSubsystem>())
            LastRainfallIndex = Climate->GetRainfallIndex();
    }
    bValid = true;
    return true;
}

bool AEarthworkActor::HasSupportNearby() const
{
    const UWorldStructuralGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    TArray<FWorldStructuralNodeSnapshot> Nodes;
    if (!Graph || !Graph->ExportSnapshot(Nodes))
        return false;
    for (const FWorldStructuralNodeSnapshot& Node : Nodes)
    {
        FStructuralNodeRecord Live;
        if (!Node.bGroundSupported || !Graph->GetNode(Node.NodeId, Live) || !Live.Actor.IsValid())
            continue;
        // A support stands at the room's level (a post), not high above it (the roof it carries).
        const FVector Location = Live.Actor->GetActorLocation();
        if (Location.Z <= GetActorLocation().Z + 150.0f &&
            FVector::Dist2D(Location, GetActorLocation()) <= SupportSearchRadiusCm)
            return true;
    }
    return false;
}

bool AEarthworkActor::CheckStability()
{
    if (bCollapsed || !bValid)
        return false;
    const bool bUnderground = Purpose == EEarthworkPurpose::Cellar || Purpose == EEarthworkPurpose::Cistern;
    if (!bUnderground || SizeCells.X * SizeCells.Y < CellsNeedingSupport || HasSupportNearby())
        return false;

    // The room caves in: its cells fill back in and what was stored is out of reach.
    UWorld* World = GetWorld();
    UWorldTerrainStateSubsystem* Terrain = World ? World->GetSubsystem<UWorldTerrainStateSubsystem>() : nullptr;
    if (!Terrain)
        return false;
    for (int32 X = OriginCell.X; X < OriginCell.X + SizeCells.X; ++X)
        for (int32 Y = OriginCell.Y; Y < OriginCell.Y + SizeCells.Y; ++Y)
            for (int32 Layer = 1; Layer <= DepthLayer; ++Layer)
            {
                FWorldTerrainCellState Cell;
                if (Terrain->TryGetCell(FIntVector(X, Y, Layer), Cell))
                {
                    FWorldTerrainCellState Filled;
                    Terrain->TryApplyCellEdit(FGuid::NewGuid(), FIntVector(X, Y, Layer), Layer,
                        ETerrainCellEdit::Fill, TEXT("Soil"), Cell.Revision, Filled);
                }
            }
    if (StorageId.IsValid())
        if (UWorldStorageSubsystem* Storage = World->GetSubsystem<UWorldStorageSubsystem>())
            Storage->SetStorageAvailable(StorageId, false);
    bCollapsed = true;
    bValid = false;
    StoredWater = 0.0f;
    return true;
}

void AEarthworkActor::Collect()
{
    const UWorldClimateSubsystem* Climate = GetWorld() ? GetWorld()->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    if (!Climate || Purpose != EEarthworkPurpose::Cistern || !IsValidEarthwork())
        return;
    const double Index = Climate->GetRainfallIndex();
    if (LastRainfallIndex < 0.0)
        LastRainfallIndex = Index;
    StoredWater = FMath::Min(static_cast<float>(CisternCapacityUnits),
        StoredWater + static_cast<float>(FMath::Max(0.0, Index - LastRainfallIndex)) * CisternUnitsPerRainHour);
    LastRainfallIndex = Index;
}

int32 AEarthworkActor::GetCisternWaterUnits()
{
    Collect();
    return FMath::FloorToInt(StoredWater);
}

bool AEarthworkActor::TryDrawWater(UInventoryCraftingComponent* Inventory, int32 Quantity)
{
    Collect();
    if (!IsValid(Inventory) || Quantity <= 0 || Purpose != EEarthworkPurpose::Cistern || !IsValidEarthwork() ||
        StoredWater < static_cast<float>(Quantity))
        return false;
    FInventoryStack Water;
    Water.ItemId = TEXT("Water.Dirty");
    Water.Quantity = Quantity;
    if (!Inventory->TryAddProducedItems(Water)) // Becomes drinkable ledger items; rain water still needs boiling.
        return false;
    StoredWater -= static_cast<float>(Quantity);
    return true;
}
