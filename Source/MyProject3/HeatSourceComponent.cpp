#include "HeatSourceComponent.h"

#include "WorldSurvivalSubsystem.h"
#include "PlacementKernelSubsystem.h"
#include "LogCarryComponent.h"
#include "LogBuildPiece.h"
#include "StickPrimitivePiece.h"
#include "StructuralPieceInterface.h"
#include "WorldObjectIdentityComponent.h"
#include "Engine/World.h"

UHeatSourceComponent::UHeatSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHeatSourceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr)
    {
        Survival->RegisterHeatSource(this);
    }
}

void UHeatSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr)
    {
        Survival->UnregisterHeatSource(this);
    }
    Super::EndPlay(EndPlayReason);
}

bool UHeatSourceComponent::AddFuel(float BurnSeconds)
{
    if (!FMath::IsFinite(BurnSeconds) || BurnSeconds <= 0.0f || FuelSeconds >= MaxFuelSeconds)
        return false;
    FuelSeconds = FMath::Min(MaxFuelSeconds, FuelSeconds + BurnSeconds);
    return true;
}

bool UHeatSourceComponent::TryAddFuelFromHeldItem(
    ULogCarryComponent* Carrier, AActor* FuelActor, FGuid OperationId,
    float BurnSeconds, float& OutAddedBurnSeconds, FString& OutFailureReason)
{
    OutAddedBurnSeconds = 0.0f;
    OutFailureReason.Reset();
    if (!OperationId.IsValid() || !IsValid(Carrier) || !IsValid(FuelActor) ||
        !FMath::IsFinite(BurnSeconds) || BurnSeconds <= 0.0f)
    {
        OutFailureReason = TEXT("Invalid fuel receiver operation, carrier, item or burn-time value.");
        return false;
    }

    UWorldObjectIdentityComponent* Identity =
        FuelActor->FindComponentByClass<UWorldObjectIdentityComponent>();
    const IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(FuelActor);
    if (!Identity || !Structural ||
        (Structural->GetStructuralKind() != EStructuralNodeKind::Wood &&
         Structural->GetStructuralKind() != EStructuralNodeKind::Stick))
    {
        OutFailureReason = TEXT("This receiver accepts physical wood or stick pieces, not stone or arbitrary actors.");
        return false;
    }

    const FGuid FuelItemId = Identity->GetInstanceId();
    if (const FHeatFuelOperationReceipt* Previous = FuelOperationReceipts.Find(OperationId))
    {
        if (Previous->FuelItemId != FuelItemId ||
            !FMath::IsNearlyEqual(Previous->RequestedBurnSeconds, BurnSeconds, 0.001f))
        {
            OutFailureReason = TEXT("This fuel operation ID was already used for another item or burn-time request.");
            return false;
        }
        OutAddedBurnSeconds = Previous->AddedBurnSeconds;
        return true;
    }

    UPlacementKernelSubsystem* Kernel = GetWorld()
        ? GetWorld()->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
    FPlacementProfileDefinition Profile;
    if (!Kernel || !Kernel->IsCatalogReady() ||
        !Kernel->GetProfile(TEXT("fire.fuel_receiver"), Profile) ||
        Profile.OperationKindText != TEXT("receiver_interaction") ||
        Profile.Geometry != TEXT("receiver_slot") || !Profile.bAtomicCommit)
    {
        OutFailureReason = TEXT("The global fire.fuel_receiver profile is missing or invalid.");
        return false;
    }

    if (!FMath::IsFinite(FuelSeconds) || !FMath::IsFinite(MaxFuelSeconds) ||
        MaxFuelSeconds <= 0.0f || FuelSeconds < 0.0f || FuelSeconds >= MaxFuelSeconds)
    {
        OutFailureReason = TEXT("This heat source has no remaining fuel capacity.");
        return false;
    }

    const float AddedSeconds = FMath::Min(BurnSeconds, MaxFuelSeconds - FuelSeconds);
    if (!FMath::IsFinite(AddedSeconds) || AddedSeconds <= 0.0f)
    {
        OutFailureReason = TEXT("The requested item would add no fuel to this heat source.");
        return false;
    }

    if (!Carrier->ConsumeHeldItemForReceiver(FuelActor, OperationId, OutFailureReason))
        return false;

    // No fallible operation follows the successful consumption. Fuel amount and
    // receipt are committed together on the same game-thread call.
    FuelSeconds = FMath::Min(MaxFuelSeconds, FuelSeconds + AddedSeconds);
    FHeatFuelOperationReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.FuelItemId = FuelItemId;
    Receipt.RequestedBurnSeconds = BurnSeconds;
    Receipt.AddedBurnSeconds = AddedSeconds;
    RememberFuelReceipt(Receipt);
    OutAddedBurnSeconds = AddedSeconds;
    return true;
}

void UHeatSourceComponent::RememberFuelReceipt(const FHeatFuelOperationReceipt& Receipt)
{
    constexpr int32 MaxReceipts = 512;
    if (!FuelOperationReceipts.Contains(Receipt.OperationId))
        FuelOperationOrder.Add(Receipt.OperationId);
    FuelOperationReceipts.Add(Receipt.OperationId, Receipt);

    while (FuelOperationOrder.Num() > MaxReceipts)
    {
        const FGuid ExpiredId = FuelOperationOrder[0];
        FuelOperationOrder.RemoveAt(0);
        FuelOperationReceipts.Remove(ExpiredId);
    }
}

bool UHeatSourceComponent::Ignite()
{
    if (FuelSeconds <= 0.0f)
        return false;
    bLit = true;
    return true;
}

void UHeatSourceComponent::Extinguish()
{
    bLit = false;
}

float UHeatSourceComponent::GetHeatAt(const FVector& WorldLocation) const
{
    if (!bLit || RadiusCm <= 0.0f || WorldLocation.ContainsNaN())
        return 0.0f;
    const float Distance = FVector::Dist(GetComponentLocation(), WorldLocation);
    if (Distance >= RadiusCm)
        return 0.0f;
    // Quadratic falloff: warm next to the fire, little at the edge.
    const float Closeness = 1.0f - Distance / RadiusCm;
    return HeatOutputCelsius * Closeness * Closeness;
}

void UHeatSourceComponent::AdvanceFuel(double DeltaSeconds, EWorldWeatherState Weather, float WeatherSeverity)
{
    if (!bLit || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
        return;

    // Heavy rain and storms drown a fire that is not sheltered enough.
    const bool bWetWeather = Weather == EWorldWeatherState::Rain || Weather == EWorldWeatherState::Storm;
    if (bWetWeather && WeatherSeverity > WeatherProtection)
    {
        bLit = false;
        return;
    }

    FuelSeconds = FMath::Max(0.0f, FuelSeconds - static_cast<float>(DeltaSeconds));
    if (FuelSeconds <= 0.0f)
        bLit = false;
}
