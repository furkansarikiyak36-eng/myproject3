#include "LogCarryComponent.h"

#include "CarryLoadComponent.h"

#include "LogBuildPiece.h"
#include "ConstructionHelpers.h"
#include "HarvestableTreeActor.h"
#include "LogAttachmentComponents.h"
#include "LogBuildToolsComponent.h"
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
}

void ULogCarryComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateHingeSwings(DeltaTime);

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
            AActor* Support = nullptr;
            bool bGround = false;
            PlacementStatus = EvaluateFarEndSupport(HeldPiece, CurrentSourceSnapPointIndex, Support, bGround)
                ? TEXT("Snap ready: E to place.")
                : TEXT("The other end has no support: it will swing down around the joint.");
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
    if (!IsValid(AimedTarget) || !AimedTarget->IsPlaced() || AimedTarget->IsHeld())
    {
        ClearSnapCandidate();
        return;
    }

    ALogBuildPiece* BestTarget = nullptr;
    int32 BestSourceIndex = INDEX_NONE;
    int32 BestTargetIndex = INDEX_NONE;
    float BestDistance = SnapSearchRadius;
    FTransform BestActorTransform = HeldPiece->GetActorTransform();

    const TArray<FLogSnapPoint>& SourcePoints = HeldPiece->GetSnapPoints();
    const FTransform HeldActorTransform = HeldPiece->GetActorTransform();

    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        ALogBuildPiece* TargetPiece = *It;
        if (!IsValid(TargetPiece) || TargetPiece != AimedTarget ||
            TargetPiece == HeldPiece || !TargetPiece->IsPlaced() || TargetPiece->IsHeld())
        {
            continue;
        }

        const TArray<FLogSnapPoint>& TargetPoints = TargetPiece->GetSnapPoints();

        for (int32 SourceIndex = 0; SourceIndex < SourcePoints.Num(); ++SourceIndex)
        {
            const FLogSnapPoint& SourcePoint = SourcePoints[SourceIndex];
            if (!SourcePoint.bCanSource || SourcePoint.bOccupied)
            {
                continue;
            }

            const FTransform SourceWorldTransform =
                SourcePoint.LocalTransform * HeldActorTransform;

            for (int32 TargetIndex = 0; TargetIndex < TargetPoints.Num(); ++TargetIndex)
            {
                if (!TargetPiece->CanSnapPointAccept(
                    TargetIndex, HeldPiece, SourceIndex))
                {
                    continue;
                }

                const FLogSnapPoint& TargetPoint = TargetPoints[TargetIndex];
                const FTransform TargetWorldTransform =
                    TargetPiece->GetSnapPointWorldTransform(TargetIndex);

                const float AnchorDistance = FVector::Distance(
                    SourceWorldTransform.GetLocation(),
                    TargetWorldTransform.GetLocation());

                // The carried actor's free-preview origin may be offset from
                // the intended socket by half a log length. Aiming directly
                // at a target's anchor should still acquire it, then the exact
                // source-to-target transform is computed below.
                const float AimToAnchorDistance = FVector::Distance(
                    AimHit.ImpactPoint,
                    TargetWorldTransform.GetLocation());

                const float CandidateDistance =
                    FMath::Min(AnchorDistance, AimToAnchorDistance);

                if (CandidateDistance > BestDistance)
                {
                    continue;
                }

                FTransform DesiredAnchorWorld = TargetWorldTransform;
                if (TargetPoint.bFlipIncomingOrientation)
                {
                    const FTransform FlipIncoming(
                        FRotator(0.0f, 180.0f, 0.0f),
                        FVector::ZeroVector,
                        FVector::OneVector);
                    DesiredAnchorWorld = FlipIncoming * TargetWorldTransform;
                }

                const FTransform DesiredActorWorld =
                    SourcePoint.LocalTransform.Inverse() * DesiredAnchorWorld;

                BestDistance = CandidateDistance;
                BestTarget = TargetPiece;
                BestSourceIndex = SourceIndex;
                BestTargetIndex = TargetIndex;
                BestActorTransform = DesiredActorWorld;
            }
        }
    }

    if (IsValid(BestTarget) &&
        BestSourceIndex != INDEX_NONE &&
        BestTargetIndex != INDEX_NONE)
    {
        SetSnapCandidate(
            BestTarget,
            BestSourceIndex,
            BestTargetIndex,
            BestActorTransform);
    }
    else
    {
        ClearSnapCandidate();
    }
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

    if (bWasSnapping)
    {
        OnSnapStateChanged.Broadcast(false);
    }
}

bool ULogCarryComponent::PlaceHeldPiece()
{
    if (bHasStackPlacement && StackTarget.IsValid() && IsValid(HeldPiece))
    {
        ALogBuildPiece* Target = StackTarget.Get();
        bHasStackPlacement = false;
        StackTarget.Reset();
        return CommitHeldLogAt(StackTransform, Target, nullptr);
    }
    if (!IsValid(HeldPiece))
    {
        return false;
    }

    ALogBuildPiece* PieceToPlace = HeldPiece;

    if (bHasValidSnap &&
        IsValid(CurrentSnapTarget) &&
        CurrentSourceSnapPointIndex != INDEX_NONE &&
        CurrentTargetSnapPointIndex != INDEX_NONE)
    {
        const FTransform PreviousTransform = PieceToPlace->GetActorTransform();
        PieceToPlace->SetActorTransform(CurrentSnapActorTransform);

        if (CurrentSnapTarget->TryReserveSnapPoints(
            CurrentTargetSnapPointIndex,
            PieceToPlace,
            CurrentSourceSnapPointIndex))
        {
            if (UWorldObjectIdentityComponent* Identity =
                PieceToPlace->FindComponentByClass<UWorldObjectIdentityComponent>())
            {
                if (!Identity->TryTransitionLifecycle(
                    EWorldObjectLifecycleState::Held,
                    EWorldObjectLifecycleState::Placed))
                {
                    PieceToPlace->DetachFromSnapParent();
                    PieceToPlace->SetActorTransform(PreviousTransform);
                    return false;
                }
            }

            FWorldOwnedItemRecord WorldRecord;
            if (!PieceToPlace->TryTransferOwnership(EWorldItemOwnerKind::Held,
                EWorldItemOwnerKind::World, FGuid(), PieceToPlace->GetActorLocation(), WorldRecord))
            {
                if (UWorldObjectIdentityComponent* Identity =
                    PieceToPlace->FindComponentByClass<UWorldObjectIdentityComponent>())
                {
                    Identity->TryTransitionLifecycle(
                        EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::Held);
                }
                PieceToPlace->DetachFromSnapParent();
                PieceToPlace->SetActorTransform(PreviousTransform);
                return false;
            }

            PieceToPlace->SetHeldState(false);
            PieceToPlace->SetPlacedState(true);

            const int32 OwnSnapIndex = CurrentSourceSnapPointIndex;
            ReleaseHeldFromLoad();
            HeldPiece = nullptr;
            bHasPlacementSurface = false;
            ClearSnapCandidate();

            // A horizontal log joined at one end must rest on something at the
            // other end; otherwise it tips off its joint and falls.
            AActor* Support = nullptr;
            bool bGround = false;
            if (!EvaluateFarEndSupport(PieceToPlace, OwnSnapIndex, Support, bGround))
            {
                // Like a board nailed at one end: the joint holds, the free end swings down.
                if (StartHingeSwing(PieceToPlace, OwnSnapIndex))
                {
                    PlacementStatus = TEXT("One end is fixed; the free end swings down.");
                    return true;
                }
                if (UWorldObjectIdentityComponent* Identity =
                    PieceToPlace->FindComponentByClass<UWorldObjectIdentityComponent>())
                {
                    Identity->TryTransitionLifecycle(
                        EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::World);
                }
                PieceToPlace->DetachFromSnapParent(true);
                PieceToPlace->SetPlacedState(false);
                PlacementStatus = TEXT("The other end had no support: the log fell.");
                return false;
            }
            const IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(Support);
            UWorldStructuralGraphSubsystem* Graph =
                GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
            if (SupportPiece && Graph && SupportPiece->IsStructurallyPlaced())
            {
                const FGuid NodeId = PieceToPlace->GetStructuralNodeId();
                Graph->TryAddSecondarySupport(NodeId, SupportPiece->GetStructuralNodeId(),
                    FName(*FString::Printf(TEXT("Rest_%s"), *NodeId.ToString())), TEXT("FarEnd"));
            }
            PlacementStatus = TEXT("Placed.");
            ApplyEndContactSupports(PieceToPlace);
            CommitHeldLifts(PieceToPlace);
            LiftDroopingEndsOnto(PieceToPlace);
            return true;
        }

        // A snap candidate was explicitly attempted but its anchors failed
        // revalidation. Do not reinterpret that failed structural operation as
        // free placement, and do not leave the held piece at the stale snap pose.
        PieceToPlace->SetActorTransform(PreviousTransform);
        ClearSnapCandidate();
        return false;
    }

    if (!bAllowFreePlacement ||
        (bRequireSurfaceForFreePlacement && !bHasPlacementSurface))
    {
        return false;
    }

    // A free-placed log becomes a stable building piece and can receive
    // subsequent logs at its available snap points.
    if (UWorldObjectIdentityComponent* Identity =
        PieceToPlace->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (!Identity->TryTransitionLifecycle(
            EWorldObjectLifecycleState::Held,
            EWorldObjectLifecycleState::Placed))
        {
            return false;
        }
    }
    FWorldOwnedItemRecord WorldRecord;
    if (!PieceToPlace->TryTransferOwnership(EWorldItemOwnerKind::Held,
        EWorldItemOwnerKind::World, FGuid(), PieceToPlace->GetActorLocation(), WorldRecord))
    {
        if (UWorldObjectIdentityComponent* Identity =
            PieceToPlace->FindComponentByClass<UWorldObjectIdentityComponent>())
        {
            Identity->TryTransitionLifecycle(
                EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::Held);
        }
        return false;
    }
    PieceToPlace->SetHeldState(false);
    PieceToPlace->SetPlacedState(true);
    if (const IStructuralPieceInterface* Surface = Cast<IStructuralPieceInterface>(PlacementSurfaceActor.Get()))
    {
        UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
        const FGuid NodeId = PieceToPlace->GetStructuralNodeId();
        if (Graph && Surface->IsStructurallyPlaced() && Graph->TryConnect(Surface->GetStructuralNodeId(),
            FName(*FString::Printf(TEXT("Rest_%s"), *NodeId.ToString())), NodeId, TEXT("Base")))
        {
            PieceToPlace->AttachToActor(PlacementSurfaceActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
        }
    }

    ReleaseHeldFromLoad();
    HeldPiece = nullptr;
    bHasPlacementSurface = false;
    ClearSnapCandidate();
    PlacementStatus = TEXT("Placed.");
    ApplyEndContactSupports(PieceToPlace);
    CommitHeldLifts(PieceToPlace);
    LiftDroopingEndsOnto(PieceToPlace);
    return true;
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
    case ELogCutAction::SplitLengthHalf:
        SelectedCutAction = ELogCutAction::SplitLengthThreeQuarter;
        break;
    case ELogCutAction::SplitLengthThreeQuarter:
        SelectedCutAction = ELogCutAction::SplitHalfLengthIntoQuarters;
        break;
    case ELogCutAction::SplitHalfLengthIntoQuarters:
        SelectedCutAction = ELogCutAction::SplitQuarterLengthIntoEighths;
        break;
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

    // Remember what already lies here, so the new pieces can be found after the cut.
    TSet<ALogBuildPiece*> Before;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
        Before.Add(*It);

    if (Piece->ApplyCutAction(SelectedCutAction))
    {
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
            ? Enum->GetDisplayNameTextByValue(static_cast<int64>(SelectedCutAction)).ToString()
            : TEXT("Processing");
        const FString Message = FString::Printf(TEXT("Log processed: %s"), *ActionName);
        UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, Message);
        }
        return true;
    }

    const FString Reason = Piece->GetCutActionBlockReason(SelectedCutAction);
    const FString Message = Reason.IsEmpty()
        ? TEXT("This cut operation is not available on the aimed log.")
        : Reason;
    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
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
    UWorldObjectIdentityComponent* Identity =
        Piece ? Piece->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
    if (!Identity || !Identity->HasValidIdentity() ||
        Identity->GetLifecycleState() != EWorldObjectLifecycleState::World ||
        Identity->StateRevision == MAX_int64)
    {
        PlacementStatus = TEXT("The piece has an invalid or non-world lifecycle state.");
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
    if (!Identity->TryTransitionLifecycle(
        EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Held))
    {
        TransferMaterialOwnership(Piece, EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, Piece->GetActorLocation());
        PlacementStatus = TEXT("The piece lifecycle rejected pickup.");
        return false;
    }
    if (!SetMaterialHeld(Piece, true))
    {
        Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World);
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

void ULogCarryComponent::UpdateHeldMaterialPreview()
{
    AActor* Piece = HeldMaterialPiece;
    MaterialSnapTarget = nullptr;
    MaterialSnapAnchor = NAME_None;
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
    const IStructuralPieceInterface* TargetPiece = Cast<IStructuralPieceInterface>(Target);
    if (TargetPiece && TargetPiece->IsStructurallyPlaced())
    {
        // Choose the free, upward-facing receiving anchor nearest to the crosshair.
        EnsureMaterialAnchors(Target);
        TArray<FName> AnchorIds;
        TargetPiece->GetStructuralAnchorIds(AnchorIds);
        FVector Origin, Extent;
        Target->GetActorBounds(true, Origin, Extent);
        const bool bSameKind = Cast<IStructuralPieceInterface>(Piece)->GetStructuralKind() == TargetPiece->GetStructuralKind();
        UMaterialConnectionSubsystem* Connections = GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>();

        float BestDistance = 120.0f;
        for (const FName& AnchorId : AnchorIds)
        {
            FStructuralAnchorView Anchor;
            if (!TargetPiece->GetStructuralAnchor(AnchorId, Anchor) || !Anchor.bCanReceive || Anchor.bOccupied)
                continue;
            const FVector AnchorWorld = (Anchor.LocalTransform * Target->GetActorTransform()).GetLocation();
            if (AnchorWorld.Z < Origin.Z - 1.0f)
                continue; // Bottom anchors cannot hold a piece resting on them.
            FString Reason;
            if (!bSameKind && (!Connections || !Connections->CanConnect(Target, AnchorId, Piece, TEXT("AutoBottom"), Reason)))
                continue;
            const float Distance = FVector::Dist(AnchorWorld, Hit.ImpactPoint);
            if (Distance >= BestDistance)
                continue;
            BestDistance = Distance;
            MaterialSnapTarget = Target;
            MaterialSnapAnchor = AnchorId;
            MaterialPreviewTransform = MakeUprightTransform(Piece, AnchorWorld);
            if (bSameKind && Cast<AStickPrimitivePiece>(Piece) && GetStickTiltDegrees() > 0.0f)
            {
                // Stick on stick: the preview leans like the placed stick will.
                MaterialPreviewTransform = MakeUprightTransform(Piece, AnchorWorld);
            }
            else if (bSameKind)
            {
                // Same-material stacks keep the lower piece's orientation.
                FStructuralAnchorView Bottom;
                Cast<IStructuralPieceInterface>(Piece)->GetStructuralAnchor(TEXT("AutoBottom"), Bottom);
                const FQuat Rotation = Target->GetActorQuat();
                const FVector Scale = Piece->GetActorScale3D();
                MaterialPreviewTransform = FTransform(Rotation,
                    AnchorWorld - Rotation.RotateVector(Bottom.LocalTransform.GetLocation() * Scale), Scale);
            }
        }
        if (MaterialSnapTarget)
        {
            Piece->SetActorTransform(MaterialPreviewTransform);
            PlacementStatus = FString::Printf(TEXT("Snap to %s: E to place."), *MaterialSnapAnchor.ToString());
            return;
        }
        Piece->SetActorLocation(ViewLocation + ViewRotation.Vector() * CarryDistance);
        PlacementStatus = TEXT("No free top anchor here for this material.");
        return;
    }

    if (TargetPiece)
    {
        Piece->SetActorLocation(ViewLocation + ViewRotation.Vector() * CarryDistance);
        PlacementStatus = TEXT("That piece is loose; place on the ground or a placed piece.");
        return;
    }

    bMaterialGroundPreview = true;
    MaterialPreviewTransform = MakeUprightTransform(Piece, Hit.ImpactPoint);
    Piece->SetActorTransform(MaterialPreviewTransform);
    PlacementStatus = TEXT("E to place on the ground.");
}

void ULogCarryComponent::FinishMaterialPlacement(AActor* Piece)
{
    // Ownership must already be committed by the placement caller. Never silently
    // discard the held reference when the ownership ledger rejects a transfer.
    if (!IsValid(Piece) || HeldMaterialPiece != Piece)
        return;

    // Anchor placement may move the actor after the pre-committed Held -> World
    // transfer. Refresh the authoritative world location using a new revision.
    if (!TransferMaterialOwnership(Piece, EWorldItemOwnerKind::World,
        EWorldItemOwnerKind::World, Piece->GetActorLocation()))
    {
        UE_LOG(LogTemp, Error, TEXT("Material placement committed but ledger location sync failed for %s."),
            *GetNameSafe(Piece));
    }

    Piece->SetActorEnableCollision(true);
    if (UCarryLoadComponent* Load = GetOwner() ? GetOwner()->FindComponentByClass<UCarryLoadComponent>() : nullptr)
        Load->UnregisterCarried(Piece);
    HeldMaterialPiece = nullptr;
    MaterialSnapTarget = nullptr;
    MaterialSnapAnchor = NAME_None;
}

bool ULogCarryComponent::TryPlaceHeldMaterialOn(AActor* Target, FName TargetAnchorId)
{
    AActor* Piece = HeldMaterialPiece;
    const IStructuralPieceInterface* TargetPiece = Cast<IStructuralPieceInterface>(Target);
    const IStructuralPieceInterface* HeldStructural = Cast<IStructuralPieceInterface>(Piece);
    if (!IsValid(Piece) || !TargetPiece || !HeldStructural || Target == Piece)
        return false;

    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    if (!Identity || Identity->GetLifecycleState() != EWorldObjectLifecycleState::Held ||
        Identity->StateRevision == MAX_int64)
    {
        PlacementStatus = TEXT("Placement blocked: the piece lifecycle is not Held.");
        return false;
    }
    // The actor and ownership ledger transition together with the canonical lifecycle.
    if (!TransferMaterialOwnership(Piece, EWorldItemOwnerKind::Held,
        EWorldItemOwnerKind::World, Piece->GetActorLocation()))
    {
        PlacementStatus = TEXT("Placement blocked: ownership ledger rejected the transfer.");
        return false;
    }
    if (!Identity->TryTransitionLifecycle(
        EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::Placed))
    {
        TransferMaterialOwnership(Piece, EWorldItemOwnerKind::World,
            EWorldItemOwnerKind::Held, Piece->GetActorLocation());
        PlacementStatus = TEXT("Placement blocked: lifecycle transition was rejected.");
        return false;
    }

    EnsureMaterialAnchors(Target);

    bool bConnected = false;
    FString Reason;
    if (HeldStructural->GetStructuralKind() != TargetPiece->GetStructuralKind())
    {
        FStructuralAnchorView Anchor;
        UMaterialConnectionSubsystem* Connections = GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>();
        if (Connections && TargetPiece->GetStructuralAnchor(TargetAnchorId, Anchor))
        {
            const FVector AnchorWorld = (Anchor.LocalTransform * Target->GetActorTransform()).GetLocation();
            bConnected = Connections->TryConnectAt(Target, TargetAnchorId, Piece, TEXT("AutoBottom"),
                MakeUprightTransform(Piece, AnchorWorld), Reason);
        }
    }
    else if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
    {
        bConnected = Stone->TryConnectToAnchor(Cast<AStoneBuildingPiece>(Target), TargetAnchorId, TEXT("AutoBottom"));
        Reason = TEXT("The stone anchor is taken or does not match.");
    }
    else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
    {
        // Stick joints take a loose stick; keep physics off so it can attach.
        Stick->SetHeldState(false);
        if (UStaticMeshComponent* Mesh = Stick->FindComponentByClass<UStaticMeshComponent>())
            Mesh->SetSimulatePhysics(false);
        bConnected = Stick->TryConnectToAnchor(Cast<AStickPrimitivePiece>(Target), TargetAnchorId, TEXT("AutoBottom"));
        if (!bConnected)
        {
            Stick->SetHeldState(true);
        }
        else if (const float Tilt = GetStickTiltDegrees(); Tilt > 0.0f)
        {
            // Lean the new stick about its joint (45 degrees or lying across).
            FStructuralAnchorView Bottom;
            Stick->GetStructuralAnchor(TEXT("AutoBottom"), Bottom);
            const FVector Pivot = (Bottom.LocalTransform * Stick->GetActorTransform()).GetLocation();
            const FQuat Lean = MakeTiltAroundView(Tilt);
            Stick->SetActorLocationAndRotation(Pivot + Lean.RotateVector(Stick->GetActorLocation() - Pivot),
                Lean * Stick->GetActorQuat(), false, nullptr, ETeleportType::TeleportPhysics);
        }
        Reason = TEXT("The stick anchor is taken or does not match.");
    }

    if (!bConnected)
    {
        // A failed graph/anchor operation must not consume the held item.
        const bool bLifecycleRestored = Identity->TryTransitionLifecycle(
            EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::Held);
        const bool bOwnershipRestored = TransferMaterialOwnership(Piece, EWorldItemOwnerKind::World,
            EWorldItemOwnerKind::Held, Piece->GetActorLocation());
        if (!bLifecycleRestored || !bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Material placement rollback failed for %s; item requires recovery."),
                *GetNameSafe(Piece));
            PlacementStatus = TEXT("Placement failed and rollback was incomplete; item needs recovery.");
            return false;
        }
        PlacementStatus = FString::Printf(TEXT("Cannot place here: %s"), *Reason);
        return false;
    }
    FinishMaterialPlacement(Piece);
    PlacementStatus = TEXT("Placed.");
    LiftDroopingEndsOnto(Piece);
    return true;
}

bool ULogCarryComponent::TryPlaceHeldMaterialOnGround(FVector GroundPoint)
{
    AActor* Piece = HeldMaterialPiece;
    if (!IsValid(Piece) || !FMath::IsFinite(GroundPoint.X) ||
        !FMath::IsFinite(GroundPoint.Y) || !FMath::IsFinite(GroundPoint.Z))
        return false;
    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    if (!Identity || Identity->GetLifecycleState() != EWorldObjectLifecycleState::Held ||
        Identity->StateRevision == MAX_int64)
        return false;

    if (!TransferMaterialOwnership(Piece, EWorldItemOwnerKind::Held,
        EWorldItemOwnerKind::World, Piece->GetActorLocation()))
    {
        PlacementStatus = TEXT("Placement blocked: ownership ledger rejected the transfer.");
        return false;
    }
    if (!Identity->TryTransitionLifecycle(
        EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::Placed))
    {
        TransferMaterialOwnership(Piece, EWorldItemOwnerKind::World,
            EWorldItemOwnerKind::Held, Piece->GetActorLocation());
        PlacementStatus = TEXT("Placement blocked: lifecycle transition was rejected.");
        return false;
    }

    const FTransform Upright = MakeUprightTransform(Piece, GroundPoint);
    bool bPlaced = false;
    if (AStoneBuildingPiece* Stone = Cast<AStoneBuildingPiece>(Piece))
        bPlaced = Stone->TryCommitGroundPlacement(Upright.GetLocation(), Upright.Rotator());
    else if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece))
        bPlaced = Stick->TryCommitGroundPlacement(Upright.GetLocation(), Upright.Rotator());

    if (!bPlaced)
    {
        const bool bLifecycleRestored = Identity->TryTransitionLifecycle(
            EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::Held);
        const bool bOwnershipRestored = TransferMaterialOwnership(Piece, EWorldItemOwnerKind::World,
            EWorldItemOwnerKind::Held, Piece->GetActorLocation());
        if (!bLifecycleRestored || !bOwnershipRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Ground placement rollback failed for %s."), *GetNameSafe(Piece));
            PlacementStatus = TEXT("Placement failed and rollback was incomplete; item needs recovery.");
            return false;
        }
        PlacementStatus = TEXT("Cannot place on the ground here.");
        return false;
    }

    FinishMaterialPlacement(Piece);
    PlacementStatus = TEXT("Placed on the ground.");
    LiftDroopingEndsOnto(Piece);
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
    if (!IsValid(Piece))
        return;
    UWorldObjectIdentityComponent* Identity =
        Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    if (!Identity || Identity->GetLifecycleState() != EWorldObjectLifecycleState::Held ||
        Identity->StateRevision == MAX_int64)
        return;

    const FVector PreviousLocation = Piece->GetActorLocation();
    FVector ViewLocation;
    FRotator ViewRotation;
    const FVector DropLocation = GetViewPoint(ViewLocation, ViewRotation)
        ? ViewLocation + ViewRotation.Vector() * 110.0f : PreviousLocation;

    if (!TransferMaterialOwnership(Piece, EWorldItemOwnerKind::Held,
        EWorldItemOwnerKind::World, DropLocation))
        return;

    // Do not clear the held pointer or enable physics unless the material actor
    // accepts the held-state transition; otherwise restore the authoritative owner.
    if (!SetMaterialHeld(Piece, false) ||
        !Identity->TryTransitionLifecycle(
            EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World))
    {
        SetMaterialHeld(Piece, true);
        if (!TransferMaterialOwnership(Piece, EWorldItemOwnerKind::World,
            EWorldItemOwnerKind::Held, PreviousLocation))
        {
            UE_LOG(LogTemp, Error, TEXT("Material drop rollback failed for %s."),
                *GetNameSafe(Piece));
        }
        return;
    }

    Piece->SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Piece->SetActorEnableCollision(true);
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
    {
        // A dashed ring around the log where the saw will go.
        const FVector RingCentre = Centre - Axis * HalfLength + Axis * (2.0f * HalfLength * FirstShare);
        const float Radius = FMath::Max(HalfSide, HalfUp) * 1.05f;
        constexpr int32 Segments = 24;
        for (int32 Index = 0; Index < Segments; Index += 2)
        {
            const float A0 = 2.0f * PI * Index / Segments;
            const float A1 = 2.0f * PI * (Index + 1) / Segments;
            OutSegmentPoints.Add(World(RingCentre + (Side * FMath::Cos(A0) + Up * FMath::Sin(A0)) * Radius));
            OutSegmentPoints.Add(World(RingCentre + (Side * FMath::Cos(A1) + Up * FMath::Sin(A1)) * Radius));
        }
        return true;
    }

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

bool ULogCarryComponent::LiftDroopingEndsOnto(AActor* NewSupport)
{
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

bool ULogCarryComponent::CommitHeldLogAt(const FTransform& WorldTransform, AActor* PrimarySupport, AActor* SecondarySupport)
{
    ALogBuildPiece* Piece = HeldPiece;
    if (!IsValid(Piece) || WorldTransform.ContainsNaN() || !GetWorld())
        return false;
    const FTransform Previous = Piece->GetActorTransform();
    Piece->SetActorTransform(WorldTransform, false, nullptr, ETeleportType::TeleportPhysics);

    UWorldObjectIdentityComponent* Identity = Piece->FindComponentByClass<UWorldObjectIdentityComponent>();
    if (Identity && !Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::Placed))
    {
        Piece->SetActorTransform(Previous);
        return false;
    }
    FWorldOwnedItemRecord WorldRecord;
    if (!Piece->TryTransferOwnership(EWorldItemOwnerKind::Held, EWorldItemOwnerKind::World, FGuid(),
        Piece->GetActorLocation(), WorldRecord))
    {
        if (Identity)
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::Held);
        Piece->SetActorTransform(Previous);
        return false;
    }
    Piece->SetHeldState(false);
    Piece->SetPlacedState(true);

    // Rest on the supports in the structural graph; without them it stands on the ground.
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    const FGuid NodeId = Piece->GetStructuralNodeId();
    const IStructuralPieceInterface* Primary = Cast<IStructuralPieceInterface>(PrimarySupport);
    if (Graph && Primary && Primary->IsStructurallyPlaced() &&
        Graph->TryConnect(Primary->GetStructuralNodeId(), FName(*FString::Printf(TEXT("Deck_%s"), *NodeId.ToString())),
            NodeId, TEXT("DeckA")))
    {
        Piece->AttachToActor(PrimarySupport, FAttachmentTransformRules::KeepWorldTransform);
        const IStructuralPieceInterface* Secondary = Cast<IStructuralPieceInterface>(SecondarySupport);
        if (Secondary && Secondary->IsStructurallyPlaced())
        {
            Graph->TryAddSecondarySupport(NodeId, Secondary->GetStructuralNodeId(),
                FName(*FString::Printf(TEXT("Deck_%s"), *NodeId.ToString())), TEXT("DeckB"));
        }
    }

    ReleaseHeldFromLoad();
    HeldPiece = nullptr;
    bHasPlacementSurface = false;
    ClearSnapCandidate();
    PlacementStatus = TEXT("Placed.");
    ApplyEndContactSupports(Piece);
    CommitHeldLifts(Piece);
    LiftDroopingEndsOnto(Piece);
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

void ULogCarryComponent::UpdateStackPlacement()
{
    bHasStackPlacement = false;
    StackTarget.Reset();
    if (!IsValid(HeldPiece) ||
        (PlacementMode != ELogPlacementMode::Horizontal && PlacementMode != ELogPlacementMode::Vertical))
        return;
    FHitResult Hit;
    if (!TraceFromView(PlacementTraceDistance, Hit))
        return;
    ALogBuildPiece* Target = Cast<ALogBuildPiece>(Hit.GetActor());
    if (!IsValid(Target) || Target == HeldPiece || !Target->IsPlaced() || !MyProject3Construction::IsHorizontalLog(Target))
        return;
    FVector ViewLocation;
    FRotator ViewRotation;
    GetViewPoint(ViewLocation, ViewRotation);
    FTransform Transform;
    if (!ComputeStackPlacement(HeldPiece, Target, Hit.ImpactPoint, ViewRotation.Yaw + HeldYawOffset, Transform))
        return;
    bHasStackPlacement = true;
    StackTarget = Target;
    StackTransform = Transform;
    ClearSnapCandidate();
    HeldPiece->SetActorTransform(Transform);
}

bool ULogCarryComponent::ComputeStackPlacement(ALogBuildPiece* Held, ALogBuildPiece* Target, const FVector& AimPoint,
    float CarryYaw, FTransform& OutTransform) const
{
    using namespace MyProject3Construction;
    FLogGeometry Lower, Upper;
    if (!GetLogGeometry(Target, Lower) || !GetLogGeometry(Held, Upper) || StackGridCm <= 1.0f)
        return false;
    FVector Along = FVector(Lower.Axis.X, Lower.Axis.Y, 0.0f);
    if (!Along.Normalize())
        return false;
    const float EndA = -Lower.HalfLength;
    const float AimAlong = FVector::DotProduct(AimPoint - Lower.Centre, Along);
    auto OnGrid = [&](float S) { return EndA + FMath::RoundToFloat((S - EndA) / StackGridCm) * StackGridCm; };
    const FVector Up = FVector::UpVector;

    FVector Centre;
    FQuat Rotation;
    if (PlacementMode == ELogPlacementMode::Vertical)
    {
        // A post standing on the log at a grid point, seated in a shallow notch.
        const float S = FMath::Clamp(OnGrid(AimAlong), EndA, -EndA);
        Centre = Lower.Centre + Along * S + Up * (Lower.Radius * 0.6f + Upper.HalfLength);
        Rotation = MakeLogRotation(Held, Up, Along);
    }
    else
    {
        const FVector Want = FRotator(0.0f, CarryYaw, 0.0f).Vector();
        const float Alignment = FVector::DotProduct(Want, Along);
        const float StackZ = Lower.Centre.Z + (Lower.Radius + Upper.Radius) * StackInterlock;
        if (FMath::Abs(Alignment) >= 0.7071f)
        {
            // Lying along the lower log, like the next course of a wall: its
            // start sits on a grid point; it may run past the lower log's end.
            const float Start = OnGrid(AimAlong - Upper.HalfLength);
            Centre = Lower.Centre + Along * (Start + Upper.HalfLength);
            Rotation = MakeLogRotation(Held, Along * FMath::Sign(Alignment), Up);
        }
        else
        {
            // Crossing it, as at a corner: one of the upper log's own grid
            // points sits on the lower log's grid point under the crosshair.
            FVector Across = FVector::CrossProduct(Up, Along);
            if (FVector::DotProduct(Across, Want) < 0.0f)
                Across = -Across;
            const FVector Crossing = Lower.Centre + Along * FMath::Clamp(OnGrid(AimAlong), EndA, -EndA);
            const float AimAcross = FVector::DotProduct(AimPoint - Crossing, Across);
            const int32 Steps = FMath::Max(0, FMath::RoundToInt(Upper.Length() / StackGridCm));
            const int32 Step = FMath::Clamp(FMath::RoundToInt((Upper.HalfLength - AimAcross) / StackGridCm), 0, Steps);
            Centre = Crossing + Across * (Upper.HalfLength - Step * StackGridCm);
            Rotation = MakeLogRotation(Held, Across, Up);
        }
        Centre.Z = StackZ;
    }
    OutTransform = MakeLogTransform(Held, Centre, Rotation);

    // Do not stack into a log that is already there.
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LogStackSlot), false, Held);
    Params.AddIgnoredActor(Target);
    if (GetOwner())
        Params.AddIgnoredActor(GetOwner());
    GetWorld()->OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
        FCollisionShape::MakeSphere(Upper.Radius * 0.5f), Params);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        const ALogBuildPiece* Other = Cast<ALogBuildPiece>(Overlap.GetActor());
        if (Other && Other->IsPlaced())
            return false;
    }
    return true;
}
