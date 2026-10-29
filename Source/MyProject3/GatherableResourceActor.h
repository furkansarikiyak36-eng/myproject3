#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatherableResourceActor.generated.h"

class UInventoryCraftingComponent;
class UStaticMeshComponent;

/**
 * A place in the world that gives a resource (P4.1): a flint outcrop, a reed bed,
 * a clay bank, a berry bush, resin on a notched pine. It holds a few units, refills
 * one unit at a time on the world clock, and may need a tool in the gatherer's hands.
 * Consumables come out as ledger-backed unique items so they can be eaten;
 * everything else as plain stacks (the same as hand crafting).
 */
UCLASS()
class MYPROJECT3_API AGatherableResourceActor : public AActor
{
    GENERATED_BODY()

public:
    AGatherableResourceActor();

    /** Resolved from zones.json after GameData is applied. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gather|Region") FName ZoneId = NAME_None;

    /** Item id from Config/GameData/items.json. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gather") FName ItemDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gather", meta=(ClampMin="1")) int32 UnitsPerGather = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gather", meta=(ClampMin="1")) int32 MaxUnits = 5;
    /** None = bare hands. The tool stays in the inventory. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gather") FName RequiredToolId = NAME_None;
    /** Simulation seconds until one unit grows back; 0 never refills (a one-time find). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gather", meta=(ClampMin="0")) double RefillSecondsPerUnit = 86400.0;
    /** Gives ledger items (needed for eating) instead of plain stacks. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gather") bool bGiveLedgerItems = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gather") UStaticMeshComponent* Mesh = nullptr;

    /** Units here now, after refilling up to the current world time. */
    UFUNCTION(BlueprintCallable, Category="Gather") int32 GetAvailableUnits();
    /** Moves up to UnitsPerGather units into the inventory. False if empty, no tool or no room. */
    UFUNCTION(BlueprintCallable, Category="Gather") bool TryGather(UInventoryCraftingComponent* Inventory, int32& OutGathered);

protected:
    virtual void BeginPlay() override;

private:
    double Now() const;
    void Refill();

    UPROPERTY(SaveGame, VisibleAnywhere, Category="Gather") int32 AvailableUnits = -1;
    UPROPERTY(SaveGame, VisibleAnywhere, Category="Gather") double RefillClockSeconds = 0.0;
};
