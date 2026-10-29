#include "WorldAgricultureSubsystem.h"

#include "WorldSurvivalSubsystem.h"

bool UWorldAgricultureSubsystem::RegisterCrop(const FWorldCropState& Crop)
{
    if (!Crop.CropId.IsValid() || Crop.CropDefinitionId.IsNone() ||
        !FMath::IsFinite(Crop.Moisture) || Crop.Moisture < 0.0f || Crop.Moisture > 1.0f ||
        !FMath::IsFinite(Crop.GrowthProgressSeconds) || Crop.GrowthProgressSeconds < 0.0 ||
        !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0 ||
        Crop.Revision <= 0 || Crops.Contains(Crop.CropId) ||
        MaxCrops <= 0 || Crops.Num() >= MaxCrops)
    {
        return false;
    }
    Crops.Add(Crop.CropId, Crop);
    return true;
}

bool UWorldAgricultureSubsystem::RegisterAnimal(const FWorldAnimalState& Animal)
{
    if (!Animal.AnimalId.IsValid() || Animal.AnimalDefinitionId.IsNone() ||
        !FMath::IsFinite(Animal.Hunger) || Animal.Hunger < 0.0f || Animal.Hunger > 100.0f ||
        !FMath::IsFinite(Animal.Health) || Animal.Health < 0.0f || Animal.Health > 100.0f ||
        Animal.ProductUnits < 0 || !FMath::IsFinite(Animal.ProductProgressSeconds) ||
        Animal.ProductProgressSeconds < 0.0 || Animal.ProductProgressSeconds >= 3600.0 || Animal.Revision <= 0 || Animals.Contains(Animal.AnimalId) ||
        MaxAnimals <= 0 || Animals.Num() >= MaxAnimals)
    {
        return false;
    }
    Animals.Add(Animal.AnimalId, Animal);
    return true;
}

bool UWorldAgricultureSubsystem::SetCropMoisture(
    const FGuid& CropId, float Moisture, int64 ExpectedRevision)
{
    FWorldCropState* Crop = Crops.Find(CropId);
    if (!Crop || Crop->Revision != ExpectedRevision || Crop->Revision == MAX_int64 ||
        !FMath::IsFinite(Moisture) || Moisture < 0.0f || Moisture > 1.0f)
    {
        return false;
    }
    Crop->Moisture = Moisture;
    ++Crop->Revision;
    return true;
}

bool UWorldAgricultureSubsystem::FeedAnimal(
    const FGuid& AnimalId, float HungerReduction, int64 ExpectedRevision)
{
    FWorldAnimalState* Animal = Animals.Find(AnimalId);
    if (!Animal || Animal->Revision != ExpectedRevision || Animal->Revision == MAX_int64 ||
        !FMath::IsFinite(HungerReduction) || HungerReduction <= 0.0f)
    {
        return false;
    }
    Animal->Hunger = FMath::Clamp(Animal->Hunger - HungerReduction, 0.0f, 100.0f);
    ++Animal->Revision;
    return true;
}

bool UWorldAgricultureSubsystem::CanAdvanceAgricultureStep(double DeltaSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0 ||
        !FMath::IsFinite(MinimumGrowthMoisture) || MinimumGrowthMoisture < 0.0f ||
        MinimumGrowthMoisture > 1.0f || !FMath::IsFinite(HungerPerHour) ||
        HungerPerHour < 0.0f || !FMath::IsFinite(ProductHungerLimit) ||
        ProductHungerLimit < 0.0f || ProductHungerLimit > 100.0f ||
        !FMath::IsFinite(RainGrowthMultiplier) || RainGrowthMultiplier < 0.0f || RainGrowthMultiplier > 2.0f ||
        !FMath::IsFinite(WindGrowthMultiplier) || WindGrowthMultiplier < 0.0f || WindGrowthMultiplier > 2.0f ||
        !FMath::IsFinite(SnowGrowthMultiplier) || SnowGrowthMultiplier < 0.0f || SnowGrowthMultiplier > 2.0f ||
        !FMath::IsFinite(StormGrowthMultiplier) || StormGrowthMultiplier < 0.0f || StormGrowthMultiplier > 2.0f ||
        !FMath::IsFinite(HeatwaveGrowthMultiplier) || HeatwaveGrowthMultiplier < 0.0f || HeatwaveGrowthMultiplier > 2.0f)
        return false;

    for (const TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        const FWorldCropState& Crop = Pair.Value;
        if (Crop.Revision == MAX_int64 ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds) ||
            !FMath::IsFinite(Crop.SecondsPerStage) || Crop.SecondsPerStage <= 0.0 ||
            !FMath::IsFinite(Crop.GrowthProgressSeconds + DeltaSeconds))
            return false;
    }
    for (const TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        const FWorldAnimalState& Animal = Pair.Value;
        if (Animal.Revision == MAX_int64 ||
            !FMath::IsFinite(Animal.Hunger) || Animal.Hunger < 0.0f || Animal.Hunger > 100.0f ||
            !FMath::IsFinite(Animal.Health) || Animal.Health < 0.0f || Animal.Health > 100.0f ||
            !FMath::IsFinite(Animal.ProductProgressSeconds) ||
            Animal.ProductProgressSeconds < 0.0 || Animal.ProductProgressSeconds >= 3600.0 ||
            !FMath::IsFinite(Animal.ProductProgressSeconds + DeltaSeconds) ||
            (Animal.Health > 0.0f && Animal.Hunger <= ProductHungerLimit &&
             !Animal.ProductDefinitionId.IsNone() &&
             Animal.ProductProgressSeconds + DeltaSeconds >= 3600.0 &&
             Animal.ProductUnits == MAX_int32))
            return false;
    }
    return true;
}

bool UWorldAgricultureSubsystem::AdvanceAgricultureStep(double DeltaSeconds)
{
    if (!CanAdvanceAgricultureStep(DeltaSeconds)) return false;

    const UWorldSurvivalSubsystem* Survival =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!Survival ||
        static_cast<uint8>(Survival->Weather) > static_cast<uint8>(EWorldWeatherState::Heatwave) ||
        !FMath::IsFinite(Survival->WeatherSeverity) ||
        Survival->WeatherSeverity < 0.0f || Survival->WeatherSeverity > 1.0f)
        return false;

    float FullSeverityMultiplier = 1.0f;
    switch (Survival->Weather)
    {
    case EWorldWeatherState::Rain:     FullSeverityMultiplier = RainGrowthMultiplier; break;
    case EWorldWeatherState::Wind:     FullSeverityMultiplier = WindGrowthMultiplier; break;
    case EWorldWeatherState::Snow:     FullSeverityMultiplier = SnowGrowthMultiplier; break;
    case EWorldWeatherState::Storm:    FullSeverityMultiplier = StormGrowthMultiplier; break;
    case EWorldWeatherState::Heatwave: FullSeverityMultiplier = HeatwaveGrowthMultiplier; break;
    case EWorldWeatherState::Clear:
    default: break;
    }
    const double WeatherGrowthMultiplier =
        FMath::Lerp(1.0, static_cast<double>(FullSeverityMultiplier),
            static_cast<double>(Survival->WeatherSeverity));
    if (!FMath::IsFinite(WeatherGrowthMultiplier) || WeatherGrowthMultiplier < 0.0 ||
        WeatherGrowthMultiplier > 2.0)
        return false;

    for (TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        FWorldCropState& Crop = Pair.Value;
        if (Crop.Stage == ECropGrowthStage::Mature || Crop.Stage == ECropGrowthStage::Withered)
            continue;
        if (Crop.Moisture < MinimumGrowthMoisture)
        {
            Crop.Stage = ECropGrowthStage::Withered;
            ++Crop.Revision;
            continue;
        }
        Crop.GrowthProgressSeconds += DeltaSeconds *
            static_cast<double>(FMath::Clamp(Crop.Moisture, 0.0f, 1.0f)) *
            WeatherGrowthMultiplier;
        while (Crop.GrowthProgressSeconds >= Crop.SecondsPerStage &&
            Crop.Stage != ECropGrowthStage::Mature)
        {
            Crop.GrowthProgressSeconds -= Crop.SecondsPerStage;
            Crop.Stage = static_cast<ECropGrowthStage>(static_cast<uint8>(Crop.Stage) + 1);
        }
        ++Crop.Revision;
    }

    for (TPair<FGuid, FWorldAnimalState>& Pair : Animals)
    {
        FWorldAnimalState& Animal = Pair.Value;
        Animal.Hunger = FMath::Clamp(Animal.Hunger + HungerPerHour *
            static_cast<float>(DeltaSeconds / 3600.0), 0.0f, 100.0f);
        if (Animal.Hunger >= 80.0f)
            Animal.Health = FMath::Max(0.0f, Animal.Health - static_cast<float>(DeltaSeconds / 3600.0) * 2.0f);
        if (Animal.Health > 0.0f && Animal.Hunger <= ProductHungerLimit &&
            !Animal.ProductDefinitionId.IsNone())
        {
            Animal.ProductProgressSeconds += DeltaSeconds;
            // DeltaSeconds is capped at one hour and progress is kept in [0, 3600),
            // so at most one product unit can complete per step. Avoid a data-driven
            // while-loop whose iteration count could be controlled by a corrupt snapshot.
            if (Animal.ProductProgressSeconds >= 3600.0)
            {
                Animal.ProductProgressSeconds -= 3600.0;
                ++Animal.ProductUnits;
            }
        }
        ++Animal.Revision;
    }
    return true;
}

bool UWorldAgricultureSubsystem::GetCrop(const FGuid& CropId, FWorldCropState& OutCrop) const
{
    const FWorldCropState* Found = Crops.Find(CropId);
    if (!Found) return false;
    OutCrop = *Found;
    return true;
}

bool UWorldAgricultureSubsystem::GetAnimal(const FGuid& AnimalId, FWorldAnimalState& OutAnimal) const
{
    const FWorldAnimalState* Found = Animals.Find(AnimalId);
    if (!Found) return false;
    OutAnimal = *Found;
    return true;
}

void UWorldAgricultureSubsystem::ExportSnapshot(
    TArray<FWorldCropState>& OutCrops, TArray<FWorldAnimalState>& OutAnimals) const
{
    Crops.GenerateValueArray(OutCrops);
    Animals.GenerateValueArray(OutAnimals);
    OutCrops.Sort([](const FWorldCropState& A, const FWorldCropState& B)
        { return A.CropId.ToString() < B.CropId.ToString(); });
    OutAnimals.Sort([](const FWorldAnimalState& A, const FWorldAnimalState& B)
        { return A.AnimalId.ToString() < B.AnimalId.ToString(); });
}

bool UWorldAgricultureSubsystem::RestoreSnapshot(
    const TArray<FWorldCropState>& CropSnapshot, const TArray<FWorldAnimalState>& AnimalSnapshot)
{
    if (CropSnapshot.Num() > MaxCrops || AnimalSnapshot.Num() > MaxAnimals ||
        MaxCrops <= 0 || MaxAnimals <= 0) return false;
    TMap<FGuid, FWorldCropState> NewCrops;
    TMap<FGuid, FWorldAnimalState> NewAnimals;
    for (const FWorldCropState& C : CropSnapshot)
    {
        if (!C.CropId.IsValid() || C.CropDefinitionId.IsNone() || C.Revision <= 0 ||
            !FMath::IsFinite(C.Moisture) || C.Moisture < 0.0f || C.Moisture > 1.0f ||
            !FMath::IsFinite(C.GrowthProgressSeconds) || C.GrowthProgressSeconds < 0.0 ||
            !FMath::IsFinite(C.SecondsPerStage) || C.SecondsPerStage <= 0.0 ||
            NewCrops.Contains(C.CropId)) return false;
        NewCrops.Add(C.CropId, C);
    }
    for (const FWorldAnimalState& A : AnimalSnapshot)
    {
        if (!A.AnimalId.IsValid() || A.AnimalDefinitionId.IsNone() || A.Revision <= 0 ||
            !FMath::IsFinite(A.Hunger) || A.Hunger < 0.0f || A.Hunger > 100.0f ||
            !FMath::IsFinite(A.Health) || A.Health < 0.0f || A.Health > 100.0f ||
            A.ProductUnits < 0 || !FMath::IsFinite(A.ProductProgressSeconds) || A.ProductProgressSeconds < 0.0 || A.ProductProgressSeconds >= 3600.0 || NewAnimals.Contains(A.AnimalId)) return false;
        NewAnimals.Add(A.AnimalId, A);
    }
    Crops = MoveTemp(NewCrops);
    Animals = MoveTemp(NewAnimals);
    return true;
}
