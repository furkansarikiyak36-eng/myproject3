#include "WorldObjectIdentitySubsystem.h"

#include "WorldObjectIdentityComponent.h"

bool UWorldObjectIdentitySubsystem::RegisterIdentity(
    UWorldObjectIdentityComponent* Component)
{
    if (!IsValid(Component))
    {
        return false;
    }

    FGuid CandidateId = Component->InstanceId;
    if (!CandidateId.IsValid())
    {
        CandidateId = FGuid::NewGuid();
    }

    if (const TWeakObjectPtr<UWorldObjectIdentityComponent>* Existing =
        RegisteredIdentities.Find(CandidateId))
    {
        UWorldObjectIdentityComponent* ExistingComponent = Existing->Get();
        if (IsValid(ExistingComponent) && ExistingComponent != Component)
        {
            // Duplicated placed actors can copy serialized GUID properties.
            // Resolve the collision before exposing this instance to gameplay.
            do
            {
                CandidateId = FGuid::NewGuid();
            }
            while (!CandidateId.IsValid() || RegisteredIdentities.Contains(CandidateId));
        }
    }

    Component->InstanceId = CandidateId;
    RegisteredIdentities.Add(CandidateId, Component);
    return true;
}

void UWorldObjectIdentitySubsystem::UnregisterIdentity(
    UWorldObjectIdentityComponent* Component)
{
    if (!IsValid(Component) || !Component->InstanceId.IsValid())
    {
        return;
    }

    const TWeakObjectPtr<UWorldObjectIdentityComponent>* Existing =
        RegisteredIdentities.Find(Component->InstanceId);
    if (Existing && Existing->Get() == Component)
    {
        RegisteredIdentities.Remove(Component->InstanceId);
    }
}
