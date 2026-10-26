#include "LogCarryComponent.h"

#include "CarryLoadComponent.h"

#include "LogBuildPiece.h"
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

    if (bIsDraggingLog)
    {
        ALogBuildPiece* Piece = DraggedPiece.Get();
        UWorldObjectIdentityComponent* Identity = IsValid(Piece)
            ? Piece->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        UWorldItemOwnershipSubsystem* Ownership = GetWorld()
            ? GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
        FWorldOwnedItemRecord ItemRecord;
        FVector ViewLocation;
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

    if (!IsValid(HeldPiece))
    {
        ClearSnapCandidate();
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

    const FVector End = ViewLocation + ViewRotation.Vector() * Distance;
    return World->LineTraceSingleByChannel(
        OutHit, ViewLocation, End, ECC_Visibility, Params);
}

void ULogCarryComponent::Interact()
{
    if (IsValid(HeldPiece))
    {
        PlaceHeldPiece();
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

    ALogBuildPiece* Piece = Cast<ALogBuildPiece>(Hit.GetActor());
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
        const bool bHitBuildPiece =
            Cast<ALogBuildPiece>(PlacementHit.GetActor()) != nullptr;
        bHasPlacementSurface = !bHitBuildPiece;

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

            ReleaseHeldFromLoad();
            HeldPiece = nullptr;
            bHasPlacementSurface = false;
            ClearSnapCandidate();
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

    ReleaseHeldFromLoad();
    HeldPiece = nullptr;
    bHasPlacementSurface = false;
    ClearSnapCandidate();
    return true;
}

void ULogCarryComponent::DropHeldPiece()
{
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
    default:
        PlacementMode = ELogPlacementMode::Horizontal;
        break;
    }

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

    if (Piece->ApplyCutAction(SelectedCutAction))
    {
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
