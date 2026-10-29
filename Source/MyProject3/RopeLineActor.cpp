#include "RopeLineActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldObjectIdentityComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

using namespace MyProject3Construction;

ARopeLineActor::ARopeLineActor()
{
    PrimaryActorTick.bCanEverTick = true;
    RopeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rope"));
    SetRootComponent(RopeMesh);
    RopeMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    RopeMesh->SetCanEverAffectNavigation(false);
}

float ARopeLineActor::GetSlopeDegrees() const
{
    const FVector Delta = End - Start;
    const float Horizontal = FVector(Delta.X, Delta.Y, 0.0f).Size();
    return FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Delta.Z), FMath::Max(Horizontal, 1.0f)));
}

void ARopeLineActor::InitializeRope(const FVector& InStart, AActor* InStartAnchor, const FVector& InEnd, AActor* InEndAnchor)
{
    Start = InStart;
    End = InEnd;
    StartAnchor = InStartAnchor;
    EndAnchor = InEndAnchor;
    bTied = FVector::Dist(Start, End) > 50.0f;
    UpdateVisual();
    if (!NodeId.IsValid())
        NodeId = FGuid::NewGuid();
    if (UWorldStructuralGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr)
    {
        FStructuralNodeRecord Existing;
        if (!Graph->GetNode(NodeId, Existing))
            Graph->RegisterNode(this, NodeId, EStructuralNodeKind::Other, bTied, 0.5f, 5000.0f, 200.0f);
    }
}

void ARopeLineActor::UpdateVisual()
{
    RopeMesh->SetStaticMesh(LoadEngineShape(TEXT("Cylinder")));
    RopeMesh->SetMaterial(0, MakeTint(this, FLinearColor(0.55f, 0.45f, 0.25f)));
    const FVector Delta = End - Start;
    const float Length = FMath::Max(Delta.Size(), 1.0f);
    SetActorTransform(FTransform(FRotationMatrix::MakeFromZ(Delta / Length).ToQuat(), 0.5f * (Start + End),
        FVector(0.04f, 0.04f, Length / 100.0f)));
}

bool ARopeLineActor::TrySendAlong(AActor* Piece)
{
    if (!IsValid(Piece) || !GetWorld() || !IsZipline() ||
        !FMath::IsFinite(MaxRideSpeed) || MaxRideSpeed <= 0.0f ||
        !FMath::IsFinite(MaxLoadMassKg) || MaxLoadMassKg < 1.0f || MaxLoadMassKg > 100000.0f ||
        !FMath::IsFinite(SlidingFrictionCoefficient) ||
        SlidingFrictionCoefficient < 0.0f || SlidingFrictionCoefficient > 1.0f ||
        Riders.ContainsByPredicate([Piece](const FRider& Rider) { return Rider.Piece.Get() == Piece; }))
        return false;

    // Use the shared nominal-volume/density estimate; a non-simulating mesh's
    // engine mass can be unset and must not bypass the authored rope capacity.
    const float PieceMassKg = EstimateMassKg(Piece);
    if (!FMath::IsFinite(PieceMassKg) || PieceMassKg <= 0.0f ||
        PieceMassKg > MaxLoadMassKg)
        return false;

    // The capacity is the total rated load of the line, not a per-item limit.
    // Re-estimate current riders from the same nominal-volume model and fail
    // closed if any in-transit actor has an invalid mass estimate.
    double ProposedTotalLoadKg = static_cast<double>(PieceMassKg);
    for (const FRider& ExistingRider : Riders)
    {
        const AActor* ExistingPiece = ExistingRider.Piece.Get();
        if (!IsValid(ExistingPiece))
            continue; // Tick will remove the stale weak rider record.
        const float ExistingMassKg = EstimateMassKg(ExistingPiece);
        if (!FMath::IsFinite(ExistingMassKg) || ExistingMassKg <= 0.0f)
            return false;
        ProposedTotalLoadKg += static_cast<double>(ExistingMassKg);
        if (!FMath::IsFinite(ProposedTotalLoadKg) ||
            ProposedTotalLoadKg > static_cast<double>(MaxLoadMassKg))
            return false;
    }
    if (!FMath::IsFinite(ProposedTotalLoadKg) ||
        ProposedTotalLoadKg > static_cast<double>(MaxLoadMassKg))
        return false;

    UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent());
    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    EWorldItemOwnerKind CurrentOwner = EWorldItemOwnerKind::World;
    FWorldOwnedItemRecord Item;
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
    if (!Root || !Identity || !Identity->HasValidIdentity() ||
        Identity->GetLifecycleState() != EWorldObjectLifecycleState::World ||
        !GetPieceOwnerKind(Piece, CurrentOwner) || CurrentOwner != EWorldItemOwnerKind::World ||
        !Ownership || !Structural ||
        !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item) ||
        (Item.OwnerKind != EWorldItemOwnerKind::World &&
         Item.OwnerKind != EWorldItemOwnerKind::Held) ||
        (Item.OwnerKind == EWorldItemOwnerKind::Held && !Item.OwnerId.IsValid()) ||
        Item.Revision == MAX_int64)
        return false;

    const FVector Upper = GetUpperPoint();
    if (!TransferPiece(Piece, EWorldItemOwnerKind::InTransit, NodeId, Upper))
        return false;

    Piece->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Piece->SetActorLocation(Upper, false, nullptr, ETeleportType::TeleportPhysics);
    Root->SetSimulatePhysics(false);
    Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FRider& Rider = Riders.AddDefaulted_GetRef();
    Rider.Piece = Piece;
    Rider.Distance = 0.0f;
    Rider.Speed = 0.0f;
    Rider.AccumulatorSeconds = 0.0;
    return true;
}

void ARopeLineActor::BeginPlay()
{
    Super::BeginPlay();
    if (bTied)
        InitializeRope(Start, StartAnchor.Get(), End, EndAnchor.Get());
}

void ARopeLineActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Riders.IsEmpty() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f ||
        !FMath::IsFinite(MaxRideSpeed) || MaxRideSpeed <= 0.0f ||
        !FMath::IsFinite(MaxLoadMassKg) || MaxLoadMassKg < 1.0f || MaxLoadMassKg > 100000.0f ||
        !FMath::IsFinite(SlidingFrictionCoefficient) ||
        SlidingFrictionCoefficient < 0.0f || SlidingFrictionCoefficient > 1.0f)
        return;

    const FVector Upper = GetUpperPoint();
    const FVector Lower = GetLowerPoint();
    const float Length = FVector::Dist(Upper, Lower);
    if (!FMath::IsFinite(Length) || Length <= 1.0f)
        return;

    const float SlopeRadians = FMath::DegreesToRadians(GetSlopeDegrees());
    constexpr float GravityCmPerSecondSquared = 980.0f;
    const float GravityAlongRope = GravityCmPerSecondSquared * FMath::Sin(SlopeRadians);
    const float NormalAcceleration = GravityCmPerSecondSquared * FMath::Cos(SlopeRadians);
    // Coulomb friction scales with the normal force. Mass cancels from sliding
    // acceleration, while the nominal mass cap above still bounds rope loading.
    const float FrictionAcceleration =
        SlidingFrictionCoefficient * FMath::Max(0.0f, NormalAcceleration);
    const float NetAcceleration = GravityAlongRope - FrictionAcceleration;
    if (!FMath::IsFinite(NetAcceleration))
        return;
    const FVector Down = (Lower - Upper).GetSafeNormal();
    if (!FMath::IsFinite(Down.X) || !FMath::IsFinite(Down.Y) || !FMath::IsFinite(Down.Z))
        return;

    // Fixed-step the rider integration so large frame deltas cannot create a
    // single huge speed/distance jump. A rider can stop when gravity cannot
    // overcome friction; no artificial minimum speed forces it downhill.
    constexpr float StepSeconds = 0.05f;
    constexpr int32 MaxStepsPerFrame = 20;

    for (int32 Index = Riders.Num() - 1; Index >= 0; --Index)
    {
        FRider& Rider = Riders[Index];
        AActor* Piece = Rider.Piece.Get();
        if (!IsValid(Piece))
        {
            Riders.RemoveAt(Index);
            continue;
        }

        UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent());
        if (!Root || !FMath::IsFinite(Rider.Distance) || Rider.Distance < 0.0f ||
            !FMath::IsFinite(Rider.Speed) || Rider.Speed < 0.0f)
        {
            continue;
        }

        Rider.AccumulatorSeconds += static_cast<double>(DeltaSeconds);
        if (!FMath::IsFinite(Rider.AccumulatorSeconds))
        {
            Rider.AccumulatorSeconds = 0.0;
            continue;
        }

        int32 Steps = 0;
        while (Rider.AccumulatorSeconds >= StepSeconds && Steps < MaxStepsPerFrame &&
            Rider.Distance < Length)
        {
            Rider.Speed = FMath::Clamp(Rider.Speed + NetAcceleration * StepSeconds, 0.0f, MaxRideSpeed);
            Rider.Distance = FMath::Clamp(Rider.Distance + Rider.Speed * StepSeconds, 0.0f, Length);
            Rider.AccumulatorSeconds -= StepSeconds;
            ++Steps;
        }

        const FVector OnRope = Upper + Down * Rider.Distance;
        FVector Origin, Extent;
        Piece->GetActorBounds(false, Origin, Extent);
        const FVector DesiredOrigin = OnRope - FVector(0.0f, 0.0f, Extent.Z + 15.0f);
        Piece->SetActorLocation(Piece->GetActorLocation() + (DesiredOrigin - Origin),
            false, nullptr, ETeleportType::TeleportPhysics);

        if (Rider.Distance < Length)
            continue;

        // Ownership transfer is the delivery commit. If it is rejected, keep
        // the rider at the lower endpoint with physics disabled and retry later.
        if (!TransferPiece(Piece, EWorldItemOwnerKind::World, FGuid(), Piece->GetActorLocation()))
            continue;

        Root->SetCollisionProfileName(TEXT("PhysicsActor"));
        Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Root->SetSimulatePhysics(true);
        Root->SetPhysicsLinearVelocity(Down * 200.0f);
        Riders.RemoveAt(Index);
    }
}
