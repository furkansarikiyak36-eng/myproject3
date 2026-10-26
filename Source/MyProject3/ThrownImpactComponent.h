#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThrownImpactComponent.generated.h"

class UPrimitiveComponent;

/**
 * Added to a thrown piece while it flies. A fast hit deals damage from the
 * piece's kinetic energy to animals, enemies, plants, structures and players.
 */
UCLASS(ClassGroup = (Gameplay))
class MYPROJECT3_API UThrownImpactComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrownImpactComponent();

    /** Below this speed (cm/s) a contact is just a bump. */
    UPROPERTY(EditAnywhere, Category = "Impact")
    float MinImpactSpeed = 350.0f;

    /** Damage per joule of kinetic energy. */
    UPROPERTY(EditAnywhere, Category = "Impact")
    float DamagePerJoule = 0.02f;

    UPROPERTY(EditAnywhere, Category = "Impact")
    float MaxLifetime = 6.0f;

    void Arm(AActor* InThrower, float InMassKg);

    /** Applies one impact at Speed (cm/s) to Other; returns the damage dealt. */
    float ApplyImpactTo(AActor* Other, float Speed, const FVector& Direction);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    UFUNCTION()
    void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        FVector NormalImpulse, const FHitResult& Hit);

    TWeakObjectPtr<AActor> Thrower;
    TSet<TWeakObjectPtr<AActor>> AlreadyHit;
    float MassKg = 10.0f;
    float Elapsed = 0.0f;
    FVector LastVelocity = FVector::ZeroVector;
};
