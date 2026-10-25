#include "LogCarryComponent.h"

#include "LogBuildPiece.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
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
}

void ULogCarryComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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

    // A supporting log with dependent pieces cannot be removed yet. Remove
    // dependent pieces from the top down, then this support becomes removable.
    Piece->DetachFromSnapParent();
    Piece->SetHeldState(true);
    Piece->SetPlacedState(false);

    HeldPiece = Piece;
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
            PieceToPlace->SetHeldState(false);
            PieceToPlace->SetPlacedState(true);

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
    PieceToPlace->SetHeldState(false);
    PieceToPlace->SetPlacedState(true);

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
    if (GetViewPoint(ViewLocation, ViewRotation))
    {
        PieceToDrop->SetActorLocation(
            ViewLocation + ViewRotation.Vector() * 110.0f);
    }

    PieceToDrop->SetHeldState(false);
    PieceToDrop->SetPlacedState(false);
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
