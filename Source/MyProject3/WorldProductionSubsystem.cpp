#include "WorldProductionSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "LogBuildPiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

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
        !Station.Jobs.IsEmpty() || !Station.PhysicalIntakeReceipts.IsEmpty() ||
        !Station.ItemIntakeReceipts.IsEmpty() || !Station.EnqueueReceipts.IsEmpty() || !Station.CollectReceipts.IsEmpty() ||
        !Station.MaterializationReceipts.IsEmpty())
        return false;
    for (const TPair<FName, int32>& Pair : Station.Stock)
        if (Pair.Key.IsNone() || Pair.Value < 0) return false;
    if (CountStockUnits(Station.Stock) > Station.StorageCapacityUnits) return false;
    Stations.Add(Station.StationId, Station);
    return true;
}

bool UWorldProductionSubsystem::ConfigureStationFireRequirement(
    const FGuid& StationId, bool bNeedsFire, bool bNeedsHotFire, bool bRequirementSatisfied)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || !StationId.IsValid())
        return false;

    const bool bEffectiveRequirement = (!bNeedsFire && !bNeedsHotFire) || bRequirementSatisfied;
    if (Station->bRequiresFire == bNeedsFire &&
        Station->bRequiresHotFire == bNeedsHotFire &&
        Station->bFireRequirementSatisfied == bEffectiveRequirement)
        return true;
    if (Station->Revision <= 0 || Station->Revision == MAX_int64)
        return false;

    Station->bRequiresFire = bNeedsFire;
    Station->bRequiresHotFire = bNeedsHotFire;
    Station->bFireRequirementSatisfied = bEffectiveRequirement;
    ++Station->Revision;
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

bool UWorldProductionSubsystem::TryIngestWorldOwnedItem(
    FGuid StationId, FGuid OperationId, FGuid ItemId,
    int64 ExpectedItemRevision, int64 ExpectedStationRevision, FName& OutStockItemId)
{
    OutStockItemId = NAME_None;
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!World || !Ownership || !StationId.IsValid() || !OperationId.IsValid() ||
        !ItemId.IsValid() || ExpectedItemRevision <= 0 || ExpectedItemRevision >= MAX_int64 - 1 ||
        ExpectedStationRevision <= 0 || ExpectedStationRevision >= MAX_int64 - 1)
        return false;

    // Cross-command idempotency: operation IDs cannot be reused between stock
    // intake, physical-log intake, queueing, collection or materialization.
    for (const TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        for (const FWorldProductionPhysicalIntakeReceipt& Receipt : Pair.Value.PhysicalIntakeReceipts)
        {
            if (Receipt.OperationId == OperationId || Receipt.SourceItemId == ItemId)
                return false;
        }
        for (const FWorldProductionItemIntakeReceipt& Receipt : Pair.Value.ItemIntakeReceipts)
        {
            if (Receipt.OperationId != OperationId)
            {
                if (Receipt.SourceItemId == ItemId)
                    return false;
                continue;
            }
            if (Pair.Key != StationId || Receipt.SourceItemId != ItemId ||
                Receipt.ExpectedItemRevision != ExpectedItemRevision ||
                Receipt.ExpectedStationRevision != ExpectedStationRevision)
                return false;
            OutStockItemId = Receipt.StockItemId;
            return !OutStockItemId.IsNone();
        }
        for (const FWorldProductionEnqueueReceipt& Receipt : Pair.Value.EnqueueReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionCollectReceipt& Receipt : Pair.Value.CollectReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionMaterializationReceipt& Receipt : Pair.Value.MaterializationReceipts)
            if (Receipt.OperationId == OperationId) return false;
    }

    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedStationRevision ||
        Station->Revision >= MAX_int64 - 1)
        return false;

    FWorldOwnedItemRecord Item;
    if (!Ownership->TryGetItem(ItemId, Item) || Item.ItemId != ItemId ||
        Item.OwnerKind != EWorldItemOwnerKind::World || Item.OwnerId.IsValid() ||
        Item.Revision != ExpectedItemRevision || Item.Revision >= MAX_int64 - 1 ||
        Item.Quantity != 1 || Item.DefinitionId.IsNone() ||
        !Item.MeshVariantSetId.IsNone())
        return false;

    // This API is for ledger-backed resources with no separate physical actor.
    // Logs and other actor-backed pieces must use their domain-specific intake.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed())
            continue;
        UWorldObjectIdentityComponent* Identity =
            Candidate->FindComponentByClass<UWorldObjectIdentityComponent>();
        if (Identity && Identity->HasValidIdentity() && Identity->GetInstanceId() == ItemId)
            return false;
    }

    const FName StockItemId = Item.DefinitionId;
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
        if (Job.State != EProductionJobState::Queued && Job.State != EProductionJobState::Running)
            continue;
        const int32 RecipeIndex = FindRecipe(Job.RecipeId);
        if (!Recipes.IsValidIndex(RecipeIndex))
            return false;
        for (const FProductionIngredient& Output : Recipes[RecipeIndex].Outputs)
        {
            if (Output.Quantity <= 0 || ReservedOutputUnits > MAX_int64 - Output.Quantity)
                return false;
            ReservedOutputUnits += Output.Quantity;
        }
    }
    if (ProposedUnits > Station->StorageCapacityUnits ||
        ReservedOutputUnits > Station->StorageCapacityUnits - ProposedUnits)
        return false;

    // Everything that can reject the request has been checked before ownership
    // mutation. After the transfer succeeds, stock and its durable receipt are
    // committed without a further validation branch.
    FWorldOwnedItemRecord TransformedItem;
    if (!Ownership->TryTransferItem(OperationId, ItemId, ExpectedItemRevision,
        EWorldItemOwnerKind::World, EWorldItemOwnerKind::Transformed,
        FGuid(), Item.WorldLocation, TransformedItem))
        return false;

    FWorldProductionItemIntakeReceipt& Receipt = Station->ItemIntakeReceipts.AddDefaulted_GetRef();
    Receipt.OperationId = OperationId;
    Receipt.SourceItemId = ItemId;
    Receipt.ExpectedItemRevision = ExpectedItemRevision;
    Receipt.ExpectedStationRevision = ExpectedStationRevision;
    Receipt.SourceDefinitionId = Item.DefinitionId;
    Receipt.StockItemId = StockItemId;
    Receipt.ResultingItemRevision = TransformedItem.Revision;
    Receipt.ResultingStationRevision = ExpectedStationRevision + 1;
    Station->Stock = MoveTemp(ProposedStock);
    ++Station->Revision;
    OutStockItemId = StockItemId;
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
        // A production operation ID may not be reused across intake, enqueue
        // and collection command kinds.
        for (const FWorldProductionEnqueueReceipt& Enqueue : Pair.Value.EnqueueReceipts)
        {
            if (Enqueue.OperationId == OperationId)
                return false;
        }
        for (const FWorldProductionCollectReceipt& Collect : Pair.Value.CollectReceipts)
        {
            if (Collect.OperationId == OperationId)
                return false;
        }
        for (const FWorldProductionMaterializationReceipt& Materialization : Pair.Value.MaterializationReceipts)
        {
            if (Materialization.OperationId == OperationId)
                return false;
        }
        for (const FWorldProductionItemIntakeReceipt& Intake : Pair.Value.ItemIntakeReceipts)
        {
            if (Intake.OperationId == OperationId || Intake.SourceItemId == ItemId)
                return false;
        }
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

bool UWorldProductionSubsystem::TryMaterializeStockAsWorldLog(
    FGuid StationId,
    FGuid OperationId,
    FName StockItemId,
    FName DefinitionId,
    FName BaseDefinitionId,
    FName MeshVariantSetId,
    int64 ExpectedStationRevision,
    const FTransform& SpawnTransform,
    TSubclassOf<ALogBuildPiece> LogActorClass,
    FGuid& OutWorldItemId)
{
    OutWorldItemId.Invalidate();
    const FVector Location = SpawnTransform.GetLocation();
    const FVector Scale = SpawnTransform.GetScale3D();
    const FQuat Rotation = SpawnTransform.GetRotation();
    if (!GetWorld() || !StationId.IsValid() || !OperationId.IsValid() ||
        StockItemId.IsNone() || DefinitionId.IsNone() || BaseDefinitionId.IsNone() ||
        ExpectedStationRevision <= 0 || ExpectedStationRevision == MAX_int64 ||
        !LogActorClass || !LogActorClass->IsChildOf(ALogBuildPiece::StaticClass()) ||
        MakePhysicalStockItemId(DefinitionId, MeshVariantSetId) != StockItemId ||
        !FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) ||
        !FMath::IsFinite(Location.Z) || !FMath::IsFinite(Scale.X) ||
        !FMath::IsFinite(Scale.Y) || !FMath::IsFinite(Scale.Z) ||
        FMath::IsNearlyZero(Scale.X) || FMath::IsNearlyZero(Scale.Y) ||
        FMath::IsNearlyZero(Scale.Z) || !FMath::IsFinite(Rotation.X) ||
        !FMath::IsFinite(Rotation.Y) || !FMath::IsFinite(Rotation.Z) ||
        !FMath::IsFinite(Rotation.W) || !Rotation.IsNormalized())
    {
        return false;
    }

    // Operation IDs are unique across intake, enqueue, collect and materialize.
    for (const TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        for (const FWorldProductionPhysicalIntakeReceipt& Receipt : Pair.Value.PhysicalIntakeReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionItemIntakeReceipt& Receipt : Pair.Value.ItemIntakeReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionEnqueueReceipt& Receipt : Pair.Value.EnqueueReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionCollectReceipt& Receipt : Pair.Value.CollectReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionMaterializationReceipt& Receipt : Pair.Value.MaterializationReceipts)
        {
            if (Receipt.OperationId != OperationId)
                continue;
            if (Pair.Key != StationId || Receipt.StockItemId != StockItemId ||
                Receipt.DefinitionId != DefinitionId || Receipt.BaseDefinitionId != BaseDefinitionId ||
                Receipt.MeshVariantSetId != MeshVariantSetId ||
                Receipt.ExpectedStationRevision != ExpectedStationRevision)
            {
                return false;
            }
            OutWorldItemId = Receipt.WorldItemId;
            return OutWorldItemId.IsValid();
        }
    }

    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedStationRevision ||
        Station->Revision == MAX_int64 || Station->Stock.FindRef(StockItemId) <= 0)
    {
        return false;
    }

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (!Ownership || !Graph)
        return false;

    FGuid NewItemId;
    bool bUniqueItemId = false;
    for (int32 Attempt = 0; Attempt < 8 && !bUniqueItemId; ++Attempt)
    {
        NewItemId = FGuid::NewGuid();
        FWorldOwnedItemRecord Existing;
        bUniqueItemId = NewItemId.IsValid() && !Ownership->TryGetItem(NewItemId, Existing);
    }
    if (!bUniqueItemId)
        return false;

    ALogBuildPiece* Piece = GetWorld()->SpawnActorDeferred<ALogBuildPiece>(
        LogActorClass, SpawnTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Piece))
        return false;

    Piece->SetActorHiddenInGame(true);
    Piece->SetActorEnableCollision(false);
    Piece->bStartsPlaced = false;
    Piece->BaseDefinitionId = BaseDefinitionId;
    Piece->SelectedProcessingMeshSetId = MeshVariantSetId;

    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    if (!Identity)
    {
        Piece->Destroy();
        return false;
    }

    Identity->InstanceId = NewItemId;
    Identity->SetDefinitionId(DefinitionId);
    Piece->FinishSpawning(SpawnTransform);
    if (!IsValid(Piece))
        return false;

    // Keep the output inert until the stock decrement and receipt are committed.
    Piece->SetActorHiddenInGame(true);
    Piece->SetActorEnableCollision(false);

    auto RollbackOutputActor = [Piece, Identity, Ownership, NewItemId]()
    {
        if (!IsValid(Piece))
            return;
        Piece->SetActorHiddenInGame(true);
        Piece->SetActorEnableCollision(false);
        if (UStaticMeshComponent* Mesh = Piece->GetLogMesh())
            Mesh->SetSimulatePhysics(false);

        FWorldOwnedItemRecord Current;
        if (Identity && Ownership && Ownership->TryGetItem(NewItemId, Current) &&
            Current.OwnerKind == EWorldItemOwnerKind::World &&
            Current.Revision < MAX_int64)
        {
            FWorldOwnedItemRecord Consumed;
            Ownership->TryTransferItem(FGuid::NewGuid(), NewItemId, Current.Revision,
                EWorldItemOwnerKind::World, EWorldItemOwnerKind::Consumed,
                FGuid(), Piece->GetActorLocation(), Consumed);
        }
        Piece->Destroy();
    };

    FWorldOwnedItemRecord RegisteredItem;
    FStructuralNodeRecord RegisteredNode;
    if (!Identity->HasValidIdentity() || Identity->GetInstanceId() != NewItemId ||
        !Ownership->TryGetItem(NewItemId, RegisteredItem) ||
        RegisteredItem.OwnerKind != EWorldItemOwnerKind::World ||
        RegisteredItem.Quantity != 1 || RegisteredItem.DefinitionId != DefinitionId ||
        RegisteredItem.BaseDefinitionId != BaseDefinitionId ||
        RegisteredItem.MeshVariantSetId != MeshVariantSetId ||
        !Graph->GetNode(NewItemId, RegisteredNode) ||
        RegisteredNode.Actor.Get() != Piece ||
        !IsValid(Piece->GetLogMesh()) || !Piece->GetLogMesh()->GetStaticMesh())
    {
        RollbackOutputActor();
        return false;
    }

    // Apply the definition only after BeginPlay has registered the item/node.
    // Missing variant tuples fail closed and leave station stock untouched.
    if (!Piece->ApplyDefinitionVariant(DefinitionId) ||
        !IsValid(Piece->GetLogMesh()) || !Piece->GetLogMesh()->GetStaticMesh())
    {
        RollbackOutputActor();
        return false;
    }

    // Applying a shorter/cut mesh may adjust actor Z to preserve its contact
    // plane. Keep the ownership ledger location aligned with the final transform.
    if (!RegisteredItem.WorldLocation.Equals(Piece->GetActorLocation(), 0.1f))
    {
        FWorldOwnedItemRecord LocatedItem;
        if (!Ownership->TryTransferItem(FGuid::NewGuid(), NewItemId,
            RegisteredItem.Revision, EWorldItemOwnerKind::World,
            EWorldItemOwnerKind::World, FGuid(), Piece->GetActorLocation(), LocatedItem))
        {
            RollbackOutputActor();
            return false;
        }
        RegisteredItem = LocatedItem;
    }

    // FinishSpawning may run Blueprint logic. Recheck the station revision and
    // stock count before commit so a re-entrant change cannot consume stale stock.
    Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedStationRevision ||
        Station->Revision == MAX_int64 || Station->Stock.FindRef(StockItemId) <= 0)
    {
        RollbackOutputActor();
        return false;
    }

    TMap<FName, int32> ProposedStock = Station->Stock;
    const int32 Remaining = ProposedStock.FindRef(StockItemId) - 1;
    ProposedStock.Add(StockItemId, Remaining);
    if (CountStockUnits(ProposedStock) == MAX_int64)
    {
        RollbackOutputActor();
        return false;
    }

    // All failure-prone actor/mesh/graph checks have completed. Stock decrement,
    // durable receipt, and activation are committed in this synchronous call.
    Station->Stock = MoveTemp(ProposedStock);
    ++Station->Revision;

    FWorldProductionMaterializationReceipt& Receipt =
        Station->MaterializationReceipts.AddDefaulted_GetRef();
    Receipt.OperationId = OperationId;
    Receipt.StockItemId = StockItemId;
    Receipt.DefinitionId = DefinitionId;
    Receipt.BaseDefinitionId = BaseDefinitionId;
    Receipt.MeshVariantSetId = MeshVariantSetId;
    Receipt.WorldItemId = NewItemId;
    Receipt.ExpectedStationRevision = ExpectedStationRevision;
    Receipt.ResultingStationRevision = Station->Revision;

    if (UStaticMeshComponent* Mesh = Piece->GetLogMesh())
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
        Mesh->WakeAllRigidBodies();
    }
    Piece->SetActorEnableCollision(true);
    Piece->SetActorHiddenInGame(false);

    OutWorldItemId = NewItemId;
    return true;
}

bool UWorldProductionSubsystem::TryEnqueueJob(
    FGuid OperationId,
    FGuid StationId,
    FName RecipeId,
    int64 ExpectedStationRevision,
    FGuid& OutJobId)
{
    OutJobId.Invalidate();
    if (!OperationId.IsValid() || !StationId.IsValid() || RecipeId.IsNone() ||
        ExpectedStationRevision <= 0 || ExpectedStationRevision == MAX_int64)
    {
        return false;
    }

    // Operation IDs are shared across the production intake/enqueue/collect
    // commands. Same-request retries replay the committed result.
    for (const TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        for (const FWorldProductionPhysicalIntakeReceipt& Intake : Pair.Value.PhysicalIntakeReceipts)
        {
            if (Intake.OperationId == OperationId)
                return false;
        }
        for (const FWorldProductionItemIntakeReceipt& Intake : Pair.Value.ItemIntakeReceipts)
            if (Intake.OperationId == OperationId) return false;
        for (const FWorldProductionEnqueueReceipt& Receipt : Pair.Value.EnqueueReceipts)
        {
            if (Receipt.OperationId != OperationId)
                continue;
            if (Pair.Key != StationId || Receipt.RecipeId != RecipeId ||
                Receipt.ExpectedStationRevision != ExpectedStationRevision)
            {
                return false;
            }
            OutJobId = Receipt.JobId;
            return OutJobId.IsValid();
        }
        for (const FWorldProductionCollectReceipt& Receipt : Pair.Value.CollectReceipts)
        {
            if (Receipt.OperationId == OperationId)
                return false;
        }
        for (const FWorldProductionMaterializationReceipt& Receipt : Pair.Value.MaterializationReceipts)
        {
            if (Receipt.OperationId == OperationId)
                return false;
        }
    }

    FWorldProductionStation* Station = Stations.Find(StationId);
    const int32 RecipeIndex = FindRecipe(RecipeId);
    if (!Station || Station->Revision != ExpectedStationRevision ||
        Station->Revision == MAX_int64 || !Recipes.IsValidIndex(RecipeIndex) ||
        MaxJobsPerStation <= 0 || Station->Jobs.Num() >= MaxJobsPerStation)
    {
        return false;
    }

    const FWorldProductionRecipe& Recipe = Recipes[RecipeIndex];
    if (!Recipe.RequiredStationTag.IsNone() &&
        Recipe.RequiredStationTag != Station->StationTag)
    {
        return false;
    }

    TMap<FName, int64> Required;
    for (const FProductionIngredient& Input : Recipe.Inputs)
    {
        if (Input.ItemId.IsNone() || Input.Quantity <= 0)
            return false;
        int64& Total = Required.FindOrAdd(Input.ItemId);
        if (Total > MAX_int64 - Input.Quantity)
            return false;
        Total += Input.Quantity;
    }

    TMap<FName, int32> ProposedStock = Station->Stock;
    for (const TPair<FName, int64>& Need : Required)
    {
        const int32 Available = ProposedStock.FindRef(Need.Key);
        if (Need.Value > MAX_int32 || Available < Need.Value)
            return false;
        ProposedStock.Add(Need.Key, Available - static_cast<int32>(Need.Value));
    }

    int64 ReservedOutputUnits = 0;
    for (const FWorldProductionJob& Job : Station->Jobs)
    {
        if (Job.State != EProductionJobState::Queued &&
            Job.State != EProductionJobState::Running)
        {
            continue;
        }

        const int32 ExistingRecipeIndex = FindRecipe(Job.RecipeId);
        if (!Recipes.IsValidIndex(ExistingRecipeIndex))
            return false;

        for (const FProductionIngredient& Output : Recipes[ExistingRecipeIndex].Outputs)
        {
            if (Output.ItemId.IsNone() || Output.Quantity <= 0 ||
                ReservedOutputUnits > MAX_int64 - Output.Quantity)
            {
                return false;
            }
            ReservedOutputUnits += Output.Quantity;
        }
    }

    int64 NewOutputUnits = 0;
    for (const FProductionIngredient& Output : Recipe.Outputs)
    {
        if (Output.ItemId.IsNone() || Output.Quantity <= 0 ||
            NewOutputUnits > MAX_int64 - Output.Quantity)
        {
            return false;
        }
        NewOutputUnits += Output.Quantity;
    }

    const int64 RemainingStockUnits = CountStockUnits(ProposedStock);
    if (RemainingStockUnits == MAX_int64 ||
        RemainingStockUnits > Station->StorageCapacityUnits ||
        ReservedOutputUnits > Station->StorageCapacityUnits - RemainingStockUnits ||
        NewOutputUnits > Station->StorageCapacityUnits -
            RemainingStockUnits - ReservedOutputUnits)
    {
        return false;
    }

    FGuid NewJobId;
    bool bUniqueJobId = false;
    for (int32 Attempt = 0; Attempt < 8 && !bUniqueJobId; ++Attempt)
    {
        NewJobId = FGuid::NewGuid();
        bUniqueJobId = NewJobId.IsValid() &&
            !Station->Jobs.ContainsByPredicate([NewJobId](const FWorldProductionJob& Job)
                { return Job.JobId == NewJobId; });
        for (const FWorldProductionEnqueueReceipt& Receipt : Station->EnqueueReceipts)
        {
            if (Receipt.JobId == NewJobId)
                bUniqueJobId = false;
        }
    }
    if (!bUniqueJobId)
        return false;

    // Every rejection check is complete before stock/job/receipt state changes.
    FWorldProductionJob Job;
    Job.JobId = NewJobId;
    Job.RecipeId = RecipeId;
    Job.State = EProductionJobState::Queued;
    Job.ProgressSeconds = 0.0;
    Job.BlockedReason = NAME_None;

    Station->Stock = MoveTemp(ProposedStock);
    Station->Jobs.Add(Job);
    ++Station->Revision;

    FWorldProductionEnqueueReceipt& Receipt =
        Station->EnqueueReceipts.AddDefaulted_GetRef();
    Receipt.OperationId = OperationId;
    Receipt.RecipeId = RecipeId;
    Receipt.JobId = NewJobId;
    Receipt.ExpectedStationRevision = ExpectedStationRevision;
    Receipt.ResultingStationRevision = Station->Revision;
    OutJobId = NewJobId;
    return true;
}

bool UWorldProductionSubsystem::EnqueueJob(
    const FGuid& StationId, FName RecipeId, FGuid& OutJobId)
{
    const FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station)
    {
        OutJobId.Invalidate();
        return false;
    }

    // Compatibility wrapper for existing Blueprint callers. New transactional
    // callers should supply a stable OperationId to TryEnqueueJob explicitly.
    return TryEnqueueJob(FGuid::NewGuid(), StationId, RecipeId,
        Station->Revision, OutJobId);
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
        const bool bFireRequired = Station.bRequiresFire || Station.bRequiresHotFire;
        if (bFireRequired && !Station.bFireRequirementSatisfied)
        {
            for (FWorldProductionJob& Job : Station.Jobs)
            {
                if ((Job.State == EProductionJobState::Queued || Job.State == EProductionJobState::Running) &&
                    Job.BlockedReason != TEXT("NeedsFire"))
                {
                    Job.BlockedReason = TEXT("NeedsFire");
                    bStationChanged = true;
                }
            }
            if (bStationChanged)
                ++Station.Revision;
            continue; // Keep queued/running progress; the same job resumes once its fire returns.
        }

        for (FWorldProductionJob& Job : Station.Jobs)
        {
            if (Job.State == EProductionJobState::Completed || Job.State == EProductionJobState::Blocked)
                continue;
            if (Job.BlockedReason == TEXT("NeedsFire"))
            {
                Job.BlockedReason = NAME_None;
                bStationChanged = true;
            }
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

bool UWorldProductionSubsystem::TryCollectCompletedJob(
    FGuid OperationId,
    FGuid StationId,
    FGuid JobId,
    int64 ExpectedStationRevision)
{
    if (!OperationId.IsValid() || !StationId.IsValid() || !JobId.IsValid() ||
        ExpectedStationRevision <= 0 || ExpectedStationRevision == MAX_int64)
    {
        return false;
    }

    for (const TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        for (const FWorldProductionPhysicalIntakeReceipt& Receipt : Pair.Value.PhysicalIntakeReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionItemIntakeReceipt& Receipt : Pair.Value.ItemIntakeReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionEnqueueReceipt& Receipt : Pair.Value.EnqueueReceipts)
            if (Receipt.OperationId == OperationId) return false;
        for (const FWorldProductionCollectReceipt& Receipt : Pair.Value.CollectReceipts)
        {
            if (Receipt.OperationId != OperationId)
                continue;
            return Pair.Key == StationId && Receipt.JobId == JobId &&
                Receipt.ExpectedStationRevision == ExpectedStationRevision;
        }
        for (const FWorldProductionMaterializationReceipt& Receipt : Pair.Value.MaterializationReceipts)
        {
            if (Receipt.OperationId == OperationId)
                return false;
        }
    }

    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedStationRevision ||
        Station->Revision == MAX_int64)
    {
        return false;
    }

    const int32 Index = Station->Jobs.IndexOfByPredicate([JobId](const FWorldProductionJob& Job)
        { return Job.JobId == JobId; });
    if (!Station->Jobs.IsValidIndex(Index) ||
        Station->Jobs[Index].State != EProductionJobState::Completed)
    {
        return false;
    }

    // Removal and durable receipt commit together; the completed output remains
    // in station stock and a retry won't attempt a second removal.
    Station->Jobs.RemoveAt(Index);
    ++Station->Revision;
    FWorldProductionCollectReceipt& Receipt =
        Station->CollectReceipts.AddDefaulted_GetRef();
    Receipt.OperationId = OperationId;
    Receipt.JobId = JobId;
    Receipt.ExpectedStationRevision = ExpectedStationRevision;
    Receipt.ResultingStationRevision = Station->Revision;
    return true;
}

bool UWorldProductionSubsystem::CollectCompletedJob(
    const FGuid& StationId, const FGuid& JobId)
{
    const FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station)
        return false;

    // Compatibility wrapper; use the operation-ID API when retries may occur.
    return TryCollectCompletedJob(FGuid::NewGuid(), StationId, JobId, Station->Revision);
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
    auto FindSnapshotRecipe = [&NewRecipes](FName RecipeId) -> const FWorldProductionRecipe*
    {
        const int32 Index = NewRecipes.IndexOfByPredicate([RecipeId](const FWorldProductionRecipe& Recipe)
            { return Recipe.RecipeId == RecipeId; });
        return NewRecipes.IsValidIndex(Index) ? &NewRecipes[Index] : nullptr;
    };
    TMap<FGuid, FWorldProductionStation> NewStations;
    TSet<FGuid> ProductionOperationIds;
    TSet<FGuid> IntakeSourceItemIds;
    TSet<FGuid> EnqueuedJobIds;
    TSet<FGuid> SnapshotJobIds;
    TSet<FGuid> MaterializedWorldItemIds;
    for (const FWorldProductionStation& S : StationSnapshot)
    {
        const int64 StockUnits = CountStockUnits(S.Stock);
        if (!S.StationId.IsValid() || S.StationTag.IsNone() || S.StorageCapacityUnits <= 0 ||
            S.Revision <= 0 || NewStations.Contains(S.StationId) ||
            StockUnits == MAX_int64 || StockUnits > S.StorageCapacityUnits)
        {
            return false;
        }

        int64 ReservedOutputUnits = 0;
        TSet<FGuid> StationJobIds;
        for (const FWorldProductionJob& J : S.Jobs)
        {
            const FWorldProductionRecipe* Recipe = FindSnapshotRecipe(J.RecipeId);
            if (!J.JobId.IsValid() || StationJobIds.Contains(J.JobId) ||
                SnapshotJobIds.Contains(J.JobId) || !Recipe ||
                static_cast<uint8>(J.State) > static_cast<uint8>(EProductionJobState::Blocked) ||
                !FMath::IsFinite(J.ProgressSeconds) || J.ProgressSeconds < 0.0 ||
                J.ProgressSeconds > Recipe->DurationSeconds)
            {
                return false;
            }
            StationJobIds.Add(J.JobId);
            SnapshotJobIds.Add(J.JobId);

            if (J.State == EProductionJobState::Queued || J.State == EProductionJobState::Running)
            {
                for (const FProductionIngredient& Output : Recipe->Outputs)
                {
                    if (ReservedOutputUnits > MAX_int64 - Output.Quantity)
                        return false;
                    ReservedOutputUnits += Output.Quantity;
                }
            }
        }
        if (ReservedOutputUnits > S.StorageCapacityUnits - StockUnits)
            return false;

        for (const FWorldProductionPhysicalIntakeReceipt& Receipt : S.PhysicalIntakeReceipts)
        {
            if (!Receipt.OperationId.IsValid() || !Receipt.SourceItemId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 || Receipt.ExpectedItemRevision == MAX_int64 ||
                Receipt.ExpectedStationRevision <= 0 || Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.SourceDefinitionId.IsNone() || Receipt.StockItemId.IsNone() ||
                Receipt.StockItemId != MakePhysicalStockItemId(
                    Receipt.SourceDefinitionId, Receipt.SourceMeshVariantSetId) ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionOperationIds.Contains(Receipt.OperationId) ||
                IntakeSourceItemIds.Contains(Receipt.SourceItemId))
            {
                return false;
            }
            ProductionOperationIds.Add(Receipt.OperationId);
            IntakeSourceItemIds.Add(Receipt.SourceItemId);
        }
        for (const FWorldProductionItemIntakeReceipt& Receipt : S.ItemIntakeReceipts)
        {
            if (!Receipt.OperationId.IsValid() || !Receipt.SourceItemId.IsValid() ||
                Receipt.ExpectedItemRevision <= 0 || Receipt.ExpectedItemRevision >= MAX_int64 - 1 ||
                Receipt.ExpectedStationRevision <= 0 || Receipt.ExpectedStationRevision >= MAX_int64 - 1 ||
                Receipt.SourceDefinitionId.IsNone() || Receipt.StockItemId != Receipt.SourceDefinitionId ||
                Receipt.ResultingItemRevision != Receipt.ExpectedItemRevision + 1 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionOperationIds.Contains(Receipt.OperationId) ||
                IntakeSourceItemIds.Contains(Receipt.SourceItemId))
                return false;

            UWorldItemOwnershipSubsystem* Ownership =
                GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
            FWorldOwnedItemRecord SourceItem;
            if (!Ownership || !Ownership->TryGetItem(Receipt.SourceItemId, SourceItem))
                return false;

            ProductionOperationIds.Add(Receipt.OperationId);
            IntakeSourceItemIds.Add(Receipt.SourceItemId);
        }
        for (const FWorldProductionEnqueueReceipt& Receipt : S.EnqueueReceipts)
        {
            if (!Receipt.OperationId.IsValid() || Receipt.RecipeId.IsNone() ||
                !RecipeIds.Contains(Receipt.RecipeId) || !Receipt.JobId.IsValid() ||
                Receipt.ExpectedStationRevision <= 0 ||
                Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionOperationIds.Contains(Receipt.OperationId) ||
                EnqueuedJobIds.Contains(Receipt.JobId))
            {
                return false;
            }
            ProductionOperationIds.Add(Receipt.OperationId);
            EnqueuedJobIds.Add(Receipt.JobId);
        }
        for (const FWorldProductionCollectReceipt& Receipt : S.CollectReceipts)
        {
            if (!Receipt.OperationId.IsValid() || !Receipt.JobId.IsValid() ||
                Receipt.ExpectedStationRevision <= 0 ||
                Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionOperationIds.Contains(Receipt.OperationId) ||
                S.Jobs.ContainsByPredicate([&Receipt](const FWorldProductionJob& Job)
                    { return Job.JobId == Receipt.JobId; }))
            {
                return false;
            }
            ProductionOperationIds.Add(Receipt.OperationId);
        }
        for (const FWorldProductionMaterializationReceipt& Receipt : S.MaterializationReceipts)
        {
            if (!Receipt.OperationId.IsValid() || Receipt.StockItemId.IsNone() ||
                Receipt.DefinitionId.IsNone() || Receipt.BaseDefinitionId.IsNone() ||
                !Receipt.WorldItemId.IsValid() ||
                Receipt.StockItemId != MakePhysicalStockItemId(
                    Receipt.DefinitionId, Receipt.MeshVariantSetId) ||
                Receipt.ExpectedStationRevision <= 0 ||
                Receipt.ExpectedStationRevision == MAX_int64 ||
                Receipt.ResultingStationRevision != Receipt.ExpectedStationRevision + 1 ||
                ProductionOperationIds.Contains(Receipt.OperationId) ||
                MaterializedWorldItemIds.Contains(Receipt.WorldItemId) ||
                IntakeSourceItemIds.Contains(Receipt.WorldItemId))
            {
                return false;
            }
            ProductionOperationIds.Add(Receipt.OperationId);
            MaterializedWorldItemIds.Add(Receipt.WorldItemId);
        }
        NewStations.Add(S.StationId, S);
    }
    Recipes = MoveTemp(NewRecipes);
    Stations = MoveTemp(NewStations);
    return true;
}

bool UWorldProductionSubsystem::TryTakeStock(
    const FGuid& StationId, FName ItemId, int32 Quantity, int64 ExpectedRevision)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedRevision || Station->Revision == MAX_int64 ||
        ItemId.IsNone() || Quantity <= 0)
        return false;
    const int32 Existing = Station->Stock.FindRef(ItemId);
    if (Existing < Quantity)
        return false;
    Existing == Quantity ? (void)Station->Stock.Remove(ItemId) : (void)Station->Stock.Add(ItemId, Existing - Quantity);
    ++Station->Revision;
    return true;
}
