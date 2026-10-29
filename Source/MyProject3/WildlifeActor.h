#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WildlifeActor.generated.h"

class UPhysicalHealthComponent;
class UStaticMeshComponent;
class UWorldObjectIdentityComponent;

UENUM(BlueprintType)
enum class EWildlifeKind : uint8
{
    SmallAnimal UMETA(DisplayName = "Small Animal"),
    Enemy UMETA(DisplayName = "Enemy"),
    Plant UMETA(DisplayName = "Plant / Bush")
};

/** Simple living target: a wandering small animal, an approaching enemy or a bush. */
UCLASS()
class MYPROJECT3_API AWildlifeActor : public AActor
{
    GENERATED_BODY()

public:
    AWildlifeActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wildlife")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wildlife")
    TObjectPtr<UPhysicalHealthComponent> Health;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wildlife")
    TObjectPtr<UWorldObjectIdentityComponent> Identity;

    /** Domain actor remains visible in-world while its authoritative animal record tracks capture and pen state. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Wildlife|Animal")
    bool bCapturedForAnimalManagement = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Wildlife|Animal")
    FGuid CapturedAnimalId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Wildlife|Animal")
    FGuid AnimalPenId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
    EWildlifeKind Kind = EWildlifeKind::SmallAnimal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
    float MoveSpeed = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
    float WanderRadius = 600.0f;

    /** Sets shape, size, colour and health for the kind. Call before or after spawning. */
    void ConfigureKind(EWildlifeKind NewKind);

    /** Called by the agriculture domain after a capture/pen transaction has been preflighted. */
    bool ApplyAnimalManagementState(const FGuid& AnimalId, const FGuid& PenId,
        const FVector& TargetLocation);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UFUNCTION()
    void HandleDied(AActor* Causer);

    FVector Home = FVector::ZeroVector;
    FVector WanderTarget = FVector::ZeroVector;
    float TimeToNewTarget = 0.0f;
};
