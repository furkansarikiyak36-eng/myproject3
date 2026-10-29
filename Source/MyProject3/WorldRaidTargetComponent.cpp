#include "WorldRaidTargetComponent.h"

#include "GameFramework/Actor.h"

FVector UWorldRaidTargetComponent::GetRaidTargetLocation() const
{
    const AActor* Owner = GetOwner();
    return Owner ? Owner->GetActorTransform().TransformPosition(TargetOffset) : FVector::ZeroVector;
}
