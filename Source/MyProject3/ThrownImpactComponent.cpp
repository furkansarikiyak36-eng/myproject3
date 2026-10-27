#include "ThrownImpactComponent.h"

#include "PhysicalHealthComponent.h"
#include "StructuralPieceInterface.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UThrownImpactComponent::UThrownImpactComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UThrownImpactComponent::Arm(AActor* InThrower, float InMassKg)
{
    Thrower = InThrower;
    MassKg = FMath::Max(0.5f, InMassKg);
    Elapsed = 0.0f;
    if (UPrimitiveComponent* Root = GetOwner() ? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()) : nullptr)
    {
        Root->SetNotifyRigidBodyCollision(true);
        Root->OnComponentHit.AddUniqueDynamic(this, &UThrownImpactComponent::HandleHit);
        LastVelocity = Root->GetPhysicsLinearVelocity();
    }
}

float UThrownImpactComponent::ApplyImpactTo(AActor* Other, float Speed, const FVector& Direction)
{
    if (!IsValid(Other) || Other == GetOwner() || Speed < MinImpactSpeed || AlreadyHit.Contains(Other))
        return 0.0f;
    if (Other == Thrower.Get() && Elapsed < 0.35f)
        return 0.0f; // Leaving the thrower's hands.
    AlreadyHit.Add(Other);

    const float SpeedMs = Speed / 100.0f;
    const float Damage = 0.5f * MassKg * SpeedMs * SpeedMs * DamagePerJoule;
    const FVector Push = Direction.GetSafeNormal() * FMath::Min(Speed * 0.6f, 900.0f) + FVector::UpVector * 150.0f;

    float Dealt = 0.0f;
    if (UPhysicalHealthComponent* Health = Other->FindComponentByClass<UPhysicalHealthComponent>())
        Dealt = Health->ApplyPhysicalDamage(Damage, Push, Thrower.Get());
    UGameplayStatics::ApplyDamage(Other, Damage, nullptr, Thrower.Get(), nullptr);

    // A heavy hit also wears down structures.
    if (const IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Other))
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            bool bExceeded = false;
            Graph->ApplyDamage(Structural->GetStructuralNodeId(), Damage * 0.2f, bExceeded);
        }
    }
    return Dealt > 0.0f ? Dealt : Damage;
}

void UThrownImpactComponent::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    const FVector Velocity = LastVelocity.SizeSquared() > HitComponent->GetPhysicsLinearVelocity().SizeSquared()
        ? LastVelocity : HitComponent->GetPhysicsLinearVelocity();
    ApplyImpactTo(OtherActor, Velocity.Size(), Velocity);
}

void UThrownImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Elapsed += DeltaTime;
    const UPrimitiveComponent* Root = GetOwner() ? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()) : nullptr;
    const FVector Velocity = Root ? Root->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    LastVelocity = Velocity;
    if (Elapsed > MaxLifetime || (Elapsed > 0.5f && Velocity.Size() < 50.0f))
        DestroyComponent();
}
