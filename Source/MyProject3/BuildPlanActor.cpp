#include "BuildPlanActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "LogContainerActor.h"
#include "StructuralPieceInterface.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStorageSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

using namespace MyProject3Construction;

ABuildPlanActor::ABuildPlanActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(false);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ABuildPlanActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bCompletionRetryPending || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
        return;

    CompletionRetryElapsedSeconds += DeltaSeconds;
    if (CompletionRetryElapsedSeconds < 2.0f)
        return;
    CompletionRetryElapsedSeconds = 0.0f;

    if (!IsComplete())
    {
        bCompletionRetryPending = false;
        SetActorTickEnabled(false);
        return;
    }

    if (Complete())
    {
        bCompletionRetryPending = false;
        SetActorTickEnabled(false);
    }
    else if (!bRetryAfterOutputSpawnFailure)
    {
        // Data/ownership errors need intervention rather than an endless retry loop.
        bCompletionRetryPending = false;
        SetActorTickEnabled(false);
    }
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
    if (!Slots.IsValidIndex(Index) || Slots[Index].bFilled ||
        !IsValid(Piece) || !Piece->IsPlaced())
        return;

    Slots[Index].bFilled = true;
    Slots[Index].Piece = Piece;
    if (Ghosts.IsValidIndex(Index) && Ghosts[Index])
    {
        Ghosts[Index]->SetVisibility(false);
        Ghosts[Index]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (IsComplete() && !Complete())
    {
        bCompletionRetryPending = bRetryAfterOutputSpawnFailure;
        CompletionRetryElapsedSeconds = 0.0f;
        SetActorTickEnabled(bCompletionRetryPending);
    }
}

bool ABuildPlanActor::Complete()
{
    bRetryAfterOutputSpawnFailure = false;
    UWorld* World = GetWorld();
    if (!World || !IsComplete())
        return false;

    ELogContainerKind ContainerKind = ELogContainerKind::SmallLogStorage;
    bool bSpawnContainer = false;
    switch (Kind)
    {
    case EBuildPlanKind::SmallLogStorage: bSpawnContainer = true; break;
    case EBuildPlanKind::LargeLogStorage: ContainerKind = ELogContainerKind::LargeLogStorage; bSpawnContainer = true; break;
    case EBuildPlanKind::LogSled: ContainerKind = ELogContainerKind::BasicSled; bSpawnContainer = true; break;
    default: break;
    }

    TArray<ALogBuildPiece*> Pieces;
    TSet<const ALogBuildPiece*> UniquePieces;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        ALogBuildPiece* Piece = Slots[Index].Piece.Get();
        if (!IsValid(Piece) || !Piece->IsPlaced() || UniquePieces.Contains(Piece))
        {
            // Re-open the invalid slot explicitly; a duplicated actor cannot
            // satisfy two physical requirements and an unplaced actor is not a commit.
            Slots[Index].bFilled = false;
            Slots[Index].Piece.Reset();
            if (Ghosts.IsValidIndex(Index) && Ghosts[Index])
            {
                Ghosts[Index]->SetVisibility(true);
                Ghosts[Index]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            }
            for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
            {
                if (!Slots[SlotIndex].Piece.IsValid())
                {
                    Slots[SlotIndex].bFilled = false;
                    Slots[SlotIndex].Piece.Reset();
                    if (Ghosts.IsValidIndex(SlotIndex) && Ghosts[SlotIndex])
                    {
                        Ghosts[SlotIndex]->SetVisibility(true);
                        Ghosts[SlotIndex]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                    }
                }
            }
            UE_LOG(LogTemp, Warning, TEXT("Build plan '%s' cannot complete: a slot piece is missing, unplaced or duplicated."),
                *GetNameSafe(this));
            return false;
        }
        UniquePieces.Add(Piece);
        Pieces.Add(Piece);
    }

    UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
    UWorldItemOwnershipSubsystem* Ownership = World->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldStorageSubsystem* Storage = World->GetSubsystem<UWorldStorageSubsystem>();
    if (!Graph || !Ownership || !Storage)
    {
        UE_LOG(LogTemp, Error, TEXT("Build plan '%s' cannot complete: required world systems are unavailable."),
            *GetNameSafe(this));
        return false;
    }

    TArray<ALogBuildPiece*> ConsumeOrder;
    if (Kind == EBuildPlanKind::LogSled)
    {
        TMap<FGuid, ALogBuildPiece*> PiecesByNodeId;
        TSet<FGuid> RemainingNodeIds;
        for (ALogBuildPiece* Piece : Pieces)
        {
            IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece);
            UWorldObjectIdentityComponent* Identity =
                Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
            FWorldOwnedItemRecord Item;
            FStructuralNodeRecord Node;
            if (!Structural || !Identity || !Identity->HasValidIdentity() ||
                Identity->GetLifecycleState() != EWorldObjectLifecycleState::Placed ||
                Identity->StateRevision > MAX_int64 - 2 ||
                !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item) ||
                Item.OwnerKind != EWorldItemOwnerKind::World || Item.Quantity != 1 ||
                Item.Revision == MAX_int64 ||
                !Graph->GetNode(Structural->GetStructuralNodeId(), Node))
            {
                UE_LOG(LogTemp, Error, TEXT("Build plan '%s' cannot consume invalid or non-world-owned sled material."),
                    *GetNameSafe(this));
                return false;
            }
            PiecesByNodeId.Add(Node.NodeId, Piece);
            RemainingNodeIds.Add(Node.NodeId);
        }

        // All supports must belong to the plan. Destroy structural leaves first.
        for (ALogBuildPiece* Piece : Pieces)
        {
            for (const FGuid& DependentId : Graph->GetAllDependents(Piece->GetStructuralNodeId()))
            {
                if (!RemainingNodeIds.Contains(DependentId))
                {
                    UE_LOG(LogTemp, Error, TEXT("Build plan '%s' cannot consume a sled part supporting an external piece."),
                        *GetNameSafe(this));
                    return false;
                }
            }
        }

        while (!RemainingNodeIds.IsEmpty())
        {
            FGuid LeafId;
            for (const FGuid& CandidateId : RemainingNodeIds)
            {
                bool bHasRemainingDependent = false;
                for (const FGuid& DependentId : Graph->GetAllDependents(CandidateId))
                {
                    if (RemainingNodeIds.Contains(DependentId))
                    {
                        bHasRemainingDependent = true;
                        break;
                    }
                }
                if (!bHasRemainingDependent)
                {
                    LeafId = CandidateId;
                    break;
                }
            }
            if (!LeafId.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Build plan '%s' cannot consume cyclic structural dependencies."),
                    *GetNameSafe(this));
                return false;
            }
            ConsumeOrder.Add(PiecesByNodeId.FindRef(LeafId));
            RemainingNodeIds.Remove(LeafId);
        }
    }

    ALogContainerActor* Container = nullptr;
    if (bSpawnContainer)
    {
        const FVector Location = GetActorLocation() +
            FVector(0, 0, Kind == EBuildPlanKind::LogSled ? 20.0f : 45.0f);
        const FTransform SpawnTransform(GetActorRotation(), Location);
        Container = World->SpawnActorDeferred<ALogContainerActor>(
            ALogContainerActor::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Container)
        {
            bRetryAfterOutputSpawnFailure = true;
            UE_LOG(LogTemp, Warning, TEXT("Build plan '%s' will retry because output container allocation failed."),
                *GetNameSafe(this));
            return false;
        }

        Container->ConfigureKind(ContainerKind);
        Container->FinishSpawning(FTransform(GetActorRotation(), Location, Container->GetActorScale3D()));
        if (!IsValid(Container) || !Storage->IsStorageAvailable(Container->GetStorageId()))
        {
            if (IsValid(Container))
                Container->Destroy();
            bRetryAfterOutputSpawnFailure = true;
            UE_LOG(LogTemp, Warning, TEXT("Build plan '%s' will retry because the output container could not register storage."),
                *GetNameSafe(this));
            return false;
        }
    }

    if (Kind == EBuildPlanKind::LogSled)
    {
        for (ALogBuildPiece* Piece : ConsumeOrder)
        {
            if (!IsValid(Piece) || !ConsumePiece(Piece))
            {
                if (IsValid(Container))
                    Container->Destroy();

                // Any successfully consumed piece is now a missing resource slot;
                // reopen it so the player can repair the interrupted build.
                for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
                {
                    if (!Slots[SlotIndex].Piece.IsValid())
                    {
                        Slots[SlotIndex].bFilled = false;
                        Slots[SlotIndex].Piece.Reset();
                        if (Ghosts.IsValidIndex(SlotIndex) && Ghosts[SlotIndex])
                        {
                            Ghosts[SlotIndex]->SetVisibility(true);
                            Ghosts[SlotIndex]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                        }
                    }
                }
                UE_LOG(LogTemp, Error, TEXT("Build plan '%s' could not consume all sled materials; missing slots were reopened."),
                    *GetNameSafe(this));
                return false;
            }
        }
    }

    Destroy();
    return true;
}
