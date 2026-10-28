#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldPersistenceTypes.h"
#include "WorldPersistenceSubsystem.generated.h"

class UWorldSaveGame;

UCLASS()
class MYPROJECT3_API UWorldPersistenceSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Persistence")
    EWorldPersistenceResult BeginSnapshot(int64 WorldSeed, double WorldTimeSeconds, FGuid& OutSaveId);

    UFUNCTION(BlueprintCallable, Category="Persistence")
    EWorldPersistenceResult PutSection(FName SectionId, int32 SchemaVersion, const TArray<uint8>& Payload);

    UFUNCTION(BlueprintCallable, Category="Persistence")
    EWorldPersistenceResult ValidateSnapshot(bool bRequireCoreSection) const;

    UFUNCTION(BlueprintCallable, Category="Persistence")
    EWorldPersistenceResult SaveSnapshotToSlot(const FString& SlotName, int32 UserIndex);

    UFUNCTION(BlueprintCallable, Category="Persistence")
    EWorldPersistenceResult LoadSnapshotFromSlot(const FString& SlotName, int32 UserIndex, bool bRequireCoreSection);

    UFUNCTION(BlueprintPure, Category="Persistence")
    FWorldSaveManifest GetCurrentManifest() const { return CurrentManifest; }

private:
    UPROPERTY(Transient)
    FWorldSaveManifest CurrentManifest;

    UPROPERTY(Transient)
    bool bHasSnapshot = false;

    static constexpr int32 CurrentFormatVersion = 1;
    static constexpr int32 MaxSectionPayloadBytes = 16 * 1024 * 1024;

    EWorldPersistenceResult ValidateManifest(const FWorldSaveManifest& Manifest, bool bRequireCoreSection) const;
    static uint32 CalculatePayloadCrc(const TArray<uint8>& Payload);
};
