#include "LogCarryComponent.h"

#include "CarryLoadComponent.h"

#include "LogBuildPiece.h"
#include "ConstructionHelpers.h"
#include "HarvestableTreeActor.h"
#include "LogAttachmentComponents.h"
#include "LogBuildToolsComponent.h"
#include "LogSnapCharacter.h"
#include "Placement/PlacementFeatureProvider.h"
#include "Placement/PlacementKernelSubsystem.h"
#include "Placement/PlacementTransaction.h"
#include "ThrownImpactComponent.h"
#include "Engine/OverlapResult.h"
#include "MaterialConnectionSubsystem.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "StructuralPieceInterface.h"
#include "WorldStructuralGraphSubsystem.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldItemOwnershipSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

namespace { FVector LocalAxisVector(ELogLongitudinalAxis Axis); }

ULogCarryComponent::ULogCarryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void ULogCarryComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!CarryOwnerId.IsValid()) CarryOwnerId = FGuid::NewGuid();
    if (UPlacementKernelSubsystem* Kernel = GetKernel())
        PlacementCommittedHandle = Kernel->OnPlacementCommitted.AddUObject(this, &ULogCarryComponent::HandlePlacementCommitted);
}

void ULogCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UPlacementKernelSubsystem* Kernel = GetKernel())
        Kernel->OnPlacementCommitted.Remove(PlacementCommittedHandle);
    Super::EndPlay(EndPlayReason);
}

UPlacementKernelSubsystem* ULogCarryComponent::GetKernel() const
{
    return GetWorld() ? GetWorld()->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
}

void ULogCarryComponent::HandlePlacementCommitted(AActor* Piece, const FPlacementSolution& Solution)
{
    if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
        ApplyEndContactSupports(Log);
    // A new post under a beam end lifts it; a fallen end over the new piece rests on it.
    LiftDroopingEndsOnto(Piece);
}

void ULogCarryComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateHingeSwings(DeltaTime);
    PromoteShoulderLog();

    if (bIsDraggingLog)
    {
        ALogBuildPiece* Piece = DraggedPiece.Get();
        UWorldObjectIdentityComponent* Identity = IsValid(Piece)
            ? Piece->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        UWorldItemOwnershipSubsystem* Ownership = GetWorld()
            ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
        FWorldOwnedItemRecord ItemRecord;
        FVector ViewLocation = FVector::ZeroVector;
        FRotator ViewRotation;
        const bool bValidDrag = IsValid(Piece) && !Piece->IsHeld() && !Piece->IsPlaced() &&
            Piece->GetAttachedPieceCount() == 0 && Identity && Identity->HasValidIdentity() &&
            Ownership && Ownership->TryGetItem(Identity->GetInstanceId(), ItemRecord) &&
            ItemRecord.OwnerKind == EWorldItemOwnerKind::World && ItemRecord.Quantity == 1 &&
            Piece->GetLogMesh() && Piece->GetLogMesh()->IsSimulatingPhysics() &&
            GetViewPoint(ViewLocation, ViewRotation) &&
            FMath::IsFinite(DragForceStrength) && FMath::IsFinite(MaxDragForceMagnitude);
        if (!bValidDrag)
        {
            EndDragAimedLog();
        }
        else
        {
            FVector Target = GetOwner() ? GetOwner()->GetActorLocation() +
                GetOwner()->GetActorForwardVector() * DragTargetDistance : ViewLocation;
            Target.Z = Piece->GetActorLocation().Z;
            FVector Force = Target - Piece->GetActorLocation();
            const float Distance = Force.Size();
            if (Distance > 5.0f)
            {
                const float Magnitude = FMath::Clamp(
                    DragForceStrength * FMath::Min(Distance / 200.0f, 1.0f),
                    0.0f, FMath::Max(0.0f, MaxDragForceMagnitude));
                Piece->GetLogMesh()->AddForce(Force.GetSafeNormal() * Magnitude);
            }
        }
    }

    if (IsValid(HeldMaterialPiece))
    {
        UpdateHeldMaterialPreview();
        return;
    }

    if (!IsValid(HeldPiece))
    {
        RestoreHeldLifts();
        ClearSnapCandidate();
        return;
    }

    // A split piece cannot stand: say so while it is held upright.
    if (PlacementMode == ELogPlacementMode::Vertical && !CanStandAsPost(HeldPiece))
        PlacementStatus = TEXT("Split pieces cannot stand as posts: use a round or hewn log.");

    // A strut between a post and its beam takes over in diagonal mode.
    UpdateStrutPlacement();
    if (bHasStrutPlacement)
    {
        UpdateHeldLifts();
        return;
    }

    // Stacking on a placed log takes over from end snaps and free placement.
    UpdateStackPlacement();
    if (bHasStackPlacement)
    {
        PlacementStatus = PlacementMode == ELogPlacementMode::Vertical
            ? TEXT("Post on the log (75 cm grid): E to place.")
            : TEXT("Stack on the log (75 cm grid, R turns it across): E to place.");
        UpdateHeldLifts();
        return;
    }

    // Keep a valid snap preview locked to the target while the player is still
    // aiming at that target. This avoids the preview jumping back and forth
    // between the free-placement point and the snap point every frame.
    if (bHasValidSnap && IsValid(CurrentSnapTarget))
    {
        FHitResult AimHit;
        const bool bStillAimingAtSnapTarget =
            TraceFromView(PlacementTraceDistance, AimHit) &&
            AimHit.GetActor() == CurrentSnapTarget;

        if (bStillAimingAtSnapTarget)
        {
            HeldPiece->SetActorTransform(CurrentSnapActorTransform);
        }
        else
        {
            ClearSnapCandidate();
            UpdateHeldPreview();
        }
    }
    else
    {
        UpdateHeldPreview();
    }

    TimeUntilSnapRefresh -= DeltaTime;
    if (TimeUntilSnapRefresh <= 0.0f)
    {
        TimeUntilSnapRefresh = FMath::Max(0.01f, SnapRefreshInterval);
        FindBestSnapCandidate();
        if (bHasValidSnap)
        {
            // A free far end needs no warning: it simply swings down around the joint.
            PlacementStatus = TEXT("Snap ready: E to place.");
        }
        else
        {
            PlacementStatus = bHasPlacementSurface ? TEXT("E to place on the ground.") : TEXT("Aim at the ground or a placed log.");
        }
        UpdateHeldLifts();
        if (HeldLifts.Num() > 0)
            PlacementStatus += TEXT("  The fallen end rests on this log: E to set it down here.");
    }

    // Lightweight prototype feedback: a green marker indicates the currently
    // selected target anchor. A proper mesh-based ghost preview can replace it.
    if (bHasValidSnap && IsValid(CurrentSnapTarget) &&
        CurrentTargetSnapPointIndex != INDEX_NONE)
    {
        const FVector MarkerLocation =
            CurrentSnapTarget->GetSnapPointWorldTransform(
                CurrentTargetSnapPointIndex).GetLocation();

        DrawDebugSphere(GetWorld(), MarkerLocation, 20.0f, 12,
            FColor::Green, false, 0.10f, 0, 2.0f);
        DrawDebugLine(GetWorld(), MarkerLocation,
            MarkerLocation + FVector::UpVector * 70.0f,
            FColor::Green, false, 0.10f, 0, 2.0f);

        const TArray<FLogSnapPoint>& TargetPoints = CurrentSnapTarget->GetSnapPoints();
        if (TargetPoints.IsValidIndex(CurrentTargetSnapPointIndex))
        {
            DrawDebugString(
                GetWorld(),
                MarkerLocation + FVector::UpVector * 80.0f,
                TargetPoints[CurrentTargetSnapPointIndex].PointId.ToString(),
                CurrentSnapTarget,
                FColor::Green,
                0.10f,
                true);
        }
    }
}

void ULogCarryComponent::BindDefaultInput(UInputComponent* InputComponent)
{
    if (!InputComponent)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("LogCarryComponent: BindDefaultInput received a null InputComponent."));
        return;
    }

    // Use this helper from a Character/Controller setup event. Projects using
    // Enhanced Input can bind the Blueprint-callable actions to Input Actions.
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ULogCarryComponent::Interact);
    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ULogCarryComponent::DropHeldPiece);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ULogCarryComponent::RotateHeldPiece);
    InputComponent->BindKey(
        EKeys::RightMouseButton, IE_Pressed, this, &ULogCarryComponent::CyclePlacementMode);
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ULogCarryComponent::CycleCutAction);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ULogCarryComponent::ProcessAimedPieceInput);
    InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ULogCarryComponent::PushOrRollAimedLogInput);
    InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ULogCarryComponent::BeginDragAimedLogInput);
    InputComponent->BindKey(EKeys::B, IE_Released, this, &ULogCarryComponent::EndDragAimedLog);
}

bool ULogCarryComponent::GetViewPoint(
    FVector& OutLocation,
    FRotator& OutRotation) const
{
    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    if (const APawn* Pawn = Cast<APawn>(Owner))
    {
        if (AController* Controller = Pawn->GetController())
        {
            Controller->GetPlayerViewPoint(OutLocation, OutRotation);
            return true;
        }
    }

    OutLocation = Owner->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
    OutRotation = Owner->GetActorRotation();
    return true;
}

bool ULogCarryComponent::TraceFromView(
    float Distance,
    FHitResult& OutHit) const
{
    UWorld* World = GetWorld();
    FVector ViewLocation;
    FRotator ViewRotation;

    if (!World || !GetViewPoint(ViewLocation, ViewRotation))
    {
        return false;
    }

    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(LogCarryTrace), false, GetOwner());
    Params.bTraceComplex = false;

    if (IsValid(HeldPiece))
    {
        Params.AddIgnoredActor(HeldPiece);
    }
    if (IsValid(HeldMaterialPiece))
    {
        Params.AddIgnoredActor(HeldMaterialPiece);
    }

    const FVector End = ViewLocation + ViewRotation.Vector() * Distance;
    return World->LineTraceSingleByChannel(
        OutHit, ViewLocation, End, ECC_Visibility, Params);
}

void ULogCarryComponent::Interact()
{
    if (ULogBuildToolsComponent* Tools = GetOwner() ? GetOwner()->FindComponentByClass<ULogBuildToolsComponent>() : nullptr)
    {
        if (Tools->TryHandleInteract())
            return;
    }
    if (IsValid(HeldPiece))
    {
        // E on a loose log while carrying one: it goes on the shoulder.
        FHitResult Hit;
        if (!IsValid(ShoulderPiece) && TraceFromView(PickupRange, Hit))
        {
            ALogBuildPiece* Loose = Cast<ALogBuildPiece>(Hit.GetActor());
            if (Loose && Loose != HeldPiece && !Loose->IsPlaced() && !Loose->IsHeld() && TryShoulderLog(Loose))
                return;
        }
        PlaceHeldPiece();
    }
    else if (IsValid(HeldMaterialPiece))
    {
        PlaceHeldMaterialPiece();
    }
    else
    {
        TryPickupTarget();
    }
}

bool ULogCarryComponent::TryPickupTarget()
{
    if (IsValid(HeldPiece))
    {
        return false;
    }

    FHitResult Hit;
    if (!TraceFromView(PickupRange, Hit))
    {
        return false;
    }
    return TryPickupPiece(Hit.GetActor());
}

bool ULogCarryComponent::TryPickupPiece(AActor* Target)
{
    if (IsValid(HeldPiece) || IsValid(HeldMaterialPiece))
    {
        return false;
    }
    if (Cast<AStoneBuildingPiece>(Target) || Cast<AStickPrimitivePiece>(Target))
    {
        return TryPickupMaterialPiece(Target);
    }

    ALogBuildPiece* Piece = Cast<ALogBuildPiece>(Target);
    if (!IsValid(Piece) || !Piece->CanBePickedUp())
    {
        return false;
    }

    // One carry budget covers every material; a full load blocks the pickup.
    UCarryLoadComponent* Load = GetOwner() ? GetOwner()->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    FString LoadReason;
    if (Load && !Load->CanCarryMore(Piece, LoadReason))
    {
        UE_LOG(LogTemp, Log, TEXT("Cannot pick up %s: %s"), *GetNameSafe(Piece), *LoadReason);
        return false;
    }

    FWorldOwnedItemRecord HeldRecord;
    if (!Piece->TryTransferOwnership(EWorldItemOwnerKind::World,
        EWorldItemOwnerKind::Held, CarryOwnerId, Piece->GetActorLocation(), HeldRecord))
    {
        return false;
    }

    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    EWorldObjectLifecycleState PreviousLifecycle = EWorldObjectLifecycleState::World;
    if (Identity)
    {
        PreviousLifecycle = Identity->GetLifecycleState();
        if ((PreviousLifecycle != EWorldObjectLifecycleState::World &&
             PreviousLifecycle != EWorldObjectLifecycleState::Placed) ||
            !Identity->TryTransitionLifecycle(
                PreviousLifecycle, EWorldObjectLifecycleState::Held))
        {
            FWorldOwnedItemRecord RolledBack;
            Piece->TryTransferOwnership(EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World,
                FGuid(), Piece->GetActorLocation(), RolledBack);
            return false;
        }
    }

    // The ledger and lifecycle state are reserved first; if the canonical
    // graph refuses detachment, roll those reservations back and keep the
    // physical piece attached.
    if (!Piece->DetachFromSnapParent())
    {
        if (Identity)
            Identity->TryTransitionLifecycle(
                EWorldObjectLifecycleState::Held, PreviousLifecycle);
        FWorldOwnedItemRecord RolledBack;
        if (!Piece->TryTransferOwnership(EWorldItemOwnerKind::Held,
            EWorldItemOwnerKind::World, FGuid(), Piece->GetActorLocation(), RolledBack))
        {
            Piece->SetActorHiddenInGame(true);
            Piece->SetActorEnableCollision(false);
            UE_LOG(LogTemp, Error, TEXT("Log pickup rollback failed for %s."),
                *GetNameSafe(Piece));
        }
        return false;
    }

    Piece->SetHeldState(true);
    Piece->SetPlacedState(false);

    HeldPiece = Piece;
    if (Load)
        Load->RegisterCarried(Piece);
    HeldYawOffset = 0.0f;
    bHasPlacementSurface = false;
    TimeUntilSnapRefresh = 0.0f;
    ClearSnapCandidate();

    return true;
}

FRotator ULogCarryComponent::MakePlacementRotation(float ViewYaw) const
{
    float Pitch = 0.0f;

    switch (PlacementMode)
    {
    case ELogPlacementMode::Vertical:
        Pitch = 90.0f;
        break;
    case ELogPlacementMode::Diagonal:
        Pitch = 45.0f;
        break;
    case ELogPlacementMode::Horizontal:
    default:
        Pitch = 0.0f;
        break;
    }

    return FRotator(Pitch, ViewYaw + HeldYawOffset, 0.0f);
}

void ULogCarryComponent::UpdateHeldPreview()
{
    if (!IsValid(HeldPiece))
    {
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    if (!GetViewPoint(ViewLocation, ViewRotation))
    {
        return;
    }

    const FRotator PlacementRotation = MakePlacementRotation(ViewRotation.Yaw);
    HeldPiece->SetActorRotation(PlacementRotation);

    const FVector ViewDirection = ViewRotation.Vector();
    FVector PreviewLocation = ViewLocation + ViewDirection * CarryDistance;
    FHitResult PlacementHit;

    bHasPlacementSurface = false;
    if (TraceFromView(PlacementTraceDistance, PlacementHit) &&
        PlacementHit.bBlockingHit)
    {
        // A carried log may be aimed at an existing log. It can snap to that
        // log, but should not be free-placed by simply overlapping its surface.
        const ALogBuildPiece* HitLog = Cast<ALogBuildPiece>(PlacementHit.GetActor());
        const bool bHitBuildPiece = HitLog != nullptr;
        // A post may stand on a placed floor or beam; it then rests on that piece.
        bHasPlacementSurface = !bHitBuildPiece ||
            (PlacementMode == ELogPlacementMode::Vertical && HitLog->IsPlaced() && PlacementHit.ImpactNormal.Z > 0.7f);
        PlacementSurfaceActor = PlacementHit.GetActor();

        if (PlacementMode == ELogPlacementMode::Horizontal && !bHitBuildPiece &&
            PlacementHit.ImpactNormal.Z > 0.6f && PlacementHit.ImpactNormal.Z < 0.995f)
        {
            // Follow the slope of the ground along the log's length.
            const FVector Along = FRotator(0.0f, PlacementRotation.Yaw, 0.0f).Vector();
            const FVector OnSlope = FVector::VectorPlaneProject(Along, PlacementHit.ImpactNormal).GetSafeNormal();
            HeldPiece->SetActorRotation(FRotator(FMath::RadiansToDegrees(FMath::Asin(OnSlope.Z)),
                PlacementRotation.Yaw, 0.0f));
        }

        FVector BoundsOrigin;
        FVector BoundsExtent;
        HeldPiece->GetActorBounds(false, BoundsOrigin, BoundsExtent);

        // Account for imported FBX pivots whose mesh bounds origin is not
        // exactly at the actor origin. The mesh bottom should sit on the hit
        // surface regardless of where the DCC pivot was authored.
        const float BottomOffsetFromActor =
            BoundsOrigin.Z - HeldPiece->GetActorLocation().Z - BoundsExtent.Z;
        const float RequiredLift = -BottomOffsetFromActor + 2.0f;
        PreviewLocation = PlacementHit.ImpactPoint +
            FVector::UpVector * RequiredLift;

        // A post near one log length from another post moves to exactly that span.
        SpanPartner.Reset();
        ALogBuildPiece* Partner = nullptr;
        FVector SnappedBase;
        float Span = 0.0f;
        if (PlacementMode == ELogPlacementMode::Vertical && !bHitBuildPiece &&
            SnapPostSpacing(PlacementHit.ImpactPoint, SnappedBase, Partner, Span))
        {
            FHitResult Ground;
            FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(PostSpanGround), false, GetOwner());
            GroundParams.AddIgnoredActor(HeldPiece);
            if (GetWorld()->LineTraceSingleByChannel(Ground, SnappedBase + FVector(0, 0, 150.0f),
                SnappedBase - FVector(0, 0, 300.0f), ECC_Visibility, GroundParams) && !Cast<ALogBuildPiece>(Ground.GetActor()))
            {
                PreviewLocation = Ground.ImpactPoint + FVector::UpVector * RequiredLift;
                SpanPartner = Partner;
                SpanCm = Span;
            }
        }
    }
    else
    {
        SpanPartner.Reset();
    }

    HeldPiece->SetActorLocation(PreviewLocation);
}

void ULogCarryComponent::FindBestSnapCandidate()
{
    if (!IsValid(HeldPiece) || !GetWorld())
    {
        ClearSnapCandidate();
        return;
    }

    // Snap to the structure the player is actually aiming at, not any nearby
    // structure behind the camera. This is essential for deliberate SOTF-style
    // placement: the crosshair chooses the candidate; socket geometry chooses
    // the exact final transform.
    FHitResult AimHit;
    if (!TraceFromView(PlacementTraceDistance, AimHit))
    {
        ClearSnapCandidate();
        return;
    }

    ALogBuildPiece* AimedTarget = Cast<ALogBuildPiece>(AimHit.GetActor());
    if (!IsValid(AimedTarget) || !AimedTarget->IsPlaced() || AimedTarget->IsHeld() || IsBadlyDamaged(AimedTarget))
    {
        ClearSnapCandidate();
        return;
    }

    // The placement kernel picks the joint; this component only feeds it the
    // player's aim and shows the result.
    UPlacementKernelSubsystem* Kernel = GetWorld()->GetSubsystem<UPlacementKernelSubsystem>();
    if (!Kernel)
    {
        ClearSnapCandidate();
        return;
    }
    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    FPlacementIntent Intent;
    Intent.Held = HeldPiece;
    Intent.AimActor = AimedTarget;
    Intent.AimPoint = AimHit.ImpactPoint;
    Intent.ViewDirection = ViewRotation.Vector();
    Intent.HeldAngleDeg = HeldYawOffset;
    const FPlacementSolution Solution = Kernel->Solve(Intent, bHasValidSnap ? &CurrentSolution : nullptr);
    const FPlacementLink* Link = Solution.GetPrimaryLink();
    FPlacementFeature SourceFeature;
    FPlacementFeature TargetFeature;
    ALogBuildPiece* Target = Link ? Cast<ALogBuildPiece>(Link->Target.Piece.Get()) : nullptr;
    if (!Solution.bValid || !Target ||
        !FPlacementProviderRegistry::FindFeature(HeldPiece, Link->Source.FeatureId, SourceFeature) ||
        !FPlacementProviderRegistry::FindFeature(Target, Link->Target.FeatureId, TargetFeature))
    {
        ClearSnapCandidate();
        return;
    }
    CurrentSolution = Solution;
    SetSnapCandidate(Target, SourceFeature.LegacyIndex, TargetFeature.LegacyIndex, Solution.PieceTransform);
}

void ULogCarryComponent::SetSnapCandidate(
    ALogBuildPiece* Target,
    int32 SourceIndex,
    int32 TargetIndex,
    const FTransform& DesiredActorTransform)
{
    if (!IsValid(HeldPiece) || !IsValid(Target))
    {
        ClearSnapCandidate();
        return;
    }

    const bool bStateChanged = !bHasValidSnap ||
        CurrentSnapTarget != Target ||
        CurrentSourceSnapPointIndex != SourceIndex ||
        CurrentTargetSnapPointIndex != TargetIndex;

    CurrentSnapTarget = Target;
    CurrentSourceSnapPointIndex = SourceIndex;
    CurrentTargetSnapPointIndex = TargetIndex;
    CurrentSnapActorTransform = DesiredActorTransform;
    // Interlock the joint: the joined end goes into the target log (hiding its
    // end grain) and a log resting across another sinks into it.
    UStaticMeshComponent* TargetMesh = Target->GetLogMesh();
    UStaticMeshComponent* HeldMesh = HeldPiece->GetLogMesh();
    if (TargetMesh && HeldMesh && HeldMesh->GetStaticMesh() && (EndInsetFraction > 0.0f || SnapInsetFraction > 0.0f))
    {
        const FVector TargetPoint =
            (Target->GetSnapPointLocalTransform(TargetIndex) * Target->GetActorTransform()).GetLocation();
        const FVector TargetCentre = TargetMesh->Bounds.Origin;
        const FVector TargetAxis = TargetMesh->GetComponentTransform()
            .TransformVectorNoScale(LocalAxisVector(Target->LongitudinalAxis)).GetSafeNormal();
        const FVector OnTargetAxis =
            TargetCentre + TargetAxis * FVector::DotProduct(TargetPoint - TargetCentre, TargetAxis);
        const FVector ToTargetAxis = OnTargetAxis - TargetPoint;

        const FVector HeldAxis = CurrentSnapActorTransform
            .TransformVectorNoScale(LocalAxisVector(HeldPiece->LongitudinalAxis)).GetSafeNormal();
        const FVector HeldCentre = CurrentSnapActorTransform.TransformPosition(
            HeldMesh->GetStaticMesh()->GetBounds().Origin);
        const FVector HeldJoint =
            (HeldPiece->GetSnapPointLocalTransform(SourceIndex) * CurrentSnapActorTransform).GetLocation();
        const float JointAlong = FVector::DotProduct(HeldJoint - HeldCentre, HeldAxis);
        const float HalfLength = FVector::DotProduct(
            HeldMesh->GetStaticMesh()->GetBounds().BoxExtent * CurrentSnapActorTransform.GetScale3D().GetAbs(),
            LocalAxisVector(HeldPiece->LongitudinalAxis).GetAbs());

        FVector Offset = FVector::ZeroVector;
        FVector Across = ToTargetAxis;
        if (HalfLength > KINDA_SMALL_NUMBER && FMath::Abs(JointAlong) > 0.5f * HalfLength)
        {
            // Joined by an end: slide along its own length into the target.
            const FVector EndDirection = HeldAxis * FMath::Sign(JointAlong);
            const float Depth = FVector::DotProduct(ToTargetAxis, EndDirection);
            if (Depth > 0.0f)
            {
                Offset += EndDirection * Depth * EndInsetFraction;
                Across -= EndDirection * Depth;
            }
        }
        Offset += Across * SnapInsetFraction;
        CurrentSnapActorTransform.AddToTranslation(Offset);
    }
    bHasValidSnap = true;

    HeldPiece->SetActorTransform(CurrentSnapActorTransform);

    if (bStateChanged)
    {
        OnSnapStateChanged.Broadcast(true);
    }
}

void ULogCarryComponent::ClearSnapCandidate()
{
    const bool bWasSnapping = bHasValidSnap;

    bHasValidSnap = false;
    CurrentSnapTarget = nullptr;
    CurrentSourceSnapPointIndex = INDEX_NONE;
    CurrentTargetSnapPointIndex = INDEX_NONE;
    CurrentSnapActorTransform = FTransform::Identity;
    CurrentSolution = FPlacementSolution();

    if (bWasSnapping)
    {
        OnSnapStateChanged.Broadcast(false);
    }
}

bool ULogCarryComponent::PlaceHeldPiece()
{
    if (bHasStrutPlacement && IsValid(HeldPiece))
        return PlaceStrut();
    if (IsValid(HeldPiece) && MyProject3Construction::IsVerticalLog(HeldPiece) && !CanStandAsPost(HeldPiece))
    {
        PlacementStatus = TEXT("Split pieces cannot stand as posts: use a round or hewn log.");
        return false;
    }
    if (bHasStackPlacement && StackSolution.bValid && IsValid(HeldPiece))
    {
        const FPlacementSolution Solution = StackSolution;
        bHasStackPlacement = false;
        StackTarget.Reset();
        return CommitHeldSolution(Solution);
    }
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!IsValid(HeldPiece) || !Kernel)
        return false;

    ALogBuildPiece* PieceToPlace = HeldPiece;
    if (bHasValidSnap && IsValid(CurrentSnapTarget) && CurrentSolution.bValid)
    {
        // The joint the preview shows, plus what the far end rests on: one commit.
        const int32 OwnSnapIndex = CurrentSourceSnapPointIndex;
        FPlacementSolution Solution = CurrentSolution;
        Solution.PieceTransform = CurrentSnapActorTransform;
        PieceToPlace->SetActorTransform(CurrentSnapActorTransform);
        AActor* Support = nullptr;
        bool bGround = false;
        const bool bFarEndHeld = EvaluateFarEndSupport(PieceToPlace, OwnSnapIndex, Support, bGround);
        const IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(Support);
        if (Support && Support != CurrentSnapTarget && FPlacementProviderRegistry::Find(Support) &&
            SupportPiece && SupportPiece->IsStructurallyPlaced())
        {
            Solution.Links.Add(UPlacementKernelSubsystem::MakeBodyLink(PieceToPlace, Support, EPlacementLinkRole::Secondary));
        }
        if (!CommitHeldSolution(Solution))
        {
            // Never reinterpret a failed joint as a free placement.
            ClearSnapCandidate();
            return false;
        }
        if (!bFarEndHeld)
        {
            // Like a board nailed at one end: the joint holds, the free end swings down.
            if (StartHingeSwing(PieceToPlace, OwnSnapIndex))
            {
                PlacementStatus = TEXT("Placed.");
                return true;
            }
            if (UWorldObjectIdentityComponent* Identity = PieceToPlace->FindComponentByClass<UWorldObjectIdentityComponent>())
                Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::World);
            PieceToPlace->DetachFromSnapParent(true);
            PieceToPlace->SetPlacedState(false);
            PlacementStatus = TEXT("The other end had no support: the log fell.");
            return false;
        }
        return true;
    }

    if (!bAllowFreePlacement || (bRequireSurfaceForFreePlacement && !bHasPlacementSurface))
        return false;
    // Free placement: on the ground, or resting on the built piece under it (a post on a floor).
    return CommitHeldSolution(Kernel->MakeDirectSolution(PieceToPlace, PieceToPlace->GetActorTransform(),
        TEXT("piece.free_place"), PlacementSurfaceActor.Get()));
}

void ULogCarryComponent::DropHeldPiece()
{
    if (IsValid(HeldMaterialPiece))
    {
        DropHeldMaterialPiece();
        return;
    }
    if (!IsValid(HeldPiece))
    {
        return;
    }

    ALogBuildPiece* PieceToDrop = HeldPiece;

    FVector ViewLocation;
    FRotator ViewRotation;
    const bool bHasViewPoint = GetViewPoint(ViewLocation, ViewRotation);
    const FVector DropLocation = bHasViewPoint
        ? ViewLocation + ViewRotation.Vector() * 110.0f
        : PieceToDrop->GetActorLocation();

    FWorldOwnedItemRecord WorldRecord;
    if (!PieceToDrop->TryTransferOwnership(EWorldItemOwnerKind::Held,
        EWorldItemOwnerKind::World, FGuid(), DropLocation, WorldRecord))
    {
        return;
    }

    if (UWorldObjectIdentityComponent* Identity =
        PieceToDrop->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (!Identity->TryTransitionLifecycle(
            EWorldObjectLifecycleState::Held,
            EWorldObjectLifecycleState::World))
        {
            FWorldOwnedItemRecord RolledBack;
            PieceToDrop->TryTransferOwnership(EWorldItemOwnerKind::World,
                EWorldItemOwnerKind::Held, CarryOwnerId, PieceToDrop->GetActorLocation(), RolledBack);
            return;
        }
    }

    PieceToDrop->SetActorLocation(DropLocation);

    /* legacy placement state follows the committed ledger transition */
    PieceToDrop->SetHeldState(false);
    PieceToDrop->SetPlacedState(false);
    ReleaseHeldFromLoad();
    HeldPiece = nullptr;
    bHasPlacementSurface = false;
    ClearSnapCandidate();
}

void ULogCarryComponent::RotateHeldPiece()
{
    if (!IsValid(HeldPiece))
    {
        return;
    }

    HeldYawOffset = FMath::UnwindDegrees(HeldYawOffset + 90.0f);
    UpdateHeldPreview();
    TimeUntilSnapRefresh = 0.0f;
}

void ULogCarryComponent::CyclePlacementMode()
{
    switch (PlacementMode)
    {
    case ELogPlacementMode::Horizontal:
        PlacementMode = ELogPlacementMode::Vertical;
        break;
    case ELogPlacementMode::Vertical:
        PlacementMode = ELogPlacementMode::Diagonal;
        break;
    case ELogPlacementMode::Diagonal:
        PlacementMode = ELogPlacementMode::Floor;
        break;
    case ELogPlacementMode::Floor:
        PlacementMode = ELogPlacementMode::Stairs;
        break;
    case ELogPlacementMode::Stairs:
    default:
        PlacementMode = ELogPlacementMode::Horizontal;
        break;
    }
    if (const UEnum* ModeEnum = StaticEnum<ELogPlacementMode>())
        PlacementStatus = FString::Printf(TEXT("Placement mode: %s"),
            *ModeEnum->GetDisplayNameTextByValue(static_cast<int64>(PlacementMode)).ToString());

    if (IsValid(HeldPiece))
    {
        UpdateHeldPreview();
        TimeUntilSnapRefresh = 0.0f;
    }
}

void ULogCarryComponent::CycleCutAction()
{
    CancelArmedCut();
    switch (SelectedCutAction)
    {
    case ELogCutAction::SplitHalf:
        SelectedCutAction = ELogCutAction::Quarter;
        break;
    case ELogCutAction::Quarter:
        SelectedCutAction = ELogCutAction::Hewn;
        break;
    case ELogCutAction::Hewn:
        SelectedCutAction = ELogCutAction::HewSecondFace;
        break;
    case ELogCutAction::HewSecondFace:
        SelectedCutAction = ELogCutAction::Gutter;
        break;
    case ELogCutAction::Gutter:
        SelectedCutAction = ELogCutAction::Trough;
        break;
    case ELogCutAction::Trough:
        SelectedCutAction = ELogCutAction::Kalas;
        break;
    case ELogCutAction::Kalas:
        SelectedCutAction = ELogCutAction::Plank;
        break;
    case ELogCutAction::Plank:
        SelectedCutAction = ELogCutAction::SplitLengthHalf;
        break;
    // One length-cut entry: where the player looks along the log picks 1/2, 3/4 or 1/4.
    case ELogCutAction::SplitLengthHalf:
    case ELogCutAction::SplitLengthThreeQuarter:
    case ELogCutAction::SplitHalfLengthIntoQuarters:
    case ELogCutAction::SplitQuarterLengthIntoEighths:
    default:
        SelectedCutAction = ELogCutAction::SplitHalf;
        break;
    }

    const UEnum* Enum = StaticEnum<ELogCutAction>();
    const FString ActionName = Enum
        ? Enum->GetDisplayNameTextByValue(static_cast<int64>(SelectedCutAction)).ToString()
        : TEXT("Unknown");
    const FString Message = FString::Printf(
        TEXT("Selected log operation: %s (F to apply to a loose log)"), *ActionName);

    UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Message);
    }
}

bool ULogCarryComponent::BeginDragAimedLog()
{
    if (IsValid(HeldPiece) || !GetWorld())
        return false;

    FHitResult Hit;
    if (!TraceFromView(PickupRange, Hit) || !Hit.bBlockingHit)
        return false;

    ALogBuildPiece* Piece = Cast<ALogBuildPiece>(Hit.GetActor());
    UWorldObjectIdentityComponent* Identity = IsValid(Piece)
        ? Piece->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    FWorldOwnedItemRecord ItemRecord;
    if (!IsValid(Piece) || Piece->IsHeld() || Piece->IsPlaced() ||
        Piece->GetAttachedPieceCount() > 0 || !Identity || !Identity->HasValidIdentity() ||
        !Ownership || !Ownership->TryGetItem(Identity->GetInstanceId(), ItemRecord) ||
        ItemRecord.OwnerKind != EWorldItemOwnerKind::World || ItemRecord.Quantity != 1 ||
        !Piece->GetLogMesh() || !Piece->GetLogMesh()->IsSimulatingPhysics() ||
        !FMath::IsFinite(DragForceStrength) || !FMath::IsFinite(MaxDragForceMagnitude) ||
        MaxDragForceMagnitude <= 0.0f)
        return false;

    DraggedPiece = Piece;
    bIsDraggingLog = true;
    return true;
}

void ULogCarryComponent::EndDragAimedLog()
{
    bIsDraggingLog = false;
    DraggedPiece.Reset();
}

bool ULogCarryComponent::TryPushOrRollAimedLog()
{
    // A held or structurally placed piece must use the carry/placement workflow,
    // not the loose-body impulse path.
    if (IsValid(HeldPiece) || !GetWorld())
    {
        return false;
    }

    FHitResult Hit;
    if (!TraceFromView(PickupRange, Hit) || !Hit.bBlockingHit)
    {
        return false;
    }

    ALogBuildPiece* Piece = Cast<ALogBuildPiece>(Hit.GetActor());
    if (!IsValid(Piece) || Piece->IsHeld() || Piece->IsPlaced() ||
        Piece->GetAttachedPieceCount() > 0)
    {
        return false;
    }

    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    if (!Identity || !Identity->HasValidIdentity())
    {
        return false;
    }

    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    FWorldOwnedItemRecord ItemRecord;
    if (!Ownership ||
        !Ownership->TryGetItem(Identity->GetInstanceId(), ItemRecord) ||
        ItemRecord.OwnerKind != EWorldItemOwnerKind::World ||
        ItemRecord.Quantity != 1)
    {
        // Only a single, world-owned physical actor can be pushed. Stack stock,
        // inventory items and in-transit cargo are not physics-push targets.
        return false;
    }

    UStaticMeshComponent* LogMesh = Piece->GetLogMesh();
    if (!IsValid(LogMesh) || !LogMesh->IsSimulatingPhysics() ||
        LogMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    if (!GetViewPoint(ViewLocation, ViewRotation))
    {
        return false;
    }

    FVector PushDirection(
        ViewRotation.Vector().X,
        ViewRotation.Vector().Y,
        0.0f);
    PushDirection = PushDirection.GetSafeNormal();
    if (PushDirection.IsNearlyZero() ||
        !FMath::IsFinite(PushImpulseStrength) ||
        !FMath::IsFinite(MaxPushImpulseMagnitude))
    {
        return false;
    }

    const float SafeMaximum = FMath::Max(0.0f, MaxPushImpulseMagnitude);
    const float SafeStrength = FMath::Clamp(
        PushImpulseStrength, 0.0f, SafeMaximum);
    if (SafeStrength <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    // Applying the impulse at the trace contact point, rather than at the
    // centre of mass, lets off-centre contact create natural rolling torque.
    LogMesh->AddImpulseAtLocation(PushDirection * SafeStrength, Hit.ImpactPoint);
    return true;
}

void ULogCarryComponent::PushOrRollAimedLogInput()
{
    if (TryPushOrRollAimedLog())
    {
        return;
    }

    const FString Message =
        TEXT("Push/roll requires an aimed loose, world-owned log with active physics.");
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, Message);
    }
}

void ULogCarryComponent::ProcessAimedPieceInput()
{
    ProcessAimedPiece();
}

bool ULogCarryComponent::ProcessAimedPiece()
{
    if (IsValid(HeldPiece))
    {
        const FString Message = TEXT("Drop the carried log before processing it.");
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Message);
        }
        return false;
    }

    FHitResult Hit;
    if (!TraceFromView(PickupRange, Hit))
    {
        const FString Message = TEXT("Aim at a loose log within interaction range.");
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Message);
        }
        return false;
    }

    ALogBuildPiece* Piece = Cast<ALogBuildPiece>(Hit.GetActor());
    if (!IsValid(Piece))
    {
        const FString Message = TEXT("The aimed object is not a log piece.");
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Message);
        }
        return false;
    }

    // First press marks the cut where the player looks; the next presses cut it.
    if (ArmedLog.Get() != Piece)
    {
        FLogCutChoice Choice;
        FString Reason;
        if (!ResolveCutChoice(Piece, Hit.ImpactPoint, Choice))
            Reason = Piece->IsPlaced()
                ? TEXT("Built pieces are not cut; choose the length cut (C) to shorten a standing post.")
                : TEXT("This piece is too short to cut again.");
        else
            CanMakeCut(Piece, Choice, Reason);
        if (!Reason.IsEmpty())
        {
            CancelArmedCut();
            PlacementStatus = Reason;
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, Reason);
            return false;
        }
        ArmedLog = Piece;
        ArmedAction = Choice.Action;
        ArmedShare = Choice.Share;
        bArmedFlipPending = Choice.bFlip;
        ArmedKind = Choice.Kind;
        ArmedWork = 0.0f;
        PlacementStatus = FString::Printf(TEXT("Marked: %s. F: cut with the axe, C: cancel."), *DescribeCut(Piece, Choice));
        return true;
    }

    // A 3/4 cut aimed at the -X end: turn the log end for end so the short piece
    // comes off where the player marked it. The marked ring stays in place.
    if (bArmedFlipPending)
    {
        bArmedFlipPending = false;
        if (UStaticMeshComponent* LogMesh = Piece->GetLogMesh())
        {
            const FVector Pivot = LogMesh->Bounds.Origin;
            const FVector AxisWorld = LogMesh->GetComponentTransform().TransformVectorNoScale(
                LocalAxisVector(Piece->LongitudinalAxis));
            const FQuat Turn(FMath::Abs(AxisWorld.Z) > 0.9f ? FVector::XAxisVector : FVector::UpVector, PI);
            FTransform Turned = Piece->GetActorTransform();
            Turned.SetRotation(Turn * Turned.GetRotation());
            Turned.SetLocation(Pivot + Turn.RotateVector(Turned.GetLocation() - Pivot));
            Piece->SetActorTransform(Turned, false, nullptr, ETeleportType::TeleportPhysics);
            if (LogMesh->IsSimulatingPhysics())
            {
                LogMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                LogMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }
            ArmedShare = 1.0f - ArmedShare;
        }
    }
    const ELogCutAction CutAction = ArmedAction;
    if (ALogSnapCharacter* Character = Cast<ALogSnapCharacter>(GetOwner()))
        Character->PlayAxeSwing();

    // Free cuts and post shortening take the same number of swings as a recipe cut.
    if (ArmedKind != ELogCutKind::Recipe)
    {
        ArmedWork += FMath::Clamp(CutWorkPerPress, 0.05f, 1.0f);
        if (ArmedWork < 1.0f - KINDA_SMALL_NUMBER)
        {
            PlacementStatus = FString::Printf(TEXT("Cutting: %.0f%% - F again to continue"), ArmedWork * 100.0f);
            return true;
        }
        FLogCutChoice Done;
        Done.Share = ArmedShare;
        Done.Kind = ArmedKind;
        const FString What = DescribeCut(Piece, Done);
        if (ArmedKind == ELogCutKind::Sharpen)
        {
            CancelArmedCut();
            USharpenedTipComponent* Tip = NewObject<USharpenedTipComponent>(Piece);
            Tip->RegisterComponent();
            const bool bSharp = Tip->InitializeTip();
            if (!bSharp)
                Tip->DestroyComponent();
            PlacementStatus = bSharp ? TEXT("Sharpened: climbers will be hurt and pushed back.") : TEXT("It could not be sharpened.");
            return bSharp;
        }
        if (ArmedKind == ELogCutKind::ShortenPost && PostHoldsAnything(Piece))
        {
            CancelArmedCut();
            PlacementStatus = TEXT("This post holds up other pieces: a post can only be cut when nothing rests on it.");
            return false;
        }
        const bool bCut = CutLogAtShare(Piece, ArmedShare);
        CancelArmedCut();
        PlacementStatus = bCut ? FString::Printf(TEXT("Done: %s."), *What) : TEXT("The cut could not be made.");
        return bCut;
    }

    // Remember what already lies here, so the new pieces can be found after the cut.
    TSet<ALogBuildPiece*> Before;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
        Before.Add(*It);

    const FWorldOperationResult CutResult = Piece->TryAdvanceCutProgress(
        CutAction,
        FMath::Clamp(CutWorkPerPress, 0.05f, 1.0f),
        FGuid::NewGuid());

    if (CutResult.IsSuccess())
    {
        // Keep work on the physical piece. Repeated interaction steps complete
        // one operation; the geometry/ownership transaction still commits only
        // when the cut reaches 100%.
        if (Piece->HasActiveCutProgress())
        {
            const UEnum* Enum = StaticEnum<ELogCutAction>();
            const FString ActionName = Enum
                ? Enum->GetDisplayNameTextByValue(static_cast<int64>(CutAction)).ToString()
                : TEXT("Processing");
            const FString Message = FString::Printf(
                TEXT("%s: %.0f%% — press F again to continue"),
                *ActionName, Piece->GetCutProgress() * 100.0f);
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 1.8f, FColor::Cyan, Message);
            return true;
        }

        // Cut pieces start at rest where the log lay; nothing is thrown about.
        for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
        {
            ALogBuildPiece* Output = *It;
            if ((Output != Piece && Before.Contains(Output)) || !Output->GetLogMesh() ||
                !Output->GetLogMesh()->IsSimulatingPhysics())
                continue;
            Output->GetLogMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Output->GetLogMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
        const UEnum* Enum = StaticEnum<ELogCutAction>();
        const FString ActionName = Enum
            ? Enum->GetDisplayNameTextByValue(static_cast<int64>(CutAction)).ToString()
            : TEXT("Processing");
        const FString Message = FString::Printf(TEXT("Log processed: %s"), *ActionName);
        UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, Message);
        }
        CancelArmedCut();
        PlacementStatus = Message;
        return true;
    }

    const FString Reason = Piece->GetCutActionBlockReason(CutAction);
    const FString Message = Reason.IsEmpty()
        ? TEXT("This cut operation is not available on the aimed log.")
        : Reason;
    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
    CancelArmedCut();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, Message);
    }
    return false;
}

void ULogCarryComponent::BeginDragAimedLogInput()
{
    // BindKey requires a void handler; the bool result is only for Blueprint callers.
    BeginDragAimedLog();
}

void ULogCarryComponent::ReleaseHeldFromLoad()
{
    if (UCarryLoadComponent* Load = GetOwner() ? GetOwner()->FindComponentByClass<UCarryLoadComponent>() : nullptr)
        Load->UnregisterCarried(HeldPiece);
}

namespace
{
    bool SetMaterialHeld(AActor* Piece, bool bHeld)
    {
        if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
            return Stone->SetHeldState(bHeld);
        if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
            return Stick->SetHeldState(bHeld);
        return false;
    }

    void EnsureMaterialAnchors(AActor* Piece)
    {
        if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
            Stone->EnsureAutomaticAnchors();
        else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
            Stick->EnsureAutomaticAnchors();
    }

    FVector LocalAxisVector(ELogLongitudinalAxis Axis)
    {
        switch (Axis)
        {
        case ELogLongitudinalAxis::Y: return FVector::YAxisVector;
        case ELogLongitudinalAxis::Z: return FVector::ZAxisVector;
        case ELogLongitudinalAxis::X:
        default: return FVector::XAxisVector;
        }
    }

    void AddDashedLine(TArray<FVector>& Out, const FVector& From, const FVector& To)
    {
        constexpr float DashCm = 12.0f;
        constexpr float GapCm = 8.0f;
        const float Length = FVector::Dist(From, To);
        const FVector Direction = (To - From).GetSafeNormal();
        for (float Start = 0.0f; Start < Length; Start += DashCm + GapCm)
        {
            Out.Add(From + Direction * Start);
            Out.Add(From + Direction * FMath::Min(Length, Start + DashCm));
        }
    }
}

bool ULogCarryComponent::TransferMaterialOwnership(
    AActor* Piece, EWorldItemOwnerKind From, EWorldItemOwnerKind To, const FVector& Location) const
{
    const IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece);
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    FWorldOwnedItemRecord Item;
    if (!Structural || !Ownership || !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item) ||
        Item.OwnerKind != From)
    {
        return false;
    }
    FWorldOwnedItemRecord Result;
    return Ownership->TryTransferItem(FGuid::NewGuid(), Item.ItemId, Item.Revision, From, To,
        To == EWorldItemOwnerKind::Held ? CarryOwnerId : FGuid(), Location, Result);
}

bool ULogCarryComponent::TryPickupMaterialPiece(AActor* Piece)
{
    const IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece);
    if (!Structural || Cast<ALogBuildPiece>(Piece) || IsValid(HeldPiece) || IsValid(HeldMaterialPiece))
        return false;
    if (Structural->IsStructurallyPlaced())
    {
        PlacementStatus = TEXT("This piece is part of a structure.");
        return false;
    }
    if (!CarryOwnerId.IsValid())
        CarryOwnerId = FGuid::NewGuid();

    UCarryLoadComponent* Load = GetOwner() ? GetOwner()->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    FString Reason;
    if (Load && !Load->CanCarryMore(Piece, Reason))
    {
        PlacementStatus = Reason;
        return false;
    }
    if (!TransferMaterialOwnership(Piece, EWorldItemOwnerKind::World, EWorldItemOwnerKind::Held, Piece->GetActorLocation()))
    {
        PlacementStatus = TEXT("The ownership ledger rejected the pickup.");
        return false;
    }
    if (!SetMaterialHeld(Piece, true))
    {
        TransferMaterialOwnership(Piece, EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, Piece->GetActorLocation());
        PlacementStatus = TEXT("The piece refused to be held.");
        return false;
    }

    EnsureMaterialAnchors(Piece);
    if (UStaticMeshComponent* Mesh = Piece->FindComponentByClass<UStaticMeshComponent>())
        Mesh->SetSimulatePhysics(false);
    Piece->SetActorEnableCollision(false);
    HeldMaterialPiece = Piece;
    if (Load)
        Load->RegisterCarried(Piece);
    PlacementStatus = TEXT("Holding: aim at the ground, a placed stone or the top of a log, then E.");
    return true;
}

FTransform ULogCarryComponent::MakeUprightTransform(AActor* Piece, const FVector& BottomWorldPoint) const
{
    FVector ViewLocation;
    FRotator ViewRotation;
    const float Yaw = GetViewPoint(ViewLocation, ViewRotation) ? ViewRotation.Yaw : 0.0f;
    FQuat Rotation = FRotator(0.0f, Yaw + HeldYawOffset, 0.0f).Quaternion();
    // Sticks lean forward by the placement mode (upright, 45 degrees, lying).
    const float Tilt = Cast<AStickPrimitivePiece>(Piece) ? GetStickTiltDegrees() : 0.0f;
    float Lift = 0.0f;
    if (Tilt > 0.0f)
    {
        Rotation = Rotation * FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Tilt));
        if (const UStaticMeshComponent* Mesh = Piece->FindComponentByClass<UStaticMeshComponent>(); Mesh && Mesh->GetStaticMesh())
            Lift = Mesh->GetStaticMesh()->GetBounds().BoxExtent.X * Piece->GetActorScale3D().X * FMath::Sin(FMath::DegreesToRadians(Tilt));
    }

    FVector BottomLocal = FVector::ZeroVector;
    FStructuralAnchorView Bottom;
    if (const IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Piece))
    {
        if (Structural->GetStructuralAnchor(TEXT("AutoBottom"), Bottom))
            BottomLocal = Bottom.LocalTransform.GetLocation();
    }
    const FVector Scale = Piece ? Piece->GetActorScale3D() : FVector::OneVector;
    return FTransform(Rotation, BottomWorldPoint + FVector::UpVector * Lift - Rotation.RotateVector(BottomLocal * Scale), Scale);
}

FPlacementSolution ULogCarryComponent::SolveMaterialOn(AActor* Target, const FVector& AimPoint) const
{
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!Kernel || !IsValid(HeldMaterialPiece))
        return FPlacementSolution();
    FVector ViewLocation;
    FRotator ViewRotation;
    const float Yaw = GetViewPoint(ViewLocation, ViewRotation) ? ViewRotation.Yaw : 0.0f;
    FPlacementIntent Intent;
    Intent.Held = HeldMaterialPiece;
    Intent.AimActor = Target;
    Intent.AimPoint = AimPoint;
    Intent.CarryYawDeg = Yaw + HeldYawOffset;
    // Sticks lean by the placement mode (upright, 45 degrees, lying).
    Intent.HeldAngleDeg = GetStickTiltDegrees();
    Intent.AllowedProfiles = { TEXT("stone.on_anchor"), TEXT("stick.on_anchor") };
    return Kernel->Solve(Intent);
}

void ULogCarryComponent::UpdateHeldMaterialPreview()
{
    AActor* Piece = HeldMaterialPiece;
    MaterialSnapTarget = nullptr;
    MaterialSnapAnchor = NAME_None;
    MaterialSolution = FPlacementSolution();
    bMaterialGroundPreview = false;

    FHitResult Hit;
    FVector ViewLocation;
    FRotator ViewRotation;
    if (!TraceFromView(PlacementTraceDistance, Hit) || !GetViewPoint(ViewLocation, ViewRotation))
    {
        if (GetViewPoint(ViewLocation, ViewRotation))
            Piece->SetActorLocation(ViewLocation + ViewRotation.Vector() * CarryDistance);
        PlacementStatus = TEXT("Aim at the ground, a placed stone or the top of a log.");
        return;
    }

    AActor* Target = Hit.GetActor();
    if (Cast<IStructuralPieceInterface>(Target))
    {
        // The placement kernel picks the free, upward-facing point nearest the crosshair.
        EnsureMaterialAnchors(Target);
        const FPlacementSolution Solution = SolveMaterialOn(Target, Hit.ImpactPoint);
        if (const FPlacementLink* Link = Solution.bValid ? Solution.GetPrimaryLink() : nullptr)
        {
            MaterialSnapTarget = Target;
            MaterialSnapAnchor = Link->Target.FeatureId;
            MaterialPreviewTransform = Solution.PieceTransform;
            MaterialSolution = Solution;
            Piece->SetActorTransform(MaterialPreviewTransform);
            PlacementStatus = FString::Printf(TEXT("Snap to %s: E to place."), *MaterialSnapAnchor.ToString());
            return;
        }
        Piece->SetActorLocation(ViewLocation + ViewRotation.Vector() * CarryDistance);
        PlacementStatus = Solution.Reason.IsEmpty() ? TEXT("No free top anchor here for this material.") : Solution.Reason;
        return;
    }

    bMaterialGroundPreview = true;
    MaterialPreviewTransform = MakeUprightTransform(Piece, Hit.ImpactPoint);
    Piece->SetActorTransform(MaterialPreviewTransform);
    PlacementStatus = TEXT("E to place on the ground.");
}

void ULogCarryComponent::FinishMaterialPlacement(AActor* Piece)
{
    // The kernel already moved it to the world in the ledger.
    Piece->SetActorEnableCollision(true);
    if (UCarryLoadComponent* Load = GetOwner() ? GetOwner()->FindComponentByClass<UCarryLoadComponent>() : nullptr)
        Load->UnregisterCarried(Piece);
    HeldMaterialPiece = nullptr;
    MaterialSnapTarget = nullptr;
    MaterialSnapAnchor = NAME_None;
    MaterialSolution = FPlacementSolution();
}

bool ULogCarryComponent::TryPlaceHeldMaterialOn(AActor* Target, FName TargetAnchorId)
{
    AActor* Piece = HeldMaterialPiece;
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!Piece || !Target || Target == Piece || !Kernel || !Cast<IStructuralPieceInterface>(Target))
        return false;
    EnsureMaterialAnchors(Target);
    // Aim straight at the asked point so the kernel takes that one.
    FPlacementFeature Wanted;
    FPlacementSolution Solution;
    if (FPlacementProviderRegistry::FindFeature(Target, TargetAnchorId, Wanted))
        Solution = SolveMaterialOn(Target, Wanted.WorldFrame.GetLocation());
    const FPlacementLink* Link = Solution.bValid ? Solution.GetPrimaryLink() : nullptr;
    if (!Link || Link->Target.FeatureId != TargetAnchorId)
    {
        PlacementStatus = FString::Printf(TEXT("Cannot place here: %s"),
            Solution.Reason.IsEmpty() || Solution.bValid ? TEXT("the anchor is taken or does not match.") : *Solution.Reason);
        return false;
    }
    FString Reason;
    if (!Kernel->CommitPlacement(Piece, Solution, FPlacementCommitOptions(), Reason))
    {
        PlacementStatus = FString::Printf(TEXT("Cannot place here: %s"), *Reason);
        return false;
    }
    FinishMaterialPlacement(Piece);
    PlacementStatus = TEXT("Placed.");
    return true;
}

bool ULogCarryComponent::TryPlaceHeldMaterialOnGround(FVector GroundPoint)
{
    AActor* Piece = HeldMaterialPiece;
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!Piece || GroundPoint.ContainsNaN() || !Kernel)
        return false;
    FString Reason;
    if (!Kernel->CommitGroundPlacement(Piece, MakeUprightTransform(Piece, GroundPoint), FPlacementCommitOptions(), Reason))
    {
        PlacementStatus = Reason;
        return false;
    }
    FinishMaterialPlacement(Piece);
    PlacementStatus = TEXT("Placed on the ground.");
    return true;
}

bool ULogCarryComponent::PlaceHeldMaterialPiece()
{
    if (!IsValid(HeldMaterialPiece))
        return false;
    if (MaterialSnapTarget && !MaterialSnapAnchor.IsNone())
        return TryPlaceHeldMaterialOn(MaterialSnapTarget, MaterialSnapAnchor);
    if (bMaterialGroundPreview)
    {
        FStructuralAnchorView Bottom;
        FVector BottomWorld = HeldMaterialPiece->GetActorLocation();
        if (const IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(HeldMaterialPiece.Get()))
        {
            if (Structural->GetStructuralAnchor(TEXT("AutoBottom"), Bottom))
                BottomWorld = (Bottom.LocalTransform * HeldMaterialPiece->GetActorTransform()).GetLocation();
        }
        return TryPlaceHeldMaterialOnGround(BottomWorld);
    }
    PlacementStatus = TEXT("Aim at the ground, a placed stone or the top of a log.");
    return false;
}

void ULogCarryComponent::DropHeldMaterialPiece()
{
    AActor* Piece = HeldMaterialPiece;
    if (!Piece)
        return;
    FVector ViewLocation;
    FRotator ViewRotation;
    const FVector DropLocation = GetViewPoint(ViewLocation, ViewRotation)
        ? ViewLocation + ViewRotation.Vector() * 110.0f : Piece->GetActorLocation();
    if (!TransferMaterialOwnership(Piece, EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, DropLocation))
        return;
    Piece->SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Piece->SetActorEnableCollision(true);
    SetMaterialHeld(Piece, false);
    if (UStaticMeshComponent* Mesh = Piece->FindComponentByClass<UStaticMeshComponent>())
    {
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
    }
    if (UCarryLoadComponent* Load = GetOwner() ? GetOwner()->FindComponentByClass<UCarryLoadComponent>() : nullptr)
        Load->UnregisterCarried(Piece);
    HeldMaterialPiece = nullptr;
    PlacementStatus = TEXT("Dropped.");
}

bool ULogCarryComponent::EvaluateFarEndSupport(
    const ALogBuildPiece* Piece, int32 OwnSnapPointIndex, AActor*& OutSupport, bool& bOutGround) const
{
    OutSupport = nullptr;
    bOutGround = false;
    const UStaticMeshComponent* Mesh = Piece ? Piece->GetLogMesh() : nullptr;
    if (!Mesh || !Mesh->GetStaticMesh() || !Piece->GetSnapPoints().IsValidIndex(OwnSnapPointIndex) || !GetWorld())
        return true;

    const FTransform MeshTransform = Mesh->GetComponentTransform();
    const FVector AxisLocal = LocalAxisVector(Piece->LongitudinalAxis);
    const FVector AxisWorld = MeshTransform.TransformVectorNoScale(AxisLocal).GetSafeNormal();
    if (FMath::Abs(AxisWorld.Z) > 0.7f)
        return true; // A post stands on its joint.

    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const float HalfLength = FVector::DotProduct(Bounds.BoxExtent, AxisLocal.GetAbs());
    const FVector JointLocal = Piece->GetSnapPointLocalTransform(OwnSnapPointIndex).GetLocation();
    const float JointAlong = FVector::DotProduct(JointLocal - Bounds.Origin, AxisLocal);
    if (HalfLength <= KINDA_SMALL_NUMBER || FMath::Abs(JointAlong) < 0.3f * HalfLength)
        return true; // Joined near the middle: balanced on its support.

    const FVector FarEnd = MeshTransform.TransformPosition(
        Bounds.Origin - AxisLocal * FMath::Sign(JointAlong) * HalfLength);
    const FVector RadiusExtent = Bounds.BoxExtent * (FVector::OneVector - AxisLocal.GetAbs());
    const float Radius = (RadiusExtent * MeshTransform.GetScale3D().GetAbs()).GetMax();

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LogFarEndSupport), false, Piece);
    if (GetOwner())
        Params.AddIgnoredActor(GetOwner());
    const FVector Start = FarEnd + FVector::UpVector * 5.0f;
    const FVector End = FarEnd - FVector::UpVector * (Radius + FarEndSupportProbeCm);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
        return false;
    if (Cast<IStructuralPieceInterface>(Hit.GetActor()))
        OutSupport = Hit.GetActor();
    else
        bOutGround = true;
    return true;
}

ALogBuildPiece* ULogCarryComponent::GetAimedLooseLog() const
{
    FHitResult Hit;
    if (!TraceFromView(PickupRange, Hit))
        return nullptr;
    ALogBuildPiece* Log = Cast<ALogBuildPiece>(Hit.GetActor());
    return IsValid(Log) && !Log->IsHeld() ? Log : nullptr;
}

bool ULogCarryComponent::BuildCutPreviewSegments(
    const ALogBuildPiece* Log, ELogCutAction Action, TArray<FVector>& OutSegmentPoints) const
{
    OutSegmentPoints.Reset();
    const UStaticMeshComponent* Mesh = Log ? Log->GetLogMesh() : nullptr;
    if (!Mesh || !Mesh->GetStaticMesh())
        return false;

    const FTransform MeshTransform = Mesh->GetComponentTransform();
    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const FVector Axis = LocalAxisVector(Log->LongitudinalAxis);
    // Two cross-section axes: Side is horizontal, Up is vertical in the mesh.
    const FVector Up = FMath::Abs(Axis.Z) > 0.5f ? FVector::XAxisVector : FVector::ZAxisVector;
    const FVector Side = FVector::CrossProduct(Up, Axis).GetSafeNormal();
    const float HalfLength = FVector::DotProduct(Bounds.BoxExtent, Axis.GetAbs());
    const float HalfSide = FVector::DotProduct(Bounds.BoxExtent, Side.GetAbs());
    const float HalfUp = FVector::DotProduct(Bounds.BoxExtent, Up.GetAbs());
    const FVector Centre = Bounds.Origin;
    auto World = [&MeshTransform](const FVector& Local) { return MeshTransform.TransformPosition(Local); };
    auto LongLine = [&](const FVector& Offset)
    {
        AddDashedLine(OutSegmentPoints, World(Centre + Offset - Axis * HalfLength),
            World(Centre + Offset + Axis * HalfLength));
    };

    float FirstShare = -1.0f;
    switch (Action)
    {
    case ELogCutAction::SplitLengthHalf: FirstShare = 0.5f; break;
    case ELogCutAction::SplitLengthThreeQuarter: FirstShare = 0.75f; break;
    case ELogCutAction::SplitHalfLengthIntoQuarters:
    case ELogCutAction::SplitQuarterLengthIntoEighths: FirstShare = 0.5f; break;
    default: break;
    }

    if (FirstShare > 0.0f)
        return BuildCutRingSegments(Log, FirstShare, OutSegmentPoints);

    switch (Action)
    {
    case ELogCutAction::SplitHalf:
        // Horizontal split plane through the centre, seen on both sides.
        LongLine(Side * HalfSide * 1.02f);
        LongLine(-Side * HalfSide * 1.02f);
        break;
    case ELogCutAction::Quarter:
        // Vertical split plane through the centre, seen on top.
        LongLine(Up * HalfUp * 1.02f);
        break;
    case ELogCutAction::Hewn:
    case ELogCutAction::HewSecondFace:
    case ELogCutAction::Kalas:
    case ELogCutAction::Plank:
        // Edges of the flat face that will be hewn on top.
        LongLine(Up * HalfUp * 0.85f + Side * HalfSide * 0.6f);
        LongLine(Up * HalfUp * 0.85f - Side * HalfSide * 0.6f);
        break;
    case ELogCutAction::Gutter:
    case ELogCutAction::Trough:
        // The channel carved along the top.
        LongLine(Up * HalfUp * 1.02f + Side * HalfSide * 0.3f);
        LongLine(Up * HalfUp * 1.02f - Side * HalfSide * 0.3f);
        break;
    default:
        return false;
    }
    return true;
}

namespace
{
    bool IsLengthCut(ELogCutAction Action)
    {
        return Action == ELogCutAction::SplitLengthHalf || Action == ELogCutAction::SplitLengthThreeQuarter ||
            Action == ELogCutAction::SplitHalfLengthIntoQuarters || Action == ELogCutAction::SplitQuarterLengthIntoEighths;
    }

    /** Mesh-space centre, long axis and half length of a log, from its mesh bounds. */
    bool GetMeshFrame(const ALogBuildPiece* Log, FTransform& OutMeshTransform, FVector& OutCentre,
        FVector& OutAxis, float& OutHalfLength, float& OutRadius)
    {
        const UStaticMeshComponent* Mesh = Log ? Log->GetLogMesh() : nullptr;
        if (!Mesh || !Mesh->GetStaticMesh())
            return false;
        OutMeshTransform = Mesh->GetComponentTransform();
        const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
        OutAxis = LocalAxisVector(Log->LongitudinalAxis);
        OutCentre = Bounds.Origin;
        OutHalfLength = FVector::DotProduct(Bounds.BoxExtent, OutAxis.GetAbs());
        const FVector Cross = Bounds.BoxExtent - Bounds.BoxExtent * OutAxis.GetAbs();
        OutRadius = Cross.GetMax();
        return OutHalfLength > KINDA_SMALL_NUMBER;
    }

    void AddDashedRing(TArray<FVector>& Out, const FTransform& Frame, const FVector& Centre,
        const FVector& Side, const FVector& Up, float Radius)
    {
        constexpr int32 Segments = 24;
        for (int32 Index = 0; Index < Segments; Index += 2)
        {
            const float A0 = 2.0f * PI * Index / Segments;
            const float A1 = 2.0f * PI * (Index + 1) / Segments;
            Out.Add(Frame.TransformPosition(Centre + (Side * FMath::Cos(A0) + Up * FMath::Sin(A0)) * Radius));
            Out.Add(Frame.TransformPosition(Centre + (Side * FMath::Cos(A1) + Up * FMath::Sin(A1)) * Radius));
        }
    }
}

bool ULogCarryComponent::BuildCutRingSegments(const ALogBuildPiece* Log, float Share, TArray<FVector>& OutSegmentPoints) const
{
    OutSegmentPoints.Reset();
    FTransform Frame;
    FVector Centre, Axis;
    float HalfLength = 0.0f, Radius = 0.0f;
    if (Share < 0.0f || !GetMeshFrame(Log, Frame, Centre, Axis, HalfLength, Radius))
        return false;
    const FVector Up = FMath::Abs(Axis.Z) > 0.5f ? FVector::XAxisVector : FVector::ZAxisVector;
    const FVector Side = FVector::CrossProduct(Up, Axis).GetSafeNormal();
    const FVector RingCentre = Centre - Axis * HalfLength + Axis * (2.0f * HalfLength * Share);
    AddDashedRing(OutSegmentPoints, Frame, RingCentre, Side, Up, Radius * 1.08f);
    return true;
}

bool ULogCarryComponent::ResolveCutChoice(const ALogBuildPiece* Log, const FVector& WorldPoint, FLogCutChoice& Out) const
{
    Out = FLogCutChoice();
    Out.Action = SelectedCutAction;
    if (!IsValid(Log))
        return false;

    FTransform Frame;
    FVector Centre, Axis;
    float HalfLength = 0.0f, Radius = 0.0f;
    MyProject3Construction::FLogGeometry Shape;
    if (!GetMeshFrame(Log, Frame, Centre, Axis, HalfLength, Radius) || !MyProject3Construction::GetLogGeometry(Log, Shape))
        return false;
    const FVector Local = Frame.InverseTransformPosition(WorldPoint);
    const float T = FMath::Clamp((FVector::DotProduct(Local - Centre, Axis) + HalfLength) / (2.0f * HalfLength), 0.0f, 1.0f);
    const float Length = Shape.Length();
    constexpr float Step = 25.0f;

    if (Log->IsPlaced() && MyProject3Construction::IsVerticalLog(Log))
    {
        // The top of a standing log is sharpened, whatever cut is selected.
        const bool bEndALowTop = Shape.EndA().Z <= Shape.EndB().Z;
        const float FromBottomTop = bEndALowTop ? T * Length : (1.0f - T) * Length;
        if (Length - FromBottomTop < FMath::Max(30.0f, 0.15f * Length) && !Log->FindComponentByClass<USharpenedTipComponent>())
        {
            Out.Kind = ELogCutKind::Sharpen;
            Out.Share = bEndALowTop ? 1.0f : 0.0f;
            return true;
        }
    }
    if (!IsLengthCut(SelectedCutAction))
        return !Log->IsPlaced();

    if (Log->IsPlaced())
    {
        // Only a standing post is cut where it stands: from the top down.
        if (!MyProject3Construction::IsVerticalLog(Log))
            return false;
        const bool bEndALow = Shape.EndA().Z <= Shape.EndB().Z;
        const float FromBottom = bEndALow ? T * Length : (1.0f - T) * Length;
        const float Keep = FMath::Clamp(FMath::RoundToFloat(FromBottom / Step) * Step, 30.0f, Length - 20.0f);
        if (Length < 55.0f)
            return false;
        Out.Kind = ELogCutKind::ShortenPost;
        Out.Share = bEndALow ? Keep / Length : 1.0f - Keep / Length;
        return true;
    }

    // Recipe points for unshortened pieces, within 45 cm of the aim.
    const float AxisScale = FMath::Abs(FVector::DotProduct(Log->GetActorScale3D(), LocalAxisVector(Log->LongitudinalAxis).GetAbs()));
    const bool bTrimmed = FMath::Abs(AxisScale - 1.0f) > 0.02f;
    if (!bTrimmed)
    {
        switch (Log->GetLengthFraction())
        {
        case ELogLengthFraction::FullLength:
            if (FMath::Abs(T - 0.5f) * Length <= 45.0f)
            {
                Out.Action = ELogCutAction::SplitLengthHalf;
                Out.Share = 0.5f;
                return true;
            }
            if (FMath::Abs(T - 0.75f) * Length <= 45.0f || FMath::Abs(T - 0.25f) * Length <= 45.0f)
            {
                // The recipe puts the 1/4 piece at +X; aiming at the -X end turns the log first.
                Out.Action = ELogCutAction::SplitLengthThreeQuarter;
                Out.Share = T > 0.5f ? 0.75f : 0.25f;
                Out.bFlip = T < 0.5f;
                return true;
            }
            break;
        case ELogLengthFraction::HalfLength:
            if (FMath::Abs(T - 0.5f) * Length <= 45.0f)
            {
                Out.Action = ELogCutAction::SplitHalfLengthIntoQuarters;
                Out.Share = 0.5f;
                return true;
            }
            break;
        case ELogLengthFraction::QuarterLength:
            if (FMath::Abs(T - 0.5f) * Length <= 30.0f)
            {
                Out.Action = ELogCutAction::SplitQuarterLengthIntoEighths;
                Out.Share = 0.5f;
                return true;
            }
            break;
        default:
            break;
        }
    }

    // Anywhere else: a free cut on a 25 cm step, both parts at least 30 cm.
    if (Length < 60.0f)
        return false;
    const float Cut = FMath::Clamp(FMath::RoundToFloat(T * Length / Step) * Step, 30.0f, Length - 30.0f);
    Out.Kind = ELogCutKind::FreeLength;
    Out.Share = Cut / Length;
    return true;
}

bool ULogCarryComponent::ResolveCutAtPoint(const ALogBuildPiece* Log, const FVector& WorldPoint,
    ELogCutAction& OutAction, float& OutShare, bool& bOutFlip) const
{
    FLogCutChoice Choice;
    const bool bResolved = ResolveCutChoice(Log, WorldPoint, Choice);
    OutAction = Choice.Action;
    OutShare = Choice.Share;
    bOutFlip = Choice.bFlip;
    return bResolved;
}

bool ULogCarryComponent::ResolveAimedCutChoice(const ALogBuildPiece* Log, FLogCutChoice& Out) const
{
    FHitResult Hit;
    const FVector Point = TraceFromView(PickupRange, Hit) && Hit.GetActor() == Log
        ? Hit.ImpactPoint : (Log ? Log->GetActorLocation() : FVector::ZeroVector);
    return ResolveCutChoice(Log, Point, Out);
}

bool ULogCarryComponent::ResolveAimedCut(const ALogBuildPiece* Log, ELogCutAction& OutAction, float& OutShare, bool& bOutFlip) const
{
    FLogCutChoice Choice;
    const bool bResolved = ResolveAimedCutChoice(Log, Choice);
    OutAction = Choice.Action;
    OutShare = Choice.Share;
    bOutFlip = Choice.bFlip;
    return bResolved;
}

bool ULogCarryComponent::CanMakeCut(const ALogBuildPiece* Log, const FLogCutChoice& Choice, FString& OutReason) const
{
    OutReason.Reset();
    if (!IsValid(Log) || Log->IsHeld())
    {
        OutReason = TEXT("Nothing to cut.");
        return false;
    }
    switch (Choice.Kind)
    {
    case ELogCutKind::ShortenPost:
        if (!Log->IsPlaced() || !MyProject3Construction::IsVerticalLog(Log))
            OutReason = TEXT("Only a standing post can be shortened where it stands.");
        else if (PostHoldsAnything(Log))
            OutReason = TEXT("This post holds up other pieces: a post can only be cut when nothing rests on it.");
        break;
    case ELogCutKind::Sharpen:
        if (!Log->IsPlaced() || !MyProject3Construction::IsVerticalLog(Log))
            OutReason = TEXT("Sharpen a log that stands upright.");
        else if (Log->FindComponentByClass<USharpenedTipComponent>())
            OutReason = TEXT("Already sharpened.");
        break;
    case ELogCutKind::FreeLength:
    {
        EWorldItemOwnerKind ItemOwner = EWorldItemOwnerKind::World;
        if (Log->IsPlaced() || !MyProject3Construction::GetPieceOwnerKind(Log, ItemOwner) || ItemOwner != EWorldItemOwnerKind::World)
            OutReason = TEXT("Cut a loose log, or a standing post.");
        break;
    }
    case ELogCutKind::Recipe:
    default:
        if (!Log->CanApplyCutAction(Choice.Action))
            OutReason = Log->GetCutActionBlockReason(Choice.Action);
        break;
    }
    return OutReason.IsEmpty();
}

FString ULogCarryComponent::DescribeCut(const ALogBuildPiece* Log, const FLogCutChoice& Choice) const
{
    MyProject3Construction::FLogGeometry Shape;
    const float Length = MyProject3Construction::GetLogGeometry(Log, Shape) ? Shape.Length() : 0.0f;
    const float First = FMath::RoundToFloat(Length * FMath::Max(Choice.Share, 0.0f));
    switch (Choice.Kind)
    {
    case ELogCutKind::ShortenPost:
    {
        const bool bEndALow = Shape.EndA().Z <= Shape.EndB().Z;
        const float Keep = bEndALow ? First : Length - First;
        return FString::Printf(TEXT("Shorten the post to %.0f cm"), Keep);
    }
    case ELogCutKind::FreeLength:
        return FString::Printf(TEXT("Cut here: %.0f + %.0f cm"), First, Length - First);
    case ELogCutKind::Sharpen:
        return TEXT("Sharpen the top into a stake");
    case ELogCutKind::Recipe:
    default:
        if (Choice.Share >= 0.0f)
            return FString::Printf(TEXT("Cut: %.0f + %.0f cm"), First, Length - First);
        return StaticEnum<ELogCutAction>()->GetDisplayNameTextByValue(static_cast<int64>(Choice.Action)).ToString();
    }
}

bool ULogCarryComponent::GetArmedCutChoice(const ALogBuildPiece*& OutLog, FLogCutChoice& OutChoice) const
{
    OutLog = ArmedLog.Get();
    OutChoice.Action = ArmedAction;
    OutChoice.Share = ArmedShare;
    OutChoice.bFlip = bArmedFlipPending;
    OutChoice.Kind = ArmedKind;
    return IsValid(OutLog) && !OutLog->IsHeld();
}

bool ULogCarryComponent::GetArmedCut(const ALogBuildPiece*& OutLog, ELogCutAction& OutAction, float& OutShare) const
{
    OutLog = ArmedLog.Get();
    OutAction = ArmedAction;
    OutShare = ArmedShare;
    return IsValid(OutLog) && !OutLog->IsHeld();
}

void ULogCarryComponent::CancelArmedCut()
{
    ArmedLog.Reset();
    ArmedShare = -1.0f;
    bArmedFlipPending = false;
    ArmedKind = ELogCutKind::Recipe;
    ArmedWork = 0.0f;
}

bool ULogCarryComponent::CutLogAtShare(ALogBuildPiece* Log, float Share)
{
    using namespace MyProject3Construction;
    FLogGeometry Shape;
    if (!IsValid(Log) || Log->IsHeld() || !GetLogGeometry(Log, Shape) || Share <= 0.0f || Share >= 1.0f)
        return false;
    const float Split = -Shape.HalfLength + Shape.Length() * Share;
    constexpr float Gap = 1.0f;
    const bool bPost = Log->IsPlaced();
    // A post keeps its lower end; a loose log keeps its -X end.
    const bool bKeepA = !bPost || Shape.EndA().Z <= Shape.EndB().Z;
    const FVector OldTop = bKeepA ? Shape.EndB() : Shape.EndA();
    const float Removed = bKeepA ? Shape.HalfLength - Split : Split + Shape.HalfLength;
    if (Removed < 15.0f || Shape.Length() - Removed < 25.0f)
        return false;

    // Pieces hanging on the log must not be scaled with it.
    TArray<AActor*> Children;
    Log->GetAttachedActors(Children);
    for (AActor* Child : Children)
        Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    ALogBuildPiece* Other = SpawnLogCopy(Log, Log->GetActorTransform());
    if (!Other)
    {
        for (AActor* Child : Children)
            Child->AttachToActor(Log, FAttachmentTransformRules::KeepWorldTransform);
        return false;
    }
    if (bKeepA)
    {
        TrimLogToRange(Other, Split + Gap, Shape.HalfLength);
        TrimLogToRange(Log, -Shape.HalfLength, Split - Gap);
    }
    else
    {
        TrimLogToRange(Other, -Shape.HalfLength, Split - Gap);
        TrimLogToRange(Log, Split + Gap, Shape.HalfLength);
    }
    for (ALogBuildPiece* Piece : { Log, Other })
    {
        UStaticMeshComponent* PieceMesh = Piece->GetLogMesh();
        if (PieceMesh && PieceMesh->IsSimulatingPhysics())
        {
            PieceMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            PieceMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
    }

    if (bPost)
    {
        // The cut-off top drops beside the post, out of the way of what comes down.
        FVector Side = GetOwner() ? (GetOwner()->GetActorLocation() - Shape.Centre).GetSafeNormal2D() : FVector::XAxisVector;
        if (Side.IsNearlyZero())
            Side = FVector::XAxisVector;
        const FVector Bottom = bKeepA ? Shape.EndA() : Shape.EndB();
        Other->SetActorLocation(Other->GetActorLocation() + Side * (Shape.Radius * 2.0f + 40.0f) +
            FVector(0.0f, 0.0f, Bottom.Z - Other->GetActorLocation().Z + 0.5f * Removed + 10.0f),
            false, nullptr, ETeleportType::TeleportPhysics);
        LowerPiecesOnPost(Log, OldTop, Removed + 2.0f * Gap, Children);
    }
    for (AActor* Child : Children)
    {
        if (IsValid(Child))
            Child->AttachToActor(Log, FAttachmentTransformRules::KeepWorldTransform);
    }
    return true;
}

void ULogCarryComponent::LowerPiecesOnPost(ALogBuildPiece* Post, const FVector& OldTop, float Drop, const TArray<AActor*>& Children)
{
    using namespace MyProject3Construction;
    FLogGeometry PostShape;
    GetLogGeometry(Post, PostShape);
    TArray<ALogBuildPiece*> Resting;
    for (AActor* Child : Children)
    {
        if (ALogBuildPiece* Piece = Cast<ALogBuildPiece>(Child))
            Resting.AddUnique(Piece);
    }
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        FLogGeometry Shape;
        if (*It == Post || !It->IsPlaced() || !GetLogGeometry(*It, Shape))
            continue;
        const float Reach = PostShape.Radius + Shape.Radius + 20.0f;
        if (FVector::Dist(ClosestPointOnSegment(OldTop, Shape.EndA(), Shape.EndB()), OldTop) < Reach &&
            !IsVerticalLog(*It))
            Resting.AddUnique(*It);
    }

    for (ALogBuildPiece* Piece : Resting)
    {
        FLogGeometry Shape;
        if (!GetLogGeometry(Piece, Shape))
            continue;
        const bool bANear = FVector::Dist(Shape.EndA(), OldTop) <= FVector::Dist(Shape.EndB(), OldTop);
        const FVector Near = bANear ? Shape.EndA() : Shape.EndB();
        const FVector Far = bANear ? Shape.EndB() : Shape.EndA();

        // Is the far end held by something else? Then the piece tilts about it.
        bool bFarHeld = false;
        for (TActorIterator<ALogBuildPiece> It(GetWorld()); It && !bFarHeld; ++It)
        {
            FLogGeometry Other;
            if (*It == Piece || *It == Post || !It->IsPlaced() || !GetLogGeometry(*It, Other))
                continue;
            bFarHeld = FVector::Dist(ClosestPointOnSegment(Far, Other.EndA(), Other.EndB()), Far) <
                Other.Radius + Shape.Radius + 15.0f;
        }
        if (!bFarHeld)
        {
            FHitResult Ground;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(PostLowerGround), false, Piece);
            bFarHeld = GetWorld()->LineTraceSingleByChannel(Ground, Far, Far - FVector(0, 0, Shape.Radius + 25.0f),
                ECC_Visibility, Params) && !Cast<ALogBuildPiece>(Ground.GetActor());
        }

        if (bFarHeld)
        {
            const FVector Arm = Near - Far;
            const float Angle = FMath::Asin(FMath::Clamp(Drop / FMath::Max(Arm.Size(), 1.0f), 0.0f, 0.9f));
            const FVector Hinge = FVector::CrossProduct(Arm, FVector::UpVector).GetSafeNormal();
            if (Hinge.IsNearlyZero())
                continue;
            FQuat Turn(Hinge, Angle);
            if (Turn.RotateVector(Arm).Z > Arm.Z)
                Turn = FQuat(Hinge, -Angle);
            FTransform Moved = Piece->GetActorTransform();
            Moved.SetRotation(Turn * Moved.GetRotation());
            Moved.SetLocation(Far + Turn.RotateVector(Moved.GetLocation() - Far));
            Piece->SetActorTransform(Moved, false, nullptr, ETeleportType::TeleportPhysics);
        }
        else
        {
            Piece->AddActorWorldOffset(FVector(0.0f, 0.0f, -Drop), false, nullptr, ETeleportType::TeleportPhysics);
        }
    }
}

int32 ULogCarryComponent::CountWallRows(const ALogBuildPiece* Target) const
{
    const UPlacementKernelSubsystem* Kernel = GetKernel();
    return Kernel ? Kernel->CountParallelRows(Target, GetMaxWallRows()) : 0;
}

int32 ULogCarryComponent::GetMaxWallRows() const
{
    const UPlacementKernelSubsystem* Kernel = GetKernel();
    const FPlacementProfile* Stack = Kernel ? Kernel->GetProfiles().Find(TEXT("log.parallel_stack")) : nullptr;
    return Stack ? FMath::RoundToInt(Stack->GetParam(TEXT("maxRows"), 6.0f)) : 6;
}

bool ULogCarryComponent::TryShoulderLog(ALogBuildPiece* Target)
{
    if (!IsValid(HeldPiece) || IsValid(ShoulderPiece) || !IsValid(Target) || Target == HeldPiece ||
        Target->IsPlaced() || Target->IsHeld() || !GetOwner())
        return false;
    ALogBuildPiece* First = HeldPiece;
    HeldPiece = nullptr;
    const bool bTaken = TryPickupPiece(Target);
    ALogBuildPiece* Second = HeldPiece;
    HeldPiece = First;
    TimeUntilSnapRefresh = 0.0f;
    if (!bTaken || !Second)
        return false;

    // On the right shoulder, lying along the walking direction, mostly behind.
    ShoulderPiece = Second;
    MyProject3Construction::FLogGeometry Shape;
    MyProject3Construction::GetLogGeometry(Second, Shape);
    const AActor* Owner = GetOwner();
    const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
    const FVector Centre = Owner->GetActorLocation() + Owner->GetActorRightVector() * 32.0f +
        FVector(0.0f, 0.0f, 55.0f) - Forward * FMath::Max(0.0f, Shape.HalfLength - 60.0f);
    Second->SetActorTransform(MyProject3Construction::MakeLogTransform(Second, Centre,
        MyProject3Construction::MakeLogRotation(Second, Forward, FVector::UpVector)));
    Second->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
    PlacementStatus = TEXT("Two logs: the second rides on your shoulder.");
    return true;
}

void ULogCarryComponent::PromoteShoulderLog()
{
    if (IsValid(HeldPiece) || !IsValid(ShoulderPiece) || IsValid(HeldMaterialPiece))
        return;
    ShoulderPiece->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    HeldPiece = ShoulderPiece;
    ShoulderPiece = nullptr;
    HeldYawOffset = 0.0f;
    TimeUntilSnapRefresh = 0.0f;
    PlacementStatus = TEXT("The second log is in your hands.");
}

void ULogCarryComponent::FindBeamsOnPost(const ALogBuildPiece* Post, TArray<ALogBuildPiece*>& OutBeams) const
{
    OutBeams.Reset();
    TArray<AActor*> Pieces;
    if (const UPlacementKernelSubsystem* Kernel = GetKernel())
        Kernel->FindRestingOnTop(Post, Pieces);
    for (AActor* Piece : Pieces)
    {
        if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
            OutBeams.Add(Log);
    }
}

bool ULogCarryComponent::PostHoldsAnything(const ALogBuildPiece* Post) const
{
    if (!IsValid(Post))
        return false;
    TArray<AActor*> Children;
    Post->GetAttachedActors(Children);
    if (Children.ContainsByPredicate([](const AActor* Child) { return Cast<ALogBuildPiece>(Child) != nullptr; }))
        return true;
    if (const UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>())
    {
        if (Graph->GetDirectDependents(Post->GetStructuralNodeId()).Num() > 0)
            return true;
    }
    TArray<ALogBuildPiece*> Beams;
    FindBeamsOnPost(Post, Beams);
    return Beams.Num() > 0;
}

bool ULogCarryComponent::CanStandAsPost(const ALogBuildPiece* Log)
{
    // The material decides (FLogFeatureProvider::CanStandUpright).
    const IPlacementFeatureProvider* Provider = FPlacementProviderRegistry::Find(Log);
    return Provider && Provider->CanStandUpright(Log);
}

bool ULogCarryComponent::IsBadlyDamaged(const AActor* Piece) const
{
    const UPlacementKernelSubsystem* Kernel = GetKernel();
    return Kernel && Kernel->IsDamaged(Piece);
}

bool ULogCarryComponent::SplitCarriedLogInHalf()
{
    ALogBuildPiece* Log = HeldPiece;
    // Only a log whose mesh family has split halves can be split.
    if (!IsValid(Log) || Log->GetCrossSectionProfile() != ELogCrossSectionProfile::Round ||
        Log->GetSelectedProcessingMeshSetId().IsNone())
        return false;
    bool bHasHalves = false;
    for (const FLogProcessingMeshSet& Set : Log->ProcessingMeshSets)
    {
        if (Set.VariantSetId != Log->GetSelectedProcessingMeshSetId())
            continue;
        for (const FLogGeometryVariantMesh& Variant : Set.MeshVariants)
            bHasHalves |= Variant.CrossSectionProfile == ELogCrossSectionProfile::HalfRound &&
                Variant.LengthFraction == Log->GetLengthFraction() && Variant.Mesh;
    }
    if (!bHasHalves)
        return false;

    // Into the world for the axe, then the halves back into the hands and onto the shoulder.
    ReleaseHeldFromLoad();
    HeldPiece = nullptr;
    bHasStackPlacement = false;
    bHasStrutPlacement = false;
    ClearSnapCandidate();
    FWorldOwnedItemRecord Record;
    Log->TryTransferOwnership(EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, FGuid(), Log->GetActorLocation(), Record);
    if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::Held)
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World);
    }
    Log->SetHeldState(false);
    if (Log->GetLogMesh())
        Log->GetLogMesh()->SetSimulatePhysics(false);

    TSet<ALogBuildPiece*> Before;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
        Before.Add(*It);
    const bool bSplit = Log->CanApplyCutAction(ELogCutAction::SplitHalf) && Log->ApplyCutAction(ELogCutAction::SplitHalf);
    ALogBuildPiece* Other = nullptr;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It && bSplit; ++It)
    {
        if (!Before.Contains(*It))
        {
            Other = *It;
            break;
        }
    }
    TryPickupPiece(Log);
    if (Other && Other->GetLogMesh())
    {
        Other->GetLogMesh()->SetSimulatePhysics(false);
        if (!TryShoulderLog(Other))
            Other->GetLogMesh()->SetSimulatePhysics(true);
    }
    if (bSplit)
        PlacementStatus = TEXT("The log was split in half: one half in your hands, the other on your shoulder.");
    return bSplit;
}

FPlacementSolution ULogCarryComponent::SolveStrut(const ALogBuildPiece* Held, const ALogBuildPiece* Aimed,
    const ALogBuildPiece* Beam, const FVector& AimPoint) const
{
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!Kernel || !Held)
        return FPlacementSolution();
    FPlacementIntent Intent;
    Intent.Held = const_cast<ALogBuildPiece*>(Held);
    Intent.AimActor = const_cast<ALogBuildPiece*>(Aimed);
    Intent.AimPoint = AimPoint;
    if (Beam)
        Intent.Candidates.Add(const_cast<ALogBuildPiece*>(Beam));
    Intent.AllowedProfiles = { TEXT("log.strut") };
    return Kernel->Solve(Intent);
}

bool ULogCarryComponent::ComputeStrutPlacement(const ALogBuildPiece* Held, const ALogBuildPiece* Post, const ALogBuildPiece* Beam,
    const FVector& AimPoint, FTransform& OutTransform, float& OutLength, FVector& OutBeamPoint) const
{
    const FPlacementSolution Solution = SolveStrut(Held, Post, Beam, AimPoint);
    if (!Solution.bValid || Solution.GetTarget(EPlacementLinkRole::Carries) != Beam)
        return false;
    OutTransform = Solution.PieceTransform;
    OutLength = Solution.RequiredLengthCm;
    OutBeamPoint = Solution.MarkerPoint;
    return true;
}

void ULogCarryComponent::UpdateStrutPlacement()
{
    using namespace MyProject3Construction;
    bHasStrutPlacement = false;
    StrutPost.Reset();
    StrutBeam.Reset();
    StrutSolution = FPlacementSolution();
    if (!IsValid(HeldPiece) || PlacementMode != ELogPlacementMode::Diagonal)
        return;
    FHitResult Hit;
    if (!TraceFromView(PlacementTraceDistance, Hit))
        return;
    ALogBuildPiece* Aimed = Cast<ALogBuildPiece>(Hit.GetActor());
    if (!IsValid(Aimed) || !Aimed->IsPlaced() || Aimed == HeldPiece)
        return;
    const FPlacementSolution Solution = SolveStrut(HeldPiece, Aimed, nullptr, Hit.ImpactPoint);
    if (!Solution.bValid)
    {
        if (!Solution.Reason.IsEmpty())
            PlacementStatus = Solution.Reason;
        return;
    }
    bHasStrutPlacement = true;
    StrutSolution = Solution;
    StrutTransform = Solution.PieceTransform;
    StrutLength = Solution.RequiredLengthCm;
    StrutJoint = Solution.MarkerPoint;
    StrutPost = Cast<ALogBuildPiece>(Solution.GetTarget(EPlacementLinkRole::Primary));
    StrutBeam = Cast<ALogBuildPiece>(Solution.GetTarget(EPlacementLinkRole::Carries));
    ClearSnapCandidate();
    HeldPiece->SetActorTransform(StrutTransform);
    PlacementStatus = HeldPiece->GetCrossSectionProfile() == ELogCrossSectionProfile::Round
        ? FString::Printf(TEXT("Strut (split in half and cut to %.0f cm): E to place."), StrutLength)
        : FString::Printf(TEXT("Strut (cut to %.0f cm): E to place."), StrutLength);
}

bool ULogCarryComponent::PlaceStrut()
{
    using namespace MyProject3Construction;
    ALogBuildPiece* Post = StrutPost.Get();
    ALogBuildPiece* Beam = StrutBeam.Get();
    const FVector Aim = StrutJoint;
    bHasStrutPlacement = false;
    if (!IsValid(HeldPiece) || !Post || !Beam)
        return false;
    // Struts are made from half logs: a round log is split first.
    if (HeldPiece->GetCrossSectionProfile() == ELogCrossSectionProfile::Round)
        SplitCarriedLogInHalf();
    FPlacementSolution Solution = IsValid(HeldPiece) ? SolveStrut(HeldPiece, Post, Beam, Aim) : FPlacementSolution();
    if (!Solution.bValid)
        return false;
    FLogGeometry Held;
    GetLogGeometry(HeldPiece, Held);
    const float Length = Solution.RequiredLengthCm;
    if (Held.Length() < Length * 0.85f)
    {
        PlacementStatus = FString::Printf(TEXT("Too short: the strut needs about %.0f cm."), Length);
        return false;
    }
    if (Held.Length() > Length + 10.0f && GetOwner())
    {
        CutOffRemainder(HeldPiece, Length, GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.0f);
        Solution = SolveStrut(HeldPiece, Post, Beam, Aim);
        if (!Solution.bValid)
            return false;
    }
    // Both ends at once: on the post, and the beam resting on it. One failing places nothing.
    if (!CommitHeldSolution(Solution))
        return false;
    PlacementStatus = TEXT("Strut placed: the beam also rests on it.");
    return true;
}

bool ULogCarryComponent::SnapPostSpacing(const FVector& Base, FVector& OutBase, ALogBuildPiece*& OutPartner, float& OutSpan) const
{
    OutPartner = nullptr;
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!Kernel)
        return false;
    FPlacementIntent Intent;
    Intent.Held = HeldPiece;
    Intent.AimPoint = Base;
    Intent.AllowedProfiles = { TEXT("log.post_on_ground") };
    const FPlacementSolution Solution = Kernel->Solve(Intent);
    const FPlacementLink* Reference = Solution.bValid ? Solution.FindLink(EPlacementLinkRole::Secondary) : nullptr;
    if (!Reference)
        return false;
    OutBase = Solution.MarkerPoint;
    OutPartner = Cast<ALogBuildPiece>(Reference->Target.Piece.Get());
    OutSpan = Solution.SpanCm;
    return OutPartner != nullptr;
}

bool ULogCarryComponent::GetPostSpan(const ALogBuildPiece*& OutPartner, float& OutSpan) const
{
    OutPartner = SpanPartner.Get();
    OutSpan = SpanCm;
    return IsValid(OutPartner) && IsValid(HeldPiece) && PlacementMode == ELogPlacementMode::Vertical;
}

bool ULogCarryComponent::BuildGroundMarkerSegments(TArray<FVector>& OutPoints) const
{
    using namespace MyProject3Construction;
    OutPoints.Reset();
    FLogGeometry Shape;
    if (!IsValid(HeldPiece) || HasConnectionPreview() || !bHasPlacementSurface || !GetLogGeometry(HeldPiece, Shape))
        return false;
    const FVector Low = Shape.EndA().Z < Shape.EndB().Z ? Shape.EndA() : Shape.EndB();
    if (IsVerticalLog(HeldPiece))
    {
        // A dashed circle where the post's foot will stand.
        const float Radius = Shape.Radius * 1.5f;
        const FVector Centre = Low + FVector(0, 0, 3.0f);
        constexpr int32 Segments = 24;
        for (int32 Index = 0; Index < Segments; Index += 2)
        {
            const float A0 = 2.0f * PI * Index / Segments;
            const float A1 = 2.0f * PI * (Index + 1) / Segments;
            OutPoints.Add(Centre + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.0f) * Radius);
            OutPoints.Add(Centre + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.0f) * Radius);
        }
        // A line to the post it lines up with.
        if (const ALogBuildPiece* Partner = SpanPartner.Get())
        {
            FLogGeometry Other;
            if (GetLogGeometry(Partner, Other))
            {
                const FVector Foot = Other.EndA().Z < Other.EndB().Z ? Other.EndA() : Other.EndB();
                const FVector Head = Other.EndA().Z < Other.EndB().Z ? Other.EndB() : Other.EndA();
                const FVector MyHead = Shape.EndA().Z < Shape.EndB().Z ? Shape.EndB() : Shape.EndA();
                AddDashedLine(OutPoints, Centre, Foot + FVector(0, 0, 3.0f));
                AddDashedLine(OutPoints, MyHead, Head);
            }
        }
        return true;
    }
    // A dashed rectangle on the ground under a lying log.
    FVector Along = Shape.Axis;
    Along.Z = 0.0f;
    Along = Along.GetSafeNormal() * Shape.HalfLength * FMath::Sqrt(FMath::Max(0.0f, 1.0f - Shape.Axis.Z * Shape.Axis.Z));
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Along.GetSafeNormal()) * Shape.Radius * 1.2f;
    const FVector Centre(Shape.Centre.X, Shape.Centre.Y, FMath::Min(Shape.EndA().Z, Shape.EndB().Z) - Shape.Radius + 3.0f);
    const FVector Corners[4] = { Centre - Along - Side, Centre + Along - Side, Centre + Along + Side, Centre - Along + Side };
    for (int32 Index = 0; Index < 4; ++Index)
        AddDashedLine(OutPoints, Corners[Index], Corners[(Index + 1) % 4]);
    return true;
}

bool ULogCarryComponent::BuildSharpenSegments(const ALogBuildPiece* Log, TArray<FVector>& OutPoints) const
{
    using namespace MyProject3Construction;
    OutPoints.Reset();
    FLogGeometry Shape;
    if (!GetLogGeometry(Log, Shape))
        return false;
    const bool bBTop = Shape.EndB().Z >= Shape.EndA().Z;
    const FVector Top = bBTop ? Shape.EndB() : Shape.EndA();
    const FVector Up = bBTop ? Shape.Axis : -Shape.Axis;
    const FVector RingCentre = Top - Up * FMath::Min(35.0f, Shape.HalfLength);
    const FVector SideA = FVector::CrossProduct(Up, FVector::XAxisVector).GetSafeNormal();
    const FVector SideB = FVector::CrossProduct(Up, SideA).GetSafeNormal();
    const float Radius = Shape.Radius * 1.08f;
    constexpr int32 Segments = 16;
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const float Angle = 2.0f * PI * Index / Segments;
        const FVector Rim = RingCentre + (SideA * FMath::Cos(Angle) + SideB * FMath::Sin(Angle)) * Radius;
        if (Index % 2 == 0)
        {
            const float Next = 2.0f * PI * (Index + 1) / Segments;
            OutPoints.Add(Rim);
            OutPoints.Add(RingCentre + (SideA * FMath::Cos(Next) + SideB * FMath::Sin(Next)) * Radius);
        }
        if (Index % 4 == 0)
            AddDashedLine(OutPoints, Rim, Top + Up * 4.0f);
    }
    return true;
}

bool ULogCarryComponent::BuildPlacementPreviewSegments(TArray<FVector>& OutOutline, TArray<FVector>& OutArrow) const
{
    OutOutline.Reset();
    OutArrow.Reset();
    FTransform Frame;
    FVector Centre, Axis;
    float HalfLength = 0.0f, Radius = 0.0f;
    if (!IsValid(HeldPiece) || !GetMeshFrame(HeldPiece, Frame, Centre, Axis, HalfLength, Radius))
        return false;

    // Where the log will lie: four dashed lines along it and a dashed ring at each end.
    const FVector Up = FMath::Abs(Axis.Z) > 0.5f ? FVector::XAxisVector : FVector::ZAxisVector;
    const FVector Side = FVector::CrossProduct(Up, Axis).GetSafeNormal();
    const float R = Radius * 1.06f;
    for (const FVector& Offset : { Side * R, -Side * R, Up * R, -Up * R })
        AddDashedLine(OutOutline, Frame.TransformPosition(Centre + Offset - Axis * HalfLength),
            Frame.TransformPosition(Centre + Offset + Axis * HalfLength));
    AddDashedRing(OutOutline, Frame, Centre - Axis * HalfLength, Side, Up, R);
    AddDashedRing(OutOutline, Frame, Centre + Axis * HalfLength, Side, Up, R);

    // The joint: an arrow along the log pointing into the piece it joins.
    FVector Joint = FVector::ZeroVector;
    bool bJoint = false;
    if (bHasStrutPlacement)
    {
        Joint = StrutJoint;
        bJoint = true;
    }
    else if (bHasStackPlacement && StackTarget.IsValid())
    {
        MyProject3Construction::FLogGeometry Target;
        if (MyProject3Construction::GetLogGeometry(StackTarget.Get(), Target))
        {
            Joint = MyProject3Construction::ClosestPointOnSegment(Frame.TransformPosition(Centre), Target.EndA(), Target.EndB());
            bJoint = true;
        }
    }
    else if (bHasValidSnap && IsValid(CurrentSnapTarget) && CurrentTargetSnapPointIndex != INDEX_NONE)
    {
        Joint = (CurrentSnapTarget->GetSnapPointLocalTransform(CurrentTargetSnapPointIndex) *
            CurrentSnapTarget->GetActorTransform()).GetLocation();
        bJoint = true;
    }
    if (!bJoint)
        return true;

    const FVector HeldCentre = Frame.TransformPosition(Centre);
    FVector Into = (Joint - HeldCentre).GetSafeNormal();
    if (Into.IsNearlyZero())
        Into = -FVector::UpVector;
    const FVector Tip = Joint - Into * 6.0f;
    const FVector Tail = Tip - Into * 90.0f;
    AddDashedLine(OutArrow, Tail, Tip);
    FVector Across = FVector::CrossProduct(Into, FVector::UpVector).GetSafeNormal();
    if (Across.IsNearlyZero())
        Across = FVector::CrossProduct(Into, FVector::XAxisVector).GetSafeNormal();
    OutArrow.Add(Tip);
    OutArrow.Add(Tip - Into * 22.0f + Across * 12.0f);
    OutArrow.Add(Tip);
    OutArrow.Add(Tip - Into * 22.0f - Across * 12.0f);
    return true;
}

bool ULogCarryComponent::StartHingeSwing(ALogBuildPiece* Piece, int32 OwnSnapIndex)
{
    AActor* Parent = Piece ? Piece->GetAttachParentActor() : nullptr;
    UPrimitiveComponent* ParentBody = Parent ? Cast<UPrimitiveComponent>(Parent->GetRootComponent()) : nullptr;
    UStaticMeshComponent* LogMesh = Piece ? Piece->GetLogMesh() : nullptr;
    if (!ParentBody || !LogMesh || !Piece->GetSnapPoints().IsValidIndex(OwnSnapIndex))
        return false;

    const FVector Pivot =
        (Piece->GetSnapPointLocalTransform(OwnSnapIndex) * Piece->GetActorTransform()).GetLocation();
    const FVector AxisWorld =
        LogMesh->GetComponentTransform().TransformVectorNoScale(LocalAxisVector(Piece->LongitudinalAxis));

    Piece->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    UPhysicsConstraintComponent* Hinge = NewObject<UPhysicsConstraintComponent>(Piece);
    Hinge->RegisterComponent();
    Hinge->SetWorldLocationAndRotation(Pivot, FRotationMatrix::MakeFromX(AxisWorld).Rotator());
    Hinge->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Hinge->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Hinge->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Hinge->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.0f);
    Hinge->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free, 0.0f);
    Hinge->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);
    Hinge->SetDisableCollision(true);

    LogMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    LogMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    LogMesh->SetSimulatePhysics(true);
    Hinge->SetConstrainedComponents(ParentBody, NAME_None, LogMesh, NAME_None);
    LogMesh->WakeAllRigidBodies();

    FSwingingLog& Swing = SwingingLogs.AddDefaulted_GetRef();
    Swing.Piece = Piece;
    Swing.Parent = Parent;
    Swing.Hinge = Hinge;
    Swing.OwnSnapIndex = OwnSnapIndex;
    return true;
}

void ULogCarryComponent::UpdateHingeSwings(float DeltaTime)
{
    for (int32 Index = SwingingLogs.Num() - 1; Index >= 0; --Index)
    {
        FSwingingLog& Swing = SwingingLogs[Index];
        ALogBuildPiece* Piece = Swing.Piece.Get();
        if (!IsValid(Piece) || !Swing.Parent.IsValid() || !Piece->GetLogMesh())
        {
            if (UPhysicsConstraintComponent* Hinge = Swing.Hinge.Get())
                Hinge->DestroyComponent();
            SwingingLogs.RemoveAt(Index);
            continue;
        }
        Swing.Elapsed += DeltaTime;
        const UStaticMeshComponent* LogMesh = Piece->GetLogMesh();
        const bool bStill = LogMesh->GetPhysicsLinearVelocity().Size() < 5.0f &&
            LogMesh->GetPhysicsAngularVelocityInDegrees().Size() < 5.0f;
        Swing.StillTime = (Swing.Elapsed > 0.3f && bStill) ? Swing.StillTime + DeltaTime : 0.0f;
        if (Swing.StillTime > 0.4f || Swing.Elapsed > 6.0f)
        {
            SettleHingeSwing(Swing);
            SwingingLogs.RemoveAt(Index);
        }
    }
}

void ULogCarryComponent::SettleHingeSwing(FSwingingLog& Swing)
{
    ALogBuildPiece* Piece = Swing.Piece.Get();
    AActor* Parent = Swing.Parent.Get();
    if (UPhysicsConstraintComponent* Hinge = Swing.Hinge.Get())
    {
        Hinge->BreakConstraint();
        Hinge->DestroyComponent();
    }
    UStaticMeshComponent* LogMesh = Piece->GetLogMesh();
    LogMesh->SetSimulatePhysics(false);
    LogMesh->SetCollisionProfileName(TEXT("BlockAll"));
    LogMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Piece->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);

    // Whatever the free end came to rest on now carries part of the load.
    AActor* Support = nullptr;
    bool bGround = false;
    EvaluateFarEndSupport(Piece, Swing.OwnSnapIndex, Support, bGround);
    const IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(Support);
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (SupportPiece && Graph && SupportPiece->IsStructurallyPlaced() && Support != Parent)
    {
        const FGuid NodeId = Piece->GetStructuralNodeId();
        Graph->TryAddSecondarySupport(NodeId, SupportPiece->GetStructuralNodeId(),
            FName(*FString::Printf(TEXT("Rest_%s"), *NodeId.ToString())), TEXT("FarEnd"));
    }
    else
    {
        RegisterDroopingLog(Piece, Swing.OwnSnapIndex);
    }
    PlacementStatus = TEXT("The joint held; the free end came to rest.");
}

void ULogCarryComponent::RegisterDroopingLog(ALogBuildPiece* Piece, int32 OwnSnapIndex)
{
    if (!IsValid(Piece) || !Piece->GetSnapPoints().IsValidIndex(OwnSnapIndex))
        return;
    DroopingLogs.RemoveAll([Piece](const FDroopingLog& Droop) { return Droop.Piece.Get() == Piece; });
    FDroopingLog& Droop = DroopingLogs.AddDefaulted_GetRef();
    Droop.Piece = Piece;
    Droop.OwnSnapIndex = OwnSnapIndex;
}

bool ULogCarryComponent::GetLogFreeEnd(const ALogBuildPiece* Piece, int32 OwnSnapIndex,
    FVector& OutFreeEnd, FVector& OutPivot, float& OutRadius) const
{
    const UStaticMeshComponent* Mesh = Piece ? Piece->GetLogMesh() : nullptr;
    if (!Mesh || !Mesh->GetStaticMesh() || !Piece->GetSnapPoints().IsValidIndex(OwnSnapIndex))
        return false;
    const FTransform MeshTransform = Mesh->GetComponentTransform();
    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const FVector AxisLocal = LocalAxisVector(Piece->LongitudinalAxis);
    const float HalfLength = FVector::DotProduct(Bounds.BoxExtent, AxisLocal.GetAbs());
    const FVector JointLocal = Piece->GetSnapPointLocalTransform(OwnSnapIndex).GetLocation();
    const float JointAlong = FVector::DotProduct(JointLocal - Bounds.Origin, AxisLocal);
    if (HalfLength <= KINDA_SMALL_NUMBER || FMath::Abs(JointAlong) < 0.3f * HalfLength)
        return false;
    OutFreeEnd = MeshTransform.TransformPosition(Bounds.Origin - AxisLocal * FMath::Sign(JointAlong) * HalfLength);
    OutPivot = (Piece->GetSnapPointLocalTransform(OwnSnapIndex) * Piece->GetActorTransform()).GetLocation();
    const FVector RadiusExtent = Bounds.BoxExtent * (FVector::OneVector - AxisLocal.GetAbs());
    OutRadius = (RadiusExtent * MeshTransform.GetScale3D().GetAbs()).GetMax();
    return true;
}

bool ULogCarryComponent::ComputeDroopLift(const ALogBuildPiece* Piece, int32 OwnSnapIndex,
    const AActor* Support, FQuat& OutRotation, FVector& OutPivot) const
{
    if (!IsValid(Piece) || !IsValid(Support) || Support == Piece)
        return false;
    FVector FreeEnd;
    float Radius = 0.0f;
    if (!GetLogFreeEnd(Piece, OwnSnapIndex, FreeEnd, OutPivot, Radius))
        return false;

    FVector Origin, Extent;
    Support->GetActorBounds(false, Origin, Extent);
    if (FMath::Abs(FreeEnd.X - Origin.X) > Extent.X + Radius ||
        FMath::Abs(FreeEnd.Y - Origin.Y) > Extent.Y + Radius)
        return false; // The free end is not over this piece.
    const float RestZ = Origin.Z + Extent.Z + Radius;
    if (FreeEnd.Z >= RestZ - 1.0f)
        return false; // Already resting at or above it.

    const FVector Current = FreeEnd - OutPivot;
    const float Length = Current.Size();
    const float Rise = RestZ - OutPivot.Z;
    if (Length <= 1.0f || FMath::Abs(Rise) >= Length)
        return false; // The joint is too low or too high to reach that top.

    FVector Horizontal(Current.X, Current.Y, 0.0f);
    if (Horizontal.SizeSquared() < 1.0f)
        Horizontal = FVector(Origin.X - OutPivot.X, Origin.Y - OutPivot.Y, 0.0f);
    if (!Horizontal.Normalize())
        return false;
    const FVector Desired = Horizontal * FMath::Sqrt(Length * Length - Rise * Rise) + FVector::UpVector * Rise;
    OutRotation = FQuat::FindBetweenVectors(Current, Desired);
    return true;
}

int32 ULogCarryComponent::RaiseBeamsOntoPost(ALogBuildPiece* Post)
{
    using namespace MyProject3Construction;
    FLogGeometry PostShape;
    if (!IsValid(Post) || !Post->IsPlaced() || !IsVerticalLog(Post) || !GetLogGeometry(Post, PostShape))
        return 0;
    const FVector Top = PostShape.EndA().Z > PostShape.EndB().Z ? PostShape.EndA() : PostShape.EndB();
    const float PostBottom = FMath::Min(PostShape.EndA().Z, PostShape.EndB().Z);
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    int32 Raised = 0;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        ALogBuildPiece* Beam = *It;
        FLogGeometry Shape;
        if (Beam == Post || !Beam->IsPlaced() || !GetLogGeometry(Beam, Shape) || FMath::Abs(Shape.Axis.Z) > 0.85f)
            continue;
        const int32 RaisedBefore = Raised;
        for (const bool bEndA : { true, false })
        {
            const FVector End = bEndA ? Shape.EndA() : Shape.EndB();
            const FVector Other = bEndA ? Shape.EndB() : Shape.EndA();
            const float Bottom = End.Z - Shape.Radius;
            // The new post stands under this end and reaches higher than it: the end rises onto it.
            if (FVector::Dist2D(End, Top) > PostShape.Radius + Shape.Radius + 10.0f ||
                Bottom >= Top.Z - 2.0f || Bottom < Top.Z - 150.0f || PostBottom > Bottom - 5.0f)
                continue;
            // An end already resting on another piece stays where it is.
            bool bRests = false;
            for (TActorIterator<ALogBuildPiece> Under(GetWorld()); Under && !bRests; ++Under)
            {
                FLogGeometry Below;
                if (*Under == Beam || *Under == Post || !Under->IsPlaced() || !GetLogGeometry(*Under, Below))
                    continue;
                const FVector Closest = ClosestPointOnSegment(End, Below.EndA(), Below.EndB());
                bRests = Closest.Z < End.Z && FVector::Dist(Closest, End) < Below.Radius + Shape.Radius + 8.0f;
            }
            if (bRests)
            {
                // The end already rests on something: the new post is cut to meet it and props it up.
                const float Keep = Bottom - PostBottom;
                const bool bPostALow = PostShape.EndA().Z <= PostShape.EndB().Z;
                if (Keep > 25.0f && TrimLogToRange(Post, bPostALow ? -PostShape.HalfLength : PostShape.HalfLength - Keep,
                    bPostALow ? -PostShape.HalfLength + Keep : PostShape.HalfLength))
                {
                    if (Graph)
                        Graph->TryAddSecondarySupport(Beam->GetStructuralNodeId(), Post->GetStructuralNodeId(),
                            FName(*FString::Printf(TEXT("Prop_%s"), *Beam->GetStructuralNodeId().ToString())), TEXT("PropEnd"));
                    PlacementStatus = FString::Printf(TEXT("Post cut to %.0f cm to fit under the beam."), Keep);
                    ++Raised;
                }
                break;
            }
            const FVector Arm = End - Other;
            const float Lift = Top.Z - Bottom;
            const float Angle = FMath::Asin(FMath::Clamp(Lift / FMath::Max(Arm.Size(), 1.0f), 0.0f, 0.9f));
            const FVector Hinge = FVector::CrossProduct(Arm, FVector::UpVector).GetSafeNormal();
            if (Hinge.IsNearlyZero())
                continue;
            FQuat Turn(Hinge, Angle);
            if (Turn.RotateVector(Arm).Z < Arm.Z)
                Turn = FQuat(Hinge, -Angle);
            FTransform Moved = Beam->GetActorTransform();
            Moved.SetRotation(Turn * Moved.GetRotation());
            Moved.SetLocation(Other + Turn.RotateVector(Moved.GetLocation() - Other));
            Beam->SetActorTransform(Moved, false, nullptr, ETeleportType::TeleportPhysics);
            if (Graph)
            {
                Graph->TryAddSecondarySupport(Beam->GetStructuralNodeId(), Post->GetStructuralNodeId(),
                    FName(*FString::Printf(TEXT("Raised_%s"), *Beam->GetStructuralNodeId().ToString())), TEXT("RaisedEnd"));
            }
            ++Raised;
            break;
        }
        if (Raised > RaisedBefore)
            continue;
        // Under the middle of the beam: the post is cut to meet the beam and props it up.
        const FVector OnBeam = ClosestPointOnSegment(Top, Shape.EndA(), Shape.EndB());
        const float BeamBottom = OnBeam.Z - Shape.Radius;
        if (FVector::Dist2D(OnBeam, Top) < PostShape.Radius + Shape.Radius && BeamBottom < Top.Z + 15.0f &&
            BeamBottom > Top.Z - 150.0f && PostBottom < BeamBottom - 25.0f)
        {
            const float Keep = BeamBottom - PostBottom;
            const bool bPostALow = PostShape.EndA().Z <= PostShape.EndB().Z;
            if (Keep < PostShape.Length() - 1.0f)
                TrimLogToRange(Post, bPostALow ? -PostShape.HalfLength : PostShape.HalfLength - Keep,
                    bPostALow ? -PostShape.HalfLength + Keep : PostShape.HalfLength);
            if (Graph)
                Graph->TryAddSecondarySupport(Beam->GetStructuralNodeId(), Post->GetStructuralNodeId(),
                    FName(*FString::Printf(TEXT("Prop_%s"), *Beam->GetStructuralNodeId().ToString())), TEXT("PropMid"));
            PlacementStatus = FString::Printf(TEXT("Post fitted under the beam (%.0f cm)."), Keep);
            ++Raised;
        }
    }
    return Raised;
}

bool ULogCarryComponent::LiftDroopingEndsOnto(AActor* NewSupport)
{
    // A new post under the end of a placed beam lifts that end onto its top.
    RaiseBeamsOntoPost(Cast<ALogBuildPiece>(NewSupport));
    bool bLifted = false;
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    for (int32 Index = DroopingLogs.Num() - 1; Index >= 0; --Index)
    {
        ALogBuildPiece* Piece = DroopingLogs[Index].Piece.Get();
        if (!IsValid(Piece) || !Piece->IsPlaced())
        {
            DroopingLogs.RemoveAt(Index);
            continue;
        }
        FQuat Rotation;
        FVector Pivot;
        if (!ComputeDroopLift(Piece, DroopingLogs[Index].OwnSnapIndex, NewSupport, Rotation, Pivot))
            continue;

        // Turn the log about its joint; the joint itself stays where it is.
        Piece->SetActorLocationAndRotation(Pivot + Rotation.RotateVector(Piece->GetActorLocation() - Pivot),
            Rotation * Piece->GetActorQuat(), false, nullptr, ETeleportType::TeleportPhysics);

        const IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(NewSupport);
        if (Graph && SupportPiece && SupportPiece->IsStructurallyPlaced())
        {
            const FGuid NodeId = Piece->GetStructuralNodeId();
            Graph->TryAddSecondarySupport(NodeId, SupportPiece->GetStructuralNodeId(),
                FName(*FString::Printf(TEXT("Rest_%s"), *NodeId.ToString())), TEXT("FarEnd"));
        }
        DroopingLogs.RemoveAt(Index);
        bLifted = true;
    }
    if (bLifted)
        PlacementStatus = TEXT("The fallen end was lifted onto the new piece.");
    return bLifted;
}

void ULogCarryComponent::RestoreHeldLifts()
{
    for (const FHeldLift& Lift : HeldLifts)
    {
        if (ALogBuildPiece* Piece = Lift.Piece.Get())
            Piece->SetActorTransform(Lift.Original, false, nullptr, ETeleportType::TeleportPhysics);
    }
    HeldLifts.Reset();
}

void ULogCarryComponent::UpdateHeldLifts()
{
    // Measure from the resting pose every time, so moving away lets the end down.
    RestoreHeldLifts();
    if (!IsValid(HeldPiece))
        return;
    for (const FDroopingLog& Droop : DroopingLogs)
    {
        ALogBuildPiece* Piece = Droop.Piece.Get();
        FQuat Rotation;
        FVector Pivot;
        if (!IsValid(Piece) || Piece == HeldPiece ||
            !ComputeDroopLift(Piece, Droop.OwnSnapIndex, HeldPiece, Rotation, Pivot))
            continue;
        FHeldLift& Lift = HeldLifts.AddDefaulted_GetRef();
        Lift.Piece = Piece;
        Lift.Original = Piece->GetActorTransform();
        Piece->SetActorLocationAndRotation(Pivot + Rotation.RotateVector(Piece->GetActorLocation() - Pivot),
            Rotation * Piece->GetActorQuat(), false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void ULogCarryComponent::CommitHeldLifts(AActor* NewSupport)
{
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    const IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(NewSupport);
    for (const FHeldLift& Lift : HeldLifts)
    {
        ALogBuildPiece* Piece = Lift.Piece.Get();
        if (!Piece)
            continue;
        if (Graph && SupportPiece && SupportPiece->IsStructurallyPlaced())
        {
            const FGuid NodeId = Piece->GetStructuralNodeId();
            Graph->TryAddSecondarySupport(NodeId, SupportPiece->GetStructuralNodeId(),
                FName(*FString::Printf(TEXT("Rest_%s"), *NodeId.ToString())), TEXT("FarEnd"));
        }
        DroopingLogs.RemoveAll([Piece](const FDroopingLog& Droop) { return Droop.Piece.Get() == Piece; });
    }
    if (HeldLifts.Num() > 0)
        PlacementStatus = TEXT("Placed; the fallen end now rests on it.");
    HeldLifts.Reset();
}

bool ULogCarryComponent::CommitHeldLogAt(const FTransform& WorldTransform, AActor* PrimarySupport, AActor* SecondarySupport,
    AActor* RestingOnPiece)
{
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!IsValid(HeldPiece) || !Kernel || WorldTransform.ContainsNaN())
        return false;
    const FPlacementSolution Solution = Kernel->MakeDirectSolution(HeldPiece, WorldTransform, TEXT("piece.direct"),
        PrimarySupport, SecondarySupport, RestingOnPiece);
    if (!Solution.bValid)
    {
        PlacementStatus = Solution.Reason;
        return false;
    }
    return CommitHeldSolution(Solution);
}

bool ULogCarryComponent::CommitHeldSolution(const FPlacementSolution& Solution)
{
    ALogBuildPiece* Piece = HeldPiece;
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!IsValid(Piece) || !Kernel)
        return false;
    // Set first: what reacts to the placement (lifted ends, braces) adds to it.
    PlacementStatus = TEXT("Placed.");
    FString Reason;
    if (!Kernel->CommitPlacement(Piece, Solution, FPlacementCommitOptions(), Reason))
    {
        PlacementStatus = Reason;
        return false;
    }
    ReleaseHeldFromLoad();
    HeldPiece = nullptr;
    bHasPlacementSurface = false;
    bHasStackPlacement = false;
    bHasStrutPlacement = false;
    StackSolution = FPlacementSolution();
    StrutSolution = FPlacementSolution();
    ClearSnapCandidate();
    CommitHeldLifts(Piece);
    return true;
}

AActor* ULogCarryComponent::ReleaseHeldToTool()
{
    UCarryLoadComponent* Load = GetOwner() ? GetOwner()->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    if (IsValid(HeldPiece))
    {
        AActor* Piece = HeldPiece;
        ReleaseHeldFromLoad();
        RestoreHeldLifts();
        HeldPiece = nullptr;
        bHasPlacementSurface = false;
        ClearSnapCandidate();
        return Piece;
    }
    if (IsValid(HeldMaterialPiece))
    {
        AActor* Piece = HeldMaterialPiece;
        if (Load)
            Load->UnregisterCarried(Piece);
        HeldMaterialPiece = nullptr;
        MaterialSnapTarget = nullptr;
        MaterialSnapAnchor = NAME_None;
        return Piece;
    }
    return nullptr;
}

bool ULogCarryComponent::ThrowHeldPiece()
{
    AActor* Piece = IsValid(HeldPiece) ? static_cast<AActor*>(HeldPiece) : HeldMaterialPiece.Get();
    FVector ViewLocation;
    FRotator ViewRotation;
    if (!Piece || !GetViewPoint(ViewLocation, ViewRotation))
        return false;
    DropHeldPiece();
    if (IsValid(HeldPiece) || IsValid(HeldMaterialPiece))
        return false; // The drop was refused.
    UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent());
    if (!Root)
        return false;
    const float Mass = MyProject3Construction::EstimateMassKg(Piece);
    const float Speed = FMath::Clamp(ThrowSpeed * FMath::Sqrt(20.0f / FMath::Max(Mass, 1.0f)), 300.0f, 2200.0f);
    Root->SetSimulatePhysics(true);
    Root->SetPhysicsLinearVelocity(ViewRotation.Vector() * Speed + FVector::UpVector * 120.0f);
    Root->SetPhysicsAngularVelocityInDegrees(ViewRotation.RotateVector(FVector(0.0f, 180.0f, 0.0f)));
    UThrownImpactComponent* Impact = NewObject<UThrownImpactComponent>(Piece);
    Impact->RegisterComponent();
    Impact->Arm(GetOwner(), Mass);
    PlacementStatus = TEXT("Thrown.");
    return true;
}

void ULogCarryComponent::ApplyEndContactSupports(ALogBuildPiece* Piece)
{
    using namespace MyProject3Construction;
    FLogGeometry Shape;
    UWorldStructuralGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!Graph || !GetLogGeometry(Piece, Shape))
        return;
    const FGuid NodeId = Piece->GetStructuralNodeId();
    FStructuralNodeRecord Node;
    if (!Graph->GetNode(NodeId, Node))
        return;

    const FVector Ends[2] = { Shape.EndA(), Shape.EndB() };
    TSet<AActor*> Seen;
    for (const FVector& End : Ends)
    {
        TArray<FOverlapResult> Overlaps;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LogEndContact), false, Piece);
        if (GetOwner())
            Params.AddIgnoredActor(GetOwner());
        GetWorld()->OverlapMultiByObjectType(Overlaps, End, FQuat::Identity,
            FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
            FCollisionShape::MakeSphere(Shape.Radius + 12.0f), Params);
        for (const FOverlapResult& Overlap : Overlaps)
        {
            AActor* Other = Overlap.GetActor();
            if (!Other || Seen.Contains(Other))
                continue;
            Seen.Add(Other);
            if (AHarvestableTreeActor* Tree = Cast<AHarvestableTreeActor>(Other))
            {
                // A standing trunk holds the log like the ground does, until it is felled.
                if (Tree->CanBeChopped() && !Piece->FindComponentByClass<UTreeAnchorComponent>())
                {
                    UTreeAnchorComponent* Anchor = NewObject<UTreeAnchorComponent>(Piece);
                    Anchor->SetTree(Tree);
                    Anchor->RegisterComponent();
                    if (!Node.ParentNodeId.IsValid())
                        Graph->SetGroundSupported(NodeId, true);
                    PlacementStatus += TEXT("  Resting on the tree.");
                }
                continue;
            }
            const IStructuralPieceInterface* OtherPiece = Cast<IStructuralPieceInterface>(Other);
            if (!OtherPiece || !OtherPiece->IsStructurallyPlaced())
                continue;
            const FGuid OtherId = OtherPiece->GetStructuralNodeId();
            if (OtherId == Node.ParentNodeId)
                continue;
            FVector OtherOrigin, OtherExtent;
            Other->GetActorBounds(true, OtherOrigin, OtherExtent);
            const FName Link(*FString::Printf(TEXT("Contact_%s"), *NodeId.ToString()));
            if (OtherOrigin.Z > End.Z + 5.0f)
            {
                // This end props up the piece above it (a brace or strut).
                if (Graph->TryAddSecondarySupport(OtherId, NodeId, Link, TEXT("BraceTop")))
                    PlacementStatus += TEXT("  Bracing the piece above.");
            }
            else if (Node.ParentNodeId.IsValid())
            {
                Graph->TryAddSecondarySupport(NodeId, OtherId, Link, TEXT("EndRest"));
            }
        }
    }
}

float ULogCarryComponent::GetStickTiltDegrees() const
{
    switch (PlacementMode)
    {
    case ELogPlacementMode::Diagonal: return 45.0f;
    case ELogPlacementMode::Horizontal: return 90.0f;
    default: return 0.0f;
    }
}

FQuat ULogCarryComponent::MakeTiltAroundView(float Degrees) const
{
    FVector ViewLocation;
    FRotator ViewRotation;
    const float Yaw = GetViewPoint(ViewLocation, ViewRotation) ? ViewRotation.Yaw : 0.0f;
    const FVector Right = FRotator(0.0f, Yaw + HeldYawOffset + 90.0f, 0.0f).Vector();
    return FQuat(Right, FMath::DegreesToRadians(Degrees));
}

FPlacementSolution ULogCarryComponent::SolveStackOn(ALogBuildPiece* Held, ALogBuildPiece* Target, const FVector& AimPoint,
    float CarryYaw) const
{
    UPlacementKernelSubsystem* Kernel = GetKernel();
    if (!Kernel || !Held)
        return FPlacementSolution();
    FPlacementIntent Intent;
    Intent.Held = Held;
    Intent.AimActor = Target;
    Intent.AimPoint = AimPoint;
    Intent.CarryYawDeg = CarryYaw;
    Intent.bUpright = PlacementMode == ELogPlacementMode::Vertical;
    Intent.AllowedProfiles = { TEXT("log.parallel_stack") };
    return Kernel->Solve(Intent);
}

void ULogCarryComponent::UpdateStackPlacement()
{
    bHasStackPlacement = false;
    StackTarget.Reset();
    StackSolution = FPlacementSolution();
    if (!IsValid(HeldPiece) ||
        (PlacementMode != ELogPlacementMode::Horizontal && PlacementMode != ELogPlacementMode::Vertical))
        return;
    FHitResult Hit;
    if (!TraceFromView(PlacementTraceDistance, Hit))
        return;
    ALogBuildPiece* Target = Cast<ALogBuildPiece>(Hit.GetActor());
    if (!IsValid(Target) || Target == HeldPiece || !Target->IsPlaced())
        return;
    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    const FPlacementSolution Solution = SolveStackOn(HeldPiece, Target, Hit.ImpactPoint, ViewRotation.Yaw + HeldYawOffset);
    if (!Solution.bValid)
    {
        if (!Solution.Reason.IsEmpty())
            PlacementStatus = Solution.Reason;
        return;
    }
    bHasStackPlacement = true;
    StackTarget = Target;
    StackSolution = Solution;
    StackTransform = Solution.PieceTransform;
    ClearSnapCandidate();
    HeldPiece->SetActorTransform(StackTransform);
}

bool ULogCarryComponent::ComputeStackPlacement(ALogBuildPiece* Held, ALogBuildPiece* Target, const FVector& AimPoint,
    float CarryYaw, FTransform& OutTransform) const
{
    const FPlacementSolution Solution = SolveStackOn(Held, Target, AimPoint, CarryYaw);
    if (!Solution.bValid)
        return false;
    OutTransform = Solution.PieceTransform;
    return true;
}
