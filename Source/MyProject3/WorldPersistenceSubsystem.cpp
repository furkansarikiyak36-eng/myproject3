#include "WorldPersistenceSubsystem.h"
#include "WorldSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

uint32 UWorldPersistenceSubsystem::CalculatePayloadCrc(const TArray<uint8>& Payload)
{
    return Payload.IsEmpty() ? 0u : FCrc::MemCrc32(Payload.GetData(), Payload.Num());
}

EWorldPersistenceResult UWorldPersistenceSubsystem::BeginSnapshot(int64 WorldSeed, double WorldTimeSeconds, FGuid& OutSaveId)
{
    if (!FMath::IsFinite(WorldTimeSeconds) || WorldTimeSeconds < 0.0)
    {
        return EWorldPersistenceResult::InvalidArgument;
    }

    FWorldSaveManifest NewManifest;
    NewManifest.FormatVersion = CurrentFormatVersion;
    NewManifest.SaveId = FGuid::NewGuid();
    NewManifest.WorldSeed = WorldSeed;
    NewManifest.WorldTimeSeconds = WorldTimeSeconds;
    CurrentManifest = MoveTemp(NewManifest);
    bHasSnapshot = true;
    OutSaveId = CurrentManifest.SaveId;
    return EWorldPersistenceResult::Success;
}

EWorldPersistenceResult UWorldPersistenceSubsystem::PutSection(FName SectionId, int32 SchemaVersion, const TArray<uint8>& Payload)
{
    if (!bHasSnapshot || SectionId.IsNone() || SchemaVersion < 1)
    {
        return EWorldPersistenceResult::InvalidArgument;
    }
    if (Payload.Num() > MaxSectionPayloadBytes)
    {
        return EWorldPersistenceResult::PayloadTooLarge;
    }
    for (const FWorldSaveSection& Existing : CurrentManifest.Sections)
    {
        if (Existing.SectionId == SectionId)
        {
            return EWorldPersistenceResult::DuplicateSection;
        }
    }

    FWorldSaveSection Section;
    Section.SectionId = SectionId;
    Section.SchemaVersion = SchemaVersion;
    Section.Payload = Payload;
    Section.PayloadCrc32 = CalculatePayloadCrc(Section.Payload);
    CurrentManifest.Sections.Add(MoveTemp(Section));
    return EWorldPersistenceResult::Success;
}

EWorldPersistenceResult UWorldPersistenceSubsystem::ValidateManifest(const FWorldSaveManifest& Manifest, bool bRequireCoreSection) const
{
    if (Manifest.FormatVersion != CurrentFormatVersion || !Manifest.SaveId.IsValid())
    {
        return EWorldPersistenceResult::UnsupportedFormat;
    }
    if (!FMath::IsFinite(Manifest.WorldTimeSeconds) || Manifest.WorldTimeSeconds < 0.0)
    {
        return EWorldPersistenceResult::InvalidArgument;
    }

    bool bHasCoreSection = false;
    TSet<FName> SeenSections;
    for (const FWorldSaveSection& Section : Manifest.Sections)
    {
        if (Section.SectionId.IsNone() || Section.SchemaVersion < 1)
        {
            return EWorldPersistenceResult::InvalidSection;
        }
        if (SeenSections.Contains(Section.SectionId))
        {
            return EWorldPersistenceResult::DuplicateSection;
        }
        SeenSections.Add(Section.SectionId);
        if (Section.Payload.Num() > MaxSectionPayloadBytes)
        {
            return EWorldPersistenceResult::PayloadTooLarge;
        }
        if (CalculatePayloadCrc(Section.Payload) != Section.PayloadCrc32)
        {
            return EWorldPersistenceResult::ChecksumMismatch;
        }
        bHasCoreSection |= Section.SectionId == FName(TEXT("CoreWorld"));
    }
    if (bRequireCoreSection && !bHasCoreSection)
    {
        return EWorldPersistenceResult::MissingRequiredSection;
    }
    return EWorldPersistenceResult::Success;
}

EWorldPersistenceResult UWorldPersistenceSubsystem::ValidateSnapshot(bool bRequireCoreSection) const
{
    if (!bHasSnapshot)
    {
        return EWorldPersistenceResult::InvalidArgument;
    }
    return ValidateManifest(CurrentManifest, bRequireCoreSection);
}

EWorldPersistenceResult UWorldPersistenceSubsystem::SaveSnapshotToSlot(const FString& SlotName, int32 UserIndex)
{
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        return EWorldPersistenceResult::InvalidArgument;
    }
    const EWorldPersistenceResult Validation = ValidateSnapshot(false);
    if (Validation != EWorldPersistenceResult::Success)
    {
        return Validation;
    }

    UWorldSaveGame* SaveObject = Cast<UWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UWorldSaveGame::StaticClass()));
    if (!SaveObject)
    {
        return EWorldPersistenceResult::SaveSlotFailure;
    }
    SaveObject->Manifest = CurrentManifest;
    return UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, UserIndex)
        ? EWorldPersistenceResult::Success
        : EWorldPersistenceResult::SaveSlotFailure;
}

EWorldPersistenceResult UWorldPersistenceSubsystem::LoadSnapshotFromSlot(const FString& SlotName, int32 UserIndex, bool bRequireCoreSection)
{
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        return EWorldPersistenceResult::InvalidArgument;
    }
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
    {
        return EWorldPersistenceResult::LoadSlotFailure;
    }
    UWorldSaveGame* SaveObject = Cast<UWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    if (!SaveObject)
    {
        return EWorldPersistenceResult::LoadSlotFailure;
    }

    const EWorldPersistenceResult Validation = ValidateManifest(SaveObject->Manifest, bRequireCoreSection);
    if (Validation != EWorldPersistenceResult::Success)
    {
        return Validation;
    }

    // Replace the active snapshot only after the loaded manifest passes every validation gate.
    CurrentManifest = SaveObject->Manifest;
    bHasSnapshot = true;
    return EWorldPersistenceResult::Success;
}
