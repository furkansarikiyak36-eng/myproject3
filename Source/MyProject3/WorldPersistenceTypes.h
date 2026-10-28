#pragma once
#include "CoreMinimal.h"
#include "WorldPersistenceTypes.generated.h"

USTRUCT(BlueprintType)
struct FWorldSaveSection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence")
    FName SectionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence")
    TArray<uint8> Payload;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Persistence")
    uint32 PayloadCrc32 = 0;
};

USTRUCT(BlueprintType)
struct FWorldSaveManifest
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Persistence")
    int32 FormatVersion = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Persistence")
    FGuid SaveId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence")
    int64 WorldSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence")
    double WorldTimeSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Persistence")
    TArray<FWorldSaveSection> Sections;
};

UENUM(BlueprintType)
enum class EWorldPersistenceResult : uint8
{
    Success,
    InvalidArgument,
    UnsupportedFormat,
    DuplicateSection,
    InvalidSection,
    PayloadTooLarge,
    ChecksumMismatch,
    MissingRequiredSection,
    SaveSlotFailure,
    LoadSlotFailure
};
