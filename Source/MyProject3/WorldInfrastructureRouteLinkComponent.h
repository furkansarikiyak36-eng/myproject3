#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldInfrastructureRouteLinkComponent.generated.h"

/**
 * Explicit adapter between one completed infrastructure actor and the logical route graph.
 * Attach it to the root actor that represents the usable route-level structure (not each plank).
 * Build/repair/damage code calls SetInfrastructureUsable when the whole crossing changes state.
 */
UCLASS(ClassGroup=(World), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UWorldInfrastructureRouteLinkComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWorldInfrastructureRouteLinkComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Must match the InfrastructureId on one or more directed route edges. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Infrastructure")
    FName InfrastructureId = NAME_None;

    /** Set true only when the route-level structure is complete and usable. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Infrastructure")
    bool bInfrastructureUsable = false;

    /** If this is attached to a structural graph node, threshold damage disables the route and repair re-enables it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="World|Infrastructure")
    bool bStructuralDamageControlsUsability = true;

    /** Updates all directional route edges sharing InfrastructureId. */
    UFUNCTION(BlueprintCallable, Category="World|Infrastructure")
    bool SetInfrastructureUsable(bool bUsable);

    /** Called by WorldStructuralGraphSubsystem after damage or repair changes this actor's node. */
    bool HandleStructuralDamageChanged(float Damage, float DamageThreshold);

    UFUNCTION(BlueprintPure, Category="World|Infrastructure")
    bool IsInfrastructureUsable() const { return bInfrastructureUsable; }
};
