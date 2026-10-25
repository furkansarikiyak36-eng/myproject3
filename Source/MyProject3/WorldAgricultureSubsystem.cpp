#include "WorldAgricultureSubsystem.h"

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
        Animal.ProductUnits < 0 || Animal.Revision <= 0 || Animals.Contains(Animal.AnimalId) ||
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

bool UWorldAgricultureSubsystem::AdvanceAgricultureStep(double DeltaSeconds)
{
    // Bounded catch-up prevents a paused game or malformed caller from applying
    // unbounded growth/hunger in one operation.
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0)
    {
        return false;
    }
    if (!FMath::IsFinite(MinimumGrowthMoisture) || MinimumGrowthMoisture < 0.0f ||
        MinimumGrowthMoisture > 1.0f || !FMath::IsFinite(HungerPerHour) ||
        HungerPerHour < 0.0f || !FMath::IsFinite(ProductHungerLimit) ||
        ProductHungerLimit < 0.0f || ProductHungerLimit > 100.0f)
    {
        return false;
    }

    for (TPair<FGuid, FWorldCropState>& Pair : Crops)
    {
        FWorldCropState& Crop = Pair.Value;
        if (Crop.Stage == ECropGrowthStage::Mature || Crop.Stage == ECropGrowthStage::Withered)
            continue;
        if (Crop.Revision == MAX_int64) return false;
        if (Crop.Moisture < MinimumGrowthMoisture)
        {
            Crop.Stage = ECropGrowthStage::Withered;
            ++Crop.Revision;
            continue;
        }
        Crop.GrowthProgressSeconds += DeltaSeconds * FMath::Clamp(Crop.Moisture, 0.0f, 1.0f);
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
        if (Animal.Revision == MAX_int64) return false;
        Animal.Hunger = FMath::Clamp(Animal.Hunger + HungerPerHour *
            static_cast<float>(DeltaSeconds / 3600.0), 0.0f, 100.0f);
        if (Animal.Hunger >= 80.0f)
            Animal.Health = FMath::Max(0.0f, Animal.Health - static_cast<float>(DeltaSeconds / 3600.0) * 2.0f);
        if (Animal.Health > 0.0f && Animal.Hunger <= ProductHungerLimit &&
            !Animal.ProductDefinitionId.IsNone() && Animal.ProductUnits < MAX_int32)
        {
            // One product unit per simulated hour at a healthy, adequately fed state.
            if (DeltaSeconds >= 3600.0) ++Animal.ProductUnits;
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
