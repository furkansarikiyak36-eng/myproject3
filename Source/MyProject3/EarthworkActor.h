#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EarthworkActor.generated.h"

class UInventoryCraftingComponent;

UENUM(BlueprintType)
enum class EEarthworkPurpose : uint8
{
    Cellar,     // dug room below ground, roofed: cool storage
    Cistern,    // dug basin: collects rain water
    Trench,     // a dug line: a defensive / drainage ditch
    Embankment  // filled cells: a raised bank against floods
};

/**
 * A purpose given to dug terrain (P5.2). It reads the logical terrain cells over its
 * footprint (cell X/Y = location / CellSizeCm, cell Z = depth layer) and is valid only
 * when they match its purpose. A valid cellar becomes a cool storage; a valid cistern
 * collects rain from the climate's saved rainfall index. An underground room with no
 * ground-supported structure near it collapses: its cells are filled back in and its
 * storage becomes unusable.
 */
UCLASS()
class MYPROJECT3_API AEarthworkActor : public AActor
{
    GENERATED_BODY()

public:
    AEarthworkActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork") EEarthworkPurpose Purpose = EEarthworkPurpose::Cellar;
    /** Footprint in terrain cells, starting at OriginCell (X, Y). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork") FIntPoint OriginCell = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork") FIntPoint SizeCells = FIntPoint(2, 2);
    /** Layer a cellar or cistern is dug to. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork", meta=(ClampMin="1")) int32 DepthLayer = 1;
    /** Footprints of this many cells or more need a support (a post) near them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork", meta=(ClampMin="1")) int32 CellsNeedingSupport = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork", meta=(ClampMin="50")) float SupportSearchRadiusCm = 400.0f;
    /** Spoilage rate of a valid cellar's storage (1 = open air). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork", meta=(ClampMin="0", ClampMax="1")) float CellarSpoilageRate = 0.4f;
    /** Dirty water units a cistern gains per unit of the climate rainfall index (severity x hours). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork", meta=(ClampMin="0")) float CisternUnitsPerRainHour = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Earthwork", meta=(ClampMin="1")) int32 CisternCapacityUnits = 20;

    /** Checks the terrain cells (and the roof for a cellar). Registers the cellar storage when valid. */
    UFUNCTION(BlueprintCallable, Category="Earthwork") bool Validate();
    UFUNCTION(BlueprintPure, Category="Earthwork") bool IsValidEarthwork() const { return bValid && !bCollapsed; }
    UFUNCTION(BlueprintPure, Category="Earthwork") bool IsCollapsed() const { return bCollapsed; }
    UFUNCTION(BlueprintPure, Category="Earthwork") FGuid GetStorageId() const { return StorageId; }

    /** An unsupported underground room collapses. Returns true if it collapsed now. */
    UFUNCTION(BlueprintCallable, Category="Earthwork") bool CheckStability();

    /** Water stored in a cistern now (from rain since the last draw, capped). */
    UFUNCTION(BlueprintCallable, Category="Earthwork") int32 GetCisternWaterUnits();
    /** Fills the inventory with up to Quantity units of dirty water from a valid cistern. */
    UFUNCTION(BlueprintCallable, Category="Earthwork") bool TryDrawWater(UInventoryCraftingComponent* Inventory, int32 Quantity);

    /** The terrain cell that holds a world location (X, Y) at a depth layer. */
    static FIntVector CellAt(const FVector& Location, int32 Layer, float CellSizeCm);

protected:
    virtual void BeginPlay() override;
    /** Every few seconds a valid underground room checks whether it still has a support. */
    virtual void Tick(float DeltaSeconds) override;

private:
    bool IsCellExcavated(int32 X, int32 Y, int32 Layer) const;
    bool HasSupportNearby() const;
    void Collect();

    UPROPERTY(SaveGame, VisibleAnywhere, Category="Earthwork") bool bValid = false;
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Earthwork") bool bCollapsed = false;
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Earthwork") FGuid StorageId;
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Earthwork") double LastRainfallIndex = -1.0;
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Earthwork") float StoredWater = 0.0f;
};
