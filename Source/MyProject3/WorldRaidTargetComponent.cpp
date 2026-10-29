#include "WorldRaidTargetComponent.h"

#include "GameFramework/Actor.h"

FVector UWorldRaidTargetComponent::GetRaidTargetLocation() const
{
    const FTransform Transform = GetComponentTransform();
    return Transform.TransformPosition(TargetOffset);
}
