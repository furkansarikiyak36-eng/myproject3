#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LogAttachmentComponents.generated.h"

class AHarvestableTreeActor;
class UStaticMeshComponent;

/**
 * The top of a standing log cut to a point. Anyone trying to climb over it is
 * hurt and pushed back, which is what makes a row of stakes a defensive wall.
 */
UCLASS(ClassGroup = (Gameplay))
class MYPROJECT3_API USharpenedTipComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USharpenedTipComponent();

    UPROPERTY(EditAnywhere, Category = "Stake")
    float ClimbDamage = 12.0f;

    UPROPERTY(EditAnywhere, Category = "Stake")
    float RepelSpeed = 450.0f;

    /** Shapes the point on the upper end of the owning log. */
    bool InitializeTip();

    FVector GetTipLocation() const { return TipLocation; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> TipMesh;

    FVector TipLocation = FVector::ZeroVector;
    float TipRadius = 15.0f;
    float Cooldown = 0.0f;
};

/**
 * A log resting against a standing tree uses the trunk as its support; when
 * the tree is felled the log loses that support and falls.
 */
UCLASS(ClassGroup = (Gameplay))
class MYPROJECT3_API UTreeAnchorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTreeAnchorComponent();

    void SetTree(AHarvestableTreeActor* InTree) { Tree = InTree; }
    AHarvestableTreeActor* GetTree() const { return Tree.Get(); }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    TWeakObjectPtr<AHarvestableTreeActor> Tree;
    float TimeToCheck = 0.0f;
};
