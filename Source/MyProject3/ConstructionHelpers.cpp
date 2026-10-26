#include "ConstructionHelpers.h"

#include "LogBuildPiece.h"
#include "StructuralPieceInterface.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace MyProject3Construction
{
FVector AxisVector(ELogLongitudinalAxis Axis)
{
    switch (Axis)
    {
    case ELogLongitudinalAxis::Y: return FVector::YAxisVector;
    case ELogLongitudinalAxis::Z: return FVector::ZAxisVector;
    case ELogLongitudinalAxis::X:
    default: return FVector::XAxisVector;
    }
}

bool GetLogGeometry(const ALogBuildPiece* Log, FLogGeometry& Out)
{
    const UStaticMeshComponent* Mesh = Log ? Log->GetLogMesh() : nullptr;
    if (!Mesh || !Mesh->GetStaticMesh())
        return false;
    const FTransform Transform = Mesh->GetComponentTransform();
    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const FVector AxisLocal = AxisVector(Log->LongitudinalAxis);
    const FVector Scale = Transform.GetScale3D().GetAbs();
    const FVector Extent = Bounds.BoxExtent * Scale;
    Out.Centre = Transform.TransformPosition(Bounds.Origin);
    Out.Axis = Transform.TransformVectorNoScale(AxisLocal).GetSafeNormal();
    Out.HalfLength = FVector::DotProduct(Extent, AxisLocal.GetAbs());
    Out.Radius = (Extent * (FVector::OneVector - AxisLocal.GetAbs())).GetMax();
    return Out.HalfLength > KINDA_SMALL_NUMBER;
}

bool IsVerticalLog(const ALogBuildPiece* Log, float MinUp)
{
    FLogGeometry Geometry;
    return GetLogGeometry(Log, Geometry) && FMath::Abs(Geometry.Axis.Z) >= MinUp;
}

bool IsHorizontalLog(const ALogBuildPiece* Log, float MaxUp)
{
    FLogGeometry Geometry;
    return GetLogGeometry(Log, Geometry) && FMath::Abs(Geometry.Axis.Z) <= MaxUp;
}

FVector ClosestPointOnSegment(const FVector& Point, const FVector& From, const FVector& To)
{
    return FMath::ClosestPointOnSegment(Point, From, To);
}

FQuat MakeLogRotation(const ALogBuildPiece* Log, const FVector& WorldAxis, const FVector& UpHint)
{
    const FVector AxisLocal = Log ? AxisVector(Log->LongitudinalAxis) : FVector::XAxisVector;
    const FQuat Frame = FRotationMatrix::MakeFromXZ(WorldAxis.GetSafeNormal(), UpHint).ToQuat();
    return Frame * FQuat::FindBetweenNormals(AxisLocal, FVector::XAxisVector);
}

FTransform MakeLogTransform(const ALogBuildPiece* Log, const FVector& WorldCentre, const FQuat& Rotation)
{
    const UStaticMeshComponent* Mesh = Log ? Log->GetLogMesh() : nullptr;
    const FVector Scale = Log ? Log->GetActorScale3D() : FVector::OneVector;
    const FVector Origin = Mesh && Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetBounds().Origin : FVector::ZeroVector;
    return FTransform(Rotation, WorldCentre - Rotation.RotateVector(Origin * Scale), Scale);
}

static UWorldItemOwnershipSubsystem* GetOwnership(const AActor* Piece)
{
    return Piece && Piece->GetWorld() ? Piece->GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
}

bool GetPieceOwnerKind(const AActor* Piece, EWorldItemOwnerKind& OutKind)
{
    const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership(Piece);
    FWorldOwnedItemRecord Item;
    if (!Structural || !Ownership || !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item))
        return false;
    OutKind = Item.OwnerKind;
    return true;
}

bool TransferPiece(AActor* Piece, EWorldItemOwnerKind To, const FGuid& OwnerId, const FVector& Location)
{
    const IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece);
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership(Piece);
    FWorldOwnedItemRecord Item;
    if (!Structural || !Ownership || !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item))
        return false;
    if (Item.OwnerKind == To && Item.OwnerId == OwnerId)
        return true;
    const bool bNeedsOwner = To == EWorldItemOwnerKind::Held || To == EWorldItemOwnerKind::Inventory ||
        To == EWorldItemOwnerKind::Storage || To == EWorldItemOwnerKind::InTransit;
    FWorldOwnedItemRecord Result;
    return Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision, Item.OwnerKind, To,
        bNeedsOwner ? (OwnerId.IsValid() ? OwnerId : FGuid::NewGuid()) : FGuid(), Location, Result);
}

bool ConsumePiece(AActor* Piece)
{
    if (!IsValid(Piece))
        return false;
    if (!TransferPiece(Piece, EWorldItemOwnerKind::Consumed, FGuid(), Piece->GetActorLocation()))
        return false;
    if (UWorldObjectIdentityComponent* Identity = Piece->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        const EWorldObjectLifecycleState State = Identity->GetLifecycleState();
        if (State == EWorldObjectLifecycleState::Placed)
            Identity->TryTransitionLifecycle(State, EWorldObjectLifecycleState::World);
        Identity->TryTransitionLifecycle(Identity->GetLifecycleState(), EWorldObjectLifecycleState::Consumed);
    }
    Piece->SetActorHiddenInGame(true);
    Piece->SetActorEnableCollision(false);
    Piece->Destroy();
    return true;
}

ALogBuildPiece* SpawnLogCopy(ALogBuildPiece* Source, const FTransform& Transform)
{
    UWorld* World = Source ? Source->GetWorld() : nullptr;
    UStaticMeshComponent* SourceMesh = Source ? Source->GetLogMesh() : nullptr;
    if (!World || !SourceMesh || !SourceMesh->GetStaticMesh())
        return nullptr;
    ALogBuildPiece* Copy = World->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Transform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Copy)
        return nullptr;
    Copy->BaseDefinitionId = Source->BaseDefinitionId;
    Copy->ProcessingMeshSets = Source->ProcessingMeshSets;
    Copy->SelectedProcessingMeshSetId = Source->SelectedProcessingMeshSetId;
    Copy->LongitudinalAxis = Source->LongitudinalAxis;
    Copy->SetPieceStaticMesh(SourceMesh->GetStaticMesh());
    Copy->FinishSpawning(Transform);
    Copy->SetActorScale3D(Transform.GetScale3D());
    if (UMaterialInterface* Material = SourceMesh->GetMaterial(0))
        Copy->GetLogMesh()->SetMaterial(0, Material);
    return Copy;
}

bool TrimLogToRange(ALogBuildPiece* Log, float FromAlong, float ToAlong)
{
    FLogGeometry Geometry;
    if (!GetLogGeometry(Log, Geometry))
        return false;
    const float From = FMath::Clamp(FMath::Min(FromAlong, ToAlong), -Geometry.HalfLength, Geometry.HalfLength);
    const float To = FMath::Clamp(FMath::Max(FromAlong, ToAlong), -Geometry.HalfLength, Geometry.HalfLength);
    const float NewLength = To - From;
    if (NewLength < 5.0f)
        return false;

    const FVector NewCentre = Geometry.Centre + Geometry.Axis * (0.5f * (From + To));
    const FVector AxisLocal = AxisVector(Log->LongitudinalAxis).GetAbs();
    const float Factor = NewLength / Geometry.Length();
    FVector Scale = Log->GetActorScale3D();
    Scale = Scale * (FVector::OneVector - AxisLocal) + Scale * AxisLocal * Factor;
    const FQuat Rotation = Log->GetActorQuat();
    const FVector Origin = Log->GetLogMesh()->GetStaticMesh()->GetBounds().Origin;
    Log->SetActorScale3D(Scale);
    Log->SetActorLocation(NewCentre - Rotation.RotateVector(Origin * Scale), false, nullptr,
        ETeleportType::TeleportPhysics);
    return true;
}

ALogBuildPiece* CutOffRemainder(ALogBuildPiece* Log, float KeepLength, const FVector& DropLocation)
{
    FLogGeometry Geometry;
    if (!GetLogGeometry(Log, Geometry) || KeepLength >= Geometry.Length() - 10.0f || KeepLength < 20.0f)
        return nullptr;
    const float Split = -Geometry.HalfLength + KeepLength;
    ALogBuildPiece* Rest = SpawnLogCopy(Log, Log->GetActorTransform());
    if (!Rest)
        return nullptr;
    TrimLogToRange(Rest, Split, Geometry.HalfLength);
    TrimLogToRange(Log, -Geometry.HalfLength, Split);
    Rest->SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
    return Rest;
}

float EstimateMassKg(const AActor* Piece)
{
    if (!Piece)
        return 0.0f;
    FVector Origin, Extent;
    Piece->GetActorBounds(false, Origin, Extent);
    const float VolumeCm3 = 8.0f * Extent.X * Extent.Y * Extent.Z * 0.6f;
    const bool bWood = Cast<const ALogBuildPiece>(Piece) != nullptr;
    return FMath::Max(0.5f, VolumeCm3 * (bWood ? 0.6f : 2.4f) / 1000.0f);
}

bool IsSmallWoodPiece(const ALogBuildPiece* Log)
{
    if (!Log)
        return false;
    const ELogLengthFraction Fraction = Log->GetLengthFraction();
    if (Fraction == ELogLengthFraction::QuarterLength || Fraction == ELogLengthFraction::EighthLength)
        return true;
    FLogGeometry Geometry;
    return GetLogGeometry(Log, Geometry) && Geometry.Length() <= 160.0f;
}

bool IsSplitBoard(const ALogBuildPiece* Log)
{
    if (!Log)
        return false;
    switch (Log->GetCrossSectionProfile())
    {
    case ELogCrossSectionProfile::Plank:
    case ELogCrossSectionProfile::HalfRound:
    case ELogCrossSectionProfile::QuarterRound:
    case ELogCrossSectionProfile::Hewn:
    case ELogCrossSectionProfile::HewnTwoFaces:
    case ELogCrossSectionProfile::SquareBeam:
        return true;
    default:
        return false;
    }
}

UStaticMesh* LoadEngineShape(const TCHAR* ShapeName)
{
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), ShapeName, ShapeName));
}

UMaterialInterface* MakeTint(UObject* Outer, const FLinearColor& Colour)
{
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Base)
        return nullptr;
    UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Outer);
    if (Instance)
        Instance->SetVectorParameterValue(TEXT("Color"), Colour);
    return Instance;
}

UMaterialInterface* GetGhostMaterial()
{
    return LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/EngineDebugMaterials/WireframeMaterial.WireframeMaterial"));
}

APawn* FindPlayerPawn(const UWorld* World)
{
    const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    return Controller ? Controller->GetPawn() : nullptr;
}
}
