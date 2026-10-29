#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RaidPlaceholderPawn.generated.h"

/**
 * Lightweight primitive-only raid pawn with NavMesh waypoint movement.
 * Fixed-step raid damage remains owned by UWorldRaidAdapterSubsystem; movement
 * is a visual/approach adapter and never commits structural damage itself.
 */
UCLASS()
class MYPROJECT3_API ARaidPlaceholderPawn : public APawn
{
    GENERATED_BODY()

public:
    ARaidPlaceholderPawn();

    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category="Raid|Movement")
    void SetRaidTargetLocation(FVector InTargetLocation);

    UFUNCTION(BlueprintPure, Category="Raid|Movement")
    bool HasRaidTarget() const { return bHasRaidTarget; }

    UFUNCTION(BlueprintPure, Category="Raid|Movement")
    bool HasReachedRaidTarget() const
    {
        return bHasRaidTarget && !RaidTargetLocation.ContainsNaN() &&
            !GetActorLocation().ContainsNaN() &&
            FMath::IsFinite(TargetAcceptanceRadiusCm) && TargetAcceptanceRadiusCm > 0.0f &&
            FVector::DistSquared(GetActorLocation(), RaidTargetLocation) <=
                FMath::Square(TargetAcceptanceRadiusCm);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Movement", meta=(ClampMin="1.0"))
    float MoveSpeedCmPerSecond = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Movement", meta=(ClampMin="0.1", ClampMax="5.0"))
    float PathRefreshSeconds = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Movement", meta=(ClampMin="10.0"))
    float TargetAcceptanceRadiusCm = 100.0f;

private:
    UPROPERTY(Transient)
    FVector RaidTargetLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    bool bHasRaidTarget = false;

    UPROPERTY(Transient)
    float TimeUntilPathRefresh = 0.0f;

    TArray<FVector> CachedPathPoints;
    int32 CurrentPathPointIndex = 0;

    bool RebuildNavigationPath();
};
