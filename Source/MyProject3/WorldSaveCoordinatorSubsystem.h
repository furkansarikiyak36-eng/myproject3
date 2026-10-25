#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldSaveCoordinatorSubsystem.generated.h"

class UWorldSimulationSaveGame;

UCLASS()
class MYPROJECT3_API UWorldSaveCoordinatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="World|Save")
    bool SaveWorldState(const FString& SlotName, int32 UserIndex = 0);

    UFUNCTION(BlueprintCallable, Category="World|Save")
    bool LoadWorldState(const FString& SlotName, int32 UserIndex = 0);

private:
    bool CaptureCurrentState(UWorldSimulationSaveGame* OutSave) const;
    bool ApplySnapshot(const UWorldSimulationSaveGame* Snapshot);
};
