#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldStorageComponent.generated.h"

/** Attach to a chest, rack, stockpile or production buffer to register finite storage. */
UCLASS(ClassGroup=(Gameplay), BlueprintType, Blueprintable,
    meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UWorldStorageComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWorldStorageComponent();

    virtual void OnComponentCreated() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Storage",
        meta=(ClampMin="1", ClampMax="100000"))
    int32 CapacityUnits = 64;

    /** Persisted on placed actors so a custom world save can match the same container. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category="World|Storage")
    FGuid StorageId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World|Storage")
    bool bIsRegistered = false;

    UFUNCTION(BlueprintPure, Category="World|Storage")
    FGuid GetStorageId() const { return StorageId; }

    UFUNCTION(BlueprintPure, Category="World|Storage")
    bool CanAcceptItem(FGuid ItemId) const;

    UFUNCTION(BlueprintCallable, Category="World|Storage")
    bool GetStoredItemIds(TArray<FGuid>& OutItemIds) const;
};
