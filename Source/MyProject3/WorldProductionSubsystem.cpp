#include "WorldProductionSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "LogBuildPiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"

int32 UWorldProductionSubsystem::FindRecipe(FName RecipeId) const
{
    return Recipes.IndexOfByPredicate([RecipeId](const FWorldProductionRecipe& R)
        { return R.RecipeId == RecipeId; });
}

int64 UWorldProductionSubsystem::CountStockUnits(const TMap<FName, int32>& Stock) const
{
    int64 Total = 0;
    for (const TPair<FName, int32>& Pair : Stock)
    {
        if (Pair.Key.IsNone() || Pair.Value < 0) return MAX_int64;
        Total += Pair.Value;
        if (Total > MAX_int32) return MAX_int64;
    }
    return Total;
}

bool UWorldProductionSubsystem::RegisterRecipe(const FWorldProductionRecipe& Recipe)
{
    if (Recipe.RecipeId.IsNone() || Recipe.Inputs.IsEmpty() || Recipe.Outputs.IsEmpty() ||
        !FMath::IsFinite(Recipe.DurationSeconds) || Recipe.DurationSeconds <= 0.0 ||
        FindRecipe(Recipe.RecipeId) != INDEX_NONE || MaxRecipes <= 0 || Recipes.Num() >= MaxRecipes)
        return false;

    auto ValidateIngredients = [](const TArray<FProductionIngredient>& List)
    {
        for (const FProductionIngredient& I : List)
            if (I.ItemId.IsNone() || I.Quantity <= 0) return false;
        return true;
    };
    if (!ValidateIngredients(Recipe.Inputs) || !ValidateIngredients(Recipe.Outputs)) return false;
    Recipes.Add(Recipe);
    return true;
}

FName UWorldProductionSubsystem::MakePhysicalStockItemId(
    FName DefinitionId, FName MeshVariantSetId)
{
    if (DefinitionId.IsNone())
        return NAME_None;
    if (MeshVariantSetId.IsNone())
        return DefinitionId;

    return FName(*FString::Printf(TEXT("%s::%s"),
        *DefinitionId.ToString(), *MeshVariantSetId.ToString()));
}

bool UWorldProductionSubsystem::RegisterStation(const FWorldProductionStation& Station)
{
    if (!Station.StationId.IsValid() || Station.StationTag.IsNone() ||
        Station.StorageCapacityUnits <= 0 || Station.Revision <= 0 ||
        Stations.Contains(Station.StationId) || MaxStations <= 0 || Stations.Num() >= MaxStations ||
        !Station.Jobs.IsEmpty() || !Station.PhysicalIntakeReceipts.IsEmpty())
        return false;
    for (const TPair<FName, int32>& Pair : Station.Stock)
        if (Pair.Key.IsNone() || Pair.Value < 0) return false;
    if (CountStockUnits(Station.Stock) > Station.StorageCapacityUnits) return false;
    Stations.Add(Station.StationId, Station);
    return true;
}

bool UWorldProductionSubsystem::AddStock(
    const FGuid& StationId, FName ItemId, int32 Quantity, int64 ExpectedRevision)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedRevision || Station->Revision == MAX_int64 ||
        ItemId.IsNone() || Quantity <= 0) return false;

    const int32 Existing = Station->Stock.FindRef(ItemId);
    if (Existing > MAX_int32 - Quantity) return false;
    TMap<FName, int32> Proposed = Station->Stock;
    Proposed.Add(ItemId, Existing + Quantity);
    int64 Reserved = 0;
    for (const FWorldProductionJob& Job : Station->Jobs)
    {
        if (Job.State != EProductionJobState::Queued && Job.State != EProductionJobState::Running) continue;
        const int32 RecipeIndex = FindRecipe(Job.RecipeId);
        if (!Recipes.IsValidIndex(RecipeIndex)) return false;
        for (const FProductionIngredient& Output : Recipes[RecipeIndex].Outputs) Reserved += Output.Quantity;
    }
    if (CountStockUnits(Proposed) + Reserved > Station->StorageCapacityUnits) return false;
    Station->Stock = MoveTemp(Proposed);
    ++Station->Revision;
    return true;
}

bool UWorldProductionSubsystem::TryIngestWorldOwnedLogPiece(
    FGuid StationId,
    FGuid OperationId,
    FGuid ItemId,
    int64 ExpectedItemRevision,
    int64 ExpectedStationRevision,
    FName& OutStockItemId)
{
    OutStockItemId = NAME_None;
    if (!GetWorld() || !StationId.IsValid() || !OperationId.IsValid() ||
        !ItemId.IsValid() || ExpectedItemRevision <= 0 ||
        ExpectedItemRevision == MAX_int64 || ExpectedStationRevision <= 0 ||
        ExpectedStationRevision == MAX_int64)
    {
        return false;
    }

    auto FindSourceActor = [this, ItemId]() -> ALogBuildPiece*
    {
        for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
        {
            ALogBuildPiece* Candidate = *It;
            if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed())
                continue;
            UWorldObjectIdentityComponent* Identity =
                Candidate->FindComponentByClass<UWorldObjectIdentityComponent>();
            if (Identity && Identity->HasValidIdentity() &&
                Identity->GetInstanceId() == ItemId)
            {
                return Candidate;
            }
        }
        return nullptr;
    };

    auto RetireResidualActor = [&FindSourceActor]()
    {
        if (ALogBuildPiece* Residual = FindSourceActor())
        {
            Residual->SetActorHiddenInGame(true);
            Residual->SetActorEnableCollision(false);
            if (UStaticMeshComponent* Mesh = Residual->GetLogMesh())
                Mesh->SetSimulatePhysics(false);
            if (!Residual->Destroy())
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Transformed physical log %s remains as a hidden, inert actor."),
                    *GetNameSafe(Residual));
            }
        }
    };

    // Intake receipts are checked before current revisions: a completed operation
    // must replay as success, not add a second stock unit after the station advances.
    for (const TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        for (const FWorldProductionPhysicalIntakeReceipt& Receipt :
            Pair.Value.PhysicalIntakeReceipts)
        {
            if (Receipt.OperationId == OperationId)
            {
                if (Pair.Key != StationId || Receipt.SourceItemId != ItemId ||
                    Receipt.ExpectedItemRevision != ExpectedItemRevision ||
                    Receipt.ExpectedStationRevision != ExpectedStationRevision)
                {
                    return false;
                }

                OutStockItemId = Receipt.StockItemId;
                RetireResidualActor();
                return !OutStockItemId.IsNone();
            }

            // One physical ID may only be ingested once, even if a caller retries
            // with a different operation ID or another station.
            if (Receipt.SourceItemId == ItemId)
                return false;
        }
    }

    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedStationRevision ||
        Station->Revision == MAX_int64)
    {
        return false;
    }

    ALogBuildPiece* Piece = FindSourceActor();
    if (!IsValid(Piece) || Piece->IsHidden() || Piece->IsHeld() ||
        Piece->IsPlaced() || Piece->GetAttachedPieceCount() > 0)
    {
        return false;
    }

    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    if (!Identity || !Identity->HasValidIdentity() ||
        Identity->GetInstanceId() != ItemId ||
        Identity->GetLifecycleState() != EWorldObjectLifecycleState::World ||
        !Ownership)
    {
        return false;
    }

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(ItemId, Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::World ||
        Item.OwnerId.IsValid() || Item.Revision != ExpectedItemRevision ||
        Item.Quantity != 1 || Item.DefinitionId.IsNone())
    {
        return false;
    }

    const FName StockItemId =
        MakePhysicalStockItemId(Item.DefinitionId, Item.MeshVariantSetId);
    if (StockItemId.IsNone())
        return false;

    const int32 CurrentQuantity = Station->Stock.FindRef(StockItemId);
    if (CurrentQuantity < 0 || CurrentQuantity == MAX_int32)
        return false;

    TMap<FName, int32> ProposedStock = Station->Stock;
    ProposedStock.Add(StockItemId, CurrentQuantity + 1);
    const int64 ProposedUnits = CountStockUnits(ProposedStock);
    if (ProposedUnits == MAX_int64)
        return false;

    int64 ReservedOutputUnits = 0;
    for (const FWorldProductionJob& Job : Station->Jobs)
    {
        if (Job.State != EProductionJobState::Queued &&
            Job.State != EProductionJobState::Running)
        {
            continue;
        }

        const int32 RecipeIndex = FindRecipe(Job.RecipeId);
        if (!Recipes.IsValidIndex(RecipeIndex))
            return false;

        for (const FProductionIngredient& Output : Recipes[RecipeIndex].Outputs)
        {
            if (Output.Quantity <= 0 ||
                ReservedOutputUnits > MAX_int64 - Output.Quantity)
            {
                return false;
            }
            ReservedOutputUnits += Output.Quantity;
        }
    }

    if (ProposedUnits > Station->StorageCapacityUnits ||
        ReservedOutputUnits > Station->StorageCapacityUnits - ProposedUnits)
    {
        return false;
    }

    // Preflight above establishes station capacity and source ownership. The
    // ownership transition is the only remaining rejecting mutation; after it
    // succeeds, station stock and the durable receipt are committed together.
    FWorldOwnedItemRecord TransformedItem;
    if (!Ownership->TryTransferItem(OperationId, ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::World, EWorldItemOwnerKind::Transformed,
        FGuid(), Item.WorldLocation, TransformedItem))
    {
        return false;
    }

    FWorldProductionPhysicalIntakeReceipt& Receipt =
        Station->PhysicalIntakeReceipts.AddDefaulted_GetRef();
    Receipt.OperationId = OperationId;
    Receipt.SourceItemId = ItemId;
    Receipt.ExpectedItemRevision = ExpectedItemRevision;
    Receipt.ExpectedStationRevision = ExpectedStationRevision;
    Receipt.SourceDefinitionId = Item.DefinitionId;
    Receipt.SourceMeshVariantSetId = Item.MeshVariantSetId;
    Receipt.StockItemId = StockItemId;
    Receipt.ResultingItemRevision = TransformedItem.Revision;
    Receipt.ResultingStationRevision = ExpectedStationRevision + 1;

    Station->Stock = MoveTemp(ProposedStock);
    ++Station->Revision;
    OutStockItemId = StockItemId;

    Piece->SetActorHiddenInGame(true);
    Piece->SetActorEnableCollision(false);
    if (UStaticMeshComponent* Mesh = Piece->GetLogMesh())
        Mesh->SetSimulatePhysics(false);
    if (!Piece->Destroy())
    {
        // The canonical record and stock already committed. Keep any surviving
        // representation inert rather than exposing a second world-owned log.
        UE_LOG(LogTemp, Error,
            TEXT("Physical log %s became production stock but actor destruction failed; actor remains quarantined."),
            *ItemId.ToString());
    }

    return true;
}

bool UWorldProductionSubsystem::EnqueueJob(
    const FGuid& StationId, FName RecipeId, FGuid& OutJobId)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    const int32 RecipeIndex = FindRecipe(RecipeId);
    if (!Station || !Recipes.IsValidIndex(RecipeIndex) || Station->Revision == MAX_int64 ||
        MaxJobsPerStation <= 0 || Station->Jobs.Num() >= MaxJobsPerStation)
        return false;

    const FWorldProductionRecipe& Recipe = Recipes[RecipeIndex];
    if (!Recipe.RequiredStationTag.IsNone() && Recipe.RequiredStationTag != Station->StationTag)
        return false;

    TMap<FName, int64> Required;
    for (const FProductionIngredient& Input : Recipe.Inputs)
        Required.FindOrAdd(Input.ItemId) += Input.Quantity;
    for (const TPair<FName, int64>& Need : Required)
        if (Need.Value > MAX_int32 || Station->Stock.FindRef(Need.Key) < Need.Value) return false;

    int64 Reserved = 0;
    for (const FWorldProductionJob& Job : Station->Jobs)
    {
        if (Job.State != EProductionJobState::Queued && Job.State != EProductionJobState::Running) continue;
        const int32 ExistingRecipe = FindRecipe(Job.RecipeId);
        if (!Recipes.IsValidIndex(ExistingRecipe)) return false;
        for (const FProductionIngredient& Output : Recipes[ExistingRecipe].Outputs) Reserved += Output.Quantity;
    }
    int64 NewOutputUnits = 0;
    for (const FProductionIngredient& Output : Recipe.Outputs) NewOutputUnits += Output.Quantity;
    if (CountStockUnits(Station->Stock) + Reserved + NewOutputUnits > Station->StorageCapacityUnits)
        return false;

    // All input, station and output-capacity checks precede mutation.
    for (const TPair<FName, int64>& Need : Required)
        Station->Stock.FindOrAdd(Need.Key) -= static_cast<int32>(Need.Value);

    FWorldProductionJob Job;
    Job.JobId = FGuid::NewGuid();
    Job.RecipeId = RecipeId;
    Job.State = EProductionJobState::Queued;
    Station->Jobs.Add(Job);
    ++Station->Revision;
    OutJobId = Job.JobId;
    return OutJobId.IsValid();
}

bool UWorldProductionSubsystem::CanAdvanceProductionStep(double DeltaSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0)
        return false;
    for (const TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        const FWorldProductionStation& Station = Pair.Value;
        bool bHasWork = false;
        for (const FWorldProductionJob& Job : Station.Jobs)
        {
            if (Job.State != EProductionJobState::Queued && Job.State != EProductionJobState::Running)
                continue;
            bHasWork = true;
            const int32 RecipeIndex = FindRecipe(Job.RecipeId);
            if (!Recipes.IsValidIndex(RecipeIndex) ||
                !FMath::IsFinite(Recipes[RecipeIndex].DurationSeconds) ||
                Recipes[RecipeIndex].DurationSeconds <= 0.0 ||
                !FMath::IsFinite(Job.ProgressSeconds) ||
                !FMath::IsFinite(Job.ProgressSeconds + DeltaSeconds))
                return false;
        }
        if (bHasWork && Station.Revision == MAX_int64) return false;
    }
    return true;
}

bool UWorldProductionSubsystem::AdvanceProductionStep(double DeltaSeconds)
{
    if (!CanAdvanceProductionStep(DeltaSeconds)) return false;

    for (TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        FWorldProductionStation& Station = Pair.Value;
        bool bStationChanged = false;
        for (FWorldProductionJob& Job : Station.Jobs)
        {
            if (Job.State == EProductionJobState::Completed || Job.State == EProductionJobState::Blocked)
                continue;
            const int32 RecipeIndex = FindRecipe(Job.RecipeId);
            const FWorldProductionRecipe& Recipe = Recipes[RecipeIndex];
            if (Job.State == EProductionJobState::Queued)
                Job.State = EProductionJobState::Running;
            Job.ProgressSeconds = FMath::Min(Recipe.DurationSeconds, Job.ProgressSeconds + DeltaSeconds);
            bStationChanged = true;
            if (Job.ProgressSeconds < Recipe.DurationSeconds) break;

            TMap<FName, int32> ProposedStock = Station.Stock;
            bool bCanCommitOutputs = true;
            for (const FProductionIngredient& Output : Recipe.Outputs)
            {
                const int32 Current = ProposedStock.FindRef(Output.ItemId);
                if (Current > MAX_int32 - Output.Quantity) { bCanCommitOutputs = false; break; }
                ProposedStock.Add(Output.ItemId, Current + Output.Quantity);
            }
            if (!bCanCommitOutputs || CountStockUnits(ProposedStock) > Station.StorageCapacityUnits)
            {
                Job.State = EProductionJobState::Blocked;
                Job.BlockedReason = FName(TEXT("OutputCapacity"));
                break;
            }
            Station.Stock = MoveTemp(ProposedStock);
            Job.State = EProductionJobState::Completed;
            Job.BlockedReason = NAME_None;
            break; // one physical production line processes one job at a time
        }
        if (bStationChanged) ++Station.Revision;
    }
    return true;
}

bool UWorldProductionSubsystem::CollectCompletedJob(const FGuid& StationId, const FGuid& JobId)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision == MAX_int64) return false;
    const int32 Index = Station->Jobs.IndexOfByPredicate([JobId](const FWorldProductionJob& J)
        { return J.JobId == JobId; });
    if (!Station->Jobs.IsValidIndex(Index) ||
        Station->Jobs[Index].State != EProductionJobState::Completed) return false;
    Station->Jobs.RemoveAt(Index);
    ++Station->Revision;
    return true;
}

bool UWorldProductionSubsystem::GetStation(
    const FGuid& StationId, FWorldProductionStation& OutStation) const
{
    const FWorldProductionStation* Found = Stations.Find(StationId);
    if (!Found) return false;
    OutStation = *Found;
    return true;
}

void UWorldProductionSubsystem::ExportSnapshot(
    TArray<FWorldProductionStation>& OutStations, TArray<FWorldProductionRecipe>& OutRecipes) const
{
    Stations.GenerateValueArray(OutStations);
    OutStations.Sort([](const FWorldProductionStation& A, const FWorldProductionStation& B)
        { return A.StationId.ToString() < B.StationId.ToString(); });
    OutRecipes = Recipes;
    OutRecipes.Sort([](const FWorldProductionRecipe& A, const FWorldProductionRecipe& B)
        { return A.RecipeId.ToString() < B.RecipeId.ToString(); });
}

bool UWorldProductionSubsystem::RestoreSnapshot(
    const TArray<FWorldProductionStation>& StationSnapshot,
    const TArray<FWorldProductionRecipe>& RecipeSnapshot)
{
    if (StationSnapshot.Num() > MaxStations || RecipeSnapshot.Num() > MaxRecipes ||
        MaxStations <= 0 || MaxRecipes <= 0) return false;
    TArray<FWorldProductionRecipe> NewRecipes;
    TSet<FName> RecipeIds;
    for (const FWorldProductionRecipe& R : RecipeSnapshot)
    {
        if (R.RecipeId.IsNone() || RecipeIds.Contains(R.RecipeId) || R.Inputs.IsEmpty() ||
            R.Outputs.IsEmpty() || !FMath::IsFinite(R.DurationSeconds) || R.DurationSeconds <= 0.0)
            return false;
        for (const FProductionIngredient& I : R.Inputs)
            if (I.ItemId.IsNone() || I.Quantity <= 0) return false;
        for (const FProductionIngredient& O : R.Outputs)
            if (O.ItemId.IsNone() || O.Quantity <= 0) return false;
        RecipeIds.Add(R.RecipeId);
        NewRecipes.Add(R);
    }
    TMap<FGuid, FWorldProductionStation> NewStations;
    TSet<FGuid> IntakeOperationIds;
    TSet<FGuid> IntakeSourceItemIds;
    for (const FWorldProductionStation& S : StationSnapshot)
    {
        if (!S.StationId.IsValid() || S.StationTag.IsNone() || S.StorageCapacityUnits <= 0 ||
            S.Revision <= 0 || NewStations.Contains(S.StationId) ||
            CountStockUnits(S.Stock) > S.StorageCapacityUnits) return false;
        for (const FWorldProductionJob& J : S.Jobs)
            if (!J.JobId.IsValid() || !RecipeIds.Contains(J.RecipeId) ||
                !FMath::IsFinite(J.ProgressSeconds) || J.ProgressSeconds < 0.0) return false;
        for (const FWorldProductionPhysicalIntakeReceipt& Receipt : S.PhysicalIntakeReceipts)
        {
            if (!Receipt.OperationId.IsValid() || !Receipt.SourceItemId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 || Receipt.ExpectedItemRevision == MAX_int64 ||
                Receipt.ExpectedStationRevision <= 0 || Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.SourceDefinitionId.IsNone() || Receipt.StockItemId.IsNone() ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                IntakeOperationIds.Contains(Receipt.OperationId) ||
                IntakeSourceItemIds.Contains(Receipt.SourceItemId))
            {
                return false;
            }
            IntakeOperationIds.Add(Receipt.OperationId);
            IntakeSourceItemIds.Add(Receipt.SourceItemId);
        }
        NewStations.Add(S.StationId, S);
    }
    Recipes = MoveTemp(NewRecipes);
    Stations = MoveTemp(NewStations);
    return true;
}
