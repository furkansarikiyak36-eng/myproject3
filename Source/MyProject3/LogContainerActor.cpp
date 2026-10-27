#include "LogContainerActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"

#include "Components/StaticMeshComponent.h"
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
    if (!IsValid(Piece) || Stored.Contains(Piece))
    {
        OutReason = TEXT("Nothing to store.");
        return false;
    }
    if (Stored.Num() >= GetCapacity())
    {
        OutReason = FString::Printf(TEXT("Full (%d/%d)."), Stored.Num(), GetCapacity());
        return false;
    }
    const ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece);
    const bool bMaterial = Cast<AStoneBuildingPiece>(Piece) || Cast<AStickPrimitivePiece>(Piece);
    if (Log && Log->IsPlaced())
    {
        OutReason = TEXT("That log is part of a structure.");
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
        if (!Log && !bMaterial)
        {
            OutReason = TEXT("The sled takes logs, stones and sticks.");
            return false;
        }
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
    FString Reason;
    if (!CanAccept(Piece, Reason))
        return false;
    if (!TransferPiece(Piece, EWorldItemOwnerKind::Storage, StorageId, GetActorLocation()))
        return false;

    if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
    {
        if (Log->IsHeld())
            Log->SetHeldState(false);
        if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
        {
            if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::Held)
                Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World);
        }
    }
    else if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
    {
        Stone->SetHeldState(false);
    }
    else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
    {
        Stick->SetHeldState(false);
    }

    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent()))
    {
        Root->SetSimulatePhysics(false);
        Root->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    Piece->SetActorEnableCollision(true);
    Piece->SetActorTransform(GetSlotTransform(Stored.Num(), Piece), false, nullptr, ETeleportType::TeleportPhysics);
    Piece->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
    Stored.Add(Piece);
    ApplyTowSpeed();
    return true;
}

AActor* ALogContainerActor::TakeOne(const FVector& DropLocation)
{
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
    return Piece && Stored.Contains(Piece);
}

void ALogContainerActor::ApplyTowSpeed() const
{
    ACharacter* Character = Cast<ACharacter>(TowedBy.Get());
    if (Character && Character->GetCharacterMovement() && PullerBaseSpeed > 0.0f)
    {
        const float Penalty = FMath::Clamp(0.25f + 0.025f * Stored.Num(), 0.0f, 0.55f);
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
        EWorldItemOwnerKind Owner;
        if (!GetPieceOwnerKind(Log, Owner) || Owner != EWorldItemOwnerKind::World)
            continue;
        if (Log->GetLogMesh() && Log->GetLogMesh()->GetPhysicsLinearVelocity().Size() > 30.0f)
            continue; // Still rolling or flying.
        TryStore(Log);
        if (Stored.Num() >= GetCapacity())
            return;
    }
}

void ALogContainerActor::BeginPlay()
{
    Super::BeginPlay();
    if (!Base->GetStaticMesh())
        ConfigureKind(Kind);
    if (!StorageId.IsValid())
        StorageId = FGuid::NewGuid();
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
            FVector Origin, Extent;
            Base->GetLocalBounds(Origin, Extent);
            Target.Z = Ground.ImpactPoint.Z + Extent.Z * GetActorScale3D().Z + 2.0f;
        }
        const FVector Smoothed = FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, 6.0f);
        SetActorLocationAndRotation(Smoothed, FRotator(0.0f, Forward.Rotation().Yaw, 0.0f));
    }
}

void ALogContainerActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (TowedBy.IsValid())
        ToggleTow(TowedBy.Get());
    Super::EndPlay(Reason);
}
