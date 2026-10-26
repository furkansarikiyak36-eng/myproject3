#include "RopeLineActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"

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
    if (!IsValid(Piece) || !IsZipline())
        return false;
    if (!TransferPiece(Piece, EWorldItemOwnerKind::InTransit, NodeId, GetUpperPoint()))
        return false;
    if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
        Log->SetHeldState(false);
    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent()))
    {
        Root->SetSimulatePhysics(false);
        Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    FRider& Rider = Riders.AddDefaulted_GetRef();
    Rider.Piece = Piece;
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
    if (Riders.IsEmpty())
        return;
    const FVector Upper = GetUpperPoint();
    const FVector Lower = GetLowerPoint();
    const float Length = FVector::Dist(Upper, Lower);
    const float Gravity = 980.0f * FMath::Sin(FMath::DegreesToRadians(GetSlopeDegrees()));
    const FVector Down = (Lower - Upper).GetSafeNormal();

    for (int32 Index = Riders.Num() - 1; Index >= 0; --Index)
    {
        FRider& Rider = Riders[Index];
        AActor* Piece = Rider.Piece.Get();
        if (!IsValid(Piece))
        {
            Riders.RemoveAt(Index);
            continue;
        }
        // Gravity along the rope minus friction, capped.
        Rider.Speed = FMath::Min(MaxRideSpeed, Rider.Speed + (Gravity - 60.0f) * DeltaSeconds);
        Rider.Distance += FMath::Max(Rider.Speed, 80.0f) * DeltaSeconds;
        const FVector OnRope = Upper + Down * FMath::Min(Rider.Distance, Length);
        FVector Origin, Extent;
        Piece->GetActorBounds(false, Origin, Extent);
        Piece->SetActorLocation(Piece->GetActorLocation() + (OnRope - FVector(0, 0, Extent.Z + 15.0f) - Origin),
            false, nullptr, ETeleportType::TeleportPhysics);

        if (Rider.Distance >= Length)
        {
            // Arrived: drop off at the lower end.
            TransferPiece(Piece, EWorldItemOwnerKind::World, FGuid(), Piece->GetActorLocation());
            if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent()))
            {
                Root->SetCollisionProfileName(TEXT("PhysicsActor"));
                Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                Root->SetSimulatePhysics(true);
                Root->SetPhysicsLinearVelocity(Down * 200.0f);
            }
            Riders.RemoveAt(Index);
        }
    }
}
