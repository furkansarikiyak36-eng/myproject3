#include "WorldCombustibleComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldSimulationClockSubsystem.h"

UWorldCombustibleComponent::UWorldCombustibleComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UWorldCombustibleComponent::EnsureCombustibleIdentity()
{
    if (CombustibleId.IsValid())
        return true;
    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
        return false;

    if (UWorldObjectIdentityComponent* Identity =
        Owner->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (!Identity->HasValidIdentity())
            Identity->EnsureIdentity();
        if (Identity->HasValidIdentity())
            CombustibleId = Identity->GetInstanceId();
    }
    if (!CombustibleId.IsValid())
        CombustibleId = FGuid::NewGuid();
    return CombustibleId.IsValid();
}

FName UWorldCombustibleComponent::ResolveRegionId() const
{
    if (!RegionId.IsNone())
        return RegionId;
    const AActor* Owner = GetOwner();
    if (!IsValid(Owner))
        return NAME_None;

    FName Resolved = NAME_None;
    for (const FName& Tag : Owner->Tags)
    {
        const FString Value = Tag.ToString();
        if (!Value.StartsWith(TEXT("Region:"), ESearchCase::CaseSensitive))
            continue;
        const FString RegionText = Value.RightChop(7);
        if (RegionText.IsEmpty())
            return NAME_None;
        const FName Candidate(*RegionText);
        if (!Resolved.IsNone() && Resolved != Candidate)
            return NAME_None;
        Resolved = Candidate;
    }
    return Resolved;
}

bool UWorldCombustibleComponent::HasIgnitionReceipt(const FGuid& OperationId) const
{
    return OperationId.IsValid() && IgnitionOperationIds.Contains(OperationId);
}

void UWorldCombustibleComponent::RecordIgnitionReceipt(const FGuid& OperationId)
{
    if (!OperationId.IsValid() || IgnitionOperationIds.Contains(OperationId))
        return;
    IgnitionOperationIds.Add(OperationId);
    while (IgnitionOperationIds.Num() > FMath::Clamp(MaxIgnitionReceipts, 1, 256))
        IgnitionOperationIds.RemoveAt(0, 1, EAllowShrinking::No);
}

bool UWorldCombustibleComponent::TryIgnite(const FGuid& OperationId, float Intensity)
{
    if (!OperationId.IsValid() || MaxIgnitionReceipts <= 0 || MaxIgnitionReceipts > 256 ||
        !FMath::IsFinite(Intensity) || Intensity <= 0.0f || Intensity > 1.0f ||
        !FMath::IsFinite(FuelUnitsRemaining) || FuelUnitsRemaining < 0.0f ||
        !FMath::IsFinite(Moisture) || Moisture < 0.0f || Moisture > 1.0f ||
        !FMath::IsFinite(IgnitionMoistureThreshold) ||
        IgnitionMoistureThreshold < 0.0f || IgnitionMoistureThreshold > 1.0f || bFirebreak)
        return false;

    if (HasIgnitionReceipt(OperationId))
        return true;
    if (bBurning)
    {
        RecordIgnitionReceipt(OperationId);
        return true;
    }
    if (FuelUnitsRemaining <= 0.0f || Moisture > IgnitionMoistureThreshold)
        return false;

    bBurning = true;
    FireIntensity = FMath::Clamp(FMath::Max(FireIntensity, Intensity), 0.01f, 1.0f);
    const UWorldSimulationClockSubsystem* Clock =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    LastIgnitedTickIndex = Clock && Clock->IsClockInitialized()
        ? Clock->GetTickIndex() : INDEX_NONE;
    RecordIgnitionReceipt(OperationId);
    SynchronizeRaidSignalTags();
    return true;
}

bool UWorldCombustibleComponent::AdvanceBurn(
    double DeltaSeconds, int64 TickIndex, float EventBurnRateMultiplier, float& OutBurnedUnits)
{
    OutBurnedUnits = 0.0f;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        TickIndex < 0 || !FMath::IsFinite(EventBurnRateMultiplier) ||
        EventBurnRateMultiplier < 0.01f || EventBurnRateMultiplier > 100.0f ||
        !FMath::IsFinite(FuelUnitsRemaining) || FuelUnitsRemaining < 0.0f ||
        !FMath::IsFinite(BurnUnitsPerHour) || BurnUnitsPerHour < 0.0f ||
        !FMath::IsFinite(FireIntensity) || FireIntensity < 0.0f || FireIntensity > 1.0f)
        return false;

    if (!bBurning || LastBurnTickIndex == TickIndex)
        return true;
    if (FuelUnitsRemaining <= 0.0f || FireIntensity <= 0.0f)
    {
        FuelUnitsRemaining = 0.0f;
        FireIntensity = 0.0f;
        bBurning = false;
        SynchronizeRaidSignalTags();
        return true;
    }

    const float Burn = BurnUnitsPerHour * EventBurnRateMultiplier *
        FireIntensity * static_cast<float>(DeltaSeconds / 3600.0);
    if (!FMath::IsFinite(Burn) || Burn < 0.0f)
        return false;

    OutBurnedUnits = FMath::Min(FuelUnitsRemaining, Burn);
    FuelUnitsRemaining = FMath::Max(0.0f, FuelUnitsRemaining - OutBurnedUnits);
    LastBurnTickIndex = TickIndex;
    if (FuelUnitsRemaining <= KINDA_SMALL_NUMBER)
    {
        FuelUnitsRemaining = 0.0f;
        FireIntensity = 0.0f;
        bBurning = false;
    }
    SynchronizeRaidSignalTags();
    return true;
}

bool UWorldCombustibleComponent::AdvanceWeatherMoisture(
    double DeltaSeconds, int64 TickIndex, EWorldWeatherState Weather, float WeatherSeverity)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        TickIndex < 0 || static_cast<uint8>(Weather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        !FMath::IsFinite(WeatherSeverity) || WeatherSeverity < 0.0f || WeatherSeverity > 1.0f ||
        !FMath::IsFinite(Moisture) || Moisture < 0.0f || Moisture > 1.0f)
        return false;
    if (LastMoistureTickIndex == TickIndex)
        return true;

    const float Hours = static_cast<float>(DeltaSeconds / 3600.0);
    float Delta = 0.0f;
    if (Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm)
        Delta = 0.25f * WeatherSeverity * Hours;
    else if (Weather == EWorldWeatherState::Snow)
        Delta = 0.12f * WeatherSeverity * Hours;
    else if (Weather == EWorldWeatherState::Heatwave)
        Delta = -0.04f * WeatherSeverity * Hours;
    Moisture = FMath::Clamp(Moisture + Delta, 0.0f, 1.0f);
    LastMoistureTickIndex = TickIndex;
    return true;
}

void UWorldCombustibleComponent::SynchronizeRaidSignalTags()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner))
        return;

    const FName FireLightTag(TEXT("RaidSignal.FireLight:0.35"));
    const FName FireSmokeTag(TEXT("RaidSignal.FireSmoke:0.65"));
    Owner->Tags.Remove(FireLightTag);
    Owner->Tags.Remove(FireSmokeTag);
    if (bBurning && FuelUnitsRemaining > 0.0f)
    {
        Owner->Tags.AddUnique(FireLightTag);
        Owner->Tags.AddUnique(FireSmokeTag);
    }
}
