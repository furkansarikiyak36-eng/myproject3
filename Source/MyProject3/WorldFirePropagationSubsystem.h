#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldFirePropagationSubsystem.generated.h"

/**
 * Deterministic, fixed-step wildfire propagation over authored combustible
 * surfaces. Event chance is evaluated per region and per logical tick; component
 * fuel/moisture/ignition state is SaveGame-backed.
 */
UCLASS()
class MYPROJECT3_API UWorldFirePropagationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World|Fire|Limits", meta=(ClampMin="1", ClampMax="4096"))
    int32 MaxCombustibleComponents = 2048;

    /** Evaluate active regional events, spread ignition, and burn fuel for one committed tick. */
    UFUNCTION(BlueprintCallable, Category="World|Fire")
    bool AdvanceRegionalFireSpread(double DeltaSeconds);

private:
    static float StableRoll01(int32 RandomSeed, int64 TickIndex,
        FName RegionId, const FGuid& SourceId, const FGuid& TargetId, const TCHAR* Purpose);
    static FGuid MakeOperationId(int32 RandomSeed, int64 TickIndex,
        FName RegionId, const FGuid& SourceId, const FGuid& TargetId, const TCHAR* Purpose);
    bool IsFirebreakBetween(const FVector& Source, const FVector& Target,
        FName RegionId, const TArray<class UWorldCombustibleComponent*>& Components) const;
};
