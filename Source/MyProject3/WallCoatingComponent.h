#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallCoatingComponent.generated.h"

class UInventoryCraftingComponent;

/** Idempotent application receipt kept with the physical wall/piece. */
USTRUCT(BlueprintType)
struct FWallCoatingApplicationReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster") FGuid OperationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster") FName ItemDefinitionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster") int32 UnitsConsumed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster") float ResultingCoating = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster") int64 ResultingRevision = 0;
};

/**
 * Persistent clay-plaster state on a placed structural piece.
 * Four plaster units fully seal a piece; the shelter sensor uses the seal as a
 * bounded gap-cover contribution when a ray passes close to the coated piece.
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UWallCoatingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWallCoatingComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Plaster", meta=(ClampMin="0.01", ClampMax="1"))
    float CoatingPerUnit = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Plaster", meta=(ClampMin="0", ClampMax="1"))
    float MinimumSealForShelter = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Plaster")
    FName PlasterItemDefinitionId = TEXT("Plaster");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster")
    float CoatingLevel = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster")
    int64 Revision = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Plaster|Limits", meta=(ClampMin="1", ClampMax="128"))
    int32 MaxReceipts = 32;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Structure|Plaster|Receipts")
    TArray<FWallCoatingApplicationReceipt> Receipts;

    UFUNCTION(BlueprintPure, Category="Structure|Plaster")
    bool ProvidesShelterSeal() const;

    UFUNCTION(BlueprintCallable, Category="Structure|Plaster")
    bool TryApplyPlaster(const FGuid& OperationId, UInventoryCraftingComponent* Inventory,
        int32 Units, int64 ExpectedRevision, float& OutCoatingLevel);

private:
    bool IsValidState() const;
};
