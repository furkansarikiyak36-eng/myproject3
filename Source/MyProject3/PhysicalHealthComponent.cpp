#include "PhysicalHealthComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UPhysicalHealthComponent::UPhysicalHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UPhysicalHealthComponent::IsStaggered() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() < StaggeredUntil;
}

float UPhysicalHealthComponent::ApplyPhysicalDamage(float Amount, FVector Impulse, AActor* Causer)
{
    if (bDead || !FMath::IsFinite(Amount) || Amount <= 0.0f || !GetOwner() ||
        !FMath::IsFinite(Impulse.X) || !FMath::IsFinite(Impulse.Y) || !FMath::IsFinite(Impulse.Z) ||
        !FMath::IsFinite(Health) || Health <= 0.0f || !FMath::IsFinite(StaggerSeconds) || StaggerSeconds < 0.0f)
        return 0.0f;
    const float Dealt = FMath::Clamp(FMath::Min(Amount, Health), 0.0f, Health);
    Health -= Dealt;
    if (GetWorld())
        StaggeredUntil = GetWorld()->GetTimeSeconds() + StaggerSeconds;

    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        Character->LaunchCharacter(Impulse, false, false);
    }
    else if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
    {
        if (Root->IsSimulatingPhysics())
            Root->AddImpulse(Impulse, NAME_None, true);
    }

    OnDamaged.Broadcast(Dealt, Causer);
    if (Health <= 0.0f)
    {
        bDead = true;
        OnDied.Broadcast(Causer);
    }
    return Dealt;
}
