#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayCoreTypes.h"
#include "WorldObjectIdentityComponent.generated.h"

/**
 * Provides identity and generic lifecycle metadata for an actor.
 *
 * This component is not a second world-state store. Domain-specific state
 * remains on the existing authoritative domain owner.
 */
UCLASS(ClassGroup = (Gameplay), BlueprintType, Blueprintable,
    meta = (BlueprintSpawnableComponent))
class MYPROJECT3_API UWorldObjectIdentityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWorldObjectIdentityComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Stable for this actor instance and serialized with the owning actor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Gameplay|Identity")
    FGuid InstanceId;

    /** Stable definition key, not a display name. Assigned by authored data. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Gameplay|Identity")
    FName DefinitionId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Gameplay|Lifecycle")
    EWorldObjectLifecycleState LifecycleState = EWorldObjectLifecycleState::World;

    /** Increments only after an accepted lifecycle transition. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Gameplay|Lifecycle")
    int64 StateRevision = 0;

    UFUNCTION(BlueprintCallable, Category = "Gameplay|Identity")
    bool EnsureIdentity();

    UFUNCTION(BlueprintPure, Category = "Gameplay|Identity")
    bool HasValidIdentity() const { return InstanceId.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Gameplay|Identity")
    FGuid GetInstanceId() const { return InstanceId; }

    UFUNCTION(BlueprintPure, Category = "Gameplay|Identity")
    FName GetDefinitionId() const { return DefinitionId; }

    UFUNCTION(BlueprintCallable, Category = "Gameplay|Identity")
    void SetDefinitionId(FName NewDefinitionId);

    UFUNCTION(BlueprintPure, Category = "Gameplay|Lifecycle")
    EWorldObjectLifecycleState GetLifecycleState() const { return LifecycleState; }

    /** Optional expected-state check prevents stale callers overwriting changes. */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Lifecycle")
    bool TryTransitionLifecycle(
        EWorldObjectLifecycleState ExpectedState,
        EWorldObjectLifecycleState NewState);

private:
    bool IsLifecycleTransitionAllowed(
        EWorldObjectLifecycleState From,
        EWorldObjectLifecycleState To) const;
};
