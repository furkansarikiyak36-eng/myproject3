#include "StickBuildingPiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AStickBuildingPiece::AStickBuildingPiece()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StickMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetSimulatePhysics(false);
}

void AStickBuildingPiece::BeginPlay()
{
    Super::BeginPlay();
    bIsPlaced = bStartsPlaced;
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsPlaced && !bIsHeld);
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

bool AStickBuildingPiece::SetHeldState(bool bNewHeld)
{
    if (bNewHeld)
    {
        if (!bCanBeCarried || bIsHeld || bIsPlaced)
        {
            return false;
        }
        bIsHeld = true;
    }
    else
    {
        if (!bIsHeld)
        {
            return true;
        }
        bIsHeld = false;
    }

    if (Mesh)
    {
        Mesh->SetSimulatePhysics(!bIsHeld && !bIsPlaced);
        Mesh->SetCollisionProfileName(
            bIsPlaced ? FName(TEXT("BlockAll")) : FName(TEXT("PhysicsActor")));
    }
    return true;
}

UStaticMesh* AStickBuildingPiece::ResolveOutputMesh(
    EStickProcessingAction Action,
    bool& bOutAmbiguousMapping) const
{
    bOutAmbiguousMapping = false;
    UStaticMesh* SourceMesh = Mesh ? Mesh->GetStaticMesh() : nullptr;
    if (ProcessingMeshSets.Num() == 0 || !IsValid(SourceMesh))
    {
        return nullptr;
    }

    const FStickProcessingMeshSet* Match = nullptr;
    for (const FStickProcessingMeshSet& Set : ProcessingMeshSets)
    {
        if (!IsValid(Set.SourceMesh) || Set.SourceMesh != SourceMesh)
        {
            continue;
        }
        if (Match)
        {
            bOutAmbiguousMapping = true;
            return nullptr;
        }
        Match = &Set;
    }
    if (!Match)
    {
        return nullptr;
    }

    switch (Action)
    {
    case EStickProcessingAction::BreakShort: return Match->ShortStickMesh;
    case EStickProcessingAction::SharpenOneEnd: return Match->SharpenedStakeMesh;
    case EStickProcessingAction::SharpenBothEnds: return Match->DoubleEndedStakeMesh;
    default: return nullptr;
    }
}

bool AStickBuildingPiece::CanProcessStick(EStickProcessingAction Action) const
{
    if (!Mesh || bIsHeld || bIsPlaced || PieceType != EStickPieceType::RawStick)
    {
        return false;
    }
    bool bAmbiguous = false;
    return IsValid(ResolveOutputMesh(Action, bAmbiguous)) && !bAmbiguous;
}

FWorldOperationResult AStickBuildingPiece::TryProcessStick(
    EStickProcessingAction Action,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }
    if (!Mesh || !IsValid(Mesh->GetStaticMesh()))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration, OperationId,
            FText::FromString(TEXT("A source stick mesh must be configured before processing.")));
    }
    if (bIsHeld || bIsPlaced)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidState, OperationId,
            FText::FromString(TEXT("Release and unplace the stick before processing it.")));
    }
    if (PieceType != EStickPieceType::RawStick)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::PrerequisiteNotMet, OperationId,
            FText::FromString(TEXT("Only a raw stick can use this processing operation.")));
    }

    EStickPieceType NewType = EStickPieceType::Other;
    switch (Action)
    {
    case EStickProcessingAction::BreakShort: NewType = EStickPieceType::ShortStick; break;
    case EStickProcessingAction::SharpenOneEnd: NewType = EStickPieceType::SharpenedStake; break;
    case EStickProcessingAction::SharpenBothEnds: NewType = EStickPieceType::DoubleEndedStake; break;
    default:
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Unsupported, OperationId,
            FText::FromString(TEXT("Unsupported stick processing action.")));
    }

    bool bAmbiguous = false;
    UStaticMesh* NewMesh = ResolveOutputMesh(Action, bAmbiguous);
    if (bAmbiguous)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::Conflict, OperationId,
            FText::FromString(TEXT("Multiple stick processing sets match this source mesh.")));
    }
    if (!IsValid(NewMesh))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration, OperationId,
            FText::FromString(TEXT("No output mesh is configured for this stick and action.")));
    }

    UStaticMesh* PreviousMesh = Mesh->GetStaticMesh();
    const FBox OldBounds = Mesh->Bounds.GetBox();
    const double OldBottomZ = OldBounds.Min.Z;
    Mesh->SetStaticMesh(NewMesh);
    Mesh->UpdateBounds();
    const FBox NewBounds = Mesh->Bounds.GetBox();
    if (OldBounds.IsValid && NewBounds.IsValid)
    {
        FVector AdjustedLocation = GetActorLocation();
        AdjustedLocation.Z += OldBottomZ - NewBounds.Min.Z;
        if (!SetActorLocation(AdjustedLocation, false, nullptr, ETeleportType::TeleportPhysics))
        {
            Mesh->SetStaticMesh(PreviousMesh);
            Mesh->UpdateBounds();
            return FWorldOperationResult::Failure(
                EWorldOperationResultCode::InternalError, OperationId,
                FText::FromString(TEXT("Stick processing was rolled back because alignment failed.")));
        }
    }
    PieceType = NewType;
    return FWorldOperationResult::Success(
        OperationId, FText::FromString(TEXT("Stick processing completed.")), TArray<FGuid>());
}

FWorldOperationResult AStickBuildingPiece::TryCommitGroundedStakePlacement(
    const FVector& SurfaceLocation,
    const FRotator& SurfaceRotation,
    bool bGroundContactConfirmed,
    FGuid OperationId)
{
    if (!OperationId.IsValid())
    {
        OperationId = FGuid::NewGuid();
    }
    if (!Mesh || !IsValid(Mesh->GetStaticMesh()))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::MissingConfiguration, OperationId,
            FText::FromString(TEXT("A valid stick mesh is required for placement.")));
    }
    if (bIsHeld || bIsPlaced)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InvalidState, OperationId,
            FText::FromString(TEXT("Only a loose, unheld stake can be placed.")));
    }
    if (PieceType != EStickPieceType::SharpenedStake &&
        PieceType != EStickPieceType::DoubleEndedStake &&
        PieceType != EStickPieceType::FenceStake)
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::PrerequisiteNotMet, OperationId,
            FText::FromString(TEXT("This stick type is not a placeable stake.")));
    }
    if (!bGroundContactConfirmed || !SurfaceLocation.IsFinite() ||
        !SurfaceRotation.IsFinite())
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::UnsupportedSurface, OperationId,
            FText::FromString(TEXT("Stake placement requires a validated ground contact and finite transform.")));
    }

    const FTransform PreviousTransform = GetActorTransform();
    if (!SetActorLocationAndRotation(SurfaceLocation, SurfaceRotation, false, nullptr,
        ETeleportType::TeleportPhysics))
    {
        return FWorldOperationResult::Failure(
            EWorldOperationResultCode::InternalError, OperationId,
            FText::FromString(TEXT("Stake placement transform could not be committed.")));
    }
    bIsPlaced = true;
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    return FWorldOperationResult::Success(
        OperationId, FText::FromString(TEXT("Grounded stake placement committed.")), TArray<FGuid>());
}
