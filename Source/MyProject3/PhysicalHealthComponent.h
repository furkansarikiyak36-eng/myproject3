#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicalHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPhysicalDamageEvent, float, Amount, AActor*, Causer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhysicalDeathEvent, AActor*, Causer);

/** Health that physical hits (thrown logs, sharpened stakes) can reduce; also staggers. */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class MYPROJECT3_API UPhysicalHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPhysicalHealthComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Health")
    float Health = 100.0f;

    /** How long a hit leaves the owner unable to act. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "0.0"))
    float StaggerSeconds = 0.8f;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FPhysicalDamageEvent OnDamaged;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FPhysicalDeathEvent OnDied;

    /** Reduces health and pushes the owner by Impulse (a velocity change in cm/s). Returns damage dealt. */
    UFUNCTION(BlueprintCallable, Category = "Health")
    float ApplyPhysicalDamage(float Amount, FVector Impulse, AActor* Causer);

    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsDead() const { return bDead; }

    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsStaggered() const;

private:
    bool bDead = false;
    double StaggeredUntil = 0.0;
};
