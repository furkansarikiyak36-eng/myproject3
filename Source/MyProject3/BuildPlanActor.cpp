#include "BuildPlanActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "LogContainerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

using namespace MyProject3Construction;

ABuildPlanActor::ABuildPlanActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

FString ABuildPlanActor::GetPlanName(EBuildPlanKind InKind)
{
    const UEnum* Enum = StaticEnum<EBuildPlanKind>();
    return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(InKind)).ToString() : TEXT("Plan");
}

void ABuildPlanActor::AddSlot(const FVector& Centre, const FRotator& Direction, float Length, int32 Support)
{
    FBuildPlanSlot& Slot = Slots.AddDefaulted_GetRef();
    Slot.Local = FTransform(Direction, Centre);
    Slot.LengthCm = Length;
    Slot.SupportSlot = Support;
}

void ABuildPlanActor::InitializePlan(EBuildPlanKind InKind)
{
    Kind = InKind;
    Slots.Reset();
    const FRotator AlongX(0, 0, 0), AlongY(0, 90, 0), Up(90, 0, 0);
    constexpr float R = 18.0f; // Typical log radius used to stack slots.
    switch (Kind)
    {
    case EBuildPlanKind::LogWall:
        for (int32 Row = 0; Row < 5; ++Row)
            AddSlot(FVector(0, 0, R + Row * 2.0f * R * 0.9f), AlongY, 300.0f, Row == 0 ? INDEX_NONE : Row - 1);
        break;
    case EBuildPlanKind::LeanToShelter:
        AddSlot(FVector(0, -150, 100), Up, 200.0f, INDEX_NONE);
        AddSlot(FVector(0, 150, 100), Up, 200.0f, INDEX_NONE);
        AddSlot(FVector(0, 0, 200 + R), AlongY, 340.0f, 0);
        for (int32 Index = 0; Index < 5; ++Index)
            AddSlot(FVector(-95, -140 + Index * 70.0f, 110), FRotator(-48, 0, 0), 280.0f, 2);
        break;
    case EBuildPlanKind::Watchtower:
        AddSlot(FVector(-100, -100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(100, -100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(-100, 100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(100, 100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(0, -100, 400 + R), AlongX, 260.0f, 0);
        AddSlot(FVector(0, 100, 400 + R), AlongX, 260.0f, 2);
        for (int32 Index = 0; Index < 6; ++Index)
            AddSlot(FVector(-100 + Index * 40.0f, 0, 400 + 3.0f * R), AlongY, 260.0f, 4);
        // Ladder rails up one side.
        AddSlot(FVector(-210, -40, 200), FRotator(70, 0, 0), 440.0f, INDEX_NONE);
        AddSlot(FVector(-210, 40, 200), FRotator(70, 0, 0), 440.0f, INDEX_NONE);
        break;
    case EBuildPlanKind::SmallLogStorage:
    case EBuildPlanKind::LargeLogStorage:
    {
        const float HalfDepth = Kind == EBuildPlanKind::LargeLogStorage ? 120.0f : 70.0f;
        const float PostHeight = Kind == EBuildPlanKind::LargeLogStorage ? 200.0f : 120.0f;
        AddSlot(FVector(0, -HalfDepth, R), AlongX, 300.0f, INDEX_NONE);
        AddSlot(FVector(0, HalfDepth, R), AlongX, 300.0f, INDEX_NONE);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float X = Index < 2 ? -140.0f : 140.0f;
            const float Y = Index % 2 == 0 ? -HalfDepth : HalfDepth;
            AddSlot(FVector(X, Y, 2.0f * R + 0.5f * PostHeight), Up, PostHeight, Index % 2);
        }
        break;
    }
    case EBuildPlanKind::LogSled:
        AddSlot(FVector(0, -50, R), AlongX, 250.0f, INDEX_NONE);
        AddSlot(FVector(0, 50, R), AlongX, 250.0f, INDEX_NONE);
        AddSlot(FVector(-80, 0, 3.0f * R), AlongY, 150.0f, 0);
        AddSlot(FVector(80, 0, 3.0f * R), AlongY, 150.0f, 0);
        break;
    case EBuildPlanKind::DefenseWall:
        for (int32 Index = 0; Index < 8; ++Index)
            AddSlot(FVector(0, -140 + Index * 40.0f, 150), Up, 300.0f, INDEX_NONE);
        break;
    }

    UStaticMesh* Cylinder = LoadEngineShape(TEXT("Cylinder"));
    UMaterialInterface* Ghost = GetGhostMaterial();
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->RegisterComponent();
        Mesh->SetStaticMesh(Cylinder);
        if (Ghost)
            Mesh->SetMaterial(0, Ghost);
        // Cylinder is 100 cm along Z: turn it to lie along the slot's X.
        const FTransform& Local = Slots[Index].Local;
        const FQuat ZToX = FQuat::FindBetweenNormals(FVector::ZAxisVector, FVector::XAxisVector);
        Mesh->SetRelativeTransform(FTransform(Local.GetRotation() * ZToX, Local.GetLocation(),
            FVector(0.36f, 0.36f, Slots[Index].LengthCm / 100.0f)));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Ghosts.Add(Mesh);
    }
}

int32 ABuildPlanActor::GetFilledCount() const
{
    int32 Count = 0;
    for (const FBuildPlanSlot& Slot : Slots)
        Count += Slot.bFilled ? 1 : 0;
    return Count;
}

FTransform ABuildPlanActor::GetSlotWorld(int32 Index) const
{
    return Slots.IsValidIndex(Index) ? Slots[Index].Local * GetActorTransform() : FTransform::Identity;
}

int32 ABuildPlanActor::FindSlot(const UPrimitiveComponent* Component, const FVector& WorldPoint) const
{
    const int32 Direct = Ghosts.IndexOfByPredicate([Component](const TObjectPtr<UStaticMeshComponent>& Ghost)
    {
        return Ghost.Get() == Component;
    });
    if (Slots.IsValidIndex(Direct) && !Slots[Direct].bFilled)
        return Direct;
    int32 Best = INDEX_NONE;
    float BestDistance = 150.0f;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        const float Distance = FVector::Dist(GetSlotWorld(Index).GetLocation(), WorldPoint);
        if (!Slots[Index].bFilled && Distance < BestDistance)
        {
            Best = Index;
            BestDistance = Distance;
        }
    }
    return Best;
}

bool ABuildPlanActor::CanFillSlot(int32 Index, FString& OutReason) const
{
    if (!Slots.IsValidIndex(Index) || Slots[Index].bFilled)
    {
        OutReason = TEXT("That part of the plan is already built.");
        return false;
    }
    const int32 Support = Slots[Index].SupportSlot;
    if (Support != INDEX_NONE && (!Slots[Support].bFilled || !Slots[Support].Piece.IsValid()))
    {
        OutReason = TEXT("Build the part it rests on first.");
        return false;
    }
    return true;
}

ALogBuildPiece* ABuildPlanActor::GetSupportPiece(int32 Index) const
{
    if (!Slots.IsValidIndex(Index) || Slots[Index].SupportSlot == INDEX_NONE)
        return nullptr;
    return Slots[Slots[Index].SupportSlot].Piece.Get();
}

void ABuildPlanActor::MarkFilled(int32 Index, ALogBuildPiece* Piece)
{
    if (!Slots.IsValidIndex(Index))
        return;
    Slots[Index].bFilled = true;
    Slots[Index].Piece = Piece;
    if (Ghosts.IsValidIndex(Index) && Ghosts[Index])
    {
        Ghosts[Index]->SetVisibility(false);
        Ghosts[Index]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (IsComplete())
        Complete();
}

void ABuildPlanActor::Complete()
{
    UWorld* World = GetWorld();
    ELogContainerKind ContainerKind = ELogContainerKind::SmallLogStorage;
    bool bSpawnContainer = false;
    switch (Kind)
    {
    case EBuildPlanKind::SmallLogStorage: bSpawnContainer = true; break;
    case EBuildPlanKind::LargeLogStorage: ContainerKind = ELogContainerKind::LargeLogStorage; bSpawnContainer = true; break;
    case EBuildPlanKind::LogSled: ContainerKind = ELogContainerKind::BasicSled; bSpawnContainer = true; break;
    default: break;
    }
    if (bSpawnContainer && World)
    {
        // A sled is a single movable object: its logs become the sled.
        if (Kind == EBuildPlanKind::LogSled)
        {
            for (FBuildPlanSlot& Slot : Slots)
                ConsumePiece(Slot.Piece.Get());
        }
        const FVector Location = GetActorLocation() + FVector(0, 0, Kind == EBuildPlanKind::LogSled ? 20.0f : 45.0f);
        ALogContainerActor* Container = World->SpawnActorDeferred<ALogContainerActor>(ALogContainerActor::StaticClass(),
            FTransform(GetActorRotation(), Location));
        if (Container)
        {
            Container->ConfigureKind(ContainerKind);
            Container->FinishSpawning(FTransform(GetActorRotation(), Location, Container->GetActorScale3D()));
        }
    }
    Destroy();
}
