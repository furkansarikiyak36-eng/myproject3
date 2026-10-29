#include "WallCoatingComponent.h"

#include "InventoryCraftingComponent.h"
#include "InventoryCraftingTypes.h"
#include "StructuralPieceInterface.h"

UWallCoatingComponent::UWallCoatingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UWallCoatingComponent::ProvidesShelterSeal() const
{
    return IsValidState() && CoatingLevel >= MinimumSealForShelter;
}

bool UWallCoatingComponent::IsValidState() const
{
    if (!FMath::IsFinite(CoatingLevel) || CoatingLevel < 0.0f || CoatingLevel > 1.0f ||
        !FMath::IsFinite(CoatingPerUnit) || CoatingPerUnit <= 0.0f || CoatingPerUnit > 1.0f ||
        !FMath::IsFinite(MinimumSealForShelter) || MinimumSealForShelter < 0.0f ||
        MinimumSealForShelter > 1.0f || PlasterItemDefinitionId.IsNone() ||
        Revision <= 0 || MaxReceipts < 1 || MaxReceipts > 128 ||
        Receipts.Num() > MaxReceipts)
        return false;

    TSet<FGuid> Seen;
    for (const FWallCoatingApplicationReceipt& Receipt : Receipts)
    {
        if (!Receipt.OperationId.IsValid() || Seen.Contains(Receipt.OperationId) ||
            Receipt.ItemDefinitionId != PlasterItemDefinitionId ||
            Receipt.UnitsConsumed <= 0 || !FMath::IsFinite(Receipt.ResultingCoating) ||
            Receipt.ResultingCoating < 0.0f || Receipt.ResultingCoating > 1.0f ||
            Receipt.ResultingRevision <= 0 || Receipt.ResultingRevision > Revision)
            return false;
        Seen.Add(Receipt.OperationId);
    }
    return true;
}

bool UWallCoatingComponent::TryApplyPlaster(const FGuid& OperationId,
    UInventoryCraftingComponent* Inventory, int32 Units, int64 ExpectedRevision,
    float& OutCoatingLevel)
{
    OutCoatingLevel = CoatingLevel;
    if (!OperationId.IsValid() || !IsValid(Inventory) || Units <= 0 || Units > 4 ||
        ExpectedRevision < 0 || !IsValidState())
        return false;

    if (const FWallCoatingApplicationReceipt* Existing = Receipts.FindByPredicate(
        [&OperationId](const FWallCoatingApplicationReceipt& Receipt)
        { return Receipt.OperationId == OperationId; }))
    {
        OutCoatingLevel = Existing->ResultingCoating;
        return Existing->ItemDefinitionId == PlasterItemDefinitionId &&
            Existing->UnitsConsumed == Units && Existing->ResultingRevision <= Revision;
    }

    const AActor* Owner = GetOwner();
    const IStructuralPieceInterface* Piece = IsValid(Owner)
        ? Cast<IStructuralPieceInterface>(Owner) : nullptr;
    if (!Piece || !Piece->IsStructurallyPlaced() || ExpectedRevision != Revision ||
        Revision >= MAX_int64 - 1 || Receipts.Num() >= MaxReceipts ||
        CoatingLevel >= 1.0f - KINDA_SMALL_NUMBER ||
        Inventory->GetItemCount(PlasterItemDefinitionId) < Units)
        return false;

    const float NewCoating = FMath::Clamp(
        CoatingLevel + CoatingPerUnit * static_cast<float>(Units), 0.0f, 1.0f);
    if (NewCoating <= CoatingLevel || !Inventory->TryRemoveItem(PlasterItemDefinitionId, Units))
        return false;

    CoatingLevel = NewCoating;
    ++Revision;
    FWallCoatingApplicationReceipt& Receipt = Receipts.AddDefaulted_GetRef();
    Receipt.OperationId = OperationId;
    Receipt.ItemDefinitionId = PlasterItemDefinitionId;
    Receipt.UnitsConsumed = Units;
    Receipt.ResultingCoating = CoatingLevel;
    Receipt.ResultingRevision = Revision;
    OutCoatingLevel = CoatingLevel;
    return true;
}
