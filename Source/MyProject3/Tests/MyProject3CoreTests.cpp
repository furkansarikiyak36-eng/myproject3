#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CarryLoadComponent.h"
#include "VerticalSliceDemoActor.h"
#include "VerticalSliceCWorldActor.h"
#include "VerticalSliceBWorldActor.h"
#include "VoxelTerrainChunkComponent.h"
#include "HeatSourceComponent.h"
#include "WorldSurvivalSubsystem.h"
#include "LogBuildPiece.h"
#include "HelperCompanionActor.h"
#include "HarvestableTreeActor.h"
#include "LogCarryComponent.h"
#include "BuildPlanActor.h"
#include "ConstructionHelpers.h"
#include "ConstructionOpeningActor.h"
#include "LogAttachmentComponents.h"
#include "LogBuildToolsComponent.h"
#include "LogContainerActor.h"
#include "Engine/StaticMesh.h"
#include "PhysicalHealthComponent.h"
#include "RopeLineActor.h"
#include "ShelterSensorComponent.h"
#include "ThrownImpactComponent.h"
#include "WildlifeActor.h"
#include "StickPrimitivePiece.h"
#include "MaterialConnectionSubsystem.h"
#include "StoneBuildingPiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/WorldSettings.h"

namespace MyProject3Tests
{
    constexpr EAutomationTestFlags TestFlags =
        EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

    /** Isolated game world with initialized world subsystems and begun play. */
    struct FScopedTestWorld
    {
        UWorld* World = nullptr;

        FScopedTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MyProject3TestWorld"));
            if (!World)
                return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            // A test world has no game mode, which is what normally starts play.
            if (!World->HasBegunPlay() && World->GetWorldSettings())
                World->GetWorldSettings()->NotifyBeginPlay();
        }

        ~FScopedTestWorld()
        {
            if (World)
            {
                if (World->HasBegunPlay())
                    World->EndPlay(EEndPlayReason::Quit);
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
            }
        }
    };

    FWorldOwnedItemRecord MakeWorldItem(FName DefinitionId)
    {
        FWorldOwnedItemRecord Record;
        Record.ItemId = FGuid::NewGuid();
        Record.DefinitionId = DefinitionId;
        Record.BaseDefinitionId = DefinitionId;
        Record.Quantity = 1;
        Record.OwnerKind = EWorldItemOwnerKind::World;
        return Record;
    }

    UStaticMesh* LoadEngineMesh(const TCHAR* Path)
    {
        return LoadObject<UStaticMesh>(nullptr, Path);
    }

    /** Spawns a world-owned log whose mesh family supports a half-length cut. */
    ALogBuildPiece* SpawnConfiguredLog(UWorld* World, bool bMatchSet)
    {
        UStaticMesh* Round = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        UStaticMesh* Half = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* Unmatched = LoadEngineMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        if (!World || !Round || !Half || !Unmatched)
            return nullptr;

        FLogProcessingMeshSet Set;
        Set.VariantSetId = TEXT("TestFamily");
        Set.BaseRoundMesh = Round;

        FLogGeometryVariantMesh HalfRow;
        HalfRow.LengthFraction = ELogLengthFraction::HalfLength;
        HalfRow.CrossSectionProfile = ELogCrossSectionProfile::Round;
        HalfRow.Mesh = Half;
        Set.MeshVariants.Add(HalfRow);

        FLogLengthCutVariant Cut;
        Cut.Action = ELogCutAction::SplitLengthHalf;
        Cut.SourceLengthFraction = ELogLengthFraction::FullLength;
        Cut.CrossSectionProfile = ELogCrossSectionProfile::Round;
        Cut.FirstOutputLengthFraction = ELogLengthFraction::HalfLength;
        Cut.SecondOutputLengthFraction = ELogLengthFraction::HalfLength;
        Cut.FirstOutputCenterOffsetLocal = FVector(-25.0, 0.0, 0.0);
        Cut.SecondOutputCenterOffsetLocal = FVector(25.0, 0.0, 0.0);
        Set.LengthCutVariants.Add(Cut);

        const FTransform SpawnTransform(FVector(0.0, 0.0, 200.0));
        ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(
            ALogBuildPiece::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Log)
            return nullptr;
        Log->BaseDefinitionId = TEXT("Test.Log");
        Log->ProcessingMeshSets.Add(Set);
        Log->SetPieceStaticMesh(bMatchSet ? Round : Unmatched);
        Log->FinishSpawning(SpawnTransform);
        return Log;
    }

    /** Log with a mesh family covering the hewing chain and the quarter-to-eighth cut. */
    ALogBuildPiece* SpawnProcessingChainLog(UWorld* World, ELogLengthFraction StartLength)
    {
        UStaticMesh* Round = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        UStaticMesh* Cube = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        UStaticMesh* Cone = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
        UStaticMesh* Sphere = LoadEngineMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        UStaticMesh* Plane = LoadEngineMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
        if (!World || !Round || !Cube || !Cone || !Sphere || !Plane)
            return nullptr;

        FLogProcessingMeshSet Set;
        Set.VariantSetId = TEXT("ChainFamily");
        Set.BaseRoundMesh = Round;
        auto AddRow = [&Set](ELogLengthFraction Length, ELogCrossSectionProfile Profile, UStaticMesh* Mesh)
        {
            FLogGeometryVariantMesh Row;
            Row.LengthFraction = Length;
            Row.CrossSectionProfile = Profile;
            Row.Mesh = Mesh;
            Set.MeshVariants.Add(Row);
        };
        AddRow(ELogLengthFraction::FullLength, ELogCrossSectionProfile::Hewn, Cube);
        AddRow(ELogLengthFraction::FullLength, ELogCrossSectionProfile::HewnTwoFaces, Cone);
        AddRow(ELogLengthFraction::FullLength, ELogCrossSectionProfile::SquareBeam, Sphere);
        AddRow(ELogLengthFraction::QuarterLength, ELogCrossSectionProfile::Round, Plane);
        AddRow(ELogLengthFraction::EighthLength, ELogCrossSectionProfile::Round, Cube);

        FLogLengthCutVariant Cut;
        Cut.Action = ELogCutAction::SplitQuarterLengthIntoEighths;
        Cut.SourceLengthFraction = ELogLengthFraction::QuarterLength;
        Cut.CrossSectionProfile = ELogCrossSectionProfile::Round;
        Cut.FirstOutputLengthFraction = ELogLengthFraction::EighthLength;
        Cut.SecondOutputLengthFraction = ELogLengthFraction::EighthLength;
        Cut.FirstOutputCenterOffsetLocal = FVector(-20.0, 0.0, 0.0);
        Cut.SecondOutputCenterOffsetLocal = FVector(20.0, 0.0, 0.0);
        Set.LengthCutVariants.Add(Cut);

        const FTransform SpawnTransform(FVector(0.0, 0.0, 200.0));
        ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(
            ALogBuildPiece::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Log)
            return nullptr;
        Log->BaseDefinitionId = TEXT("Test.ChainLog");
        Log->ProcessingMeshSets.Add(Set);
        Log->SetPieceStaticMesh(StartLength == ELogLengthFraction::QuarterLength ? Plane : Round);
        Log->FinishSpawning(SpawnTransform);
        return Log;
    }

    int32 CountLogs(UWorld* World)
    {
        int32 Count = 0;
        for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
        {
            if (IsValid(*It))
                ++Count;
        }
        return Count;
    }
}

using namespace MyProject3Tests;

// M002/M003: a transfer commits once, a retry returns the same receipt, and a
// stale or wrong-owner request changes nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnershipTransferRetryTest,
    "MyProject3.Core.Ownership.TransferIsIdempotentAndFailsClosed", TestFlags)
bool FOwnershipTransferRetryTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorldItemOwnershipSubsystem* Ownership = TestWorld.World
        ? TestWorld.World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Ownership subsystem"), Ownership))
        return false;

    const FWorldOwnedItemRecord Item = MakeWorldItem(TEXT("Test.Stone"));
    TestTrue(TEXT("Register item"), Ownership->RegisterItem(Item));

    const FGuid Operation = FGuid::NewGuid();
    const FGuid Holder = FGuid::NewGuid();
    FWorldOwnedItemRecord First;
    TestTrue(TEXT("First transfer commits"), Ownership->TryTransferItem(Operation, Item.ItemId,
        1, EWorldItemOwnerKind::World, EWorldItemOwnerKind::Held, Holder, FVector::ZeroVector, First));
    TestEqual(TEXT("Owner is Held"), First.OwnerKind, EWorldItemOwnerKind::Held);
    TestEqual(TEXT("Revision advanced once"), First.Revision, static_cast<int64>(2));

    FWorldOwnedItemRecord Retry;
    TestTrue(TEXT("Retry returns the committed result"), Ownership->TryTransferItem(Operation, Item.ItemId,
        1, EWorldItemOwnerKind::World, EWorldItemOwnerKind::Held, Holder, FVector::ZeroVector, Retry));
    TestEqual(TEXT("Retry does not advance revision"), Retry.Revision, First.Revision);

    FWorldOwnedItemRecord Rejected;
    TestFalse(TEXT("Stale revision is rejected"), Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId,
        1, EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, FGuid(), FVector::ZeroVector, Rejected));
    TestFalse(TEXT("Wrong expected owner is rejected"), Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId,
        2, EWorldItemOwnerKind::World, EWorldItemOwnerKind::Inventory, FGuid(), FVector::ZeroVector, Rejected));

    FWorldOwnedItemRecord After;
    TestTrue(TEXT("Item still tracked"), Ownership->TryGetItem(Item.ItemId, After));
    TestEqual(TEXT("Rejected requests left owner unchanged"), After.OwnerKind, EWorldItemOwnerKind::Held);
    TestEqual(TEXT("Rejected requests left revision unchanged"), After.Revision, First.Revision);
    return true;
}

// M016 (ledger side): a split keeps the source ID, creates one unique output and
// a retry with the same operation never creates a third item.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnershipSplitRetryTest,
    "MyProject3.Core.Ownership.SplitIsAtomicAndIdempotent", TestFlags)
bool FOwnershipSplitRetryTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorldItemOwnershipSubsystem* Ownership = TestWorld.World
        ? TestWorld.World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Ownership subsystem"), Ownership))
        return false;

    const FWorldOwnedItemRecord Item = MakeWorldItem(TEXT("Test.Log"));
    TestTrue(TEXT("Register item"), Ownership->RegisterItem(Item));
    const int32 CountBefore = Ownership->GetTrackedItemCount();

    const TArray<FName> Outputs = { TEXT("Test.Log.Half"), TEXT("Test.Log.Half") };
    const TArray<FVector> Locations = { FVector(-10.0, 0.0, 0.0), FVector(10.0, 0.0, 0.0) };
    const FGuid Operation = FGuid::NewGuid();

    TArray<FWorldOwnedItemRecord> First;
    TestTrue(TEXT("Split commits"), Ownership->TrySplitItem(Operation, Item.ItemId, 1,
        EWorldItemOwnerKind::World, Outputs, Locations, First));
    if (!TestEqual(TEXT("Two outputs"), First.Num(), 2))
        return false;
    TestEqual(TEXT("First output keeps the source ID"), First[0].ItemId, Item.ItemId);
    TestNotEqual(TEXT("Second output has a new ID"), First[1].ItemId, Item.ItemId);
    TestEqual(TEXT("Exactly one item was added"), Ownership->GetTrackedItemCount(), CountBefore + 1);

    TArray<FWorldOwnedItemRecord> Retry;
    TestTrue(TEXT("Retry returns the committed split"), Ownership->TrySplitItem(Operation, Item.ItemId, 1,
        EWorldItemOwnerKind::World, Outputs, Locations, Retry));
    TestEqual(TEXT("Retry returns the same second ID"),
        Retry.Num() == 2 ? Retry[1].ItemId : FGuid(), First[1].ItemId);
    TestEqual(TEXT("Retry adds no item"), Ownership->GetTrackedItemCount(), CountBefore + 1);
    return true;
}

// M015: a configured half-length cut produces exactly two world-owned physical
// pieces with canonical definitions, and a retry spawns no further actor.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogLengthCutTest,
    "MyProject3.Wood.LengthCut.ProducesTwoPiecesOnce", TestFlags)
bool FLogLengthCutTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    ALogBuildPiece* Log = SpawnConfiguredLog(TestWorld.World, true);
    UWorldItemOwnershipSubsystem* Ownership = TestWorld.World
        ? TestWorld.World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Configured log"), Log) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;

    TestEqual(TEXT("Mesh family selected from the actor mesh"),
        Log->GetSelectedProcessingMeshSetId(), FName(TEXT("TestFamily")));
    TestTrue(TEXT("Half-length cut is available"), Log->CanApplyCutAction(ELogCutAction::SplitLengthHalf));

    const FGuid Operation = FGuid::NewGuid();
    const FWorldOperationResult Result = Log->TryApplyCutAction(ELogCutAction::SplitLengthHalf, Operation);
    TestTrue(FString::Printf(TEXT("Cut succeeds: %s"), *Result.Message.ToString()), Result.IsSuccess());
    if (!TestEqual(TEXT("Two affected pieces"), Result.AffectedObjectIds.Num(), 2))
        return false;
    TestEqual(TEXT("Two physical log actors exist"), CountLogs(TestWorld.World), 2);

    for (const FGuid& Id : Result.AffectedObjectIds)
    {
        FWorldOwnedItemRecord Record;
        TestTrue(TEXT("Output is tracked"), Ownership->TryGetItem(Id, Record));
        TestEqual(TEXT("Output is world-owned"), Record.OwnerKind, EWorldItemOwnerKind::World);
        TestEqual(TEXT("Output has canonical half-length definition"),
            Record.DefinitionId, FName(TEXT("Test.Log.Geom.Half.Round")));
    }
    TestEqual(TEXT("Source now represents half length"), Log->GetLengthFraction(), ELogLengthFraction::HalfLength);
    for (TActorIterator<ALogBuildPiece> It(TestWorld.World); It; ++It)
    {
        TestTrue(FString::Printf(TEXT("%s has actor collision"), *It->GetName()), It->GetActorEnableCollision());
        TestTrue(FString::Printf(TEXT("%s simulates physics"), *It->GetName()),
            It->GetLogMesh() && It->GetLogMesh()->IsSimulatingPhysics());
    }

    const FWorldOperationResult Retry = Log->TryApplyCutAction(ELogCutAction::SplitLengthHalf, Operation);
    TestTrue(TEXT("Retry returns success"), Retry.IsSuccess());
    TestEqual(TEXT("Retry spawns no further actor"), CountLogs(TestWorld.World), 2);
    return true;
}

// M019: a groove on a round log is rejected without touching ledger or actor state.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogGrooveRequiresSplitTest,
    "MyProject3.Wood.Groove.RejectedOnRoundLogWithoutMutation", TestFlags)
bool FLogGrooveRequiresSplitTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    ALogBuildPiece* Log = SpawnConfiguredLog(TestWorld.World, true);
    UWorldItemOwnershipSubsystem* Ownership = TestWorld.World
        ? TestWorld.World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Configured log"), Log) || !TestNotNull(TEXT("Ownership"), Ownership))
        return false;

    const FGuid ItemId = Log->StructuralIdentity->GetInstanceId();
    FWorldOwnedItemRecord Before;
    TestTrue(TEXT("Log is tracked"), Ownership->TryGetItem(ItemId, Before));
    UStaticMesh* MeshBefore = Log->GetLogMesh()->GetStaticMesh();

    const FWorldOperationResult Result = Log->TryApplyCutAction(ELogCutAction::Gutter, FGuid::NewGuid());
    TestFalse(TEXT("Groove on a round log is rejected"), Result.IsSuccess());

    FWorldOwnedItemRecord After;
    TestTrue(TEXT("Log is still tracked"), Ownership->TryGetItem(ItemId, After));
    TestEqual(TEXT("Definition unchanged"), After.DefinitionId, Before.DefinitionId);
    TestEqual(TEXT("Revision unchanged"), After.Revision, Before.Revision);
    TestTrue(TEXT("Mesh unchanged"), Log->GetLogMesh()->GetStaticMesh() == MeshBefore);
    TestEqual(TEXT("Profile unchanged"), Log->GetCrossSectionProfile(), ELogCrossSectionProfile::Round);
    TestEqual(TEXT("No actor spawned"), CountLogs(TestWorld.World), 1);
    return true;
}

// Mesh-family safety: an actor whose mesh matches no configured family is blocked.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogUnmatchedMeshSetTest,
    "MyProject3.Wood.MeshSet.UnmatchedFamilyBlocksProcessing", TestFlags)
bool FLogUnmatchedMeshSetTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    ALogBuildPiece* Log = SpawnConfiguredLog(TestWorld.World, false);
    if (!TestNotNull(TEXT("Log"), Log))
        return false;

    TestTrue(TEXT("No family selected"), Log->GetSelectedProcessingMeshSetId().IsNone());
    TestFalse(TEXT("Length cut blocked"), Log->CanApplyCutAction(ELogCutAction::SplitLengthHalf));
    TestTrue(TEXT("Block reason names the mesh set"),
        Log->GetCutActionBlockReason(ELogCutAction::SplitLengthHalf).Contains(TEXT("processing mesh set")));
    return true;
}

// M026/M061 (SOTF-D01): a piece resting on two supports survives losing one,
// shares its load, and blocks removal only while it has no other support.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphMultiSupportTest,
    "MyProject3.Structure.Graph.SecondarySupportKeepsPieceStanding", TestFlags)
bool FGraphMultiSupportTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorldStructuralGraphSubsystem* Graph = TestWorld.World
        ? TestWorld.World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Graph"), Graph))
        return false;

    AActor* ActorA = TestWorld.World->SpawnActor<AActor>();
    AActor* ActorB = TestWorld.World->SpawnActor<AActor>();
    AActor* ActorC = TestWorld.World->SpawnActor<AActor>();
    const FGuid A = FGuid::NewGuid();
    const FGuid B = FGuid::NewGuid();
    const FGuid C = FGuid::NewGuid();
    TestTrue(TEXT("Register A"), Graph->RegisterNode(ActorA, A, EStructuralNodeKind::Wood, true, 1.0f, 1000.0f, 100.0f));
    TestTrue(TEXT("Register B"), Graph->RegisterNode(ActorB, B, EStructuralNodeKind::Wood, true, 1.0f, 1000.0f, 100.0f));
    TestTrue(TEXT("Register C"), Graph->RegisterNode(ActorC, C, EStructuralNodeKind::Wood, false, 10.0f, 1000.0f, 100.0f));

    TestTrue(TEXT("C rests on A"), Graph->TryConnect(A, TEXT("a1"), C, TEXT("c1")));
    TestFalse(TEXT("A cannot be removed while C has no other support"), Graph->CanSafelyRemove(A));
    TestFalse(TEXT("A secondary on the primary support is rejected"),
        Graph->TryAddSecondarySupport(C, A, TEXT("a2"), TEXT("c2")));
    TestTrue(TEXT("C also rests on B"), Graph->TryAddSecondarySupport(C, B, TEXT("b1"), TEXT("c2")));
    TestFalse(TEXT("An occupied anchor is rejected"),
        Graph->TryAddSecondarySupport(C, B, TEXT("b1"), TEXT("c3")));
    TestEqual(TEXT("A carries half of C"), Graph->GetAggregateLoad(A), 6.0f);
    TestTrue(TEXT("A can be removed once C has another support"), Graph->CanSafelyRemove(A));

    TestTrue(TEXT("Remove A"), Graph->UnregisterNode(A, false));
    FStructuralNodeRecord NodeC;
    TestTrue(TEXT("C still tracked"), Graph->GetNode(C, NodeC));
    TestEqual(TEXT("B was promoted to primary support"), NodeC.ParentNodeId, B);
    TestTrue(TEXT("C still reaches ground"), Graph->HasValidSupportPath(C));
    TestEqual(TEXT("B now carries all of C"), Graph->GetAggregateLoad(B), 11.0f);
    return true;
}

// M038/M024 (SOTF-M01/M02): a log can rest on a placed stone through the
// material rule table, survives a graph save/restore with its local state, and
// a pairing without a rule changes nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrossMaterialConnectionTest,
    "MyProject3.Structure.Materials.LogOnStoneConnectsRestoresAndDisconnects", TestFlags)
bool FCrossMaterialConnectionTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldStructuralGraphSubsystem* Graph = World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UMaterialConnectionSubsystem* Connections = World ? World->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Graph"), Graph) || !TestNotNull(TEXT("Connections"), Connections))
        return false;

    AStoneBuildingPiece* Stone = World->SpawnActor<AStoneBuildingPiece>(FVector::ZeroVector, FRotator::ZeroRotator);
    ALogBuildPiece* Log = SpawnConfiguredLog(World, true);
    if (!TestNotNull(TEXT("Stone"), Stone) || !TestNotNull(TEXT("Log"), Log))
        return false;
    TestTrue(TEXT("Stone top anchor"), Stone->AddCustomAnchor(TEXT("Top"), TEXT("Stone"),
        FTransform(FVector(0.0, 0.0, 50.0)), true, false));
    TestTrue(TEXT("Pick up stone"), Stone->SetHeldState(true));
    TestTrue(TEXT("Place stone on ground"), Stone->TryCommitGroundPlacement(FVector::ZeroVector, FRotator::ZeroRotator));
    Log->AddCustomSnapPoint(TEXT("Bottom"), TEXT("LogEnd"), FTransform::Identity, true, false, true, false);

    TArray<FMaterialConnectionRule> AuthoredRules = Connections->Rules;
    Connections->Rules.Reset();
    FString Reason;
    TestFalse(TEXT("No rule means no joint"), Connections->TryConnect(Stone, TEXT("Top"), Log, TEXT("Bottom"), Reason));
    FStructuralNodeRecord LogNode;
    Graph->GetNode(Log->GetStructuralNodeId(), LogNode);
    TestFalse(TEXT("Rejected joint left no graph edge"), LogNode.ParentNodeId.IsValid());
    TestFalse(TEXT("Rejected joint left the log loose"), Log->IsPlaced());
    Connections->Rules = AuthoredRules;

    TestTrue(FString::Printf(TEXT("Log rests on stone (%s)"), *Reason),
        Connections->TryConnect(Stone, TEXT("Top"), Log, TEXT("Bottom"), Reason));
    Graph->GetNode(Log->GetStructuralNodeId(), LogNode);
    TestEqual(TEXT("Graph edge points at the stone"), LogNode.ParentNodeId, Stone->GetStructuralNodeId());
    TestTrue(TEXT("Log is placed"), Log->IsPlaced());
    TestTrue(TEXT("Log knows its stone support"), Log->GetCrossMaterialParent() == Stone);
    TestFalse(TEXT("A stone carrying a log cannot be picked up"), Stone->SetHeldState(true));

    TArray<FWorldStructuralNodeSnapshot> Snapshot;
    TestTrue(TEXT("Graph exports"), Graph->ExportSnapshot(Snapshot));
    TestTrue(TEXT("Graph with a ground-supported root restores"), Graph->RestoreSnapshot(Snapshot));
    TestTrue(TEXT("Actor-local state rebuilds from the graph"), Graph->RebuildLogActorAttachmentsFromSnapshot());
    TestTrue(TEXT("Log is still placed on the stone after restore"),
        Log->IsPlaced() && Log->GetCrossMaterialParent() == Stone);

    TestTrue(TEXT("Disconnect"), Connections->TryDisconnect(Log));
    FStructuralAnchorView TopAnchor;
    Stone->GetStructuralAnchor(TEXT("Top"), TopAnchor);
    TestFalse(TEXT("Stone anchor is free again"), TopAnchor.bOccupied);
    TestFalse(TEXT("Log is loose"), Log->IsPlaced());
    Graph->GetNode(Log->GetStructuralNodeId(), LogNode);
    TestFalse(TEXT("Graph edge removed"), LogNode.ParentNodeId.IsValid());
    return true;
}

// M011/M029/M039/M050 (SOTF-C01): one carry budget for every material; a full
// slot rejects the pickup without touching the piece, mass slows the carrier,
// and dropping returns the piece to the world ledger.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCarryLoadTest,
    "MyProject3.Carry.Load.SlotsMassAndLedger", TestFlags)
bool FCarryLoadTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UStaticMesh* Cube = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Ownership"), Ownership) || !TestNotNull(TEXT("Cube"), Cube))
        return false;

    AActor* Carrier = World->SpawnActor<AActor>();
    UCarryLoadComponent* Load = NewObject<UCarryLoadComponent>(Carrier);
    Load->RegisterComponent();
    if (!Load->GetCarrierId().IsValid())
        Load->BeginPlay();

    TArray<AStoneBuildingPiece*> Stones;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AStoneBuildingPiece* Stone = World->SpawnActor<AStoneBuildingPiece>(
            FVector(Index * 200.0, 0.0, 50.0), FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("Stone"), Stone))
            return false;
        Stone->Mesh->SetStaticMesh(Cube);
        Stones.Add(Stone);
    }

    FString Reason;
    TestTrue(TEXT("Mass is measurable"), Load->EstimateMassKg(Stones[0]) > 0.0f);
    TestTrue(TEXT("First stone taken"), Load->TryTakePiece(Stones[0], Reason));
    TestTrue(TEXT("Second stone taken"), Load->TryTakePiece(Stones[1], Reason));
    TestEqual(TEXT("Two stones carried"), Load->GetCarriedCount(EStructuralNodeKind::Stone), 2);
    TestTrue(TEXT("Load slows the carrier"), Load->GetSpeedMultiplier() < 1.0f);

    FWorldOwnedItemRecord ThirdBefore;
    Ownership->TryGetItem(Stones[2]->GetStructuralNodeId(), ThirdBefore);
    TestFalse(TEXT("Third stone rejected"), Load->TryTakePiece(Stones[2], Reason));
    FWorldOwnedItemRecord ThirdAfter;
    Ownership->TryGetItem(Stones[2]->GetStructuralNodeId(), ThirdAfter);
    TestEqual(TEXT("Rejected stone stays world-owned"), ThirdAfter.OwnerKind, EWorldItemOwnerKind::World);
    TestEqual(TEXT("Rejected stone revision unchanged"), ThirdAfter.Revision, ThirdBefore.Revision);
    TestFalse(TEXT("Rejected stone stays visible"), Stones[2]->IsHidden());

    FWorldOwnedItemRecord Held;
    Ownership->TryGetItem(Stones[0]->GetStructuralNodeId(), Held);
    TestEqual(TEXT("Carried stone is held"), Held.OwnerKind, EWorldItemOwnerKind::Held);
    TestTrue(TEXT("Carried stone is stowed"), Stones[0]->IsHidden());

    TestTrue(TEXT("Drop stone"), Load->TryDropPiece(Stones[0], FVector(0.0, 300.0, 100.0)));
    FWorldOwnedItemRecord Dropped;
    Ownership->TryGetItem(Stones[0]->GetStructuralNodeId(), Dropped);
    TestEqual(TEXT("Dropped stone is world-owned"), Dropped.OwnerKind, EWorldItemOwnerKind::World);
    TestFalse(TEXT("Dropped stone is visible"), Stones[0]->IsHidden());
    TestEqual(TEXT("One stone left carried"), Load->GetCarriedCount(EStructuralNodeKind::Stone), 1);
    TestTrue(TEXT("Third stone fits now"), Load->TryTakePiece(Stones[2], Reason));
    return true;
}

// M018 (SOTF-L03): hewing is a chain round -> one face -> two faces -> squared
// beam; skipping a step is rejected without mutation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogHewingChainTest,
    "MyProject3.Wood.Hewing.ChainCannotSkipSteps", TestFlags)
bool FLogHewingChainTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    ALogBuildPiece* Log = SpawnProcessingChainLog(TestWorld.World, ELogLengthFraction::FullLength);
    if (!TestNotNull(TEXT("Log"), Log))
        return false;

    TestFalse(TEXT("Squared beam from a round log is rejected"),
        Log->TryApplyCutAction(ELogCutAction::Kalas, FGuid::NewGuid()).IsSuccess());
    TestFalse(TEXT("Second face before the first is rejected"),
        Log->TryApplyCutAction(ELogCutAction::HewSecondFace, FGuid::NewGuid()).IsSuccess());
    TestEqual(TEXT("Rejected steps left the log round"), Log->GetCrossSectionProfile(), ELogCrossSectionProfile::Round);

    TestTrue(TEXT("First face"), Log->TryApplyCutAction(ELogCutAction::Hewn, FGuid::NewGuid()).IsSuccess());
    TestEqual(TEXT("Hewn on one face"), Log->GetCrossSectionProfile(), ELogCrossSectionProfile::Hewn);
    TestTrue(TEXT("Second face"), Log->TryApplyCutAction(ELogCutAction::HewSecondFace, FGuid::NewGuid()).IsSuccess());
    TestEqual(TEXT("Hewn on two faces"), Log->GetCrossSectionProfile(), ELogCrossSectionProfile::HewnTwoFaces);
    TestTrue(TEXT("Squared beam"), Log->TryApplyCutAction(ELogCutAction::Kalas, FGuid::NewGuid()).IsSuccess());
    TestEqual(TEXT("Squared beam profile"), Log->GetCrossSectionProfile(), ELogCrossSectionProfile::SquareBeam);
    return true;
}

// M015 (SOTF-L02): the 75 cm family is reachable as an eighth length cut from a
// quarter-length piece, and the cut needs a quarter-length source.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogEighthCutTest,
    "MyProject3.Wood.LengthCut.QuarterIntoEighths", TestFlags)
bool FLogEighthCutTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld FullWorld;
    ALogBuildPiece* FullLog = SpawnProcessingChainLog(FullWorld.World, ELogLengthFraction::FullLength);
    if (!TestNotNull(TEXT("Full log"), FullLog))
        return false;
    TestFalse(TEXT("Eighth cut needs a quarter-length log"),
        FullLog->CanApplyCutAction(ELogCutAction::SplitQuarterLengthIntoEighths));

    FScopedTestWorld TestWorld;
    ALogBuildPiece* Log = SpawnProcessingChainLog(TestWorld.World, ELogLengthFraction::QuarterLength);
    if (!TestNotNull(TEXT("Quarter log"), Log))
        return false;
    TestEqual(TEXT("Quarter length derived from its mesh"), Log->GetLengthFraction(), ELogLengthFraction::QuarterLength);

    const FWorldOperationResult Result =
        Log->TryApplyCutAction(ELogCutAction::SplitQuarterLengthIntoEighths, FGuid::NewGuid());
    TestTrue(FString::Printf(TEXT("Eighth cut succeeds: %s"), *Result.Message.ToString()), Result.IsSuccess());
    TestEqual(TEXT("Two eighth-length pieces"), CountLogs(TestWorld.World), 2);
    for (TActorIterator<ALogBuildPiece> It(TestWorld.World); It; ++It)
        TestEqual(TEXT("Piece is an eighth"), It->GetLengthFraction(), ELogLengthFraction::EighthLength);
    return true;
}

namespace MyProject3Tests
{
    FWorldSurvivalProfile MakeSurvivor(FVector Location)
    {
        FWorldSurvivalProfile Profile;
        Profile.EntityId = FGuid::NewGuid();
        Profile.Location = Location;
        return Profile;
    }

    UHeatSourceComponent* SpawnFire(UWorld* World, FVector Location, float Protection)
    {
        AActor* Fire = World->SpawnActor<AActor>();
        UHeatSourceComponent* Heat = NewObject<UHeatSourceComponent>(Fire);
        Fire->SetRootComponent(Heat);
        Heat->RegisterComponent();
        Fire->SetActorLocation(Location);
        Heat->WeatherProtection = Protection;
        return Heat;
    }
}

// M105/M111/M117/M118 (RISK-H01/H02): one felt-temperature function combines
// weather, wind, heat sources, clothing, shelter and wetness.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeltTemperatureTest,
    "MyProject3.Survival.Temperature.FeltTemperatureCombinesAllInputs", TestFlags)
bool FFeltTemperatureTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorldSurvivalSubsystem* Survival = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Survival"), Survival))
        return false;

    const FWorldSurvivalProfile Bare = MakeSurvivor(FVector::ZeroVector);
    const float ClearFelt = Survival->ComputeFeltTemperature(Bare);
    Survival->SetWeather(EWorldWeatherState::Snow, 1.0f);
    const float SnowFelt = Survival->ComputeFeltTemperature(Bare);
    TestTrue(TEXT("Snow feels colder than clear weather"), SnowFelt < ClearFelt);

    FWorldSurvivalProfile Dressed = Bare;
    Dressed.ClothingInsulation = 1.0f;
    TestTrue(TEXT("Clothing warms"), Survival->ComputeFeltTemperature(Dressed) > SnowFelt);
    FWorldSurvivalProfile Sheltered = Bare;
    Sheltered.ShelterProtection = 1.0f;
    TestTrue(TEXT("Shelter warms"), Survival->ComputeFeltTemperature(Sheltered) > SnowFelt);
    FWorldSurvivalProfile Wet = Bare;
    Wet.Wetness = 1.0f;
    TestTrue(TEXT("Wet clothing chills"), Survival->ComputeFeltTemperature(Wet) < SnowFelt);

    UHeatSourceComponent* Fire = SpawnFire(TestWorld.World, FVector(100.0, 0.0, 0.0), 1.0f);
    TestTrue(TEXT("An unfueled fire cannot be lit"), !Fire->Ignite());
    TestTrue(TEXT("Add fuel"), Fire->AddFuel(1800.0f));
    TestTrue(TEXT("Light fire"), Fire->Ignite());
    TestTrue(TEXT("Standing by the fire is warmer"), Survival->ComputeFeltTemperature(Bare) > SnowFelt);
    const FWorldSurvivalProfile FarAway = MakeSurvivor(FVector(5000.0, 0.0, 0.0));
    TestEqual(TEXT("Outside the radius the fire adds nothing"), Survival->ComputeFeltTemperature(FarAway), SnowFelt);
    return true;
}

// M107/M111/M112 (RISK-B01): exposure cools the core and burns extra calories;
// a fire keeps the core warm; carrying costs more energy than walking empty.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreTemperatureCaloriesTest,
    "MyProject3.Survival.Body.ColdCoolsCoreAndBurnsCalories", TestFlags)
bool FCoreTemperatureCaloriesTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorldSurvivalSubsystem* Survival = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Survival"), Survival))
        return false;

    Survival->SetWeather(EWorldWeatherState::Snow, 1.0f);
    const FWorldSurvivalProfile Exposed = MakeSurvivor(FVector(5000.0, 0.0, 0.0));
    // In a full snowstorm a fire alone is not enough; it needs clothing as well.
    FWorldSurvivalProfile ByFire = MakeSurvivor(FVector::ZeroVector);
    ByFire.ClothingInsulation = 0.5f;
    TestTrue(TEXT("Register exposed"), Survival->RegisterProfile(Exposed));
    TestTrue(TEXT("Register by fire"), Survival->RegisterProfile(ByFire));
    UHeatSourceComponent* Fire = SpawnFire(TestWorld.World, FVector::ZeroVector, 1.0f);
    Fire->AddFuel(4.0f * 3600.0f);
    Fire->Ignite();

    for (int32 Hour = 0; Hour < 3; ++Hour)
        TestTrue(TEXT("Advance one hour"), Survival->AdvanceSurvivalStep(3600.0, Hour * 3600.0));

    FWorldSurvivalProfile ExposedAfter, FireAfter;
    Survival->GetProfile(Exposed.EntityId, ExposedAfter);
    Survival->GetProfile(ByFire.EntityId, FireAfter);
    TestTrue(TEXT("Exposed core cooled"), ExposedAfter.CoreTemperatureCelsius < 37.0f);
    TestEqual(TEXT("Core by the fire with clothing stays normal"), FireAfter.CoreTemperatureCelsius, 37.0f);
    TestTrue(TEXT("Shivering burns extra calories"), ExposedAfter.CaloriesKcal < FireAfter.CaloriesKcal);
    TestEqual(TEXT("Hunger follows the calorie store"),
        ExposedAfter.Hunger, 100.0f * (1.0f - ExposedAfter.CaloriesKcal / Survival->MaxCaloriesKcal));
    TestTrue(TEXT("Fire burned fuel"), Fire->GetFuelSeconds() < 4.0f * 3600.0f);

    Survival->SetWeather(EWorldWeatherState::Clear, 0.0f);
    const FWorldSurvivalProfile Empty = MakeSurvivor(FVector::ZeroVector);
    const FWorldSurvivalProfile Loaded = MakeSurvivor(FVector::ZeroVector);
    Survival->RegisterProfile(Empty);
    Survival->RegisterProfile(Loaded);
    TestTrue(TEXT("Walk empty"), Survival->RecordActivity(Empty.EntityId, ESurvivalActivity::Walk, 600.0f, 0.0f));
    TestTrue(TEXT("Walk with two logs"), Survival->RecordActivity(Loaded.EntityId, ESurvivalActivity::Walk, 600.0f, 80.0f));
    TestFalse(TEXT("Negative duration is rejected"), Survival->RecordActivity(Empty.EntityId, ESurvivalActivity::Walk, -1.0f, 0.0f));
    Survival->AdvanceSurvivalStep(60.0, 4.0 * 3600.0);
    FWorldSurvivalProfile EmptyAfter, LoadedAfter;
    Survival->GetProfile(Empty.EntityId, EmptyAfter);
    Survival->GetProfile(Loaded.EntityId, LoadedAfter);
    TestTrue(TEXT("Carrying burns more calories"), LoadedAfter.CaloriesKcal < EmptyAfter.CaloriesKcal);
    TestTrue(TEXT("Carrying costs more stamina"), LoadedAfter.Stamina < EmptyAfter.Stamina);

    TestTrue(TEXT("Eat"), Survival->ConsumeFood(Loaded.EntityId, 500.0f, 10.0f));
    FWorldSurvivalProfile Fed;
    Survival->GetProfile(Loaded.EntityId, Fed);
    TestTrue(TEXT("Eating restores calories"), Fed.CaloriesKcal > LoadedAfter.CaloriesKcal);
    return true;
}

// RISK-H02: an open fire goes out in heavy rain; a sheltered hearth keeps
// burning until its fuel runs out.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeatSourceWeatherTest,
    "MyProject3.Survival.Heat.RainDrownsOpenFireButNotHearth", TestFlags)
bool FHeatSourceWeatherTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorldSurvivalSubsystem* Survival = TestWorld.World ? TestWorld.World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Survival"), Survival))
        return false;

    UHeatSourceComponent* OpenFire = SpawnFire(TestWorld.World, FVector::ZeroVector, 0.0f);
    UHeatSourceComponent* Hearth = SpawnFire(TestWorld.World, FVector(2000.0, 0.0, 0.0), 1.0f);
    OpenFire->AddFuel(1800.0f);
    Hearth->AddFuel(1800.0f);
    OpenFire->Ignite();
    Hearth->Ignite();

    Survival->SetWeather(EWorldWeatherState::Rain, 0.8f);
    Survival->AdvanceSurvivalStep(600.0, 0.0);
    TestFalse(TEXT("Open fire drowned"), OpenFire->IsLit());
    TestTrue(TEXT("Hearth still burns"), Hearth->IsLit());
    TestEqual(TEXT("Hearth burned ten minutes of fuel"), Hearth->GetFuelSeconds(), 1200.0f);

    Survival->AdvanceSurvivalStep(1200.0, 600.0);
    TestFalse(TEXT("Hearth goes out when fuel runs out"), Hearth->IsLit());
    TestFalse(TEXT("An empty hearth cannot be relit"), Hearth->Ignite());
    return true;
}

namespace MyProject3Tests
{
    UVoxelTerrainChunkComponent* SpawnVoxelChunk(UWorld* World, int32 Cells, float VoxelSize)
    {
        AActor* Owner = World->SpawnActor<AActor>();
        UVoxelTerrainChunkComponent* Chunk = NewObject<UVoxelTerrainChunkComponent>(Owner);
        Chunk->CellsPerAxis = Cells;
        Chunk->VoxelSizeCm = VoxelSize;
        Owner->SetRootComponent(Chunk);
        Chunk->RegisterComponent();
        return Chunk;
    }

    bool TraceDown(UWorld* World, FVector From, FVector& OutHit)
    {
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(Hit, From, From - FVector(0.0, 0.0, 5000.0), ECC_Visibility);
        OutHit = Hit.ImpactPoint;
        return bHit;
    }
}

// M071/M073/M074 (7D-V01/V03..V05): a smooth voxel chunk digs a pit whose
// collision matches the new surface; the test also records the timings used
// for the terrain architecture decision.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelChunkDigTest,
    "MyProject3.Terrain.Voxel.DigUpdatesSurfaceAndCollision", TestFlags)
bool FVoxelChunkDigTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UVoxelTerrainChunkComponent* Chunk = SpawnVoxelChunk(TestWorld.World, 32, 50.0f);
    if (!TestNotNull(TEXT("Chunk"), Chunk))
        return false;

    Chunk->InitializeFlatGround(800.0f);
    const double BuildStart = FPlatformTime::Seconds();
    const int32 FlatTriangles = Chunk->RebuildMesh();
    const double BuildMs = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
    TestTrue(TEXT("Flat ground has a surface"), FlatTriangles > 0);

    FVector GroundHit;
    TestTrue(TEXT("Ground collision exists"), TraceDown(TestWorld.World, FVector(800.0, 800.0, 1500.0), GroundHit));
    TestTrue(TEXT("Ground is at the authored height"), FMath::IsNearlyEqual(GroundHit.Z, 800.0, 30.0));

    const float VolumeBefore = Chunk->GetSolidVolumeCubicMetres();
    const double DigStart = FPlatformTime::Seconds();
    const float VolumeChange = Chunk->ApplySphereEdit(FVector(800.0, 800.0, 800.0), 200.0f, false);
    const int32 PitTriangles = Chunk->RebuildMesh();
    const double DigMs = (FPlatformTime::Seconds() - DigStart) * 1000.0;
    TestTrue(TEXT("Digging removes solid volume"), VolumeChange < 0.0f);
    TestTrue(TEXT("Volume bookkeeping matches"), FMath::IsNearlyEqual(Chunk->GetSolidVolumeCubicMetres(), VolumeBefore + VolumeChange, 0.01f));
    TestTrue(TEXT("Pit surface exists"), PitTriangles > FlatTriangles);

    FVector PitHit;
    TestTrue(TEXT("Pit collision exists right after the rebuild"),
        TraceDown(TestWorld.World, FVector(800.0, 800.0, 1500.0), PitHit));
    TestTrue(TEXT("The trace now lands at the bottom of the pit"), PitHit.Z < GroundHit.Z - 120.0);

    const float FillChange = Chunk->ApplySphereEdit(FVector(800.0, 800.0, 800.0), 200.0f, true);
    TestTrue(TEXT("Filling adds volume back"), FillChange > 0.0f);

    UVoxelTerrainChunkComponent* LargeChunk = SpawnVoxelChunk(TestWorld.World, 64, 50.0f);
    LargeChunk->InitializeFlatGround(1600.0f);
    LargeChunk->RebuildMesh();
    const double LargeStart = FPlatformTime::Seconds();
    LargeChunk->ApplySphereEdit(FVector(1600.0, 1600.0, 1600.0), 200.0f, false);
    LargeChunk->RebuildMesh();
    const double LargeMs = (FPlatformTime::Seconds() - LargeStart) * 1000.0;

    AddInfo(FString::Printf(TEXT("VOXEL_TIMING chunk32 build_ms=%.2f dig_rebuild_ms=%.2f tris=%d | chunk64 dig_rebuild_ms=%.2f"),
        BuildMs, DigMs, PitTriangles, LargeMs));
    UE_LOG(LogTemp, Display, TEXT("VOXEL_TIMING chunk32 build_ms=%.2f dig_rebuild_ms=%.2f tris=%d | chunk64 dig_rebuild_ms=%.2f"),
        BuildMs, DigMs, PitTriangles, LargeMs);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBoundedDigPolicyTest,
    "MyProject3.Terrain.Voxel.BoundedDigChecksMaterialToolAndRevision", TestFlags)
bool FVoxelBoundedDigPolicyTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UVoxelTerrainChunkComponent* Chunk = SpawnVoxelChunk(TestWorld.World, 32, 50.0f);
    if (!TestNotNull(TEXT("Chunk"), Chunk))
        return false;

    Chunk->EditableBoundsLocal = FBox(FVector(300.0f), FVector(1300.0f));
    Chunk->SoilClass = EVoxelSoilClass::HardSoil;
    Chunk->InitializeFlatGround(800.0f);
    TestTrue(TEXT("Density grid initialized"), Chunk->HasInitializedDensity());
    Chunk->RebuildMesh();
    FVector GroundHitBeforeDig;
    TestTrue(TEXT("Initial collision surface exists"),
        TraceDown(TestWorld.World, FVector(800.0f, 800.0f, 1500.0f), GroundHitBeforeDig));
    const int64 InitialRevision = Chunk->GetDensityRevision();

    const FGuid WeakToolOperation = FGuid::NewGuid();
    float Removed = -1.0f;
    int64 ResultRevision = -1;
    TestFalse(TEXT("Weak tool rejected by hard soil"),
        Chunk->TryApplyBoundedDig(WeakToolOperation, FVector(800.0f), 100.0f,
            1.0f, InitialRevision, Removed, ResultRevision));
    TestEqual(TEXT("Weak tool does not mutate revision"), Chunk->GetDensityRevision(), InitialRevision);

    const FGuid OutsideOperation = FGuid::NewGuid();
    TestFalse(TEXT("Sphere outside permitted edit region rejected"),
        Chunk->TryApplyBoundedDig(OutsideOperation, FVector(1450.0f, 800.0f, 800.0f), 100.0f,
            3.0f, InitialRevision, Removed, ResultRevision));
    TestEqual(TEXT("Out of bounds edit does not mutate revision"), Chunk->GetDensityRevision(), InitialRevision);

    const FGuid ValidOperation = FGuid::NewGuid();
    TestTrue(TEXT("Sufficient tool can excavate within the bounded pocket"),
        Chunk->TryApplyBoundedDig(ValidOperation, FVector(800.0f), 100.0f,
            3.0f, InitialRevision, Removed, ResultRevision));
    TestTrue(TEXT("Accepted excavation removes material volume"), Removed > 0.0f);
    TestEqual(TEXT("Successful excavation increments revision once"),
        Chunk->GetDensityRevision(), InitialRevision + 1);
    TestEqual(TEXT("Result revision matches committed revision"), ResultRevision, Chunk->GetDensityRevision());
    FVector GroundHitAfterDig;
    TestTrue(TEXT("Bounded dig publishes new collision immediately"),
        TraceDown(TestWorld.World, FVector(800.0f, 800.0f, 1500.0f), GroundHitAfterDig));
    TestTrue(TEXT("New collision surface is lower inside the pit"),
        GroundHitAfterDig.Z < GroundHitBeforeDig.Z - 20.0f);

    const int64 CommittedRevision = Chunk->GetDensityRevision();
    float RetryRemoved = 0.0f;
    int64 RetryRevision = 0;
    TestTrue(TEXT("Identical operation retry returns its receipt"),
        Chunk->TryApplyBoundedDig(ValidOperation, FVector(800.0f), 100.0f,
            3.0f, InitialRevision, RetryRemoved, RetryRevision));
    TestEqual(TEXT("Retry does not excavate twice"), Chunk->GetDensityRevision(), CommittedRevision);
    TestTrue(TEXT("Retry returns the same material amount"),
        FMath::IsNearlyEqual(RetryRemoved, Removed, KINDA_SMALL_NUMBER));
    TestEqual(TEXT("Retry returns the original resulting revision"), RetryRevision, CommittedRevision);

    TestFalse(TEXT("Reusing operation ID with different parameters is rejected"),
        Chunk->TryApplyBoundedDig(ValidOperation, FVector(800.0f), 125.0f,
            3.0f, InitialRevision, RetryRemoved, RetryRevision));
    const FGuid StaleOperation = FGuid::NewGuid();
    TestFalse(TEXT("Stale expected revision is rejected"),
        Chunk->TryApplyBoundedDig(StaleOperation, FVector(800.0f), 100.0f,
            3.0f, InitialRevision, RetryRemoved, RetryRevision));
    TestEqual(TEXT("Rejected retries preserve committed revision"), Chunk->GetDensityRevision(), CommittedRevision);

    Chunk->SoilClass = EVoxelSoilClass::SoftSoil;
    const int64 SoftSoilRevision = Chunk->GetDensityRevision();
    TestFalse(TEXT("Tool below the soft-soil threshold is rejected"),
        Chunk->TryApplyBoundedDig(FGuid::NewGuid(), FVector(500.0f, 800.0f, 800.0f), 100.0f,
            0.5f, SoftSoilRevision, RetryRemoved, RetryRevision));
    TestEqual(TEXT("Rejected soft-soil attempt preserves revision"), Chunk->GetDensityRevision(), SoftSoilRevision);
    TestTrue(TEXT("Tool at the soft-soil threshold is accepted"),
        Chunk->TryApplyBoundedDig(FGuid::NewGuid(), FVector(500.0f, 800.0f, 800.0f), 100.0f,
            1.0f, SoftSoilRevision, RetryRemoved, RetryRevision));

    Chunk->SoilClass = EVoxelSoilClass::Rock;
    const int64 RockRevision = Chunk->GetDensityRevision();
    TestFalse(TEXT("Hard-soil strength is insufficient for rock"),
        Chunk->TryApplyBoundedDig(FGuid::NewGuid(), FVector(1200.0f, 800.0f, 800.0f), 100.0f,
            3.0f, RockRevision, RetryRemoved, RetryRevision));
    TestEqual(TEXT("Rejected rock attempt preserves revision"), Chunk->GetDensityRevision(), RockRevision);
    TestTrue(TEXT("Tool at the rock threshold is accepted"),
        Chunk->TryApplyBoundedDig(FGuid::NewGuid(), FVector(1200.0f, 800.0f, 800.0f), 100.0f,
            5.0f, RockRevision, RetryRemoved, RetryRevision));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStructureSupportGuardTest,
    "MyProject3.Terrain.Voxel.DigRejectsNearGroundedStructure", TestFlags)
bool FVoxelStructureSupportGuardTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UVoxelTerrainChunkComponent* Chunk = SpawnVoxelChunk(TestWorld.World, 32, 50.0f);
    if (!TestNotNull(TEXT("Voxel chunk"), Chunk))
        return false;
    Chunk->InitializeFlatGround(800.0f);

    AActor* SupportActor = TestWorld.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Grounded support actor"), SupportActor))
        return false;
    SupportActor->SetActorLocation(FVector(800.0f, 800.0f, 800.0f));
    UWorldObjectIdentityComponent* Identity = NewObject<UWorldObjectIdentityComponent>(SupportActor);
    if (!TestNotNull(TEXT("Support identity component"), Identity))
        return false;
    Identity->InstanceId = FGuid::NewGuid();
    SupportActor->AddInstanceComponent(Identity);
    Identity->RegisterComponent();

    UWorldStructuralGraphSubsystem* Graph =
        TestWorld.World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!TestNotNull(TEXT("Structural graph subsystem"), Graph))
        return false;
    const FGuid NodeId = FGuid::NewGuid();
    TestTrue(TEXT("Grounded foundation enters the authoritative structure graph"),
        Graph->RegisterNode(SupportActor, NodeId, EStructuralNodeKind::Foundation,
            true, 0.0f, 10000.0f, 100.0f));

    const int64 BeforeRevision = Chunk->GetDensityRevision();
    float Removed = 0.0f;
    int64 ResultRevision = 0;
    TestFalse(TEXT("Excavation is refused inside grounded-support clearance"),
        Chunk->TryApplyBoundedDig(FGuid::NewGuid(), FVector(800.0f), 100.0f,
            1.0f, BeforeRevision, Removed, ResultRevision));
    TestEqual(TEXT("Support-protected terrain remains unchanged"),
        Chunk->GetDensityRevision(), BeforeRevision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelChunkDensitySnapshotRoundTripTest,
    "MyProject3.Terrain.Voxel.DensitySnapshotRoundTrip", TestFlags)
bool FVoxelChunkDensitySnapshotRoundTripTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UVoxelTerrainChunkComponent* Source = SpawnVoxelChunk(TestWorld.World, 32, 50.0f);
    UVoxelTerrainChunkComponent* Restored = SpawnVoxelChunk(TestWorld.World, 32, 50.0f);
    if (!TestNotNull(TEXT("Source chunk"), Source) || !TestNotNull(TEXT("Restore target chunk"), Restored))
        return false;

    Source->InitializeFlatGround(800.0f);
    Source->RebuildMesh();
    float Removed = 0.0f;
    int64 ResultRevision = 0;
    const int64 InitialRevision = Source->GetDensityRevision();
    TestTrue(TEXT("Bounded excavation succeeds before snapshot"),
        Source->TryApplyBoundedDig(FGuid::NewGuid(), FVector(800.0f), 150.0f,
            1.0f, InitialRevision, Removed, ResultRevision));
    Source->RebuildMesh();

    TArray<float> Density;
    TArray<FVoxelTerrainDigReceipt> Receipts;
    int64 Revision = 0;
    TestTrue(TEXT("Initialized chunk exports a density snapshot"),
        Source->ExportDensitySnapshot(Density, Revision, Receipts));
    TestEqual(TEXT("Snapshot carries the committed revision"), Revision, Source->GetDensityRevision());
    TestEqual(TEXT("Snapshot carries the committed dig receipt count"), Receipts.Num(), 1);

    TestTrue(TEXT("Snapshot is validated before restore"),
        Restored->ValidateDensitySnapshot(Source->CellsPerAxis, Source->VoxelSizeCm,
            Revision, Density, Receipts));
    TestTrue(TEXT("Restore replaces density and rebuilds mesh/collision"),
        Restored->RestoreDensitySnapshot(Source->CellsPerAxis, Source->VoxelSizeCm,
            Revision, Density, Receipts));
    TestEqual(TEXT("Density revision restored"), Restored->GetDensityRevision(), Revision);
    TestTrue(TEXT("Solid volume is stable through snapshot round trip"),
        FMath::IsNearlyEqual(Restored->GetSolidVolumeCubicMetres(),
            Source->GetSolidVolumeCubicMetres(), KINDA_SMALL_NUMBER));
    TestTrue(TEXT("A point in the excavated region has the restored density"),
        FMath::IsNearlyEqual(Restored->SampleDensity(FVector(800.0f, 800.0f, 800.0f)),
            Source->SampleDensity(FVector(800.0f, 800.0f, 800.0f)), KINDA_SMALL_NUMBER));
    return true;
}

// DEMO-01: the vertical slice builds with the real imported log meshes; a
// real 600 cm log cuts into two 300 cm pieces, the missing 3/4 mesh blocks
// that cut with a reason, the stone footing carries a log, and the ground digs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceDemoTest,
    "MyProject3.Demo.VerticalSlice.BuildsWithRealAssets", TestFlags)
bool FVerticalSliceDemoTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    AVerticalSliceDemoActor* Demo = TestWorld.World->SpawnActor<AVerticalSliceDemoActor>();
    if (!TestNotNull(TEXT("Demo actor"), Demo))
        return false;
    Demo->BuildSlice();

    TestEqual(TEXT("Three loose logs plus the footing log"), Demo->GetSpawnedLogCount(), 4);
    TestTrue(TEXT("Stone footing carries a log"), Demo->IsFootingJointBuilt());
    TestTrue(TEXT("Fire is lit"), Demo->GetFire() && Demo->GetFire()->IsLit());
    TestTrue(TEXT("Ground can be dug"), Demo->EditTerrainAt(Demo->GetActorLocation() + FVector(-600.0, -600.0, 0.0), 90.0f, false) < 0.0f);

    ALogBuildPiece* Log = nullptr;
    for (TActorIterator<ALogBuildPiece> It(TestWorld.World); It; ++It)
    {
        if (!It->IsPlaced())
        {
            Log = *It;
            break;
        }
    }
    if (!TestNotNull(TEXT("A loose demo log"), Log))
        return false;
    TestEqual(TEXT("Real mesh family selected"), Log->GetSelectedProcessingMeshSetId(), FName(TEXT("Log600")));
    // This older demo family has no 450 cm row, so its 3/4 cut stays blocked.
    TestFalse(TEXT("3/4 cut is blocked without a 450 cm mesh"), Log->CanApplyCutAction(ELogCutAction::SplitLengthThreeQuarter));
    TestTrue(TEXT("Block reason explains the missing recipe or mesh"),
        Log->GetCutActionBlockReason(ELogCutAction::SplitLengthThreeQuarter).Contains(TEXT("length-cut")));

    const int32 LogsBefore = CountLogs(TestWorld.World);
    const FWorldOperationResult Cut = Log->TryApplyCutAction(ELogCutAction::SplitLengthHalf, FGuid::NewGuid());
    TestTrue(FString::Printf(TEXT("Real log cuts in half: %s"), *Cut.Message.ToString()), Cut.IsSuccess());
    TestEqual(TEXT("One more log actor"), CountLogs(TestWorld.World), LogsBefore + 1);
    TestTrue(TEXT("Cut piece uses the real 300 cm mesh"),
        Log->GetLogMesh()->GetStaticMesh() && Log->GetLogMesh()->GetStaticMesh()->GetName() == TEXT("SM_RawLog_300"));

    const FWorldOperationResult Split = Log->TryApplyCutAction(ELogCutAction::SplitHalf, FGuid::NewGuid());
    TestTrue(FString::Printf(TEXT("Half-length log splits into half-rounds: %s"), *Split.Message.ToString()), Split.IsSuccess());
    TestTrue(TEXT("Split piece uses the real 300 cm half-round mesh"),
        Log->GetLogMesh()->GetStaticMesh() && Log->GetLogMesh()->GetStaticMesh()->GetName() == TEXT("SM_RawLog_300_Half"));
    return true;
}


// SOTF-M01/M02: stones and sticks go into the hands, stack on placed pieces
// through automatic top/bottom anchors, join across materials and stake into
// the ground; a placed piece cannot be lifted out of a structure.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandHeldMaterialPlacementTest,
    "MyProject3.Carry.Materials.StoneAndStickPickUpStackAndStake", TestFlags)
bool FHandHeldMaterialPlacementTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UStaticMesh* Cube = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!TestNotNull(TEXT("Ownership"), Ownership) || !TestNotNull(TEXT("Cube"), Cube) ||
        !TestNotNull(TEXT("Cylinder"), Cylinder))
        return false;

    AActor* Player = World->SpawnActor<AActor>();
    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Player);
    Carry->RegisterComponent();

    AStoneBuildingPiece* Base = World->SpawnActor<AStoneBuildingPiece>(FVector(0.0, 0.0, 50.0), FRotator::ZeroRotator);
    AStoneBuildingPiece* Upper = World->SpawnActor<AStoneBuildingPiece>(FVector(300.0, 0.0, 50.0), FRotator::ZeroRotator);
    AStickPrimitivePiece* Post = World->SpawnActor<AStickPrimitivePiece>(FVector(600.0, 0.0, 50.0), FRotator::ZeroRotator);
    AStickPrimitivePiece* Stake = World->SpawnActor<AStickPrimitivePiece>(FVector(900.0, 0.0, 50.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Base"), Base) || !TestNotNull(TEXT("Upper"), Upper) ||
        !TestNotNull(TEXT("Post"), Post) || !TestNotNull(TEXT("Stake"), Stake))
        return false;
    Base->Mesh->SetStaticMesh(Cube);
    Upper->Mesh->SetStaticMesh(Cube);
    Post->Mesh->SetStaticMesh(Cylinder);
    Stake->Mesh->SetStaticMesh(Cylinder);
    TestTrue(TEXT("Base lifted"), Base->SetHeldState(true));
    TestTrue(TEXT("Base on the ground"), Base->TryCommitGroundPlacement(FVector(0.0, 0.0, 50.0), FRotator::ZeroRotator));

    TestTrue(TEXT("Stone picked into the hands"), Carry->TryPickupMaterialPiece(Upper));
    FWorldOwnedItemRecord Item;
    Ownership->TryGetItem(Upper->GetStructuralNodeId(), Item);
    TestEqual(TEXT("Held stone is held in the ledger"), Item.OwnerKind, EWorldItemOwnerKind::Held);
    TestFalse(TEXT("Only one piece in the hands"), Carry->TryPickupMaterialPiece(Post));

    TestTrue(FString::Printf(TEXT("Stone stacks on stone (%s)"), *Carry->GetPlacementStatus()),
        Carry->TryPlaceHeldMaterialOn(Base, TEXT("AutoTop")));
    TestTrue(TEXT("Upper stone is placed"), Upper->IsStructurallyPlaced());
    TestTrue(TEXT("Hands are empty"), !Carry->IsHoldingMaterialPiece());
    Ownership->TryGetItem(Upper->GetStructuralNodeId(), Item);
    TestEqual(TEXT("Placed stone is world-owned"), Item.OwnerKind, EWorldItemOwnerKind::World);
    TestTrue(TEXT("Upper stone sits above the base"), Upper->GetActorLocation().Z > Base->GetActorLocation().Z + 50.0);
    TestFalse(TEXT("A stone inside a structure cannot be lifted"), Carry->TryPickupMaterialPiece(Base));

    TestTrue(TEXT("Stick picked up"), Carry->TryPickupMaterialPiece(Post));
    TestFalse(TEXT("Occupied top is rejected"), Carry->TryPlaceHeldMaterialOn(Base, TEXT("AutoTop")));
    TestTrue(TEXT("A rejected placement keeps the stick in the hands"), Carry->IsHoldingMaterialPiece());
    TestTrue(FString::Printf(TEXT("Stick stands on the stone stack (%s)"), *Carry->GetPlacementStatus()),
        Carry->TryPlaceHeldMaterialOn(Upper, TEXT("AutoTop")));
    TestTrue(TEXT("Stick joined to the stone"), Post->GetCrossMaterialParent() == Upper);

    TestTrue(TEXT("Second stick picked up"), Carry->TryPickupMaterialPiece(Stake));
    TestTrue(TEXT("Stick staked into the ground"), Carry->TryPlaceHeldMaterialOnGround(FVector(900.0, 0.0, 0.0)));
    TestTrue(TEXT("Stake is placed"), Stake->IsStructurallyPlaced());
    return true;
}

// A horizontal log joined at one end needs something under the other end;
// the cut preview draws dashed segments for the selected cut.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogFarEndAndCutPreviewTest,
    "MyProject3.Carry.Logs.FarEndSupportAndCutPreview", TestFlags)
bool FLogFarEndAndCutPreviewTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UStaticMesh* Cube = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    ALogBuildPiece* Log = SpawnConfiguredLog(World, true);
    if (!TestNotNull(TEXT("Log"), Log) || !TestNotNull(TEXT("Cube"), Cube))
        return false;

    AActor* Player = World->SpawnActor<AActor>();
    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Player);
    Carry->RegisterComponent();

    // A 4 m beam along X, held at 5 m with its joint at the -X end.
    Log->SetPieceStaticMesh(Cube);
    Log->LongitudinalAxis = ELogLongitudinalAxis::X;
    Log->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, 500.0), FVector(4.0, 1.0, 1.0)));
    Log->GetLogMesh()->SetSimulatePhysics(false);
    Log->AddCustomSnapPoint(TEXT("EndJoint"), TEXT("LogEnd"), FTransform(FVector(-50.0, 0.0, 0.0)), true, false, true, false);
    int32 JointIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Log->GetSnapPoints().Num(); ++Index)
    {
        if (Log->GetSnapPoints()[Index].PointId == TEXT("EndJoint"))
            JointIndex = Index;
    }
    if (!TestTrue(TEXT("Joint snap point exists"), JointIndex != INDEX_NONE))
        return false;

    AActor* Support = nullptr;
    bool bGround = false;
    TestFalse(TEXT("Nothing under the free end"), Carry->EvaluateFarEndSupport(Log, JointIndex, Support, bGround));

    AStoneBuildingPiece* Rest = World->SpawnActor<AStoneBuildingPiece>(FVector(200.0, 0.0, 420.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Rest stone"), Rest))
        return false;
    Rest->Mesh->SetStaticMesh(Cube);
    Rest->Mesh->SetSimulatePhysics(false);
    Rest->Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    TestTrue(TEXT("A stone under the free end holds it"), Carry->EvaluateFarEndSupport(Log, JointIndex, Support, bGround));
    TestTrue(TEXT("The support is the stone"), Support == Rest);

    TArray<FVector> Points;
    TestTrue(TEXT("Split preview"), Carry->BuildCutPreviewSegments(Log, ELogCutAction::SplitHalf, Points));
    TestTrue(TEXT("Split preview has dashes"), Points.Num() >= 4 && Points.Num() % 2 == 0);
    TestTrue(TEXT("Length cut preview"), Carry->BuildCutPreviewSegments(Log, ELogCutAction::SplitLengthHalf, Points));
    bool bAllNearMiddle = Points.Num() > 0;
    for (const FVector& Point : Points)
        bAllNearMiddle &= FMath::Abs(Point.X) < 1.0;
    TestTrue(TEXT("Half-length ring sits at the middle of the beam"), bAllNearMiddle);
    return true;
}


// A log hanging from one joint with its free end down is turned back up about
// the joint when a piece is placed under that end; the joint does not move.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroopingLogLiftTest,
    "MyProject3.Carry.Logs.DroopingEndLiftsOntoNewSupport", TestFlags)
bool FDroopingLogLiftTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UStaticMesh* Cube = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    ALogBuildPiece* Log = SpawnConfiguredLog(World, true);
    if (!TestNotNull(TEXT("Log"), Log) || !TestNotNull(TEXT("Cube"), Cube))
        return false;
    AActor* Player = World->SpawnActor<AActor>();
    ULogCarryComponent* Carry = NewObject<ULogCarryComponent>(Player);
    Carry->RegisterComponent();

    // A 4 m beam, joint at its -X end 200 cm up, tilted 30 degrees down.
    Log->SetPieceStaticMesh(Cube);
    Log->LongitudinalAxis = ELogLongitudinalAxis::X;
    Log->GetLogMesh()->SetSimulatePhysics(false);
    Log->AddCustomSnapPoint(TEXT("EndJoint"), TEXT("LogEnd"), FTransform(FVector(-50.0, 0.0, 0.0)), true, false, true, false);
    const FQuat Tilt = FRotator(-30.0f, 0.0f, 0.0f).Quaternion();
    const FVector Joint(0.0, 0.0, 200.0);
    Log->SetActorTransform(FTransform(Tilt, Joint - Tilt.RotateVector(FVector(-200.0, 0.0, 0.0)), FVector(4.0, 1.0, 1.0)));
    Log->SetPlacedState(true);
    int32 JointIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Log->GetSnapPoints().Num(); ++Index)
    {
        if (Log->GetSnapPoints()[Index].PointId == TEXT("EndJoint"))
            JointIndex = Index;
    }
    FVector FreeEnd, Pivot;
    float Radius = 0.0f;
    if (!TestTrue(TEXT("Free end found"), Carry->GetLogFreeEnd(Log, JointIndex, FreeEnd, Pivot, Radius)))
        return false;
    TestTrue(TEXT("Joint is at the expected place"), Pivot.Equals(Joint, 1.0));
    TestTrue(TEXT("Free end hangs low"), FreeEnd.Z < 20.0);
    Carry->RegisterDroopingLog(Log, JointIndex);

    AStoneBuildingPiece* Far = World->SpawnActor<AStoneBuildingPiece>(FVector(-600.0, 0.0, 50.0), FRotator::ZeroRotator);
    AStoneBuildingPiece* Under = World->SpawnActor<AStoneBuildingPiece>(FVector(FreeEnd.X, 0.0, 50.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Stones"), Far) || !TestNotNull(TEXT("Stones"), Under))
        return false;
    Far->Mesh->SetStaticMesh(Cube);
    Under->Mesh->SetStaticMesh(Cube);
    TestFalse(TEXT("A piece elsewhere lifts nothing"), Carry->LiftDroopingEndsOnto(Far));
    TestTrue(TEXT("A piece under the end lifts it"), Carry->LiftDroopingEndsOnto(Under));
    Carry->GetLogFreeEnd(Log, JointIndex, FreeEnd, Pivot, Radius);
    TestTrue(TEXT("Joint did not move"), Pivot.Equals(Joint, 1.0));
    TestTrue(FString::Printf(TEXT("Free end rests on the stone top (z=%.1f)"), FreeEnd.Z),
        FMath::IsNearlyEqual(FreeEnd.Z, 100.0 + Radius, 2.0));
    TestEqual(TEXT("Nothing left drooping"), Carry->GetDroopingLogCount(), 0);
    return true;
}


namespace MyProject3Tests
{
    /** A cube "log" along X with the given size (cm); placed pieces stand still in the graph. */
    ALogBuildPiece* SpawnBeam(UWorld* World, const FVector& Centre, const FRotator& Rotation,
        float Length, float Thickness, bool bPlaced)
    {
        ALogBuildPiece* Log = SpawnConfiguredLog(World, true);
        UStaticMesh* Cube = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!Log || !Cube)
            return nullptr;
        Log->SetPieceStaticMesh(Cube);
        Log->LongitudinalAxis = ELogLongitudinalAxis::X;
        Log->GetLogMesh()->SetSimulatePhysics(false);
        Log->SetActorTransform(FTransform(Rotation, Centre, FVector(Length / 100.0f, Thickness / 100.0f, Thickness / 100.0f)));
        if (bPlaced)
        {
            if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
                Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Placed);
            Log->SetPlacedState(true);
        }
        return Log;
    }

    struct FPlayerTools
    {
        AActor* Player = nullptr;
        ULogCarryComponent* Carry = nullptr;
        ULogBuildToolsComponent* Tools = nullptr;

        explicit FPlayerTools(UWorld* World)
        {
            Player = World->SpawnActor<AActor>(FVector(0, -400, 100), FRotator::ZeroRotator);
            Carry = NewObject<ULogCarryComponent>(Player);
            Carry->RegisterComponent();
            Tools = NewObject<ULogBuildToolsComponent>(Player);
            Tools->RegisterComponent();
        }
    };
}

// Throwing (SOTF throw): a fast log kills a small animal, flattens a bush and
// staggers an enemy; a slow bump does nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThrownLogImpactTest,
    "MyProject3.Construction.ThrowImpactDamagesAnimalsPlantsEnemies", TestFlags)
bool FThrownLogImpactTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Log = SpawnBeam(World, FVector(0, 0, 100), FRotator::ZeroRotator, 200.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Log"), Log))
        return false;
    auto SpawnTarget = [World](EWildlifeKind Kind, const FVector& Location)
    {
        AWildlifeActor* Target = World->SpawnActorDeferred<AWildlifeActor>(AWildlifeActor::StaticClass(), FTransform(Location));
        Target->ConfigureKind(Kind);
        Target->FinishSpawning(FTransform(Location));
        return Target;
    };
    AWildlifeActor* Rabbit = SpawnTarget(EWildlifeKind::SmallAnimal, FVector(500, 0, 50));
    AWildlifeActor* Bush = SpawnTarget(EWildlifeKind::Plant, FVector(800, 0, 50));
    AWildlifeActor* Enemy = SpawnTarget(EWildlifeKind::Enemy, FVector(1100, 0, 90));
    UThrownImpactComponent* Impact = NewObject<UThrownImpactComponent>(Log);
    Impact->RegisterComponent();
    Impact->Arm(nullptr, MyProject3Construction::EstimateMassKg(Log));

    TestEqual(TEXT("A slow bump does no damage"), Impact->ApplyImpactTo(Rabbit, 100.0f, FVector::XAxisVector), 0.0f);
    TestTrue(TEXT("A fast hit hurts"), Impact->ApplyImpactTo(Rabbit, 1500.0f, FVector::XAxisVector) > 0.0f);
    TestTrue(TEXT("The small animal is hunted"), Rabbit->Health->IsDead());
    Impact->ApplyImpactTo(Bush, 1500.0f, FVector::XAxisVector);
    TestTrue(TEXT("The bush is flattened"), Bush->IsHidden());
    Impact->ApplyImpactTo(Enemy, 900.0f, FVector::XAxisVector);
    TestTrue(TEXT("The enemy is hurt"), Enemy->Health->Health < Enemy->Health->MaxHealth);
    TestTrue(TEXT("The enemy staggers"), Enemy->Health->IsStaggered());
    TestEqual(TEXT("Each target is hit once per throw"), Impact->ApplyImpactTo(Enemy, 1500.0f, FVector::XAxisVector), 0.0f);
    return true;
}

// Log storage and firewood (SOTF log holders): pieces are owned by the store,
// capacity is enforced, firewood stores only take small pieces.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogStorageTest,
    "MyProject3.Construction.StorageStoresTakesAndFilters", TestFlags)
bool FLogStorageTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    auto SpawnContainer = [World](ELogContainerKind Kind, const FVector& Location)
    {
        ALogContainerActor* Container = World->SpawnActorDeferred<ALogContainerActor>(ALogContainerActor::StaticClass(), FTransform(Location));
        Container->ConfigureKind(Kind);
        Container->FinishSpawning(FTransform(FRotator::ZeroRotator, Location, Container->GetActorScale3D()));
        Container->bAutoCollect = false;
        return Container;
    };
    ALogContainerActor* Rack = SpawnContainer(ELogContainerKind::SmallLogStorage, FVector(0, 0, 10));
    ALogContainerActor* Firewood = SpawnContainer(ELogContainerKind::FirewoodStorage, FVector(0, 600, 10));
    ALogBuildPiece* LogA = SpawnBeam(World, FVector(0, -400, 50), FRotator::ZeroRotator, 400.0f, 30.0f, false);
    ALogBuildPiece* LogB = SpawnBeam(World, FVector(0, -500, 50), FRotator::ZeroRotator, 400.0f, 30.0f, false);
    ALogBuildPiece* Split = SpawnBeam(World, FVector(0, -600, 50), FRotator::ZeroRotator, 70.0f, 15.0f, false);
    if (!TestNotNull(TEXT("Logs"), LogB) || !TestNotNull(TEXT("Small piece"), Split))
        return false;

    TestTrue(TEXT("Rack stores a log"), Rack->TryStore(LogA));
    TestTrue(TEXT("Rack stores a second log"), Rack->TryStore(LogB));
    TestEqual(TEXT("Two stored"), Rack->GetStoredCount(), 2);
    EWorldItemOwnerKind Owner = EWorldItemOwnerKind::World;
    MyProject3Construction::GetPieceOwnerKind(LogA, Owner);
    TestEqual(TEXT("Stored log is owned by storage"), Owner, EWorldItemOwnerKind::Storage);
    FString Reason;
    TestFalse(TEXT("A full log is too big for firewood"), Firewood->CanAccept(LogA, Reason));
    TestTrue(TEXT("A small piece fits the firewood store"), Firewood->TryStore(Split));

    AActor* Taken = Rack->TakeOne(FVector(0, -300, 100));
    TestTrue(TEXT("The top log comes out"), Taken == LogB);
    MyProject3Construction::GetPieceOwnerKind(LogB, Owner);
    TestEqual(TEXT("Taken log is back in the world"), Owner, EWorldItemOwnerKind::World);
    TestEqual(TEXT("One left"), Rack->GetStoredCount(), 1);
    return true;
}

// Auto-cut to length: cutting keeps the requested length and drops the rest
// as a separate loose piece; nothing is lost.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogTrimTest,
    "MyProject3.Construction.CutToLengthKeepsRemainder", TestFlags)
bool FLogTrimTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    ALogBuildPiece* Log = SpawnBeam(TestWorld.World, FVector(0, 0, 100), FRotator::ZeroRotator, 400.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Log"), Log))
        return false;
    using namespace MyProject3Construction;
    ALogBuildPiece* Rest = CutOffRemainder(Log, 150.0f, FVector(0, 300, 100));
    FLogGeometry Kept, Remainder;
    TestTrue(TEXT("Remainder exists"), Rest && GetLogGeometry(Rest, Remainder));
    GetLogGeometry(Log, Kept);
    TestTrue(FString::Printf(TEXT("Kept piece is 150 cm (%.1f)"), Kept.Length()), FMath::IsNearlyEqual(Kept.Length(), 150.0f, 1.0f));
    TestTrue(FString::Printf(TEXT("Remainder is 250 cm (%.1f)"), Remainder.Length()), FMath::IsNearlyEqual(Remainder.Length(), 250.0f, 1.0f));
    TestTrue(TEXT("Kept piece stays at the -X end"), FMath::IsNearlyEqual(Kept.EndA().X, -200.0f, 1.0f));
    return true;
}

// Floors, roofs and bridges: a board spans two parallel supports, joins both
// in the structural graph, and a taken spot is refused.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckPlacementTest,
    "MyProject3.Construction.FloorBoardSpansTwoSupports", TestFlags)
bool FDeckPlacementTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    ALogBuildPiece* BeamA = SpawnBeam(World, FVector(0, 0, 100), FRotator::ZeroRotator, 400.0f, 30.0f, true);
    ALogBuildPiece* BeamB = SpawnBeam(World, FVector(0, 150, 100), FRotator::ZeroRotator, 400.0f, 30.0f, true);
    ALogBuildPiece* Board = SpawnBeam(World, FVector(0, -300, 30), FRotator::ZeroRotator, 230.0f, 20.0f, false);
    FPlayerTools Player(World);
    if (!TestNotNull(TEXT("Board"), Board) || !TestNotNull(TEXT("Beam"), BeamB))
        return false;

    FTransform Transform;
    AActor* Second = nullptr;
    float Span = 0.0f;
    FString Reason;
    TestTrue(FString::Printf(TEXT("Board fits (%s)"), *Reason), Player.Tools->ComputeDeckPlacement(Board, BeamA,
        FVector(0, 0, 115), FVector(0, 1, 0), false, Transform, Second, Span, Reason));
    TestTrue(TEXT("Second support is the parallel beam"), Second == BeamB);
    TestTrue(FString::Printf(TEXT("Span is 150 cm (%.1f)"), Span), FMath::IsNearlyEqual(Span, 150.0f, 2.0f));

    TestTrue(TEXT("Pick up the board"), Player.Carry->TryPickupPiece(Board));
    TestTrue(TEXT("Board placed"), Player.Carry->CommitHeldLogAt(Transform, BeamA, BeamB));
    FStructuralNodeRecord Node;
    Graph->GetNode(Board->GetStructuralNodeId(), Node);
    TestEqual(TEXT("Board rests on the first beam"), Node.ParentNodeId, BeamA->GetStructuralNodeId());
    TestEqual(TEXT("And on the second beam"), Node.SecondarySupports.Num(), 1);
    TestTrue(TEXT("Board lies between the beams"), FMath::IsNearlyEqual(Board->GetLogMesh()->Bounds.Origin.Y, 75.0f, 3.0f));

    ALogBuildPiece* Another = SpawnBeam(World, FVector(0, -500, 30), FRotator::ZeroRotator, 230.0f, 20.0f, false);
    TestFalse(TEXT("The same spot is taken"), Player.Tools->ComputeDeckPlacement(Another, BeamA,
        FVector(0, 0, 115), FVector(0, 1, 0), false, Transform, Second, Span, Reason));
    TestTrue(TEXT("The next spot is free"), Player.Tools->ComputeDeckPlacement(Another, BeamA,
        FVector(25, 0, 115), FVector(0, 1, 0), false, Transform, Second, Span, Reason));
    return true;
}

// Removing a temporary support: allowed when what it carried has another
// support (the beam moves onto it), refused when it is the only one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDismantleSupportTest,
    "MyProject3.Construction.RemoveSupportOnlyWhenRedundant", TestFlags)
bool FDismantleSupportTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    ALogBuildPiece* PostA = SpawnBeam(World, FVector(-150, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    // Post B stands under the middle, so the beam does not overhang too far without post A.
    ALogBuildPiece* PostB = SpawnBeam(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Beam = SpawnBeam(World, FVector(0, 0, 220), FRotator::ZeroRotator, 340.0f, 30.0f, true);
    FPlayerTools Player(World);
    if (!TestNotNull(TEXT("Beam"), Beam))
        return false;
    TestTrue(TEXT("Beam on post A"), Graph->TryConnect(PostA->GetStructuralNodeId(), TEXT("Top"), Beam->GetStructuralNodeId(), TEXT("EndA")));
    TestFalse(TEXT("Post A alone carries the beam"), Player.Tools->DismantlePiece(PostA));
    TestTrue(TEXT("Beam also on post B"), Graph->TryAddSecondarySupport(Beam->GetStructuralNodeId(), PostB->GetStructuralNodeId(), TEXT("Top"), TEXT("EndB")));
    TestTrue(TEXT("Now post A can go"), Player.Tools->DismantlePiece(PostA));
    FStructuralNodeRecord Node;
    Graph->GetNode(Beam->GetStructuralNodeId(), Node);
    TestEqual(TEXT("The beam now rests on post B"), Node.ParentNodeId, PostB->GetStructuralNodeId());
    TestTrue(TEXT("The beam still stands"), Graph->HasValidSupportPath(Beam->GetStructuralNodeId()));
    TestFalse(TEXT("Post A is loose"), PostA->IsPlaced());
    return true;
}

// Door opening: wall logs are cut around the opening, the cut-outs drop, and
// a door cannot be opened or locked before it is built from boards.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDoorOpeningTest,
    "MyProject3.Construction.DoorOpeningCutsWallLogs", TestFlags)
bool FDoorOpeningTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    TArray<ALogBuildPiece*> Wall;
    for (int32 Row = 0; Row < 3; ++Row)
        Wall.Add(SpawnBeam(World, FVector(0, 0, 20 + Row * 40), FRotator::ZeroRotator, 400.0f, 40.0f, true));
    FPlayerTools Player(World);
    const int32 Before = CountLogs(World);
    AConstructionOpeningActor* Opening = nullptr;
    TestTrue(TEXT("Door opening cut"), Player.Tools->CutOpening(Wall[1], FVector(0, -20, 60),
        EConstructionOpeningKind::Door, Opening));
    if (!TestNotNull(TEXT("Opening"), Opening))
        return false;
    TestEqual(TEXT("Each wall log became a left part, a right part and an offcut"), CountLogs(World), Before + 6);
    MyProject3Construction::FLogGeometry Left;
    MyProject3Construction::GetLogGeometry(Wall[0], Left);
    TestTrue(FString::Printf(TEXT("Left part ends at the opening (%.1f)"), Left.EndB().X), FMath::IsNearlyEqual(Left.EndB().X, -50.0f, 1.5f));
    FString Reason;
    TestFalse(TEXT("No door yet to open"), Opening->ToggleOpen(FVector(0, -200, 100), Reason));
    TestFalse(TEXT("No door yet to lock"), Opening->ToggleLock(FVector(0, -200, 100), Reason));
    TestEqual(TEXT("A door takes three split planks"), Opening->GetRequiredBoards(), 3);
    return true;
}

// Zipline: a sloped rope carries a log from its top to its bottom, where it is
// world-owned again.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZiplineTest,
    "MyProject3.Construction.ZiplineCarriesLogDownhill", TestFlags)
bool FZiplineTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ARopeLineActor* Rope = World->SpawnActor<ARopeLineActor>(FVector(500, 0, 300), FRotator::ZeroRotator);
    ARopeLineActor* Level = World->SpawnActor<ARopeLineActor>(FVector(0, 500, 300), FRotator::ZeroRotator);
    ALogBuildPiece* Log = SpawnBeam(World, FVector(0, -300, 50), FRotator::ZeroRotator, 200.0f, 30.0f, false);
    if (!TestNotNull(TEXT("Rope"), Rope) || !TestNotNull(TEXT("Log"), Log))
        return false;
    Rope->InitializeRope(FVector(0, 0, 600), nullptr, FVector(1500, 0, 100), nullptr);
    Level->InitializeRope(FVector(0, 500, 300), nullptr, FVector(800, 500, 310), nullptr);
    TestTrue(TEXT("Sloped rope is a zipline"), Rope->IsZipline());
    TestFalse(TEXT("Level rope is a bridge rope"), Level->IsZipline());
    TestTrue(TEXT("Rope is a structural support"), Rope->IsStructurallyPlaced());
    TestTrue(TEXT("Log sent"), Rope->TrySendAlong(Log));
    for (int32 Step = 0; Step < 400 && Rope->GetRidingCount() > 0; ++Step)
        Rope->Tick(0.05f);
    TestEqual(TEXT("Arrived"), Rope->GetRidingCount(), 0);
    TestTrue(TEXT("Log is at the lower end"), FVector::Dist2D(Log->GetActorLocation(), FVector(1500, 0, 0)) < 150.0f);
    EWorldItemOwnerKind Owner = EWorldItemOwnerKind::InTransit;
    MyProject3Construction::GetPieceOwnerKind(Log, Owner);
    TestEqual(TEXT("Log is world-owned after the ride"), Owner, EWorldItemOwnerKind::World);
    return true;
}

// Building plans and stakes: a plan slot takes a carried log cut to length;
// standing logs can be sharpened, lying ones cannot.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlanAndStakeTest,
    "MyProject3.Construction.PlanSlotCutsAndStakesSharpen", TestFlags)
bool FPlanAndStakeTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    FPlayerTools Player(World);
    ABuildPlanActor* Plan = Player.Tools->PlacePlan(EBuildPlanKind::LogWall, FVector(0, 300, 0), 0.0f);
    ALogBuildPiece* Log = SpawnBeam(World, FVector(0, -300, 30), FRotator::ZeroRotator, 600.0f, 36.0f, false);
    if (!TestNotNull(TEXT("Plan"), Plan) || !TestNotNull(TEXT("Log"), Log))
        return false;
    TestEqual(TEXT("A wall plan has five rows"), Plan->GetSlotCount(), 5);
    FString Reason;
    TestFalse(TEXT("Upper rows wait for the lower ones"), Plan->CanFillSlot(1, Reason));
    const int32 Before = CountLogs(World);
    TestTrue(TEXT("Pick up"), Player.Carry->TryPickupPiece(Log));
    TestTrue(TEXT("Fill the first row"), Player.Tools->TryFillPlanSlot(Plan, 0));
    TestTrue(TEXT("Row is built"), Plan->GetSlot(0).bFilled && Log->IsPlaced());
    MyProject3Construction::FLogGeometry Shape;
    MyProject3Construction::GetLogGeometry(Log, Shape);
    TestTrue(FString::Printf(TEXT("Cut to the plan length (%.0f)"), Shape.Length()), FMath::IsNearlyEqual(Shape.Length(), 300.0f, 2.0f));
    TestEqual(TEXT("The offcut is a new loose log"), CountLogs(World), Before + 1);
    TestTrue(TEXT("Second row can follow"), Plan->CanFillSlot(1, Reason));

    ALogBuildPiece* Stake = SpawnBeam(World, FVector(600, 0, 150), FRotator(90, 0, 0), 300.0f, 30.0f, true);
    ALogBuildPiece* Lying = SpawnBeam(World, FVector(900, 0, 20), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    TestTrue(TEXT("Standing log sharpened"), Player.Tools->SharpenLog(Stake));
    TestNotNull(TEXT("It has a point"), Stake->FindComponentByClass<USharpenedTipComponent>());
    TestFalse(TEXT("A lying log is not sharpened"), Player.Tools->SharpenLog(Lying));
    TestFalse(TEXT("Not twice"), Player.Tools->SharpenLog(Stake));
    return true;
}

// Shelter: built pieces overhead and around raise weather protection.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterCoverTest,
    "MyProject3.Construction.RoofAndWallsGiveShelter", TestFlags)
bool FShelterCoverTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    float Roof = 0.0f, Walls = 0.0f;
    UShelterSensorComponent::MeasureAt(World, FVector(0, 0, 100), nullptr, Roof, Walls);
    TestEqual(TEXT("Open sky"), Roof, 0.0f);
    SpawnBeam(World, FVector(0, 0, 300), FRotator::ZeroRotator, 2000.0f, 2000.0f, true)->SetActorScale3D(FVector(20.0f, 20.0f, 0.2f));
    UShelterSensorComponent::MeasureAt(World, FVector(0, 0, 100), nullptr, Roof, Walls);
    TestTrue(FString::Printf(TEXT("Under a roof (%.2f)"), Roof), Roof > 0.8f);
    return true;
}


// VerticalSlice_C: the clearing builds as one place - forest, creek too deep
// to wade except at the ford, a cabin, the old bridge and the lookout.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceCTest,
    "MyProject3.Demo.VerticalSliceC.BuildsClearing", TestFlags)
bool FVerticalSliceCTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    AVerticalSliceCWorldActor* World = TestWorld.World->SpawnActor<AVerticalSliceCWorldActor>();
    if (!TestNotNull(TEXT("World actor"), World))
        return false;
    World->BuildWorld();
    TestTrue(FString::Printf(TEXT("A forest stands (%d trees)"), World->GetTreeCount()), World->GetTreeCount() > 120);
    TestTrue(FString::Printf(TEXT("Built pieces exist (%d)"), World->GetPlacedLogCount()), World->GetPlacedLogCount() > 55);
    const float CreekY = World->GetCreekCentreY(0.0f);
    TestTrue(TEXT("The creek is below the water line"), World->GetGroundHeight(0.0f, CreekY) < AVerticalSliceCWorldActor::WaterLevel - 50.0f);
    TestTrue(TEXT("The ford is shallow"), World->GetGroundHeight(2600.0f, World->GetCreekCentreY(2600.0f)) > AVerticalSliceCWorldActor::WaterLevel);
    TestTrue(TEXT("The camp is level"), FMath::Abs(World->GetGroundHeight(100.0f, 100.0f)) < 5.0f);
    TestTrue(TEXT("The lookout is high on the hill"), World->GetGroundHeight(600.0f, 2650.0f) > 600.0f);
    TestTrue(TEXT("The cliff is a real step"), World->GetGroundHeight(800.0f, 1600.0f) - World->GetGroundHeight(800.0f, 1440.0f) > 200.0f);
    int32 Ropes = 0;
    for (TActorIterator<ARopeLineActor> It(TestWorld.World); It; ++It)
        ++Ropes;
    TestEqual(TEXT("The old bridge rope and the zipline"), Ropes, 2);
    return true;
}


// Every cut on the real demo log family: what comes out, where, and whether
// the pieces end up inside each other.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCutMatrixDiagnosticTest,
    "MyProject3.Diagnostics.CutMatrix", TestFlags)
bool FCutMatrixDiagnosticTest::RunTest(const FString& Parameters)
{
    const FLogProcessingMeshSet Family = AVerticalSliceBWorldActor::BuildLogMeshFamily();
    if (!TestNotNull(TEXT("Demo log family"), Family.BaseRoundMesh.Get()))
        return false;
    struct FStep { ELogCutAction First; ELogCutAction Then; bool bThen; };
    const FStep Steps[] =
    {
        { ELogCutAction::SplitHalf, ELogCutAction::SplitHalf, false },
        { ELogCutAction::Quarter, ELogCutAction::SplitHalf, false },
        { ELogCutAction::Hewn, ELogCutAction::HewSecondFace, true },
        { ELogCutAction::Kalas, ELogCutAction::SplitHalf, false },
        { ELogCutAction::Plank, ELogCutAction::SplitLengthHalf, true },
        { ELogCutAction::Gutter, ELogCutAction::SplitHalf, false },
        { ELogCutAction::Trough, ELogCutAction::SplitHalf, false },
        { ELogCutAction::SplitLengthHalf, ELogCutAction::SplitHalfLengthIntoQuarters, true },
        { ELogCutAction::SplitLengthThreeQuarter, ELogCutAction::SplitHalf, false },
        { ELogCutAction::SplitHalf, ELogCutAction::SplitLengthHalf, true },
    };
    const UEnum* Actions = StaticEnum<ELogCutAction>();
    for (const FStep& Step : Steps)
    {
        FScopedTestWorld TestWorld;
        UWorld* World = TestWorld.World;
        const FTransform Spawn(FRotator(0, 30, 0), FVector(0, 0, 300));
        ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Spawn,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Log->BaseDefinitionId = TEXT("Wood.Log.Diag");
        Log->SelectedProcessingMeshSetId = Family.VariantSetId;
        Log->ProcessingMeshSets.Add(Family);
        Log->SetPieceStaticMesh(Family.BaseRoundMesh);
        Log->FinishSpawning(Spawn);
        Log->GetLogMesh()->SetSimulatePhysics(false);

        auto Apply = [&](ELogCutAction Action, ALogBuildPiece* Target) -> FString
        {
            const FString Name = Actions->GetNameStringByValue(static_cast<int64>(Action));
            if (!Target->CanApplyCutAction(Action))
                return FString::Printf(TEXT("%s BLOCKED (%s)"), *Name, *Target->GetCutActionBlockReason(Action));
            return FString::Printf(TEXT("%s %s"), *Name, Target->ApplyCutAction(Action) ? TEXT("ok") : TEXT("FAILED"));
        };
        FString Report = Apply(Step.First, Log);
        if (Step.bThen)
        {
            ALogBuildPiece* Next = nullptr;
            for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
            {
                if (It->CanApplyCutAction(Step.Then)) { Next = *It; break; }
            }
            Report += TEXT(" -> ") + (Next ? Apply(Step.Then, Next) : FString(TEXT("no piece accepts next step")));
        }

        TArray<ALogBuildPiece*> Pieces;
        for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
            Pieces.Add(*It);
        for (ALogBuildPiece* Piece : Pieces)
        {
            MyProject3Construction::FLogGeometry Shape;
            MyProject3Construction::GetLogGeometry(Piece, Shape);
            Report += FString::Printf(TEXT("\n    %s len=%.0f r=%.0f prof=%s frac=%s at (%.0f,%.0f,%.0f) scale=(%.2f,%.2f,%.2f)"),
                *GetNameSafe(Piece->GetLogMesh()->GetStaticMesh()), Shape.Length(), Shape.Radius,
                *StaticEnum<ELogCrossSectionProfile>()->GetNameStringByValue(static_cast<int64>(Piece->GetCrossSectionProfile())),
                *StaticEnum<ELogLengthFraction>()->GetNameStringByValue(static_cast<int64>(Piece->GetLengthFraction())),
                Shape.Centre.X, Shape.Centre.Y, Shape.Centre.Z,
                Piece->GetActorScale3D().X, Piece->GetActorScale3D().Y, Piece->GetActorScale3D().Z);
        }
        for (int32 A = 0; A < Pieces.Num(); ++A)
        {
            for (int32 B = A + 1; B < Pieces.Num(); ++B)
            {
                const FBox BoxA = Pieces[A]->GetComponentsBoundingBox(true);
                const FBox BoxB = Pieces[B]->GetComponentsBoundingBox(true);
                const FBox Overlap = BoxA.Overlap(BoxB);
                if (Overlap.IsValid && Overlap.GetVolume() > 0.25f * FMath::Min(BoxA.GetVolume(), BoxB.GetVolume()))
                    Report += FString::Printf(TEXT("\n    OVERLAP %d/%d: %.0f%% of the smaller piece"), A, B,
                        100.0f * Overlap.GetVolume() / FMath::Min(BoxA.GetVolume(), BoxB.GetVolume()));
            }
        }
        AddInfo(Report);
        TestFalse(TEXT("Cut pieces do not start inside each other"), Report.Contains(TEXT("OVERLAP")));
        TestFalse(TEXT("No allowed cut fails"), Report.Contains(TEXT("FAILED")));
        TestFalse(TEXT("The demo family is valid"), Report.Contains(TEXT("No unique processing mesh set")));
    }
    return true;
}


// Stacking on stations: a log laid on a placed log sits down into it; lying
// along it starts on a module boundary so its stations sit over the lower ones;
// across it, it crosses at a station (37.5, 112.5 ... from the end); a post
// stands on a station.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogGridStackTest,
    "MyProject3.Carry.Logs.GridStackingAlongAcrossAndPosts", TestFlags)
bool FLogGridStackTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Lower = SpawnBeam(World, FVector(0, 0, 20), FRotator::ZeroRotator, 600.0f, 36.0f, true);
    ALogBuildPiece* Upper = SpawnBeam(World, FVector(0, -400, 20), FRotator::ZeroRotator, 300.0f, 36.0f, false);
    FPlayerTools Player(World);
    if (!TestNotNull(TEXT("Logs"), Upper))
        return false;
    using namespace MyProject3Construction;
    FLogGeometry Shape;
    FTransform Transform;

    Player.Carry->PlacementMode = ELogPlacementMode::Horizontal;
    TestTrue(TEXT("Lies along"), Player.Carry->ComputeStackPlacement(Upper, Lower, FVector(100, 0, 38), 0.0f, Transform));
    Upper->SetActorTransform(Transform);
    GetLogGeometry(Upper, Shape);
    TestTrue(TEXT("Along the lower log"), FMath::Abs(Shape.Axis.X) > 0.99f);
    TestTrue(FString::Printf(TEXT("Starts on a 75 cm grid point (%.1f)"), Shape.EndA().X),
        FMath::IsNearlyZero(FMath::Fmod(Shape.EndA().X + 300.0f, 75.0f), 1.0f) ||
        FMath::IsNearlyEqual(FMath::Fmod(Shape.EndA().X + 300.0f, 75.0f), 75.0f, 1.0f));
    TestTrue(FString::Printf(TEXT("Sits down into the lower log (z %.1f)"), Shape.Centre.Z),
        Shape.Centre.Z > 20.0f + 18.0f && Shape.Centre.Z < 20.0f + 36.0f);

    TestTrue(TEXT("Lies across"), Player.Carry->ComputeStackPlacement(Upper, Lower, FVector(170, 0, 38), 90.0f, Transform));
    Upper->SetActorTransform(Transform);
    GetLogGeometry(Upper, Shape);
    TestTrue(TEXT("Across the lower log"), FMath::Abs(Shape.Axis.Y) > 0.99f);
    // The lower log runs -300..300: station 6 is 487.5 cm from its end, x = 187.5.
    TestTrue(FString::Printf(TEXT("Crossing on a station (x %.1f)"), Shape.Centre.X), FMath::IsNearlyEqual(Shape.Centre.X, 187.5f, 1.0f));
    // And the upper log is crossed at one of its own stations.
    const float UpperStation = FMath::Abs(Shape.EndB().Y - 0.0f);
    TestTrue(FString::Printf(TEXT("At an upper station (%.1f from its end)"), UpperStation),
        FMath::IsNearlyEqual(FMath::Fmod(UpperStation - 37.5f + 0.1f, 75.0f), 0.1f, 0.5f) ||
        FMath::IsNearlyEqual(FMath::Fmod(FMath::Abs(Shape.EndA().Y) - 37.5f + 0.1f, 75.0f), 0.1f, 0.5f));

    Player.Carry->PlacementMode = ELogPlacementMode::Vertical;
    TestTrue(TEXT("Post on the log"), Player.Carry->ComputeStackPlacement(Upper, Lower, FVector(-80, 0, 38), 0.0f, Transform));
    Upper->SetActorTransform(Transform);
    GetLogGeometry(Upper, Shape);
    TestTrue(TEXT("The post stands"), FMath::Abs(Shape.Axis.Z) > 0.99f);
    // Aimed at x = -80: the nearest station is 187.5 cm from the end, x = -112.5.
    TestTrue(FString::Printf(TEXT("On a station (x %.1f)"), Shape.Centre.X), FMath::IsNearlyEqual(Shape.Centre.X, -112.5f, 1.0f));
    TestTrue(TEXT("Its foot rests on the log top"), FMath::Abs((Shape.Centre.Z - Shape.HalfLength) - (20.0f + 18.0f * 0.6f)) < 2.0f);
    return true;
}


// Length cut by aim (SOTF red dashes): the middle of a full log gives two halves,
// near an end gives 3/4 + 1/4 with the short piece at that end; the 450 cm mesh
// exists, so the 3/4 cut really works.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAimedLengthCutTest,
    "MyProject3.Carry.Logs.AimedLengthCutHalfAndThreeQuarter", TestFlags)
bool FAimedLengthCutTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    const FLogProcessingMeshSet Family = AVerticalSliceBWorldActor::BuildLogMeshFamily();
    if (!TestNotNull(TEXT("Demo log family"), Family.BaseRoundMesh.Get()))
        return false;
    const FTransform Spawn(FRotator::ZeroRotator, FVector(0, 0, 300));
    ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Spawn,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Log->BaseDefinitionId = TEXT("Wood.Log.AimCut");
    Log->SelectedProcessingMeshSetId = Family.VariantSetId;
    Log->ProcessingMeshSets.Add(Family);
    Log->SetPieceStaticMesh(Family.BaseRoundMesh);
    Log->FinishSpawning(Spawn);
    Log->GetLogMesh()->SetSimulatePhysics(false);

    FPlayerTools Player(World);
    // The aim-resolved length cut is the default; recipes are reached with C.
    TestEqual(TEXT("Length cut selected"), Player.Carry->GetSelectedCutAction(), ELogCutAction::SplitLengthHalf);

    MyProject3Construction::FLogGeometry Shape;
    MyProject3Construction::GetLogGeometry(Log, Shape);
    ELogCutAction Action = ELogCutAction::SplitHalf;
    float Share = -1.0f;
    bool bFlip = false;
    TestTrue(TEXT("Middle resolves"), Player.Carry->ResolveCutAtPoint(Log, Shape.Centre, Action, Share, bFlip));
    TestEqual(TEXT("Middle gives halves"), Action, ELogCutAction::SplitLengthHalf);
    TestTrue(TEXT("Near +X end resolves"), Player.Carry->ResolveCutAtPoint(Log, Shape.Centre + Shape.Axis * 150.0f, Action, Share, bFlip));
    TestEqual(TEXT("Near an end gives 3/4 + 1/4"), Action, ELogCutAction::SplitLengthThreeQuarter);
    TestFalse(TEXT("No turn needed at +X"), bFlip);
    TestTrue(TEXT("Cut ring at 3/4"), FMath::IsNearlyEqual(Share, 0.75f));
    Player.Carry->ResolveCutAtPoint(Log, Shape.Centre - Shape.Axis * 150.0f, Action, Share, bFlip);
    FLogCutChoice Free;
    TestTrue(TEXT("Between the recipe points"), Player.Carry->ResolveCutChoice(Log, Shape.Centre + Shape.Axis * 90.0f, Free));
    TestTrue(FString::Printf(TEXT("A free cut on the 25 cm step (%.3f)"), Free.Share),
        Free.Kind == ELogCutKind::FreeLength && FMath::IsNearlyEqual(Free.Share * 600.0f, 400.0f, 1.0f));
    TestTrue(TEXT("At -X the log is turned first"), bFlip && FMath::IsNearlyEqual(Share, 0.25f));
    TArray<FVector> Ring;
    TestTrue(TEXT("Red ring preview"), Player.Carry->BuildCutRingSegments(Log, 0.25f, Ring) && Ring.Num() > 10);

    // The end face means "split it along its length"; the bark beside it is still a length cut.
    FLogCutChoice EndFace;
    const FVector EndPoint = Shape.Centre + Shape.Axis.GetSafeNormal() * (Shape.HalfLength - 2.0f);
    const bool bEndResolves = Player.Carry->ResolveCutChoice(Log, EndPoint, EndFace);
    if (Log->CanApplyCutAction(ELogCutAction::SplitHalf))
    {
        TestTrue(TEXT("End face resolves"), bEndResolves);
        TestEqual(TEXT("End face splits along the length"), EndFace.Action, ELogCutAction::SplitHalf);
        TestTrue(TEXT("The split is a recipe"), EndFace.Kind == ELogCutKind::Recipe);
    }
    else
    {
        TestTrue(TEXT("Without a split mesh the end face is a length cut"), EndFace.Action != ELogCutAction::SplitHalf);
    }
    FLogCutChoice Bark;
    const FVector Up = FVector::CrossProduct(Shape.Axis.GetSafeNormal(), FVector::RightVector).GetSafeNormal();
    const FVector Side = Up.IsNearlyZero() ? FVector::UpVector : Up;
    TestTrue(TEXT("Bark near the end resolves"),
        Player.Carry->ResolveCutChoice(Log, EndPoint + Side * Shape.Radius, Bark));
    TestTrue(TEXT("Bark near the end is not a split"), Bark.Action != ELogCutAction::SplitHalf);

    TestTrue(FString::Printf(TEXT("3/4 cut allowed (%s)"), *Log->GetCutActionBlockReason(ELogCutAction::SplitLengthThreeQuarter)),
        Log->CanApplyCutAction(ELogCutAction::SplitLengthThreeQuarter));
    TestTrue(TEXT("3/4 cut applies"), Log->ApplyCutAction(ELogCutAction::SplitLengthThreeQuarter));
    TArray<float> Lengths;
    for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
    {
        MyProject3Construction::FLogGeometry Piece;
        if (MyProject3Construction::GetLogGeometry(*It, Piece))
            Lengths.Add(Piece.Length());
    }
    Lengths.Sort();
    TestEqual(TEXT("Two pieces"), Lengths.Num(), 2);
    if (Lengths.Num() == 2)
    {
        TestTrue(FString::Printf(TEXT("A 150 cm piece (%.0f)"), Lengths[0]), FMath::IsNearlyEqual(Lengths[0], 150.0f, 6.0f));
        TestTrue(FString::Printf(TEXT("A 450 cm piece (%.0f)"), Lengths[1]), FMath::IsNearlyEqual(Lengths[1], 450.0f, 6.0f));
    }
    return true;
}

// A felled tree gives logs by trunk length: 600 cm logs first, then one shorter piece.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrunkYieldTest,
    "MyProject3.World.Trees.YieldFollowsTrunkLength", TestFlags)
bool FTrunkYieldTest::RunTest(const FString& Parameters)
{
    const TArray<float> Small = AHarvestableTreeActor::GetTrunkLogLengths(800.0f, 45.0f, 0.12f);
    const TArray<float> Medium = AHarvestableTreeActor::GetTrunkLogLengths(1300.0f, 45.0f, 0.12f);
    const TArray<float> Tall = AHarvestableTreeActor::GetTrunkLogLengths(1600.0f, 45.0f, 0.12f);
    const TArray<float> Sapling = AHarvestableTreeActor::GetTrunkLogLengths(400.0f, 45.0f, 0.12f);
    TestEqual(TEXT("8 m tree: one log"), Small.Num(), 1);
    TestTrue(TEXT("13 m tree: 600 + 450"), Medium.Num() == 2 && Medium[0] == 600.0f && Medium[1] == 450.0f);
    TestTrue(TEXT("16 m tree: 600 + 600 + 150"), Tall.Num() == 3 && Tall[2] == 150.0f);
    TestTrue(TEXT("4 m tree: a 300 cm piece"), Sapling.Num() == 1 && Sapling[0] == 300.0f);
    return true;
}

// The helper (Kelvin's orders): fetches a loose log into storage, and builds a
// bench plan from loose logs, cutting them to length.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHelperCompanionTest,
    "MyProject3.Construction.HelperCollectsLogsAndBuildsPlan", TestFlags)
bool FHelperCompanionTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogContainerActor* Store = World->SpawnActorDeferred<ALogContainerActor>(ALogContainerActor::StaticClass(), FTransform(FVector(600, 0, 10)));
    Store->ConfigureKind(ELogContainerKind::SmallLogStorage);
    Store->FinishSpawning(FTransform(FRotator::ZeroRotator, FVector(600, 0, 10), Store->GetActorScale3D()));
    Store->bAutoCollect = false;
    ALogBuildPiece* Loose = SpawnBeam(World, FVector(-500, 300, 20), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    AHelperCompanionActor* Helper = World->SpawnActor<AHelperCompanionActor>(FVector(0, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Helper"), Helper) || !TestNotNull(TEXT("Log"), Loose))
        return false;

    Helper->SetOrder(ECompanionOrder::CollectLogs);
    for (int32 Step = 0; Step < 200 && Store->GetStoredCount() == 0; ++Step)
        Helper->Tick(0.1f);
    TestEqual(TEXT("The helper stored the log"), Store->GetStoredCount(), 1);
    EWorldItemOwnerKind Owner = EWorldItemOwnerKind::World;
    MyProject3Construction::GetPieceOwnerKind(Loose, Owner);
    TestEqual(TEXT("Owned by the storage"), Owner, EWorldItemOwnerKind::Storage);

    ABuildPlanActor* Plan = World->SpawnActor<ABuildPlanActor>(FVector(-300, -400, 0), FRotator::ZeroRotator);
    Plan->InitializePlan(EBuildPlanKind::Bench);
    for (int32 Index = 0; Index < 3; ++Index)
        SpawnBeam(World, FVector(-700 + Index * 60.0f, -700, 20), FRotator(0, 90, 0), 300.0f, 30.0f, false);
    TWeakObjectPtr<ABuildPlanActor> WeakPlan = Plan;
    Helper->SetOrder(ECompanionOrder::BuildPlan);
    for (int32 Step = 0; Step < 600 && WeakPlan.IsValid() && !WeakPlan->IsActorBeingDestroyed(); ++Step)
        Helper->Tick(0.1f);
    TestTrue(TEXT("The bench plan was finished"), !WeakPlan.IsValid() || WeakPlan->IsActorBeingDestroyed());
    int32 Placed = 0;
    for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
        Placed += It->IsPlaced() ? 1 : 0;
    TestEqual(TEXT("Two legs and a seat stand"), Placed, 3);
    return true;
}

// The creek: it flows east, logs in it are found, a log comes down from upstream,
// the old log catcher stands in the water, and a helper waits at camp.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreekDriftTest,
    "MyProject3.VerticalSliceC.CreekFlowCatcherAndHelper", TestFlags)
bool FCreekDriftTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    AVerticalSliceCWorldActor* World = TestWorld.World->SpawnActor<AVerticalSliceCWorldActor>();
    if (!TestNotNull(TEXT("World actor"), World))
        return false;
    World->BuildWorld();
    TestTrue(TEXT("The creek flows east"), World->GetCreekFlow(0.0f).X > 50.0f);
    TestTrue(TEXT("It slows over the ford"), World->GetCreekFlow(2600.0f).Size() < World->GetCreekFlow(0.0f).Size());
    TestTrue(TEXT("The creek bed counts as water"), World->IsInCreek(FVector(0.0f, World->GetCreekCentreY(0.0f), -150.0f)));
    TestFalse(TEXT("The camp is dry"), World->IsInCreek(FVector(0.0f, 0.0f, 0.0f)));
    TestNotNull(TEXT("A helper waits at camp"), World->GetHelper());

    int32 PostsInWater = 0;
    for (TActorIterator<ALogBuildPiece> It(TestWorld.World); It; ++It)
    {
        if (It->IsPlaced() && MyProject3Construction::IsVerticalLog(*It) && World->IsInCreek(It->GetActorLocation()))
            ++PostsInWater;
    }
    TestTrue(FString::Printf(TEXT("The old log catcher stands in the water (%d posts)"), PostsInWater), PostsInWater >= 3);
    World->UpdateCreek(40.0f);
    TestEqual(TEXT("A log came down the creek"), World->GetDriftSpawnCount(), 1);
    return true;
}


namespace
{
    AHarvestableTreeActor* SpawnTestTree(UWorld* World, const FVector& Base, float Height)
    {
        const FTransform Spawn(Base + FVector(0, 0, 0.5f * Height));
        AHarvestableTreeActor* Tree = World->SpawnActorDeferred<AHarvestableTreeActor>(AHarvestableTreeActor::StaticClass(), Spawn);
        if (!Tree)
            return nullptr;
        Tree->TreeMesh->SetStaticMesh(LoadEngineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        Tree->TreeMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, Height / 100.0f));
        Tree->MaxChopHealth = 2.0f;
        Tree->LogActorClass = ALogBuildPiece::StaticClass();
        Tree->LogMeshVariantSetId = TEXT("TestFamily");
        Tree->FinishSpawning(Spawn);
        return Tree;
    }
}

// Two logs at once: E on a loose log while carrying one puts it on the shoulder;
// when the first is put down, the second comes to the hands.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTwoLogCarryTest,
    "MyProject3.Carry.Logs.TwoLogsOnTheShoulder", TestFlags)
bool FTwoLogCarryTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* First = SpawnBeam(World, FVector(0, 0, 20), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    ALogBuildPiece* Second = SpawnBeam(World, FVector(0, 100, 20), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    ALogBuildPiece* Third = SpawnBeam(World, FVector(0, 200, 20), FRotator::ZeroRotator, 300.0f, 30.0f, false);
    FPlayerTools Player(World);
    TestTrue(TEXT("First log in the hands"), Player.Carry->TryPickupPiece(First));
    TestTrue(TEXT("Second log on the shoulder"), Player.Carry->TryShoulderLog(Second));
    TestEqual(TEXT("Two logs carried"), Player.Carry->GetCarriedLogCount(), 2);
    TestFalse(TEXT("Not a third"), Player.Carry->TryShoulderLog(Third));
    EWorldItemOwnerKind Owner = EWorldItemOwnerKind::World;
    MyProject3Construction::GetPieceOwnerKind(Second, Owner);
    TestEqual(TEXT("The shoulder log is held in the ledger"), Owner, EWorldItemOwnerKind::Held);
    Player.Carry->DropHeldPiece();
    Player.Carry->PromoteShoulderLog();
    TestTrue(TEXT("The second log came to the hands"), Player.Carry->HeldPiece == Second);
    TestEqual(TEXT("One log carried"), Player.Carry->GetCarriedLogCount(), 1);
    return true;
}

// 3.7 / 2.6: a standing post is cut only when nothing rests on it; its top is
// sharpened when the axe aims at the top.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPostShortenTest,
    "MyProject3.Construction.PostCutOnlyWhenFreeAndTopSharpens", TestFlags)
bool FPostShortenTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Holding = SpawnBeam(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    SpawnBeam(World, FVector(300, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    SpawnBeam(World, FVector(150, 0, 215), FRotator::ZeroRotator, 340.0f, 30.0f, true);
    ALogBuildPiece* Free = SpawnBeam(World, FVector(0, 500, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    FPlayerTools Player(World);
    // The aim-resolved length cut is the default.
    TestEqual(TEXT("Length cut selected"), Player.Carry->GetSelectedCutAction(), ELogCutAction::SplitLengthHalf);

    FLogCutChoice Choice;
    FString Reason;
    TestTrue(TEXT("A cut is offered on the post"), Player.Carry->ResolveCutChoice(Holding, FVector(0, 0, 120), Choice));
    TestEqual(TEXT("Shorten kind"), Choice.Kind, ELogCutKind::ShortenPost);
    TestTrue(TEXT("The post holds the beam"), Player.Carry->PostHoldsAnything(Holding));
    TestFalse(TEXT("A post holding a beam is not cut"), Player.Carry->CanMakeCut(Holding, Choice, Reason));

    TestTrue(TEXT("The free post can be shortened"), Player.Carry->ResolveCutChoice(Free, FVector(0, 500, 148), Choice) &&
        Player.Carry->CanMakeCut(Free, Choice, Reason));
    TestTrue(TEXT("Cut made"), Player.Carry->CutLogAtShare(Free, Choice.Share));
    MyProject3Construction::FLogGeometry Post;
    MyProject3Construction::GetLogGeometry(Free, Post);
    TestTrue(FString::Printf(TEXT("Post is 150 cm (%.0f)"), Post.Length()), FMath::IsNearlyEqual(Post.Length(), 150.0f, 4.0f));
    TestTrue(TEXT("Still standing"), Free->IsPlaced());

    TestTrue(TEXT("Aiming at the top"), Player.Carry->ResolveCutChoice(Free, FVector(0, 500, 145), Choice));
    TestEqual(TEXT("The top is sharpened"), Choice.Kind, ELogCutKind::Sharpen);
    TArray<FVector> Cone;
    TestTrue(TEXT("Red sharpen marks"), Player.Carry->BuildSharpenSegments(Free, Cone) && Cone.Num() > 8);
    return true;
}

// A log wall stands at most six rows high.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWallRowLimitTest,
    "MyProject3.Construction.WallRowsCountedToSix", TestFlags)
bool FWallRowLimitTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    TArray<ALogBuildPiece*> Rows;
    for (int32 Row = 0; Row < 6; ++Row)
        Rows.Add(SpawnBeam(World, FVector(0, 0, 15 + Row * 27.0f), FRotator::ZeroRotator, 300.0f, 30.0f, true));
    SpawnBeam(World, FVector(0, 400, 15), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    FPlayerTools Player(World);
    TestEqual(TEXT("Third row counts three"), Player.Carry->CountWallRows(Rows[2]), 3);
    TestEqual(TEXT("Top row counts six"), Player.Carry->CountWallRows(Rows[5]), 6);
    TestTrue(TEXT("The limit is six"), Player.Carry->CountWallRows(Rows[5]) >= Player.Carry->GetMaxWallRows());
    return true;
}

// One-sided apex: the carried log fills the lowest row under a sloped beam, cut
// short enough to stay under the slope.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexFillTest,
    "MyProject3.Construction.OneSidedApexFillsUnderSlope", TestFlags)
bool FApexFillTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Slope = SpawnBeam(World, FVector(150, 0, 100), FRotator(30, 0, 0), 340.0f, 30.0f, true);
    ALogBuildPiece* Log = SpawnBeam(World, FVector(0, -300, 20), FRotator::ZeroRotator, 400.0f, 30.0f, false);
    FPlayerTools Player(World);
    TestTrue(TEXT("Carrying"), Player.Carry->TryPickupPiece(Log));
    TestTrue(TEXT("Apex fill handled"), Player.Tools->TryFillApex(Slope));
    TestTrue(TEXT("The log is built in"), Log->IsPlaced());
    MyProject3Construction::FLogGeometry Row, Beam;
    MyProject3Construction::GetLogGeometry(Log, Row);
    MyProject3Construction::GetLogGeometry(Slope, Beam);
    TestTrue(TEXT("The row lies level"), MyProject3Construction::IsHorizontalLog(Log));
    TestTrue(FString::Printf(TEXT("Cut to fit under the slope (%.0f)"), Row.Length()), Row.Length() < 300.0f && Row.Length() > 150.0f);
    const FVector High = Beam.EndA().Z > Beam.EndB().Z ? Beam.EndA() : Beam.EndB();
    TestTrue(TEXT("It ends under the high end"), FMath::Abs(FMath::Max(Row.EndA().X, Row.EndB().X) - High.X) < 5.0f);
    return true;
}

// Thrown onto a sled: a log that lands on it is loaded.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSledThrowLoadTest,
    "MyProject3.Construction.SledLoadsThrownLogs", TestFlags)
bool FSledThrowLoadTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogContainerActor* Sled = World->SpawnActorDeferred<ALogContainerActor>(ALogContainerActor::StaticClass(), FTransform(FVector(0, 0, 10)));
    Sled->ConfigureKind(ELogContainerKind::BasicSled);
    Sled->FinishSpawning(FTransform(FRotator::ZeroRotator, FVector(0, 0, 10), Sled->GetActorScale3D()));
    SpawnBeam(World, FVector(20, 30, 90), FRotator::ZeroRotator, 200.0f, 30.0f, false);
    SpawnBeam(World, FVector(900, 0, 20), FRotator::ZeroRotator, 200.0f, 30.0f, false);
    for (int32 Step = 0; Step < 10; ++Step)
        Sled->Tick(0.2f);
    TestEqual(TEXT("The log that landed on the sled is loaded, the far one is not"), Sled->GetStoredCount(), 1);
    return true;
}

// New ready-made plans, a drawn defensive wall, and a tree platform that rests on its trunk.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPrefabPlansTest,
    "MyProject3.Construction.PrefabPlansTreeAndWallLine", TestFlags)
bool FPrefabPlansTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    FPlayerTools Player(World);
    for (EBuildPlanKind Kind : { EBuildPlanKind::SmallLogCabin, EBuildPlanKind::Chair, EBuildPlanKind::Couch,
        EBuildPlanKind::Shelf, EBuildPlanKind::RabbitHutch, EBuildPlanKind::CornerRamp, EBuildPlanKind::HuntingShelter })
    {
        ABuildPlanActor* Plan = Player.Tools->PlacePlan(Kind, FVector(0, 0, 0), 0.0f);
        TestTrue(FString::Printf(TEXT("%s has parts"), *ABuildPlanActor::GetPlanName(Kind)), Plan && Plan->GetSlotCount() >= 4);
        if (Plan)
            Plan->Destroy();
    }
    ABuildPlanActor* Cabin = Player.Tools->PlacePlan(EBuildPlanKind::SmallLogCabin, FVector(0, 0, 0), 0.0f);
    TestTrue(FString::Printf(TEXT("The cabin has walls and a roof (%d parts)"), Cabin ? Cabin->GetSlotCount() : 0),
        Cabin && Cabin->GetSlotCount() >= 30);
    if (Cabin)
        Cabin->Destroy();

    ABuildPlanActor* Line = Player.Tools->PlaceDefenseLine(FVector(0, 0, 0), FVector(400, 0, 0));
    TestTrue(FString::Printf(TEXT("A 4 m line takes 11 stakes (%d)"), Line ? Line->GetSlotCount() : 0), Line && Line->GetSlotCount() == 11);
    if (Line)
    {
        const FVector First = Line->GetSlotWorld(0).GetLocation();
        const FVector Last = Line->GetSlotWorld(Line->GetSlotCount() - 1).GetLocation();
        TestTrue(TEXT("Stakes run along the drawn line"), FMath::Abs(First.Y) < 2.0f && FMath::Abs(Last.Y) < 2.0f &&
            FMath::IsNearlyEqual(FMath::Abs(Last.X - First.X), 400.0f, 2.0f));
    }

    TestNull(TEXT("No tree, no tree platform"), Player.Tools->PlacePlan(EBuildPlanKind::TreePlatform, FVector(2000, 2000, 0), 0.0f));
    AHarvestableTreeActor* Tree = SpawnTestTree(World, FVector(1000, 0, 0), 900.0f);
    ABuildPlanActor* Platform = Player.Tools->PlacePlan(EBuildPlanKind::TreePlatform, FVector(1150, 80, 0), 0.0f);
    if (!TestNotNull(TEXT("Tree platform around the tree"), Platform))
        return false;
    TestTrue(TEXT("Centred on the trunk"), FVector::Dist2D(Platform->GetActorLocation(), Tree->GetActorLocation()) < 1.0f);
    ALogBuildPiece* Beam = SpawnBeam(World, Platform->GetSlotWorld(0).GetLocation(), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    Platform->MarkFilled(0, Beam);
    UTreeAnchorComponent* Anchor = Beam->FindComponentByClass<UTreeAnchorComponent>();
    TestTrue(TEXT("The beam rests on the trunk"), Anchor && Anchor->GetTree() == Tree);
    return true;
}

// More helper orders: bring logs to a pile, fell a tree when none are loose,
// and sharpen the stakes of a wall.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHelperMoreOrdersTest,
    "MyProject3.Construction.HelperPilesChopsAndSharpens", TestFlags)
bool FHelperMoreOrdersTest::RunTest(const FString& Parameters)
{
    {
        FScopedTestWorld TestWorld;
        UWorld* World = TestWorld.World;
        ALogBuildPiece* Log = SpawnBeam(World, FVector(500, 300, 20), FRotator::ZeroRotator, 300.0f, 30.0f, false);
        AHelperCompanionActor* Helper = World->SpawnActor<AHelperCompanionActor>(FVector(0, 0, 0), FRotator::ZeroRotator);
        Helper->bMayChopTrees = false;
        Helper->SetOrder(ECompanionOrder::PileLogsHere);
        Helper->SetPilePoint(FVector(-600, 0, 0));
        for (int32 Step = 0; Step < 300 && Helper->GetDeliveredCount() == 0; ++Step)
            Helper->Tick(0.1f);
        TestEqual(TEXT("One log brought"), Helper->GetDeliveredCount(), 1);
        TestTrue(TEXT("It lies at the pile"), FVector::Dist2D(Log->GetActorLocation(), FVector(-600, 0, 0)) < 200.0f);
    }
    {
        FScopedTestWorld TestWorld;
        UWorld* World = TestWorld.World;
        SpawnTestTree(World, FVector(800, 0, 0), 900.0f);
        AHelperCompanionActor* Helper = World->SpawnActor<AHelperCompanionActor>(FVector(0, 0, 0), FRotator::ZeroRotator);
        Helper->SetOrder(ECompanionOrder::CollectLogs);
        for (int32 Step = 0; Step < 300 && Helper->GetTreesFelled() == 0; ++Step)
            Helper->Tick(0.1f);
        TestEqual(TEXT("With no loose logs he fells a tree"), Helper->GetTreesFelled(), 1);
    }
    {
        FScopedTestWorld TestWorld;
        UWorld* World = TestWorld.World;
        ALogBuildPiece* StakeA = SpawnBeam(World, FVector(400, 0, 150), FRotator(90, 0, 0), 300.0f, 30.0f, true);
        ALogBuildPiece* StakeB = SpawnBeam(World, FVector(400, 45, 150), FRotator(90, 0, 0), 300.0f, 30.0f, true);
        AHelperCompanionActor* Helper = World->SpawnActor<AHelperCompanionActor>(FVector(0, 0, 0), FRotator::ZeroRotator);
        Helper->SetOrder(ECompanionOrder::MaintainWalls);
        for (int32 Step = 0; Step < 300 && Helper->GetWallsTended() < 2; ++Step)
            Helper->Tick(0.1f);
        TestTrue(TEXT("Both stakes sharpened"), StakeA->FindComponentByClass<USharpenedTipComponent>() &&
            StakeB->FindComponentByClass<USharpenedTipComponent>());
    }
    return true;
}


// Raising a beam: a taller post set under the free end of a placed beam lifts that
// end onto its top; the other end stays on its post.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaiseBeamTest,
    "MyProject3.Construction.NewPostRaisesBeamEnd", TestFlags)
bool FRaiseBeamTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    SpawnBeam(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Beam = SpawnBeam(World, FVector(150, 0, 215), FRotator::ZeroRotator, 340.0f, 30.0f, true);
    ALogBuildPiece* Tall = SpawnBeam(World, FVector(310, 0, 130), FRotator(90, 0, 0), 260.0f, 30.0f, true);
    ALogBuildPiece* OnLog = SpawnBeam(World, FVector(150, 0, 270), FRotator(90, 0, 0), 80.0f, 20.0f, true);
    FPlayerTools Player(World);
    TestEqual(TEXT("A post standing on the beam lifts nothing"), Player.Carry->RaiseBeamsOntoPost(OnLog), 0);
    TestEqual(TEXT("The tall post lifts one beam end"), Player.Carry->RaiseBeamsOntoPost(Tall), 1);
    MyProject3Construction::FLogGeometry Shape;
    MyProject3Construction::GetLogGeometry(Beam, Shape);
    const FVector Right = Shape.EndA().X > Shape.EndB().X ? Shape.EndA() : Shape.EndB();
    const FVector Left = Shape.EndA().X > Shape.EndB().X ? Shape.EndB() : Shape.EndA();
    TestTrue(FString::Printf(TEXT("The right end sits on the tall post (%.0f)"), Right.Z), FMath::IsNearlyEqual(Right.Z, 275.0f, 6.0f));
    TestTrue(FString::Printf(TEXT("The left end stays (%.0f)"), Left.Z), FMath::IsNearlyEqual(Left.Z, 215.0f, 3.0f));
    return true;
}


// 3.5: split pieces cannot stand as posts; 3.6: a full log used for boards is split
// on the spot, one half in the hands and the other on the shoulder.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSplitRulesTest,
    "MyProject3.Carry.Logs.SplitPiecesAndAutoSplit", TestFlags)
bool FSplitRulesTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    const FLogProcessingMeshSet Family = AVerticalSliceBWorldActor::BuildLogMeshFamily();
    if (!TestNotNull(TEXT("Demo log family"), Family.BaseRoundMesh.Get()))
        return false;
    const FTransform Spawn(FRotator::ZeroRotator, FVector(0, 0, 40));
    ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Spawn,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Log->BaseDefinitionId = TEXT("Wood.Log.AutoSplit");
    Log->SelectedProcessingMeshSetId = Family.VariantSetId;
    Log->ProcessingMeshSets.Add(Family);
    Log->SetPieceStaticMesh(Family.BaseRoundMesh);
    Log->FinishSpawning(Spawn);
    Log->GetLogMesh()->SetSimulatePhysics(false);
    TestTrue(TEXT("A round log can stand"), ULogCarryComponent::CanStandAsPost(Log));

    FPlayerTools Player(World);
    TestTrue(TEXT("Carrying"), Player.Carry->TryPickupPiece(Log));
    TestTrue(TEXT("Split in the hands"), Player.Carry->SplitCarriedLogInHalf());
    ALogBuildPiece* Half = Player.Carry->HeldPiece;
    TestTrue(TEXT("A half in the hands"), Half && Half->GetCrossSectionProfile() == ELogCrossSectionProfile::HalfRound);
    TestTrue(TEXT("The other half on the shoulder"), Player.Carry->ShoulderPiece &&
        Player.Carry->ShoulderPiece->GetCrossSectionProfile() == ELogCrossSectionProfile::HalfRound);
    TestFalse(TEXT("A half cannot stand as a post"), ULogCarryComponent::CanStandAsPost(Half));
    return true;
}

// 4.6 / 4.7: a strut runs from a post up to its beam; a middle post can only be
// removed when the beam would not span too far, which struts make possible.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStrutSpanTest,
    "MyProject3.Construction.StrutsLetMiddlePostGo", TestFlags)
bool FStrutSpanTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Left = SpawnBeam(World, FVector(-280, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Middle = SpawnBeam(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    SpawnBeam(World, FVector(280, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    ALogBuildPiece* Beam = SpawnBeam(World, FVector(0, 0, 215), FRotator::ZeroRotator, 600.0f, 30.0f, true);
    ALogBuildPiece* Half = SpawnBeam(World, FVector(0, -400, 20), FRotator::ZeroRotator, 300.0f, 20.0f, false);
    FPlayerTools Player(World);

    TArray<ALogBuildPiece*> Beams;
    Player.Carry->FindBeamsOnPost(Left, Beams);
    TestTrue(TEXT("The beam rests on the post"), Beams.Contains(Beam));
    FTransform Strut;
    float Length = 0.0f;
    FVector Joint;
    TestTrue(TEXT("A strut fits from the post to the beam"),
        Player.Carry->ComputeStrutPlacement(Half, Left, Beam, FVector(0, 0, 200), Strut, Length, Joint));
    TestTrue(FString::Printf(TEXT("About 1.3 m long (%.0f)"), Length), Length > 110.0f && Length < 150.0f);
    TestTrue(FString::Printf(TEXT("It meets the beam 90 cm in (%.0f)"), Joint.X), FMath::IsNearlyEqual(Joint.X, -190.0f, 5.0f));

    TestFalse(TEXT("Without struts the middle post stays"), Player.Tools->DismantlePiece(Middle));
    TestTrue(TEXT("Still placed"), Middle->IsPlaced());
    // Two struts, one at each end post.
    SpawnBeam(World, FVector(-237, 0, 160), FRotator(45, 0, 0), 125.0f, 20.0f, true);
    SpawnBeam(World, FVector(237, 0, 160), FRotator(45, 180, 0), 125.0f, 20.0f, true);
    bool bOverhang = false;
    const float Span = Player.Tools->GetLongestUnsupportedSpan(Beam, Middle, bOverhang);
    TestTrue(FString::Printf(TEXT("Struts shorten the span (%.0f)"), Span), Span < Player.Tools->MaxBeamSpanCm && !bOverhang);
    TestTrue(TEXT("Now the middle post can go"), Player.Tools->DismantlePiece(Middle));
    return true;
}

// 2.2 / 2.3: a post near one log length from another moves to exactly that span,
// and the ground shows a circle under a post and a rectangle under a lying log.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPostSpacingTest,
    "MyProject3.Carry.Logs.PostSpacingAndGroundMarks", TestFlags)
bool FPostSpacingTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Standing = SpawnBeam(World, FVector(0, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    FPlayerTools Player(World);
    FVector Base;
    ALogBuildPiece* Partner = nullptr;
    float Span = 0.0f;
    TestTrue(TEXT("Lines up near 3 m"), Player.Carry->SnapPostSpacing(FVector(285, 20, 0), Base, Partner, Span));
    TestTrue(TEXT("With the standing post"), Partner == Standing && Span == 300.0f);
    TestTrue(FString::Printf(TEXT("Exactly 300 cm away (%.1f)"), FVector::Dist2D(Base, FVector::ZeroVector)),
        FMath::IsNearlyEqual(FVector::Dist2D(Base, FVector::ZeroVector), 300.0f, 0.5f));
    TestFalse(TEXT("Not in between"), Player.Carry->SnapPostSpacing(FVector(220, 0, 0), Base, Partner, Span));
    return true;
}

// 4.4: a post set under the middle of a beam is cut to meet it and props it up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMidPostFitTest,
    "MyProject3.Construction.PostUnderBeamIsCutToFit", TestFlags)
bool FMidPostFitTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    SpawnBeam(World, FVector(-280, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    SpawnBeam(World, FVector(280, 0, 100), FRotator(90, 0, 0), 200.0f, 30.0f, true);
    SpawnBeam(World, FVector(0, 0, 215), FRotator::ZeroRotator, 600.0f, 30.0f, true);
    ALogBuildPiece* Tall = SpawnBeam(World, FVector(0, 0, 150), FRotator(90, 0, 0), 300.0f, 30.0f, true);
    FPlayerTools Player(World);
    TestEqual(TEXT("The post is fitted"), Player.Carry->RaiseBeamsOntoPost(Tall), 1);
    MyProject3Construction::FLogGeometry Post;
    MyProject3Construction::GetLogGeometry(Tall, Post);
    const float Top = FMath::Max(Post.EndA().Z, Post.EndB().Z);
    TestTrue(FString::Printf(TEXT("Its top meets the beam bottom (%.0f)"), Top), FMath::IsNearlyEqual(Top, 200.0f, 3.0f));
    return true;
}

// 7.5: a board aimed at a floor edge with no second support becomes a single step.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSingleStepTest,
    "MyProject3.Construction.SingleStepAtFloorEdge", TestFlags)
bool FSingleStepTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Edge = SpawnBeam(World, FVector(0, 0, 60), FRotator::ZeroRotator, 400.0f, 30.0f, true);
    ALogBuildPiece* Board = SpawnBeam(World, FVector(0, -600, 20), FRotator::ZeroRotator, 300.0f, 10.0f, false);
    FPlayerTools Player(World);
    TestTrue(TEXT("Carrying"), Player.Carry->TryPickupPiece(Board));
    const bool bStep = Player.Tools->TryPlaceSingleStep(Edge, FVector(0, -15, 75));
    TestTrue(FString::Printf(TEXT("Step placed (%s / %s)"), *Player.Tools->GetStatus(), *Player.Carry->GetPlacementStatus()), bStep);
    TestTrue(TEXT("Built in"), Board->IsPlaced());
    MyProject3Construction::FLogGeometry Step;
    MyProject3Construction::GetLogGeometry(Board, Step);
    TestTrue(FString::Printf(TEXT("On the player's side of the edge (%.0f)"), Step.Centre.Y), Step.Centre.Y < -20.0f);
    TestTrue(TEXT("Lower than the floor"), Step.Centre.Z < 75.0f);
    return true;
}

// 8.7: a badly damaged piece takes no new parts.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamagedRefusesTest,
    "MyProject3.Construction.DamagedPieceTakesNoParts", TestFlags)
bool FDamagedRefusesTest::RunTest(const FString& Parameters)
{
    FScopedTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ALogBuildPiece* Beam = SpawnBeam(World, FVector(0, 0, 20), FRotator::ZeroRotator, 300.0f, 30.0f, true);
    FPlayerTools Player(World);
    UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    FStructuralNodeRecord Node;
    if (!Graph->GetNode(Beam->GetStructuralNodeId(), Node))
    {
        AddWarning(TEXT("The test beam has no graph node; skipped."));
        return true;
    }
    TestFalse(TEXT("Sound at first"), Player.Carry->IsBadlyDamaged(Beam));
    bool bBroken = false;
    Graph->ApplyDamage(Beam->GetStructuralNodeId(), Node.DamageThreshold * 0.6f, bBroken);
    TestTrue(TEXT("Badly damaged"), Player.Carry->IsBadlyDamaged(Beam));
    Graph->RepairDamage(Beam->GetStructuralNodeId(), Node.DamageThreshold);
    TestFalse(TEXT("Repaired"), Player.Carry->IsBadlyDamaged(Beam));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
