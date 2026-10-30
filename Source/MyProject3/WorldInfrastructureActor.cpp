#include "WorldInfrastructureActor.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "InventoryCraftingComponent.h"
#include "WorldGameDataSubsystem.h"
#include "WorldLogisticsRouteSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"

AWorldInfrastructureActor::AWorldInfrastructureActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

bool AWorldInfrastructureActor::FindInventory(
    const FGuid& OwnerId, UInventoryCraftingComponent*& OutInventory) const
{
    OutInventory = nullptr;
    UWorld* World = GetWorld();
    if (!World || !OwnerId.IsValid())
        return false;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        UInventoryCraftingComponent* Candidate =
            IsValid(*It) ? (*It)->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
        if (!IsValid(Candidate) || Candidate->GetInventoryOwnerId() != OwnerId)
            continue;
        if (OutInventory)
            return false;
        OutInventory = Candidate;
    }
    return OutInventory != nullptr;
}

bool AWorldInfrastructureActor::ConsumeMaterialsAtomically(
    UInventoryCraftingComponent* Inventory, const FGuid& OwnerId,
    const TArray<FGameDataItemAmount>& Materials, const TArray<FName>& RequiredTools)
{
    if (!IsValid(Inventory) || !OwnerId.IsValid() ||
        Inventory->GetInventoryOwnerId() != OwnerId || Materials.IsEmpty())
        return false;

    for (const FName ToolId : RequiredTools)
        if (ToolId.IsNone() || Inventory->GetItemCount(ToolId) <= 0)
            return false;

    FGuid OwnerActorId;
    FGuid PreviousOwnerId;
    TArray<FInventoryStack> PreviousStacks;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousOwnerId, PreviousStacks, PreviousQuickAccess);
    if (PreviousOwnerId != OwnerId)
        return false;

    for (const FGameDataItemAmount& Cost : Materials)
    {
        int32 Available = 0;
        if (Cost.ItemId.IsNone() || Cost.Quantity <= 0)
            return false;
        for (const FInventoryStack& Stack : PreviousStacks)
        {
            if (Stack.ItemId != Cost.ItemId)
                continue;
            // Unique ledger-owned objects need their own ownership transaction.
            if (Stack.OwnedWorldItemId.IsValid())
                return false;
            Available += Stack.Quantity;
        }
        if (Available < Cost.Quantity)
            return false;
    }

    auto Rollback = [&]() -> bool
    {
        return Inventory->RestoreSnapshot(OwnerId, PreviousStacks, PreviousQuickAccess);
    };
    for (const FGameDataItemAmount& Cost : Materials)
        if (!Inventory->TryRemoveItem(Cost.ItemId, Cost.Quantity))
        {
            if (!Rollback())
                if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                    GetWorld() ? GetWorld()->GetSubsystem<UWorldSimulationCoordinatorSubsystem>() : nullptr)
                    Coordinator->MarkSimulationFaulted(
                        TEXT("Infrastructure material rollback failed; stop simulation and recover/reset the world."));
            return false;
        }
    return true;
}

bool AWorldInfrastructureActor::HasCompletedOperation(const FGuid& OperationId) const
{
    return OperationId.IsValid() && CompletedOperationIds.Contains(OperationId);
}

void AWorldInfrastructureActor::RecordCompletedOperation(const FGuid& OperationId)
{
    if (!OperationId.IsValid() || CompletedOperationIds.Contains(OperationId))
        return;
    CompletedOperationIds.Add(OperationId);
    while (CompletedOperationIds.Num() > FMath::Clamp(MaxOperationReceipts, 1, 64))
        CompletedOperationIds.RemoveAt(0, 1, EAllowShrinking::No);
}

bool AWorldInfrastructureActor::BuildFromInventory(
    const FGuid& OperationId, const FGuid& InventoryOwnerId)
{
    if (!OperationId.IsValid() || InfrastructureId.IsNone() ||
        !InventoryOwnerId.IsValid() || MaxOperationReceipts <= 0 || MaxOperationReceipts > 64)
        return false;
    if (HasCompletedOperation(OperationId))
        return IsInfrastructureBuilt();

    UWorld* World = GetWorld();
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldLogisticsRouteSubsystem* Routes =
        World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    const FGameDataInfrastructure* Definition =
        GameData ? GameData->FindInfrastructure(InfrastructureId) : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    if (!World || !GameData || !GameData->IsLoaded() || !Definition ||
        (bAffectsRouteGraph && !Routes) || !FindInventory(InventoryOwnerId, Inventory))
        return false;
    if (IsInfrastructureBuilt())
        return false;

    TArray<FLogisticsRouteNode> PreviousNodes;
    TArray<FLogisticsRouteEdge> PreviousEdges;
    if (bAffectsRouteGraph)
        Routes->ExportSnapshot(PreviousNodes, PreviousEdges);

    FGuid OwnerActorId;
    FGuid PreviousOwnerId;
    TArray<FInventoryStack> PreviousStacks;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousOwnerId, PreviousStacks, PreviousQuickAccess);
    if (PreviousOwnerId != InventoryOwnerId ||
        !ConsumeMaterialsAtomically(Inventory, InventoryOwnerId,
            Definition->BuildMaterials, Definition->Tools))
        return false;

    if (bAffectsRouteGraph &&
        (!Routes->SetInfrastructureBuilt(InfrastructureId, true) ||
         !Routes->SetInfrastructureBuiltCostMultiplier(
             InfrastructureId, Definition->BuiltRouteCostMultiplier)))
    {
        const bool bInventoryRestored =
            Inventory->RestoreSnapshot(InventoryOwnerId, PreviousStacks, PreviousQuickAccess);
        const bool bRoutesRestored = Routes->RestoreSnapshot(PreviousNodes, PreviousEdges);
        if (!bInventoryRestored || !bRoutesRestored)
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(
                    TEXT("Infrastructure construction rollback failed; stop simulation and recover/reset the world."));
        return false;
    }

    bConstructed = true;
    LocalCondition = Definition->MaxCondition;
    RecordCompletedOperation(OperationId);
    return true;
}

bool AWorldInfrastructureActor::RepairFromInventory(
    const FGuid& OperationId, const FGuid& InventoryOwnerId)
{
    if (!OperationId.IsValid() || InfrastructureId.IsNone() ||
        !InventoryOwnerId.IsValid() || MaxOperationReceipts <= 0 || MaxOperationReceipts > 64)
        return false;
    if (HasCompletedOperation(OperationId))
        return true;

    UWorld* World = GetWorld();
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldLogisticsRouteSubsystem* Routes =
        World ? World->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
    const FGameDataInfrastructure* Definition =
        GameData ? GameData->FindInfrastructure(InfrastructureId) : nullptr;
    UInventoryCraftingComponent* Inventory = nullptr;
    if (!World || !GameData || !GameData->IsLoaded() || !Definition ||
        (bAffectsRouteGraph && !Routes) || !FindInventory(InventoryOwnerId, Inventory))
        return false;

    float CurrentCondition = LocalCondition;
    float MaxCondition = Definition->MaxCondition;
    bool bRouteBuilt = bConstructed;
    if (bAffectsRouteGraph &&
        !Routes->GetInfrastructureState(InfrastructureId, CurrentCondition, MaxCondition, bRouteBuilt))
        return false;
    if (CurrentCondition >= MaxCondition || Definition->RepairConditionPerJob <= 0.0f)
        return false;

    TArray<FLogisticsRouteNode> PreviousNodes;
    TArray<FLogisticsRouteEdge> PreviousEdges;
    if (bAffectsRouteGraph)
        Routes->ExportSnapshot(PreviousNodes, PreviousEdges);

    FGuid OwnerActorId;
    FGuid PreviousOwnerId;
    TArray<FInventoryStack> PreviousStacks;
    FName PreviousQuickAccess = NAME_None;
    Inventory->ExportSnapshot(OwnerActorId, PreviousOwnerId, PreviousStacks, PreviousQuickAccess);
    if (PreviousOwnerId != InventoryOwnerId ||
        !ConsumeMaterialsAtomically(Inventory, InventoryOwnerId,
            Definition->RepairMaterials, Definition->Tools))
        return false;

    if (bAffectsRouteGraph &&
        !Routes->RepairInfrastructure(InfrastructureId, Definition->RepairConditionPerJob))
    {
        const bool bInventoryRestored =
            Inventory->RestoreSnapshot(InventoryOwnerId, PreviousStacks, PreviousQuickAccess);
        const bool bRoutesRestored = Routes->RestoreSnapshot(PreviousNodes, PreviousEdges);
        if (!bInventoryRestored || !bRoutesRestored)
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                Coordinator->MarkSimulationFaulted(
                    TEXT("Infrastructure repair rollback failed; stop simulation and recover/reset the world."));
        return false;
    }

    LocalCondition = FMath::Clamp(
        CurrentCondition + Definition->RepairConditionPerJob, 0.0f, MaxCondition);
    bConstructed = bRouteBuilt || LocalCondition > 0.0f;
    RecordCompletedOperation(OperationId);
    return true;
}

bool AWorldInfrastructureActor::IsInfrastructureBuilt() const
{
    if (bAffectsRouteGraph)
    {
        const UWorldLogisticsRouteSubsystem* Routes =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
        float Condition = 0.0f;
        float MaxCondition = 0.0f;
        bool bBuilt = false;
        return Routes && Routes->GetInfrastructureState(
            InfrastructureId, Condition, MaxCondition, bBuilt) && bBuilt;
    }
    return bConstructed;
}

float AWorldInfrastructureActor::GetInfrastructureCondition() const
{
    if (bAffectsRouteGraph)
    {
        const UWorldLogisticsRouteSubsystem* Routes =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldLogisticsRouteSubsystem>() : nullptr;
        float Condition = 0.0f;
        float MaxCondition = 0.0f;
        bool bBuilt = false;
        if (Routes && Routes->GetInfrastructureState(
            InfrastructureId, Condition, MaxCondition, bBuilt))
            return Condition;
    }
    return LocalCondition;
}
