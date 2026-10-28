#include "WildlifeActor.h"

#include "ConstructionHelpers.h"
#include "PhysicalHealthComponent.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldSimulationClockSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AWildlifeActor::AWildlifeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Body->SetNotifyRigidBodyCollision(true);
    Health = CreateDefaultSubobject<UPhysicalHealthComponent>(TEXT("Health"));
    Identity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));
}

void AWildlifeActor::ConfigureKind(EWildlifeKind NewKind)
{
    using namespace MyProject3Construction;
    Kind = NewKind;
    switch (Kind)
    {
    case EWildlifeKind::SmallAnimal:
        Body->SetStaticMesh(LoadEngineShape(TEXT("Sphere")));
        SetActorScale3D(FVector(0.45f, 0.3f, 0.3f));
        Body->SetMaterial(0, MakeTint(this, FLinearColor(0.45f, 0.3f, 0.18f)));
        Health->MaxHealth = Health->Health = 20.0f;
        MoveSpeed = 220.0f;
        break;
    case EWildlifeKind::Enemy:
        Body->SetStaticMesh(LoadEngineShape(TEXT("Cylinder")));
        SetActorScale3D(FVector(0.6f, 0.6f, 1.8f));
        Body->SetMaterial(0, MakeTint(this, FLinearColor(0.6f, 0.08f, 0.06f)));
        Health->MaxHealth = Health->Health = 120.0f;
        Health->StaggerSeconds = 1.5f;
        MoveSpeed = 160.0f;
        break;
    case EWildlifeKind::Plant:
        Body->SetStaticMesh(LoadEngineShape(TEXT("Cone")));
        SetActorScale3D(FVector(0.8f, 0.8f, 0.9f));
        Body->SetMaterial(0, MakeTint(this, FLinearColor(0.05f, 0.14f, 0.04f)));
        Health->MaxHealth = Health->Health = 15.0f;
        MoveSpeed = 0.0f;
        break;
    }
}

void AWildlifeActor::BeginPlay()
{
    Super::BeginPlay();
    if (!Body->GetStaticMesh())
        ConfigureKind(Kind);
    if (Identity)
        Identity->EnsureIdentity();
    if (!bWanderStateInitialized)
    {
        Home = GetActorLocation();
        WanderTarget = Home;
        TimeToNewTarget = 0.0f;
        MovementAccumulatorSeconds = 0.0;
        bWanderStateInitialized = true;
    }
    if (Health)
        Health->OnDied.AddUniqueDynamic(this, &AWildlifeActor::HandleDied);
}


int32 AWildlifeActor::GetWanderSeed() const
{
    UWorldSimulationClockSubsystem* Clock =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (Clock && Identity && Identity->HasValidIdentity())
        return Clock->GetDeterministicSeedForOperation(Identity->GetInstanceId());

    // Deterministic fallback for actors initialized before the world clock.
    const FGuid Id = Identity ? Identity->GetInstanceId() : FGuid();
    const uint32 Mixed = Id.A ^ (Id.B * 0x9e3779b9u) ^ (Id.C * 0x85ebca6bu) ^ Id.D;
    return static_cast<int32>(Mixed);
}

void AWildlifeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Health || Health->IsDead() || Health->IsStaggered() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f ||
        !FMath::IsFinite(MoveSpeed) || MoveSpeed <= 0.0f || !GetWorld())
        return;

    // Accumulate a fixed 0.1-second movement step instead of integrating different
    // frame deltas directly. Backlog is preserved rather than silently discarded.
    constexpr double FixedMovementStep = 0.1;
    constexpr int32 MaxStepsPerFrame = 10;
    MovementAccumulatorSeconds += static_cast<double>(DeltaSeconds);
    if (!FMath::IsFinite(MovementAccumulatorSeconds))
        return;

    int32 Steps = 0;
    while (MovementAccumulatorSeconds >= FixedMovementStep && Steps < MaxStepsPerFrame)
    {
        SimulateMovementStep(static_cast<float>(FixedMovementStep));
        MovementAccumulatorSeconds -= FixedMovementStep;
        ++Steps;
    }
}

void AWildlifeActor::SimulateMovementStep(float StepSeconds)
{
    if (!GetWorld() || !Identity || !Identity->HasValidIdentity() ||
        !FMath::IsFinite(StepSeconds) || StepSeconds <= 0.0f ||
        !FMath::IsFinite(WanderRadius) || WanderRadius < 0.0f ||
        !FMath::IsFinite(MoveSpeed) || MoveSpeed <= 0.0f)
        return;

    FVector Target = WanderTarget;
    if (Kind == EWildlifeKind::Enemy)
    {
        // Enemy chase remains target-driven, but its motion advances in fixed steps.
        if (const APawn* Player = MyProject3Construction::FindPlayerPawn(GetWorld()))
        {
            if (FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 2500.0f)
                Target = Player->GetActorLocation();
        }
    }
    else
    {
        TimeToNewTarget -= StepSeconds;
        if (TimeToNewTarget <= 0.0f || FVector::Dist2D(GetActorLocation(), WanderTarget) < 40.0f)
        {
            if (WanderSequence == MAX_int64)
                return;

            // A per-instance stream avoids FMath's global RNG and call-order coupling.
            const uint32 Sequence = static_cast<uint32>(WanderSequence);
            FRandomStream Stream(GetWanderSeed() ^ static_cast<int32>(Sequence * 196613u));
            const float Angle = Stream.FRandRange(0.0f, 2.0f * PI);
            const float Radius = FMath::Sqrt(Stream.FRand()) * WanderRadius;
            WanderTarget = Home + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
            TimeToNewTarget = Stream.FRandRange(2.0f, 5.0f);
            ++WanderSequence;
        }
        Target = WanderTarget;
    }

    FVector ToTarget = Target - GetActorLocation();
    ToTarget.Z = 0.0f;
    if (ToTarget.Size() < 150.0f)
        return;
    FVector Next = GetActorLocation() + ToTarget.GetSafeNormal() * MoveSpeed * StepSeconds;
    if (!FMath::IsFinite(Next.X) || !FMath::IsFinite(Next.Y) || !FMath::IsFinite(Next.Z))
        return;

    // Follow the ground under the body.
    FVector Origin, Extent;
    GetActorBounds(true, Origin, Extent);
    FHitResult Ground;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WildlifeGround), false, this);
    if (GetWorld()->LineTraceSingleByChannel(Ground, Next + FVector(0, 0, 200), Next - FVector(0, 0, 600),
        ECC_Visibility, Params))
    {
        Next.Z = Ground.ImpactPoint.Z + (GetActorLocation().Z - (Origin.Z - Extent.Z));
    }
    SetActorLocationAndRotation(Next, ToTarget.Rotation(), true);
}

void AWildlifeActor::HandleDied(AActor* Causer)
{
    using namespace MyProject3Construction;
    switch (Kind)
    {
    case EWildlifeKind::SmallAnimal:
        // Hunted: lies on its side and can be collected later.
        SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 90.0f));
        Body->SetMaterial(0, MakeTint(this, FLinearColor(0.2f, 0.12f, 0.08f)));
        break;
    case EWildlifeKind::Enemy:
        Body->SetCollisionProfileName(TEXT("PhysicsActor"));
        Body->SetSimulatePhysics(true);
        break;
    case EWildlifeKind::Plant:
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        break;
    }
}
