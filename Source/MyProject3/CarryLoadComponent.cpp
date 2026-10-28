#include "CarryLoadComponent.h"
#include "ConstructionHelpers.h"

#include "LogBuildPiece.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "StructuralPieceInterface.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
    bool SetPieceHeld(AActor* Piece, bool bHeld)
    {
        if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
        {
            Log->SetHeldState(bHeld);
            return true;
        }
        if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
            return Stone->SetHeldState(bHeld);
        if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
            return Stick->SetHeldState(bHeld);
        return false;
    }

    UStaticMeshComponent* GetPieceMesh(const AActor* Piece)
    {
        return Piece ? Piece->FindComponentByClass<UStaticMeshComponent>() : nullptr;
    }
}

UCarryLoadComponent::UCarryLoadComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    auto AddRule = [this](EStructuralNodeKind Kind, int32 MaxCount, float Density)
    {
        FCarrySlotRule Rule;
        Rule.Kind = Kind;
        Rule.MaxCount = MaxCount;
        Rule.DensityKgPerCubicMetre = Density;
        SlotRules.Add(Rule);
    };
    AddRule(EStructuralNodeKind::Wood, 2, 600.0f);   // two logs on the shoulder
    AddRule(EStructuralNodeKind::Stick, 10, 600.0f); // a bundle of sticks
    AddRule(EStructuralNodeKind::Stone, 2, 2500.0f); // one stone per hand
}

void UCarryLoadComponent::BeginPlay()
{
    Super::BeginPlay();
    CarrierId = FGuid::NewGuid();
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
            BaseWalkSpeed = Movement->MaxWalkSpeed;
    }
}

const FCarrySlotRule* UCarryLoadComponent::FindRule(EStructuralNodeKind Kind) const
{
    const FCarrySlotRule* Match = nullptr;
    for (const FCarrySlotRule& Rule : SlotRules)
    {
        if (Rule.Kind != Kind)
            continue;
        if (Match)
            return nullptr; // Duplicate material rules make carry limits/density ambiguous.
        if (Rule.MaxCount <= 0 || !FMath::IsFinite(Rule.DensityKgPerCubicMetre) ||
            Rule.DensityKgPerCubicMetre <= 0.0f)
            return nullptr;
        Match = &Rule;
    }
    return Match;
}

float UCarryLoadComponent::EstimateMassKg(const AActor* Piece) const
{
    const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
    const FCarrySlotRule* Rule = Structural ? FindRule(Structural->GetStructuralKind()) : nullptr;
    if (!Rule)
        return 0.0f;
    return MyProject3Construction::EstimateMassKg(
        Piece, BoundsFillFactor, Rule->DensityKgPerCubicMetre);
}

bool UCarryLoadComponent::CanCarryMore(AActor* Piece, FString& OutReason) const
{
    const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
    if (!IsValid(Piece) || !Structural)
    {
        OutReason = TEXT("Only physical building materials can be carried.");
        return false;
    }
    if (!FMath::IsFinite(MaxCarryMassKg) || MaxCarryMassKg <= 0.0f ||
        !FMath::IsFinite(MinSpeedMultiplier) || MinSpeedMultiplier < 0.0f ||
        MinSpeedMultiplier > 1.0f ||
        !FMath::IsFinite(BoundsFillFactor) || BoundsFillFactor <= 0.0f ||
        BoundsFillFactor > 1.0f)
    {
        OutReason = TEXT("Carry configuration is invalid; no item can be taken safely.");
        return false;
    }
    if (CarriedPieces.Contains(Piece))
    {
        OutReason = TEXT("This piece is already carried.");
        return false;
    }
    const FCarrySlotRule* Rule = FindRule(Structural->GetStructuralKind());
    if (!Rule || GetCarriedCount(Structural->GetStructuralKind()) >= Rule->MaxCount)
    {
        OutReason = TEXT("No free carry slot for this material.");
        return false;
    }
    const float Mass = EstimateMassKg(Piece);
    if (!FMath::IsFinite(Mass) || Mass <= 0.0f)
    {
        // Unknown mass is never treated as free to carry.
        OutReason = TEXT("This piece has no measurable mass.");
        return false;
    }
    return true;
}

bool UCarryLoadComponent::RegisterCarried(AActor* Piece)
{
    FString Reason;
    if (!CanCarryMore(Piece, Reason))
        return false;
    CarriedPieces.Add(Piece);
    ApplyMovementSpeed();
    return true;
}

void UCarryLoadComponent::UnregisterCarried(AActor* Piece)
{
    CarriedPieces.RemoveAll([Piece](const TWeakObjectPtr<AActor>& Item)
    {
        return !Item.IsValid() || Item.Get() == Piece;
    });
    ApplyMovementSpeed();
}

bool UCarryLoadComponent::TryTakePiece(AActor* Piece, FString& OutReason)
{
    if (!CanCarryMore(Piece, OutReason))
        return false;

    IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece);
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldObjectIdentityComponent* Identity =
        IsValid(Piece) ? Piece->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
    FWorldOwnedItemRecord Item;
    if (!Structural || !Ownership || !Identity || !Identity->HasValidIdentity() ||
        !CarrierId.IsValid() || Identity->GetLifecycleState() != EWorldObjectLifecycleState::World ||
        Identity->StateRevision == MAX_int64 ||
        Structural->IsStructurallyPlaced() || Structural->GetCrossMaterialParent() ||
        !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::World)
    {
        OutReason = TEXT("Only a loose, world-owned piece can be taken.");
        return false;
    }
    if (const ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
    {
        if (!Log->CanBePickedUp())
        {
            OutReason = TEXT("This log supports other pieces or cannot be carried.");
            return false;
        }
    }

    FWorldOwnedItemRecord HeldRecord;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
        EWorldItemOwnerKind::World, EWorldItemOwnerKind::Held, CarrierId,
        Piece->GetActorLocation(), HeldRecord))
    {
        OutReason = TEXT("The ownership ledger rejected the pickup.");
        return false;
    }
    if (!Identity->TryTransitionLifecycle(
        EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Held))
    {
        FWorldOwnedItemRecord RolledBack;
        Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, HeldRecord.Revision,
            EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, FGuid(),
            Piece->GetActorLocation(), RolledBack);
        OutReason = TEXT("The piece lifecycle rejected the pickup.");
        return false;
    }
    if (!SetPieceHeld(Piece, true))
    {
        Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World);
        FWorldOwnedItemRecord RolledBack;
        const bool bOwnershipRestored = Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, HeldRecord.Revision,
            EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, FGuid(),
            Piece->GetActorLocation(), RolledBack);
        if (!bOwnershipRestored)
        {
            Piece->SetActorHiddenInGame(true);
            Piece->SetActorEnableCollision(false);
        }
        OutReason = TEXT("The piece refused to be held.");
        return false;
    }

    if (UStaticMeshComponent* Mesh = GetPieceMesh(Piece))
        Mesh->SetSimulatePhysics(false);
    Piece->SetActorHiddenInGame(true);
    Piece->SetActorEnableCollision(false);
    CarriedPieces.Add(Piece);
    ApplyMovementSpeed();
    OutReason.Reset();
    return true;
}

bool UCarryLoadComponent::TryDropPiece(AActor* Piece, FVector WorldLocation)
{
    IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece);
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldObjectIdentityComponent* Identity =
        IsValid(Piece) ? Piece->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
    FWorldOwnedItemRecord Item;
    if (!Structural || !Ownership || !Identity || !CarriedPieces.Contains(Piece) ||
        !FMath::IsFinite(WorldLocation.X) || !FMath::IsFinite(WorldLocation.Y) ||
        !FMath::IsFinite(WorldLocation.Z) ||
        Identity->GetLifecycleState() != EWorldObjectLifecycleState::Held ||
        Identity->StateRevision == MAX_int64 ||
        !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item) ||
        Item.OwnerKind != EWorldItemOwnerKind::Held || Item.OwnerId != CarrierId)
    {
        return false;
    }

    FWorldOwnedItemRecord WorldRecord;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision,
        EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, FGuid(), WorldLocation, WorldRecord))
    {
        return false;
    }

    if (!SetPieceHeld(Piece, false) ||
        !Identity->TryTransitionLifecycle(
            EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World))
    {
        SetPieceHeld(Piece, true);
        FWorldOwnedItemRecord RolledBack;
        if (!Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, WorldRecord.Revision,
            EWorldItemOwnerKind::World, EWorldItemOwnerKind::Held, CarrierId,
            Item.WorldLocation, RolledBack))
        {
            Piece->SetActorHiddenInGame(true);
            Piece->SetActorEnableCollision(false);
            UE_LOG(LogTemp, Error, TEXT("Carry-load drop rollback failed for %s."), *GetNameSafe(Piece));
        }
        return false;
    }

    Piece->SetActorLocation(WorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Piece->SetActorHiddenInGame(false);
    Piece->SetActorEnableCollision(true);
    if (UStaticMeshComponent* Mesh = GetPieceMesh(Piece))
    {
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
    }
    UnregisterCarried(Piece);
    return true;
}

int32 UCarryLoadComponent::GetCarriedCount(EStructuralNodeKind Kind) const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<AActor>& Item : CarriedPieces)
    {
        const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Item.Get());
        if (Structural && Structural->GetStructuralKind() == Kind)
            ++Count;
    }
    return Count;
}

float UCarryLoadComponent::GetCarriedMassKg() const
{
    double Total = 0.0;
    for (const TWeakObjectPtr<AActor>& Item : CarriedPieces)
    {
        if (!Item.IsValid())
            continue;
        const float Mass = EstimateMassKg(Item.Get());
        if (!FMath::IsFinite(Mass) || Mass < 0.0f)
            return TNumericLimits<float>::Max();
        Total += static_cast<double>(Mass);
        if (!FMath::IsFinite(Total) || Total >= TNumericLimits<float>::Max())
            return TNumericLimits<float>::Max();
    }
    return static_cast<float>(Total);
}

float UCarryLoadComponent::GetSpeedMultiplier() const
{
    if (!FMath::IsFinite(MaxCarryMassKg) || MaxCarryMassKg <= 0.0f ||
        !FMath::IsFinite(MinSpeedMultiplier) || MinSpeedMultiplier < 0.0f ||
        MinSpeedMultiplier > 1.0f)
        return 1.0f;

    const float Mass = GetCarriedMassKg();
    const float Ratio = FMath::IsFinite(Mass)
        ? FMath::Clamp(Mass / MaxCarryMassKg, 0.0f, 1.0f) : 1.0f;
    return FMath::Lerp(1.0f, MinSpeedMultiplier, Ratio);
}

void UCarryLoadComponent::ApplyMovementSpeed()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement || BaseWalkSpeed <= 0.0f)
        return;
    Movement->MaxWalkSpeed = BaseWalkSpeed * GetSpeedMultiplier();
}

TArray<AActor*> UCarryLoadComponent::GetCarriedPieces() const
{
    TArray<AActor*> Result;
    for (const TWeakObjectPtr<AActor>& Item : CarriedPieces)
    {
        if (Item.IsValid())
            Result.Add(Item.Get());
    }
    return Result;
}
