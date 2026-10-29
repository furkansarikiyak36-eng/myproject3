#include "WorldThreatDirectorSubsystem.h"

#include "WorldSimulationClockSubsystem.h"
#include "WorldGameDataSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "InventoryCraftingComponent.h"
#include "HeatSourceComponent.h"
#include "LogContainerActor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
    struct FThreatTargetCandidate
    {
        FVector Location = FVector::ZeroVector;
        float Weight = 0.0f;
    };

    bool IsFoodForRaidSignals(const FGameDataItem* Item)
    {
        return Item && Item->bConsumable &&
            !Item->Id.ToString().StartsWith(TEXT("Water."), ESearchCase::IgnoreCase);
    }
}

float UWorldThreatDirectorSubsystem::ComputeRaidPressureFromSignals(
    float StoredUnits, float FoodSmellUnits, float LightSignature, float SmokeSignature) const
{
    if (!FMath::IsFinite(StoredUnits) || StoredUnits < 0.0f ||
        !FMath::IsFinite(FoodSmellUnits) || FoodSmellUnits < 0.0f ||
        !FMath::IsFinite(LightSignature) || LightSignature < 0.0f ||
        !FMath::IsFinite(SmokeSignature) || SmokeSignature < 0.0f ||
        !FMath::IsFinite(BaseRaidPressure) || BaseRaidPressure < 0.0f ||
        !FMath::IsFinite(StorageUnitsForFullPressure) || StorageUnitsForFullPressure <= 0.0f ||
        !FMath::IsFinite(StoragePressureWeight) || StoragePressureWeight < 0.0f ||
        !FMath::IsFinite(FoodUnitsForFullSmell) || FoodUnitsForFullSmell <= 0.0f ||
        !FMath::IsFinite(FoodSmellPressureWeight) || FoodSmellPressureWeight < 0.0f ||
        !FMath::IsFinite(LightPressureWeight) || LightPressureWeight < 0.0f ||
        !FMath::IsFinite(SmokePressureWeight) || SmokePressureWeight < 0.0f ||
        !FMath::IsFinite(MaxObservedLightSignature) || MaxObservedLightSignature <= 0.0f ||
        !FMath::IsFinite(MaxObservedSmokeSignature) || MaxObservedSmokeSignature <= 0.0f ||
        !FMath::IsFinite(MaxRaidPressure) || MaxRaidPressure <= 0.0f)
        return -1.0f;

    const float StorageTerm = FMath::Clamp(StoredUnits / StorageUnitsForFullPressure, 0.0f, 1.0f)
        * StoragePressureWeight;
    const float SmellTerm = FMath::Clamp(FoodSmellUnits / FoodUnitsForFullSmell, 0.0f, 1.0f)
        * FoodSmellPressureWeight;
    const float LightTerm = FMath::Clamp(LightSignature / MaxObservedLightSignature, 0.0f, 1.0f)
        * LightPressureWeight;
    const float SmokeTerm = FMath::Clamp(SmokeSignature / MaxObservedSmokeSignature, 0.0f, 1.0f)
        * SmokePressureWeight;
    const float Pressure = BaseRaidPressure + StorageTerm + SmellTerm + LightTerm + SmokeTerm;
    return FMath::IsFinite(Pressure) ? FMath::Clamp(Pressure, 0.1f, MaxRaidPressure) : -1.0f;
}

bool UWorldThreatDirectorSubsystem::CalculateRaidPressureAndTarget(
    int32 Seed, FVector& OutTargetLocation, float& OutPressure) const
{
    OutTargetLocation = FVector::ZeroVector;
    OutPressure = 0.0f;
    UWorld* World = GetWorld();
    if (!World)
        return false;

    const UWorldGameDataSubsystem* GameData = World->GetSubsystem<UWorldGameDataSubsystem>();
    const UWorldItemOwnershipSubsystem* Ownership = World->GetSubsystem<UWorldItemOwnershipSubsystem>();
    FRandomStream Stream(Seed);

    TMap<FGuid, FVector> StorageLocations;
    TMap<FGuid, FVector> InventoryLocations;
    TMap<FGuid, float> StorageUnitsById;
    TMap<FGuid, float> StorageFoodById;
    TArray<FThreatTargetCandidate> Candidates;
    float StoredUnits = 0.0f;
    float FoodSmellUnits = 0.0f;
    float LightSignature = 0.0f;
    float SmokeSignature = 0.0f;

    // Resolve physical container and inventory positions before scoring owned items.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
            continue;

        if (const ALogContainerActor* Container = Cast<ALogContainerActor>(Actor))
        {
            const FGuid StorageId = Container->GetStorageId();
            if (StorageId.IsValid())
                StorageLocations.Add(StorageId, Container->GetActorLocation());
        }

        if (const UInventoryCraftingComponent* Inventory =
            Actor->FindComponentByClass<UInventoryCraftingComponent>())
        {
            const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
            if (InventoryOwnerId.IsValid())
                InventoryLocations.Add(InventoryOwnerId, Actor->GetActorLocation());
        }

        TInlineComponentArray<UHeatSourceComponent*> HeatSources(Actor);
        for (const UHeatSourceComponent* Heat : HeatSources)
        {
            if (!IsValid(Heat) || !Heat->IsLit() || Heat->GetFuelSeconds() <= 0.0f)
                continue;
            const float Light = FMath::IsFinite(Heat->GetRaidLightSignature())
                ? FMath::Max(0.0f, Heat->GetRaidLightSignature()) : 0.0f;
            const float Smoke = FMath::IsFinite(Heat->GetRaidSmokeSignature())
                ? FMath::Max(0.0f, Heat->GetRaidSmokeSignature()) : 0.0f;
            LightSignature += Light;
            SmokeSignature += Smoke;
            const float Attraction = Light * 0.75f + Smoke;
            if (Attraction > 0.0f && FMath::IsFinite(Attraction))
                Candidates.Add({ Heat->GetComponentLocation(), Attraction });
        }
    }

    TArray<FWorldOwnedItemRecord> Items;
    if (Ownership)
        Ownership->ExportSnapshot(Items);
    for (const FWorldOwnedItemRecord& Item : Items)
    {
        if (Item.Quantity <= 0 || !FMath::IsFinite(static_cast<float>(Item.Quantity)) ||
            !FMath::IsFinite(Item.WorldLocation.X) || !FMath::IsFinite(Item.WorldLocation.Y) ||
            !FMath::IsFinite(Item.WorldLocation.Z))
            continue;

        const float Quantity = static_cast<float>(Item.Quantity);
        const FGameDataItem* Definition = GameData && GameData->IsApplied()
            ? GameData->GetItems().Find(Item.DefinitionId) : nullptr;
        const bool bFood = IsFoodForRaidSignals(Definition);

        if (Item.OwnerKind == EWorldItemOwnerKind::Storage)
        {
            StoredUnits += Quantity;
            StorageUnitsById.FindOrAdd(Item.OwnerId) += Quantity;
            if (bFood)
            {
                FoodSmellUnits += Quantity;
                StorageFoodById.FindOrAdd(Item.OwnerId) += Quantity;
            }
            continue;
        }

        if (!bFood)
            continue;

        if (Item.OwnerKind == EWorldItemOwnerKind::World)
        {
            // Exposed food is easy to smell and makes a plausible raider target.
            FoodSmellUnits += Quantity * 0.65f;
            Candidates.Add({ Item.WorldLocation, Quantity * 0.55f });
        }
        else if (Item.OwnerKind == EWorldItemOwnerKind::Inventory ||
                 Item.OwnerKind == EWorldItemOwnerKind::Equipped)
        {
            // Carried food contributes a smaller smell signal than food left at camp.
            FoodSmellUnits += Quantity * 0.15f;
            if (const FVector* Location = InventoryLocations.Find(Item.OwnerId))
                Candidates.Add({ *Location, Quantity * 0.12f });
        }
        else if (Item.OwnerKind == EWorldItemOwnerKind::Held ||
                 Item.OwnerKind == EWorldItemOwnerKind::InTransit)
        {
            FoodSmellUnits += Quantity * 0.3f;
            Candidates.Add({ Item.WorldLocation, Quantity * 0.2f });
        }
    }

    for (const TPair<FGuid, float>& Pair : StorageUnitsById)
    {
        const FVector* Location = StorageLocations.Find(Pair.Key);
        const float FoodUnits = StorageFoodById.FindRef(Pair.Key);
        if (Location)
        {
            // The overall stored quantity is a small attraction; food raises it substantially.
            const float Attraction = Pair.Value * 0.05f + FoodUnits * 0.45f;
            if (Attraction > 0.0f && FMath::IsFinite(Attraction))
                Candidates.Add({ *Location, Attraction });
        }
    }

    OutPressure = ComputeRaidPressureFromSignals(
        StoredUnits, FoodSmellUnits, LightSignature, SmokeSignature);
    if (OutPressure < 0.0f || !FMath::IsFinite(OutPressure))
        return false;

    // A small seeded variation keeps equal settlements from producing identical strength,
    // without making the result dependent on map/container iteration order.
    OutPressure = FMath::Clamp(OutPressure * Stream.FRandRange(0.95f, 1.05f),
        0.1f, MaxRaidPressure);

    float TotalWeight = 0.0f;
    for (const FThreatTargetCandidate& Candidate : Candidates)
    {
        if (Candidate.Weight > 0.0f && FMath::IsFinite(Candidate.Weight) &&
            !Candidate.Location.ContainsNaN())
            TotalWeight += Candidate.Weight;
    }

    if (FMath::IsFinite(TotalWeight) && TotalWeight > 0.0f)
    {
        float Selection = Stream.FRandRange(0.0f, TotalWeight);
        const FThreatTargetCandidate* Selected = nullptr;
        for (const FThreatTargetCandidate& Candidate : Candidates)
        {
            if (Candidate.Weight <= 0.0f || !FMath::IsFinite(Candidate.Weight) ||
                Candidate.Location.ContainsNaN())
                continue;
            Selection -= Candidate.Weight;
            if (Selection <= 0.0f)
            {
                Selected = &Candidate;
                break;
            }
        }
        if (!Selected)
        {
            for (int32 Index = Candidates.Num() - 1; Index >= 0; --Index)
            {
                if (Candidates[Index].Weight > 0.0f && !Candidates[Index].Location.ContainsNaN())
                {
                    Selected = &Candidates[Index];
                    break;
                }
            }
        }
        if (Selected)
        {
            const float Angle = Stream.FRandRange(0.0f, 2.0f * PI);
            const float Radius = Stream.FRandRange(120.0f, 350.0f);
            OutTargetLocation = Selected->Location + FVector(FMath::Cos(Angle) * Radius,
                FMath::Sin(Angle) * Radius, 0.0f);
            return !OutTargetLocation.ContainsNaN();
        }
    }

    // Fallback for an undiscovered/empty world remains deterministic.
    OutTargetLocation = FVector(Stream.FRandRange(-5000.0f, 5000.0f),
        Stream.FRandRange(-5000.0f, 5000.0f), 0.0f);
    return !OutTargetLocation.ContainsNaN();
}

UWorldSimulationClockSubsystem* UWorldThreatDirectorSubsystem::GetClock() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
}

bool UWorldThreatDirectorSubsystem::InitializeDirector(double FirstRaidDelaySeconds)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!Clock || !Clock->IsClockInitialized() ||
        !FMath::IsFinite(FirstRaidDelaySeconds) || FirstRaidDelaySeconds < 0.0 ||
        !FMath::IsFinite(MinimumIntervalSeconds) ||
        !FMath::IsFinite(MaximumIntervalSeconds) ||
        MinimumIntervalSeconds < 1.0 || MaximumIntervalSeconds < MinimumIntervalSeconds ||
        !FMath::IsFinite(WarningDurationSeconds) || WarningDurationSeconds <= 0.0 ||
        !FMath::IsFinite(RaidDurationSeconds) || RaidDurationSeconds <= 0.0 ||
        MaxHistoryEntries < 1)
    {
        return false;
    }

    CurrentRaid = FThreatRaidRecord();
    RaidHistory.Reset();
    RaidSerial = 0;
    NextRaidTimeSeconds = Clock->GetSimulationTimeSeconds() + FirstRaidDelaySeconds;
    bInitialized = FMath::IsFinite(NextRaidTimeSeconds);
    return bInitialized;
}

bool UWorldThreatDirectorSubsystem::CanAdvanceDirectorAtTime(
    double TargetTimeSeconds, int64 TargetTickIndex) const
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!bInitialized || !Clock || !Clock->IsClockInitialized() ||
        !FMath::IsFinite(TargetTimeSeconds) ||
        TargetTimeSeconds < Clock->GetSimulationTimeSeconds() ||
        TargetTickIndex < Clock->GetTickIndex())
    {
        return false;
    }

    const bool bWarningWillActivate =
        CurrentRaid.State == EThreatRaidState::Warning &&
        TargetTimeSeconds >= CurrentRaid.ActiveStartTime;
    const bool bWillFinishCurrent =
        (CurrentRaid.State == EThreatRaidState::Active ||
         (bWarningWillActivate && TargetTimeSeconds >= CurrentRaid.PlannedEndTime)) &&
        TargetTimeSeconds >= CurrentRaid.PlannedEndTime;

    if ((bWarningWillActivate && CurrentRaid.Revision == MAX_int64) ||
        (bWillFinishCurrent && CurrentRaid.Revision >=
            (bWarningWillActivate ? MAX_int64 - 1 : MAX_int64)))
    {
        return false;
    }

    const bool bWillNeedRaid =
        (CurrentRaid.State == EThreatRaidState::None || IsTerminal(CurrentRaid.State) ||
         bWillFinishCurrent) &&
        TargetTimeSeconds >= NextRaidTimeSeconds;
    if (!bWillNeedRaid)
    {
        return true;
    }

    if (RaidSerial == MAX_int64)
    {
        return false;
    }

    const int64 ProposedSerial = RaidSerial + 1;
    const int32 Seed = Clock->GetRandomSeed() ^ static_cast<int32>(ProposedSerial * 196613);
    const FGuid ProposedId(Clock->GetRandomSeed(), static_cast<int32>(ProposedSerial),
        static_cast<int32>(TargetTickIndex), Seed);
    const double ActiveStart = TargetTimeSeconds + WarningDurationSeconds;
    const double PlannedEnd = ActiveStart + RaidDurationSeconds;
    const double NextTime = PlannedEnd + MakeNextInterval(ProposedSerial);
    return ProposedId.IsValid() && FMath::IsFinite(ActiveStart) &&
        FMath::IsFinite(PlannedEnd) && FMath::IsFinite(NextTime);
}

bool UWorldThreatDirectorSubsystem::AdvanceDirectorAtTime(
    double TargetTimeSeconds, int64 TargetTickIndex)
{
    if (!CanAdvanceDirectorAtTime(TargetTimeSeconds, TargetTickIndex))
    {
        return false;
    }

    UWorldSimulationClockSubsystem* Clock = GetClock();
    const double Now = TargetTimeSeconds;

    if (CurrentRaid.State == EThreatRaidState::Warning && Now >= CurrentRaid.ActiveStartTime)
    {
        CurrentRaid.State = EThreatRaidState::Active;
        ++CurrentRaid.Revision;
    }

    if (CurrentRaid.State == EThreatRaidState::Active && Now >= CurrentRaid.PlannedEndTime)
    {
        FinishCurrentRaid(EThreatRaidState::Failed, Now);
    }

    if ((CurrentRaid.State == EThreatRaidState::None || IsTerminal(CurrentRaid.State)) &&
        Now >= NextRaidTimeSeconds)
    {
        if (RaidSerial == MAX_int64)
        {
            return false;
        }

        ++RaidSerial;
        const int32 Seed = Clock->GetRandomSeed() ^ static_cast<int32>(RaidSerial * 196613);
        FRandomStream Stream(Seed);
        const FGuid RaidId = FGuid(Clock->GetRandomSeed(), static_cast<int32>(RaidSerial),
            static_cast<int32>(TargetTickIndex), Seed);
        if (!RaidId.IsValid())
        {
            return false;
        }

        CurrentRaid = FThreatRaidRecord();
        CurrentRaid.RaidId = RaidId;
        CurrentRaid.Serial = RaidSerial;
        CurrentRaid.State = EThreatRaidState::Warning;
        CurrentRaid.WarningStartTime = Now;
        CurrentRaid.ActiveStartTime = Now + WarningDurationSeconds;
        CurrentRaid.PlannedEndTime = CurrentRaid.ActiveStartTime + RaidDurationSeconds;
        CurrentRaid.Pressure = Stream.FRandRange(1.0f, 3.0f);
        CurrentRaid.TargetLocation = FVector(
            Stream.FRandRange(-5000.0f, 5000.0f),
            Stream.FRandRange(-5000.0f, 5000.0f), 0.0f);
        CurrentRaid.Revision = 1;
        NextRaidTimeSeconds = CurrentRaid.PlannedEndTime + MakeNextInterval(RaidSerial);
        if (!FMath::IsFinite(NextRaidTimeSeconds))
        {
            CurrentRaid = FThreatRaidRecord();
            return false;
        }
    }

    return true;
}

bool UWorldThreatDirectorSubsystem::AdvanceDirector()
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!Clock || !Clock->IsClockInitialized() || Clock->GetTickIndex() == MAX_int64)
    {
        return false;
    }
    return AdvanceDirectorAtTime(Clock->GetSimulationTimeSeconds(), Clock->GetTickIndex());
}

bool UWorldThreatDirectorSubsystem::TryClaimDamagePulse(const FGuid& RaidId, int64 PulseIndex)
{
    if (!RaidId.IsValid() || PulseIndex < 0 ||
        CurrentRaid.RaidId != RaidId || CurrentRaid.State != EThreatRaidState::Active ||
        CurrentRaid.Revision >= MAX_int64 - 1 ||
        CurrentRaid.LastAppliedDamagePulseIndex >= PulseIndex)
        return false;

    // Persist the pulse claim before any actor damage/destruction. A failed
    // materialization/collapse may block the result, but must not replay damage.
    CurrentRaid.LastAppliedDamagePulseIndex = PulseIndex;
    ++CurrentRaid.Revision;
    return true;
}

bool UWorldThreatDirectorSubsystem::ResolveCurrentRaid(bool bRepelled)
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!bInitialized || !Clock || !Clock->IsClockInitialized() ||
        (CurrentRaid.State != EThreatRaidState::Warning &&
         CurrentRaid.State != EThreatRaidState::Active))
    {
        return false;
    }

    FinishCurrentRaid(bRepelled ? EThreatRaidState::Repelled : EThreatRaidState::Failed,
        Clock->GetSimulationTimeSeconds());
    return true;
}

bool UWorldThreatDirectorSubsystem::CancelCurrentRaid()
{
    UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!bInitialized || !Clock || !Clock->IsClockInitialized() ||
        (CurrentRaid.State != EThreatRaidState::Warning &&
         CurrentRaid.State != EThreatRaidState::Active))
    {
        return false;
    }

    FinishCurrentRaid(EThreatRaidState::Cancelled, Clock->GetSimulationTimeSeconds());
    return true;
}

bool UWorldThreatDirectorSubsystem::GetCurrentRaid(FThreatRaidRecord& OutRaid) const
{
    if (CurrentRaid.State == EThreatRaidState::None)
    {
        return false;
    }
    OutRaid = CurrentRaid;
    return true;
}

bool UWorldThreatDirectorSubsystem::IsTerminal(EThreatRaidState State) const
{
    return State == EThreatRaidState::Repelled ||
        State == EThreatRaidState::Failed ||
        State == EThreatRaidState::Cancelled;
}

double UWorldThreatDirectorSubsystem::MakeNextInterval(int64 Serial) const
{
    const UWorldSimulationClockSubsystem* Clock = GetClock();
    if (!Clock)
    {
        return 0.0;
    }
    const int32 Seed = Clock->GetRandomSeed() ^ static_cast<int32>(Serial * 83492791);
    FRandomStream Stream(Seed);
    return Stream.FRandRange(
        static_cast<float>(MinimumIntervalSeconds),
        static_cast<float>(MaximumIntervalSeconds));
}

void UWorldThreatDirectorSubsystem::FinishCurrentRaid(EThreatRaidState FinalState, double Now)
{
    if (CurrentRaid.State == EThreatRaidState::None || IsTerminal(CurrentRaid.State))
    {
        return;
    }

    CurrentRaid.State = FinalState;
    ++CurrentRaid.Revision;
    if (MaxHistoryEntries < 1)
    {
        RaidHistory.Reset();
        return;
    }
    while (RaidHistory.Num() >= MaxHistoryEntries)
    {
        RaidHistory.RemoveAt(0, 1, EAllowShrinking::No);
    }
    RaidHistory.Add(CurrentRaid);
    if (!FMath::IsFinite(NextRaidTimeSeconds) || NextRaidTimeSeconds < Now)
    {
        NextRaidTimeSeconds = Now + MakeNextInterval(RaidSerial);
    }
}
