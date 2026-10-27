#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WildlifeActor.generated.h"

class UPhysicalHealthComponent;
class UStaticMeshComponent;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
    EWildlifeKind Kind = EWildlifeKind::SmallAnimal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
    float MoveSpeed = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
    float WanderRadius = 600.0f;

    /** Sets shape, size, colour and health for the kind. Call before or after spawning. */
    void ConfigureKind(EWildlifeKind NewKind);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UFUNCTION()
    void HandleDied(AActor* Causer);

    FVector Home = FVector::ZeroVector;
    FVector WanderTarget = FVector::ZeroVector;
    float TimeToNewTarget = 0.0f;
};
