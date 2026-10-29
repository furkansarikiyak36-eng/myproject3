#include "WildlifeActor.h"

#include "ConstructionHelpers.h"
#include "PhysicalHealthComponent.h"
#include "ShelterSensorComponent.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldObjectIdentityComponent.h"

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
    Identity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldIdentity"));
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

    if (IsValid(Identity))
    {
        const FName IdentityDefinition = Kind == EWildlifeKind::SmallAnimal
            ? FName(TEXT("Wildlife.SmallAnimal"))
            : (Kind == EWildlifeKind::Enemy ? FName(TEXT("Wildlife.Enemy")) : FName(TEXT("Wildlife.Plant")));
        Identity->SetDefinitionId(IdentityDefinition);
    }
}

void AWildlifeActor::BeginPlay()
{
    Super::BeginPlay();
    if (!Body->GetStaticMesh())
        ConfigureKind(Kind);
    if (IsValid(Identity))
        Identity->EnsureIdentity();
    Home = GetActorLocation();
    WanderTarget = Home;
    Health->OnDied.AddUniqueDynamic(this, &AWildlifeActor::HandleDied);
}

bool AWildlifeActor::ApplyAnimalManagementState(
    const FGuid& AnimalId, const FGuid& PenId, const FVector& TargetLocation)
{
    if (!AnimalId.IsValid() || TargetLocation.ContainsNaN() ||
        !IsValid(Identity) || !Identity->EnsureIdentity() ||
        !IsValid(Body) || !IsValid(Health) || Health->IsDead() ||
        Kind != EWildlifeKind::SmallAnimal)
        return false;

    if (bCapturedForAnimalManagement && CapturedAnimalId != AnimalId)
        return false;

    SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
    bCapturedForAnimalManagement = true;
    CapturedAnimalId = AnimalId;
    AnimalPenId = PenId;
    MoveSpeed = 0.0f;
    TimeToNewTarget = 0.0f;
    WanderTarget = GetActorLocation();
    return true;
}

bool AWildlifeActor::TryBiteTarget(APawn* TargetPawn)
{
    if (Kind != EWildlifeKind::Enemy || !IsValid(TargetPawn) ||
        !IsValid(Health) || Health->IsDead() || TimeToNextBiteSeconds > 0.0f ||
        FVector::Dist2D(TargetPawn->GetActorLocation(), GetActorLocation()) > 190.0f)
        return false;

    if (UPhysicalHealthComponent* TargetHealth =
        TargetPawn->FindComponentByClass<UPhysicalHealthComponent>())
    {
        const FVector PushDirection =
            (TargetPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        TargetHealth->ApplyPhysicalDamage(
            8.0f, PushDirection * 110.0f + FVector(0.0f, 0.0f, 35.0f), this);
    }

    // The physical bite still hurts if the survival profile isn't registered,
    // but the configured persistent injury is applied when that profile exists.
    if (const UShelterSensorComponent* Shelter =
        TargetPawn->FindComponentByClass<UShelterSensorComponent>();
        Shelter && Shelter->SurvivalEntityId.IsValid() && GetWorld())
    {
        if (UWorldSurvivalSubsystem* Survival =
            GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>())
        {
            FWorldSurvivalProfile Profile;
            if (Survival->GetProfile(Shelter->SurvivalEntityId, Profile))
            {
                Survival->TryApplyCondition(FGuid::NewGuid(), Profile.EntityId,
                    Profile.Revision, FName(TEXT("Injury.AnimalBite")), 0.5f);
            }
        }
    }

    TimeToNextBiteSeconds = 3.0f;
    return true;
}

void AWildlifeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TimeToNextBiteSeconds = FMath::Max(0.0f, TimeToNextBiteSeconds - FMath::Max(0.0f, DeltaSeconds));
    if (bCapturedForAnimalManagement || Health->IsDead() || Health->IsStaggered() ||
        MoveSpeed <= 0.0f || !GetWorld())
        return;

    FVector Target = WanderTarget;
    if (Kind == EWildlifeKind::Enemy)
    {
        // Enemies approach nearby players and make a bounded melee attack
        // when within reach. The cooldown prevents one hit per frame.
        if (APawn* Player = MyProject3Construction::FindPlayerPawn(GetWorld()))
        {
            const float DistanceToPlayer = FVector::Dist2D(Player->GetActorLocation(), GetActorLocation());
            if (DistanceToPlayer < 2500.0f)
            {
                Target = Player->GetActorLocation();
                if (DistanceToPlayer <= 190.0f && TryBiteTarget(Player))
                    return;
            }
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
