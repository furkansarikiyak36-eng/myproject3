#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "Placement/LogFeatureProvider.h"
#include "Placement/PlacementFeatureProvider.h"
#include "Placement/PlacementKernelSubsystem.h"
#include "Placement/PlacementProfiles.h"
#include "Placement/PlacementSolvers.h"
#include "Placement/PlacementTransaction.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace PlacementKernelTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

    struct FPlacementTestWorld
    {
        UWorld* World = nullptr;

        FPlacementTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlacementKernelTestWorld"));
            if (!World)
                return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            if (!World->HasBegunPlay() && World->GetWorldSettings())
                World->GetWorldSettings()->NotifyBeginPlay();
        }

        ~FPlacementTestWorld()
        {
            if (!World)
                return;
            if (World->HasBegunPlay())
                World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    };

    /** A log made of the engine cube, its length set by actor scale (like a freely cut log). */
    ALogBuildPiece* SpawnCubeLog(UWorld* World, const FVector& Centre, const FRotator& Rotation,
        float Length, float Thickness, bool bPlaced)
    {
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!World || !Cube)
            return nullptr;
        const FTransform Spawn(Rotation, Centre, FVector(Length / 100.0f, Thickness / 100.0f, Thickness / 100.0f));
        ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Spawn,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Log)
            return nullptr;
        Log->BaseDefinitionId = TEXT("Test.Log");
        Log->LongitudinalAxis = ELogLongitudinalAxis::X;
        Log->SetPieceStaticMesh(Cube);
        Log->FinishSpawning(Spawn);
        Log->GetLogMesh()->SetSimulatePhysics(false);
        Log->SetActorTransform(Spawn);
        if (bPlaced)
        {
            if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
                Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Placed);
            Log->SetPlacedState(true);
        }
        return Log;
    }

    int32 CountKind(const TArray<FPlacementFeature>& Features, EPlacementFeatureKind Kind)
    {
        int32 Count = 0;
        for (const FPlacementFeature& Feature : Features)
            Count += Feature.Kind == Kind ? 1 : 0;
        return Count;
    }
}

using namespace PlacementKernelTests;

// The catalogue loads, and a profile breaking a rule is rejected instead of half used.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementProfilesTest, "MyProject3.Placement.ProfilesLoadAndRejectBadRules", Flags)
bool FPlacementProfilesTest::RunTest(const FString& Parameters)
{
    FPlacementProfileRegistry Project;
    Project.LoadDefault();
    TestTrue(TEXT("Project catalogue has the end joint"), Project.Find(TEXT("log.end_to_end")) != nullptr);
    TestEqual(TEXT("Project catalogue has no rejected profiles"), Project.GetErrors().Num(), 0);
    for (const FString& Error : Project.GetErrors())
        AddError(Error);

    // Packaged builds read the compiled copy: it must say exactly what the project file says.
    FPlacementProfileRegistry BuiltIn;
    TestTrue(TEXT("Built-in catalogue parses"), BuiltIn.LoadFromString(FPlacementProfileRegistry::GetBuiltInCatalogue()));
    TestEqual(TEXT("Built-in catalogue has no rejected profiles"), BuiltIn.GetErrors().Num(), 0);
    TestEqual(TEXT("Built-in catalogue matches the project file (run SyncPlacementCatalogue.py)"),
        BuiltIn.GetProfiles().Num(), Project.GetProfiles().Num());
    for (const FPlacementProfile& Profile : Project.GetProfiles())
    {
        const FPlacementProfile* Copy = BuiltIn.Find(Profile.Id);
        TestTrue(FString::Printf(TEXT("%s is in the built-in copy"), *Profile.Id.ToString()), Copy != nullptr);
        if (!Copy)
            continue;
        TestTrue(FString::Printf(TEXT("%s has the same mode and links"), *Profile.Id.ToString()),
            Copy->Mode == Profile.Mode && Copy->RequiredLinks == Profile.RequiredLinks && Copy->bEnabled == Profile.bEnabled);
        for (const TPair<FName, float>& Param : Profile.Params)
            TestEqual(FString::Printf(TEXT("%s param %s"), *Profile.Id.ToString(), *Param.Key.ToString()),
                Copy->GetParam(Param.Key, -1.0f), Param.Value);
    }
    TestTrue(TEXT("Every mechanic has its profile"),
        Project.Find(TEXT("log.parallel_stack")) && Project.Find(TEXT("log.deck")) && Project.Find(TEXT("log.stairs")) &&
        Project.Find(TEXT("log.step")) && Project.Find(TEXT("log.gable")) && Project.Find(TEXT("log.strut")) &&
        Project.Find(TEXT("log.post_on_ground")) && Project.Find(TEXT("stone.on_anchor")) && Project.Find(TEXT("stick.on_anchor")));

    FPlacementProfileRegistry Bad;
    Bad.LoadFromString(TEXT(R"({ "schemaVersion": 1, "profiles": [
        { "id": "a", "solver": "SpanTwo", "requiredLinks": 1 },
        { "id": "b", "solver": "AlignOne" },
        { "id": "b", "solver": "AlignOne" },
        { "id": "c", "solver": "AlignOne", "angle": { "policy": "Grid" } },
        { "id": "d", "solver": "Teleport" } ] })"));
    TestEqual(TEXT("Only the good profile is kept"), Bad.GetProfiles().Num(), 1);
    TestEqual(TEXT("Four profiles are rejected with a reason"), Bad.GetErrors().Num(), 4);

    FPlacementProfileRegistry Wrong;
    TestFalse(TEXT("A newer schema is refused"), Wrong.LoadFromString(TEXT(R"({ "schemaVersion": 7, "profiles": [] })")));
    return true;
}

// The player roughly holds an angle; the solver rounds it to the profile's grid
// and keeps its choice until the player clearly moves past the halfway point.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementAngleTest, "MyProject3.Placement.AngleGridWithoutFlicker", Flags)
bool FPlacementAngleTest::RunTest(const FString& Parameters)
{
    FPlacementProfile Grid;
    Grid.AnglePolicy = EPlacementAnglePolicy::Grid;
    Grid.AnglesDeg = { 0, 15, 30, 45, 60, 75, 90 };
    Grid.AngleHysteresisDeg = 4.0f;
    TestEqual(TEXT("38 degrees becomes 45"), PlacementMath::ChooseAngle(Grid, 38.0f), 45.0f);
    TestEqual(TEXT("37 degrees becomes 30"), PlacementMath::ChooseAngle(Grid, 37.0f), 30.0f);
    const float Previous = 30.0f;
    TestEqual(TEXT("Just past halfway the angle stays"), PlacementMath::ChooseAngle(Grid, 39.0f, &Previous), 30.0f);
    TestEqual(TEXT("Clearly past it, the angle changes"), PlacementMath::ChooseAngle(Grid, 42.0f, &Previous), 45.0f);
    TestEqual(TEXT("Angles wrap around"), PlacementMath::ChooseAngle(Grid, -10.0f), 0.0f);

    FPlacementProfile Fixed;
    TestEqual(TEXT("Fixed ignores the held angle"), PlacementMath::ChooseAngle(Fixed, 70.0f), 0.0f);

    FPlacementProfile Range;
    Range.AnglePolicy = EPlacementAnglePolicy::Range;
    Range.AngleMinDeg = 30.0f;
    Range.AngleMaxDeg = 60.0f;
    Range.AngleStepDeg = 10.0f;
    TestEqual(TEXT("Range rounds to its step"), PlacementMath::ChooseAngle(Range, 44.0f), 40.0f);
    TestEqual(TEXT("Range clamps to its end"), PlacementMath::ChooseAngle(Range, 85.0f), 60.0f);
    return true;
}

// Body stations every 75 cm from the log's real world length: 8/6/4/2/1 plus two ends.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementStationsTest, "MyProject3.Placement.LogStationsFromRealLength", Flags)
bool FPlacementStationsTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    const TPair<float, int32> Cases[] = { { 600.0f, 8 }, { 450.0f, 6 }, { 300.0f, 4 }, { 150.0f, 2 }, { 75.0f, 1 }, { 400.0f, 5 } };
    for (const TPair<float, int32>& Case : Cases)
    {
        ALogBuildPiece* Log = SpawnCubeLog(TestWorld.World, FVector(0, 0, 100), FRotator::ZeroRotator, Case.Key, 30.0f, false);
        if (!TestNotNull(TEXT("Log"), Log))
            return false;
        TArray<FPlacementFeature> Features;
        FLogFeatureProvider::BuildModularFeatures(Log, 75.0f, Features);
        TestEqual(FString::Printf(TEXT("%.0f cm: stations"), Case.Key), CountKind(Features, EPlacementFeatureKind::Station), Case.Value);
        TestEqual(FString::Printf(TEXT("%.0f cm: ends"), Case.Key), CountKind(Features, EPlacementFeatureKind::End), 2);
        // First station 37.5 cm from the -X end, the next one 75 cm on, in the world.
        const FVector EndA = Features[0].WorldFrame.GetLocation();
        TestTrue(FString::Printf(TEXT("%.0f cm: first station at 37.5"), Case.Key),
            FMath::IsNearlyEqual(FVector::Dist(EndA, Features[2].WorldFrame.GetLocation()), 37.5f, 0.5f));
        if (Case.Value > 1)
            TestTrue(FString::Printf(TEXT("%.0f cm: 75 cm between stations"), Case.Key),
                FMath::IsNearlyEqual(FVector::Dist(Features[2].WorldFrame.GetLocation(), Features[3].WorldFrame.GetLocation()), 75.0f, 0.5f));
        Log->Destroy();
    }
    return true;
}

// The kernel finds the joint the old snap code found, keeps the held log's own
// length, and commits it all or nothing; a stale preview is refused.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementSolveCommitTest, "MyProject3.Placement.SolveCommitAndRollBackJoint", Flags)
bool FPlacementSolveCommitTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    ALogBuildPiece* Target = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, 400, 100), FRotator::ZeroRotator, 150.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Held"), Held))
        return false;
    TestTrue(TEXT("Logs have a feature provider"), FPlacementProviderRegistry::Find(Target) != nullptr);

    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = Target;
    Intent.AimPoint = FVector(150, 0, 100); // The +X end of the target.
    const FPlacementSolution Solution = Kernel->Solve(Intent);
    if (!TestTrue(FString::Printf(TEXT("A joint is found (%s)"), *Solution.Reason), Solution.bValid))
        return false;
    TestEqual(TEXT("One link"), Solution.Links.Num(), 1);
    TestTrue(TEXT("The held log keeps its own length"),
        Solution.PieceTransform.GetScale3D().Equals(Held->GetActorScale3D(), 0.001f));

    FPlacementFeature TargetFeature;
    FPlacementProviderRegistry::FindFeature(Target, Solution.Links[0].Target.FeatureId, TargetFeature);
    const FVector Joint = TargetFeature.WorldFrame.GetLocation();
    FPlacementFeature SourceFeature;
    FPlacementProviderRegistry::FindFeature(Held, Solution.Links[0].Source.FeatureId, SourceFeature);
    const FVector SourceAtJoint = (SourceFeature.LocalFrame * Solution.PieceTransform).GetLocation();
    TestTrue(FString::Printf(TEXT("The held joint lands on the target joint (%.1f cm off)"), FVector::Dist(SourceAtJoint, Joint)),
        FVector::Dist(SourceAtJoint, Joint) < 1.0f);

    // Roll back: nothing stays joined.
    {
        FPlacementTransaction Tx;
        FString Reason;
        Held->SetActorTransform(Solution.PieceTransform);
        TestTrue(FString::Printf(TEXT("Link commits (%s)"), *Reason), Kernel->CommitLinks(Solution, Tx, Reason));
        FPlacementProviderRegistry::FindFeature(Target, Solution.Links[0].Target.FeatureId, TargetFeature);
        TestTrue(TEXT("The target point is taken"), TargetFeature.bOccupied);
        Tx.Rollback();
    }
    FPlacementProviderRegistry::FindFeature(Target, Solution.Links[0].Target.FeatureId, TargetFeature);
    TestFalse(TEXT("After rollback the target point is free"), TargetFeature.bOccupied);
    TestTrue(TEXT("After rollback the held log is not attached"), Held->GetAttachParentActor() == nullptr);

    // Stale: the target changes shape after the preview.
    FString Reason;
    TestTrue(TEXT("The solution is current"), Kernel->IsStillCurrent(Solution, Reason));
    Target->SetActorScale3D(FVector(2.0f, 0.3f, 0.3f));
    TestFalse(TEXT("A changed target makes the preview stale"), Kernel->IsStillCurrent(Solution, Reason));
    FPlacementTransaction Tx;
    TestFalse(TEXT("A stale preview does not commit"), Kernel->CommitLinks(Solution, Tx, Reason));
    TestTrue(TEXT("And leaves nothing to undo"), Tx.Failed() && Tx.GetUndoCount() == 0);
    return true;
}

// Step 0 of the plan: a board needing two supports is not left on one when the
// second fails; it stays in the hands and the graph is unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementTwoSupportTest, "MyProject3.Placement.TwoSupportsOrNothing", Flags)
bool FPlacementTwoSupportTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    ALogBuildPiece* BeamA = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 400.0f, 30.0f, true);
    ALogBuildPiece* Board = SpawnCubeLog(World, FVector(0, -300, 30), FRotator::ZeroRotator, 230.0f, 20.0f, false);
    AActor* Player = World->SpawnActor<AActor>(FVector(0, -400, 100), FRotator::ZeroRotator);
    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Player);
    Carry->RegisterComponent();
    if (!TestNotNull(TEXT("Graph"), Graph) || !TestNotNull(TEXT("Board"), Board) || !TestTrue(TEXT("Pick up"), Carry->TryPickupPiece(Board)))
        return false;

    const FTransform Before = Board->GetActorTransform();
    const FTransform Wanted(FRotator(0, 90, 0), FVector(0, 75, 120), Board->GetActorScale3D());
    // The same beam cannot be both supports: the second link fails.
    TestFalse(TEXT("Board with a failing second support is not placed"), Carry->CommitHeldLogAt(Wanted, BeamA, BeamA));
    TestTrue(TEXT("Board is still in the hands"), Carry->HeldPiece == Board && Board->IsHeld() && !Board->IsPlaced());
    FStructuralNodeRecord Node;
    Graph->GetNode(Board->GetStructuralNodeId(), Node);
    TestFalse(TEXT("No half joint is left in the graph"), Node.ParentNodeId.IsValid());
    TestEqual(TEXT("No second support either"), Node.SecondarySupports.Num(), 0);
    TestTrue(TEXT("The board went back to where it was"), Board->GetActorLocation().Equals(Before.GetLocation(), 0.5f));
    if (UWorldObjectIdentityComponent* Identity = Board->FindComponentByClass<UWorldObjectIdentityComponent>())
        TestEqual(TEXT("Lifecycle is back to held"), Identity->GetLifecycleState(), EWorldObjectLifecycleState::Held);

    // With one support only, it rests on that beam.
    TestTrue(TEXT("Board on one support is placed"), Carry->CommitHeldLogAt(Wanted, BeamA, nullptr));
    Graph->GetNode(Board->GetStructuralNodeId(), Node);
    TestEqual(TEXT("It rests on the beam"), Node.ParentNodeId, BeamA->GetStructuralNodeId());
    return true;
}

// Step 8: a log's own joint points are one station per 75 cm module of its real
// length (no copy per side); a cut lays them out again; a T joint takes the side
// of the station the player aims at.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementStationsOnLogTest, "MyProject3.Placement.StationsEvery75cmAndSideFromAim", Flags)
bool FPlacementStationsOnLogTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    ALogBuildPiece* Beam = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 600.0f, 30.0f, true);
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, 600, 100), FRotator::ZeroRotator, 150.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Beam"), Beam) || !TestNotNull(TEXT("Held"), Held))
        return false;

    auto CountStations = [](const ALogBuildPiece* Log)
    {
        TArray<FPlacementFeature> Features;
        FPlacementProviderRegistry::Find(Log)->GetFeatures(Log, Features);
        int32 Count = 0;
        for (const FPlacementFeature& Feature : Features)
            Count += Feature.Kind == EPlacementFeatureKind::Station ? 1 : 0;
        return Count;
    };
    TestEqual(TEXT("A 600 cm log has 8 stations"), CountStations(Beam), 8);
    TestEqual(TEXT("A 150 cm log has 2 stations"), CountStations(Held), 2);
    int32 SidePoints = 0;
    for (const FLogSnapPoint& Point : Beam->GetSnapPoints())
        SidePoints += Point.PointId.ToString().StartsWith(TEXT("AutoSide")) ? 1 : 0;
    TestEqual(TEXT("No point is copied per side any more"), SidePoints, 0);

    // A cut (scale) lays the stations out again for the new length.
    Held->SetActorScale3D(FVector(3.0f, 0.3f, 0.3f));
    TestEqual(TEXT("Cut to 300 cm it has 4 stations"), CountStations(Held), 4);

    // T joint: aim at the beam's side, then at its top, at the same station.
    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = Beam;
    Intent.AllowedProfiles = { TEXT("log.end_to_station") };
    Intent.AimPoint = FVector(37.5f, -15.0f, 100.0f); // The -Y side.
    FPlacementSolution Side = Kernel->Solve(Intent);
    Intent.AimPoint = FVector(37.5f, 0.0f, 115.0f); // The top.
    FPlacementSolution Top = Kernel->Solve(Intent);
    if (!TestTrue(TEXT("A side joint is found"), Side.bValid) || !TestTrue(TEXT("A top joint is found"), Top.bValid))
        return false;
    TestTrue(FString::Printf(TEXT("The side joint is on the -Y face (%s)"), *Side.MarkerPoint.ToString()),
        FMath::IsNearlyEqual(Side.MarkerPoint.Y, -15.0f, 1.0f) && FMath::IsNearlyEqual(Side.MarkerPoint.Z, 100.0f, 1.0f));
    TestTrue(FString::Printf(TEXT("The top joint is on the top face (%s)"), *Top.MarkerPoint.ToString()),
        FMath::IsNearlyEqual(Top.MarkerPoint.Z, 115.0f, 1.0f));
    const FVector SideAxis = Side.PieceTransform.GetUnitAxis(EAxis::X);
    TestTrue(TEXT("The side joint holds the log out sideways"), FMath::Abs(SideAxis.Y) > 0.99f);
    TestEqual(TEXT("Both use the same station"), Side.Links[0].Target.FeatureId, Top.Links[0].Target.FeatureId);
    return true;
}

// Step 10: every link the kernel makes lives in the structural graph, which the
// save coordinator stores; a board on two beams comes back on both after a load.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementSaveTest, "MyProject3.Placement.LinksSurviveSaveAndLoad", Flags)
bool FPlacementSaveTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    ALogBuildPiece* BeamA = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 400.0f, 30.0f, true);
    ALogBuildPiece* BeamB = SpawnCubeLog(World, FVector(0, 150, 100), FRotator::ZeroRotator, 400.0f, 30.0f, true);
    ALogBuildPiece* Board = SpawnCubeLog(World, FVector(0, -300, 30), FRotator::ZeroRotator, 230.0f, 20.0f, false);
    AActor* Player = World->SpawnActor<AActor>(FVector(0, -400, 100), FRotator::ZeroRotator);
    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Player);
    Carry->RegisterComponent();
    if (!TestNotNull(TEXT("Graph"), Graph) || !TestTrue(TEXT("Pick up"), Carry->TryPickupPiece(Board)))
        return false;
    TestTrue(FString::Printf(TEXT("Board on two beams (%s)"), *Carry->GetPlacementStatus()),
        Carry->CommitHeldLogAt(FTransform(FRotator(0, 90, 0), FVector(0, 75, 125), Board->GetActorScale3D()), BeamA, BeamB));

    TArray<FWorldStructuralNodeSnapshot> Saved;
    TestTrue(TEXT("The graph is saved"), Graph->ExportSnapshot(Saved));
    // Lose the links (as a fresh load would start without them) ...
    Graph->TryRemoveSecondarySupport(Board->GetStructuralNodeId(), BeamB->GetStructuralNodeId());
    Graph->TryDisconnect(Board->GetStructuralNodeId());
    FStructuralNodeRecord Node;
    Graph->GetNode(Board->GetStructuralNodeId(), Node);
    TestFalse(TEXT("Links are gone before the load"), Node.ParentNodeId.IsValid());
    // ... and load them back.
    TestTrue(TEXT("The graph loads"), Graph->RestoreSnapshot(Saved));
    Graph->GetNode(Board->GetStructuralNodeId(), Node);
    TestEqual(TEXT("It rests on the first beam again"), Node.ParentNodeId, BeamA->GetStructuralNodeId());
    TestTrue(TEXT("And on the second beam"), Node.SecondarySupports.Num() == 1 &&
        Node.SecondarySupports[0].SupportNodeId == BeamB->GetStructuralNodeId());
    return true;
}

// Modularity: the kernel and its math never name a material. New materials come
// in as providers and profiles; if this test fails, a material rule leaked in.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementKernelNeutralTest, "MyProject3.Placement.KernelNamesNoMaterial", Flags)
bool FPlacementKernelNeutralTest::RunTest(const FString& Parameters)
{
    const TCHAR* Files[] = {
        TEXT("PlacementKernelSubsystem.h"), TEXT("PlacementKernelSubsystem.cpp"), TEXT("PlacementTypes.h"),
        TEXT("PlacementBodyMath.h"), TEXT("PlacementBodyMath.cpp"), TEXT("PlacementSolvers.h"), TEXT("PlacementSolvers.cpp"),
        TEXT("PlacementProfiles.h"), TEXT("PlacementProfiles.cpp"), TEXT("PlacementTransaction.h") };
    const TCHAR* Forbidden[] = { TEXT("ALogBuildPiece"), TEXT("AStoneBuildingPiece"), TEXT("AStickPrimitivePiece"),
        TEXT("ARopeLineActor"), TEXT("LogBuildPiece.h"), TEXT("StoneBuildingPiece.h"), TEXT("StickPrimitivePiece.h") };
    const FString Folder = FPaths::Combine(FPaths::GameSourceDir(), TEXT("MyProject3"), TEXT("Placement"));
    for (const TCHAR* File : Files)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Folder, File)))
        {
            AddWarning(FString::Printf(TEXT("%s not readable here; skipped"), File));
            continue;
        }
        for (const TCHAR* Word : Forbidden)
            TestFalse(FString::Printf(TEXT("%s does not name %s"), File, Word), Text.Contains(Word));
    }
    // Logs, ropes, stones and sticks each describe themselves.
    TestTrue(TEXT("At least four materials have providers"), FPlacementProviderRegistry::Num() >= 4);
    return true;
}

// Steps 6 and 7: the player and the helper (or a plan) go through the same
// commit, and every placement is announced so the world can react to it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementOneCommitTest, "MyProject3.Placement.EveryPlacerUsesOneCommit", Flags)
bool FPlacementOneCommitTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    ALogBuildPiece* Beam = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 400.0f, 30.0f, true);
    ALogBuildPiece* ByPlayer = SpawnCubeLog(World, FVector(0, -300, 30), FRotator::ZeroRotator, 200.0f, 20.0f, false);
    ALogBuildPiece* ByHelper = SpawnCubeLog(World, FVector(0, 300, 30), FRotator::ZeroRotator, 200.0f, 20.0f, false);
    AActor* Player = World->SpawnActor<AActor>(FVector(0, -400, 100), FRotator::ZeroRotator);
    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Player);
    Carry->RegisterComponent();
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Beam"), Beam) || !TestTrue(TEXT("Pick up"), Carry->TryPickupPiece(ByPlayer)))
        return false;

    TArray<AActor*> Announced;
    const FDelegateHandle Handle = Kernel->OnPlacementCommitted.AddLambda(
        [&Announced](AActor* Piece, const FPlacementSolution&) { Announced.Add(Piece); });
    TestTrue(TEXT("The player places on the beam"),
        Carry->CommitHeldLogAt(FTransform(FRotator(0, 90, 0), FVector(-100, 0, 125), ByPlayer->GetActorScale3D()), Beam, nullptr));

    // The helper's piece lies in the world (not in anyone's hands) when it is built in.
    FPlacementCommitOptions Options;
    Options.FromOwner = EWorldItemOwnerKind::World;
    Options.FromLifecycle = EWorldObjectLifecycleState::World;
    FString Reason;
    const FPlacementSolution Slot = Kernel->MakeDirectSolution(ByHelper,
        FTransform(FRotator(0, 90, 0), FVector(100, 0, 125), ByHelper->GetActorScale3D()), TEXT("plan.slot"), Beam);
    TestTrue(FString::Printf(TEXT("The helper places on the beam (%s)"), *Reason), Kernel->CommitPlacement(ByHelper, Slot, Options, Reason));
    Kernel->OnPlacementCommitted.Remove(Handle);

    TestEqual(TEXT("Both placements were announced"), Announced.Num(), 2);
    TestTrue(TEXT("Both rest on the beam"), ByPlayer->IsPlaced() && ByHelper->IsPlaced() &&
        ByPlayer->GetAttachParentActor() == Beam && ByHelper->GetAttachParentActor() == Beam);
    return true;
}

#endif
