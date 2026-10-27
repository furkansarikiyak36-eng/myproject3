#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "Placement/LogFeatureProvider.h"
#include "Placement/PlacementBodyMath.h"
#include "Placement/PlacementFeatureProvider.h"
#include "Placement/PlacementKernelSubsystem.h"
#include "Placement/PlacementProfiles.h"
#include "Placement/PlacementSolvers.h"
#include "Placement/PlacementTransaction.h"
#include "StoneBuildingPiece.h"
#include "VerticalSliceCWorldActor.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "EngineUtils.h"

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

// The station map of a log: End.A, End.B and a station per 75 cm module of its
// real (scaled) length: 600/450/300/150/75 cm give 8/6/4/2/1, 400 cm gives 5.
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
        // The game's own features of the log: End.A, End.B and Station.00 ...
        TArray<FPlacementFeature> Features;
        FPlacementProviderRegistry::Find(Log)->GetFeatures(Log, Features);
        TestEqual(FString::Printf(TEXT("%.0f cm: stations"), Case.Key), CountKind(Features, EPlacementFeatureKind::Station), Case.Value);
        TestEqual(FString::Printf(TEXT("%.0f cm: ends"), Case.Key), CountKind(Features, EPlacementFeatureKind::End), 2);
        FPlacementFeature EndA, EndB, First, Second;
        TestTrue(FString::Printf(TEXT("%.0f cm: End.A and End.B"), Case.Key),
            FPlacementProviderRegistry::FindFeature(Log, TEXT("End.A"), EndA) && FPlacementProviderRegistry::FindFeature(Log, TEXT("End.B"), EndB));
        TestTrue(FString::Printf(TEXT("%.0f cm: End.A is the -X end"), Case.Key),
            EndA.WorldFrame.GetLocation().X < EndB.WorldFrame.GetLocation().X);
        // Station.00 is 37.5 cm from End.A, the next one 75 cm on, in the world.
        TestTrue(FString::Printf(TEXT("%.0f cm: Station.00 exists"), Case.Key), FPlacementProviderRegistry::FindFeature(Log, TEXT("Station.00"), First));
        TestTrue(FString::Printf(TEXT("%.0f cm: first station at 37.5 (%.1f)"), Case.Key,
            FVector::Dist(EndA.WorldFrame.GetLocation(), First.WorldFrame.GetLocation())),
            FMath::IsNearlyEqual(FVector::Dist(EndA.WorldFrame.GetLocation(), First.WorldFrame.GetLocation()), 37.5f, 0.5f));
        if (Case.Value > 1 && FPlacementProviderRegistry::FindFeature(Log, TEXT("Station.01"), Second))
            TestTrue(FString::Printf(TEXT("%.0f cm: 75 cm between stations"), Case.Key),
                FMath::IsNearlyEqual(FVector::Dist(First.WorldFrame.GetLocation(), Second.WorldFrame.GetLocation()), 75.0f, 0.5f));
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

    // A change of length makes old solutions stale at once; the cut lays the
    // stations out again as its own step (a query never changes the log).
    const uint32 Before = FPlacementProviderRegistry::Find(Held)->GetGeometryRevision(Held);
    Held->SetActorScale3D(FVector(3.0f, 0.3f, 0.3f));
    TestNotEqual(TEXT("The new length is a new geometry revision"), FPlacementProviderRegistry::Find(Held)->GetGeometryRevision(Held), Before);
    TestEqual(TEXT("A query alone does not lay stations out"), CountStations(Held), 2);
    Held->RefreshStationsIfStale();
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

// The scene too: every built log in VerticalSlice_C went in through the kernel,
// and the cabin is stacked like the player stacks: corners cross at stations of
// both logs and every row above the first rests on both walls under it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementSceneTest, "MyProject3.Placement.SliceCBuiltThroughKernel", Flags)
bool FPlacementSceneTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Graph"), Graph))
        return false;
    TSet<AActor*> Announced;
    const FDelegateHandle Handle = Kernel->OnPlacementCommitted.AddLambda(
        [&Announced](AActor* Piece, const FPlacementSolution&) { Announced.Add(Piece); });
    AVerticalSliceCWorldActor* Slice = World->SpawnActor<AVerticalSliceCWorldActor>();
    if (Slice)
        Slice->BuildWorld();
    Kernel->OnPlacementCommitted.Remove(Handle);
    if (!TestNotNull(TEXT("Slice C"), Slice))
        return false;

    int32 Placed = 0;
    TArray<ALogBuildPiece*> Cabin;
    const FVector2D CabinCentre(-1350.0f, 380.0f);
    for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
    {
        if (!It->IsPlaced())
            continue;
        ++Placed;
        TestTrue(FString::Printf(TEXT("%s was built through the kernel"), *It->GetName()), Announced.Contains(*It));
        MyProject3Construction::FLogGeometry Shape;
        if (MyProject3Construction::GetLogGeometry(*It, Shape) && Shape.Length() > 400.0f && FMath::Abs(Shape.Axis.Z) < 0.1f &&
            FVector2D::Distance(FVector2D(Shape.Centre), CabinCentre) < 300.0f)
            Cabin.Add(*It);
    }
    TestTrue(FString::Printf(TEXT("Built pieces exist (%d)"), Placed), Placed > 55);
    TestEqual(TEXT("The cabin has 6 rows of 4 logs"), Cabin.Num(), 24);

    float Lowest = TNumericLimits<float>::Max();
    for (ALogBuildPiece* Log : Cabin)
        Lowest = FMath::Min(Lowest, Log->GetLogMesh()->Bounds.Origin.Z);
    int32 Crossings = 0;
    for (ALogBuildPiece* Log : Cabin)
    {
        MyProject3Construction::FLogGeometry Shape;
        MyProject3Construction::GetLogGeometry(Log, Shape);
        FStructuralNodeRecord Node;
        Graph->GetNode(Log->GetStructuralNodeId(), Node);
        if (Shape.Centre.Z > Lowest + 5.0f)
            TestTrue(FString::Printf(TEXT("%s rests on both walls under it"), *Log->GetName()),
                Node.ParentNodeId.IsValid() && Node.SecondarySupports.Num() >= 1);
        for (ALogBuildPiece* Other : Cabin)
        {
            MyProject3Construction::FLogGeometry Under;
            MyProject3Construction::GetLogGeometry(Other, Under);
            if (Other == Log || FMath::Abs(FVector::DotProduct(Shape.Axis, Under.Axis)) > 0.1f || Under.Centre.Z >= Shape.Centre.Z ||
                Shape.Centre.Z - Under.Centre.Z > Shape.Radius + Under.Radius)
                continue;
            FVector OnLog, OnUnder;
            FMath::SegmentDistToSegmentSafe(Shape.EndA(), Shape.EndB(), Under.EndA(), Under.EndB(), OnLog, OnUnder);
            auto OnStation = [](const FVector& Point, const FVector& Start)
            {
                const float Along = FVector::Dist2D(Point, Start);
                return FMath::Abs(FMath::Fmod(Along, 75.0f) - 37.5f) < 1.0f;
            };
            ++Crossings;
            TestTrue(FString::Printf(TEXT("%s crosses %s at stations of both"), *Log->GetName(), *Other->GetName()),
                OnStation(OnLog, Shape.EndA()) && OnStation(OnUnder, Under.EndA()));
        }
    }
    // 12 short logs cross 2 long ones each, and 10 long logs (rows 2-6) cross 2 short ones each.
    TestEqual(TEXT("Every log above the first row crosses the two under it"), Crossings, 12 * 2 + 10 * 2);
    return true;
}

// An undo that fails is reported, so the kernel can stop trusting the world
// instead of calling a half-undone placement a clean refusal.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementRollbackTest, "MyProject3.Placement.FailedUndoIsReported", Flags)
bool FPlacementRollbackTest::RunTest(const FString& Parameters)
{
    TArray<FString> Undone;
    {
        FPlacementTransaction Tx;
        Tx.Step(TEXT("A"), [] { return true; }, [&Undone] { Undone.Add(TEXT("A")); return true; });
        Tx.Step(TEXT("B"), [] { return true; }, [&Undone] { Undone.Add(TEXT("B")); return false; });
        TestFalse(TEXT("Step C fails"), Tx.Step(TEXT("C"), [] { return false; }));
        TestTrue(TEXT("The transaction failed at C"), Tx.Failed() && Tx.GetFailedStep() == TEXT("C"));
        TestTrue(TEXT("The failed undo is reported"), Tx.RollbackFailed() && Tx.GetFailedUndo() == TEXT("B"));
    }
    TestTrue(TEXT("Undone in reverse order, all of them"), Undone.Num() == 2 && Undone[0] == TEXT("B") && Undone[1] == TEXT("A"));

    FPlacementTransaction Clean;
    Clean.Step(TEXT("A"), [] { return true; }, [] { return true; });
    Clean.Fail(TEXT("Check"));
    TestFalse(TEXT("A clean rollback is not a failed one"), Clean.RollbackFailed());
    return true;
}

// The ledger is checked before anything changes: a piece the ledger does not
// hold where expected is not set on the ground at all.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementGroundLedgerTest, "MyProject3.Placement.GroundPlacementNeedsTheLedger", Flags)
bool FPlacementGroundLedgerTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    AStoneBuildingPiece* Stone = World->SpawnActor<AStoneBuildingPiece>(FVector(0, 0, 50), FRotator::ZeroRotator);
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Stone"), Stone) || !TestNotNull(TEXT("Cube"), Cube))
        return false;
    Stone->Mesh->SetStaticMesh(Cube);
    // Lifted by the piece itself but still world-owned in the ledger: placing it "from the hands" must not happen.
    TestTrue(TEXT("Lifted"), Stone->SetHeldState(true));
    FString Reason;
    TestFalse(TEXT("Not placed from the hands"), Kernel->CommitGroundPlacement(Stone,
        FTransform(FVector(0, 0, 50)), FPlacementCommitOptions(), Reason));
    TestFalse(FString::Printf(TEXT("Nothing changed (%s)"), *Reason), Stone->IsPlaced());
    // From where the ledger really has it, it is placed.
    FPlacementCommitOptions FromWorld;
    FromWorld.FromOwner = EWorldItemOwnerKind::World;
    FromWorld.FromLifecycle = EWorldObjectLifecycleState::World;
    TestTrue(FString::Printf(TEXT("Placed from the world (%s)"), *Reason),
        Kernel->CommitGroundPlacement(Stone, FTransform(FVector(0, 0, 50)), FromWorld, Reason));
    TestTrue(TEXT("Now on the ground"), Stone->IsPlaced());
    return true;
}

// A log joined at one end: what its other end rests on is decided in the same
// solution. Over nothing it may hang (the snap profiles are hinges); over a
// built log, that log becomes its second support.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementFreeEndTest, "MyProject3.Placement.FreeEndInTheSolution", Flags)
bool FPlacementFreeEndTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    ALogBuildPiece* Beam = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, 600, 100), FRotator::ZeroRotator, 150.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Held"), Held))
        return false;
    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = Beam;
    Intent.AimPoint = FVector(150, 0, 100);
    Intent.AllowedProfiles = { TEXT("log.end_to_end") };
    FPlacementSolution Hanging = Kernel->Solve(Intent);
    TestTrue(TEXT("Over nothing it is a hinge joint"), Hanging.bValid && Hanging.bFreeEndHangs);
    TestEqual(TEXT("With one link"), Hanging.Links.Num(), 1);

    // A built log under where the far end will be (x 300).
    SpawnCubeLog(World, FVector(300, 0, 70), FRotator(0, 90, 0), 200.0f, 30.0f, true);
    FPlacementSolution Resting = Kernel->Solve(Intent);
    TestTrue(TEXT("Over a log it does not hang"), Resting.bValid && !Resting.bFreeEndHangs);
    TestTrue(TEXT("That log is its second support"), Resting.FindLink(EPlacementLinkRole::Secondary) != nullptr);
    return true;
}

// The three basic log joints pick the right features, only wall logs take them,
// and a committed piece ends exactly where the solution put it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementBasicJointsTest, "MyProject3.Placement.BasicJointsSectionsAndExactCommit", Flags)
bool FPlacementBasicJointsTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    // A post standing at the origin, top at z 200; a beam to hold.
    ALogBuildPiece* Post = SpawnCubeLog(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, 600, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Post"), Post) || !TestNotNull(TEXT("Held"), Held))
        return false;

    // C: station to end: the held log lies on the post top with one of its stations.
    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = Post;
    Intent.AimPoint = FVector(0, 0, 200);
    Intent.AllowedProfiles = { TEXT("log.station_to_end") };
    const FPlacementSolution OnPost = Kernel->Solve(Intent);
    if (!TestTrue(FString::Printf(TEXT("Station to end is found (%s)"), *OnPost.Reason), OnPost.bValid))
        return false;
    TestTrue(TEXT("It joins a station of the held log"), OnPost.Links[0].Source.FeatureId.ToString().StartsWith(TEXT("Station.")));
    TestTrue(TEXT("To the post's top end"), OnPost.Links[0].Target.FeatureId == TEXT("End.B") || OnPost.Links[0].Target.FeatureId == TEXT("End.A"));
    const FVector Axis = OnPost.PieceTransform.GetUnitAxis(EAxis::X);
    TestTrue(TEXT("The held log lies flat"), FMath::Abs(Axis.Z) < 0.05f);
    TestTrue(FString::Printf(TEXT("On top of the post (z %.1f)"), OnPost.PieceTransform.GetLocation().Z),
        OnPost.PieceTransform.GetLocation().Z > 200.0f && OnPost.PieceTransform.GetLocation().Z < 215.0f + 1.0f);

    // Commit: the piece ends exactly where the solution put it, no second correction.
    FPlacementCommitOptions Options;
    Options.FromOwner = EWorldItemOwnerKind::World;
    Options.FromLifecycle = EWorldObjectLifecycleState::World;
    FString Reason;
    TestTrue(FString::Printf(TEXT("Committed (%s)"), *Reason), Kernel->CommitPlacement(Held, OnPost, Options, Reason));
    TestTrue(TEXT("Exactly the solved transform"), Held->GetActorTransform().Equals(OnPost.PieceTransform, 0.01f));

    // A plank is no wall log: it takes no end joint.
    ALogBuildPiece* Plank = SpawnCubeLog(World, FVector(0, -600, 300), FRotator::ZeroRotator, 150.0f, 10.0f, false);
    TArray<FPlacementFeature> Unused;
    FPlacementFeatureFilter Wall;
    Wall.Sections = { TEXT("Round"), TEXT("Hewn"), TEXT("HewnTwoFaces"), TEXT("SquareBeam") };
    TestTrue(TEXT("A round log is a wall log"), Wall.AllowsSection(FPlacementProviderRegistry::Find(Held)->GetSection(Held)));
    TestFalse(TEXT("A plank is not"), Wall.AllowsSection(TEXT("Plank")));
    TestFalse(TEXT("A half log is not"), Wall.AllowsSection(TEXT("HalfRound")));

    // Authored points: an end joins a hand-made point, and only End -> Authored or Authored -> any.
    Plank->AddCustomSnapPoint(TEXT("Notch"), TEXT("LogEnd"), FTransform(FVector(0, 0, 50)), false, true, true, true);
    const FPlacementProfile* Authored = Kernel->GetProfiles().Find(TEXT("log.authored"));
    const FPlacementProfile* EndToAuthored = Kernel->GetProfiles().Find(TEXT("log.end_to_authored"));
    TestTrue(TEXT("log.authored starts from an authored point only"), Authored && !Authored->Source.bAnyKind &&
        Authored->Source.Kind == EPlacementFeatureKind::Authored);
    TestTrue(TEXT("log.end_to_authored ends on an authored point"), EndToAuthored && !EndToAuthored->Target.bAnyKind &&
        EndToAuthored->Target.Kind == EPlacementFeatureKind::Authored);
    FPlacementFeature Notch;
    TestTrue(TEXT("The notch is an authored feature"),
        FPlacementProviderRegistry::FindFeature(Plank, TEXT("Notch"), Notch) && Notch.Kind == EPlacementFeatureKind::Authored);
    return true;
}

// The context candidate list is authoritative for search: even when AimActor is
// not the best target, the solver must compare all candidates and choose the closest
// valid feature to the aim point.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementContextCandidatesTest,
    "MyProject3.Placement.ContextCandidatesChooseBestFeature", Flags)
bool FPlacementContextCandidatesTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World ? World->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
    ALogBuildPiece* NearPost = SpawnCubeLog(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* DirectButFarPost = SpawnCubeLog(World, FVector(0, 100, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, 600, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Near post"), NearPost) ||
        !TestNotNull(TEXT("Direct target"), DirectButFarPost) || !TestNotNull(TEXT("Held log"), Held))
        return false;

    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = DirectButFarPost; // The direct hit is deliberately not the best feature.
    Intent.AimPoint = FVector(0, 0, 200);
    Intent.Candidates = { DirectButFarPost, NearPost };
    Intent.AllowedProfiles = { TEXT("log.station_to_end") };

    const FPlacementSolution Solution = Kernel->Solve(Intent);
    if (!TestTrue(FString::Printf(TEXT("A contextual solution is found (%s)"), *Solution.Reason), Solution.bValid))
        return false;

    const FPlacementLink* Link = Solution.GetPrimaryLink();
    TestNotNull(TEXT("The solution has a primary link"), Link);
    if (!Link)
        return false;
    TestTrue(TEXT("The nearer target is selected, not AimActor"), Link->Target.Piece.Get() == NearPost);
    TestTrue(TEXT("The source is a log station"), Link->Source.FeatureId.ToString().StartsWith(TEXT("Station.")));
    return true;
}

// Aim-resolved cutting is the default: the resolved ring follows the aimed
// position on a loose log instead of requiring the player to cycle to a cut mode.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementAimCutTest,
    "MyProject3.Placement.AimChoosesDefaultCutPosition", Flags)
bool FPlacementAimCutTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
    if (!TestNotNull(TEXT("World"), World) || !TestNotNull(TEXT("Owner"), Owner))
        return false;

    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Owner);
    if (!TestNotNull(TEXT("Carry component"), Carry))
        return false;
    Owner->AddInstanceComponent(Carry);
    Carry->RegisterComponent();

    TestEqual(TEXT("Length cutting is the default action"), Carry->GetSelectedCutAction(),
        ELogCutAction::SplitLengthHalf);

    ALogBuildPiece* Log = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Loose log"), Log))
        return false;

    ELogCutAction Action = ELogCutAction::SplitHalf;
    float Share = -1.0f;
    bool bFlip = false;
    const FVector QuarterPoint = Log->GetActorLocation() + FVector(-75.0f, 0.0f, 0.0f);
    TestTrue(TEXT("An aimed quarter position resolves"), Carry->ResolveCutAtPoint(Log, QuarterPoint, Action, Share, bFlip));
    TestTrue(FString::Printf(TEXT("The cut ring follows the quarter point (share %.3f)"), Share),
        FMath::IsNearlyEqual(Share, 0.25f, 0.02f));
    TestTrue(TEXT("The result differs from the default midpoint"), !FMath::IsNearlyEqual(Share, 0.5f, 0.02f));
    return true;
}

// Real notched joints come from the imported joinery meshes: the cross-lap
// pieces carry a NotchCradle socket, a log made with one has that joint point,
// and two of them cross notch in notch, the upper one turned over and across.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementNotchTest, "MyProject3.Placement.NotchedPiecesCrossInTheirNotches", Flags)
bool FPlacementNotchTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    UStaticMesh* CrossLap = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/DemoAssets/Logs/Joinery/SM_RawLog_75_CrossLap_Lower.SM_RawLog_75_CrossLap_Lower"));
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("The cross-lap mesh is imported"), CrossLap))
        return false;
    TestNotNull(TEXT("It has its notch socket"), CrossLap->FindSocket(TEXT("NotchCradle")));

    auto SpawnNotched = [World, CrossLap](const FVector& Centre, bool bPlaced)
    {
        const FTransform Spawn(Centre);
        ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Spawn,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Log->BaseDefinitionId = TEXT("Test.Notched");
        Log->LongitudinalAxis = ELogLongitudinalAxis::X;
        Log->SetPieceStaticMesh(CrossLap);
        Log->FinishSpawning(Spawn);
        Log->GetLogMesh()->SetSimulatePhysics(false);
        if (bPlaced)
        {
            if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
                Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Placed);
            Log->SetPlacedState(true);
        }
        return Log;
    };
    ALogBuildPiece* Lower = SpawnNotched(FVector(0, 0, 100), true);
    ALogBuildPiece* Upper = SpawnNotched(FVector(0, 400, 100), false);
    FPlacementFeature Notch;
    if (!TestTrue(TEXT("The log has the notch as a joint point"),
        FPlacementProviderRegistry::FindFeature(Lower, TEXT("Socket.NotchCradle"), Notch)))
        return false;
    TestTrue(TEXT("It is an authored point tagged NotchCradle"), Notch.Kind == EPlacementFeatureKind::Authored && Notch.Tag == TEXT("NotchCradle"));

    FPlacementIntent Intent;
    Intent.Held = Upper;
    Intent.AimActor = Lower;
    Intent.AimPoint = Notch.WorldFrame.GetLocation();
    Intent.AllowedProfiles = { TEXT("log.notch_pair") };
    const FPlacementSolution Pair = Kernel->Solve(Intent);
    if (!TestTrue(FString::Printf(TEXT("Notch in notch is found (%s)"), *Pair.Reason), Pair.bValid))
        return false;
    const FVector UpperAxis = Pair.PieceTransform.GetUnitAxis(EAxis::X);
    TestTrue(FString::Printf(TEXT("The upper piece crosses at 90 degrees (%s)"), *UpperAxis.ToString()), FMath::Abs(UpperAxis.X) < 0.05f && FMath::Abs(UpperAxis.Z) < 0.05f);
    FPlacementFeature UpperNotch;
    FPlacementProviderRegistry::FindFeature(Upper, TEXT("Socket.NotchCradle"), UpperNotch);
    const FTransform UpperNotchWorld = UpperNotch.LocalFrame * Pair.PieceTransform;
    TestTrue(TEXT("Its notch meets the lower notch"), UpperNotchWorld.GetLocation().Equals(Notch.WorldFrame.GetLocation(), 1.0f));
    TestTrue(TEXT("Its notch faces down"), UpperNotchWorld.GetUnitAxis(EAxis::X).Z < -0.95f);
    // Half and half: the two pieces share one height band, the upper sits no higher than the lower's top.
    const FVector LowerCentre = Lower->GetLogMesh()->Bounds.Origin;
    Upper->SetActorTransform(Pair.PieceTransform);
    const FVector UpperCentre = Upper->GetLogMesh()->Bounds.Origin;
    TestTrue(FString::Printf(TEXT("They interlock (lower centre %s, upper centre %s, notch local %s, lower actor %s)"),
        *LowerCentre.ToString(), *UpperCentre.ToString(), *Notch.LocalFrame.GetLocation().ToString(), *Lower->GetActorLocation().ToString()),
        FMath::Abs(UpperCentre.Z - LowerCentre.Z) < 1.0f);
    return true;
}

// The game decides from where the player looks, with no mode: on a post top a
// log lies across; on a wall top it is the next row; at a side a T, at an end it
// continues; on the ground it lies, looking steeply down it stands; beside a
// post under a beam it is a strut. Aiming at the ground near a log is still the ground.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementContextTest, "MyProject3.Placement.ContextChoosesWhatTheAimMeans", Flags)
bool FPlacementContextTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Post = SpawnCubeLog(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Wall = SpawnCubeLog(World, FVector(600, 0, 100), FRotator::ZeroRotator, 600.0f, 30.0f, true);
    ALogBuildPiece* PostB = SpawnCubeLog(World, FVector(0, 800, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    SpawnCubeLog(World, FVector(0, 800, 215), FRotator::ZeroRotator, 300.0f, 30.0f, true); // A beam on PostB.
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, -300, 50), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    AActor* Player = World->SpawnActor<AActor>(FVector(0, -400, 100), FRotator::ZeroRotator);
    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Player);
    Carry->RegisterComponent();
    if (!TestNotNull(TEXT("Held"), Held) || !TestTrue(TEXT("Pick up"), Carry->TryPickupPiece(Held)))
        return false;

    auto Ask = [&](AActor* Actor, const FVector& Point, float Pitch)
    {
        FLogContextAim Aim;
        Aim.AimActor = Actor;
        Aim.AimPoint = Point;
        Aim.ViewPitchDeg = Pitch;
        Aim.ViewDirection = FRotator(Pitch, 90.0f, 0.0f).Vector();
        Aim.CarryYawDeg = 90.0f;
        return Carry->ResolveContext(Aim);
    };
    auto Profile = [](const FLogContextResult& Result) { return Result.Solution.ProfileId.ToString(); };

    const FLogContextResult OnPost = Ask(Post, FVector(0, 0, 200), -20.0f);
    TestTrue(FString::Printf(TEXT("Post top: a joint (%s, %s)"), *Profile(OnPost), *OnPost.Reason), OnPost.Action == ELogContextAction::Snap);
    TestEqual(TEXT("Post top: it lies across"), Profile(OnPost), FString(TEXT("log.station_to_end")));
    TestTrue(TEXT("Post top: flat"), FMath::Abs(OnPost.Solution.PieceTransform.GetUnitAxis(EAxis::X).Z) < 0.05f);

    const FLogContextResult Extend = Ask(Post, FVector(0, 0, 200), -75.0f);
    TestEqual(TEXT("Post top looking down: it continues the post"), Profile(Extend), FString(TEXT("log.end_to_end")));

    const FLogContextResult OnWall = Ask(Wall, FVector(600, 0, 115), -20.0f);
    TestTrue(FString::Printf(TEXT("Wall top: the next row (%s)"), *OnWall.Reason), OnWall.Action == ELogContextAction::Stack);

    const FLogContextResult AtSide = Ask(Wall, FVector(600, -15, 100), -10.0f);
    TestTrue(FString::Printf(TEXT("Wall side: a T joint (%s, %s)"), *Profile(AtSide), *AtSide.Reason),
        AtSide.Action == ELogContextAction::Snap && Profile(AtSide) == TEXT("log.end_to_station"));

    const FLogContextResult AtEnd = Ask(Wall, FVector(900, 0, 100), -10.0f);
    TestTrue(FString::Printf(TEXT("Wall end: end on end (%s)"), *Profile(AtEnd)),
        AtEnd.Action == ELogContextAction::Snap && Profile(AtEnd) == TEXT("log.end_to_end"));

    TestTrue(TEXT("Ground: it lies"), Ask(nullptr, FVector(0, -600, 0), -30.0f).Action == ELogContextAction::Ground);
    const FLogContextResult Standing = Ask(nullptr, FVector(0, -600, 0), -70.0f);
    TestTrue(TEXT("Ground looking down: a post"), Standing.Action == ELogContextAction::Post && Standing.bUpright);
    TestTrue(TEXT("Ground beside a log is still the ground"), Ask(Wall, FVector(600, -150, 0), -30.0f).Action == ELogContextAction::Ground);

    const FLogContextResult Strut = Ask(PostB, FVector(15, 800, 120), -10.0f);
    TestTrue(FString::Printf(TEXT("Post side under a beam: a strut (%s)"), *Strut.Reason), Strut.Action == ELogContextAction::Strut);
    return true;
}

// Diagonals are symmetric: a log across a post top is centred on it on the
// structure's 45 degree grid, or spans to a second post centred between them; a
// diagonal on a wall mirrors one already there; corner to corner on a frame; a
// second strut takes the leg of the one on the other side of the post.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementSymmetryTest, "MyProject3.Placement.DiagonalsAreSymmetric", Flags)
bool FPlacementSymmetryTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World ? World->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Kernel"), Kernel))
        return false;
    FPlacementCommitOptions Options;
    Options.FromOwner = EWorldItemOwnerKind::World;
    Options.FromLifecycle = EWorldObjectLifecycleState::World;
    auto YawOf = [](const FTransform& Transform)
    {
        const FVector Axis = Transform.GetUnitAxis(EAxis::X);
        return FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X));
    };
    auto SameLine = [](float A, float B) { return PlacementBodyMath::LineYawDistance(A, B) < 0.5f; };

    // 1. A post alone: carried at 50 degrees, the log lies at 45, centred on the top.
    ALogBuildPiece* Post = SpawnCubeLog(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, 600, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = Post;
    Intent.AimPoint = FVector(0, 0, 200);
    Intent.CarryYawDeg = 50.0f;
    Intent.AllowedProfiles = { TEXT("log.station_to_end") };
    const FPlacementSolution Alone = Kernel->Solve(Intent);
    if (!TestTrue(FString::Printf(TEXT("Across the post (%s)"), *Alone.Reason), Alone.bValid))
        return false;
    TestTrue(FString::Printf(TEXT("At 45 degrees (%.1f)"), YawOf(Alone.PieceTransform)), SameLine(YawOf(Alone.PieceTransform), 45.0f));
    TestTrue(FString::Printf(TEXT("Centred on the top (%s)"), *Alone.PieceTransform.GetLocation().ToString()),
        Alone.PieceTransform.GetLocation().Equals(FVector(0, 0, 215), 1.0f));

    // 2. A second free post it reaches, near the carry direction: it spans both, centred.
    ALogBuildPiece* Second = SpawnCubeLog(World, FVector(180, 130, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    Intent.CarryYawDeg = 40.0f;
    const FPlacementSolution Span = Kernel->Solve(Intent);
    const float SpanYaw = FMath::RadiansToDegrees(FMath::Atan2(130.0f, 180.0f));
    TestTrue(FString::Printf(TEXT("Post to post (%.1f, want %.1f)"), YawOf(Span.PieceTransform), SpanYaw),
        Span.bValid && SameLine(YawOf(Span.PieceTransform), SpanYaw));
    TestTrue(FString::Printf(TEXT("Centred between them (%s)"), *Span.PieceTransform.GetLocation().ToString()),
        Span.PieceTransform.GetLocation().Equals(FVector(90, 65, 215), 1.0f));
    TestTrue(TEXT("It rests on the second post too"), Span.GetTarget(EPlacementLinkRole::Secondary) == Second);

    // 3. Carried away from it: the 45 degree grid now runs from the line between the posts.
    Intent.CarryYawDeg = 80.0f;
    const FPlacementSolution OnGrid = Kernel->Solve(Intent);
    TestTrue(FString::Printf(TEXT("On the structure's grid (%.1f, want %.1f)"), YawOf(OnGrid.PieceTransform), SpanYaw + 45.0f),
        OnGrid.bValid && SameLine(YawOf(OnGrid.PieceTransform), SpanYaw + 45.0f) && !OnGrid.FindLink(EPlacementLinkRole::Secondary));

    // 4. A wall of 640 cm (stations not symmetric about its centre): a diagonal, then its mirror.
    ALogBuildPiece* Wall = SpawnCubeLog(World, FVector(0, -1000, 100), FRotator::ZeroRotator, 640.0f, 30.0f, true);
    ALogBuildPiece* First = SpawnCubeLog(World, FVector(0, -1400, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    FPlacementIntent OnWall;
    OnWall.Held = First;
    OnWall.AimActor = Wall;
    OnWall.AimPoint = FVector(-207.5f, -1000, 115);
    OnWall.CarryYawDeg = 45.0f;
    OnWall.AllowedProfiles = { TEXT("log.parallel_stack") };
    const FPlacementSolution Diagonal = Kernel->Solve(OnWall);
    if (!TestTrue(FString::Printf(TEXT("A diagonal on the wall (%s)"), *Diagonal.Reason), Diagonal.bValid))
        return false;
    TestTrue(FString::Printf(TEXT("At 45 degrees to the wall (%.1f)"), YawOf(Diagonal.PieceTransform)),
        SameLine(YawOf(Diagonal.PieceTransform), 45.0f));
    TestTrue(FString::Printf(TEXT("Centred on the station (%s)"), *Diagonal.PieceTransform.GetLocation().ToString()),
        FVector::Dist2D(Diagonal.PieceTransform.GetLocation(), FVector(-207.5f, -1000, 0)) < 1.0f);
    FString Reason;
    TestTrue(FString::Printf(TEXT("First diagonal committed (%s)"), *Reason), Kernel->CommitPlacement(First, Diagonal, Options, Reason));
    ALogBuildPiece* Mirror = SpawnCubeLog(World, FVector(0, -1500, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    OnWall.Held = Mirror;
    OnWall.AimPoint = FVector(205.0f, -1000, 115);
    OnWall.CarryYawDeg = -45.0f;
    const FPlacementSolution Mirrored = Kernel->Solve(OnWall);
    TestTrue(FString::Printf(TEXT("The mirror leans the other way (%.1f)"), YawOf(Mirrored.PieceTransform)),
        Mirrored.bValid && SameLine(YawOf(Mirrored.PieceTransform), -45.0f));
    TestTrue(FString::Printf(TEXT("Exactly at the mirrored place, off the station grid (%s)"), *Mirrored.PieceTransform.GetLocation().ToString()),
        FVector::Dist2D(Mirrored.PieceTransform.GetLocation(), FVector(207.5f, -1000, 0)) < 1.0f);

    // 5. A frame of two parallel beams: from an end station, corner to corner.
    ALogBuildPiece* BeamA = SpawnCubeLog(World, FVector(0, -2000, 100), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    ALogBuildPiece* BeamB = SpawnCubeLog(World, FVector(0, -1880, 100), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    ALogBuildPiece* Brace = SpawnCubeLog(World, FVector(0, -2400, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    FPlacementIntent OnFrame = OnWall;
    OnFrame.Held = Brace;
    OnFrame.AimActor = BeamA;
    OnFrame.AimPoint = FVector(-110.0f, -2000, 115);
    OnFrame.CarryYawDeg = 35.0f;
    const FPlacementSolution Corner = Kernel->Solve(OnFrame);
    const float CornerYaw = FMath::RadiansToDegrees(FMath::Atan2(120.0f, 225.0f));
    TestTrue(FString::Printf(TEXT("Corner to corner (%.1f, want %.1f; %s)"), YawOf(Corner.PieceTransform), CornerYaw, *Corner.Reason),
        Corner.bValid && SameLine(YawOf(Corner.PieceTransform), CornerYaw));
    TestTrue(FString::Printf(TEXT("Centred between the corners (%s)"), *Corner.PieceTransform.GetLocation().ToString()),
        FVector::Dist2D(Corner.PieceTransform.GetLocation(), FVector(0, -1940, 0)) < 1.0f);
    TestTrue(TEXT("It rests on the far beam too"), Corner.GetTarget(EPlacementLinkRole::Secondary) == BeamB);

    // 6. Struts: the post is off the beam's station grid, so the two sides differ until one is built.
    ALogBuildPiece* Column = SpawnCubeLog(World, FVector(0, 3000, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    // The beam rests on the column, its -X end at x -280; built through the kernel so it is in the structure.
    ALogBuildPiece* Beam = SpawnCubeLog(World, FVector(20, 3000, 215), FRotator::ZeroRotator, 600.0f, 30.0f, false);
    const FPlacementSolution BeamOnColumn = Kernel->MakeDirectSolution(Beam, Beam->GetActorTransform(),
        TEXT("log.station_to_end"), Column);
    TestTrue(FString::Printf(TEXT("Beam built on the column (%s)"), *Reason), Kernel->CommitPlacement(Beam, BeamOnColumn, Options, Reason));
    ALogBuildPiece* StrutA = SpawnCubeLog(World, FVector(0, 3600, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    ALogBuildPiece* StrutB = SpawnCubeLog(World, FVector(0, 3700, 300), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    FPlacementIntent Brace1;
    Brace1.Held = StrutA;
    Brace1.AimActor = Column;
    Brace1.AimPoint = FVector(15, 3000, 120);
    Brace1.AllowedProfiles = { TEXT("log.strut") };
    const FPlacementSolution Right = Kernel->Solve(Brace1);
    if (!TestTrue(FString::Printf(TEXT("A strut on the +X side (%s)"), *Right.Reason), Right.bValid))
        return false;
    TestTrue(FString::Printf(TEXT("It meets the beam station 57.5 cm out (%.1f)"), Right.MarkerPoint.X),
        FMath::IsNearlyEqual(Right.MarkerPoint.X, 57.5f, 0.5f));
    FPlacementIntent Brace2 = Brace1;
    Brace2.Held = StrutB;
    Brace2.AimPoint = FVector(-15, 3000, 120);
    const FPlacementSolution LeftAlone = Kernel->Solve(Brace2);
    TestTrue(FString::Printf(TEXT("Alone, the -X side takes its own station (%.1f)"), LeftAlone.MarkerPoint.X),
        LeftAlone.bValid && FMath::IsNearlyEqual(LeftAlone.MarkerPoint.X, -92.5f, 0.5f));
    TestTrue(FString::Printf(TEXT("+X strut committed (%s)"), *Reason), Kernel->CommitPlacement(StrutA, Right, Options, Reason));
    const FPlacementSolution LeftMirrored = Kernel->Solve(Brace2);
    TestTrue(FString::Printf(TEXT("With the +X strut built, the -X one mirrors it (%.1f)"), LeftMirrored.MarkerPoint.X),
        LeftMirrored.bValid && FMath::IsNearlyEqual(LeftMirrored.MarkerPoint.X, -57.5f, 0.5f));
    return true;
}

// End to end, a log bends toward where it is carried, on a 45 degree grid: an
// open corner, an L, a sharp <. Carried back toward the target it goes on straight.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementBendTest, "MyProject3.Placement.EndJointsBendIntoCorners", Flags)
bool FPlacementBendTest::RunTest(const FString& Parameters)
{
    FPlacementTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UPlacementKernelSubsystem* Kernel = World ? World->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
    // A beam along X, its +X end at x 150; radius 15.
    ALogBuildPiece* Beam = SpawnCubeLog(World, FVector(0, 0, 100), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    ALogBuildPiece* Held = SpawnCubeLog(World, FVector(0, 600, 100), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Kernel"), Kernel) || !TestNotNull(TEXT("Beam"), Beam) || !TestNotNull(TEXT("Held"), Held))
        return false;
    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = Beam;
    Intent.AimPoint = FVector(150, 0, 100);
    Intent.AllowedProfiles = { TEXT("log.end_to_end") };
    auto Joint = [&](float CarryYaw)
    {
        Intent.CarryYawDeg = CarryYaw;
        return Kernel->Solve(Intent);
    };
    auto LineYaw = [](const FPlacementSolution& Solution)
    {
        const FVector Axis = Solution.PieceTransform.GetUnitAxis(EAxis::X);
        return FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X));
    };
    // The corner lies one radius inside the beam's end; the held log runs from there.
    const FVector Corner(135, 0, 100);
    struct FCase { float Carry; float Bend; };
    const FCase Cases[] = { { 40.0f, 45.0f }, { 95.0f, 90.0f }, { 140.0f, 135.0f }, { -50.0f, -45.0f } };
    for (const FCase& Case : Cases)
    {
        const FPlacementSolution Bent = Joint(Case.Carry);
        const FVector Direction = FRotator(0.0f, Case.Bend, 0.0f).Vector();
        const FVector Want = Corner + Direction * (150.0f - 15.0f);
        TestTrue(FString::Printf(TEXT("Carried at %.0f: bends %.0f (%s)"), Case.Carry, Case.Bend, *Bent.Reason),
            Bent.bValid && FMath::IsNearlyEqual(Bent.AngleDeg, Case.Bend, 0.1f) &&
            PlacementBodyMath::LineYawDistance(LineYaw(Bent), Case.Bend) < 0.5f);
        TestTrue(FString::Printf(TEXT("Bend %.0f: it runs from the corner (%s, want %s)"), Case.Bend,
            *Bent.PieceTransform.GetLocation().ToString(), *Want.ToString()), Bent.PieceTransform.GetLocation().Equals(Want, 1.0f));
        TestEqual(TEXT("Joined end to end"), Bent.Links.Num() > 0 ? Bent.Links[0].Target.FeatureId : NAME_None, FName(TEXT("End.B")));
    }
    const FPlacementSolution Straight = Joint(0.0f);
    TestTrue(TEXT("Carried on: straight"), Straight.bValid && FMath::IsNearlyZero(Straight.AngleDeg) &&
        PlacementBodyMath::LineYawDistance(LineYaw(Straight), 0.0f) < 0.5f && Straight.PieceTransform.GetLocation().X > 250.0f);
    const FPlacementSolution Back = Joint(180.0f);
    TestTrue(TEXT("Carried back toward the beam: still straight"), Back.bValid && FMath::IsNearlyZero(Back.AngleDeg) &&
        PlacementBodyMath::LineYawDistance(LineYaw(Back), 0.0f) < 0.5f);
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
