#include "WorldDemoScenarioActor.h"
#include "WorldGameDataSubsystem.h"

#include "Engine/World.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldLogisticsRouteSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "WorldStorageSubsystem.h"
#include "WorldSurvivalSubsystem.h"

AWorldDemoScenarioActor::AWorldDemoScenarioActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AWorldDemoScenarioActor::BeginPlay()
{
    Super::BeginPlay();
    InitializeScenario();
}

bool AWorldDemoScenarioActor::InitializeScenario()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        ScenarioStatus = TEXT("Blocked: no world context.");
        bScenarioReady = false;
        return false;
    }

    UWorldStorageSubsystem* Storage = World->GetSubsystem<UWorldStorageSubsystem>();
    UWorldLogisticsRouteSubsystem* Routes = World->GetSubsystem<UWorldLogisticsRouteSubsystem>();
    UWorldAgricultureSubsystem* Agriculture = World->GetSubsystem<UWorldAgricultureSubsystem>();
    UWorldProductionSubsystem* Production = World->GetSubsystem<UWorldProductionSubsystem>();
    UWorldGameDataSubsystem* GameData = World->GetSubsystem<UWorldGameDataSubsystem>();
    UWorldSurvivalSubsystem* Survival = World->GetSubsystem<UWorldSurvivalSubsystem>();

    if ((bRegisterStorehouse && !Storage) || (bRegisterSettlementRoutes && !Routes) ||
        (bRegisterLogicalTestCrop && !Agriculture) ||
        (bRegisterConfiguredProduction && (!Production || !GameData)) ||
        (bSetDemoWeather && !Survival))
    {
        ScenarioStatus = TEXT("Blocked: one or more required world subsystems are unavailable.");
        bScenarioReady = false;
        return false;
    }

    // Validate all locally knowable configuration before mutating any subsystem.
    if ((bRegisterStorehouse && (!StorehouseId.IsValid() || StorehouseCapacity <= 0)) ||
        (bRegisterSettlementRoutes &&
            (!FMath::IsFinite(CampToTimberRouteCost) || CampToTimberRouteCost < 0.0f ||
             !FMath::IsFinite(TimberToWorkshopRouteCost) || TimberToWorkshopRouteCost < 0.0f)) ||
        (bRegisterLogicalTestCrop &&
            (!TestCropId.IsValid() || TestCropDefinitionId.IsNone() ||
             !FMath::IsFinite(TestCropSecondsPerStage) || TestCropSecondsPerStage <= 0.0 ||
             !FMath::IsFinite(TestCropMoisture) || TestCropMoisture < 0.0f || TestCropMoisture > 1.0f)) ||
        (bSetDemoWeather &&
            (!FMath::IsFinite(DemoWeatherSeverity) || DemoWeatherSeverity < 0.0f || DemoWeatherSeverity > 1.0f)) ||
        (bRegisterConfiguredProduction &&
            (!ProductionStationId.IsValid() || ProductionStationTag.IsNone() || ProductionCapacity <= 0 ||
             ProductionRecipeId.IsNone())))
    {
        ScenarioStatus = TEXT("Blocked: invalid scenario configuration.");
        bScenarioReady = false;
        return false;
    }

    // Registrations are idempotent: restored records are validated and reused,
    // not overwritten with defaults on every BeginPlay.
    if ((bRegisterStorehouse && !EnsureStorage(Storage)) ||
        (bRegisterSettlementRoutes && !EnsureRoutes(Routes)) ||
        (bRegisterLogicalTestCrop && !EnsureCrop(Agriculture)) ||
        (bRegisterConfiguredProduction && !EnsureProduction(Production)))
    {
        ScenarioStatus = TEXT("Blocked: a configured record could not be registered or did not match the existing saved record.");
        bScenarioReady = false;
        return false;
    }

    if (bSetDemoWeather && !Survival->SetWeather(DemoWeather, DemoWeatherSeverity))
    {
        ScenarioStatus = TEXT("Blocked: configured weather was rejected.");
        bScenarioReady = false;
        return false;
    }

    bScenarioReady = true;
    ScenarioStatus = TEXT("Ready: enabled records registered in world subsystems. Visual actors and player interactions must be wired separately.");
    return true;
}

bool AWorldDemoScenarioActor::EnsureStorage(UWorldStorageSubsystem* Storage) const
{
    FWorldStorageContainerRecord Existing;
    if (Storage->GetStorage(StorehouseId, Existing))
    {
        return Existing.CapacityUnits == StorehouseCapacity && Existing.bAvailable;
    }
    return Storage->RegisterStorage(StorehouseId, StorehouseCapacity);
}

bool AWorldDemoScenarioActor::EnsureRoutes(UWorldLogisticsRouteSubsystem* Routes) const
{
    FLogisticsRouteNode Camp;
    Camp.NodeId = TEXT("Demo_Camp");
    Camp.Location = CampRouteLocation;

    FLogisticsRouteNode Timber;
    Timber.NodeId = TEXT("Demo_Timber");
    Timber.Location = TimberRouteLocation;

    FLogisticsRouteNode Workshop;
    Workshop.NodeId = TEXT("Demo_Workshop");
    Workshop.Location = WorkshopRouteLocation;

    if (!Routes->UpsertNode(Camp) || !Routes->UpsertNode(Timber) || !Routes->UpsertNode(Workshop))
    {
        return false;
    }

    FLogisticsRouteEdge CampToTimber;
    CampToTimber.FromNode = Camp.NodeId;
    CampToTimber.ToNode = Timber.NodeId;
    CampToTimber.Cost = CampToTimberRouteCost;

    FLogisticsRouteEdge TimberToWorkshop;
    TimberToWorkshop.FromNode = Timber.NodeId;
    TimberToWorkshop.ToNode = Workshop.NodeId;
    TimberToWorkshop.Cost = TimberToWorkshopRouteCost;

    if (!Routes->UpsertEdge(CampToTimber) || !Routes->UpsertEdge(TimberToWorkshop))
    {
        return false;
    }

    TArray<FName> Path;
    float TotalCost = 0.0f;
    return Routes->FindRoute(Camp.NodeId, Workshop.NodeId, Path, TotalCost) &&
        Path.Num() == 3 && Path[0] == Camp.NodeId && Path[1] == Timber.NodeId &&
        Path[2] == Workshop.NodeId && FMath::IsFinite(TotalCost);
}

bool AWorldDemoScenarioActor::EnsureCrop(UWorldAgricultureSubsystem* Agriculture) const
{
    FWorldCropState Existing;
    if (Agriculture->GetCrop(TestCropId, Existing))
    {
        return Existing.CropDefinitionId == TestCropDefinitionId &&
            Existing.SecondsPerStage == TestCropSecondsPerStage;
    }

    FWorldCropState Crop;
    Crop.CropId = TestCropId;
    Crop.CropDefinitionId = TestCropDefinitionId;
    Crop.PlotCell = FIntVector(0, 0, 0);
    Crop.Stage = ECropGrowthStage::Seed;
    Crop.Moisture = TestCropMoisture;
    Crop.SecondsPerStage = TestCropSecondsPerStage;
    return Agriculture->RegisterCrop(Crop);
}

bool AWorldDemoScenarioActor::EnsureProduction(UWorldProductionSubsystem* Production) const
{
    // Game data is the source of truth. This scenario option may select a recipe
    // by ID, but may not author a parallel set of ingredients/outputs in the level.
    UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    const FGameDataTransformation* DataRecipe =
        GameData ? GameData->FindTransformation(ProductionRecipeId) : nullptr;
    if (!Production || !GameData || !GameData->IsApplied() || !DataRecipe ||
        DataRecipe->bPhysical || DataRecipe->Inputs.IsEmpty() || DataRecipe->Outputs.IsEmpty() ||
        DataRecipe->Where != ProductionStationTag)
    {
        return false;
    }

    FWorldProductionRecipe Recipe;
    Recipe.RecipeId = DataRecipe->Id;
    Recipe.RequiredStationTag = DataRecipe->Where;
    Recipe.DurationSeconds = FMath::Max(1.0, DataRecipe->DurationSeconds);
    auto Ingredient = [](FName ItemId, int32 Quantity)
    {
        FProductionIngredient Result;
        Result.ItemId = ItemId;
        Result.Quantity = Quantity;
        return Result;
    };
    for (const FGameDataItemAmount& Amount : DataRecipe->Inputs)
        Recipe.Inputs.Add(Ingredient(Amount.ItemId, Amount.Quantity));
    for (const FGameDataItemAmount& Amount : DataRecipe->Outputs)
        Recipe.Outputs.Add(Ingredient(Amount.ItemId, Amount.Quantity));

    // Validate the referenced data recipe before mutating the station registry.
    TArray<FWorldProductionStation> Stations;
    TArray<FWorldProductionRecipe> Recipes;
    Production->ExportSnapshot(Stations, Recipes);

    const FWorldProductionRecipe* ExistingRecipe = Recipes.FindByPredicate(
        [&Recipe](const FWorldProductionRecipe& Existing) { return Existing.RecipeId == Recipe.RecipeId; });
    if (ExistingRecipe)
    {
        const auto IngredientsMatch = [](const TArray<FProductionIngredient>& A,
            const TArray<FProductionIngredient>& B)
        {
            if (A.Num() != B.Num()) return false;
            for (int32 Index = 0; Index < A.Num(); ++Index)
            {
                if (A[Index].ItemId != B[Index].ItemId || A[Index].Quantity != B[Index].Quantity)
                    return false;
            }
            return true;
        };
        if (ExistingRecipe->RequiredStationTag != Recipe.RequiredStationTag ||
            !FMath::IsNearlyEqual(ExistingRecipe->DurationSeconds, Recipe.DurationSeconds, 0.001) ||
            !IngredientsMatch(ExistingRecipe->Inputs, Recipe.Inputs) ||
            !IngredientsMatch(ExistingRecipe->Outputs, Recipe.Outputs))
        {
            return false;
        }
    }

    const FWorldProductionStation* ExistingStation = Stations.FindByPredicate(
        [this](const FWorldProductionStation& Station) { return Station.StationId == ProductionStationId; });
    if (ExistingStation)
    {
        if (ExistingStation->StationTag != ProductionStationTag ||
            ExistingStation->StorageCapacityUnits != ProductionCapacity)
        {
            return false;
        }
    }
    else
    {
        FWorldProductionStation Station;
        Station.StationId = ProductionStationId;
        Station.StationTag = ProductionStationTag;
        Station.StorageCapacityUnits = ProductionCapacity;
        if (!Production->RegisterStation(Station))
        {
            return false;
        }
    }

    // Normally ApplyToWorld has already registered this recipe. If not, register
    // only the exact recipe materialized from transformations.json above.
    return ExistingRecipe != nullptr || Production->RegisterRecipe(Recipe);
}
