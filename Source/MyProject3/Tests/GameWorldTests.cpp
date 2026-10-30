// Tests for the game plan's world systems (calendar, climate, needs ...).
// Plan: Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md, tasks: memory/ROADMAP.md.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "WorldCalendar.h"
#include "WorldClimateSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldItemOwnershipSubsystem.h"
#include "InventoryCraftingComponent.h"
#include "WorldStorageSubsystem.h"
#include "HeatSourceComponent.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldLogisticsRouteSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "GatherableResourceActor.h"
#include "ProductionStationActor.h"
#include "VoxelTerrainChunkComponent.h"
#include "WorldProductionSubsystem.h"
#include "WorldTerrainStateSubsystem.h"
#include "WorldRegionalEventSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldThreatDirectorSubsystem.h"
#include "WorldRaidAdapterSubsystem.h"
#include "WorldOperationLedgerSubsystem.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WorldSimulationClockSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace GameWorldTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    constexpr double Day = 86400.0;

    struct FTestWorld
    {
        UWorld* World = nullptr;

        FTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GameWorldTestWorld"));
            if (!World)
                return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            if (!World->HasBegunPlay() && World->GetWorldSettings())
                World->GetWorldSettings()->NotifyBeginPlay();
        }

        ~FTestWorld()
        {
            if (!World)
                return;
            if (World->HasBegunPlay())
                World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    };
}

using namespace GameWorldTests;

// P1.1: the game starts on the last day of March, year 1, at 08:00; 5 days a month.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCalendarStartAndRolloverTest,
    "MyProject3.World.Calendar.StartDateAndRollovers", Flags)
bool FCalendarStartAndRolloverTest::RunTest(const FString& Parameters)
{
    const FWorldCalendarConfig Config;
    FWorldCalendarDate Date;

    TestTrue(TEXT("Start date"), WorldCalendar::DateFromTime(0.0, Config, Date));
    TestEqual(TEXT("Start year"), Date.Year, 1);
    TestEqual(TEXT("Start month is March"), Date.Month, 3);
    TestEqual(TEXT("Start day is the last of March"), Date.DayOfMonth, 5);
    TestEqual(TEXT("Start hour"), Date.HourOfDay, 8.0f);
    TestTrue(TEXT("March is spring"), Date.Season == EWorldSeason::Spring);
    TestEqual(TEXT("No day has passed"), Date.DaysElapsed, int64(0));

    // 16:00 the same day: still day 0.
    TestTrue(TEXT("Same day"), WorldCalendar::DateFromTime(8.0 * 3600.0, Config, Date));
    TestEqual(TEXT("Same day of month"), Date.DayOfMonth, 5);
    TestEqual(TEXT("Afternoon"), Date.HourOfDay, 16.0f);

    // Midnight rolls March 5 into April 1.
    TestTrue(TEXT("Next day"), WorldCalendar::DateFromTime(16.0 * 3600.0, Config, Date));
    TestEqual(TEXT("Month rollover"), Date.Month, 4);
    TestEqual(TEXT("First of April"), Date.DayOfMonth, 1);
    TestEqual(TEXT("Midnight"), Date.HourOfDay, 0.0f);
    TestEqual(TEXT("One day passed"), Date.DaysElapsed, int64(1));

    // Start is day index 14 of a 60-day year, so 46 midnights later it is January 1 of year 2.
    const double NewYear = 16.0 * 3600.0 + 45.0 * Day;
    TestTrue(TEXT("New year"), WorldCalendar::DateFromTime(NewYear, Config, Date));
    TestEqual(TEXT("Year rollover"), Date.Year, 2);
    TestEqual(TEXT("January"), Date.Month, 1);
    TestEqual(TEXT("First day"), Date.DayOfMonth, 1);
    TestEqual(TEXT("Day of year"), Date.DayOfYear, 1);
    TestTrue(TEXT("January is winter"), Date.Season == EWorldSeason::Winter);

    // The day before is December 31 (day 5 of month 12).
    TestTrue(TEXT("Old year's last day"), WorldCalendar::DateFromTime(NewYear - 1.0, Config, Date));
    TestEqual(TEXT("Still year 1"), Date.Year, 1);
    TestEqual(TEXT("December"), Date.Month, 12);
    TestEqual(TEXT("Last day of December"), Date.DayOfMonth, 5);
    TestEqual(TEXT("Day 60 of the year"), Date.DayOfYear, 60);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCalendarSeasonsAndConfigTest,
    "MyProject3.World.Calendar.SeasonsAndInvalidConfig", Flags)
bool FCalendarSeasonsAndConfigTest::RunTest(const FString& Parameters)
{
    const EWorldSeason Expected[12] = {
        EWorldSeason::Winter, EWorldSeason::Winter, EWorldSeason::Spring, EWorldSeason::Spring,
        EWorldSeason::Spring, EWorldSeason::Summer, EWorldSeason::Summer, EWorldSeason::Summer,
        EWorldSeason::Autumn, EWorldSeason::Autumn, EWorldSeason::Autumn, EWorldSeason::Winter };
    for (int32 Month = 1; Month <= 12; ++Month)
        TestTrue(FString::Printf(TEXT("Season of month %d"), Month), WorldCalendar::SeasonOfMonth(Month) == Expected[Month - 1]);

    FWorldCalendarDate Date;
    FWorldCalendarConfig Bad;
    Bad.DaysPerMonth = 0;
    TestFalse(TEXT("Zero days a month is refused"), WorldCalendar::DateFromTime(0.0, Bad, Date));
    Bad = FWorldCalendarConfig();
    Bad.StartDayOfMonth = 6;
    TestFalse(TEXT("Start day beyond the month is refused"), WorldCalendar::DateFromTime(0.0, Bad, Date));
    TestFalse(TEXT("Negative time is refused"), WorldCalendar::DateFromTime(-1.0, FWorldCalendarConfig(), Date));
    TestFalse(TEXT("NaN time is refused"), WorldCalendar::DateFromTime(NAN, FWorldCalendarConfig(), Date));

    // A longer month setting works the same way.
    FWorldCalendarConfig Long;
    Long.DaysPerMonth = 30;
    Long.StartDayOfMonth = 30;
    TestTrue(TEXT("30-day months"), WorldCalendar::DateFromTime(16.0 * 3600.0, Long, Date));
    TestEqual(TEXT("30-day month rolls into April"), Date.Month, 4);
    return true;
}

// The date comes from the saved simulation time, so a clock restore restores the date.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCalendarClockSaveTest,
    "MyProject3.World.Calendar.ClockRestoreKeepsDate", Flags)
bool FCalendarClockSaveTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorldSimulationClockSubsystem* Clock =
        TestWorld.World ? TestWorld.World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock))
        return false;

    FWorldCalendarDate Date;
    TestFalse(TEXT("No date before the clock starts"), Clock->GetCalendarDate(Date));
    TestTrue(TEXT("Start clock"), Clock->InitializeClock(7));
    TestTrue(TEXT("Advance 20 days"), Clock->AdvanceSimulation(20.0 * Day));
    FWorldCalendarDate Before;
    TestTrue(TEXT("Date before save"), Clock->GetCalendarDate(Before));
    const FWorldSimulationClockSnapshot Saved = Clock->ExportSnapshot();

    TestTrue(TEXT("Advance further"), Clock->AdvanceSimulation(10.0 * Day));
    TestTrue(TEXT("Restore"), Clock->RestoreSnapshot(Saved));
    FWorldCalendarDate After;
    TestTrue(TEXT("Date after load"), Clock->GetCalendarDate(After));
    TestEqual(TEXT("Same year"), After.Year, Before.Year);
    TestEqual(TEXT("Same month"), After.Month, Before.Month);
    TestEqual(TEXT("Same day"), After.DayOfMonth, Before.DayOfMonth);
    TestEqual(TEXT("20 days after the end of March is in July"), Before.Month, 7);
    return true;
}

// P1.2: each month only rolls weather that fits it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimateMonthWeatherTest,
    "MyProject3.World.Climate.MonthsRollFittingWeather", Flags)
bool FClimateMonthWeatherTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorldClimateSubsystem* Climate = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Climate"), Climate))
        return false;

    int32 JanuaryHeat = 0, JulySnow = 0, JanuarySnowOrStorm = 0, JulyDry = 0;
    for (int32 Seed = 1; Seed <= 20; ++Seed)
        for (int64 DayIndex = 0; DayIndex < 30; ++DayIndex)
        {
            EWorldWeatherState Weather;
            float Severity = -1.0f;
            TestTrue(TEXT("Roll January"), Climate->RollWeatherForDay(Seed, DayIndex, 1, Weather, Severity));
            JanuaryHeat += Weather == EWorldWeatherState::Heatwave ? 1 : 0;
            JanuarySnowOrStorm += (Weather == EWorldWeatherState::Snow || Weather == EWorldWeatherState::Storm) ? 1 : 0;
            TestTrue(TEXT("Severity in range"), Severity >= 0.0f && Severity <= 1.0f);
            TestTrue(TEXT("Roll July"), Climate->RollWeatherForDay(Seed, DayIndex, 7, Weather, Severity));
            JulySnow += Weather == EWorldWeatherState::Snow ? 1 : 0;
            JulyDry += (Weather == EWorldWeatherState::Clear || Weather == EWorldWeatherState::Wind ||
                Weather == EWorldWeatherState::Heatwave) ? 1 : 0;
        }
    TestEqual(TEXT("January never has a heatwave"), JanuaryHeat, 0);
    TestEqual(TEXT("July never snows"), JulySnow, 0);
    TestTrue(TEXT("January often snows"), JanuarySnowOrStorm > 150);
    TestTrue(TEXT("July is mostly dry"), JulyDry > 450);

    EWorldWeatherState A, B;
    float SA, SB;
    Climate->RollWeatherForDay(42, 10, 4, A, SA);
    Climate->RollWeatherForDay(42, 10, 4, B, SB);
    TestTrue(TEXT("Same seed and day give the same weather"), A == B && SA == SB);

    TestEqual(TEXT("Coldest at 05:00"), Climate->GetBaseTemperature(1, 5.0f), Climate->Months[0].MinTemperatureCelsius, 0.01f);
    TestEqual(TEXT("Warmest at 15:00"), Climate->GetBaseTemperature(7, 15.0f), Climate->Months[6].MaxTemperatureCelsius, 0.01f);
    return true;
}

// P1.2: snow builds up in winter and melts in spring; melt wets the ground; save keeps it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimateSnowSeasonTest,
    "MyProject3.World.Climate.SnowBuildsInWinterAndMeltsInSpring", Flags)
bool FClimateSnowSeasonTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorldSimulationClockSubsystem* Clock = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Climate"), Climate))
        return false;

    // January 1 of year 2 at 00:00: 16 hours + 45 days after the start.
    const double NewYear = 16.0 * 3600.0 + 45.0 * Day;
    TestTrue(TEXT("Clock"), Clock->InitializeClock(5, NewYear));
    TestTrue(TEXT("Climate starts"), Climate->InitializeClimate());
    TestEqual(TEXT("Winter starts with old snow"), Climate->GetSnowDepthCm(), 40.0f);

    double Time = NewYear;
    float MinSnowInWinter = Climate->GetSnowDepthCm();
    float MaxWetness = 0.0f;
    auto RunHours = [&](int32 Hours)
    {
        for (int32 Hour = 0; Hour < Hours; ++Hour)
        {
            Time += 3600.0;
            if (!Climate->AdvanceClimateStep(3600.0, Time))
                return false;
            MaxWetness = FMath::Max(MaxWetness, Climate->GetGroundWetness());
        }
        return true;
    };
    // January and February: below freezing all day, so snow never melts.
    for (int32 Hour = 0; Hour < 10 * 24; ++Hour)
    {
        Time += 3600.0;
        TestTrue(TEXT("Winter hour"), Climate->AdvanceClimateStep(3600.0, Time));
        TestTrue(TEXT("Winter snow never shrinks"), Climate->GetSnowDepthCm() >= MinSnowInWinter - 0.001f);
        MinSnowInWinter = Climate->GetSnowDepthCm();
    }
    TestTrue(TEXT("Snow piled up over winter"), Climate->GetSnowDepthCm() > 40.0f);
    TestTrue(TEXT("Winter air is below freezing at night"), Climate->GetBaseTemperature(2, 5.0f) < 0.0f);

    // Save the climate at the end of February.
    const FWorldClimateSnapshot Saved = Climate->ExportSnapshot();

    // March to May.
    TestTrue(TEXT("Spring"), RunHours(15 * 24));
    TestEqual(TEXT("Snow is gone by June"), Climate->GetSnowDepthCm(), 0.0f);
    TestTrue(TEXT("Melt and rain wet the ground"), MaxWetness > 0.3f);

    TestTrue(TEXT("Load the February save"), Climate->RestoreSnapshot(Saved));
    TestEqual(TEXT("Snow restored"), Climate->GetSnowDepthCm(), Saved.SnowDepthCm);
    TestTrue(TEXT("Weather restored"), Climate->GetWeather() == Saved.Weather);
    FWorldClimateSnapshot Bad = Saved;
    Bad.GroundWetness = 2.0f;
    TestFalse(TEXT("Broken save refused"), Climate->RestoreSnapshot(Bad));
    return true;
}

// P1.2: the fixed-step coordinator starts the climate and hands its weather to survival.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimateCoordinatorTest,
    "MyProject3.World.Climate.CoordinatorDrivesSurvivalWeather", Flags)
bool FClimateCoordinatorTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationCoordinatorSubsystem* Coordinator = World ? World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate = World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Coordinator"), Coordinator) || !TestNotNull(TEXT("Climate"), Climate) ||
        !TestNotNull(TEXT("Survival"), Survival) || !TestNotNull(TEXT("Clock"), Clock))
        return false;

    TestTrue(TEXT("Start simulation"), Coordinator->InitializeWorldSimulation(11, 0.0, 1.0e7));
    TestTrue(TEXT("Climate started with the world"), Climate->IsClimateInitialized());
    for (int32 Hour = 0; Hour < 30; ++Hour)
    {
        TestTrue(TEXT("Fixed hour"), Coordinator->AdvanceFixedStep(3600.0));
        TestTrue(TEXT("Survival sees the climate weather"), Survival->Weather == Climate->GetWeather());
        TestEqual(TEXT("Survival air temperature is the climate's"), Survival->AmbientTemperatureCelsius, Climate->GetTemperatureCelsius());
    }
    FWorldCalendarDate Date;
    TestTrue(TEXT("Date"), Clock->GetCalendarDate(Date));
    TestEqual(TEXT("30 hours after 08:00 on March 5 is April 1, 14:00"), Date.Month, 4);
    TestEqual(TEXT("Hour"), Date.HourOfDay, 14.0f, 0.01f);
    return true;
}

// P2.1 + P2.5: dirty water makes you ill, boiled water does not; spoiled meat poisons.
// A condition that is not defined refuses the whole action (the item stays).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsumeCausesConditionTest,
    "MyProject3.World.Needs.DirtyWaterAndSpoiledFoodCauseConditions", Flags)
bool FConsumeCausesConditionTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Survival"), Survival) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;
    TestTrue(TEXT("Clock"), Clock->InitializeClock(3, 100.0));

    AActor* Owner = World->SpawnActor<AActor>();
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(Owner);
    Owner->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    const FGuid InventoryId = Inventory->GetInventoryOwnerId();

    TArray<FInventoryStack> Stacks;
    auto AddItem = [&](FName DefinitionId) -> FGuid
    {
        FWorldOwnedItemRecord Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = DefinitionId;
        Item.BaseDefinitionId = DefinitionId;
        Item.OwnerKind = EWorldItemOwnerKind::Inventory;
        Item.OwnerId = InventoryId;
        Item.Quantity = 1;
        Item.CreatedAtWorldTimeSeconds = 100.0;
        TestTrue(TEXT("Register item"), Ownership->RegisterItem(Item));
        Inventory->ItemWeightGrams.Add(DefinitionId, 300);
        FInventoryStack Stack;
        Stack.ItemId = DefinitionId;
        Stack.Quantity = 1;
        Stack.MaxStack = 1;
        Stack.OwnedWorldItemId = Item.ItemId;
        Stacks.Add(Stack);
        return Item.ItemId;
    };
    const FGuid Dirty = AddItem(TEXT("Water.Dirty"));
    const FGuid Boiled = AddItem(TEXT("Water.Boiled"));
    const FGuid Meat = AddItem(TEXT("Meat.Cooked"));
    const FGuid Mystery = AddItem(TEXT("Berry.Unknown"));
    TestTrue(TEXT("Inventory holds the items"), Inventory->RestoreSnapshot(InventoryId, Stacks, NAME_None));

    FWorldSurvivalConditionDefinition Illness;
    Illness.DefinitionId = TEXT("Illness.DirtyWater");
    Illness.Kind = EWorldSurvivalConditionKind::Illness;
    Illness.HealthLossPerHour = 1.0f;
    Illness.DurationSeconds = 2.0 * 86400.0;
    TestTrue(TEXT("Illness defined"), Survival->RegisterConditionDefinition(Illness));
    FWorldSurvivalConditionDefinition Poisoning;
    Poisoning.DefinitionId = TEXT("Poisoning.SpoiledFood");
    Poisoning.Kind = EWorldSurvivalConditionKind::Poisoning;
    Poisoning.HealthLossPerHour = 3.0f;
    Poisoning.DurationSeconds = 86400.0;
    TestTrue(TEXT("Poisoning defined"), Survival->RegisterConditionDefinition(Poisoning));

    FWorldSurvivalConsumableDefinition DirtyWater;
    DirtyWater.DefinitionId = TEXT("Water.Dirty");
    DirtyWater.ThirstRelief = 40.0f;
    DirtyWater.ConditionOnConsumeId = Illness.DefinitionId;
    DirtyWater.ConditionSeverity = 0.6f;
    TestTrue(TEXT("Dirty water"), Survival->RegisterConsumableDefinition(DirtyWater));
    FWorldSurvivalConsumableDefinition BoiledWater;
    BoiledWater.DefinitionId = TEXT("Water.Boiled");
    BoiledWater.ThirstRelief = 40.0f;
    TestTrue(TEXT("Boiled water"), Survival->RegisterConsumableDefinition(BoiledWater));
    FWorldSurvivalConsumableDefinition CookedMeat;
    CookedMeat.DefinitionId = TEXT("Meat.Cooked");
    CookedMeat.CaloriesKcal = 600.0f;
    CookedMeat.FreshnessDurationSeconds = 3600.0;
    CookedMeat.SpoiledEnergyFraction = 0.3f;
    CookedMeat.ConditionWhenSpoiledId = Poisoning.DefinitionId;
    TestTrue(TEXT("Cooked meat"), Survival->RegisterConsumableDefinition(CookedMeat));
    FWorldSurvivalConsumableDefinition UnknownBerry;
    UnknownBerry.DefinitionId = TEXT("Berry.Unknown");
    UnknownBerry.CaloriesKcal = 50.0f;
    UnknownBerry.ConditionOnConsumeId = TEXT("Condition.NotDefined");
    TestTrue(TEXT("Berry"), Survival->RegisterConsumableDefinition(UnknownBerry));

    FWorldSurvivalProfile Profile;
    Profile.EntityId = FGuid::NewGuid();
    Profile.CaloriesKcal = 1000.0f;
    Profile.Hunger = 60.0f;
    Profile.Thirst = 70.0f;
    TestTrue(TEXT("Profile"), Survival->RegisterProfile(Profile));
    auto Current = [&]() { FWorldSurvivalProfile P; Survival->GetProfile(Profile.EntityId, P); return P; };

    TestTrue(TEXT("Drink boiled water"), Survival->TryConsumeFoodItem(FGuid::NewGuid(), Profile.EntityId, Current().Revision, Boiled, 1, InventoryId));
    TestEqual(TEXT("Boiled water is safe"), Current().Conditions.Num(), 0);
    TestEqual(TEXT("Thirst relieved"), Current().Thirst, 30.0f);

    TestTrue(TEXT("Drink dirty water"), Survival->TryConsumeFoodItem(FGuid::NewGuid(), Profile.EntityId, Current().Revision, Dirty, 1, InventoryId));
    TestEqual(TEXT("Dirty water makes you ill"), Current().Conditions.Num(), 1);
    if (Current().Conditions.Num() == 1)
    {
        TestTrue(TEXT("It is an illness"), Current().Conditions[0].Kind == EWorldSurvivalConditionKind::Illness);
        TestEqual(TEXT("Severity from the water"), Current().Conditions[0].Severity, 0.6f);
    }

    const int64 BeforeBerry = Current().Revision;
    TestFalse(TEXT("Unknown condition refuses the meal"), Survival->TryConsumeFoodItem(FGuid::NewGuid(), Profile.EntityId, BeforeBerry, Mystery, 1, InventoryId));
    FWorldOwnedItemRecord BerryRecord;
    TestTrue(TEXT("Berry still exists"), Ownership->TryGetItem(Mystery, BerryRecord));
    TestTrue(TEXT("Berry still in the inventory"), BerryRecord.OwnerKind == EWorldItemOwnerKind::Inventory);
    TestEqual(TEXT("Profile untouched"), Current().Revision, BeforeBerry);

    TestTrue(TEXT("Two hours pass"), Clock->AdvanceSimulation(7200.0));
    const float CaloriesBefore = Current().CaloriesKcal;
    TestTrue(TEXT("Eat spoiled meat"), Survival->TryConsumeFoodItem(FGuid::NewGuid(), Profile.EntityId, Current().Revision, Meat, 1, InventoryId));
    TestEqual(TEXT("Spoiled meat gives 30% energy"), Current().CaloriesKcal, CaloriesBefore + 180.0f, 0.01f);
    TestEqual(TEXT("Spoiled meat poisons"), Current().Conditions.Num(), 2);
    if (Current().Conditions.Num() == 2)
        TestTrue(TEXT("Poisoning"), Current().Conditions[1].Kind == EWorldSurvivalConditionKind::Poisoning);
    return true;
}

// P2.2: sleep under a roof and warm recovers more than sleep outside in the cold.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRestPlaceQualityTest,
    "MyProject3.World.Needs.RestPlaceDecidesRecovery", Flags)
bool FRestPlaceQualityTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Survival"), Survival))
        return false;
    TestTrue(TEXT("Clock"), Clock->InitializeClock(1, 0.0));
    Survival->AmbientTemperatureCelsius = 15.0f;

    FWorldSurvivalProfile Outside;
    Outside.EntityId = FGuid::NewGuid();
    Outside.Fatigue = 80.0f;
    Outside.Stamina = 20.0f;
    Outside.Location = FVector(0.0, 0.0, 0.0);
    FWorldSurvivalProfile Cabin = Outside;
    Cabin.EntityId = FGuid::NewGuid();
    Cabin.ShelterProtection = 1.0f;
    TestTrue(TEXT("Outside sleeper"), Survival->RegisterProfile(Outside));
    TestTrue(TEXT("Cabin sleeper"), Survival->RegisterProfile(Cabin));

    FWorldSurvivalProfile Cold = Outside;
    Cold.FeltTemperatureCelsius = -5.0f;
    Cold.Wetness = 0.8f;
    FWorldSurvivalProfile Warm = Cabin;
    Warm.FeltTemperatureCelsius = 20.0f;
    TestEqual(TEXT("Open, cold and wet is the worst place"), Survival->GetRestQuality(Cold), 0.5f * 0.5f * 0.75f, 0.001f);
    TestEqual(TEXT("Roof and warmth is the best place"), Survival->GetRestQuality(Warm), 1.0f, 0.001f);

    // Two hours, so neither sleeper reaches full stamina.
    TestTrue(TEXT("Outside sleeps"), Survival->TryBeginRest(FGuid::NewGuid(), Outside.EntityId, 1, 2.0 * 3600.0));
    TestTrue(TEXT("Cabin sleeps"), Survival->TryBeginRest(FGuid::NewGuid(), Cabin.EntityId, 1, 2.0 * 3600.0));
    for (int32 Hour = 1; Hour <= 2; ++Hour)
    {
        TestTrue(TEXT("Clock hour"), Clock->AdvanceSimulation(3600.0));
        TestTrue(TEXT("Night hour"), Survival->AdvanceSurvivalStep(3600.0, Hour * 3600.0));
    }
    FWorldSurvivalProfile OutsideAfter, CabinAfter;
    Survival->GetProfile(Outside.EntityId, OutsideAfter);
    Survival->GetProfile(Cabin.EntityId, CabinAfter);
    TestTrue(TEXT("Both rested"), OutsideAfter.Fatigue < 80.0f && CabinAfter.Fatigue < 80.0f);
    TestTrue(TEXT("The cabin sleeper is less tired"), CabinAfter.Fatigue < OutsideAfter.Fatigue);
    TestTrue(TEXT("The cabin sleeper has more stamina"), CabinAfter.Stamina > OutsideAfter.Stamina);
    return true;
}

// P2.4: food ages slower in a cool cellar and in frost, normally in the open.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpoilageByStorageTest,
    "MyProject3.World.Needs.SpoilageFollowsStorageAndTemperature", Flags)
bool FSpoilageByStorageTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldStorageSubsystem* Storage = World ? World->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate = World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Ownership"), Ownership) ||
        !TestNotNull(TEXT("Storage"), Storage) || !TestNotNull(TEXT("Climate"), Climate))
        return false;
    TestTrue(TEXT("Clock"), Clock->InitializeClock(2, 0.0));

    const FGuid Cellar = FGuid::NewGuid();
    TestTrue(TEXT("Cellar"), Storage->RegisterStorage(Cellar, 32));
    TestTrue(TEXT("Cellar is cool"), Storage->SetStorageSpoilageRate(Cellar, 0.25f));
    TestFalse(TEXT("Rate limits"), Storage->SetStorageSpoilageRate(Cellar, 9.0f));

    auto Register = [&](EWorldItemOwnerKind Kind, const FGuid& OwnerId) -> FGuid
    {
        FWorldOwnedItemRecord Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("Meat.Cooked");
        Item.OwnerKind = Kind;
        Item.OwnerId = OwnerId;
        Item.CreatedAtWorldTimeSeconds = 0.0;
        TestTrue(TEXT("Register meat"), Ownership->RegisterItem(Item));
        return Item.ItemId;
    };
    const FGuid Open = Register(EWorldItemOwnerKind::World, FGuid());
    const FGuid Stored = Register(EWorldItemOwnerKind::Storage, Cellar);

    TestTrue(TEXT("Four hours"), Clock->AdvanceSimulation(4.0 * 3600.0));
    TestEqual(TEXT("Open air ages normally"), Ownership->GetSpoilageAgeSeconds(Open), 4.0 * 3600.0, 1.0);
    TestEqual(TEXT("Cellar ages a quarter"), Ownership->GetSpoilageAgeSeconds(Stored), 1.0 * 3600.0, 1.0);

    // Taking it out keeps the cellar time and continues at the open-air rate.
    FWorldOwnedItemRecord Record;
    TestTrue(TEXT("Read stored"), Ownership->TryGetItem(Stored, Record));
    FWorldOwnedItemRecord Moved;
    TestTrue(TEXT("Take out of the cellar"), Ownership->TryTransferItem(FGuid::NewGuid(), Stored, Record.Revision,
        EWorldItemOwnerKind::Storage, EWorldItemOwnerKind::World, FGuid(), FVector::ZeroVector, Moved));
    TestTrue(TEXT("Two more hours"), Clock->AdvanceSimulation(2.0 * 3600.0));
    TestEqual(TEXT("Cellar hour + two open hours"), Ownership->GetSpoilageAgeSeconds(Stored), 3.0 * 3600.0, 1.0);

    // Frost: start the climate in January; the open-air meat now ages at the frost rate.
    TestTrue(TEXT("Jump to January"), Clock->RestoreSnapshot([&]()
    {
        FWorldSimulationClockSnapshot Snapshot = Clock->ExportSnapshot();
        Snapshot.SimulationTimeSeconds = 16.0 * 3600.0 + 45.0 * Day;
        return Snapshot;
    }()));
    TestTrue(TEXT("Winter climate"), Climate->InitializeClimate());
    TestTrue(TEXT("Frost slows ageing"), Ownership->GetSpoilageRate(Moved) <= 0.25f);
    return true;
}

// P3.1: wet wood burns short and weak, charcoal burns hot enough to fire clay.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelKindsTest,
    "MyProject3.World.Fire.WetWoodWeakCharcoalHot", Flags)
bool FFuelKindsTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;
    TestTrue(TEXT("Clock"), Clock->InitializeClock(4, 0.0));

    AActor* InventoryActor = World->SpawnActor<AActor>();
    AActor* FireActor = World->SpawnActor<AActor>();
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(InventoryActor);
    InventoryActor->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    UWorldObjectIdentityComponent* FireIdentity = NewObject<UWorldObjectIdentityComponent>(FireActor);
    FireActor->AddInstanceComponent(FireIdentity);
    FireIdentity->RegisterComponent();
    FireIdentity->EnsureIdentity();
    UHeatSourceComponent* Fire = NewObject<UHeatSourceComponent>(FireActor);
    FireActor->AddInstanceComponent(Fire);
    Fire->RegisterComponent();
    Fire->MaxFuelSeconds = 20000.0f;

    const FGuid InventoryId = Inventory->GetInventoryOwnerId();
    TArray<FInventoryStack> Stacks;
    auto AddFuel = [&](FName DefinitionId) -> FGuid
    {
        FWorldOwnedItemRecord Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = DefinitionId;
        Item.OwnerKind = EWorldItemOwnerKind::Inventory;
        Item.OwnerId = InventoryId;
        TestTrue(TEXT("Register fuel"), Ownership->RegisterItem(Item));
        Inventory->ItemWeightGrams.Add(DefinitionId, 500);
        FInventoryStack Stack;
        Stack.ItemId = DefinitionId;
        Stack.Quantity = 1;
        Stack.MaxStack = 1;
        Stack.OwnedWorldItemId = Item.ItemId;
        Stacks.Add(Stack);
        return Item.ItemId;
    };
    const FGuid Wet = AddFuel(TEXT("Firewood.Wet"));
    const FGuid Coal1 = AddFuel(TEXT("Charcoal"));
    const FGuid Coal2 = AddFuel(TEXT("Charcoal.B"));
    TestTrue(TEXT("Inventory"), Inventory->RestoreSnapshot(InventoryId, Stacks, NAME_None));

    FHeatFuelDefinition WetWood;
    WetWood.ItemDefinitionId = TEXT("Firewood.Wet");
    WetWood.BurnSecondsPerItem = 600.0f;
    WetWood.HeatOutputMultiplier = 0.6f;
    FHeatFuelDefinition Charcoal;
    Charcoal.ItemDefinitionId = TEXT("Charcoal");
    Charcoal.BurnSecondsPerItem = 2400.0f;
    Charcoal.HeatOutputMultiplier = 1.5f;
    FHeatFuelDefinition CharcoalB = Charcoal;
    CharcoalB.ItemDefinitionId = TEXT("Charcoal.B");
    FHeatFuelDefinition Bad = Charcoal;
    Bad.ItemDefinitionId = TEXT("Bad");
    Bad.HeatOutputMultiplier = 9.0f;
    TestTrue(TEXT("Wet wood fuel"), Fire->RegisterFuelDefinition(WetWood));
    TestTrue(TEXT("Charcoal fuel"), Fire->RegisterFuelDefinition(Charcoal));
    TestTrue(TEXT("Charcoal fuel B"), Fire->RegisterFuelDefinition(CharcoalB));
    TestFalse(TEXT("Impossible heat refused"), Fire->RegisterFuelDefinition(Bad));

    TestTrue(TEXT("Wet wood in"), Fire->TryAddFuelItem(FGuid::NewGuid(), Wet, 1, InventoryId));
    TestTrue(TEXT("Light"), Fire->Ignite());
    const FVector Near = Fire->GetComponentLocation() + FVector(100.0, 0.0, 0.0);
    const float WetHeat = Fire->GetHeatAt(Near);
    TestEqual(TEXT("Wet wood fire is weak"), Fire->GetFuelHeatMultiplier(), 0.6f, 0.001f);
    TestFalse(TEXT("Wet wood is not hot enough for clay"), Fire->IsBurningHot());

    TestTrue(TEXT("Charcoal in"), Fire->TryAddFuelItem(FGuid::NewGuid(), Coal1, 1, InventoryId));
    // (600 x 0.6 + 2400 x 1.5) / 3000 = 1.32
    TestEqual(TEXT("Mixed fire"), Fire->GetFuelHeatMultiplier(), 1.32f, 0.001f);
    TestTrue(TEXT("Mostly charcoal is hot"), Fire->IsBurningHot());
    TestTrue(TEXT("More charcoal"), Fire->TryAddFuelItem(FGuid::NewGuid(), Coal2, 1, InventoryId));
    TestTrue(TEXT("Charcoal fire warms more"), Fire->GetHeatAt(Near) > WetHeat * 2.0f);
    return true;
}

// P3.2: a fire warms more inside a closed shelter than in the open.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterKeepsFireHeatTest,
    "MyProject3.World.Fire.ShelterKeepsFireHeat", Flags)
bool FShelterKeepsFireHeatTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Survival"), Survival))
        return false;
    Survival->AmbientTemperatureCelsius = -5.0f;
    Survival->SetWeather(EWorldWeatherState::Clear, 0.0f);

    AActor* FireActor = World->SpawnActor<AActor>();
    UHeatSourceComponent* Fire = NewObject<UHeatSourceComponent>(FireActor);
    FireActor->SetRootComponent(Fire);
    Fire->RegisterComponent();
    TestTrue(TEXT("Fuel"), Fire->AddFuel(3600.0f));
    TestTrue(TEXT("Light"), Fire->Ignite());

    FWorldSurvivalProfile Open;
    Open.Location = Fire->GetComponentLocation() + FVector(150.0, 0.0, 0.0);
    FWorldSurvivalProfile Inside = Open;
    Inside.ShelterProtection = 1.0f;
    FWorldSurvivalProfile InsideNoFire = Inside;
    InsideNoFire.Location = FVector(100000.0, 0.0, 0.0);

    const float FireHeat = Survival->GetHeatAt(Open.Location);
    TestTrue(TEXT("The fire warms"), FireHeat > 1.0f);
    const float OpenFelt = Survival->ComputeFeltTemperature(Open);
    const float InsideFelt = Survival->ComputeFeltTemperature(Inside);
    const float ShelterOnly = Survival->ComputeFeltTemperature(InsideNoFire) - (Survival->AmbientTemperatureCelsius);
    TestEqual(TEXT("Inside = open + shelter warmth + kept fire heat"),
        InsideFelt - OpenFelt, Survival->ShelterWarmthCelsius + Survival->ShelterHeatRetention * FireHeat, 0.01f);
    TestTrue(TEXT("Fire indoors beats shelter alone"), InsideFelt - Survival->AmbientTemperatureCelsius > ShelterOnly + FireHeat);
    return true;
}

// P0.3/P0.4: the primitive-era data loads, every reference resolves, all 38 plan steps are there.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameDataLoadsTest,
    "MyProject3.World.GameData.LoadsAndCoversThePlan", Flags)
bool FGameDataLoadsTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorldGameDataSubsystem* Data = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data))
        return false;
    TestTrue(FString::Printf(TEXT("Loads (%s)"), *Data->GetLastError()), Data->LoadDefault());
    TSet<int32> PlanSteps;
    int32 HandRecipes = 0, StationRecipes = 0, PhysicalParts = 0;
    for (const FGameDataTransformation& T : Data->GetTransformations())
    {
        PlanSteps.Add(T.PlanNumber);
        HandRecipes += T.Where == TEXT("hand") && !T.bPhysical ? 1 : 0;
        StationRecipes += Data->GetStations().Contains(T.Where) ? 1 : 0;
        PhysicalParts += T.bPhysical ? 1 : 0;
    }
    TestEqual(TEXT("All 38 plan transformations"), PlanSteps.Num(), 38);
    TestTrue(TEXT("Hand recipes"), HandRecipes >= 25);
    TestTrue(TEXT("Station recipes"), StationRecipes >= 20);
    TestTrue(TEXT("Physical parts are marked"), PhysicalParts >= 8);
    TestEqual(TEXT("Stations"), Data->GetStations().Num(), 10);
    TestEqual(TEXT("Three crops"), Data->GetCrops().Num(), 3);
    TestEqual(TEXT("Fifteen field action definitions"), Data->GetFieldActions().Num(), 15);
    TestEqual(TEXT("Eight infrastructure definitions"), Data->GetInfrastructures().Num(), 8);
    TestEqual(TEXT("Two typed structural repair recipes"), Data->GetStructureRepairs().Num(), 2);
    TestEqual(TEXT("Four raid countermeasure definitions"), Data->GetRaidDefenses().Num(), 4);
    TestEqual(TEXT("Three typed knowledge rewards"), Data->GetKnowledge().Num(), 3);
    TestEqual(TEXT("Five ordered discoveries"), Data->GetDiscoveries().Num(), 5);
    TestTrue(TEXT("Bedroll is a catalogued item"), Data->GetItems().Contains(TEXT("Bedroll")));
    const FGameDataItem* GrainHarvest = Data->GetItems().Find(TEXT("Grain.Harvest"));
    const FGameDataItem* RootHarvest = Data->GetItems().Find(TEXT("Root.Harvest"));
    const FGameDataItem* LegumeHarvest = Data->GetItems().Find(TEXT("Legume.Harvest"));
    TestTrue(TEXT("Three harvest products load as perishable consumables"),
        GrainHarvest && GrainHarvest->bConsumable &&
        RootHarvest && RootHarvest->bConsumable &&
        LegumeHarvest && LegumeHarvest->bConsumable);
    TestEqual(TEXT("Route anchors for the current authored slice"), Data->GetRouteNodes().Num(), 6);
    TestEqual(TEXT("Directed route edges"), Data->GetRouteEdges().Num(), 12);
    TestTrue(TEXT("Every material group has items"), Data->GetItems().Num() >= 80);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameDataRejectsBrokenTest,
    "MyProject3.World.GameData.BrokenReferenceIsRefused", Flags)
bool FGameDataRejectsBrokenTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorldGameDataSubsystem* Data = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data))
        return false;
    TestTrue(TEXT("Good data first"), Data->LoadDefault());
    const int32 GoodCount = Data->GetTransformations().Num();

    const FString Source = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("GameData"));
    const FString Broken = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("BrokenGameData"));
    IFileManager::Get().DeleteDirectory(*Broken, false, true);
    IFileManager::Get().MakeDirectory(*Broken, true);
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(Source, TEXT("*.json")), true, false);
    for (const FString& File : Files)
        IFileManager::Get().Copy(*FPaths::Combine(Broken, File), *FPaths::Combine(Source, File));
    FString Text;
    TestTrue(TEXT("Read recipes"), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Broken, TEXT("transformations.json"))));
    Text = Text.Replace(TEXT("\"item\": \"Flint\""), TEXT("\"item\": \"Unobtainium\""));
    TestTrue(TEXT("Write broken recipes"), FFileHelper::SaveStringToFile(Text, *FPaths::Combine(Broken, TEXT("transformations.json"))));

    TestFalse(TEXT("Unknown item refused"), Data->LoadFromDirectory(Broken));
    TestTrue(TEXT("The error names it"), Data->GetLastError().Contains(TEXT("Unobtainium")));
    TestTrue(TEXT("Good data kept"), Data->IsLoaded() && Data->GetTransformations().Num() == GoodCount);
    IFileManager::Get().DeleteDirectory(*Broken, false, true);
    return true;
}

// P0.4 + P4.2/P4.4: applied data feeds production, survival, climate, terrain and hand crafting;
// a tool is needed but not used up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameDataAppliesTest,
    "MyProject3.World.GameData.FeedsSystemsAndToolsAreKept", Flags)
bool FGameDataAppliesTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldProductionSubsystem* Production = World ? World->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    UWorldTerrainStateSubsystem* Terrain = World ? World->GetSubsystem<UWorldTerrainStateSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate = World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldAgricultureSubsystem* Agriculture = World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    UWorldLogisticsRouteSubsystem* Routes = World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Data"), Data) || !TestNotNull(TEXT("Production"), Production) ||
        !TestNotNull(TEXT("Terrain"), Terrain) || !TestNotNull(TEXT("Climate"), Climate) ||
        !TestNotNull(TEXT("Agriculture"), Agriculture) || !TestNotNull(TEXT("Routes"), Routes) ||
        !TestNotNull(TEXT("Survival"), Survival) || !TestNotNull(TEXT("Clock"), Clock))
        return false;
    TestTrue(TEXT("Load"), Data->LoadDefault());
    TestTrue(FString::Printf(TEXT("Apply (%s)"), *Data->GetLastError()), Data->ApplyToWorld());
    TestFalse(TEXT("Apply only once"), Data->ApplyToWorld());
    TestEqual(TEXT("Exactly five zone definitions load"), Data->GetZones().Num(), 5);
    TestEqual(TEXT("Five ordered discovery definitions load"), Data->GetDiscoveries().Num(), 5);
    TestEqual(TEXT("Five zone catalog matches IDs 0 through 4"), Data->GetZones().Num(),
        Data->GetZones().FilterByPredicate([](const FGameDataZone& Z) { return Z.Number >= 0 && Z.Number <= 4; }).Num());
    TestTrue(TEXT("Route graph loaded into the world"), Routes->GetNodeCount() == Data->GetRouteNodes().Num() &&
        Routes->GetEdgeCount() == Data->GetRouteEdges().Num());
    const FGameDataZone* CreekZone = Data->GetZones().FindByPredicate(
        [](const FGameDataZone& Z) { return Z.Id == TEXT("Zone.1"); });
    TestNotNull(TEXT("Creek zone catalog entry"), CreekZone);
    if (CreekZone)
    {
        TestTrue(TEXT("Creek lists clay and fish"), CreekZone->ResourceItemIds.Contains(TEXT("Clay")) &&
            CreekZone->ResourceItemIds.Contains(TEXT("Fish.Raw")));
        TestTrue(TEXT("Creek has a permanent bridge solution"),
            CreekZone->PermanentSolutionIds.Contains(TEXT("Infrastructure.CreekBridge")));
    }
    TestEqual(TEXT("Crop definitions are registered into agriculture"), Agriculture->CropDefinitions.Num(), Data->GetCrops().Num());
    TestEqual(TEXT("Animal definitions are registered into agriculture"), Agriculture->AnimalDefinitions.Num(), Data->GetAnimals().Num());
    const FWorldAnimalDefinition* GoatDefinition = Agriculture->AnimalDefinitions.FindByPredicate(
        [](const FWorldAnimalDefinition& D) { return D.AnimalDefinitionId == TEXT("Animal.Goat"); });
    TestNotNull(TEXT("Goat definition"), GoatDefinition);
    if (GoatDefinition)
    {
        TestEqual(TEXT("Goat feed item"), GoatDefinition->FeedDefinitionId, FName(TEXT("Grass")));
        TestEqual(TEXT("Goat milk item"), GoatDefinition->ProductDefinitionId, FName(TEXT("Milk.Raw")));
        TestEqual(TEXT("Goat milk interval is six hours"), GoatDefinition->ProductIntervalSeconds, 21600.0);
    }

    TestTrue(TEXT("Initialize shared clock for discovery recording"), Clock->InitializeClock(2048, 0.0));
    FWorldSurvivalProfile Explorer;
    Explorer.EntityId = FGuid::NewGuid();
    TestTrue(TEXT("Register exploration profile"), Survival->RegisterProfile(Explorer));
    FString ZoneAccessReason;

    TestFalse(TEXT("Creek entry is blocked until the camp is discovered"), Data->TryRecordZoneEntry(
        FGuid::NewGuid(), Explorer.EntityId, TEXT("Zone.1"), FVector(760.0, -1200.0, 0.0), ZoneAccessReason));
    TestTrue(TEXT("Zone rejection explains the prerequisite"), ZoneAccessReason.Contains(TEXT("requires discovery")));

    const FGuid CampEntryOperation = FGuid::NewGuid();
    TestTrue(TEXT("Record base-camp entry through the game-data adapter"), Data->TryRecordZoneEntry(
        CampEntryOperation, Explorer.EntityId, TEXT("Zone.0"), FVector::ZeroVector, ZoneAccessReason));
    TestTrue(TEXT("Retry same camp entry idempotently"), Data->TryRecordZoneEntry(
        CampEntryOperation, Explorer.EntityId, TEXT("Zone.0"), FVector::ZeroVector, ZoneAccessReason));
    TestTrue(TEXT("Base discovery unlocks grain seed knowledge"),
        Survival->IsDefinitionUnlocked(Explorer.EntityId, TEXT("Seed.Grain")));

    TestTrue(TEXT("Record creek entry after the camp prerequisite"), Data->TryRecordZoneEntry(
        FGuid::NewGuid(), Explorer.EntityId, TEXT("Zone.1"), FVector(760.0, -1200.0, 0.0), ZoneAccessReason));
    TestTrue(TEXT("Creek discovery unlocks bridge infrastructure"), Survival->IsDefinitionUnlocked(
        Explorer.EntityId, TEXT("Infrastructure.CreekBridge")));

    TestFalse(TEXT("Lake-swamp zone is locked before high-forest discovery"),
        Data->CanEnterZone(Explorer.EntityId, TEXT("Zone.3"), ZoneAccessReason));
    TestTrue(TEXT("Record high-forest entry after creek discovery"), Data->TryRecordZoneEntry(
        FGuid::NewGuid(), Explorer.EntityId, TEXT("Zone.2"), FVector(600.0, 2650.0, 700.0), ZoneAccessReason));
    TestTrue(TEXT("Lake-swamp zone opens after high-forest discovery"),
        Data->CanEnterZone(Explorer.EntityId, TEXT("Zone.3"), ZoneAccessReason));
    TestTrue(TEXT("Record lake-swamp entry after high-forest discovery"), Data->TryRecordZoneEntry(
        FGuid::NewGuid(), Explorer.EntityId, TEXT("Zone.3"), FVector(900.0, 3000.0, 0.0), ZoneAccessReason));

    TestFalse(TEXT("Mountain pass is seasonally closed in the starting month"),
        Data->CanEnterZone(Explorer.EntityId, TEXT("Zone.4"), ZoneAccessReason));
    TestTrue(TEXT("Seasonal denial gives a reason"), ZoneAccessReason.Contains(TEXT("seasonally closed")));
    TestTrue(TEXT("Advance calendar into June"), Clock->AdvanceSimulation(11.0 * 86400.0));
    TestFalse(TEXT("Mountain pass still requires cold-protection equipment"),
        Data->CanEnterZone(Explorer.EntityId, TEXT("Zone.4"), ZoneAccessReason));
    TestTrue(TEXT("Equipment denial names the required capability"),
        ZoneAccessReason.Contains(TEXT("Capability.ColdProtection")));

    TArray<FWorldProductionStation> Stations;
    TArray<FWorldProductionRecipe> Recipes;
    Production->ExportSnapshot(Stations, Recipes);
    const FWorldProductionRecipe* Kiln = Recipes.FindByPredicate([](const FWorldProductionRecipe& R) { return R.RecipeId == TEXT("fire_pot"); });
    if (TestNotNull(TEXT("Pottery is a kiln recipe"), Kiln))
        TestEqual(TEXT("Needs the pit kiln"), Kiln->RequiredStationTag, FName(TEXT("pit_kiln")));
    const FWorldProductionRecipe* Plank = Recipes.FindByPredicate([](const FWorldProductionRecipe& R) { return R.RecipeId == TEXT("make_plank"); });
    if (TestNotNull(TEXT("Plank recipe"), Plank))
        TestTrue(TEXT("Station tools come back"), Plank->Outputs.ContainsByPredicate([](const FProductionIngredient& I) { return I.ItemId == TEXT("Tool.Wedge"); }));
    TestEqual(TEXT("Six terrain layers"), Terrain->ResourceYieldRules.Num(), 6);
    TestEqual(TEXT("Twelve months"), Climate->Months.Num(), 12);

    // A new inventory picks up the hand recipes by itself.
    AActor* Owner = World->SpawnActor<AActor>();
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(Owner);
    Owner->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    FInventoryStack Flint;
    Flint.ItemId = TEXT("Flint");
    Flint.Quantity = 2;
    TestTrue(TEXT("Flint in hand"), Inventory->TryAddItem(Flint));
    TArray<FInventoryStack> Missing;
    TestFalse(TEXT("No hammer stone, no knapping"), Inventory->CanCraft(TEXT("knap_flake"), Missing));
    TestTrue(TEXT("The hammer stone is what is missing"), Missing.ContainsByPredicate([](const FInventoryStack& S) { return S.ItemId == TEXT("HammerStone"); }));
    FInventoryStack Hammer;
    Hammer.ItemId = TEXT("HammerStone");
    Hammer.Quantity = 1;
    Hammer.MaxStack = 1;
    Hammer.bIsTool = true;
    Hammer.Category = EInventoryItemCategory::Tool;
    TestTrue(TEXT("Hammer stone in hand"), Inventory->TryAddItem(Hammer));
    TestTrue(TEXT("Knap"), Inventory->TryCraft(TEXT("knap_flake")));
    TestEqual(TEXT("Two flakes"), Inventory->GetItemCount(TEXT("FlintFlake")), 2);
    TestEqual(TEXT("One flint used"), Inventory->GetItemCount(TEXT("Flint")), 1);
    TestEqual(TEXT("Hammer stone kept"), Inventory->GetItemCount(TEXT("HammerStone")), 1);
    return true;
}

// P7.4: bridge infrastructure changes traversal cost and its built state survives route snapshot restore.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteInfrastructureSnapshotTest,
    "MyProject3.Logistics.InfrastructureBridgeReducesRouteCostAndPersists", Flags)
bool FRouteInfrastructureSnapshotTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldLogisticsRouteSubsystem* Routes = World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Route graph"), Routes))
        return false;

    TestTrue(TEXT("Load zone and route data"), Data->LoadDefault());
    TestTrue(FString::Printf(TEXT("Apply world data (%s)"), *Data->GetLastError()), Data->ApplyToWorld());
    TestTrue(TEXT("All route nodes were applied"), Routes->GetNodeCount() == Data->GetRouteNodes().Num());
    TestTrue(TEXT("All route edges were applied"), Routes->GetEdgeCount() == Data->GetRouteEdges().Num());

    const FName StartNode(TEXT("Route.Camp"));
    const FName DestinationNode(TEXT("Route.Creek.EastBank"));
    FLogisticsRouteCostContext Context;
    TArray<FName> BeforePath;
    float BeforeCost = 0.0f;
    TestTrue(TEXT("Find route before bridge construction"),
        Routes->FindRouteWithContext(StartNode, DestinationNode, Context, BeforePath, BeforeCost));
    TestTrue(TEXT("Before the bridge, the route detours through the ford"),
        BeforePath.Contains(TEXT("Route.Ford.WestBank")) &&
        BeforePath.Contains(TEXT("Route.Ford.EastBank")));

    TestTrue(TEXT("Bridge construction marks its two route directions built"),
        Routes->SetInfrastructureBuilt(TEXT("Infrastructure.CreekBridge"), true));
    TArray<FName> AfterPath;
    float AfterCost = 0.0f;
    TestTrue(TEXT("Find route after bridge construction"),
        Routes->FindRouteWithContext(StartNode, DestinationNode, Context, AfterPath, AfterCost));
    TestTrue(TEXT("After the bridge, the route uses the direct crossing"),
        AfterPath.Contains(TEXT("Route.Creek.WestBank")) &&
        AfterPath.Contains(TEXT("Route.Creek.EastBank")) &&
        !AfterPath.Contains(TEXT("Route.Ford.WestBank")));
    TestTrue(TEXT("Bridge lowers the same journey's route cost"), AfterCost < BeforeCost);

    TArray<FLogisticsRouteNode> SavedNodes;
    TArray<FLogisticsRouteEdge> SavedEdges;
    Routes->ExportSnapshot(SavedNodes, SavedEdges);
    TestTrue(TEXT("Temporarily remove bridge state"),
        Routes->SetInfrastructureBuilt(TEXT("Infrastructure.CreekBridge"), false));
    TArray<FName> UnbuiltPath;
    float UnbuiltCost = 0.0f;
    TestTrue(TEXT("Find route while bridge is marked absent"),
        Routes->FindRouteWithContext(StartNode, DestinationNode, Context, UnbuiltPath, UnbuiltCost));
    TestTrue(TEXT("Removed bridge restores ford detour"), UnbuiltPath.Contains(TEXT("Route.Ford.WestBank")));
    TestTrue(TEXT("Restore route graph snapshot"), Routes->RestoreSnapshot(SavedNodes, SavedEdges));

    TArray<FName> RestoredPath;
    float RestoredCost = 0.0f;
    TestTrue(TEXT("Route resolves after snapshot restore"),
        Routes->FindRouteWithContext(StartNode, DestinationNode, Context, RestoredPath, RestoredCost));
    TestTrue(TEXT("Built bridge state survives snapshot restore"),
        RestoredPath == AfterPath && FMath::IsNearlyEqual(RestoredCost, AfterCost, 0.001f));
    return true;
}

// P7.2/P7.3: zone entry consumes the authored season/capability/discovery contract;
// discovery rewards are persistent and required equipment must be genuinely owned and equipped.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoneAccessAndDiscoveryProgressionTest,
    "MyProject3.Exploration.ZoneAccessRequiresSeasonGearAndDiscovery", Flags)
bool FZoneAccessAndDiscoveryProgressionTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Survival"), Survival) ||
        !TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;

    TestTrue(TEXT("Load zones, routes and discoveries"), Data->LoadDefault());
    TestTrue(FString::Printf(TEXT("Apply definitions (%s)"), *Data->GetLastError()), Data->ApplyToWorld());
    // The configured calendar begins at March 5. Sixteen configured day lengths advance into July.
    TestTrue(TEXT("Initialize July calendar"), Clock->InitializeClock(28019, 16.0 * Day));

    FWorldSurvivalProfile Profile;
    Profile.EntityId = FGuid::NewGuid();
    Profile.Location = FVector(0.0, 0.0, 0.0);
    TestTrue(TEXT("Register player survival profile"), Survival->RegisterProfile(Profile));

    FString AccessReason;
    TestTrue(TEXT("Base valley has no progression gate"),
        Data->CanEnterZone(Profile.EntityId, TEXT("Zone.0"), AccessReason));
    TestFalse(TEXT("Unknown zone is rejected"),
        Data->CanEnterZone(Profile.EntityId, TEXT("Zone.99"), AccessReason));

    TestFalse(TEXT("Lake/swamp remains closed before its required discovery"),
        Data->CanEnterZone(Profile.EntityId, TEXT("Zone.3"), AccessReason));
    TestTrue(TEXT("Reason explains the discovery gate"), AccessReason.Contains(TEXT("Discovery.Zone2.HighForest")));

    auto RecordDiscovery = [&](FName DiscoveryId) -> bool
    {
        FWorldSurvivalProfile Current;
        if (!Survival->GetProfile(Profile.EntityId, Current))
            return false;
        return Survival->TryRecordConfiguredDiscovery(FGuid::NewGuid(), Profile.EntityId,
            Current.Revision, DiscoveryId, Profile.Location, Clock->GetSimulationTimeSeconds());
    };

    TestTrue(TEXT("Record camp discovery"), RecordDiscovery(TEXT("Discovery.Zone0.Camp")));
    TestTrue(TEXT("Camp discovery unlocks grain seed definition"),
        Survival->IsDefinitionUnlocked(Profile.EntityId, TEXT("Seed.Grain")));
    FWorldSurvivalProfile CurrentProfile;
    TestTrue(TEXT("Read profile before out-of-order discovery"), Survival->GetProfile(Profile.EntityId, CurrentProfile));
    TestFalse(TEXT("High forest discovery cannot skip creek knowledge"),
        Survival->TryRecordConfiguredDiscovery(FGuid::NewGuid(), Profile.EntityId, CurrentProfile.Revision,
            TEXT("Discovery.Zone2.HighForest"), Profile.Location, Clock->GetSimulationTimeSeconds()));

    TestTrue(TEXT("Record creek discovery"), RecordDiscovery(TEXT("Discovery.Zone1.Creek")));
    TestTrue(TEXT("Creek discovery unlocks its bridge solution"),
        Survival->IsDefinitionUnlocked(Profile.EntityId, TEXT("Infrastructure.CreekBridge")));
    TestTrue(TEXT("Record high-forest discovery after creek"), RecordDiscovery(TEXT("Discovery.Zone2.HighForest")));
    TestTrue(TEXT("Lake/swamp opens after the high-forest discovery"),
        Data->CanEnterZone(Profile.EntityId, TEXT("Zone.3"), AccessReason));
    FWorldSurvivalProfile OtherProfile;
    OtherProfile.EntityId = FGuid::NewGuid();
    TestTrue(TEXT("Register a second player profile"), Survival->RegisterProfile(OtherProfile));
    TestFalse(TEXT("One player's discovery does not open a zone for another player"),
        Data->CanEnterZone(OtherProfile.EntityId, TEXT("Zone.3"), AccessReason));
    TestTrue(TEXT("Record lake/swamp discovery"), RecordDiscovery(TEXT("Discovery.Zone3.LakeSwamp")));

    TestFalse(TEXT("Mountain pass requires cold-protection clothing in July"),
        Data->CanEnterZone(Profile.EntityId, TEXT("Zone.4"), AccessReason));
    TestTrue(TEXT("Reason identifies cold protection"), AccessReason.Contains(TEXT("Capability.ColdProtection")));
    TestTrue(TEXT("Read profile before the restricted mountain-pass discovery"),
        Survival->GetProfile(Profile.EntityId, CurrentProfile));
    TestFalse(TEXT("Mountain-pass discovery cannot be recorded before required clothing"),
        Survival->TryRecordConfiguredDiscovery(FGuid::NewGuid(), Profile.EntityId,
            CurrentProfile.Revision, TEXT("Discovery.Zone4.MountainPass"), Profile.Location,
            Clock->GetSimulationTimeSeconds()));

    AActor* InventoryOwnerActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Inventory owner"), InventoryOwnerActor))
        return false;
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(InventoryOwnerActor);
    InventoryOwnerActor->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    TestTrue(TEXT("Apply item definitions to clothing inventory"), Data->ApplyToInventory(Inventory));

    auto AddUniqueClothing = [&](FName DefinitionId) -> FWorldOwnedItemRecord
    {
        FWorldOwnedItemRecord Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = DefinitionId;
        Item.BaseDefinitionId = DefinitionId;
        Item.Quantity = 1;
        Item.OwnerKind = EWorldItemOwnerKind::Inventory;
        Item.OwnerId = Inventory->GetInventoryOwnerId();
        Item.Revision = 1;
        TestTrue(FString::Printf(TEXT("Register owned clothing %s"), *DefinitionId.ToString()),
            Ownership->RegisterItem(Item));
        FInventoryStack Stack;
        Stack.ItemId = DefinitionId;
        Stack.Quantity = 1;
        Stack.Category = EInventoryItemCategory::Protection;
        Stack.MaxStack = 1;
        Stack.OwnedWorldItemId = Item.ItemId;
        TestTrue(FString::Printf(TEXT("Add clothing stack %s"), *DefinitionId.ToString()),
            Inventory->TryAddItem(Stack));
        return Item;
    };

    const FWorldOwnedItemRecord Tunic = AddUniqueClothing(TEXT("Clothing.HideTunic"));
    const FWorldOwnedItemRecord Boots = AddUniqueClothing(TEXT("Clothing.HideBoots"));

    TestTrue(TEXT("Read profile before equipping tunic"), Survival->GetProfile(Profile.EntityId, CurrentProfile));
    TestTrue(TEXT("Equip owned hide tunic"), Survival->TryEquipItem(FGuid::NewGuid(), Profile.EntityId,
        CurrentProfile.Revision, Tunic.ItemId, Tunic.Revision, Inventory->GetInventoryOwnerId()));
    TestTrue(TEXT("Tunic grants the cold-protection capability"),
        Survival->HasEquippedCapability(Profile.EntityId, TEXT("Capability.ColdProtection")));
    TestTrue(TEXT("Mountain pass becomes enterable in summer with cold-protection clothing"),
        Data->CanEnterZone(Profile.EntityId, TEXT("Zone.4"), AccessReason));

    TestTrue(TEXT("Read profile before equipping boots"), Survival->GetProfile(Profile.EntityId, CurrentProfile));
    TestTrue(TEXT("Equip owned hide boots"), Survival->TryEquipItem(FGuid::NewGuid(), Profile.EntityId,
        CurrentProfile.Revision, Boots.ItemId, Boots.Revision, Inventory->GetInventoryOwnerId()));
    TestTrue(TEXT("Record mountain-pass discovery with both required clothing items"),
        RecordDiscovery(TEXT("Discovery.Zone4.MountainPass")));
    TestTrue(TEXT("Mountain-pass discovery unlocks the forward-camp infrastructure"),
        Survival->IsDefinitionUnlocked(Profile.EntityId, TEXT("Infrastructure.ForwardCamp")));
    return true;
}

// P4.1: a gather spot needs its tool, empties, and grows back on the world clock.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGatherableResourceTest,
    "MyProject3.World.Gather.ToolEmptiesAndRefills", Flags)
bool FGatherableResourceTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;
    TestTrue(TEXT("Clock"), Clock->InitializeClock(6, 0.0));
    UWorldGameDataSubsystem* Data = World->GetSubsystem<UWorldGameDataSubsystem>();
    TestTrue(TEXT("Game data (item weights)"), Data && Data->LoadDefault() && Data->ApplyToWorld());

    AActor* Owner = World->SpawnActor<AActor>();
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(Owner);
    Owner->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();

    AGatherableResourceActor* Reeds = World->SpawnActor<AGatherableResourceActor>();
    if (!TestNotNull(TEXT("Reed bed"), Reeds))
        return false;
    Reeds->ItemDefinitionId = TEXT("Reed");
    Reeds->RequiredToolId = TEXT("FlintFlake");
    Reeds->UnitsPerGather = 2;
    Reeds->MaxUnits = 3;
    Reeds->RefillSecondsPerUnit = Day;

    int32 Got = 0;
    TestFalse(TEXT("No flake, no reeds"), Reeds->TryGather(Inventory, Got));
    FInventoryStack Flake;
    Flake.ItemId = TEXT("FlintFlake");
    TestTrue(TEXT("Flake in hand"), Inventory->TryAddItem(Flake));
    TestEqual(TEXT("Starts full"), Reeds->GetAvailableUnits(), 3);
    TestTrue(TEXT("Cut reeds"), Reeds->TryGather(Inventory, Got));
    TestEqual(TEXT("Two at a time"), Got, 2);
    TestTrue(TEXT("Cut the rest"), Reeds->TryGather(Inventory, Got));
    TestEqual(TEXT("Only one was left"), Got, 1);
    TestFalse(TEXT("Empty"), Reeds->TryGather(Inventory, Got));
    TestEqual(TEXT("Three reeds"), Inventory->GetItemCount(TEXT("Reed")), 3);
    TestEqual(TEXT("Flake kept"), Inventory->GetItemCount(TEXT("FlintFlake")), 1);

    TestTrue(TEXT("A day and a half"), Clock->AdvanceSimulation(1.5 * Day));
    TestEqual(TEXT("One grew back"), Reeds->GetAvailableUnits(), 1);
    TestTrue(TEXT("Another day"), Clock->AdvanceSimulation(Day));
    TestEqual(TEXT("Two grew back"), Reeds->GetAvailableUnits(), 2);

    // Food comes out as ledger items so it can be eaten.
    AGatherableResourceActor* Bush = World->SpawnActor<AGatherableResourceActor>();
    Bush->ItemDefinitionId = TEXT("Berries");
    Bush->bGiveLedgerItems = true;
    Bush->UnitsPerGather = 2;
    TestTrue(TEXT("Pick berries"), Bush->TryGather(Inventory, Got));
    TArray<FInventoryStack> Items;
    FGuid OwnerActor, InventoryId;
    FName Quick;
    Inventory->ExportSnapshot(OwnerActor, InventoryId, Items, Quick);
    int32 LedgerBerries = 0;
    for (const FInventoryStack& Stack : Items)
    {
        FWorldOwnedItemRecord Record;
        if (Stack.ItemId == TEXT("Berries") && Stack.OwnedWorldItemId.IsValid() && Ownership->TryGetItem(Stack.OwnedWorldItemId, Record))
            LedgerBerries += Record.OwnerKind == EWorldItemOwnerKind::Inventory ? 1 : 0;
    }
    TestEqual(TEXT("Two berries in the ledger"), LedgerBerries, 2);
    return true;
}

// P4.3: a pit kiln fires a pot only next to a charcoal-hot fire; inputs are used up, outputs collected.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionStationTest,
    "MyProject3.World.Station.KilnNeedsHotFire", Flags)
bool FProductionStationTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldProductionSubsystem* Production = World ? World->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Data"), Data) || !TestNotNull(TEXT("Production"), Production))
        return false;
    TestTrue(TEXT("Data"), Data->LoadDefault() && Data->ApplyToWorld());

    AActor* Owner = World->SpawnActor<AActor>();
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(Owner);
    Owner->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    FInventoryStack Pot;
    Pot.ItemId = TEXT("Pot.Raw");
    TestTrue(TEXT("Raw pot in hand"), Inventory->TryAddItem(Pot));

    AProductionStationActor* Kiln = World->SpawnActor<AProductionStationActor>();
    if (!TestNotNull(TEXT("Kiln"), Kiln))
        return false;
    Kiln->StationDataId = TEXT("pit_kiln");
    TestTrue(TEXT("Kiln registered"), Kiln->EnsureRegistered());
    TestTrue(TEXT("Pot into the kiln"), Kiln->TryDeposit(Inventory, TEXT("Pot.Raw"), 1));
    TestEqual(TEXT("Pot left the hands"), Inventory->GetItemCount(TEXT("Pot.Raw")), 0);

    FGuid Job;
    TestFalse(TEXT("No fire, no firing"), Kiln->TryStartRecipe(TEXT("fire_pot"), Job));

    AActor* FireActor = World->SpawnActor<AActor>();
    UHeatSourceComponent* Fire = NewObject<UHeatSourceComponent>(FireActor);
    FireActor->SetRootComponent(Fire);
    Fire->RegisterComponent();
    FireActor->SetActorLocation(Kiln->GetActorLocation() + FVector(100.0, 0.0, 0.0));
    TestTrue(TEXT("Wood fire"), Fire->RestoreSimulationFuelState(7200.0f, true, 1.0f));
    TestFalse(TEXT("A wood fire is not hot enough"), Kiln->TryStartRecipe(TEXT("fire_pot"), Job));
    TestTrue(TEXT("Charcoal fire"), Fire->RestoreSimulationFuelState(7200.0f, true, 1.5f));
    TestTrue(TEXT("Firing starts"), Kiln->TryStartRecipe(TEXT("fire_pot"), Job));
    TestEqual(TEXT("The raw pot is used"), Kiln->GetStockCount(TEXT("Pot.Raw")), 0);

    for (int32 Hour = 0; Hour < 4; ++Hour)
        TestTrue(TEXT("Kiln hour"), Production->AdvanceProductionStep(3600.0));
    TestEqual(TEXT("Collect"), Kiln->CollectFinishedJobs(), 1);
    TestEqual(TEXT("A fired pot"), Kiln->GetStockCount(TEXT("Pot.Fired")), 1);
    TestTrue(TEXT("Take it"), Kiln->TryTake(Inventory, TEXT("Pot.Fired"), 1));
    TestEqual(TEXT("Fired pot in hand"), Inventory->GetItemCount(TEXT("Pot.Fired")), 1);
    TestFalse(TEXT("Nothing more to take"), Kiln->TryTake(Inventory, TEXT("Pot.Fired"), 1));
    return true;
}

// P5.1: dig layers come from layers.json; clay comes out of its layer, not out of the surface layer.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDigLayersFromDataTest,
    "MyProject3.World.Dig.LayersFromDataYieldTheirItem", Flags)
bool FDigLayersFromDataTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldTerrainStateSubsystem* Terrain = World ? World->GetSubsystem<UWorldTerrainStateSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Data"), Data) || !TestNotNull(TEXT("Terrain"), Terrain) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;
    TestTrue(TEXT("Data"), Data->LoadDefault() && Data->ApplyToWorld());

    AActor* ChunkActor = World->SpawnActor<AActor>();
    UWorldObjectIdentityComponent* Identity = NewObject<UWorldObjectIdentityComponent>(ChunkActor);
    ChunkActor->AddInstanceComponent(Identity);
    Identity->RegisterComponent();
    TestTrue(TEXT("Chunk identity"), Identity->EnsureIdentity());
    UVoxelTerrainChunkComponent* Voxel = NewObject<UVoxelTerrainChunkComponent>(ChunkActor);
    ChunkActor->AddInstanceComponent(Voxel);
    Voxel->CellsPerAxis = 16;
    Voxel->VoxelSizeCm = 25.0f;
    Voxel->EditableBoundsLocal = FBox(FVector::ZeroVector, FVector(400.0f));
    Voxel->RegisterComponent();
    Voxel->InitializeFlatGround(350.0f);
    TestTrue(TEXT("Ground"), Voxel->RebuildMesh() > 0);

    FWorldTerrainCellState Cell;
    TArray<FGuid> Outputs;
    TestFalse(TEXT("No clay in the surface layer"), Terrain->TryExcavateForResource(
        FGuid::NewGuid(), Voxel, FVector(100.0f, 100.0f, 250.0f), 60.0f, 1.0f,
        Voxel->GetDensityRevision(), FIntVector(1, 1, 0), 0, 0, TEXT("Clay"), Cell, Outputs));

    TestTrue(TEXT("Dig clay in its layer"), Terrain->TryExcavateForResource(
        FGuid::NewGuid(), Voxel, FVector(250.0f, 250.0f, 200.0f), 60.0f, 1.0f,
        Voxel->GetDensityRevision(), FIntVector(2, 2, 1), 1, 0, TEXT("Clay"), Cell, Outputs));
    TestTrue(TEXT("Clay came out"), Outputs.Num() >= 1);
    for (const FGuid& Id : Outputs)
    {
        FWorldOwnedItemRecord Record;
        TestTrue(TEXT("Clay item exists"), Ownership->TryGetItem(Id, Record));
        TestEqual(TEXT("It is clay"), Record.DefinitionId, FName(TEXT("Clay")));
    }
    return true;
}


// P6.1/P6.2: field operations are revision guarded and operation-ID retries replay one result.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFarmFieldWorkReceiptsTest,
    "MyProject3.World.Farming.FieldWorkIsRevisionGuardedAndIdempotent", Flags)
bool FFarmFieldWorkReceiptsTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorldAgricultureSubsystem* Agriculture = TestWorld.World
        ? TestWorld.World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Agriculture subsystem"), Agriculture))
        return false;

    FWorldFieldCellState SeedField;
    SeedField.Cell = FIntVector(7, 3, 0);
    SeedField.Fertility = 55.0f;
    SeedField.WeedPressure = 0.8f;
    SeedField.Moisture = 0.2f;
    TestTrue(TEXT("Register field cell"), Agriculture->RegisterFieldCell(SeedField));

    FWorldFieldCellState Field;
    TestTrue(TEXT("Read registered field cell"), Agriculture->GetFieldCell(SeedField.Cell, Field));
    const int64 InitialRevision = Field.Revision;
    const FGuid ClearOperation = FGuid::NewGuid();
    TestTrue(TEXT("Clear field"), Agriculture->TryApplyFieldWork(
        ClearOperation, Field.Cell, InitialRevision, EWorldFieldWorkKind::Clear, 1.0f, Field));
    TestTrue(TEXT("Field is cleared"), Field.bCleared);
    TestFalse(TEXT("Clearing invalidates old till state"), Field.bTilled);
    const int64 ClearedRevision = Field.Revision;

    FWorldFieldCellState Retried;
    TestTrue(TEXT("Same clear operation replays"), Agriculture->TryApplyFieldWork(
        ClearOperation, Field.Cell, InitialRevision, EWorldFieldWorkKind::Clear, 1.0f, Retried));
    TestEqual(TEXT("Retry returns current field revision"), Retried.Revision, ClearedRevision);
    TestFalse(TEXT("An operation ID cannot be reused for a different action"), Agriculture->TryApplyFieldWork(
        ClearOperation, Field.Cell, InitialRevision, EWorldFieldWorkKind::Till, 1.0f, Retried));
    TestFalse(TEXT("Stale field revision is rejected"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, InitialRevision, EWorldFieldWorkKind::Till, 1.0f, Retried));

    TestTrue(TEXT("Till cleared field"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Till, 1.0f, Field));
    TestTrue(TEXT("Field is tilled"), Field.bTilled);
    const float FertilityBefore = Field.Fertility;
    TestTrue(TEXT("Fertilize field state"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Fertilize, 12.0f, Field));
    TestTrue(TEXT("Fertilizer raises fertility"), Field.Fertility > FertilityBefore);
    TestTrue(TEXT("Mulch field state"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Mulch, 0.25f, Field));
    TestTrue(TEXT("Mulch coverage changes"), Field.MulchCoverage > 0.0f);
    TestTrue(TEXT("Water field state"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Water, 0.4f, Field));
    TestTrue(TEXT("Water raises moisture"), Field.Moisture > 0.2f);
    return true;
}

// P6.2/P6.3: planting consumes one real owned seed only in season, and harvest returns seed items once.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFarmSeasonSeedReturnAndPersistenceTest,
    "MyProject3.World.Farming.SeasonalPlantHarvestReturnsSeedsAndPersistsField", Flags)
bool FFarmSeasonSeedReturnAndPersistenceTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldAgricultureSubsystem* Agriculture = World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Agriculture"), Agriculture) ||
        !TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;
    TestTrue(TEXT("Load primitive-era data"), Data->LoadDefault());
    TestTrue(FString::Printf(TEXT("Apply game data: %s"), *Data->GetLastError()), Data->ApplyToWorld());
    TestTrue(TEXT("Start clock before planting"), Clock->InitializeClock(31415, 0.0));

    FWorldCropDefinition FastCrop;
    FastCrop.CropDefinitionId = TEXT("Crop.FastTest");
    FastCrop.SeedDefinitionId = TEXT("Seed.Grain");
    FastCrop.HarvestDefinitionId = TEXT("Grain.Harvest");
    FastCrop.HarvestUnits = 2;
    FastCrop.SecondsPerStage = 0.5;
    FastCrop.MinimumPlantMoisture = 0.15f;
    FastCrop.PlantMonths.Add(4);
    FastCrop.SeedReturnUnits = 1;
    FastCrop.SoilExhaustionPerHarvest = 8.0f;
    FastCrop.MinimumFertility = 10.0f;
    FastCrop.MaximumPlantWeedPressure = 0.7f;
    TestTrue(TEXT("Register fast test crop"), Agriculture->RegisterCropDefinition(FastCrop));

    AActor* InventoryOwnerActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Inventory owner actor"), InventoryOwnerActor))
        return false;
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(InventoryOwnerActor);
    InventoryOwnerActor->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    Inventory->ItemWeightGrams.Add(TEXT("Seed.Grain"), 50);

    FWorldOwnedItemRecord SeedRecord;
    SeedRecord.ItemId = FGuid::NewGuid();
    SeedRecord.DefinitionId = TEXT("Seed.Grain");
    SeedRecord.BaseDefinitionId = SeedRecord.DefinitionId;
    SeedRecord.Quantity = 1;
    SeedRecord.OwnerKind = EWorldItemOwnerKind::Inventory;
    SeedRecord.OwnerId = Inventory->GetInventoryOwnerId();
    SeedRecord.Revision = 1;
    TestTrue(TEXT("Register unique owned seed"), Ownership->RegisterItem(SeedRecord));

    FInventoryStack SeedStack;
    SeedStack.ItemId = TEXT("Seed.Grain");
    SeedStack.Quantity = 1;
    SeedStack.Category = EInventoryItemCategory::Resource;
    SeedStack.MaxStack = 1;
    SeedStack.OwnedWorldItemId = SeedRecord.ItemId;
    TestTrue(TEXT("Add unique seed reference to inventory"), Inventory->TryAddItem(SeedStack));

    FWorldFieldCellState NewField;
    NewField.Cell = FIntVector(12, 4, 0);
    NewField.Fertility = 60.0f;
    NewField.WeedPressure = 0.8f;
    NewField.MulchCoverage = 0.0f;
    NewField.Moisture = 0.25f;
    TestTrue(TEXT("Register planting plot"), Agriculture->RegisterFieldCell(NewField));

    FWorldFieldCellState Field;
    TestTrue(TEXT("Read field"), Agriculture->GetFieldCell(NewField.Cell, Field));
    TestTrue(TEXT("Clear plot"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Clear, 1.0f, Field));
    TestTrue(TEXT("Till plot"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Till, 1.0f, Field));
    TestTrue(TEXT("Fertilize plot"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Fertilize, 10.0f, Field));
    TestTrue(TEXT("Mulch plot"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Mulch, 0.25f, Field));
    TestTrue(TEXT("Water plot"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Water, 0.5f, Field));

    FGuid CropId;
    const FGuid PlantOperation = FGuid::NewGuid();
    const int64 BeforePlantFieldRevision = Field.Revision;
    TestFalse(TEXT("Out-of-season planting is rejected"), Agriculture->TryPlantCropOnField(
        PlantOperation, FastCrop.CropDefinitionId, SeedRecord.ItemId, SeedRecord.Revision,
        Inventory->GetInventoryOwnerId(), Field.Cell, BeforePlantFieldRevision, Field.Moisture, CropId));
    TestEqual(TEXT("Rejected planting keeps seed in inventory"), Inventory->GetItemCount(TEXT("Seed.Grain")), 1);
    FWorldOwnedItemRecord StillOwnedSeed;
    TestTrue(TEXT("Seed ledger remains available"), Ownership->TryGetItem(SeedRecord.ItemId, StillOwnedSeed));
    TestTrue(TEXT("Rejected planting does not transfer seed ownership"), StillOwnedSeed.OwnerKind == EWorldItemOwnerKind::Inventory);

    TestTrue(TEXT("Advance logical clock to April"), Clock->AdvanceSimulation(16.0 * 3600.0));
    FWorldCalendarDate Date;
    TestTrue(TEXT("April date is available"), Clock->GetCalendarDate(Date));
    TestEqual(TEXT("Current month is April"), Date.Month, 4);
    TestTrue(TEXT("Seasonal planting succeeds"), Agriculture->TryPlantCropOnField(
        PlantOperation, FastCrop.CropDefinitionId, SeedRecord.ItemId, SeedRecord.Revision,
        Inventory->GetInventoryOwnerId(), Field.Cell, BeforePlantFieldRevision, Field.Moisture, CropId));
    FGuid RetriedCropId;
    TestTrue(TEXT("Plant retry returns same crop"), Agriculture->TryPlantCropOnField(
        PlantOperation, FastCrop.CropDefinitionId, SeedRecord.ItemId, SeedRecord.Revision,
        Inventory->GetInventoryOwnerId(), Field.Cell, BeforePlantFieldRevision, Field.Moisture, RetriedCropId));
    TestTrue(TEXT("Plant retry does not create a second crop"), RetriedCropId == CropId);
    TestEqual(TEXT("Planted seed stack is consumed"), Inventory->GetItemCount(TEXT("Seed.Grain")), 0);
    TestTrue(TEXT("Planted field references crop"), Agriculture->GetFieldCell(Field.Cell, Field));
    TestTrue(TEXT("Field holds the planted crop ID"), Field.PlantedCropId == CropId);

    for (int32 Step = 0; Step < 4; ++Step)
        TestTrue(FString::Printf(TEXT("Advance crop growth step %d"), Step), Agriculture->AdvanceAgricultureStep(1.0));
    FWorldCropState MatureCrop;
    TestTrue(TEXT("Crop state exists"), Agriculture->GetCrop(CropId, MatureCrop));
    TestTrue(TEXT("Crop matured from field moisture"), MatureCrop.Stage == ECropGrowthStage::Mature);

    TArray<FGuid> HarvestedItemIds;
    const FGuid HarvestOperation = FGuid::NewGuid();
    const int32 BeforeHarvestItemCount = Ownership->GetTrackedItemCount();
    TestTrue(TEXT("Harvest mature crop"), Agriculture->TryHarvestCrop(
        HarvestOperation, CropId, MatureCrop.Revision, HarvestedItemIds));
    TestEqual(TEXT("Harvest includes two crops plus one seed return"), HarvestedItemIds.Num(), 3);
    FWorldCropState HarvestedCrop;
    TestTrue(TEXT("Harvested crop state exists"), Agriculture->GetCrop(CropId, HarvestedCrop));
    TestEqual(TEXT("Harvest item count"), HarvestedCrop.HarvestOutputItemIds.Num(), 2);
    TestEqual(TEXT("Seed return count"), HarvestedCrop.SeedOutputItemIds.Num(), 1);
    FWorldOwnedItemRecord ReturnedSeed;
    TestTrue(TEXT("Returned seed exists in ownership ledger"), Ownership->TryGetItem(HarvestedCrop.SeedOutputItemIds[0], ReturnedSeed));
    TestEqual(TEXT("Returned item definition is seed"), ReturnedSeed.DefinitionId, FName(TEXT("Seed.Grain")));
    TestTrue(TEXT("Returned seed is back in the world"), ReturnedSeed.OwnerKind == EWorldItemOwnerKind::World);
    TestTrue(TEXT("Harvest releases field cell"), Agriculture->GetFieldCell(Field.Cell, Field));
    TestFalse(TEXT("Harvested field no longer has a live crop link"), Field.PlantedCropId.IsValid());
    TestEqual(TEXT("Soil fertility loses the configured harvest cost"), Field.Fertility, 62.0f);

    TArray<FGuid> RetryOutputs;
    TestTrue(TEXT("Harvest retry replays recorded outputs"), Agriculture->TryHarvestCrop(
        HarvestOperation, CropId, MatureCrop.Revision, RetryOutputs));
    TestTrue(TEXT("Retry output IDs match"), RetryOutputs == HarvestedItemIds);
    TestEqual(TEXT("Retry creates no additional item"), Ownership->GetTrackedItemCount(), BeforeHarvestItemCount + 3);

    TArray<FWorldCropState> SavedCrops;
    TArray<FWorldAnimalState> SavedAnimals;
    TArray<FWorldFieldCellState> SavedFields;
    Agriculture->ExportSnapshot(SavedCrops, SavedAnimals, SavedFields);
    const float SavedFertility = Field.Fertility;
    TestTrue(TEXT("Change field after snapshot"), Agriculture->TryApplyFieldWork(
        FGuid::NewGuid(), Field.Cell, Field.Revision, EWorldFieldWorkKind::Water, 0.1f, Field));
    TestTrue(TEXT("Restore farm snapshot"), Agriculture->RestoreSnapshot(SavedCrops, SavedAnimals, SavedFields));
    TestTrue(TEXT("Read restored field"), Agriculture->GetFieldCell(Field.Cell, Field));
    TestEqual(TEXT("Field fertility round-trips"), Field.Fertility, SavedFertility);
    TestTrue(TEXT("Harvested crop still round-trips"), Agriculture->GetCrop(CropId, HarvestedCrop));
    TestTrue(TEXT("Restored seed outputs retain identity"), HarvestedCrop.SeedOutputItemIds[0] == ReturnedSeed.ItemId);
    return true;
}


// P6.4: live capture, stake/rope pen, item-backed feeding, products and slaughter are retry-safe and saved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimalCapturePenSlaughterLifecycleTest,
    "MyProject3.World.Animals.CapturePenFeedCollectSlaughterAndRestore", Flags)
bool FAnimalCapturePenSlaughterLifecycleTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldAgricultureSubsystem* Agriculture = World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Agriculture"), Agriculture) ||
        !TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;

    TestTrue(TEXT("Load game data"), Data->LoadDefault());
    TestTrue(FString::Printf(TEXT("Apply game data: %s"), *Data->GetLastError()), Data->ApplyToWorld());
    TestTrue(TEXT("Initialize logical clock"), Clock->InitializeClock(9017, 0.0));

    AActor* OwnerActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Inventory owner actor"), OwnerActor))
        return false;
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(OwnerActor);
    OwnerActor->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    TestTrue(TEXT("Apply item weights to inventory"), Data->ApplyToInventory(Inventory));
    Inventory->ItemWeightGrams.Add(TEXT("Stake"), 900);
    Inventory->ItemWeightGrams.Add(TEXT("Rope"), 200);
    Inventory->ItemWeightGrams.Add(TEXT("Net"), 100);
    Inventory->ItemWeightGrams.Add(TEXT("Grass"), 500);

    FInventoryStack Stakes;
    Stakes.ItemId = TEXT("Stake");
    Stakes.Quantity = 4;
    Stakes.Category = EInventoryItemCategory::BuildingPart;
    Stakes.MaxStack = 99;
    TestTrue(TEXT("Add four stakes for pen"), Inventory->TryAddItem(Stakes));

    FInventoryStack Rope;
    Rope.ItemId = TEXT("Rope");
    Rope.Quantity = 1;
    Rope.Category = EInventoryItemCategory::Resource;
    Rope.MaxStack = 99;
    TestTrue(TEXT("Add rope for pen"), Inventory->TryAddItem(Rope));

    auto AddUniqueInventoryItem = [&](FName DefinitionId) -> FWorldOwnedItemRecord
    {
        FWorldOwnedItemRecord Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = DefinitionId;
        Item.BaseDefinitionId = DefinitionId;
        Item.Quantity = 1;
        Item.OwnerKind = EWorldItemOwnerKind::Inventory;
        Item.OwnerId = Inventory->GetInventoryOwnerId();
        Item.Revision = 1;
        TestTrue(FString::Printf(TEXT("Register unique item %s"), *DefinitionId.ToString()),
            Ownership->RegisterItem(Item));

        FInventoryStack Stack;
        Stack.ItemId = DefinitionId;
        Stack.Quantity = 1;
        Stack.Category = DefinitionId == TEXT("Net") ? EInventoryItemCategory::Tool : EInventoryItemCategory::Resource;
        Stack.MaxStack = 1;
        Stack.bIsTool = DefinitionId == TEXT("Net");
        Stack.OwnedWorldItemId = Item.ItemId;
        TestTrue(FString::Printf(TEXT("Put unique item %s in inventory"), *DefinitionId.ToString()),
            Inventory->TryAddItem(Stack));
        return Item;
    };

    const FWorldOwnedItemRecord NetItem = AddUniqueInventoryItem(TEXT("Net"));
    const FWorldOwnedItemRecord FeedItem = AddUniqueInventoryItem(TEXT("Grass"));

    FWorldAnimalState Goat;
    Goat.AnimalId = FGuid::NewGuid();
    Goat.AnimalDefinitionId = TEXT("Animal.Goat");
    Goat.Hunger = 10.0f;
    Goat.Health = 100.0f;
    Goat.Location = FVector(400.0, 50.0, 0.0);
    TestTrue(TEXT("Register wild goat"), Agriculture->RegisterAnimal(Goat));
    FWorldAnimalState Animal;
    TestTrue(TEXT("Read wild goat"), Agriculture->GetAnimal(Goat.AnimalId, Animal));
    TestTrue(TEXT("Game data assigns feed and product definitions"), Animal.FeedDefinitionId == TEXT("Grass") &&
        Animal.ProductDefinitionId == TEXT("Milk.Raw"));
    TestFalse(TEXT("A feed item cannot be used as a capture tool"), Agriculture->TryCaptureAnimal(
        FGuid::NewGuid(), Animal.AnimalId, Animal.Revision, FeedItem.ItemId, FeedItem.Revision,
        Inventory->GetInventoryOwnerId()));

    const FGuid CaptureOperation = FGuid::NewGuid();
    TestTrue(TEXT("Capture goat with a count-based rope stack"), Agriculture->TryCaptureAnimalWithToolStack(
        CaptureOperation, Animal.AnimalId, Animal.Revision, TEXT("Rope"), Inventory->GetInventoryOwnerId()));
    TestTrue(TEXT("Count-based capture retry succeeds without consuming the rope"), Agriculture->TryCaptureAnimalWithToolStack(
        CaptureOperation, Animal.AnimalId, Animal.Revision, TEXT("Rope"), Inventory->GetInventoryOwnerId()));
    TestEqual(TEXT("Rope remains available for pen construction"), Inventory->GetItemCount(TEXT("Rope")), 1);
    TestEqual(TEXT("Unique net remains in inventory"), Inventory->GetItemCount(TEXT("Net")), 1);

    FWorldAnimalState Rabbit;
    Rabbit.AnimalId = FGuid::NewGuid();
    Rabbit.AnimalDefinitionId = TEXT("Animal.Rabbit");
    Rabbit.Health = 20.0f;
    Rabbit.Location = FVector(460.0, 60.0, 0.0);
    TestTrue(TEXT("Register wild rabbit"), Agriculture->RegisterAnimal(Rabbit));
    TestTrue(TEXT("Read wild rabbit"), Agriculture->GetAnimal(Rabbit.AnimalId, Rabbit));
    const FGuid UniqueCaptureOperation = FGuid::NewGuid();
    TestTrue(TEXT("Capture rabbit with a uniquely-owned net"), Agriculture->TryCaptureAnimal(
        UniqueCaptureOperation, Rabbit.AnimalId, Rabbit.Revision, NetItem.ItemId, NetItem.Revision,
        Inventory->GetInventoryOwnerId()));
    TestTrue(TEXT("Unique-item capture retry remains idempotent"), Agriculture->TryCaptureAnimal(
        UniqueCaptureOperation, Rabbit.AnimalId, Rabbit.Revision, NetItem.ItemId, NetItem.Revision,
        Inventory->GetInventoryOwnerId()));
    TestTrue(TEXT("Unique capture retains the net identity"), Agriculture->GetAnimal(Rabbit.AnimalId, Rabbit));
    TestTrue(TEXT("Uniquely-captured rabbit stores tool ID"), Rabbit.CaptureToolItemId == NetItem.ItemId);

    FGuid PenId;
    FGuid RetriedPenId;
    const FGuid BuildPenOperation = FGuid::NewGuid();
    TestTrue(TEXT("Build pen from four stakes and one rope"), Agriculture->TryBuildAnimalPen(
        BuildPenOperation, Inventory->GetInventoryOwnerId(), FVector(600.0, 100.0, 0.0), 4, PenId));
    TestTrue(TEXT("Pen build retry returns the same pen"), Agriculture->TryBuildAnimalPen(
        BuildPenOperation, Inventory->GetInventoryOwnerId(), FVector(600.0, 100.0, 0.0), 4, RetriedPenId));
    TestTrue(TEXT("Pen ID retry is stable"), PenId == RetriedPenId);
    TestEqual(TEXT("Stake materials are consumed"), Inventory->GetItemCount(TEXT("Stake")), 0);
    TestEqual(TEXT("Rope material is consumed"), Inventory->GetItemCount(TEXT("Rope")), 0);

    FWorldAnimalPenState Pen;
    TestTrue(TEXT("Read new pen"), Agriculture->GetAnimalPen(PenId, Pen));
    const FGuid AssignOperation = FGuid::NewGuid();
    TestTrue(TEXT("Assign captured goat to pen"), Agriculture->TryAssignAnimalToPen(
        AssignOperation, Animal.AnimalId, Animal.Revision + 1, PenId, Pen.Revision));
    TestTrue(TEXT("Assignment retry replays successfully"), Agriculture->TryAssignAnimalToPen(
        AssignOperation, Animal.AnimalId, Animal.Revision + 1, PenId, Pen.Revision));
    TestTrue(TEXT("Close pen gate"), Agriculture->TrySetAnimalPenGate(
        FGuid::NewGuid(), PenId, Pen.Revision + 1, true));
    TestFalse(TEXT("A pen containing an animal cannot be opened"), Agriculture->TrySetAnimalPenGate(
        FGuid::NewGuid(), PenId, Pen.Revision + 2, false));

    TestTrue(TEXT("Read penned goat"), Agriculture->GetAnimal(Animal.AnimalId, Animal));
    const FGuid FeedOperation = FGuid::NewGuid();
    TestTrue(TEXT("Feed with the exact owned grass item"), Agriculture->TryFeedAnimal(
        FeedOperation, Animal.AnimalId, Animal.Revision, FeedItem.ItemId, FeedItem.Revision,
        Inventory->GetInventoryOwnerId(), 30.0f));
    TestTrue(TEXT("Feed retry cannot eat grass twice"), Agriculture->TryFeedAnimal(
        FeedOperation, Animal.AnimalId, Animal.Revision, FeedItem.ItemId, FeedItem.Revision,
        Inventory->GetInventoryOwnerId(), 30.0f));
    TestEqual(TEXT("Grass is consumed once"), Inventory->GetItemCount(TEXT("Grass")), 0);

    for (int32 Hour = 0; Hour < 6; ++Hour)
        TestTrue(FString::Printf(TEXT("Advance husbandry hour %d"), Hour + 1), Agriculture->AdvanceAgricultureStep(3600.0));
    TestTrue(TEXT("Read goat after production time"), Agriculture->GetAnimal(Animal.AnimalId, Animal));
    TestTrue(TEXT("Goat produced at least one milk item"), Animal.ProductUnits >= 1);

    TArray<FGuid> MilkIds;
    const FGuid CollectOperation = FGuid::NewGuid();
    TestTrue(TEXT("Collect produced milk"), Agriculture->TryCollectAnimalProducts(
        CollectOperation, Animal.AnimalId, Animal.Revision, MilkIds));
    TestEqual(TEXT("One milk output was created"), MilkIds.Num(), 1);
    TArray<FGuid> MilkRetryIds;
    TestTrue(TEXT("Milk collection retry returns same outputs"), Agriculture->TryCollectAnimalProducts(
        CollectOperation, Animal.AnimalId, Animal.Revision, MilkRetryIds));
    TestTrue(TEXT("Milk output IDs are stable"), MilkRetryIds == MilkIds);
    FWorldOwnedItemRecord Milk;
    TestTrue(TEXT("Milk item is in the ownership ledger"), Ownership->TryGetItem(MilkIds[0], Milk));
    TestTrue(TEXT("Milk has configured definition"), Milk.DefinitionId == TEXT("Milk.Raw"));

    TestTrue(TEXT("Read goat after collection"), Agriculture->GetAnimal(Animal.AnimalId, Animal));
    const FGuid SlaughterOperation = FGuid::NewGuid();
    TArray<FGuid> SlaughterIds;
    const int32 ItemCountBeforeSlaughter = Ownership->GetTrackedItemCount();
    TestTrue(TEXT("Slaughter healthy penned goat with closed gate"), Agriculture->TrySlaughterAnimal(
        SlaughterOperation, Animal.AnimalId, Animal.Revision, SlaughterIds));
    TestEqual(TEXT("Configured goat slaughter creates six outputs"), SlaughterIds.Num(), 6);
    TestEqual(TEXT("Slaughter adds exactly six owned items"), Ownership->GetTrackedItemCount(), ItemCountBeforeSlaughter + 6);

    FWorldAnimalState SlaughteredAnimal;
    TestTrue(TEXT("Read slaughtered animal state"), Agriculture->GetAnimal(Animal.AnimalId, SlaughteredAnimal));
    TestTrue(TEXT("Slaughter marks animal dead"), SlaughteredAnimal.bSlaughtered && SlaughteredAnimal.Health == 0.0f);
    TestFalse(TEXT("Slaughter clears animal's pen link"), SlaughteredAnimal.PenId.IsValid());
    FWorldAnimalPenState PenAfterSlaughter;
    TestTrue(TEXT("Read pen after slaughter"), Agriculture->GetAnimalPen(PenId, PenAfterSlaughter));
    TestEqual(TEXT("Slaughter releases one pen slot"), PenAfterSlaughter.AnimalIds.Num(), 0);
    TArray<FGuid> SlaughterRetry;
    TestTrue(TEXT("Slaughter retry replays outputs"), Agriculture->TrySlaughterAnimal(
        SlaughterOperation, Animal.AnimalId, Animal.Revision, SlaughterRetry));
    TestTrue(TEXT("Slaughter output IDs remain stable"), SlaughterRetry == SlaughterIds);
    TestEqual(TEXT("Slaughter retry creates no new items"), Ownership->GetTrackedItemCount(), ItemCountBeforeSlaughter + 6);

    TArray<FWorldCropState> SavedCrops;
    TArray<FWorldAnimalState> SavedAnimals;
    TArray<FWorldFieldCellState> SavedFields;
    TArray<FWorldAnimalPenState> SavedPens;
    Agriculture->ExportSnapshot(SavedCrops, SavedAnimals, SavedFields, SavedPens);
    TestTrue(TEXT("Open empty pen after snapshot"), Agriculture->TrySetAnimalPenGate(
        FGuid::NewGuid(), PenId, PenAfterSlaughter.Revision, false));
    TestTrue(TEXT("Restore crops, animals, fields and pens together"), Agriculture->RestoreSnapshot(
        SavedCrops, SavedAnimals, SavedFields, SavedPens));
    TestTrue(TEXT("Read restored closed pen"), Agriculture->GetAnimalPen(PenId, PenAfterSlaughter));
    TestTrue(TEXT("Pen gate state is restored"), PenAfterSlaughter.bGateClosed);
    TestTrue(TEXT("Read restored slaughter record"), Agriculture->GetAnimal(Animal.AnimalId, SlaughteredAnimal));
    TestTrue(TEXT("Slaughter output identity persists"), SlaughteredAnimal.SlaughterOutputItemIds == SlaughterIds);
    return true;
}


// P7.2/P7.4: the configured creek bridge makes the route cheaper, and built state survives graph snapshots.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfiguredZoneBridgeRouteCostTest,
    "MyProject3.World.Traversal.ConfiguredBridgeReducesCreekRouteCostAndPersists", Flags)
bool FConfiguredZoneBridgeRouteCostTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldLogisticsRouteSubsystem* Routes = World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Route graph"), Routes))
        return false;

    TestTrue(TEXT("Load zone and route data"), Data->LoadDefault());
    TestTrue(FString::Printf(TEXT("Apply route data: %s"), *Data->GetLastError()), Data->ApplyToWorld());
    FLogisticsRouteCostContext Context;
    Context.CarriedMassKg = 0.0f;
    Context.MaxCarryMassKg = 100.0f;
    Context.WeatherSeverity = 0.0f;

    TArray<FName> BeforePath;
    float BeforeCost = 0.0f;
    TestTrue(TEXT("Find creek path before bridge construction"), Routes->FindRouteWithContext(
        TEXT("Route.Camp"), TEXT("Route.Creek.EastBank"), Context, BeforePath, BeforeCost));
    TestTrue(TEXT("Pre-bridge route uses the slower ford"), BeforePath.Contains(TEXT("Route.Ford.WestBank")) &&
        BeforePath.Contains(TEXT("Route.Ford.EastBank")));

    TestTrue(TEXT("Mark creek bridge as built"), Routes->SetInfrastructureBuilt(TEXT("Infrastructure.CreekBridge"), true));
    TArray<FName> AfterPath;
    float AfterCost = 0.0f;
    TestTrue(TEXT("Find creek path after bridge construction"), Routes->FindRouteWithContext(
        TEXT("Route.Camp"), TEXT("Route.Creek.EastBank"), Context, AfterPath, AfterCost));
    TestTrue(TEXT("Built bridge route is cheaper"), AfterCost < BeforeCost);
    TestTrue(TEXT("Post-bridge route crosses the direct bridge edge"),
        AfterPath.Contains(TEXT("Route.Creek.WestBank")) && AfterPath.Contains(TEXT("Route.Creek.EastBank")));
    TestFalse(TEXT("Post-bridge route does not detour through the ford"),
        AfterPath.Contains(TEXT("Route.Ford.WestBank")) || AfterPath.Contains(TEXT("Route.Ford.EastBank")));

    TArray<FLogisticsRouteNode> SavedNodes;
    TArray<FLogisticsRouteEdge> SavedEdges;
    Routes->ExportSnapshot(SavedNodes, SavedEdges);
    TestTrue(TEXT("Temporarily remove bridge-built state"), Routes->SetInfrastructureBuilt(TEXT("Infrastructure.CreekBridge"), false));
    float UnbuiltCost = 0.0f;
    TArray<FName> UnbuiltPath;
    TestTrue(TEXT("Route remains possible via ford"), Routes->FindRouteWithContext(
        TEXT("Route.Camp"), TEXT("Route.Creek.EastBank"), Context, UnbuiltPath, UnbuiltCost));
    TestTrue(TEXT("Graph snapshot restores infrastructure state"),
        Routes->RestoreSnapshot(SavedNodes, SavedEdges));
    float RestoredCost = 0.0f;
    TArray<FName> RestoredPath;
    TestTrue(TEXT("Recompute route after restore"), Routes->FindRouteWithContext(
        TEXT("Route.Camp"), TEXT("Route.Creek.EastBank"), Context, RestoredPath, RestoredCost));
    TestTrue(TEXT("Built bridge cost is restored"), FMath::IsNearlyEqual(RestoredCost, AfterCost, 0.001f));
    TestTrue(TEXT("Built bridge route remains direct after restore"), RestoredPath.Contains(TEXT("Route.Creek.WestBank")) &&
        RestoredPath.Contains(TEXT("Route.Creek.EastBank")));
    return true;
}


/** P8.1: regional event definitions are validated and derive the same active set for the same world state. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegionalEventDefinitionsDeterminismTest,
    "MyProject3.World.Events.RegionalDefinitionsAreValidAndDeterministic", Flags)
bool FRegionalEventDefinitionsDeterminismTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Regional events"), Events) ||
        !TestNotNull(TEXT("Clock"), Clock) ||
        !TestNotNull(TEXT("Climate"), Climate))
        return false;

    TestTrue(FString::Printf(TEXT("Load events.json: %s"), *Events->GetLastError()),
        Events->LoadDefaultDefinitions());
    const TArray<FName> Regions = Events->GetRegionIds();
    TestEqual(TEXT("Five configured world regions"), Regions.Num(), 5);
    TestTrue(TEXT("Region IDs use stable lexical order"),
        Regions.Num() == 5 && Regions[0] == FName(TEXT("Zone.0")) &&
        Regions[4] == FName(TEXT("Zone.4")));

    const double WinterStart = 16.0 * 3600.0 + 45.0 * Day;
    TestTrue(TEXT("Initialize deterministic clock"), Clock->InitializeClock(481, WinterStart));
    TestTrue(TEXT("Initialize seasonal climate"), Climate->InitializeClimate());
    TestEqual(TEXT("Winter snow is seeded from the calendar"), Climate->GetSnowDepthCm(), 40.0f);

    FWorldRegionProfile Mountain;
    TestTrue(TEXT("Read mountain-pass profile"), Events->GetRegionProfile(TEXT("Zone.4"), Mountain));
    TestTrue(TEXT("Mountain profile has snow-load tuning"), Mountain.SnowLoadUnitsPerCmM2 > 0.0f);
    FWorldRegionalEventEvaluation First;
    FWorldRegionalEventEvaluation Retry;
    TestTrue(TEXT("Evaluate mountain event conditions"), Events->EvaluateRegion(TEXT("Zone.4"), First));
    TestTrue(TEXT("Repeat regional event evaluation"), Events->EvaluateRegion(TEXT("Zone.4"), Retry));
    TestTrue(TEXT("Active event list is deterministic"),
        First.ActiveEventIds == Retry.ActiveEventIds);
    TestTrue(TEXT("Route multiplier is deterministic"),
        FMath::IsNearlyEqual(First.RouteCostMultiplier, Retry.RouteCostMultiplier));
    TestTrue(TEXT("Raid modifier is deterministic"),
        FMath::IsNearlyEqual(First.RaidDefenseMultiplier, Retry.RaidDefenseMultiplier));
    return true;
}

/** P8.2: store/smell/light/smoke inputs can trigger a region-specific, receipt-protected raid. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSignalDrivenRaidAndPersistenceTest,
    "MyProject3.World.Events.SignalRaidIsRegionTargetedAndIdempotent", Flags)
bool FSignalDrivenRaidAndPersistenceTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    UWorldThreatDirectorSubsystem* Threat =
        World ? World->GetSubsystem<UWorldThreatDirectorSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph =
        World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) ||
        !TestNotNull(TEXT("Regional events"), Events) ||
        !TestNotNull(TEXT("Threat director"), Threat) ||
        !TestNotNull(TEXT("Structural graph"), Graph))
        return false;

    TestTrue(TEXT("Initialize clock"), Clock->InitializeClock(57));
    TestTrue(TEXT("Load regional event profiles"), Events->LoadDefaultDefinitions());
    TestTrue(TEXT("Initialize threat director"), Threat->InitializeDirector(1800.0));

    AActor* DefenseActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Defense actor"), DefenseActor))
        return false;
    DefenseActor->Tags.Add(FName(TEXT("Region:Zone.1")));
    DefenseActor->Tags.Add(FName(TEXT("RaidDefense:1.0")));
    const FGuid DefenseNodeId(21, 22, 23, 24);
    TestTrue(TEXT("Register supported regional defense"),
        Graph->RegisterNode(DefenseActor, DefenseNodeId, EStructuralNodeKind::Wood, true, 1.0f, 100.0f, 100.0f));
    TestEqual(TEXT("Only the matching region receives defense rating"),
        Graph->GetRegionDefenseRating(TEXT("Zone.1")), 1.0f);
    TestEqual(TEXT("Other regions do not inherit the defense rating"),
        Graph->GetRegionDefenseRating(TEXT("Zone.0")), 0.0f);

    const FGuid OperationId(11, 22, 33, 44);
    bool bStarted = false;
    TestTrue(TEXT("Accept strong settlement signals"),
        Threat->TryStartRaidFromSignals(OperationId, TEXT("Zone.1"), 1.0f, 1.0f, 1.0f, 1.0f, bStarted));
    TestTrue(TEXT("Signals exceed threshold and start warning"), bStarted);

    FThreatRaidRecord Raid;
    TestTrue(TEXT("Read the triggered raid"), Threat->GetCurrentRaid(Raid));
    TestTrue(TEXT("Raid target is the signalled region"), Raid.TargetRegionId == FName(TEXT("Zone.1")));
    TestTrue(TEXT("Raid uses the configured creek-region target"),
        Raid.TargetLocation.Equals(FVector(1400.0f, 800.0f, 0.0f), 0.01f));

    const FWorldThreatDirectorSnapshot Saved = Threat->ExportSnapshot();
    TestEqual(TEXT("Signal receipts use snapshot schema 2"), Saved.SchemaVersion, 2);
    TestTrue(TEXT("Restore raid and signal receipts"), Threat->RestoreSnapshot(Saved));
    bStarted = true;
    TestTrue(TEXT("Retry is an idempotent successful no-op"),
        Threat->TryStartRaidFromSignals(OperationId, TEXT("Zone.1"), 1.0f, 1.0f, 1.0f, 1.0f, bStarted));
    TestFalse(TEXT("Duplicate operation does not report a fresh start"), bStarted);

    FWorldThreatDirectorSnapshot LegacySnapshot = Saved;
    LegacySnapshot.SchemaVersion = 1;
    LegacySnapshot.SeenSignalOperationIds.Reset();
    LegacySnapshot.LastSignalRaidTimeSeconds = -1.0;
    TestTrue(TEXT("Restore legacy threat snapshot schema v1"), Threat->RestoreSnapshot(LegacySnapshot));
    return true;
}

/** P8.3: exposed structural members carry climate-driven snow load and stop supporting an overloaded frame. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStructuralSnowLoadAffectsSupportTest,
    "MyProject3.World.Events.SnowLoadCanOverloadExposedStructure", Flags)
bool FStructuralSnowLoadAffectsSupportTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph =
        World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldRaidAdapterSubsystem* HazardAdapter =
        World ? World->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) ||
        !TestNotNull(TEXT("Climate"), Climate) ||
        !TestNotNull(TEXT("Regional events"), Events) ||
        !TestNotNull(TEXT("Structural graph"), Graph) ||
        !TestNotNull(TEXT("Hazard adapter"), HazardAdapter))
        return false;

    const double WinterStart = 16.0 * 3600.0 + 45.0 * Day;
    TestTrue(TEXT("Start winter clock"), Clock->InitializeClock(913, WinterStart));
    TestTrue(TEXT("Load event tuning"), Events->LoadDefaultDefinitions());
    TestTrue(TEXT("Initialize winter climate"), Climate->InitializeClimate());

    AActor* Roof = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Roof test actor"), Roof))
        return false;
    Roof->Tags.Add(FName(TEXT("Region:Zone.4")));
    Roof->Tags.Add(FName(TEXT("SnowExposed")));
    Roof->Tags.Add(FName(TEXT("SnowAreaM2:4.0")));
    const FGuid NodeId(91, 92, 93, 94);
    TestTrue(TEXT("Register exposed roof support"),
        Graph->RegisterNode(Roof, NodeId, EStructuralNodeKind::Wood, true, 20.0f, 50.0f, 100.0f));

    const float SnowLoad = Graph->GetNodeSnowLoad(NodeId);
    TestTrue(TEXT("Snow load is calculated from depth, area and region tuning"), SnowLoad >= 48.0f);
    TestTrue(TEXT("Aggregate load includes environmental snow"), Graph->GetAggregateLoad(NodeId) > 50.0f);
    TestFalse(TEXT("Overloaded exposed member cannot claim a valid support path"),
        Graph->HasValidSupportPath(NodeId));
    TestTrue(TEXT("Fixed-step hazard pass resolves the snow overload"),
        HazardAdapter->AdvanceRegionalHazardDamage(60.0));
    FStructuralNodeRecord CollapsedRoof;
    TestFalse(TEXT("Snow-overloaded member is removed by the collapse resolver"),
        Graph->GetNode(NodeId, CollapsedRoof));
    return true;
}


/** P8.1: route edges with a region ID consume that region's live event cost multiplier. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegionalHazardChangesRouteCostTest,
    "MyProject3.World.Events.RegionalHazardsIncreaseRouteCost", Flags)
bool FRegionalHazardChangesRouteCostTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate =
        World ? World->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    UWorldLogisticsRouteSubsystem* Routes =
        World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph =
        World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldRaidAdapterSubsystem* HazardAdapter =
        World ? World->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) ||
        !TestNotNull(TEXT("Climate"), Climate) ||
        !TestNotNull(TEXT("Regional events"), Events) ||
        !TestNotNull(TEXT("Route graph"), Routes) ||
        !TestNotNull(TEXT("Structural graph"), Graph) ||
        !TestNotNull(TEXT("Hazard adapter"), HazardAdapter))
        return false;

    // The event always occurs during day 3: chance=1, and the event window
    // begins at a deterministic time in that day and lasts for 72 hours.
    const double LateInThirdDay = 16.0 * 3600.0 + 2.0 * Day + 23.9 * 3600.0;
    TestTrue(TEXT("Initialize clock inside the event window"), Clock->InitializeClock(321, LateInThirdDay));
    TestTrue(TEXT("Initialize climate"), Climate->InitializeClimate());

    const FString TestDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("P8RouteEventTest"));
    IFileManager::Get().MakeDirectory(*TestDirectory, true);
    const FString EventJson = TEXT(
        "{\"regions\":[{\"id\":\"Zone.0\",\"elevation_m\":0,\"slope_ratio\":0,"
        "\"flood_exposure\":0,\"vegetation_fuel_load\":0,\"snow_load_units_per_cm_m2\":0.1,"
        "\"raid_target\":[0,0,0]}],"
        "\"events\":[{\"id\":\"Event.RouteTest\",\"type\":\"Flood\","
        "\"regions\":[\"Zone.0\"],\"months\":[],\"required_weather\":[],"
        "\"chance_per_day\":1,\"duration_hours\":72,\"route_cost_multiplier\":2,"
        "\"structure_damage_per_hour\":2,\"raid_defense_multiplier\":1,\"snow_load_multiplier\":1,"
        "\"survival_severity_multiplier\":1}]}");
    const FString EventPath = FPaths::Combine(TestDirectory, TEXT("events.json"));
    TestTrue(TEXT("Write temporary deterministic event fixture"),
        FFileHelper::SaveStringToFile(EventJson, *EventPath));
    TestTrue(FString::Printf(TEXT("Load event fixture: %s"), *Events->GetLastError()),
        Events->LoadDefinitionsFromDirectory(TestDirectory));

    FWorldRegionalEventEvaluation Evaluation;
    TestTrue(TEXT("Evaluate the test region"), Events->EvaluateRegion(TEXT("Zone.0"), Evaluation));
    TestTrue(TEXT("Test event is active"), Evaluation.ActiveEventIds.Contains(TEXT("Event.RouteTest")));
    TestEqual(TEXT("Regional event doubles route costs"), Evaluation.RouteCostMultiplier, 2.0f);

    FLogisticsRouteNode Start;
    Start.NodeId = TEXT("Route.Test.Start");
    FLogisticsRouteNode End;
    End.NodeId = TEXT("Route.Test.End");
    TestTrue(TEXT("Add start node"), Routes->UpsertNode(Start));
    TestTrue(TEXT("Add end node"), Routes->UpsertNode(End));
    FLogisticsRouteEdge Edge;
    Edge.FromNode = Start.NodeId;
    Edge.ToNode = End.NodeId;
    Edge.RegionId = TEXT("Zone.0");
    Edge.Cost = 10.0f;
    TestTrue(TEXT("Add region-tagged edge"), Routes->UpsertEdge(Edge));

    float Cost = 0.0f;
    TestTrue(TEXT("Compute event-adjusted edge cost"), Routes->ComputeEdgeCost(
        Edge, FLogisticsRouteCostContext(), Cost));
    TestEqual(TEXT("Route cost applies regional event multiplier"), Cost, 20.0f, 0.001f);

    AActor* RegionalWall = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Regional wall actor"), RegionalWall))
        return false;
    RegionalWall->Tags.Add(FName(TEXT("Region:Zone.0")));
    const FGuid WallNodeId(31, 32, 33, 34);
    TestTrue(TEXT("Register wall in hazard region"),
        Graph->RegisterNode(RegionalWall, WallNodeId, EStructuralNodeKind::Wood,
            true, 1.0f, 100.0f, 100.0f));
    TestTrue(TEXT("Apply one fixed hour of regional event damage"),
        HazardAdapter->AdvanceRegionalHazardDamage(3600.0));
    FStructuralNodeRecord WallAfterDamage;
    TestTrue(TEXT("Read wall after hazard damage"), Graph->GetNode(WallNodeId, WallAfterDamage));
    TestEqual(TEXT("Regional event damage rate is applied per simulation hour"),
        WallAfterDamage.Damage, 2.0f, 0.001f);

    IFileManager::Get().Delete(*EventPath, false, true);
    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    return true;
}


/** P8.3: a repair consumes one owned material item and replays the same committed result only once. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaterialBackedStructuralRepairIsIdempotent,
    "MyProject3.World.Events.StructuralRepairConsumesOwnedMaterialOnce", Flags)
bool FMaterialBackedStructuralRepairIsIdempotent::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* GameData = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph = World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldRaidAdapterSubsystem* HazardAdapter = World ? World->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), GameData) ||
        !TestNotNull(TEXT("Ownership"), Ownership) ||
        !TestNotNull(TEXT("Structural graph"), Graph) ||
        !TestNotNull(TEXT("Hazard adapter"), HazardAdapter))
        return false;

    TestTrue(TEXT("Load game data for material category validation"), GameData->LoadDefault());

    AActor* InventoryActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Inventory actor"), InventoryActor))
        return false;
    UInventoryCraftingComponent* Inventory =
        NewObject<UInventoryCraftingComponent>(InventoryActor);
    InventoryActor->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    TestTrue(TEXT("Inventory owner identity"), InventoryOwnerId.IsValid());

    FWorldOwnedItemRecord Stone;
    Stone.ItemId = FGuid(101, 102, 103, 104);
    Stone.DefinitionId = TEXT("Stone");
    Stone.BaseDefinitionId = Stone.DefinitionId;
    Stone.OwnerKind = EWorldItemOwnerKind::Inventory;
    Stone.OwnerId = InventoryOwnerId;
    Stone.Quantity = 1;
    TestTrue(TEXT("Register owned repair stone"), Ownership->RegisterItem(Stone));

    Inventory->ItemWeightGrams.Add(TEXT("Stone"), 500);
    FInventoryStack StoneStack;
    StoneStack.ItemId = TEXT("Stone");
    StoneStack.Quantity = 1;
    StoneStack.MaxStack = 1;
    StoneStack.OwnedWorldItemId = Stone.ItemId;
    TArray<FInventoryStack> InventoryStacks;
    InventoryStacks.Add(StoneStack);
    TestTrue(TEXT("Put unique stone into inventory"),
        Inventory->RestoreSnapshot(InventoryOwnerId, InventoryStacks, NAME_None));

    AActor* Wall = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Wall actor"), Wall))
        return false;
    Wall->Tags.Add(FName(TEXT("Region:Zone.0")));
    const FGuid WallNodeId(111, 112, 113, 114);
    TestTrue(TEXT("Register wall node"),
        Graph->RegisterNode(Wall, WallNodeId, EStructuralNodeKind::Wood, true, 1.0f, 100.0f, 100.0f));

    bool bThresholdExceeded = false;
    TestTrue(TEXT("Damage wall before repair"), Graph->ApplyDamage(WallNodeId, 40.0f, bThresholdExceeded));
    FStructuralNodeRecord DamagedWall;
    TestTrue(TEXT("Read damaged wall"), Graph->GetNode(WallNodeId, DamagedWall));
    FWorldOwnedItemRecord OwnedStone;
    TestTrue(TEXT("Read owned material record"), Ownership->TryGetItem(Stone.ItemId, OwnedStone));

    const FGuid RepairOperation(121, 122, 123, 124);
    TestTrue(TEXT("Repair wall with one owned stone"), HazardAdapter->TryRepairStructureWithMaterial(
        RepairOperation, WallNodeId, DamagedWall.Revision, Stone.ItemId,
        OwnedStone.Revision, InventoryOwnerId, 25.0f));

    FStructuralNodeRecord RepairedWall;
    TestTrue(TEXT("Read repaired wall"), Graph->GetNode(WallNodeId, RepairedWall));
    TestEqual(TEXT("Repair reduces structural damage by configured amount"), RepairedWall.Damage, 15.0f, 0.001f);
    FWorldOwnedItemRecord ConsumedStone;
    TestTrue(TEXT("Read consumed repair stone"), Ownership->TryGetItem(Stone.ItemId, ConsumedStone));
    TestTrue(TEXT("Repair material leaves inventory ownership"),
        ConsumedStone.OwnerKind == EWorldItemOwnerKind::Consumed);
    TestEqual(TEXT("Repair material stack is removed"), Inventory->GetItemCount(TEXT("Stone")), 0);

    TestTrue(TEXT("Retry returns original repair receipt"),
        HazardAdapter->TryRepairStructureWithMaterial(RepairOperation, WallNodeId,
            DamagedWall.Revision, Stone.ItemId, OwnedStone.Revision, InventoryOwnerId, 25.0f));
    FStructuralNodeRecord AfterRetry;
    TestTrue(TEXT("Read wall after repair retry"), Graph->GetNode(WallNodeId, AfterRetry));
    TestEqual(TEXT("Retry does not repair twice"), AfterRetry.Damage, 15.0f, 0.001f);
    return true;
}




/** P8.2: region-tagged world actors feed immutable per-tick raid sensor samples. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegionalRaidSignalSensorScanTest,
    "MyProject3.World.Events.RegionTaggedSensorsCanTriggerRaid", Flags)
bool FRegionalRaidSignalSensorScanTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldRegionalEventSubsystem* Events =
        World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    UWorldThreatDirectorSubsystem* Threat =
        World ? World->GetSubsystem<UWorldThreatDirectorSubsystem>() : nullptr;
    UWorldRaidAdapterSubsystem* Adapter =
        World ? World->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Clock"), Clock) ||
        !TestNotNull(TEXT("Regional events"), Events) ||
        !TestNotNull(TEXT("Threat director"), Threat) ||
        !TestNotNull(TEXT("Raid adapter"), Adapter))
        return false;

    TestTrue(TEXT("Initialize clock"), Clock->InitializeClock(77));
    TestTrue(TEXT("Load region profiles"), Events->LoadDefaultDefinitions());
    TestTrue(TEXT("Initialize threat director"), Threat->InitializeDirector(3600.0));

    AActor* Store = World->SpawnActor<AActor>();
    AActor* FoodRack = World->SpawnActor<AActor>();
    AActor* Fire = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Store sensor actor"), Store) ||
        !TestNotNull(TEXT("Food rack sensor actor"), FoodRack) ||
        !TestNotNull(TEXT("Fire sensor actor"), Fire))
        return false;

    Store->Tags.Add(FName(TEXT("Region:Zone.2")));
    Store->Tags.Add(FName(TEXT("RaidSignal.Storage:0.8")));
    FoodRack->Tags.Add(FName(TEXT("Region:Zone.2")));
    FoodRack->Tags.Add(FName(TEXT("RaidSignal.Smell:0.8")));
    Fire->Tags.Add(FName(TEXT("Region:Zone.2")));
    Fire->Tags.Add(FName(TEXT("RaidSignal.Light:0.5")));
    Fire->Tags.Add(FName(TEXT("RaidSignal.Smoke:0.5")));

    TestTrue(TEXT("Scan region-tagged sensor contributions"), Adapter->AdvanceRegionalRaidSignals());
    FThreatRaidRecord Raid;
    TestTrue(TEXT("Sensor sample triggers a warning"), Threat->GetCurrentRaid(Raid));
    TestTrue(TEXT("Raid targets the sensor region"), Raid.TargetRegionId == FName(TEXT("Zone.2")));
    const FGuid FirstRaidId = Raid.RaidId;

    TestTrue(TEXT("Repeated same tick scan is a no-op"), Adapter->AdvanceRegionalRaidSignals());
    TestTrue(TEXT("Read raid after retry"), Threat->GetCurrentRaid(Raid));
    TestTrue(TEXT("Retry does not replace the existing raid"), Raid.RaidId == FirstRaidId);
    return true;
}


// Faz 6 — the 15-step field catalogue applies tool/material gates to transactional field work.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataDrivenFieldActionTransactionTest,
    "MyProject3.World.Farming.DataDrivenFieldActionsConsumeCostsAndKeepTools", Flags)
bool FDataDrivenFieldActionTransactionTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldAgricultureSubsystem* Agriculture = World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Agriculture"), Agriculture))
        return false;
    TestTrue(TEXT("Load game data"), Data->LoadDefault());

    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Inventory owner"), Owner))
        return false;
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(Owner);
    Owner->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    TestTrue(TEXT("Apply item definitions and weights"), Data->ApplyToInventory(Inventory));

    auto AddStack = [&](FName Id, int32 Quantity, bool bTool)
    {
        FInventoryStack Stack;
        Stack.ItemId = Id;
        Stack.Quantity = Quantity;
        Stack.MaxStack = 99;
        Stack.Category = bTool ? EInventoryItemCategory::Tool : EInventoryItemCategory::Resource;
        Stack.bIsTool = bTool;
        return Inventory->TryAddItem(Stack);
    };
    TestTrue(TEXT("Add digging stick"), AddStack(TEXT("Tool.DiggingStick"), 1, true));
    TestTrue(TEXT("Add hoe"), AddStack(TEXT("Tool.Hoe"), 1, true));
    TestTrue(TEXT("Add compost"), AddStack(TEXT("Compost"), 1, false));

    FWorldFieldCellState Cell;
    Cell.Cell = FIntVector(29, 7, 0);
    Cell.Fertility = 40.0f;
    Cell.WeedPressure = 0.7f;
    Cell.Moisture = 0.2f;
    TestTrue(TEXT("Register field"), Agriculture->RegisterFieldCell(Cell));
    TestTrue(TEXT("Read field"), Agriculture->GetFieldCell(Cell.Cell, Cell));

    TestTrue(TEXT("Clear requires the digging stick"),
        Agriculture->TryApplyFieldAction(FGuid::NewGuid(), TEXT("Field.Clear"),
            Cell.Cell, Cell.Revision, Inventory->GetInventoryOwnerId(), Cell));
    TestTrue(TEXT("Field becomes clear"), Cell.bCleared);
    TestTrue(TEXT("Till requires the hoe"),
        Agriculture->TryApplyFieldAction(FGuid::NewGuid(), TEXT("Field.Till"),
            Cell.Cell, Cell.Revision, Inventory->GetInventoryOwnerId(), Cell));
    TestTrue(TEXT("Field becomes tilled"), Cell.bTilled);

    const FGuid FertilizeOperation = FGuid::NewGuid();
    const int64 FertilizeRevision = Cell.Revision;
    const float BeforeFertilize = Cell.Fertility;
    TestTrue(TEXT("Fertilize requires hoe and compost"),
        Agriculture->TryApplyFieldAction(FertilizeOperation, TEXT("Field.Fertilize"),
            Cell.Cell, Cell.Revision, Inventory->GetInventoryOwnerId(), Cell));
    TestTrue(TEXT("Fertilizer raises field fertility"), Cell.Fertility > BeforeFertilize);
    TestEqual(TEXT("Compost is consumed once"), Inventory->GetItemCount(TEXT("Compost")), 0);
    TestEqual(TEXT("Tools are not consumed"), Inventory->GetItemCount(TEXT("Tool.Hoe")), 1);
    TestEqual(TEXT("Digging tool remains"), Inventory->GetItemCount(TEXT("Tool.DiggingStick")), 1);

    const float AfterFertilize = Cell.Fertility;
    FWorldFieldCellState Retry;
    TestTrue(TEXT("Fertilize retry returns committed result"),
        Agriculture->TryApplyFieldAction(FertilizeOperation, TEXT("Field.Fertilize"),
            Cell.Cell, FertilizeRevision, Inventory->GetInventoryOwnerId(), Retry));
    TestEqual(TEXT("Retry does not apply fertilizer twice"), Retry.Fertility, AfterFertilize);
    TestEqual(TEXT("Retry does not consume a second compost"), Inventory->GetItemCount(TEXT("Compost")), 0);
    return true;
}

// Faz 7 — infrastructure condition affects route cost and zero condition closes required edges.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteInfrastructureDurabilityTest,
    "MyProject3.Logistics.InfrastructureWearRepairAndFailureAffectRouteCost", Flags)
bool FRouteInfrastructureDurabilityTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldLogisticsRouteSubsystem* Routes = World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Routes"), Routes))
        return false;
    TestTrue(TEXT("Load game data"), Data->LoadDefault());
    TestTrue(TEXT("Apply route graph"), Data->ApplyToWorld());
    const FName InfrastructureId(TEXT("Infrastructure.HillSteps"));
    TestTrue(TEXT("Mark hill steps built"), Routes->SetInfrastructureBuilt(InfrastructureId, true));
    TestTrue(TEXT("Apply configured built route multiplier"),
        Routes->SetInfrastructureBuiltCostMultiplier(InfrastructureId, 0.7f));

    TArray<FLogisticsRouteNode> Nodes;
    TArray<FLogisticsRouteEdge> Edges;
    Routes->ExportSnapshot(Nodes, Edges);
    FLogisticsRouteEdge* Edge = Edges.FindByPredicate(
        [&InfrastructureId](const FLogisticsRouteEdge& Candidate)
        { return Candidate.InfrastructureId == InfrastructureId && Candidate.bRequiresInfrastructure; });
    if (!TestNotNull(TEXT("Required hill-step route edge"), Edge))
        return false;

    FLogisticsRouteCostContext Context;
    float MaintainedCost = 0.0f;
    TestTrue(TEXT("Compute maintained route cost"), Routes->ComputeEdgeCost(*Edge, Context, MaintainedCost));
    TestTrue(TEXT("Apply storm wear to the infrastructure"),
        Routes->ApplyInfrastructureWear(InfrastructureId, 50.0f));
    float Condition = 0.0f;
    float MaxCondition = 0.0f;
    bool bBuilt = false;
    TestTrue(TEXT("Read worn infrastructure state"),
        Routes->GetInfrastructureState(InfrastructureId, Condition, MaxCondition, bBuilt));
    TestEqual(TEXT("Wear reduces condition"), Condition, 50.0f);
    TestTrue(TEXT("Worn steps remain built until destroyed"), bBuilt);

    FLogisticsRouteEdge CurrentEdge;
    Routes->ExportSnapshot(Nodes, Edges);
    const FLogisticsRouteEdge* WornEdge = Edges.FindByPredicate(
        [&InfrastructureId](const FLogisticsRouteEdge& Candidate)
        { return Candidate.InfrastructureId == InfrastructureId && Candidate.bRequiresInfrastructure; });
    if (!TestNotNull(TEXT("Worn required edge"), WornEdge))
        return false;
    CurrentEdge = *WornEdge;
    float WornCost = 0.0f;
    TestTrue(TEXT("Compute worn route cost"), Routes->ComputeEdgeCost(CurrentEdge, Context, WornCost));
    TestTrue(TEXT("Wear increases route cost"), WornCost > MaintainedCost);

    TestTrue(TEXT("Repair with condition points"), Routes->RepairInfrastructure(InfrastructureId, 25.0f));
    TestTrue(TEXT("Read repaired infrastructure"),
        Routes->GetInfrastructureState(InfrastructureId, Condition, MaxCondition, bBuilt));
    TestEqual(TEXT("Repair restores twenty-five condition"), Condition, 75.0f);

    TestTrue(TEXT("Further weather wear destroys the route infrastructure"),
        Routes->ApplyInfrastructureWear(InfrastructureId, 100.0f));
    TestTrue(TEXT("Read destroyed infrastructure"),
        Routes->GetInfrastructureState(InfrastructureId, Condition, MaxCondition, bBuilt));
    TestEqual(TEXT("Condition reaches zero"), Condition, 0.0f);
    TestFalse(TEXT("Destroyed steps are no longer built"), bBuilt);
    Routes->ExportSnapshot(Nodes, Edges);
    const FLogisticsRouteEdge* FailedEdge = Edges.FindByPredicate(
        [&InfrastructureId](const FLogisticsRouteEdge& Candidate)
        { return Candidate.InfrastructureId == InfrastructureId && Candidate.bRequiresInfrastructure; });
    if (!TestNotNull(TEXT("Failed required edge"), FailedEdge))
        return false;
    TestFalse(TEXT("Required route edge is unavailable after failure"),
        Routes->ComputeEdgeCost(*FailedEdge, Context, WornCost));
    return true;
}

// Faz 8 — recipe repair consumes the correct wood materials, keeps tools and deduplicates retries.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWoodStoneRecipeRepairTest,
    "MyProject3.World.Structures.TypedWoodRepairConsumesRecipeOnce", Flags)
bool FWoodStoneRecipeRepairTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph = World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldRaidAdapterSubsystem* Adapter = World ? World->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game data"), Data) || !TestNotNull(TEXT("Structural graph"), Graph) ||
        !TestNotNull(TEXT("Raid adapter"), Adapter))
        return false;
    TestTrue(TEXT("Load recipes and catalogues"), Data->LoadDefault());

    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Repair inventory owner"), Owner))
        return false;
    UInventoryCraftingComponent* Inventory = NewObject<UInventoryCraftingComponent>(Owner);
    Owner->AddInstanceComponent(Inventory);
    Inventory->RegisterComponent();
    TestTrue(TEXT("Apply item weights"), Data->ApplyToInventory(Inventory));

    auto AddStack = [&](FName Id, int32 Count, bool bTool)
    {
        FInventoryStack Stack;
        Stack.ItemId = Id;
        Stack.Quantity = Count;
        Stack.MaxStack = 99;
        Stack.Category = bTool ? EInventoryItemCategory::Tool : EInventoryItemCategory::Resource;
        Stack.bIsTool = bTool;
        return Inventory->TryAddItem(Stack);
    };
    TestTrue(TEXT("Add wood repair log"), AddStack(TEXT("Log"), 1, false));
    TestTrue(TEXT("Add rope"), AddStack(TEXT("Rope"), 1, false));
    TestTrue(TEXT("Add axe tool"), AddStack(TEXT("Tool.Axe"), 1, true));

    AActor* Wall = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Wood wall"), Wall))
        return false;
    Wall->Tags.Add(TEXT("Region:Zone.0"));
    const FGuid NodeId(701, 702, 703, 704);
    TestTrue(TEXT("Register wood node"),
        Graph->RegisterNode(Wall, NodeId, EStructuralNodeKind::Wood, true, 1.0f, 100.0f, 100.0f));
    bool bThresholdExceeded = false;
    TestTrue(TEXT("Damage wall"), Graph->ApplyDamage(NodeId, 40.0f, bThresholdExceeded));
    FStructuralNodeRecord Before;
    TestTrue(TEXT("Read damaged wall"), Graph->GetNode(NodeId, Before));
    const FGuid RepairOperation(705, 706, 707, 708);

    TestTrue(TEXT("Apply typed wood repair recipe"),
        Adapter->TryRepairStructureWithRecipe(RepairOperation, NodeId, Before.Revision,
            TEXT("Repair.Wood"), Inventory->GetInventoryOwnerId()));
    FStructuralNodeRecord Repaired;
    TestTrue(TEXT("Read repaired wall"), Graph->GetNode(NodeId, Repaired));
    TestEqual(TEXT("Wood recipe repairs thirty damage"), Repaired.Damage, 10.0f);
    TestEqual(TEXT("Log is consumed"), Inventory->GetItemCount(TEXT("Log")), 0);
    TestEqual(TEXT("Rope is consumed"), Inventory->GetItemCount(TEXT("Rope")), 0);
    TestEqual(TEXT("Axe tool remains"), Inventory->GetItemCount(TEXT("Tool.Axe")), 1);

    TestTrue(TEXT("Retry is a successful no-op"),
        Adapter->TryRepairStructureWithRecipe(RepairOperation, NodeId, Before.Revision,
            TEXT("Repair.Wood"), Inventory->GetInventoryOwnerId()));
    TestTrue(TEXT("Read wall after retry"), Graph->GetNode(NodeId, Repaired));
    TestEqual(TEXT("Retry does not repair again"), Repaired.Damage, 10.0f);
    return true;
}

// Faz 8 — defense type definitions feed structure rating and signal suppression.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaidCountermeasureDefinitionTest,
    "MyProject3.World.Events.CountermeasureDefinitionsReduceRaidPressure", Flags)
bool FRaidCountermeasureDefinitionTest::RunTest(const FString& Parameters)
{
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldGameDataSubsystem* Data = World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph = World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock = World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldRegionalEventSubsystem* Events = World ? World->GetSubsystem<UWorldRegionalEventSubsystem>() : nullptr;
    UWorldThreatDirectorSubsystem* Threat = World ? World->GetSubsystem<UWorldThreatDirectorSubsystem>() : nullptr;
    UWorldRaidAdapterSubsystem* Adapter = World ? World->GetSubsystem<UWorldRaidAdapterSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Data"), Data) || !TestNotNull(TEXT("Graph"), Graph) ||
        !TestNotNull(TEXT("Clock"), Clock) || !TestNotNull(TEXT("Events"), Events) ||
        !TestNotNull(TEXT("Threat"), Threat) || !TestNotNull(TEXT("Adapter"), Adapter))
        return false;
    TestTrue(TEXT("Load data"), Data->LoadDefault());
    TestTrue(TEXT("Load event profiles"), Events->LoadDefaultDefinitions());
    TestTrue(TEXT("Initialize clock"), Clock->InitializeClock(913));
    TestTrue(TEXT("Initialize threat director"), Threat->InitializeDirector(3600.0));

    AActor* Defense = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Fence defense actor"), Defense))
        return false;
    Defense->Tags.Add(TEXT("Region:Zone.1"));
    Defense->Tags.Add(TEXT("RaidDefenseType:RaidDefense.Fence"));
    const FGuid NodeId(811, 812, 813, 814);
    TestTrue(TEXT("Register typed fence defense"),
        Graph->RegisterNode(Defense, NodeId, EStructuralNodeKind::Wood, true, 1.0f, 100.0f, 100.0f));
    TestEqual(TEXT("Fence contributes configured defense rating"),
        Graph->GetRegionDefenseRating(TEXT("Zone.1")), 1.5f);

    AActor* Store = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Store signal actor"), Store))
        return false;
    Store->Tags.Add(TEXT("Region:Zone.2"));
    Store->Tags.Add(TEXT("RaidSignal.Storage:1.0"));
    Store->Tags.Add(TEXT("RaidSignalSuppress.Storage:1.0"));
    TestTrue(TEXT("Suppressed signal scan succeeds"), Adapter->AdvanceRegionalRaidSignals());
    FThreatRaidRecord Raid;
    TestFalse(TEXT("Fully suppressed storage alone does not create a raid"),
        Threat->GetCurrentRaid(Raid));
    return true;
}

#endif
