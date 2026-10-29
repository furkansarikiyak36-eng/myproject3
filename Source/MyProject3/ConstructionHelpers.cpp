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
    auto CopyGeometryConfiguration = [Source, Copy, SourceMesh]()
    {
        Copy->BaseDefinitionId = Source->BaseDefinitionId;
        Copy->bHasCustomLengthScale = Source->bHasCustomLengthScale;
        Copy->PieceType = Source->PieceType;
        Copy->LengthFraction = Source->LengthFraction;
        Copy->CrossSectionProfile = Source->CrossSectionProfile;
        Copy->bUseCanonicalGeometryDimensions = Source->bUseCanonicalGeometryDimensions;
        Copy->ProcessingMeshSets = Source->ProcessingMeshSets;
        Copy->SelectedProcessingMeshSetId = Source->SelectedProcessingMeshSetId;
        Copy->BaseLogVariantMesh = Source->BaseLogVariantMesh;
        Copy->BaseLogVariantLengthFraction = Source->BaseLogVariantLengthFraction;
        Copy->GeometryVariantMeshes = Source->GeometryVariantMeshes;
        Copy->LengthCutVariants = Source->LengthCutVariants;
        Copy->SplitHalfVariantMesh = Source->SplitHalfVariantMesh;
        Copy->SplitHalfPieceCenterOffsetLocal = Source->SplitHalfPieceCenterOffsetLocal;
        Copy->SplitHalfChildRotationOffsetLocal = Source->SplitHalfChildRotationOffsetLocal;
        Copy->QuarterVariantMesh = Source->QuarterVariantMesh;
        Copy->QuarterPieceCenterOffsetLocal = Source->QuarterPieceCenterOffsetLocal;
        Copy->QuarterChildRotationOffsetLocal = Source->QuarterChildRotationOffsetLocal;
        Copy->HewnVariantMesh = Source->HewnVariantMesh;
        Copy->GutterVariantMesh = Source->GutterVariantMesh;
        Copy->TroughVariantMesh = Source->TroughVariantMesh;
        Copy->KalasVariantMesh = Source->KalasVariantMesh;
        Copy->PlankVariantMesh = Source->PlankVariantMesh;
        Copy->LongitudinalAxis = Source->LongitudinalAxis;
        Copy->bAutoGenerateEndSnapPoints = Source->bAutoGenerateEndSnapPoints;
        Copy->DefaultEndSnapTag = Source->DefaultEndSnapTag;
        Copy->bAutoGenerateSideSnapPoints = Source->bAutoGenerateSideSnapPoints;
        Copy->SideSnapSpacing = Source->SideSnapSpacing;
        Copy->SideSnapTag = Source->SideSnapTag;
        Copy->bCanBeCarried = Source->bCanBeCarried;
        Copy->SnapPoints = Source->SnapPoints;
        Copy->SetPieceStaticMesh(SourceMesh->GetStaticMesh());
    };
    CopyGeometryConfiguration();
    Copy->FinishSpawning(Transform);
    if (!IsValid(Copy))
        return nullptr;
    // Construction scripts may restore defaults, so the spawned offcut must
    // retain the exact source mesh-family/recipe configuration afterward too.
    CopyGeometryConfiguration();
    Copy->SetActorScale3D(Transform.GetScale3D());
    if (UMaterialInterface* Material = SourceMesh->GetMaterial(0))
        Copy->GetLogMesh()->SetMaterial(0, Material);
    return Copy;
}

bool TrimLogToRange(ALogBuildPiece* Log, float FromAlong, float ToAlong)
{
    FLogGeometry Geometry;
    if (!GetLogGeometry(Log, Geometry) || !FMath::IsFinite(FromAlong) ||
        !FMath::IsFinite(ToAlong) || Geometry.Length() <= KINDA_SMALL_NUMBER)
        return false;

    const float From = FMath::Clamp(FMath::Min(FromAlong, ToAlong), -Geometry.HalfLength, Geometry.HalfLength);
    const float To = FMath::Clamp(FMath::Max(FromAlong, ToAlong), -Geometry.HalfLength, Geometry.HalfLength);
    const float NewLength = To - From;
    if (!FMath::IsFinite(NewLength) || NewLength < 5.0f)
        return false;

    // A static-mesh scale change is not a geometric cut: it stretches the bark,
    // end grain and collision together, while leaving no truthful length recipe.
    // Exact processing actions use authored output meshes through LogBuildPiece;
    // arbitrary build/opening cuts must wait for a real mesh-slicing pipeline.
    return FMath::IsNearlyEqual(From, -Geometry.HalfLength, 0.5f) &&
        FMath::IsNearlyEqual(To, Geometry.HalfLength, 0.5f);
}

ALogBuildPiece* CutOffRemainder(ALogBuildPiece* Log, float KeepLength, const FVector& DropLocation)
{
    FLogGeometry Geometry;
    if (!IsValid(Log) || !GetLogGeometry(Log, Geometry) ||
        !FMath::IsFinite(KeepLength) ||
        !FMath::IsFinite(DropLocation.X) || !FMath::IsFinite(DropLocation.Y) ||
        !FMath::IsFinite(DropLocation.Z) ||
        Log->bHasCustomLengthScale || Log->IsPlaced() ||
        Log->GetAttachedPieceCount() > 0 ||
        KeepLength >= Geometry.Length() - 10.0f || KeepLength < 20.0f)
        return nullptr;

    auto LengthRatio = [](ELogLengthFraction Fraction, double& OutRatio) -> bool
    {
        switch (Fraction)
        {
        case ELogLengthFraction::FullLength: OutRatio = 1.0; return true;
        case ELogLengthFraction::ThreeQuarterLength: OutRatio = 0.75; return true;
        case ELogLengthFraction::HalfLength: OutRatio = 0.5; return true;
        case ELogLengthFraction::QuarterLength: OutRatio = 0.25; return true;
        case ELogLengthFraction::EighthLength: OutRatio = 0.125; return true;
        default: OutRatio = 0.0; return false;
        }
    };

    const ELogLengthFraction SourceLength = Log->GetLengthFraction();
    const ELogCrossSectionProfile Profile = Log->GetCrossSectionProfile();
    double SourceRatio = 0.0;
    if (!LengthRatio(SourceLength, SourceRatio) || SourceRatio <= 0.0)
        return nullptr;

    const double OriginalLength = Geometry.Length();
    const double DesiredKeptRatio = SourceRatio * KeepLength / OriginalLength;
    const double DesiredRestRatio = SourceRatio - DesiredKeptRatio;
    const double Tolerance = FMath::Max(2.0, static_cast<double>(KeepLength) * 0.02);
    const ELogCutAction CandidateActions[] = {
        ELogCutAction::SplitLengthHalf,
        ELogCutAction::SplitLengthThreeQuarter,
        ELogCutAction::SplitHalfLengthIntoQuarters,
        ELogCutAction::SplitQuarterLengthIntoEighths
    };

    const FTransform OriginalTransform = Log->GetActorTransform();
    ELogLengthFraction KeptFraction = ELogLengthFraction::FullLength;
    ELogLengthFraction RestFraction = ELogLengthFraction::FullLength;
    FTransform KeptTransform = OriginalTransform;
    FTransform RestTransform = OriginalTransform;
    bool bFoundRecipe = false;

    for (const ELogCutAction Action : CandidateActions)
    {
        FLogLengthCutVariant Variant;
        if (!Log->TryGetAuthoredLengthCutVariant(Action, Variant) ||
            Variant.SourceLengthFraction != SourceLength ||
            Variant.CrossSectionProfile != Profile)
            continue;

        double FirstRatio = 0.0;
        double SecondRatio = 0.0;
        if (!LengthRatio(Variant.FirstOutputLengthFraction, FirstRatio) ||
            !LengthRatio(Variant.SecondOutputLengthFraction, SecondRatio))
            continue;

        const double FirstLength = OriginalLength * FirstRatio / SourceRatio;
        const double SecondLength = OriginalLength * SecondRatio / SourceRatio;
        const bool bKeepFirst =
            FMath::Abs(FirstRatio - DesiredKeptRatio) <= 0.01 &&
            FMath::Abs(SecondRatio - DesiredRestRatio) <= 0.01 &&
            FMath::Abs(FirstLength - KeepLength) <= Tolerance &&
            FMath::Abs(SecondLength - (OriginalLength - KeepLength)) <= Tolerance;
        const bool bKeepSecond =
            FMath::Abs(SecondRatio - DesiredKeptRatio) <= 0.01 &&
            FMath::Abs(FirstRatio - DesiredRestRatio) <= 0.01 &&
            FMath::Abs(SecondLength - KeepLength) <= Tolerance &&
            FMath::Abs(FirstLength - (OriginalLength - KeepLength)) <= Tolerance;
        if (!bKeepFirst && !bKeepSecond)
            continue;

        const FVector FirstLocation =
            OriginalTransform.TransformPosition(Variant.FirstOutputCenterOffsetLocal);
        const FVector SecondLocation =
            OriginalTransform.TransformPosition(Variant.SecondOutputCenterOffsetLocal);
        const FQuat FirstRotation =
            OriginalTransform.GetRotation() * Variant.FirstOutputRotationOffsetLocal.Quaternion();
        const FQuat SecondRotation =
            OriginalTransform.GetRotation() * Variant.SecondOutputRotationOffsetLocal.Quaternion();

        if (bKeepFirst)
        {
            KeptFraction = Variant.FirstOutputLengthFraction;
            RestFraction = Variant.SecondOutputLengthFraction;
            KeptTransform = FTransform(FirstRotation, FirstLocation, OriginalTransform.GetScale3D());
            RestTransform = FTransform(SecondRotation, SecondLocation, OriginalTransform.GetScale3D());
        }
        else
        {
            KeptFraction = Variant.SecondOutputLengthFraction;
            RestFraction = Variant.FirstOutputLengthFraction;
            KeptTransform = FTransform(SecondRotation, SecondLocation, OriginalTransform.GetScale3D());
            RestTransform = FTransform(FirstRotation, FirstLocation, OriginalTransform.GetScale3D());
        }
        bFoundRecipe = true;
        break;
    }

    // Arbitrary lengths remain unavailable until a true mesh-slicing pipeline is
    // implemented. The operation only proceeds when an exact two-output recipe
    // and both authored meshes can preserve nominal geometry and end grain.
    if (!bFoundRecipe || !KeptTransform.IsValid() || !RestTransform.IsValid())
        return nullptr;

    ALogBuildPiece* Rest = SpawnLogCopy(Log, RestTransform);
    if (!Rest)
        return nullptr;

    if (!Rest->TrySetAuthoredGeometryVariant(
            RestFraction, Profile, RestTransform, FGuid::NewGuid()))
    {
        Rest->Destroy();
        return nullptr;
    }

    if (!Log->TrySetAuthoredGeometryVariant(
            KeptFraction, Profile, KeptTransform, FGuid::NewGuid()))
    {
        Rest->Destroy();
        return nullptr;
    }

    // The offcut's shape remains authored and exact; only its final drop position
    // is changed after the split geometry has committed.
    if (Rest->SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics))
    {
        if (!TransferPiece(Rest, EWorldItemOwnerKind::World, FGuid(), Rest->GetActorLocation()))
        {
            // Keep actor transform and ownership position coherent if the ledger
            // refuses to record the drop location.
            Rest->SetActorTransform(RestTransform, false, nullptr, ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Error,
                TEXT("Offcut location transfer was rejected; restored its authored split transform."));
        }
    }
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
