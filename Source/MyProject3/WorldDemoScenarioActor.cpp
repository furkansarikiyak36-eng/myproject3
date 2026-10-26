#include "WorldDemoScenarioActor.h"

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
    UWorldSurvivalSubsystem* Survival = World->GetSubsystem<UWorldSurvivalSubsystem>();

    if ((bRegisterStorehouse && !Storage) || (bRegisterSettlementRoutes && !Routes) ||
        (bRegisterLogicalTestCrop && !Agriculture) ||
        (bRegisterConfiguredProduction && !Production) ||
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
             ConfiguredRecipe.RecipeId.IsNone() || ConfiguredRecipe.Inputs.IsEmpty() ||
             ConfiguredRecipe.Outputs.IsEmpty() || !FMath::IsFinite(ConfiguredRecipe.DurationSeconds) ||
             ConfiguredRecipe.DurationSeconds <= 0.0)))
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
    TArray<FWorldProductionStation> Stations;
    TArray<FWorldProductionRecipe> Recipes;
    Production->ExportSnapshot(Stations, Recipes);

    const FWorldProductionStation* ExistingStation = Stations.FindByPredicate(
        [this](const FWorldProductionStation& S) { return S.StationId == ProductionStationId; });
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

    FWorldProductionRecipe Recipe = ConfiguredRecipe;
    if (Recipe.RequiredStationTag.IsNone())
    {
        Recipe.RequiredStationTag = ProductionStationTag;
    }

    const FWorldProductionRecipe* ExistingRecipe = Recipes.FindByPredicate(
        [&Recipe](const FWorldProductionRecipe& R) { return R.RecipeId == Recipe.RecipeId; });
    if (ExistingRecipe)
    {
        // Do not silently mutate a recipe restored from save. Compare ingredient
        // identities and quantities, not just array lengths.
        const auto IngredientsMatch = [](const TArray<FProductionIngredient>& A,
            const TArray<FProductionIngredient>& B)
        {
            if (A.Num() != B.Num()) return false;
            for (int32 Index = 0; Index < A.Num(); ++Index)
            {
                if (A[Index].ItemId != B[Index].ItemId ||
                    A[Index].Quantity != B[Index].Quantity) return false;
            }
            return true;
        };

        return ExistingRecipe->DurationSeconds == Recipe.DurationSeconds &&
            ExistingRecipe->RequiredStationTag == Recipe.RequiredStationTag &&
            IngredientsMatch(ExistingRecipe->Inputs, Recipe.Inputs) &&
            IngredientsMatch(ExistingRecipe->Outputs, Recipe.Outputs);
    }

    return Production->RegisterRecipe(Recipe);
}
