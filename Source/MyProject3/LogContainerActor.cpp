#include "LogContainerActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "StructuralPieceInterface.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStorageSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldObjectIdentitySubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

using namespace MyProject3Construction;

ALogContainerActor::ALogContainerActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
    SetRootComponent(Base);
    Base->SetCollisionProfileName(TEXT("BlockAll"));
    Identity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));
}

void ALogContainerActor::ConfigureKind(ELogContainerKind NewKind)
{
    Kind = NewKind;
    Base->SetStaticMesh(LoadEngineShape(TEXT("Cube")));
    switch (Kind)
    {
    case ELogContainerKind::SmallLogStorage: SetActorScale3D(FVector(3.2f, 1.4f, 0.15f)); break;
    case ELogContainerKind::LargeLogStorage: SetActorScale3D(FVector(3.2f, 2.4f, 0.15f)); break;
    case ELogContainerKind::FirewoodStorage: SetActorScale3D(FVector(1.6f, 1.0f, 0.15f)); break;
    case ELogContainerKind::BasicSled: SetActorScale3D(FVector(2.6f, 1.1f, 0.12f)); break;
    case ELogContainerKind::AdvancedSled: SetActorScale3D(FVector(3.2f, 1.4f, 0.12f)); break;
    }
    Base->SetMaterial(0, MakeTint(this, IsSled() ? FLinearColor(0.35f, 0.22f, 0.1f) : FLinearColor(0.28f, 0.2f, 0.12f)));
    bAutoCollect = !IsSled();
}

int32 ALogContainerActor::GetCapacity() const
{
    switch (Kind)
    {
    case ELogContainerKind::LargeLogStorage: return 24;
    case ELogContainerKind::FirewoodStorage: return 32;
    case ELogContainerKind::AdvancedSled: return 16;
    case ELogContainerKind::BasicSled:
    case ELogContainerKind::SmallLogStorage:
    default: return 8;
    }
}

bool ALogContainerActor::CanAccept(const AActor* Piece, FString& OutReason) const
{
    const int32 LiveStoredCount = GetValidStoredCount();
    if (!IsValid(Piece) || OwnsPiece(Piece))
    {
        OutReason = TEXT("Nothing to store.");
        return false;
    }
    if (LiveStoredCount >= GetCapacity())
    {
        OutReason = FString::Printf(TEXT("Full (%d/%d)."), LiveStoredCount, GetCapacity());
        return false;
    }

    const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
    const ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece);
    const AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece);
    const AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece);
    if (!Structural || (!Log && !Stone && !Stick) ||
        Structural->IsStructurallyPlaced() || Structural->GetCrossMaterialParent())
    {
        OutReason = TEXT("Only a loose, independent physical piece can be stored.");
        return false;
    }

    const UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    UWorld* World = GetWorld();
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldStructuralGraphSubsystem* Graph =
        World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldStorageSubsystem* Storage =
        World ? World->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    FWorldOwnedItemRecord Item;
    FStructuralNodeRecord Node;
    if (!bStorageRegistered || !Identity || !Identity->HasValidIdentity() ||
        !Ownership || !Graph || !Storage ||
        !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item) ||
        !Graph->GetNode(Structural->GetStructuralNodeId(), Node) ||
        Node.ParentNodeId.IsValid() || !Node.SecondarySupports.IsEmpty() ||
        !Graph->GetAllDependents(Node.NodeId).IsEmpty())
    {
        OutReason = TEXT("The piece has active support links or the storage is unavailable.");
        return false;
    }

    const bool bWorldOwned = Item.OwnerKind == EWorldItemOwnerKind::World &&
        Identity->GetLifecycleState() == EWorldObjectLifecycleState::World;
    const bool bHeldOwned = Item.OwnerKind == EWorldItemOwnerKind::Held &&
        Item.OwnerId.IsValid() && Identity->GetLifecycleState() == EWorldObjectLifecycleState::Held;
    const bool bLocalHeld = Log ? Log->IsHeld() :
        (Stone ? Stone->IsHeld() : Stick->IsHeld());
    if ((!bWorldOwned && !bHeldOwned) || bLocalHeld != bHeldOwned || Item.Quantity != 1)
    {
        OutReason = TEXT("The piece owner and lifecycle state disagree.");
        return false;
    }

    // Shared capacity includes reservations made by logistics cargo. The local
    // Stored count is only the visible slot limit, not the authoritative capacity.
    if (!Storage->CanAcceptItem(StorageId, Item.ItemId))
    {
        OutReason = TEXT("The container storage is full or its space is reserved for incoming cargo.");
        return false;
    }

    switch (Kind)
    {
    case ELogContainerKind::FirewoodStorage:
        if (!IsSmallWoodPiece(Log))
        {
            OutReason = TEXT("Only firewood-sized pieces (1/4 length or shorter) go here.");
            return false;
        }
        return true;
    case ELogContainerKind::AdvancedSled:
        return true;
    default:
        if (!Log)
        {
            OutReason = TEXT("Only logs go here.");
            return false;
        }
        return true;
    }
}

FTransform ALogContainerActor::GetSlotTransform(int32 Index, const AActor* Piece) const
{
    // Rows of pieces lying along the container's X axis, stacked in layers.
    const int32 PerLayer = Kind == ELogContainerKind::FirewoodStorage ? 8
        : Kind == ELogContainerKind::LargeLogStorage ? 6 : 4;
    const float Spacing = Kind == ELogContainerKind::FirewoodStorage ? 18.0f : 34.0f;
    const int32 Layer = Index / PerLayer;
    const int32 Column = Index % PerLayer;
    FVector Origin, Extent;
    GetActorBounds(true, Origin, Extent);
    const FVector LocalOffset(0.0f, (Column - 0.5f * (PerLayer - 1)) * Spacing, 0.0f);
    const FVector Centre = Origin + GetActorQuat().RotateVector(LocalOffset) +
        FVector::UpVector * (Extent.Z + 16.0f + Layer * (Spacing - 2.0f));
    if (const ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
        return MakeLogTransform(Log, Centre, MakeLogRotation(Log, GetActorForwardVector(), FVector::UpVector));
    return FTransform(GetActorQuat(), Centre, Piece ? Piece->GetActorScale3D() : FVector::OneVector);
}

bool ALogContainerActor::TryStore(AActor* Piece)
{
    CompactStoredItems();
    FString Reason;
    if (!CanAccept(Piece, Reason))
        return false;

    UWorld* World = GetWorld();
    UWorldStorageSubsystem* Storage =
        World ? World->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldObjectIdentityComponent* Identity =
        IsValid(Piece) ? Piece->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
    const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
    FWorldOwnedItemRecord BeforeItem;
    if (!Storage || !Ownership || !Identity || !Structural ||
        !Ownership->TryGetItem(Structural->GetStructuralNodeId(), BeforeItem))
        return false;

    const FTransform BeforeTransform = Piece->GetActorTransform();
    UPrimitiveComponent* BeforeRoot = Cast<UPrimitiveComponent>(Piece->GetRootComponent());
    const bool bBeforeSimulatesPhysics = BeforeRoot && BeforeRoot->IsSimulatingPhysics();
    const ECollisionEnabled::Type BeforeCollision = BeforeRoot
        ? BeforeRoot->GetCollisionEnabled() : ECollisionEnabled::NoCollision;
    const FName BeforeCollisionProfile = BeforeRoot
        ? BeforeRoot->GetCollisionProfileName() : NAME_None;
    const bool bBeforeActorCollision = Piece->GetActorEnableCollision();
    const FTransform SlotTransform = GetSlotTransform(GetValidStoredCount(), Piece);
    const EWorldItemOwnerKind PreviousOwner = BeforeItem.OwnerKind;
    const FGuid PreviousOwnerId = BeforeItem.OwnerId;
    const bool bWasHeld = PreviousOwner == EWorldItemOwnerKind::Held;

    FWorldOwnedItemRecord StoredRecord;
    if (!Ownership->TryTransferItem(FGuid::NewGuid(), BeforeItem.ItemId, BeforeItem.Revision,
        PreviousOwner, EWorldItemOwnerKind::Storage, StorageId, GetActorLocation(), StoredRecord))
        return false;

    auto RollbackOwnership = [&]() -> bool
    {
        FWorldOwnedItemRecord Current;
        if (!Ownership->TryGetItem(BeforeItem.ItemId, Current) ||
            Current.OwnerKind != EWorldItemOwnerKind::Storage || Current.OwnerId != StorageId)
            return false;
        FWorldOwnedItemRecord Restored;
        return Ownership->TryTransferItem(FGuid::NewGuid(), Current.ItemId, Current.Revision,
            EWorldItemOwnerKind::Storage, PreviousOwner, PreviousOwnerId,
            BeforeItem.WorldLocation, Restored);
    };

    if (bWasHeld && !Identity->TryTransitionLifecycle(
        EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World))
    {
        if (!RollbackOwnership())
        {
            Piece->SetActorHiddenInGame(true);
            Piece->SetActorEnableCollision(false);
        }
        return false;
    }

    bool bLocalStateChanged = false;
    if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
    {
        if (Log->IsHeld())
            Log->SetHeldState(false);
        bLocalStateChanged = true;
    }
    else if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
    {
        bLocalStateChanged = Stone->SetHeldState(false);
    }
    else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
    {
        bLocalStateChanged = Stick->SetHeldState(false);
    }

    if (!bLocalStateChanged)
    {
        if (bWasHeld)
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Held);
        if (!RollbackOwnership())
        {
            Piece->SetActorHiddenInGame(true);
            Piece->SetActorEnableCollision(false);
        }
        return false;
    }

    Piece->SetActorTransform(SlotTransform, false, nullptr, ETeleportType::TeleportPhysics);
    if (!Piece->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform))
    {
        Piece->SetActorTransform(BeforeTransform, false, nullptr, ETeleportType::TeleportPhysics);
        if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
        {
            if (bWasHeld) Log->SetHeldState(true);
        }
        else if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
        {
            if (bWasHeld) Stone->SetHeldState(true);
        }
        else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
        {
            if (bWasHeld) Stick->SetHeldState(true);
        }

        if (bWasHeld)
        {
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Held);
            Piece->SetActorEnableCollision(false);
        }
        else
        {
            Piece->SetActorEnableCollision(bBeforeActorCollision);
        }
        if (BeforeRoot)
        {
            BeforeRoot->SetCollisionProfileName(BeforeCollisionProfile);
            BeforeRoot->SetCollisionEnabled(BeforeCollision);
            BeforeRoot->SetSimulatePhysics(bBeforeSimulatesPhysics);
        }
        if (!RollbackOwnership())
        {
            Piece->SetActorHiddenInGame(true);
            Piece->SetActorEnableCollision(false);
            UE_LOG(LogTemp, Error, TEXT("Container store rollback failed for item %s."),
                *BeforeItem.ItemId.ToString());
        }
        return false;
    }

    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent()))
    {
        Root->SetSimulatePhysics(false);
        Root->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    Piece->SetActorEnableCollision(true);
    Stored.Add(Piece);
    ApplyTowSpeed();
    return true;
}

AActor* ALogContainerActor::TakeOne(const FVector& DropLocation)
{
    if (!FMath::IsFinite(DropLocation.X) || !FMath::IsFinite(DropLocation.Y) ||
        !FMath::IsFinite(DropLocation.Z))
        return nullptr;
    while (Stored.Num() > 0)
    {
        AActor* Piece = Stored.Pop();
        if (!IsValid(Piece))
            continue;
        if (!TransferPiece(Piece, EWorldItemOwnerKind::World, FGuid(), DropLocation))
        {
            Stored.Add(Piece);
            return nullptr;
        }
        Piece->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Piece->SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
        if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent()))
        {
            Root->SetCollisionProfileName(TEXT("PhysicsActor"));
            Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Root->SetSimulatePhysics(true);
        }
        ApplyTowSpeed();
        return Piece;
    }
    return nullptr;
}

bool ALogContainerActor::OwnsPiece(const AActor* Piece) const
{
    return Piece && Stored.ContainsByPredicate([Piece](const TObjectPtr<AActor>& Item) { return Item.Get() == Piece; });
}

int32 ALogContainerActor::GetValidStoredCount() const
{
    int32 Count = 0;
    for (const TObjectPtr<AActor>& Piece : Stored)
        if (IsValid(Piece.Get()))
            ++Count;
    return Count;
}

void ALogContainerActor::CompactStoredItems()
{
    Stored.RemoveAll([](const TObjectPtr<AActor>& Piece)
    {
        return !IsValid(Piece.Get());
    });
}

void ALogContainerActor::ApplyTowSpeed() const
{
    ACharacter* Character = Cast<ACharacter>(TowedBy.Get());
    if (Character && Character->GetCharacterMovement() && PullerBaseSpeed > 0.0f)
    {
        const float Penalty = FMath::Clamp(0.25f + 0.025f * GetValidStoredCount(), 0.0f, 0.55f);
        Character->GetCharacterMovement()->MaxWalkSpeed = PullerBaseSpeed * (1.0f - Penalty);
    }
}

bool ALogContainerActor::ToggleTow(APawn* Puller)
{
    if (!IsSled() || !Puller)
        return false;
    ACharacter* Character = Cast<ACharacter>(TowedBy.Get());
    if (TowedBy.IsValid())
    {
        if (Character && Character->GetCharacterMovement() && PullerBaseSpeed > 0.0f)
            Character->GetCharacterMovement()->MaxWalkSpeed = PullerBaseSpeed;
        TowedBy.Reset();
        return true;
    }
    TowedBy = Puller;
    Character = Cast<ACharacter>(Puller);
    PullerBaseSpeed = Character && Character->GetCharacterMovement() ? Character->GetCharacterMovement()->MaxWalkSpeed : 0.0f;
    Base->SetSimulatePhysics(false);
    ApplyTowSpeed();
    return true;
}

void ALogContainerActor::CollectNearbyLogs()
{
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        ALogBuildPiece* Log = *It;
        if (!IsValid(Log) || Log->IsHeld() || Log->IsPlaced() || Log->GetAttachParentActor() ||
            FVector::Dist(Log->GetActorLocation(), GetActorLocation()) > CollectRadius)
            continue;
        EWorldItemOwnerKind ItemOwner = EWorldItemOwnerKind::World;
        if (!GetPieceOwnerKind(Log, ItemOwner) || ItemOwner != EWorldItemOwnerKind::World)
            continue;
        if (Log->GetLogMesh() && Log->GetLogMesh()->GetPhysicsLinearVelocity().Size() > 30.0f)
            continue; // Still rolling or flying.
        TryStore(Log);
        if (GetStoredCount() >= GetCapacity())
            return;
    }
}

void ALogContainerActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (Identity && Identity->EnsureIdentity())
        StorageId = Identity->GetInstanceId();
}

void ALogContainerActor::BeginPlay()
{
    Super::BeginPlay();
    if (!Base->GetStaticMesh())
        ConfigureKind(Kind);

    UWorld* World = GetWorld();
    UWorldObjectIdentitySubsystem* IdentityRegistry =
        World ? World->GetSubsystem<UWorldObjectIdentitySubsystem>() : nullptr;
    if (!Identity || !Identity->EnsureIdentity() || !IdentityRegistry ||
        !IdentityRegistry->RegisterIdentity(Identity))
    {
        UE_LOG(LogTemp, Error, TEXT("Container '%s' has no valid canonical identity."),
            *GetNameSafe(this));
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        SetActorTickEnabled(false);
        return;
    }
    if (Identity->DefinitionId.IsNone())
        Identity->SetDefinitionId(FName(TEXT("Container.Storage")));
    StorageId = Identity->GetInstanceId();

    UWorldStorageSubsystem* Storage =
        World ? World->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    bStorageRegistered = Storage && Storage->RegisterStorage(StorageId, GetCapacity());
    if (!bStorageRegistered)
    {
        UE_LOG(LogTemp, Error, TEXT("Container '%s' could not register storage ID %s."),
            *GetNameSafe(this), *StorageId.ToString());
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        SetActorTickEnabled(false);
    }
}

void ALogContainerActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bAutoCollect && !IsSled())
    {
        TimeToCollect -= DeltaSeconds;
        if (TimeToCollect <= 0.0f)
        {
            TimeToCollect = 1.0f;
            CollectNearbyLogs();
        }
    }

    if (APawn* Puller = TowedBy.Get())
    {
        // The sled slides behind the puller and follows the ground.
        const FVector Forward = FVector(Puller->GetActorForwardVector().X, Puller->GetActorForwardVector().Y, 0.0f).GetSafeNormal();
        FVector Target = Puller->GetActorLocation() - Forward * 230.0f;
        FHitResult Ground;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SledGround), false, this);
        Params.AddIgnoredActor(Puller);
        for (AActor* Piece : Stored)
            Params.AddIgnoredActor(Piece);
        if (GetWorld()->LineTraceSingleByChannel(Ground, Target + FVector(0, 0, 300), Target - FVector(0, 0, 800),
            ECC_Visibility, Params))
        {
            const float HalfHeight = Base->GetStaticMesh() ? Base->GetStaticMesh()->GetBounds().BoxExtent.Z : 50.0f;
            Target.Z = Ground.ImpactPoint.Z + HalfHeight * GetActorScale3D().Z + 2.0f;
        }
        const FVector Smoothed = FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, 6.0f);
        SetActorLocationAndRotation(Smoothed, FRotator(0.0f, Forward.Rotation().Yaw, 0.0f));
    }
}

void ALogContainerActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (TowedBy.IsValid())
        ToggleTow(TowedBy.Get());
    if (bStorageRegistered)
    {
        UWorldStorageSubsystem* Storage =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
        if (Storage && !Storage->SetStorageAvailable(StorageId, false))
            UE_LOG(LogTemp, Error, TEXT("Container storage %s could not be marked unavailable."),
                *StorageId.ToString());
        bStorageRegistered = false;
    }
    Super::EndPlay(Reason);
}
