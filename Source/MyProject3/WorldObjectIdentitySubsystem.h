#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldObjectIdentitySubsystem.generated.h"

class UWorldObjectIdentityComponent;

/**
 * Tracks identity-component IDs within one UWorld. It only arbitrates identity
 * uniqueness; it does not own gameplay state or persist a world snapshot.
 */
UCLASS()
class MYPROJECT3_API UWorldObjectIdentitySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    bool RegisterIdentity(UWorldObjectIdentityComponent* Component);
    void UnregisterIdentity(UWorldObjectIdentityComponent* Component);

private:
    TMap<FGuid, TWeakObjectPtr<UWorldObjectIdentityComponent>> RegisteredIdentities;
};
