#include "RaidPlaceholderPawn.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ARaidPlaceholderPawn::ARaidPlaceholderPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    UCapsuleComponent* Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    SetRootComponent(Capsule);
    Capsule->InitCapsuleSize(34.0f, 88.0f);
    Capsule->SetCollisionProfileName(TEXT("Pawn"));

    UStaticMeshComponent* Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Capsule);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        Visual->SetStaticMesh(SphereMesh.Object);
        Visual->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
        Visual->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.10f));
    }
}

void ARaidPlaceholderPawn::SetRaidTargetLocation(FVector InTargetLocation)
{
    if (!FMath::IsFinite(InTargetLocation.X) ||
        !FMath::IsFinite(InTargetLocation.Y) ||
        !FMath::IsFinite(InTargetLocation.Z))
    {
        bHasRaidTarget = false;
        CachedPathPoints.Reset();
        CurrentPathPointIndex = 0;
        return;
    }

    if (!bHasRaidTarget || !RaidTargetLocation.Equals(InTargetLocation, 5.0f))
    {
        RaidTargetLocation = InTargetLocation;
        TimeUntilPathRefresh = 0.0f;
        CachedPathPoints.Reset();
        CurrentPathPointIndex = 0;
    }
    bHasRaidTarget = true;
}

bool ARaidPlaceholderPawn::RebuildNavigationPath()
{
    UWorld* World = GetWorld();
    if (!World || !bHasRaidTarget)
        return false;

    UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
    if (!Navigation)
    {
        CachedPathPoints.Reset();
        CurrentPathPointIndex = 0;
        return false;
    }

    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
        this, GetActorLocation(), RaidTargetLocation, this);
    if (!IsValid(Path) || Path->PathPoints.Num() < 2)
    {
        CachedPathPoints.Reset();
        CurrentPathPointIndex = 0;
        return false;
    }

    CachedPathPoints = Path->PathPoints;
    CurrentPathPointIndex = 1;
    return true;
}

void ARaidPlaceholderPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bHasRaidTarget || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f ||
        !FMath::IsFinite(MoveSpeedCmPerSecond) || MoveSpeedCmPerSecond <= 0.0f ||
        !FMath::IsFinite(PathRefreshSeconds) || PathRefreshSeconds < 0.1f ||
        PathRefreshSeconds > 5.0f ||
        !FMath::IsFinite(TargetAcceptanceRadiusCm) || TargetAcceptanceRadiusCm <= 0.0f)
    {
        return;
    }

    TimeUntilPathRefresh -= DeltaSeconds;
    if (TimeUntilPathRefresh <= 0.0f || CachedPathPoints.Num() < 2)
    {
        RebuildNavigationPath();
        TimeUntilPathRefresh = FMath::Clamp(PathRefreshSeconds, 0.1f, 5.0f);
    }

    if (CachedPathPoints.Num() < 2)
        return; // No navmesh/path means no unsafe straight-line teleport fallback.

    const float AcceptanceSquared = FMath::Square(TargetAcceptanceRadiusCm);
    const FVector CurrentLocation = GetActorLocation();
    while (CurrentPathPointIndex < CachedPathPoints.Num() &&
        FVector::DistSquared(CurrentLocation, CachedPathPoints[CurrentPathPointIndex]) <= AcceptanceSquared)
    {
        ++CurrentPathPointIndex;
    }
    if (CurrentPathPointIndex >= CachedPathPoints.Num())
        return;

    const FVector NextPoint = CachedPathPoints[CurrentPathPointIndex];
    const FVector Offset = NextPoint - CurrentLocation;
    const float Distance = Offset.Size();
    if (!FMath::IsFinite(Distance) || Distance <= KINDA_SMALL_NUMBER)
        return;

    const float TravelDistance = FMath::Min(MoveSpeedCmPerSecond * DeltaSeconds, Distance);
    const FVector NewLocation = CurrentLocation + Offset * (TravelDistance / Distance);
    if (!NewLocation.ContainsNaN())
    {
        FHitResult Hit;
        SetActorLocation(NewLocation, true, &Hit, ETeleportType::None);
    }
}
