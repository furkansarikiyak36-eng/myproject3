#include "WorldObjectIdentityComponent.h"

#include "Engine/World.h"
#include "WorldObjectIdentitySubsystem.h"

UWorldObjectIdentityComponent::UWorldObjectIdentityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWorldObjectIdentityComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!EnsureIdentity())
    {
        UE_LOG(LogTemp, Error,
            TEXT("WorldObjectIdentityComponent on '%s' could not create an instance ID."),
            *GetNameSafe(GetOwner()));
        return;
    }

    UWorld* World = GetWorld();
    UWorldObjectIdentitySubsystem* IdentitySubsystem =
        World ? World->GetSubsystem<UWorldObjectIdentitySubsystem>() : nullptr;

    if (!IdentitySubsystem || !IdentitySubsystem->RegisterIdentity(this))
    {
        UE_LOG(LogTemp, Error,
            TEXT("WorldObjectIdentityComponent on '%s' could not register its instance ID."),
            *GetNameSafe(GetOwner()));
    }
}

void UWorldObjectIdentityComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UWorldObjectIdentitySubsystem* IdentitySubsystem =
            World->GetSubsystem<UWorldObjectIdentitySubsystem>())
        {
            IdentitySubsystem->UnregisterIdentity(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

bool UWorldObjectIdentityComponent::EnsureIdentity()
{
    if (InstanceId.IsValid())
    {
        return true;
    }

    InstanceId = FGuid::NewGuid();
    return InstanceId.IsValid();
}

void UWorldObjectIdentityComponent::SetDefinitionId(FName NewDefinitionId)
{
    DefinitionId = NewDefinitionId;
}

bool UWorldObjectIdentityComponent::TryTransitionLifecycle(
    EWorldObjectLifecycleState ExpectedState,
    EWorldObjectLifecycleState NewState)
{
    if (LifecycleState != ExpectedState ||
        !IsLifecycleTransitionAllowed(LifecycleState, NewState))
    {
        return false;
    }

    LifecycleState = NewState;
    ++StateRevision;
    return true;
}

bool UWorldObjectIdentityComponent::IsLifecycleTransitionAllowed(
    EWorldObjectLifecycleState From,
    EWorldObjectLifecycleState To) const
{
    if (From == To ||
        From == EWorldObjectLifecycleState::Consumed ||
        From == EWorldObjectLifecycleState::Destroyed)
    {
        return false;
    }

    switch (From)
    {
    case EWorldObjectLifecycleState::World:
        return To == EWorldObjectLifecycleState::Held ||
            To == EWorldObjectLifecycleState::Placed ||
            To == EWorldObjectLifecycleState::Consumed ||
            To == EWorldObjectLifecycleState::Destroyed;

    case EWorldObjectLifecycleState::Held:
        return To == EWorldObjectLifecycleState::World ||
            To == EWorldObjectLifecycleState::Placed ||
            To == EWorldObjectLifecycleState::Consumed ||
            To == EWorldObjectLifecycleState::Destroyed;

    case EWorldObjectLifecycleState::Placed:
        return To == EWorldObjectLifecycleState::World ||
            To == EWorldObjectLifecycleState::Held ||
            To == EWorldObjectLifecycleState::Destroyed;

    case EWorldObjectLifecycleState::Consumed:
    case EWorldObjectLifecycleState::Destroyed:
    default:
        return false;
    }
}
