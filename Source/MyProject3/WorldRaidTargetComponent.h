#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldRaidTargetComponent.generated.h"

/**
 * Authored raid objective marker for a settlement/core/defense actor.
 * The adapter prefers these markers over guessing a target from arbitrary
 * structural pieces; the structural graph still supplies the damageable node.
 */
UCLASS(ClassGroup=(Gameplay), BlueprintType, Blueprintable,
    meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UWorldRaidTargetComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Target")
    bool bRaidTargetEnabled = true;

    /** Higher-priority authored settlement objectives win before distance is considered. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Target")
    int32 TargetPriority = 0;

    /** Local-space offset from the component transform to the settlement's raid objective. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Target")
    FVector TargetOffset = FVector::ZeroVector;

    UFUNCTION(BlueprintPure, Category="Raid|Target")
    FVector GetRaidTargetLocation() const;
};
