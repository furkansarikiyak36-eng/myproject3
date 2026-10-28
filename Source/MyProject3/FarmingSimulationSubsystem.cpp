#include "FarmingSimulationSubsystem.h"

FWorldOperationResult UFarmingSimulationSubsystem::Failure(EWorldOperationResultCode Code,
    const FGuid& OperationId, const TCHAR* Message) const
{
    return FWorldOperationResult::Failure(Code, OperationId, FText::FromString(Message));
}

FWorldOperationResult UFarmingSimulationSubsystem::RegisterCropDefinition(
    const FCropDefinition& Definition, FGuid OperationId)
{
    if (Definition.DefinitionId.IsNone() || Definition.TicksToMature <= 0 || Definition.BaseYield <= 0 ||
        !FMath::IsFinite(Definition.MinSoilMoisture) || !FMath::IsFinite(Definition.MaxSoilMoisture) ||
        !FMath::IsFinite(Definition.MinNutrients) || Definition.MinSoilMoisture < 0.0f ||
        Definition.MaxSoilMoisture > 1.0f || Definition.MinSoilMoisture > Definition.MaxSoilMoisture ||
        Definition.MaxSoilMoisture < 0.0f || Definition.MinNutrients < 0.0f)
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId, TEXT("Crop definition has invalid IDs, limits or yield."));
    if (CropDefinitions.Contains(Definition.DefinitionId))
        return Failure(EWorldOperationResultCode::Conflict, OperationId, TEXT("Crop definition already exists."));
    CropDefinitions.Add(Definition.DefinitionId, Definition);
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Crop definition registered.")), {});
}

FWorldOperationResult UFarmingSimulationSubsystem::RegisterAnimalDefinition(
    const FAnimalDefinition& Definition, FGuid OperationId)
{
    if (Definition.DefinitionId.IsNone() || Definition.FeedResourceId.IsNone() ||
        Definition.TicksToMature <= 0 || Definition.ReproductionTicks <= 0 ||
        !FMath::IsFinite(Definition.FeedPerTick) || Definition.FeedPerTick <= 0.0f ||
        !FMath::IsFinite(Definition.ProductPerTick) || Definition.ProductPerTick < 0.0f ||
        !FMath::IsFinite(Definition.ManureNutrientsPerTick) || Definition.ManureNutrientsPerTick < 0.0f)
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId, TEXT("Animal definition has invalid IDs, tick limits or rates."));
    if (AnimalDefinitions.Contains(Definition.DefinitionId))
        return Failure(EWorldOperationResultCode::Conflict, OperationId, TEXT("Animal definition already exists."));
    AnimalDefinitions.Add(Definition.DefinitionId, Definition);
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Animal definition registered.")), {});
}

FWorldOperationResult UFarmingSimulationSubsystem::SetSeason(FName SeasonId, FGuid OperationId)
{
    if (SeasonId.IsNone())
        return Failure(EWorldOperationResultCode::InvalidState, OperationId, TEXT("Season ID must not be empty."));
    CurrentSeason = SeasonId;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Simulation season updated.")), {});
}

FWorldOperationResult UFarmingSimulationSubsystem::TryPlantCrop(FName DefinitionId,
    float SoilMoisture, float Nutrients, FGuid& OutCropId, FGuid OperationId)
{
    const FCropDefinition* Definition = CropDefinitions.Find(DefinitionId);
    if (!Definition)
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId, TEXT("Crop definition is not registered."));
    if (!FMath::IsFinite(SoilMoisture) || !FMath::IsFinite(Nutrients) ||
        SoilMoisture < 0.0f || SoilMoisture > 1.0f || Nutrients < 0.0f)
        return Failure(EWorldOperationResultCode::InvalidState, OperationId, TEXT("Soil inputs are outside valid ranges."));
    if (!Definition->RequiredSeason.IsNone() && Definition->RequiredSeason != CurrentSeason)
        return Failure(EWorldOperationResultCode::PrerequisiteNotMet, OperationId, TEXT("Current season does not satisfy the crop definition."));
    if (SoilMoisture < Definition->MinSoilMoisture || SoilMoisture > Definition->MaxSoilMoisture ||
        Nutrients < Definition->MinNutrients)
        return Failure(EWorldOperationResultCode::PrerequisiteNotMet, OperationId, TEXT("Soil moisture or nutrient level is unsuitable for planting."));

    FCropRuntimeState State;
    State.CropId = FGuid::NewGuid();
    State.DefinitionId = DefinitionId;
    State.SoilMoisture = SoilMoisture;
    State.Nutrients = Nutrients;
    OutCropId = State.CropId;
    Crops.Add(State.CropId, State);
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Crop planted.")), {State.CropId});
}

FWorldOperationResult UFarmingSimulationSubsystem::TryWaterCrop(FGuid CropId, float WaterAmount, FGuid OperationId)
{
    FCropRuntimeState* Crop = Crops.Find(CropId);
    if (!Crop || Crop->Stage == ECropGrowthStage::Harvested)
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Crop is missing or already harvested."));
    if (!FMath::IsFinite(WaterAmount) || WaterAmount <= 0.0f)
        return Failure(EWorldOperationResultCode::InvalidState, OperationId, TEXT("Water amount must be finite and positive."));
    Crop->SoilMoisture = FMath::Clamp(Crop->SoilMoisture + WaterAmount, 0.0f, 1.0f);
    ++Crop->Revision;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Crop moisture updated.")), {CropId});
}

FWorldOperationResult UFarmingSimulationSubsystem::TryAddCropNutrients(FGuid CropId, float NutrientAmount, FGuid OperationId)
{
    FCropRuntimeState* Crop = Crops.Find(CropId);
    if (!Crop || Crop->Stage == ECropGrowthStage::Harvested)
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Crop is missing or already harvested."));
    if (!FMath::IsFinite(NutrientAmount) || NutrientAmount <= 0.0f)
        return Failure(EWorldOperationResultCode::InvalidState, OperationId, TEXT("Nutrient amount must be finite and positive."));
    Crop->Nutrients += NutrientAmount;
    ++Crop->Revision;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Crop nutrients updated.")), {CropId});
}

FWorldOperationResult UFarmingSimulationSubsystem::TryHarvestCrop(FGuid CropId, int32& OutYield, FGuid OperationId)
{
    FCropRuntimeState* Crop = Crops.Find(CropId);
    if (!Crop || Crop->Stage != ECropGrowthStage::Mature)
        return Failure(EWorldOperationResultCode::PrerequisiteNotMet, OperationId, TEXT("Only mature crops can be harvested."));
    const FCropDefinition* Definition = CropDefinitions.Find(Crop->DefinitionId);
    if (!Definition)
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId, TEXT("Crop definition is missing."));
    const float MoistureFactor = FMath::Clamp(Crop->SoilMoisture, 0.25f, 1.0f);
    const float NutrientFactor = FMath::Clamp(Crop->Nutrients, 0.25f, 1.0f);
    const int32 Yield = FMath::Max(1, FMath::FloorToInt(Definition->BaseYield * MoistureFactor * NutrientFactor * (1.0f - Crop->Stress)));
    Crop->Stage = ECropGrowthStage::Harvested;
    ++Crop->Revision;
    OutYield = Yield;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Crop harvested; yield calculated.")), {CropId});
}

FWorldOperationResult UFarmingSimulationSubsystem::TryRegisterAnimal(FName DefinitionId, int32 HabitatCapacity,
    FGuid& OutAnimalId, FGuid OperationId)
{
    if (!AnimalDefinitions.Contains(DefinitionId))
        return Failure(EWorldOperationResultCode::MissingConfiguration, OperationId, TEXT("Animal definition is not registered."));
    if (HabitatCapacity <= 0)
        return Failure(EWorldOperationResultCode::CapacityExceeded, OperationId, TEXT("Habitat capacity must be positive."));
    int32 SameSpecies = 0;
    for (const TPair<FGuid, FAnimalRuntimeState>& Pair : Animals)
        if (Pair.Value.bAlive && Pair.Value.DefinitionId == DefinitionId) ++SameSpecies;
    if (SameSpecies >= HabitatCapacity)
        return Failure(EWorldOperationResultCode::CapacityExceeded, OperationId, TEXT("Habitat capacity for this species is full."));
    FAnimalRuntimeState State;
    State.AnimalId = FGuid::NewGuid();
    State.DefinitionId = DefinitionId;
    OutAnimalId = State.AnimalId;
    Animals.Add(State.AnimalId, State);
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Animal registered in habitat.")), {State.AnimalId});
}

FWorldOperationResult UFarmingSimulationSubsystem::TryFeedAnimal(FGuid AnimalId, float FeedAmount, FGuid OperationId)
{
    FAnimalRuntimeState* Animal = Animals.Find(AnimalId);
    if (!Animal || !Animal->bAlive)
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Animal is missing or dead."));
    if (!FMath::IsFinite(FeedAmount) || FeedAmount <= 0.0f)
        return Failure(EWorldOperationResultCode::InvalidState, OperationId, TEXT("Feed amount must be finite and positive."));
    Animal->FeedReserve += FeedAmount;
    ++Animal->Revision;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Animal feed reserve increased.")), {AnimalId});
}

FWorldOperationResult UFarmingSimulationSubsystem::TryCollectAnimalProducts(FGuid AnimalId, int32& OutUnits, FGuid OperationId)
{
    FAnimalRuntimeState* Animal = Animals.Find(AnimalId);
    if (!Animal || !Animal->bAlive)
        return Failure(EWorldOperationResultCode::InvalidTarget, OperationId, TEXT("Animal is missing or dead."));
    if (Animal->ProductUnitsReady <= 0)
        return Failure(EWorldOperationResultCode::PrerequisiteNotMet, OperationId, TEXT("No animal products are ready."));
    OutUnits = Animal->ProductUnitsReady;
    Animal->ProductUnitsReady = 0;
    ++Animal->Revision;
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Animal products collected from simulation state.")), {AnimalId});
}

void UFarmingSimulationSubsystem::AdvanceOneTick()
{
    ++SimulationTick;
    WorldTimeSeconds += FixedTickSeconds;

    for (TPair<FGuid, FCropRuntimeState>& Pair : Crops)
    {
        FCropRuntimeState& Crop = Pair.Value;
        if (Crop.Stage == ECropGrowthStage::Harvested) continue;
        const FCropDefinition* Definition = CropDefinitions.Find(Crop.DefinitionId);
        if (!Definition) continue;

        const bool bSeasonValid = Definition->RequiredSeason.IsNone() || Definition->RequiredSeason == CurrentSeason;
        const bool bSoilValid = Crop.SoilMoisture >= Definition->MinSoilMoisture &&
            Crop.SoilMoisture <= Definition->MaxSoilMoisture && Crop.Nutrients >= Definition->MinNutrients;
        if (!bSeasonValid || !bSoilValid)
            Crop.Stress = FMath::Clamp(Crop.Stress + 0.1f, 0.0f, 0.95f);
        else
        {
            Crop.Stress = FMath::Max(0.0f, Crop.Stress - 0.05f);
            ++Crop.GrowthTicks;
        }
        if (Crop.GrowthTicks >= Definition->TicksToMature) Crop.Stage = ECropGrowthStage::Mature;
        else if (Crop.GrowthTicks >= Definition->TicksToMature * 2 / 3) Crop.Stage = ECropGrowthStage::Growing;
        else if (Crop.GrowthTicks >= Definition->TicksToMature / 3) Crop.Stage = ECropGrowthStage::Sprout;
        ++Crop.Revision;
    }

    for (TPair<FGuid, FAnimalRuntimeState>& Pair : Animals)
    {
        FAnimalRuntimeState& Animal = Pair.Value;
        if (!Animal.bAlive) continue;
        const FAnimalDefinition* Definition = AnimalDefinitions.Find(Animal.DefinitionId);
        if (!Definition) continue;
        if (Animal.FeedReserve >= Definition->FeedPerTick)
        {
            Animal.FeedReserve -= Definition->FeedPerTick;
            Animal.Health = FMath::Min(100.0f, Animal.Health + 0.5f);
            ++Animal.GrowthTicks;
            if (!Definition->ProductResourceId.IsNone() && Definition->ProductPerTick > 0.0f &&
                Animal.GrowthTicks >= Definition->TicksToMature)
                Animal.ProductUnitsReady += FMath::FloorToInt(Definition->ProductPerTick);
            if (Animal.GrowthTicks >= Definition->TicksToMature)
                ++Animal.ReproductionProgress;
            AccumulatedManureNutrients += Definition->ManureNutrientsPerTick;
        }
        else
        {
            Animal.Health = FMath::Max(0.0f, Animal.Health - 2.0f);
            if (Animal.Health <= 0.0f) Animal.bAlive = false;
        }
        ++Animal.Revision;
    }
}

FWorldOperationResult UFarmingSimulationSubsystem::AdvanceSimulationTicks(int32 TickCount, FGuid OperationId)
{
    if (TickCount <= 0 || TickCount > 100000)
        return Failure(EWorldOperationResultCode::OutOfRange, OperationId, TEXT("Tick count must be between 1 and 100000."));
    for (int32 Index = 0; Index < TickCount; ++Index) AdvanceOneTick();
    return FWorldOperationResult::Success(OperationId, FText::FromString(TEXT("Deterministic simulation ticks advanced.")), {});
}

bool UFarmingSimulationSubsystem::GetCropState(FGuid CropId, FCropRuntimeState& OutState) const
{
    const FCropRuntimeState* State = Crops.Find(CropId);
    if (!State) return false;
    OutState = *State;
    return true;
}

bool UFarmingSimulationSubsystem::GetAnimalState(FGuid AnimalId, FAnimalRuntimeState& OutState) const
{
    const FAnimalRuntimeState* State = Animals.Find(AnimalId);
    if (!State) return false;
    OutState = *State;
    return true;
}

FFarmingSimulationSnapshot UFarmingSimulationSubsystem::GetSnapshot() const
{
    FFarmingSimulationSnapshot Snapshot;
    Snapshot.SimulationTick = SimulationTick;
    Snapshot.WorldTimeSeconds = WorldTimeSeconds;
    Snapshot.CurrentSeason = CurrentSeason;
    Snapshot.AccumulatedManureNutrients = AccumulatedManureNutrients;
    return Snapshot;
}