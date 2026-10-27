#include "LogBuildToolsComponent.h"

#include "ConstructionHelpers.h"
#include "HeatSourceComponent.h"
#include "LogAttachmentComponents.h"
#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "LogContainerActor.h"
#include "StickPrimitivePiece.h"
#include "HarvestableTreeActor.h"
#include "RopeLineActor.h"
#include "StructuralPieceInterface.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Components/InputComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

using namespace MyProject3Construction;

ULogBuildToolsComponent::ULogBuildToolsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void ULogBuildToolsComponent::BeginPlay()
{
    Super::BeginPlay();
    // Previews adjust the carried log after the carry component has moved it.
    if (ULogCarryComponent* Carry = GetCarry())
        AddTickPrerequisiteComponent(Carry);
}

FString ULogBuildToolsComponent::GetHelpText()
{
    return TEXT(
        "E pick up / place / use      G drop      Q throw      R rotate      RMB placement mode\n"
        "  modes: Horizontal, Vertical, Diagonal (45), Floor (floors, roofs, bridges, ramps), Stairs\n"
        "C choose cut (length cut: look along the log for 1/2, 3/4 or 1/4)   F mark the cut, F again to cut\n"
        "V push/roll   B drag\n"
        "  red dashes: where it will be cut   white dashes: where the carried log will lie, arrow: the joint\n"
        "LMB on a log with the axe: mark a cut, LMB again to cut; on the top of a standing log: sharpen; on a damaged piece: repair\n"
        "X sharpen standing log   K door / gate opening   P window opening\n"
        "Lock a door: carry a stick, E on the closed door from the inside; L takes the bar off\n"
        "Diagonal mode, aim at a post with a beam on it: a strut (payanda); struts let middle posts be removed\n"
        "Delete remove a support   H tie rope (twice; sloped rope = zipline)\n"
        "Z choose building plan   Y place / remove plan   T pull or release sled\n"
        "Carrying a log: E on storage/sled stores it, on a fire burns it, on a zipline top sends it\n"
        "U carrying a log, aim at a sloped beam: fill the gable (one-sided apex) under it, row by row\n"
        "Carrying a log, E on another loose log puts it on your shoulder (two logs at once)\n"
        "Defensive wall plan: Y at the start, Y at the end of the line\n"
        "J helper: follow / collect logs into storage / bring logs here / finish the nearest plan / sharpen and repair walls / stay\n"
        "F1 hide this help");
}

ULogCarryComponent* ULogBuildToolsComponent::GetCarry() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<ULogCarryComponent>() : nullptr;
}

bool ULogBuildToolsComponent::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Pawn && Pawn->GetController())
    {
        Pawn->GetController()->GetPlayerViewPoint(OutLocation, OutRotation);
        return true;
    }
    if (GetOwner())
    {
        GetOwner()->GetActorEyesViewPoint(OutLocation, OutRotation);
        return true;
    }
    return false;
}

bool ULogBuildToolsComponent::TraceView(float Distance, FHitResult& OutHit) const
{
    FVector Location;
    FRotator Rotation;
    if (!GetView(Location, Rotation) || !GetWorld())
        return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildToolsTrace), false, GetOwner());
    if (const ULogCarryComponent* Carry = GetCarry())
    {
        if (Carry->HeldPiece)
            Params.AddIgnoredActor(Carry->HeldPiece);
        if (Carry->HeldMaterialPiece)
            Params.AddIgnoredActor(Carry->HeldMaterialPiece);
    }
    return GetWorld()->LineTraceSingleByChannel(OutHit, Location, Location + Rotation.Vector() * Distance,
        ECC_Visibility, Params);
}

void ULogBuildToolsComponent::SetStatus(const FString& NewStatus, float HoldSeconds)
{
    Status = NewStatus;
    StatusHold = HoldSeconds;
}

void ULogBuildToolsComponent::SetPreviewStatus(const FString& NewStatus)
{
    if (StatusHold <= 0.0f)
        Status = NewStatus;
}

void ULogBuildToolsComponent::BindDefaultInput(UInputComponent* InputComponent)
{
    if (!InputComponent)
        return;
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ULogBuildToolsComponent::SharpenInput);
    InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ULogBuildToolsComponent::DoorInput);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ULogBuildToolsComponent::WindowInput);
    InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ULogBuildToolsComponent::LockInput);
    InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ULogBuildToolsComponent::RopeInput);
    InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ULogBuildToolsComponent::TowInput);
    InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ULogBuildToolsComponent::CyclePlanInput);
    InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ULogBuildToolsComponent::PlacePlanInput);
    InputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &ULogBuildToolsComponent::DismantleInput);
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ULogBuildToolsComponent::ThrowInput);
    InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ULogBuildToolsComponent::HelpInput);
    InputComponent->BindKey(EKeys::U, IE_Pressed, this, &ULogBuildToolsComponent::ApexInput);
}

bool ULogBuildToolsComponent::GetSupportLine(const AActor* Support, FVector& OutA, FVector& OutB, float& OutRadius) const
{
    if (const ARopeLineActor* Rope = Cast<ARopeLineActor>(Support))
    {
        OutA = Rope->GetStart();
        OutB = Rope->GetEnd();
        OutRadius = 3.0f;
        return true;
    }
    FLogGeometry Geometry;
    if (!GetLogGeometry(Cast<ALogBuildPiece>(Support), Geometry))
        return false;
    OutA = Geometry.EndA();
    OutB = Geometry.EndB();
    OutRadius = Geometry.Radius;
    return true;
}

AConstructionOpeningActor* ULogBuildToolsComponent::FindOpening(const FHitResult& Hit) const
{
    return Cast<AConstructionOpeningActor>(Hit.GetActor());
}

ALogContainerActor* ULogBuildToolsComponent::FindContainer(const FHitResult& Hit) const
{
    if (ALogContainerActor* Container = Cast<ALogContainerActor>(Hit.GetActor()))
        return Container;
    ALogContainerActor* Parent = Hit.GetActor() ? Cast<ALogContainerActor>(Hit.GetActor()->GetAttachParentActor()) : nullptr;
    return Parent && Parent->OwnsPiece(Hit.GetActor()) ? Parent : nullptr;
}

void ULogBuildToolsComponent::DropReleasedPiece(AActor* Piece) const
{
    if (!IsValid(Piece))
        return;
    TransferPiece(Piece, EWorldItemOwnerKind::World, FGuid(), Piece->GetActorLocation());
    if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Piece))
        Log->SetHeldState(false);
    if (UWorldObjectIdentityComponent* Identity = Piece->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::Held)
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World);
    }
    Piece->SetActorEnableCollision(true);
    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Piece->GetRootComponent()))
    {
        Root->SetCollisionProfileName(TEXT("PhysicsActor"));
        Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Root->SetSimulatePhysics(true);
    }
}

void ULogBuildToolsComponent::MakePlaced(ALogBuildPiece* Log) const
{
    if (!Log)
        return;
    if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::World)
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Placed);
    }
    Log->SetPlacedState(true);
}

// ---------------------------------------------------------------- interact

bool ULogBuildToolsComponent::TryHandleInteract()
{
    ULogCarryComponent* Carry = GetCarry();
    if (!Carry)
        return false;
    FHitResult Hit;
    const bool bHit = TraceView(Carry->PlacementTraceDistance, Hit);
    const bool bNear = bHit && Hit.Distance <= InteractRange;
    FVector ViewLocation;
    FRotator ViewRotation;
    GetView(ViewLocation, ViewRotation);
    ALogBuildPiece* HeldLog = Carry->HeldPiece;
    AActor* HeldAny = HeldLog ? static_cast<AActor*>(HeldLog) : Carry->HeldMaterialPiece.Get();

    if (HeldLog && bHit)
    {
        if (ABuildPlanActor* Plan = Cast<ABuildPlanActor>(Hit.GetActor()))
        {
            const int32 Slot = Plan->FindSlot(Hit.GetComponent(), Hit.ImpactPoint);
            if (Slot != INDEX_NONE)
                return TryFillPlanSlot(Plan, Slot);
        }
    }
    if (HeldLog && bNear)
    {
        if (AConstructionOpeningActor* Opening = FindOpening(Hit))
        {
            if (!Opening->HasPanel() && Opening->Kind != EConstructionOpeningKind::Window)
            {
                FString Reason;
                if (!IsSplitBoard(HeldLog) && HeldLog->GetCrossSectionProfile() == ELogCrossSectionProfile::Round &&
                    Carry->SplitCarriedLogInHalf())
                {
                    HeldLog = Carry->HeldPiece;
                }
                if (!HeldLog || !IsSplitBoard(HeldLog))
                {
                    SetStatus(TEXT("Doors and gates need split boards (plank, half or quarter log)."));
                    return true;
                }
                Carry->ReleaseHeldToTool();
                if (Opening->TryAddBoard(HeldLog, GetOwner()->GetActorLocation(), Hit.ImpactPoint, Reason))
                    SetStatus(Opening->HasPanel() ? TEXT("Door finished: E opens it, L locks it from this side.")
                        : FString::Printf(TEXT("Board added (%d/%d)."), Opening->GetBoardsAdded(), Opening->GetRequiredBoards()));
                else
                {
                    SetStatus(Reason);
                    DropReleasedPiece(HeldLog);
                }
                return true;
            }
        }
    }
    if (AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Carry->HeldMaterialPiece.Get()); Stick && bNear)
    {
        if (AConstructionOpeningActor* Opening = FindOpening(Hit); Opening && Opening->HasPanel())
        {
            FString Reason;
            Carry->ReleaseHeldToTool();
            Stick->SetHeldState(false);
            Stick->SetActorEnableCollision(false);
            if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Stick->GetRootComponent()))
                Root->SetSimulatePhysics(false);
            TransferPiece(Stick, EWorldItemOwnerKind::World, FGuid(), Stick->GetActorLocation());
            if (Opening->TryBarWithStick(Stick, GetOwner()->GetActorLocation(), Reason))
            {
                if (UWorldObjectIdentityComponent* Identity = Stick->FindComponentByClass<UWorldObjectIdentityComponent>())
                {
                    if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::Held)
                        Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::Placed);
                }
                SetStatus(TEXT("Barred with the stick. L from the inside takes the bar off."));
            }
            else
            {
                SetStatus(Reason);
                DropReleasedPiece(Stick);
            }
            return true;
        }
    }
    if (HeldAny && bNear)
    {
        if (ALogContainerActor* Container = FindContainer(Hit))
        {
            FString Reason;
            if (!Container->CanAccept(HeldAny, Reason))
            {
                SetStatus(Reason);
                return true;
            }
            Carry->ReleaseHeldToTool();
            if (Container->TryStore(HeldAny))
                SetStatus(FString::Printf(TEXT("Stored (%d/%d)."), Container->GetStoredCount(), Container->GetCapacity()));
            else
                DropReleasedPiece(HeldAny);
            return true;
        }
        if (Hit.GetActor() && Hit.GetActor()->FindComponentByClass<UHeatSourceComponent>())
            return FeedFire(Hit.GetActor());
    }
    if (HeldAny && bHit)
    {
        if (ARopeLineActor* Rope = Cast<ARopeLineActor>(Hit.GetActor()))
        {
            if (Rope->IsZipline() && FVector::Dist(GetOwner()->GetActorLocation(), Rope->GetUpperPoint()) < 500.0f)
            {
                Carry->ReleaseHeldToTool();
                if (UWorldObjectIdentityComponent* Identity = HeldAny->FindComponentByClass<UWorldObjectIdentityComponent>())
                {
                    if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::Held)
                        Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Held, EWorldObjectLifecycleState::World);
                }
                if (Rope->TrySendAlong(HeldAny))
                    SetStatus(TEXT("Sent down the zipline."));
                else
                    DropReleasedPiece(HeldAny);
                return true;
            }
        }
    }
    if (HeldLog && bHit && (Carry->PlacementMode == ELogPlacementMode::Floor ||
        Carry->PlacementMode == ELogPlacementMode::Stairs))
    {
        return TryPlaceDeck(Hit.GetActor(), Hit.ImpactPoint, ViewRotation.Vector(),
            Carry->PlacementMode == ELogPlacementMode::Stairs);
    }

    if (!HeldAny && bNear)
    {
        if (AConstructionOpeningActor* Opening = FindOpening(Hit))
        {
            FString Reason;
            if (Opening->ToggleOpen(GetOwner()->GetActorLocation(), Reason))
                SetStatus(Opening->IsOpen() ? TEXT("Opened.") : TEXT("Closed."), 1.5f);
            else
                SetStatus(Reason);
            return true;
        }
        if (ALogContainerActor* Container = FindContainer(Hit))
        {
            const FVector Drop = ViewLocation + ViewRotation.Vector() * 120.0f;
            AActor* Piece = Container->TakeOne(Drop);
            if (!Piece)
            {
                SetStatus(TEXT("It is empty."));
                return true;
            }
            Carry->TryPickupPiece(Piece);
            SetStatus(FString::Printf(TEXT("Taken (%d left)."), Container->GetStoredCount()));
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- previews

void ULogBuildToolsComponent::UpdatePreview()
{
    ULogCarryComponent* Carry = GetCarry();
    ALogBuildPiece* Held = Carry ? Carry->HeldPiece.Get() : nullptr;
    if (!Held)
        return;
    FHitResult Hit;
    FVector ViewLocation;
    FRotator ViewRotation;
    if (!GetView(ViewLocation, ViewRotation) || !TraceView(Carry->PlacementTraceDistance, Hit))
        return;

    if (ABuildPlanActor* Plan = Cast<ABuildPlanActor>(Hit.GetActor()))
    {
        const int32 Slot = Plan->FindSlot(Hit.GetComponent(), Hit.ImpactPoint);
        if (Slot != INDEX_NONE)
        {
            const FTransform SlotWorld = Plan->GetSlotWorld(Slot);
            Held->SetActorTransform(MakeLogTransform(Held, SlotWorld.GetLocation(),
                MakeLogRotation(Held, SlotWorld.GetRotation().GetAxisX(), SlotWorld.GetRotation().GetAxisZ())));
            FLogGeometry Geometry;
            GetLogGeometry(Held, Geometry);
            const float Needed = Plan->GetSlot(Slot).LengthCm;
            FString Reason;
            const bool bReady = Plan->CanFillSlot(Slot, Reason);
            SetPreviewStatus(FString::Printf(TEXT("%s: part %d/%d. %s%s"), *ABuildPlanActor::GetPlanName(Plan->Kind),
                Plan->GetFilledCount() + 1, Plan->GetSlotCount(), bReady ? TEXT("E to place") : *Reason,
                Geometry.Length() > Needed * 1.1f ? *FString::Printf(TEXT(" (cut to %.0f cm)"), Needed) : TEXT("")));
            return;
        }
    }

    if (Carry->PlacementMode == ELogPlacementMode::Floor || Carry->PlacementMode == ELogPlacementMode::Stairs)
    {
        FTransform Transform;
        AActor* Second = nullptr;
        float Span = 0.0f;
        FString Reason;
        if (ComputeDeckPlacement(Held, Hit.GetActor(), Hit.ImpactPoint, ViewRotation.Vector(),
            Carry->PlacementMode == ELogPlacementMode::Stairs, Transform, Second, Span, Reason))
        {
            Held->SetActorTransform(Transform);
            SetPreviewStatus(FString::Printf(TEXT("%s across a %.0f cm gap: E to place."),
                Carry->PlacementMode == ELogPlacementMode::Stairs ? TEXT("Step") : TEXT("Board"), Span));
        }
        else
        {
            SetPreviewStatus(Reason);
        }
        return;
    }

    if (Carry->PlacementMode == ELogPlacementMode::Vertical)
    {
        // Help levelling posts on uneven ground: compare tops with the nearest post.
        FLogGeometry Geometry;
        if (!GetLogGeometry(Held, Geometry))
            return;
        const float Top = Geometry.Centre.Z + Geometry.HalfLength;
        float BestDistance = 450.0f;
        float Delta = 0.0f;
        bool bFound = false;
        for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
        {
            FLogGeometry Other;
            if (*It == Held || !It->IsPlaced() || !GetLogGeometry(*It, Other) || FMath::Abs(Other.Axis.Z) < 0.8f)
                continue;
            const float Distance = FVector::Dist2D(Other.Centre, Geometry.Centre);
            if (Distance < BestDistance && Distance > 5.0f)
            {
                BestDistance = Distance;
                Delta = Top - (Other.Centre.Z + Other.HalfLength);
                bFound = true;
            }
        }
        if (bFound)
            SetPreviewStatus(FString::Printf(TEXT("Post top %+.0f cm from the nearby post%s"), Delta,
                FMath::Abs(Delta) > 8.0f ? TEXT(": use a shorter/longer log to level.") : TEXT(": level.")));
    }
}

void ULogBuildToolsComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    StatusHold -= DeltaTime;
    if (StatusHold <= 0.0f)
        Status.Reset();
    UpdatePreview();
}

// ---------------------------------------------------------------- floors, roofs, bridges, stairs

bool ULogBuildToolsComponent::ComputeDeckPlacement(ALogBuildPiece* Board, AActor* Support, const FVector& AimPoint,
    const FVector& ViewDirection, bool bStairs, FTransform& OutTransform, AActor*& OutSecondSupport,
    float& OutSpan, FString& OutReason) const
{
    FLogGeometry BoardShape;
    if (!GetLogGeometry(Board, BoardShape))
    {
        OutReason = TEXT("Carry a log or board.");
        return false;
    }
    const IStructuralPieceInterface* SupportPiece = Cast<IStructuralPieceInterface>(Support);
    FVector A0, A1;
    float R1 = 0.0f;
    if (!SupportPiece || !SupportPiece->IsStructurallyPlaced() || !GetSupportLine(Support, A0, A1, R1))
    {
        OutReason = TEXT("Floor mode: aim at a placed beam, stringer or rope.");
        return false;
    }
    FVector Axis1 = (A1 - A0).GetSafeNormal();
    if (Axis1.Z < 0.0f)
    {
        Axis1 = -Axis1;
        Swap(A0, A1);
    }
    if (Axis1.Z > 0.95f)
    {
        OutReason = TEXT("Boards rest on beams, not on standing posts.");
        return false;
    }
    const bool bSloped = Axis1.Z > 0.3f;
    const FVector Horizontal1 = FVector(Axis1.X, Axis1.Y, 0.0f).GetSafeNormal();
    const FVector Perp = FVector::CrossProduct(FVector::UpVector, Horizontal1).GetSafeNormal();
    const FVector P = ClosestPointOnSegment(AimPoint, A0, A1);
    const float MaxSpan = BoardShape.Length() - 20.0f;

    AActor* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    float BestOffset = 0.0f;
    auto Consider = [&](AActor* Candidate)
    {
        if (!Candidate || Candidate == Support || Candidate == Board)
            return;
        const IStructuralPieceInterface* Piece = Cast<IStructuralPieceInterface>(Candidate);
        FVector C0, C1;
        float Rc = 0.0f;
        if (!Piece || !Piece->IsStructurallyPlaced() || !GetSupportLine(Candidate, C0, C1, Rc))
            return;
        if (FMath::Abs(FVector::DotProduct((C1 - C0).GetSafeNormal(), Axis1)) < 0.9f)
            return; // Not parallel.
        const FVector Delta = ClosestPointOnSegment(P, C0, C1) - P;
        const float Offset = FVector::DotProduct(Delta, Perp);
        if ((Delta - Perp * Offset).Size() > 80.0f || FMath::Abs(Offset) < 30.0f || FMath::Abs(Offset) > MaxSpan)
            return;
        // Prefer the side the player looks toward, then the nearer one.
        const float Score = FMath::Abs(Offset) - (FVector::DotProduct(Perp * FMath::Sign(Offset), ViewDirection) > 0.0f ? 10000.0f : 0.0f);
        if (Score < BestScore)
        {
            BestScore = Score;
            Best = Candidate;
            BestOffset = Offset;
        }
    };
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        if (FVector::Dist(It->GetActorLocation(), P) < 2.0f * BoardShape.Length() + 600.0f)
            Consider(*It);
    }
    for (TActorIterator<ARopeLineActor> It(GetWorld()); It; ++It)
        Consider(*It);
    if (!Best)
    {
        OutReason = FString::Printf(TEXT("Needs a second parallel beam within %.0f cm (the board's length)."), MaxSpan);
        return false;
    }

    const float Pitch = bStairs ? StairStepSpacing : 2.0f * BoardShape.Radius + 1.0f;
    const float SupportLength = FVector::Dist(A0, A1);
    const float Along = FMath::Clamp(FMath::RoundToFloat(FVector::DotProduct(P - A0, Axis1) / Pitch) * Pitch,
        BoardShape.Radius, FMath::Max(BoardShape.Radius, SupportLength - BoardShape.Radius));
    const FVector P2 = A0 + Axis1 * Along;
    FVector C0, C1;
    float R2 = 0.0f;
    GetSupportLine(Best, C0, C1, R2);
    const FVector Q2 = ClosestPointOnSegment(P2, C0, C1);
    FVector BoardAxis = Q2 - P2;
    BoardAxis.Z = 0.0f;
    if (!BoardAxis.Normalize())
    {
        OutReason = TEXT("The supports are on top of each other.");
        return false;
    }
    FVector Centre = 0.5f * (P2 + Q2);
    Centre.Z = FMath::Max(P2.Z + R1, Q2.Z + R2) + BoardShape.Radius;
    FVector UpHint = FVector::UpVector;
    if (bSloped && !bStairs)
    {
        // Ramp boards tilt with the slope so their faces form one surface.
        UpHint = FVector::CrossProduct(BoardAxis, Axis1).GetSafeNormal();
        if (UpHint.Z < 0.0f)
            UpHint = -UpHint;
    }
    OutTransform = MakeLogTransform(Board, Centre, MakeLogRotation(Board, BoardAxis, UpHint));
    OutSecondSupport = Best;
    OutSpan = FVector::Dist2D(P2, Q2);

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DeckSlot), false, Board);
    Params.AddIgnoredActor(Support);
    Params.AddIgnoredActor(Best);
    Params.AddIgnoredActor(GetOwner());
    GetWorld()->OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
        FCollisionShape::MakeSphere(BoardShape.Radius * 0.7f), Params);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        if (Cast<ALogBuildPiece>(Overlap.GetActor()))
        {
            OutReason = TEXT("This spot already has a board.");
            return false;
        }
    }
    return true;
}

bool ULogBuildToolsComponent::TryPlaceDeck(AActor* Support, const FVector& AimPoint, const FVector& ViewDirection, bool bStairs)
{
    ULogCarryComponent* Carry = GetCarry();
    ALogBuildPiece* Board = Carry ? Carry->HeldPiece.Get() : nullptr;
    FTransform Transform;
    AActor* Second = nullptr;
    float Span = 0.0f;
    FString Reason;
    if (!ComputeDeckPlacement(Board, Support, AimPoint, ViewDirection, bStairs, Transform, Second, Span, Reason))
    {
        // No second support: on a floor or beam edge it becomes a single step.
        if (TryPlaceSingleStep(Cast<ALogBuildPiece>(Support), AimPoint))
            return true;
        SetStatus(Reason);
        return true;
    }
    if (Carry->IsBadlyDamaged(Support) || Carry->IsBadlyDamaged(Second))
    {
        SetStatus(TEXT("This piece is damaged: repair it before building on it (LMB with the axe)."));
        return true;
    }
    // Floors, ramps and steps are made of split logs: a full log is split on the spot.
    FString SplitNote;
    if (Board->GetCrossSectionProfile() == ELogCrossSectionProfile::Round && Carry->SplitCarriedLogInHalf())
    {
        Board = Carry->HeldPiece;
        SplitNote = TEXT(" Split in half; the other half is on your shoulder.");
        if (!Board || !ComputeDeckPlacement(Board, Support, AimPoint, ViewDirection, bStairs, Transform, Second, Span, Reason))
        {
            SetStatus(Reason);
            return true;
        }
    }
    // A board much longer than the gap is cut to fit; the offcut drops at the feet.
    FLogGeometry Shape;
    GetLogGeometry(Board, Shape);
    const float Fit = Span + 4.0f * Shape.Radius + 30.0f;
    FString Note;
    if (Shape.Length() > Fit * 1.15f)
    {
        if (CutOffRemainder(Board, Fit, GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.0f))
        {
            Note = FString::Printf(TEXT(" Cut to %.0f cm."), Fit);
            ComputeDeckPlacement(Board, Support, AimPoint, ViewDirection, bStairs, Transform, Second, Span, Reason);
        }
    }
    if (!Carry->CommitHeldLogAt(Transform, Support, Second))
    {
        SetStatus(TEXT("The board could not be placed."));
        return true;
    }
    SetStatus(FString::Printf(TEXT("%s placed.%s%s"), bStairs ? TEXT("Step") : TEXT("Board"), *Note, *SplitNote), 3.0f);
    return true;
}

bool ULogBuildToolsComponent::TryPlaceSingleStep(ALogBuildPiece* Edge, const FVector& AimPoint)
{
    ULogCarryComponent* Carry = GetCarry();
    FLogGeometry Floor;
    if (!Carry || !Carry->HeldPiece || !Edge || !Edge->IsPlaced() || !IsHorizontalLog(Edge) || !GetLogGeometry(Edge, Floor) ||
        Carry->IsBadlyDamaged(Edge))
        return false;
    if (Carry->HeldPiece->GetCrossSectionProfile() == ELogCrossSectionProfile::Round)
        Carry->SplitCarriedLogInHalf();
    ALogBuildPiece* Board = Carry->HeldPiece;
    FLogGeometry Step;
    if (!Board || !GetLogGeometry(Board, Step))
        return false;
    // In front of the edge, on the player's side, a little lower than its top.
    const FVector OnEdge = ClosestPointOnSegment(AimPoint, Floor.EndA(), Floor.EndB());
    FVector Out = GetOwner()->GetActorLocation() - OnEdge;
    Out.Z = 0.0f;
    const FVector Along = Floor.Axis.GetSafeNormal2D();
    Out = (Out - Along * FVector::DotProduct(Out, Along)).GetSafeNormal();
    if (Out.IsNearlyZero())
    {
        // Otherwise the side of the edge the player aimed at.
        Out = AimPoint - OnEdge;
        Out.Z = 0.0f;
        Out = (Out - Along * FVector::DotProduct(Out, Along)).GetSafeNormal();
    }
    if (Out.IsNearlyZero())
        return false;
    constexpr float StepLength = 150.0f;
    if (Step.Length() > StepLength + 10.0f)
        CutOffRemainder(Board, StepLength, GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.0f);
    GetLogGeometry(Board, Step);
    FVector Centre = OnEdge + Out * (Floor.Radius + Step.Radius + 25.0f);
    Centre.Z = Floor.Centre.Z + Floor.Radius - 25.0f - Step.Radius;
    const FTransform Target = MakeLogTransform(Board, Centre, MakeLogRotation(Board, Along, FVector::UpVector));
    if (!Carry->CommitHeldLogAt(Target, Edge, nullptr))
        return false;
    SetStatus(TEXT("Single step placed."), 2.5f);
    return true;
}

float ULogBuildToolsComponent::GetLongestUnsupportedSpan(const ALogBuildPiece* Beam, const ALogBuildPiece* Ignore,
    bool& bOutOverhangTooLong) const
{
    bOutOverhangTooLong = false;
    FLogGeometry B;
    if (!GetLogGeometry(Beam, B))
        return 0.0f;
    const FVector Start = B.EndA();
    const FVector Dir = B.Axis.GetSafeNormal();
    TArray<float> Points;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        FLogGeometry O;
        if (*It == Beam || *It == Ignore || !It->IsPlaced() || !GetLogGeometry(*It, O))
            continue;
        const float Up = FMath::Abs(O.Axis.Z);
        TArray<FVector> Contacts;
        if (Up >= 0.3f)
        {
            // A post or a strut: its upper end.
            Contacts.Add(O.EndA().Z > O.EndB().Z ? O.EndA() : O.EndB());
        }
        else if (FMath::Abs(FVector::DotProduct(O.Axis.GetSafeNormal(), Dir)) > 0.95f)
        {
            // A log lying along under it holds it along their overlap.
            for (float Step = -O.HalfLength; Step <= O.HalfLength; Step += 50.0f)
                Contacts.Add(O.Centre + O.Axis * Step);
        }
        else
        {
            // A log crossing under it.
            FVector OnOther = ClosestPointOnSegment(B.Centre, O.EndA(), O.EndB());
            const FVector OnBeam = ClosestPointOnSegment(OnOther, B.EndA(), B.EndB());
            OnOther = ClosestPointOnSegment(OnBeam, O.EndA(), O.EndB());
            Contacts.Add(OnOther);
        }
        for (const FVector& Contact : Contacts)
        {
            const FVector OnBeam = ClosestPointOnSegment(Contact, B.EndA(), B.EndB());
            if (Contact.Z <= OnBeam.Z + 5.0f && FVector::Dist(Contact, OnBeam) < O.Radius + B.Radius + 15.0f)
                Points.Add(FVector::DotProduct(OnBeam - Start, Dir));
        }
    }
    if (Points.Num() == 0)
    {
        bOutOverhangTooLong = true;
        return B.Length();
    }
    Points.Sort();
    bOutOverhangTooLong = Points[0] > MaxBeamOverhangCm || B.Length() - Points.Last() > MaxBeamOverhangCm;
    float Longest = 0.0f;
    for (int32 Index = 1; Index < Points.Num(); ++Index)
        Longest = FMath::Max(Longest, Points[Index] - Points[Index - 1]);
    return Longest;
}

// ---------------------------------------------------------------- plans

ABuildPlanActor* ULogBuildToolsComponent::PlacePlan(EBuildPlanKind Kind, const FVector& Location, float Yaw)
{
    if (!GetWorld())
        return nullptr;
    FVector Where = Location;
    AHarvestableTreeActor* Tree = nullptr;
    if (ABuildPlanActor::NeedsTree(Kind))
    {
        // Tree plans go around the nearest standing trunk.
        float Best = 500.0f;
        for (TActorIterator<AHarvestableTreeActor> It(GetWorld()); It; ++It)
        {
            const float Distance = FVector::Dist2D(It->GetActorLocation(), Location);
            if (It->CanBeChopped() && Distance < Best)
            {
                Best = Distance;
                Tree = *It;
            }
        }
        if (!Tree)
            return nullptr;
        const FBoxSphereBounds Trunk = Tree->TreeMesh->Bounds;
        Where = FVector(Trunk.Origin.X, Trunk.Origin.Y, Trunk.Origin.Z - Trunk.BoxExtent.Z);
    }
    ABuildPlanActor* Plan = GetWorld()->SpawnActor<ABuildPlanActor>(Where, FRotator(0.0f, Yaw, 0.0f));
    if (Plan)
    {
        Plan->AnchorTree = Tree;
        Plan->InitializePlan(Kind);
    }
    return Plan;
}

ABuildPlanActor* ULogBuildToolsComponent::PlaceDefenseLine(const FVector& From, const FVector& To)
{
    const FVector Line = To - From;
    const float Length = Line.Size2D();
    if (!GetWorld() || Length < 80.0f)
        return nullptr;
    // The plan's stakes run along its local Y.
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Line.Y, Line.X)) - 90.0f;
    ABuildPlanActor* Plan = GetWorld()->SpawnActor<ABuildPlanActor>(0.5f * (From + To), FRotator(0.0f, Yaw, 0.0f));
    if (Plan)
    {
        Plan->LineLengthCm = Length;
        Plan->InitializePlan(EBuildPlanKind::DefenseWall);
    }
    return Plan;
}

bool ULogBuildToolsComponent::TryFillApex(ALogBuildPiece* SlopedBeam)
{
    ULogCarryComponent* Carry = GetCarry();
    ALogBuildPiece* Log = Carry ? Carry->HeldPiece.Get() : nullptr;
    FLogGeometry Beam, Held;
    if (!Log || !SlopedBeam || !SlopedBeam->IsPlaced() || !GetLogGeometry(SlopedBeam, Beam) || !GetLogGeometry(Log, Held))
    {
        SetStatus(TEXT("Carry a log and aim at a sloped beam to fill the gable under it."));
        return false;
    }
    const float Slope = FMath::Abs(Beam.Axis.Z);
    if (Slope < 0.15f || Slope > 0.9f)
    {
        SetStatus(TEXT("A gable is filled under a sloped beam."));
        return false;
    }
    FVector Low = Beam.EndA(), High = Beam.EndB();
    if (Low.Z > High.Z)
        Swap(Low, High);
    const FVector Run = High - Low;
    const FVector Along = Run.GetSafeNormal2D();
    const float Span = Run.Size2D();
    const float Tan = (High.Z - Low.Z) / FMath::Max(Span, 1.0f);
    const float Step = 2.0f * Held.Radius * 0.9f;

    // Rows from the beam's low end upwards; each ends where it meets the slope.
    for (int32 Row = 0; Row < 24; ++Row)
    {
        const float Z = Low.Z - Beam.Radius + Held.Radius + Row * Step;
        const float Start = (Z + Held.Radius + Beam.Radius - Low.Z) / Tan;
        const float Length = Span - Start;
        if (Length < 40.0f)
            break;
        FVector Centre = Low + Along * (Start + 0.5f * Length);
        Centre.Z = Z;
        bool bTaken = false;
        for (TActorIterator<ALogBuildPiece> It(GetWorld()); It && !bTaken; ++It)
        {
            FLogGeometry Other;
            if (*It == SlopedBeam || *It == Log || !It->IsPlaced() || !GetLogGeometry(*It, Other))
                continue;
            bTaken = FVector::Dist(ClosestPointOnSegment(Centre, Other.EndA(), Other.EndB()), Centre) < Held.Radius;
        }
        if (bTaken)
            continue;
        if (Held.Length() < Length * 0.85f)
        {
            SetStatus(FString::Printf(TEXT("Too short: this gable row needs about %.0f cm."), Length));
            return true;
        }
        if (Held.Length() > Length + 10.0f)
            CutOffRemainder(Log, Length, GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.0f);
        const FTransform Target = MakeLogTransform(Log, Centre, MakeLogRotation(Log, Along, FVector::UpVector));
        if (!Carry->CommitHeldLogAt(Target, SlopedBeam, nullptr))
        {
            SetStatus(TEXT("The log could not be placed."));
            return true;
        }
        SetStatus(FString::Printf(TEXT("Gable row %d placed (%.0f cm)."), Row + 1, Length));
        return true;
    }
    SetStatus(TEXT("The gable under this beam is full."));
    return true;
}

void ULogBuildToolsComponent::ApexInput()
{
    FHitResult Hit;
    if (TraceView(InteractRange, Hit))
        TryFillApex(Cast<ALogBuildPiece>(Hit.GetActor()));
    else
        SetStatus(TEXT("Carry a log and aim at a sloped beam to fill the gable under it."));
}

bool ULogBuildToolsComponent::TryFillPlanSlot(ABuildPlanActor* Plan, int32 SlotIndex)
{
    ULogCarryComponent* Carry = GetCarry();
    ALogBuildPiece* Log = Carry ? Carry->HeldPiece.Get() : nullptr;
    FString Reason;
    if (!Plan || !Log || !Plan->CanFillSlot(SlotIndex, Reason))
    {
        SetStatus(Reason.IsEmpty() ? TEXT("Carry a log to build the plan.") : Reason);
        return true;
    }
    FLogGeometry Shape;
    GetLogGeometry(Log, Shape);
    const float Needed = Plan->GetSlot(SlotIndex).LengthCm;
    if (Shape.Length() < Needed * 0.85f)
    {
        SetStatus(FString::Printf(TEXT("Too short: this part needs about %.0f cm (log is %.0f cm)."), Needed, Shape.Length()));
        return true;
    }
    FString Note;
    if (Shape.Length() > Needed * 1.1f &&
        CutOffRemainder(Log, Needed, GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.0f))
    {
        Note = FString::Printf(TEXT(" Cut to %.0f cm."), Needed);
    }
    const FTransform SlotWorld = Plan->GetSlotWorld(SlotIndex);
    const FTransform Target = MakeLogTransform(Log, SlotWorld.GetLocation(),
        MakeLogRotation(Log, SlotWorld.GetRotation().GetAxisX(), SlotWorld.GetRotation().GetAxisZ()));
    if (!Carry->CommitHeldLogAt(Target, Plan->GetSupportPiece(SlotIndex), nullptr))
    {
        SetStatus(TEXT("The log could not be placed."));
        return true;
    }
    const FString Name = ABuildPlanActor::GetPlanName(Plan->Kind);
    const int32 Done = Plan->GetFilledCount() + 1;
    const int32 Total = Plan->GetSlotCount();
    Plan->MarkFilled(SlotIndex, Log);
    SetStatus(Done >= Total ? FString::Printf(TEXT("%s finished.%s"), *Name, *Note)
        : FString::Printf(TEXT("%s: %d/%d.%s"), *Name, Done, Total, *Note));
    return true;
}

// ---------------------------------------------------------------- stakes, openings, removal

bool ULogBuildToolsComponent::SharpenLog(ALogBuildPiece* Log)
{
    if (!Log || !IsVerticalLog(Log))
    {
        SetStatus(TEXT("Sharpen a log that stands upright."));
        return false;
    }
    if (Log->FindComponentByClass<USharpenedTipComponent>())
    {
        SetStatus(TEXT("Already sharpened."));
        return false;
    }
    USharpenedTipComponent* Tip = NewObject<USharpenedTipComponent>(Log);
    Tip->RegisterComponent();
    if (!Tip->InitializeTip())
    {
        Tip->DestroyComponent();
        return false;
    }
    SetStatus(TEXT("Sharpened: climbers will be hurt and pushed back."));
    return true;
}

bool ULogBuildToolsComponent::DismantlePiece(ALogBuildPiece* Piece)
{
    UWorldStructuralGraphSubsystem* Graph = GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!Piece || !Graph || !Piece->IsPlaced())
    {
        SetStatus(TEXT("Aim at a log that is part of a structure."));
        return false;
    }
    const FGuid NodeId = Piece->GetStructuralNodeId();
    if (!Graph->CanSafelyRemove(NodeId))
    {
        SetStatus(TEXT("Other pieces rest only on this one: add another support first."));
        return false;
    }
    if (ULogCarryComponent* Carry = GetCarry(); Carry && IsVerticalLog(Piece))
    {
        // A beam may not be left spanning too far: struts at its posts let middle posts go.
        TArray<ALogBuildPiece*> Beams;
        Carry->FindBeamsOnPost(Piece, Beams);
        for (const ALogBuildPiece* Beam : Beams)
        {
            bool bOverhang = false;
            const float Span = GetLongestUnsupportedSpan(Beam, Piece, bOverhang);
            if (Span > MaxBeamSpanCm || bOverhang)
            {
                SetStatus(FString::Printf(TEXT("Without this post the beam would sag (%.0f cm unsupported). ")
                    TEXT("Put struts at its posts first (half logs, diagonal mode)."), bOverhang ? MaxBeamOverhangCm : Span));
                return false;
            }
        }
    }
    TArray<AActor*> Attached;
    Piece->GetAttachedActors(Attached);

    // Removing the node hands every child over to its next support.
    if (!Graph->UnregisterNode(NodeId, false))
    {
        SetStatus(TEXT("The structure refused the change."));
        return false;
    }
    Graph->RegisterNode(Piece, NodeId, EStructuralNodeKind::Wood, false, 1.0f, 100000.0f, 100.0f);

    for (AActor* Child : Attached)
    {
        if (ALogBuildPiece* ChildLog = Cast<ALogBuildPiece>(Child))
        {
            ChildLog->DetachFromSnapParent(true);
            ChildLog->SetPlacedState(true);
        }
        else
        {
            Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        }
        const IStructuralPieceInterface* ChildPiece = Cast<IStructuralPieceInterface>(Child);
        FStructuralNodeRecord ChildNode;
        if (ChildPiece && Graph->GetNode(ChildPiece->GetStructuralNodeId(), ChildNode) && ChildNode.ParentNodeId.IsValid())
        {
            Graph->SetGroundSupported(ChildNode.NodeId, false);
            FStructuralNodeRecord NewParent;
            if (Graph->GetNode(ChildNode.ParentNodeId, NewParent) && NewParent.Actor.IsValid())
                Child->AttachToActor(NewParent.Actor.Get(), FAttachmentTransformRules::KeepWorldTransform);
        }
    }

    Piece->DetachFromSnapParent(true);
    Piece->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Piece->SetPlacedState(false);
    if (UWorldObjectIdentityComponent* Identity = Piece->FindComponentByClass<UWorldObjectIdentityComponent>())
    {
        if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::Placed)
            Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::World);
    }
    SetStatus(TEXT("Removed from the structure; the rest stands on its other supports."));
    return true;
}

bool ULogBuildToolsComponent::CutGate(ALogBuildPiece* Post, const FVector& AimPoint, AConstructionOpeningActor*& OutOpening)
{
    FLogGeometry PostShape;
    GetLogGeometry(Post, PostShape);
    // The wall runs from this post to its nearest standing neighbour.
    FVector WallDirection = FVector::ZeroVector;
    float Nearest = 90.0f;
    TArray<ALogBuildPiece*> Posts;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        FLogGeometry Other;
        if (!It->IsPlaced() || !GetLogGeometry(*It, Other) || FMath::Abs(Other.Axis.Z) < 0.8f)
            continue;
        Posts.Add(*It);
        const float Distance = FVector::Dist2D(Other.Centre, PostShape.Centre);
        if (*It != Post && Distance < Nearest)
        {
            Nearest = Distance;
            WallDirection = Other.Centre - PostShape.Centre;
        }
    }
    WallDirection.Z = 0.0f;
    if (!WallDirection.Normalize())
    {
        FVector Location;
        FRotator Rotation;
        GetView(Location, Rotation);
        WallDirection = FVector::CrossProduct(FVector::UpVector, Rotation.Vector()).GetSafeNormal2D();
    }
    const FVector Normal = FVector::CrossProduct(FVector::UpVector, WallDirection);
    TArray<ALogBuildPiece*> Removed;
    float Min = TNumericLimits<float>::Max(), Max = -TNumericLimits<float>::Max(), Bottom = PostShape.Centre.Z, Height = 1e6f;
    for (ALogBuildPiece* Candidate : Posts)
    {
        FLogGeometry Shape;
        GetLogGeometry(Candidate, Shape);
        const FVector Offset = Shape.Centre - PostShape.Centre;
        const float Along = FVector::DotProduct(Offset, WallDirection);
        if (FMath::Abs(Along) > 0.5f * GateWidth || FMath::Abs(FVector::DotProduct(Offset, Normal)) > 40.0f)
            continue;
        Removed.Add(Candidate);
        Min = FMath::Min(Min, Along - Shape.Radius);
        Max = FMath::Max(Max, Along + Shape.Radius);
        Bottom = FMath::Min(Bottom, Shape.Centre.Z - Shape.HalfLength);
        Height = FMath::Min(Height, Shape.Length());
    }
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    for (ALogBuildPiece* Candidate : Removed)
    {
        if (!Graph || !Graph->CanSafelyRemove(Candidate->GetStructuralNodeId()))
        {
            SetStatus(TEXT("A post here carries the wall alone; add a support before cutting a gate."));
            return false;
        }
    }
    for (ALogBuildPiece* Candidate : Removed)
        DismantlePiece(Candidate);
    const FVector Centre = PostShape.Centre + WallDirection * (0.5f * (Min + Max));
    OutOpening = GetWorld()->SpawnActor<AConstructionOpeningActor>(
        FVector(Centre.X, Centre.Y, Bottom + 0.5f * Height), WallDirection.Rotation());
    if (OutOpening)
        OutOpening->InitializeOpening(EConstructionOpeningKind::Gate, Max - Min, Height);
    SetStatus(FString::Printf(TEXT("Gate opening cut (%d posts out). Fill it with split boards."), Removed.Num()));
    return OutOpening != nullptr;
}

bool ULogBuildToolsComponent::CutOpening(ALogBuildPiece* WallLog, const FVector& AimPoint,
    EConstructionOpeningKind Kind, AConstructionOpeningActor*& OutOpening)
{
    OutOpening = nullptr;
    FLogGeometry Wall;
    if (!GetLogGeometry(WallLog, Wall) || !WallLog->IsPlaced())
    {
        SetStatus(TEXT("Aim at a wall."));
        return false;
    }
    if (FMath::Abs(Wall.Axis.Z) > 0.8f)
    {
        if (Kind == EConstructionOpeningKind::Window)
        {
            SetStatus(TEXT("Windows are cut into walls of lying logs."));
            return false;
        }
        return CutGate(WallLog, AimPoint, OutOpening);
    }
    if (FMath::Abs(Wall.Axis.Z) > 0.3f)
    {
        SetStatus(TEXT("Openings are cut into walls of lying logs."));
        return false;
    }

    const FVector Along = FVector(Wall.Axis.X, Wall.Axis.Y, 0.0f).GetSafeNormal();
    const FVector Normal = FVector::CrossProduct(FVector::UpVector, Along);
    const FVector Origin = Wall.Centre;
    const float Width = Kind == EConstructionOpeningKind::Window ? WindowWidth : DoorWidth;
    const float Height = Kind == EConstructionOpeningKind::Window ? WindowHeight : DoorHeight;
    const float Mid = FVector::DotProduct(AimPoint - Origin, Along);
    const float S0 = Mid - 0.5f * Width;
    const float S1 = Mid + 0.5f * Width;

    struct FWallLog { ALogBuildPiece* Log; FLogGeometry Shape; float Centre; float Sign; };
    TArray<FWallLog> WallLogs;
    float Base = TNumericLimits<float>::Max();
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        FLogGeometry Shape;
        if (!It->IsPlaced() || !GetLogGeometry(*It, Shape) || FMath::Abs(FVector::DotProduct(Shape.Axis, Along)) < 0.95f)
            continue;
        if (FMath::Abs(FVector::DotProduct(Shape.Centre - Origin, Normal)) > 1.2f * FMath::Max(Shape.Radius, Wall.Radius))
            continue;
        const float Centre = FVector::DotProduct(Shape.Centre - Origin, Along);
        if (Centre + Shape.HalfLength < S0 || Centre - Shape.HalfLength > S1)
            continue;
        WallLogs.Add({ *It, Shape, Centre, static_cast<float>(FMath::Sign(FVector::DotProduct(Shape.Axis, Along))) });
        Base = FMath::Min(Base, Shape.Centre.Z - Shape.Radius);
    }
    const float Z0 = Kind == EConstructionOpeningKind::Window ? Base + WindowSill : Base;
    const float Z1 = Z0 + Height;

    // Do not cut through a post that carries the wall.
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        FLogGeometry Shape;
        if (!It->IsPlaced() || !GetLogGeometry(*It, Shape) || FMath::Abs(Shape.Axis.Z) < 0.8f)
            continue;
        const float S = FVector::DotProduct(Shape.Centre - Origin, Along);
        const bool bInside = S > S0 - Shape.Radius && S < S1 + Shape.Radius &&
            FMath::Abs(FVector::DotProduct(Shape.Centre - Origin, Normal)) < 2.0f * Wall.Radius &&
            Shape.Centre.Z + Shape.HalfLength > Z0 && Shape.Centre.Z - Shape.HalfLength < Z1;
        if (bInside)
        {
            SetStatus(TEXT("The opening would cut a load-bearing post; move it along the wall."));
            return false;
        }
    }

    int32 Cut = 0;
    for (const FWallLog& Entry : WallLogs)
    {
        const float ZCentre = Entry.Shape.Centre.Z;
        if (ZCentre < Z0 || ZCentre > Z1)
            continue;
        const float A = Entry.Centre - Entry.Shape.HalfLength;
        const float B = Entry.Centre + Entry.Shape.HalfLength;
        const float OverlapFrom = FMath::Max(A, S0);
        const float OverlapTo = FMath::Min(B, S1);
        if (OverlapTo - OverlapFrom < 5.0f)
            continue;
        // Convert wall coordinates to the log's own along-axis coordinates.
        auto ToLog = [&Entry](float S) { return (S - Entry.Centre) * Entry.Sign; };
        const bool bLeft = OverlapFrom - A >= 25.0f;
        const bool bRight = B - OverlapTo >= 25.0f;

        // The cut-out piece drops loose in front of the wall, where it can be
        // picked up: useful as firewood or a board.
        if (ALogBuildPiece* Offcut = SpawnLogCopy(Entry.Log, Entry.Log->GetActorTransform()))
        {
            TrimLogToRange(Offcut, ToLog(OverlapFrom), ToLog(OverlapTo));
            const float Side = FVector::DotProduct(GetOwner()->GetActorLocation() - Origin, Normal) >= 0.0f ? 1.0f : -1.0f;
            const float Out = 2.5f * Entry.Shape.Radius + 40.0f + 2.2f * Entry.Shape.Radius * Cut;
            Offcut->SetActorLocation(Offcut->GetActorLocation() + Normal * Side * Out + FVector::UpVector * 5.0f,
                false, nullptr, ETeleportType::TeleportPhysics);
            if (UStaticMeshComponent* OffcutMesh = Offcut->GetLogMesh())
                OffcutMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        }
        if (bLeft && bRight)
        {
            if (ALogBuildPiece* RightPart = SpawnLogCopy(Entry.Log, Entry.Log->GetActorTransform()))
            {
                TrimLogToRange(RightPart, ToLog(OverlapTo), ToLog(B));
                MakePlaced(RightPart);
            }
            TrimLogToRange(Entry.Log, ToLog(A), ToLog(OverlapFrom));
        }
        else if (bLeft)
        {
            TrimLogToRange(Entry.Log, ToLog(A), ToLog(OverlapFrom));
        }
        else if (bRight)
        {
            TrimLogToRange(Entry.Log, ToLog(OverlapTo), ToLog(B));
        }
        else if (!DismantlePiece(Entry.Log))
        {
            continue;
        }
        ++Cut;
    }
    if (Cut == 0)
    {
        SetStatus(Kind == EConstructionOpeningKind::Window
            ? TEXT("No wall logs at window height here.") : TEXT("No wall logs to cut here."));
        return false;
    }
    const FVector Centre = Origin + Along * Mid;
    OutOpening = GetWorld()->SpawnActor<AConstructionOpeningActor>(
        FVector(Centre.X, Centre.Y, 0.5f * (Z0 + Z1)), Along.Rotation());
    if (OutOpening)
        OutOpening->InitializeOpening(Kind, Width, Height);
    SetStatus(Kind == EConstructionOpeningKind::Window
        ? FString::Printf(TEXT("Window cut through %d logs."), Cut)
        : FString::Printf(TEXT("Door opening cut through %d logs. Fill it with %d split boards."), Cut,
            OutOpening ? OutOpening->GetRequiredBoards() : 4));
    return OutOpening != nullptr;
}

// ---------------------------------------------------------------- ropes, fire

ARopeLineActor* ULogBuildToolsComponent::TieRope(const FVector& From, AActor* FromActor, const FVector& To, AActor* ToActor)
{
    const float Length = FVector::Dist(From, To);
    if (Length < 100.0f || Length > 3000.0f || !GetWorld())
    {
        SetStatus(TEXT("A rope can span 1 to 30 m."));
        return nullptr;
    }
    ARopeLineActor* Rope = GetWorld()->SpawnActor<ARopeLineActor>(0.5f * (From + To), FRotator::ZeroRotator);
    if (!Rope)
        return nullptr;
    Rope->InitializeRope(From, FromActor, To, ToActor);
    SetStatus(Rope->IsZipline()
        ? FString::Printf(TEXT("Zipline tied (%.0f deg): carry a log to its top end and press E."), Rope->GetSlopeDegrees())
        : TEXT("Rope tied: tie a second one alongside and lay boards across in Floor mode for a bridge."));
    return Rope;
}

bool ULogBuildToolsComponent::FeedFire(AActor* Fire)
{
    ULogCarryComponent* Carry = GetCarry();
    UHeatSourceComponent* Heat = Fire ? Fire->FindComponentByClass<UHeatSourceComponent>() : nullptr;
    ALogBuildPiece* Wood = Carry ? Carry->HeldPiece.Get() : nullptr;
    if (!Heat || !Wood)
    {
        SetStatus(TEXT("Carry wood to feed the fire."));
        return true;
    }
    if (!IsSmallWoodPiece(Wood))
    {
        SetStatus(TEXT("Too big for the fire: split or cut it to 1/4 length first."));
        return true;
    }
    const float Seconds = EstimateMassKg(Wood) * FuelSecondsPerKg;
    Carry->ReleaseHeldToTool();
    Heat->AddFuel(Seconds);
    ConsumePiece(Wood);
    SetStatus(FString::Printf(TEXT("Fed the fire: +%.0f min."), Seconds / 60.0f));
    return true;
}

// ---------------------------------------------------------------- input

void ULogBuildToolsComponent::SharpenInput()
{
    FHitResult Hit;
    if (TraceView(InteractRange, Hit))
        SharpenLog(Cast<ALogBuildPiece>(Hit.GetActor()));
}

void ULogBuildToolsComponent::DoorInput()
{
    FHitResult Hit;
    AConstructionOpeningActor* Opening = nullptr;
    if (TraceView(InteractRange, Hit))
        CutOpening(Cast<ALogBuildPiece>(Hit.GetActor()), Hit.ImpactPoint, EConstructionOpeningKind::Door, Opening);
}

void ULogBuildToolsComponent::WindowInput()
{
    FHitResult Hit;
    AConstructionOpeningActor* Opening = nullptr;
    if (TraceView(InteractRange, Hit))
        CutOpening(Cast<ALogBuildPiece>(Hit.GetActor()), Hit.ImpactPoint, EConstructionOpeningKind::Window, Opening);
}

void ULogBuildToolsComponent::LockInput()
{
    FHitResult Hit;
    AConstructionOpeningActor* Opening = TraceView(InteractRange, Hit) ? FindOpening(Hit) : nullptr;
    FString Reason;
    if (!Opening)
        SetStatus(TEXT("Aim at a door or gate."));
    else if (Opening->GetBar())
    {
        if (AActor* Stick = Opening->RemoveBar(GetOwner()->GetActorLocation(), Reason))
        {
            if (UWorldObjectIdentityComponent* Identity = Stick->FindComponentByClass<UWorldObjectIdentityComponent>())
            {
                if (Identity->GetLifecycleState() == EWorldObjectLifecycleState::Placed)
                    Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::Placed, EWorldObjectLifecycleState::World);
            }
            DropReleasedPiece(Stick);
            SetStatus(TEXT("Bar taken off: the door is open to use."), 2.0f);
        }
        else
            SetStatus(Reason);
    }
    else if (Opening->IsLocked() && Opening->ToggleLock(GetOwner()->GetActorLocation(), Reason))
        SetStatus(TEXT("Unlocked."), 1.5f);
    else
        SetStatus(TEXT("Carry a stick and press E on the closed door from the inside to bar it."));
}

void ULogBuildToolsComponent::RopeInput()
{
    FHitResult Hit;
    if (!TraceView(3000.0f, Hit))
    {
        bRopeStarted = false;
        SetStatus(TEXT("Rope cancelled."));
        return;
    }
    if (!bRopeStarted)
    {
        bRopeStarted = true;
        RopeStart = Hit.ImpactPoint;
        RopeStartActor = Hit.GetActor();
        SetStatus(TEXT("Rope tied here: aim at the other end and press H."), 30.0f);
        return;
    }
    bRopeStarted = false;
    TieRope(RopeStart, RopeStartActor.Get(), Hit.ImpactPoint, Hit.GetActor());
}

void ULogBuildToolsComponent::TowInput()
{
    FHitResult Hit;
    ALogContainerActor* Sled = TraceView(InteractRange, Hit) ? FindContainer(Hit) : nullptr;
    if (!Sled)
    {
        for (TActorIterator<ALogContainerActor> It(GetWorld()); It; ++It)
        {
            if (It->IsTowed())
                Sled = *It;
        }
    }
    if (!Sled || !Sled->IsSled())
    {
        SetStatus(TEXT("Aim at a sled."));
        return;
    }
    Sled->ToggleTow(Cast<APawn>(GetOwner()));
    SetStatus(Sled->IsTowed() ? TEXT("Pulling the sled (T to let go).") : TEXT("Sled released."), 2.0f);
}

void ULogBuildToolsComponent::CyclePlanInput()
{
    SelectedPlan = static_cast<EBuildPlanKind>((static_cast<uint8>(SelectedPlan) + 1) %
        (static_cast<uint8>(ABuildPlanActor::LastKind) + 1));
    SetStatus(FString::Printf(TEXT("Plan: %s (Y to place)."), *ABuildPlanActor::GetPlanName(SelectedPlan)));
}

void ULogBuildToolsComponent::PlacePlanInput()
{
    FHitResult Hit;
    if (!TraceView(1500.0f, Hit))
        return;
    if (ABuildPlanActor* Plan = Cast<ABuildPlanActor>(Hit.GetActor()))
    {
        Plan->Destroy();
        SetStatus(TEXT("Plan removed (built parts stay)."));
        return;
    }
    FVector Location;
    FRotator Rotation;
    GetView(Location, Rotation);
    if (SelectedPlan == EBuildPlanKind::DefenseWall)
    {
        // Draw the wall: Y at the start of the line, Y again at its end.
        if (!bWallLineStarted)
        {
            bWallLineStarted = true;
            WallLineStart = Hit.ImpactPoint;
            SetStatus(TEXT("Defensive wall: aim at the end of the line and press Y."), 8.0f);
            return;
        }
        bWallLineStarted = false;
        if (ABuildPlanActor* Line = PlaceDefenseLine(WallLineStart, Hit.ImpactPoint))
            SetStatus(FString::Printf(TEXT("Defensive wall drawn: %d stakes. Carry logs in, or tell the helper (J) to finish it."),
                Line->GetSlotCount()));
        else
            SetStatus(TEXT("The line is too short."));
        return;
    }
    if (PlacePlan(SelectedPlan, Hit.ImpactPoint, Rotation.Yaw))
        SetStatus(FString::Printf(TEXT("%s plan placed: carry logs into the ghost parts."), *ABuildPlanActor::GetPlanName(SelectedPlan)));
    else if (ABuildPlanActor::NeedsTree(SelectedPlan))
        SetStatus(TEXT("Tree plans go around a standing tree: aim near one."));
}

void ULogBuildToolsComponent::DismantleInput()
{
    FHitResult Hit;
    if (TraceView(InteractRange, Hit))
        DismantlePiece(Cast<ALogBuildPiece>(Hit.GetActor()));
}

void ULogBuildToolsComponent::ThrowInput()
{
    if (ULogCarryComponent* Carry = GetCarry())
    {
        if (Carry->ThrowHeldPiece())
            SetStatus(TEXT("Thrown."), 1.0f);
    }
}

void ULogBuildToolsComponent::HelpInput()
{
    bShowHelp = !bShowHelp;
}
