#include "WorldObjectIdentityComponent.h"

UWorldObjectIdentityComponent::UWorldObjectIdentityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWorldObjectIdentityComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureIdentity();
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
