#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarvestableTreeActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UWorldObjectIdentityComponent;
class ALogBuildPiece;

/**
 * Minimal physical tree lifecycle adapter.
 * It owns tree health/fall/grounded state only; spawned logs register through
 * ALogBuildPiece and the existing WorldItemOwnershipSubsystem.
 */
UCLASS(Blueprintable)
class MYPROJECT3_API AHarvestableTreeActor : public AActor
{
    GENERATED_BODY()

public:
    AHarvestableTreeActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tree")
    TObjectPtr<UStaticMeshComponent> TreeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tree|Identity")
    TObjectPtr<UWorldObjectIdentityComponent> Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest", meta=(ClampMin="1.0"))
    float MaxChopHealth = 5.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Tree|Harvest")
    float ChopHealth = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest", meta=(ClampMin="0.0"))
    float FallImpulse = 250000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest", meta=(ClampMin="1"))
    int32 LogsToSpawn = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest")
    TSubclassOf<ALogBuildPiece> LogActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest")
    FName LogMeshVariantSetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest", meta=(ClampMin="0.1"))
    float GroundedSpeedThreshold = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest", meta=(ClampMin="0.1"))
    float GroundedHoldSeconds = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Tree|Harvest")
    bool bFellingStarted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Tree|Harvest")
    bool bGrounded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Tree|Harvest")
    bool bLogsSpawned = false;

    UFUNCTION(BlueprintCallable, Category="Tree|Harvest")
    bool ApplyChopDamage(float Damage, FVector ChopDirection);

    UFUNCTION(BlueprintPure, Category="Tree|Harvest")
    bool CanBeChopped() const { return !bFellingStarted && !bGrounded && !bLogsSpawned; }

private:
    float StableGroundedSeconds = 0.0f;

    bool HasGroundContact() const;
    bool SpawnHarvestLogs();
};
