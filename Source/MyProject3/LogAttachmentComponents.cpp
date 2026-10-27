#include "LogAttachmentComponents.h"

#include "ConstructionHelpers.h"
#include "HarvestableTreeActor.h"
#include "LogBuildPiece.h"
#include "PhysicalHealthComponent.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

using namespace MyProject3Construction;

USharpenedTipComponent::USharpenedTipComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
}

bool USharpenedTipComponent::InitializeTip()
{
    ALogBuildPiece* Log = Cast<ALogBuildPiece>(GetOwner());
    FLogGeometry Geometry;
    if (!GetLogGeometry(Log, Geometry))
        return false;
    const FVector Up = Geometry.Axis.Z >= 0.0f ? Geometry.Axis : -Geometry.Axis;
    const FVector Top = Geometry.Centre + Up * Geometry.HalfLength;
    TipRadius = Geometry.Radius;
    const float TipHeight = FMath::Max(25.0f, Geometry.Radius * 2.2f);
    TipLocation = Top + Up * TipHeight;

    TipMesh = NewObject<UStaticMeshComponent>(Log);
    TipMesh->SetupAttachment(Log->GetRootComponent());
    TipMesh->SetUsingAbsoluteScale(true);
    TipMesh->RegisterComponent();
    TipMesh->SetStaticMesh(LoadEngineShape(TEXT("Cone")));
    if (UMaterialInterface* Material = Log->GetLogMesh()->GetMaterial(0))
        TipMesh->SetMaterial(0, Material);
    TipMesh->SetCollisionProfileName(TEXT("BlockAll"));
    // The engine cone is 100 cm tall with its base at -50.
    TipMesh->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Up).ToQuat(), Top + Up * (0.5f * TipHeight),
        FVector(2.0f * Geometry.Radius / 100.0f, 2.0f * Geometry.Radius / 100.0f, TipHeight / 100.0f)));
    Log->Tags.AddUnique(TEXT("SharpenedTip"));
    return true;
}

void USharpenedTipComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Cooldown -= DeltaTime;
    if (Cooldown > 0.0f || !TipMesh || !GetWorld())
        return;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(StakeTip), false, GetOwner());
    GetWorld()->OverlapMultiByObjectType(Overlaps, TipLocation, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(TipRadius + 45.0f), Params);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        ACharacter* Climber = Cast<ACharacter>(Overlap.GetActor());
        if (!Climber)
            continue;
        // Push the climber back off the points and hurt them.
        FVector Away = Climber->GetActorLocation() - TipLocation;
        Away.Z = 0.0f;
        const FVector Push = Away.GetSafeNormal() * RepelSpeed + FVector::UpVector * 120.0f;
        if (UPhysicalHealthComponent* Health = Climber->FindComponentByClass<UPhysicalHealthComponent>())
            Health->ApplyPhysicalDamage(ClimbDamage, Push, GetOwner());
        else
            Climber->LaunchCharacter(Push, true, true);
        Cooldown = 0.6f;
    }
}

UTreeAnchorComponent::UTreeAnchorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTreeAnchorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    TimeToCheck -= DeltaTime;
    if (TimeToCheck > 0.0f)
        return;
    TimeToCheck = 0.5f;
    const AHarvestableTreeActor* Standing = Tree.Get();
    if (Standing && Standing->CanBeChopped())
        return;

    // The trunk is gone or falling: the log loses its anchor.
    if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(GetOwner()))
    {
        if (UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
            Graph->SetGroundSupported(Log->GetStructuralNodeId(), false);
        Log->DetachFromSnapParent(true);
        Log->SetPlacedState(false);
        if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::World);
    }
    DestroyComponent();
}
