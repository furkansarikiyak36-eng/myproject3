#include "WildlifeActor.h"

#include "ConstructionHelpers.h"
#include "PhysicalHealthComponent.h"

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
        Body->SetMaterial(0, MakeTint(this, FLinearColor(0.12f, 0.45f, 0.1f)));
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
    Home = GetActorLocation();
    WanderTarget = Home;
    Health->OnDied.AddUniqueDynamic(this, &AWildlifeActor::HandleDied);
}

void AWildlifeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Health->IsDead() || Health->IsStaggered() || MoveSpeed <= 0.0f || !GetWorld())
        return;

    FVector Target = WanderTarget;
    if (Kind == EWildlifeKind::Enemy)
    {
        // Enemies walk toward a nearby player and stop at arm's length.
        if (const APawn* Player = MyProject3Construction::FindPlayerPawn(GetWorld()))
        {
            if (FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 2500.0f)
                Target = Player->GetActorLocation();
        }
    }
    else
    {
        TimeToNewTarget -= DeltaSeconds;
        if (TimeToNewTarget <= 0.0f || FVector::Dist2D(GetActorLocation(), WanderTarget) < 40.0f)
        {
            const FVector2D Offset = FMath::RandPointInCircle(WanderRadius);
            WanderTarget = Home + FVector(Offset.X, Offset.Y, 0.0f);
            TimeToNewTarget = FMath::FRandRange(2.0f, 5.0f);
        }
    }

    FVector ToTarget = Target - GetActorLocation();
    ToTarget.Z = 0.0f;
    if (ToTarget.Size() < 150.0f)
        return;
    FVector Next = GetActorLocation() + ToTarget.GetSafeNormal() * MoveSpeed * DeltaSeconds;

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
