#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LogBuildingTypes.h"
#include "HarvestableTreeActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UWorldObjectIdentityComponent;
class ALogBuildPiece;

/** A size-banded yield contract. Height is measured from the tree mesh's world bounds. */
USTRUCT(BlueprintType)
struct FHarvestTreeYieldRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Yield", meta=(ClampMin="0.0"))
    float MinWorldHeight = 0.0f;

    /** Zero means no upper bound; otherwise the bound is exclusive. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Yield", meta=(ClampMin="0.0"))
    float MaxWorldHeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Yield", meta=(ClampMin="1"))
    int32 LogCount = 3;

    /** The selected log Blueprint must have the matching length/shape mesh variant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Yield")
    ELogPieceType OutputPieceType = ELogPieceType::WholeLog;
};

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

    /**
     * Optional runtime/editor-authored mesh family supplied to spawned logs.
     * Empty preserves the existing Blueprint-configured output behaviour.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Harvest|Meshes")
    TArray<FLogProcessingMeshSet> OutputProcessingMeshSets;
    
    /** If configured, exactly one height band must match; no filename-based size guesses. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree|Yield")
    TArray<FHarvestTreeYieldRule> YieldRules;

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
    UPROPERTY(SaveGame)
    float StableGroundedSeconds = 0.0f;

    bool HasGroundContact() const;
    bool SpawnHarvestLogs();
};
