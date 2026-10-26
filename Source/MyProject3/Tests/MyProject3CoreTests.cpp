#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "LogBuildPiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

#endif // WITH_DEV_AUTOMATION_TESTS
