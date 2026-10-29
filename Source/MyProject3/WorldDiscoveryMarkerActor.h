#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldDiscoveryMarkerActor.generated.h"

class UStaticMeshComponent;

/**
 * Cheap primitive discovery marker. The world survival ledger owns permanence;
 * this actor only presents a location and commits one configured discovery when
 * the player reaches it. Rewards may be ordinary stacks or unique ledger-owned items
 * (plantable seeds use the latter so the crop transaction can consume an exact item).
 */
UCLASS()
class MYPROJECT3_API AWorldDiscoveryMarkerActor : public AActor
{
    GENERATED_BODY()

public:
    AWorldDiscoveryMarkerActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Discovery")
    TObjectPtr<UStaticMeshComponent> MarkerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Discovery")
    FName DiscoveryId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Discovery")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Discovery", meta=(ClampMin="50.0", ClampMax="2500.0"))
    float RevealRadiusCm = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Discovery")
    FName RewardItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Discovery", meta=(ClampMin="0", ClampMax="64"))
    int32 RewardQuantity = 0;

    /** When true, each reward unit receives a distinct ownership-ledger identity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Discovery")
    bool bRewardUniqueItems = false;

private:
    float TimeUntilScan = 0.0f;
    bool bResolved = false;
    bool bRangeMessageShown = false;

    bool IsAlreadyDiscovered() const;
    bool TryCommitDiscovery();
    void HideAsResolved();
};
