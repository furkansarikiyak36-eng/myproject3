#include "WorldFirePropagationSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
#include "WorldCombustibleComponent.h"
#include "WorldRegionalEventSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldSurvivalSubsystem.h"

namespace
{
    struct FCombustibleEntry
    {
        UWorldCombustibleComponent* Component = nullptr;
        FGuid Id;
        FName RegionId = NAME_None;
        FVector Location = FVector::ZeroVector;
        bool bBurningAtStart = false;
    };

    struct FProposedIgnition
    {
        UWorldCombustibleComponent* Target = nullptr;
        FGuid OperationId;
        float Intensity = 1.0f;
    };

    float GetDrynessFactor(const UWorldCombustibleComponent* Component)
    {
        if (!IsValid(Component) || Component->IgnitionMoistureThreshold <= 0.0f)
            return 0.0f;
        if (Component->GetMoisture() > Component->IgnitionMoistureThreshold)
            return 0.0f;
        return FMath::Clamp(1.0f -
            Component->GetMoisture() / Component->IgnitionMoistureThreshold, 0.0f, 1.0f);
    }
}

float UWorldFirePropagationSubsystem::StableRoll01(
    int32 RandomSeed, int64 TickIndex, FName RegionId,
    const FGuid& SourceId, const FGuid& TargetId, const TCHAR* Purpose)
{
    const FString Key = FString::Printf(TEXT("%d|%lld|%s|%s|%s|%s"),
        RandomSeed, static_cast<long long>(TickIndex), *RegionId.ToString(),
        *SourceId.ToString(), *TargetId.ToString(), Purpose);
    const uint32 Hash = FCrc::StrCrc32(*Key);
    return static_cast<float>(Hash) / static_cast<float>(MAX_uint32);
}

FGuid UWorldFirePropagationSubsystem::MakeOperationId(
    int32 RandomSeed, int64 TickIndex, FName RegionId,
    const FGuid& SourceId, const FGuid& TargetId, const TCHAR* Purpose)
{
    const FString Base = FString::Printf(TEXT("%d|%lld|%s|%s|%s|%s"),
        RandomSeed, static_cast<long long>(TickIndex), *RegionId.ToString(),
        *SourceId.ToString(), *TargetId.ToString(), Purpose);
    uint32 Parts[4];
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FString Salted = FString::Printf(TEXT("%s|%d"), *Base, Index);
        Parts[Index] = FCrc::StrCrc32(*Salted);
    }
    if ((Parts[0] | Parts[1] | Parts[2] | Parts[3]) == 0)
        Parts[3] = 1;
    return FGuid(Parts[0], Parts[1], Parts[2], Parts[3]);
}

bool UWorldFirePropagationSubsystem::IsFirebreakBetween(
    const FVector& Source, const FVector& Target, FName RegionId,
    const TArray<UWorldCombustibleComponent*>& Components) const
{
    const FVector Segment = Target - Source;
    const double SegmentLengthSquared = Segment.SizeSquared();
    if (SegmentLengthSquared <= 1.0)
        return false;

    constexpr double FirebreakHalfWidthCm = 125.0;
    for (const UWorldCombustibleComponent* Component : Components)
    {
        if (!IsValid(Component) || !Component->bFirebreak ||
            Component->ResolveRegionId() != RegionId)
            continue;
        const AActor* Actor = Component->GetOwner();
        if (!IsValid(Actor))
            continue;
        const FVector Offset = Actor->GetActorLocation() - Source;
        const double T = FVector::DotProduct(Offset, Segment) / SegmentLengthSquared;
        if (T <= 0.02 || T >= 0.98)
            continue;
        const FVector Closest = Source + Segment * T;
        if (FVector::DistSquared(Closest, Actor->GetActorLocation()) <=
            FMath::Square(FirebreakHalfWidthCm))
            return true;
    }
    return false;
}

bool UWorldFirePropagationSubsystem::AdvanceRegionalFireSpread(double DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        MaxCombustibleComponents <= 0 || MaxCombustibleComponents > 4096)
        return false;

    UWorld* World = GetWorld();
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!World || !Clock || !Clock->IsClockInitialized() || !Survival)
        return false;

    TArray<UWorldCombustibleComponent*> Components;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
            continue;
        TArray<UWorldCombustibleComponent*> ActorComponents;
        Actor->GetComponents<UWorldCombustibleComponent>(ActorComponents);
        for (UWorldCombustibleComponent* Component : ActorComponents)
        {
            if (!IsValid(Component))
                continue;
            Components.Add(Component);
            if (Components.Num() > MaxCombustibleComponents)
                return false;
        }
    }
    if (Components.IsEmpty())
        return true;

    TArray<FCombustibleEntry> Entries;
    Entries.Reserve(Components.Num());
    TSet<FGuid> SeenIds;
    for (UWorldCombustibleComponent* Component : Components)
    {
        if (!Component->EnsureCombustibleIdentity())
            return false;
        const FGuid Id = Component->CombustibleId;
        if (!Id.IsValid() || SeenIds.Contains(Id) ||
            !FMath::IsFinite(Component->FuelUnitsRemaining) || Component->FuelUnitsRemaining < 0.0f ||
            !FMath::IsFinite(Component->Moisture) || Component->Moisture < 0.0f || Component->Moisture > 1.0f ||
            !FMath::IsFinite(Component->IgnitionMoistureThreshold) ||
            Component->IgnitionMoistureThreshold < 0.0f || Component->IgnitionMoistureThreshold > 1.0f ||
            !FMath::IsFinite(Component->IgnitionSusceptibility) ||
            Component->IgnitionSusceptibility < 0.0f || Component->IgnitionSusceptibility > 4.0f ||
            !FMath::IsFinite(Component->SpreadRadiusCm) || Component->SpreadRadiusCm < 0.0f ||
            !FMath::IsFinite(Component->SpreadChanceMultiplier) ||
            Component->SpreadChanceMultiplier < 0.0f || Component->SpreadChanceMultiplier > 4.0f ||
            !FMath::IsFinite(Component->BurnUnitsPerHour) || Component->BurnUnitsPerHour < 0.0f)
            return false;
        SeenIds.Add(Id);

        FCombustibleEntry& Entry = Entries.AddDefaulted_GetRef();
        Entry.Component = Component;
        Entry.Id = Id;
        Entry.RegionId = Component->ResolveRegionId();
        Entry.Location = Component->GetOwner()->GetActorLocation();
        Entry.bBurningAtStart = Component->IsBurning();
    }
    Entries.Sort([](const FCombustibleEntry& A, const FCombustibleEntry& B)
        { return A.Id.ToString() < B.Id.ToString(); });

    const int64 TickIndex = Clock->GetTickIndex();
    const int32 RandomSeed = Clock->GetRandomSeed();
    if (TickIndex < 0)
        return false;

    // Resolve all regional event state before changing moisture, fuel or ignition.
    TMap<FName, FWorldRegionalEventEvaluation> EvaluationByRegion;
    for (const FCombustibleEntry& Entry : Entries)
    {
        if (Entry.RegionId.IsNone() || EvaluationByRegion.Contains(Entry.RegionId))
            continue;
        if (!Events || !Events->AreDefinitionsLoaded())
            return false;
        FWorldRegionalEventEvaluation Evaluation;
        if (!Events->EvaluateRegion(Entry.RegionId, Evaluation))
            return false;
        if (!FMath::IsFinite(Evaluation.FireIgnitionChancePerHour) ||
            Evaluation.FireIgnitionChancePerHour < 0.0f || Evaluation.FireIgnitionChancePerHour > 1.0f ||
            !FMath::IsFinite(Evaluation.FireSpreadChancePerHour) ||
            Evaluation.FireSpreadChancePerHour < 0.0f || Evaluation.FireSpreadChancePerHour > 1.0f ||
            !FMath::IsFinite(Evaluation.FireBurnRateMultiplier) ||
            Evaluation.FireBurnRateMultiplier < 0.01f || Evaluation.FireBurnRateMultiplier > 100.0f)
            return false;
        EvaluationByRegion.Add(Entry.RegionId, Evaluation);
    }

    const double Hours = DeltaSeconds / 3600.0;
    for (const FCombustibleEntry& Entry : Entries)
        if (!Entry.Component->AdvanceWeatherMoisture(
            DeltaSeconds, TickIndex, Survival->Weather, Survival->WeatherSeverity))
            return false;

    TArray<FProposedIgnition> Proposals;
    TSet<FGuid> ProposedTargetIds;
    auto AddProposal = [&Proposals, &ProposedTargetIds](
        UWorldCombustibleComponent* Target, const FGuid& OperationId, float Intensity)
    {
        if (!IsValid(Target) || !OperationId.IsValid() || ProposedTargetIds.Contains(Target->CombustibleId))
            return;
        FProposedIgnition& Proposal = Proposals.AddDefaulted_GetRef();
        Proposal.Target = Target;
        Proposal.OperationId = OperationId;
        Proposal.Intensity = FMath::Clamp(Intensity, 0.01f, 1.0f);
        ProposedTargetIds.Add(Target->CombustibleId);
    };

    // Lightning and dry vegetation can ignite a surface without an adjacent flame.
    for (const FCombustibleEntry& Entry : Entries)
    {
        UWorldCombustibleComponent* Component = Entry.Component;
        if (Entry.RegionId.IsNone() || Entry.bBurningAtStart || Component->bFirebreak ||
            Component->FuelUnitsRemaining <= 0.0f)
            continue;
        const FWorldRegionalEventEvaluation* Evaluation = EvaluationByRegion.Find(Entry.RegionId);
        if (!Evaluation || Evaluation->FireIgnitionChancePerHour <= 0.0f)
            continue;
        const float Dryness = GetDrynessFactor(Component);
        const float Chance = FMath::Clamp(
            Evaluation->FireIgnitionChancePerHour * static_cast<float>(Hours) *
            Component->IgnitionSusceptibility * Dryness, 0.0f, 1.0f);
        const FGuid NoSource;
        if (Chance > 0.0f && StableRoll01(RandomSeed, TickIndex, Entry.RegionId,
            NoSource, Entry.Id, TEXT("natural-ignition")) < Chance)
        {
            AddProposal(Component, MakeOperationId(RandomSeed, TickIndex, Entry.RegionId,
                NoSource, Entry.Id, TEXT("natural-ignition")), 1.0f);
        }
    }

    // Existing flames spread only inside their authored region and radius.
    for (const FCombustibleEntry& SourceEntry : Entries)
    {
        UWorldCombustibleComponent* Source = SourceEntry.Component;
        if (!SourceEntry.bBurningAtStart || SourceEntry.RegionId.IsNone() ||
            Source->bFirebreak || Source->SpreadRadiusCm <= 0.0f)
            continue;
        const FWorldRegionalEventEvaluation* Evaluation = EvaluationByRegion.Find(SourceEntry.RegionId);
        if (!Evaluation || Evaluation->FireSpreadChancePerHour <= 0.0f)
            continue;

        for (const FCombustibleEntry& TargetEntry : Entries)
        {
            UWorldCombustibleComponent* Target = TargetEntry.Component;
            if (TargetEntry.bBurningAtStart || Target->bBurning ||
                TargetEntry.Id == SourceEntry.Id || TargetEntry.RegionId != SourceEntry.RegionId ||
                Target->bFirebreak || Target->FuelUnitsRemaining <= 0.0f)
                continue;
            if (FVector::DistSquared(SourceEntry.Location, TargetEntry.Location) >
                FMath::Square(Source->SpreadRadiusCm))
                continue;
            if (IsFirebreakBetween(SourceEntry.Location, TargetEntry.Location,
                SourceEntry.RegionId, Components))
                continue;

            const float Dryness = GetDrynessFactor(Target);
            const float Chance = FMath::Clamp(
                Evaluation->FireSpreadChancePerHour * static_cast<float>(Hours) *
                Source->SpreadChanceMultiplier * Source->FireIntensity * Dryness, 0.0f, 1.0f);
            if (Chance <= 0.0f)
                continue;
            if (StableRoll01(RandomSeed, TickIndex, SourceEntry.RegionId,
                SourceEntry.Id, TargetEntry.Id, TEXT("fire-spread")) < Chance)
            {
                AddProposal(Target, MakeOperationId(RandomSeed, TickIndex, SourceEntry.RegionId,
                    SourceEntry.Id, TargetEntry.Id, TEXT("fire-spread")),
                    FMath::Max(0.15f, Source->FireIntensity * 0.75f));
            }
        }
    }

    // Burning material consumes its fuel once for this logical tick.
    for (const FCombustibleEntry& Entry : Entries)
    {
        if (!Entry.bBurningAtStart)
            continue;
        const FWorldRegionalEventEvaluation* Evaluation =
            EvaluationByRegion.Find(Entry.RegionId);
        const float BurnMultiplier = Evaluation ? Evaluation->FireBurnRateMultiplier : 1.0f;
        float BurnedUnits = 0.0f;
        if (!Entry.Component->AdvanceBurn(DeltaSeconds, TickIndex, BurnMultiplier, BurnedUnits))
            return false;
    }

    // New flames begin burning next fixed step; they cannot spend a full hour of
    // fuel in the same tick in which they were ignited.
    for (const FProposedIgnition& Proposal : Proposals)
        if (IsValid(Proposal.Target) &&
            !Proposal.Target->TryIgnite(Proposal.OperationId, Proposal.Intensity))
            return false;

    for (const FCombustibleEntry& Entry : Entries)
        if (IsValid(Entry.Component))
            Entry.Component->SynchronizeRaidSignalTags();

    return true;
}
