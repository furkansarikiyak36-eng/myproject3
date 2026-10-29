#include "WorldInfrastructureRouteLinkComponent.h"

#include "Engine/World.h"
#include "WorldLogisticsRouteSubsystem.h"

UWorldInfrastructureRouteLinkComponent::UWorldInfrastructureRouteLinkComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWorldInfrastructureRouteLinkComponent::BeginPlay()
{
    Super::BeginPlay();

    // Only an explicitly preconfigured usable structure writes on begin play.
    // False is not pushed here: a loaded route snapshot may already hold the
    // authoritative runtime state for this route.
    if (bInfrastructureUsable && !SetInfrastructureUsable(true))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Infrastructure route link %s could not apply its initial state."),
            *GetNameSafe(GetOwner()));
    }
}

void UWorldInfrastructureRouteLinkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // The root route-level actor disappearing means its route contribution is gone.
    // The subsystem fails closed if no matching authored route edge exists.
    if (!InfrastructureId.IsNone())
    {
        if (UWorld* World = GetWorld())
        {
            if (UWorldLogisticsRouteSubsystem* Routes =
                World->GetSubsystem<UWorldLogisticsRouteSubsystem>())
            {
                Routes->SetInfrastructureBuilt(InfrastructureId, false);
            }
        }
    }
    bInfrastructureUsable = false;
    Super::EndPlay(EndPlayReason);
}

bool UWorldInfrastructureRouteLinkComponent::SetInfrastructureUsable(bool bUsable)
{
    if (InfrastructureId.IsNone())
        return false;

    UWorld* World = GetWorld();
    UWorldLogisticsRouteSubsystem* Routes =
        World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    if (!Routes || !Routes->SetInfrastructureBuilt(InfrastructureId, bUsable))
        return false;

    bInfrastructureUsable = bUsable;
    return true;
}


bool UWorldInfrastructureRouteLinkComponent::HandleStructuralDamageChanged(
    float Damage, float DamageThreshold)
{
    if (!bStructuralDamageControlsUsability ||
        !FMath::IsFinite(Damage) || Damage < 0.0f ||
        !FMath::IsFinite(DamageThreshold) || DamageThreshold <= 0.0f)
        return false;

    return SetInfrastructureUsable(Damage < DamageThreshold);
}
