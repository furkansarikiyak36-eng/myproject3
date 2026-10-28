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
    if (!IsValid(Piece) || !Piece->GetWorld())
        return false;

    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece);
    UWorldItemOwnershipSubsystem* Ownership = GetOwnership(Piece);
    FWorldOwnedItemRecord BeforeItem;
    if (!Identity || !Identity->HasValidIdentity() || !Structural || !Ownership ||
        !Ownership->TryGetItem(Structural->GetStructuralNodeId(), BeforeItem) ||
        (BeforeItem.OwnerKind != EWorldItemOwnerKind::World &&
         BeforeItem.OwnerKind != EWorldItemOwnerKind::Held) ||
        BeforeItem.Quantity != 1 || BeforeItem.Revision == MAX_int64)
        return false;

    const EWorldObjectLifecycleState BeforeLifecycle = Identity->GetLifecycleState();
    const int64 BeforeLifecycleRevision = Identity->StateRevision;
    const FVector BeforeLocation = Piece->GetActorLocation();
    const bool bLifecycleConsumable =
        BeforeLifecycle == EWorldObjectLifecycleState::World ||
        BeforeLifecycle == EWorldObjectLifecycleState::Held ||
        BeforeLifecycle == EWorldObjectLifecycleState::Placed;
    const int64 RequiredTransitions =
        BeforeLifecycle == EWorldObjectLifecycleState::Placed ? 2 : 1;
    if (!bLifecycleConsumable ||
        BeforeLifecycleRevision < 0 ||
        BeforeLifecycleRevision > MAX_int64 - RequiredTransitions)
        return false;

    if (!TransferPiece(Piece, EWorldItemOwnerKind::Consumed, FGuid(), BeforeLocation))
        return false;

    bool bLifecycleCommitted = true;
    if (BeforeLifecycle == EWorldObjectLifecycleState::Placed)
        bLifecycleCommitted = Identity->TryTransitionLifecycle(
            EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::World);
    if (bLifecycleCommitted)
        bLifecycleCommitted = Identity->TryTransitionLifecycle(
            BeforeLifecycle == EWorldObjectLifecycleState::Placed
                ? EWorldObjectLifecycleState::World : BeforeLifecycle,
            EWorldObjectLifecycleState::Consumed);

    auto Rollback = [&]()
    {
        Identity->LifecycleState = BeforeLifecycle;
        Identity->StateRevision = BeforeLifecycleRevision;
        FWorldOwnedItemRecord Restored;
        FWorldOwnedItemRecord Current;
        if (!Ownership->TryGetItem(BeforeItem.ItemId, Current) ||
            Current.OwnerKind != EWorldItemOwnerKind::Consumed)
            return false;
        return Ownership->TryTransferItem(FGuid::NewGuid(), Current.ItemId, Current.Revision,
            EWorldItemOwnerKind::Consumed, BeforeItem.OwnerKind, BeforeItem.OwnerId,
            BeforeItem.WorldLocation, Restored);
    };

    if (!bLifecycleCommitted)
    {
        if (!Rollback())
        {
            Piece->SetActorHiddenInGame(true);
            Piece->SetActorEnableCollision(false);
            UE_LOG(LogTemp, Error, TEXT("Consumption rollback failed for '%s'; item ownership needs recovery."),
                *GetNameSafe(Piece));
        }
        return false;
    }

    Piece->SetActorHiddenInGame(true);
    Piece->SetActorEnableCollision(false);
    if (!Piece->Destroy())
    {
        if (!Rollback())
        {
            UE_LOG(LogTemp, Error, TEXT("Failed actor destroy and consumption rollback for '%s'."),
                *GetNameSafe(Piece));
        }
        else
        {
            Piece->SetActorHiddenInGame(false);
            Piece->SetActorEnableCollision(true);
        }
        return false;
    }
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
    Copy->bHasCustomLengthScale = Source->bHasCustomLengthScale;
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
    if (!FMath::IsNearlyEqual(Factor, 1.0f, 0.001f))
        Log->bHasCustomLengthScale = true;
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
    if (!GetLogGeometry(Log, Geometry) || !FMath::IsFinite(KeepLength) ||
        !FMath::IsFinite(DropLocation.X) || !FMath::IsFinite(DropLocation.Y) ||
        !FMath::IsFinite(DropLocation.Z) ||
        KeepLength >= Geometry.Length() - 10.0f || KeepLength < 20.0f)
        return nullptr;

    const FTransform SourceTransform = Log->GetActorTransform();
    const bool bSourceHadCustomLengthScale = Log->bHasCustomLengthScale;
    const float OriginalLength = Geometry.Length();
    const float Split = -Geometry.HalfLength + KeepLength;
    ALogBuildPiece* Rest = SpawnLogCopy(Log, SourceTransform);
    if (!Rest)
        return nullptr;

    // A two-output cut is one logical operation. Never leave a full duplicate or
    // a partially trimmed source when either geometry transformation fails.
    const bool bRestTrimmed = TrimLogToRange(Rest, Split, Geometry.HalfLength);
    const bool bSourceTrimmed = bRestTrimmed && TrimLogToRange(Log, -Geometry.HalfLength, Split);
    FLogGeometry KeptGeometry;
    FLogGeometry RestGeometry;
    const bool bLengthsValid = bSourceTrimmed &&
        GetLogGeometry(Log, KeptGeometry) && GetLogGeometry(Rest, RestGeometry) &&
        FMath::Abs(KeptGeometry.Length() - KeepLength) <= FMath::Max(2.0f, KeepLength * 0.02f) &&
        FMath::Abs((KeptGeometry.Length() + RestGeometry.Length()) - OriginalLength) <=
            FMath::Max(3.0f, OriginalLength * 0.02f);

    if (!bLengthsValid)
    {
        Log->SetActorTransform(SourceTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Log->bHasCustomLengthScale = bSourceHadCustomLengthScale;
        Rest->Destroy();
        return nullptr;
    }

    Rest->SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
    return Rest;
}

float EstimateMassKg(const AActor* Piece, float BoundsFillFactor, float DensityKgPerCubicMetre)
{
    if (!IsValid(Piece) || !FMath::IsFinite(BoundsFillFactor) ||
        BoundsFillFactor <= 0.0f || BoundsFillFactor > 1.0f ||
        !FMath::IsFinite(DensityKgPerCubicMetre) || DensityKgPerCubicMetre <= 0.0f)
        return 0.0f;

    const UStaticMeshComponent* Mesh = Piece->FindComponentByClass<UStaticMeshComponent>();
    if (!Mesh || !Mesh->GetStaticMesh())
        return 0.0f;
    const FVector Extent = Mesh->GetStaticMesh()->GetBounds().BoxExtent *
        Mesh->GetComponentScale().GetAbs();
    const double VolumeCubicMetres =
        (8.0 * static_cast<double>(Extent.X) * Extent.Y * Extent.Z) / 1.0e6;
    const double Mass = VolumeCubicMetres * BoundsFillFactor * DensityKgPerCubicMetre;
    return FMath::IsFinite(Mass) && Mass > 0.0 && Mass <= TNumericLimits<float>::Max()
        ? static_cast<float>(Mass) : 0.0f;
}

float EstimateMassKg(const AActor* Piece)
{
    const IStructuralPieceInterface* Structural = Cast<const IStructuralPieceInterface>(Piece);
    if (!Structural)
        return EstimateMassKg(Piece, 0.7f, 1000.0f);

    float DensityKgPerCubicMetre = 600.0f;
    switch (Structural->GetStructuralKind())
    {
    case EStructuralNodeKind::Wood:
    case EStructuralNodeKind::Stick:
        DensityKgPerCubicMetre = 600.0f;
        break;
    case EStructuralNodeKind::Stone:
        DensityKgPerCubicMetre = 2500.0f;
        break;
    default:
        DensityKgPerCubicMetre = 1000.0f;
        break;
    }
    // Keep the approximate material density shared with carry-load defaults.
    return EstimateMassKg(Piece, 0.7f, DensityKgPerCubicMetre);
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
