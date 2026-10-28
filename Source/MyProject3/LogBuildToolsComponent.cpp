#include "LogBuildToolsComponent.h"

#include "ConstructionHelpers.h"
#include "HeatSourceComponent.h"
#include "LogAttachmentComponents.h"
#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "LogContainerActor.h"
#include "RopeLineActor.h"
#include "StructuralPieceInterface.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"

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
        "C choose cut   F cut aimed log   V push/roll   B drag\n"
        "X sharpen standing log   K door / gate opening   P window opening   L lock door (inside)\n"
        "Delete remove a support   H tie rope (twice; sloped rope = zipline)\n"
        "Z choose building plan   Y place / remove plan   T pull or release sled\n"
        "Carrying a log: E on storage/sled stores it, on a fire burns it, on a zipline top sends it\n"
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
                if (!IsSplitBoard(HeldLog))
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
        SetStatus(Reason);
        return true;
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
    SetStatus(FString::Printf(TEXT("%s placed.%s"), bStairs ? TEXT("Step") : TEXT("Board"), *Note), 2.5f);
    return true;
}

// ---------------------------------------------------------------- plans

ABuildPlanActor* ULogBuildToolsComponent::PlacePlan(EBuildPlanKind Kind, const FVector& Location, float Yaw)
{
    if (!GetWorld())
        return nullptr;
    ABuildPlanActor* Plan = GetWorld()->SpawnActor<ABuildPlanActor>(Location, FRotator(0.0f, Yaw, 0.0f));
    if (Plan)
        Plan->InitializePlan(Kind);
    return Plan;
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
    const FTransform PreCutTransform = Log->GetActorTransform();
    const bool bPreCutCustomLengthScale = Log->bHasCustomLengthScale;
    ALogBuildPiece* Remainder = nullptr;
    if (Shape.Length() > Needed * 1.1f)
    {
        Remainder = CutOffRemainder(Log, Needed,
            GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 80.0f);
        if (!Remainder)
        {
            SetStatus(TEXT("Cut rejected: the required length could not be produced safely."));
            return true;
        }
        Note = FString::Printf(TEXT(" Cut to %.0f cm."), Needed);
    }
    const FTransform SlotWorld = Plan->GetSlotWorld(SlotIndex);
    const FTransform Target = MakeLogTransform(Log, SlotWorld.GetLocation(),
        MakeLogRotation(Log, SlotWorld.GetRotation().GetAxisX(), SlotWorld.GetRotation().GetAxisZ()));
    if (!Carry->CommitHeldLogAt(Target, Plan->GetSupportPiece(SlotIndex), nullptr))
    {
        // If the cut succeeded but placement did not, restore the source geometry
        // and retire the staged offcut; the plan must not consume a partial cut.
        if (IsValid(Remainder))
            Remainder->Destroy();
        if (IsValid(Log) && !Log->IsActorBeingDestroyed())
        {
            Log->SetActorTransform(PreCutTransform, false, nullptr, ETeleportType::TeleportPhysics);
            Log->bHasCustomLengthScale = bPreCutCustomLengthScale;
        }
        SetStatus(TEXT("The log could not be placed; the cut was rolled back."));
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
    OutOpening = nullptr;
    FLogGeometry PostShape;
    UWorld* World = GetWorld();
    UWorldStructuralGraphSubsystem* Graph = World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!IsValid(Post) || !World || !Graph || !Ownership ||
        !GetLogGeometry(Post, PostShape) || !Post->IsPlaced() ||
        !FMath::IsFinite(AimPoint.X) || !FMath::IsFinite(AimPoint.Y) || !FMath::IsFinite(AimPoint.Z))
    {
        SetStatus(TEXT("Aim at a placed post."));
        return false;
    }

    // The wall runs from this post to its nearest standing neighbour.
    FVector WallDirection = FVector::ZeroVector;
    float Nearest = 90.0f;
    TArray<ALogBuildPiece*> Posts;
    for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
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
        FVector ViewLocation;
        FRotator ViewRotation;
        GetView(ViewLocation, ViewRotation);
        WallDirection = FVector::CrossProduct(FVector::UpVector, ViewRotation.Vector()).GetSafeNormal2D();
    }
    if (WallDirection.IsNearlyZero())
    {
        SetStatus(TEXT("Could not determine a valid wall direction for this gate."));
        return false;
    }

    const FVector Normal = FVector::CrossProduct(FVector::UpVector, WallDirection);
    TArray<ALogBuildPiece*> Removed;
    float Min = TNumericLimits<float>::Max();
    float Max = -TNumericLimits<float>::Max();
    float Bottom = PostShape.Centre.Z;
    float Height = TNumericLimits<float>::Max();
    for (ALogBuildPiece* Candidate : Posts)
    {
        FLogGeometry Shape;
        if (!GetLogGeometry(Candidate, Shape))
            continue;
        const FVector Offset = Shape.Centre - PostShape.Centre;
        const float Along = FVector::DotProduct(Offset, WallDirection);
        if (FMath::Abs(Along) > 0.5f * GateWidth ||
            FMath::Abs(FVector::DotProduct(Offset, Normal)) > 40.0f)
            continue;
        Removed.Add(Candidate);
        Min = FMath::Min(Min, Along - Shape.Radius);
        Max = FMath::Max(Max, Along + Shape.Radius);
        Bottom = FMath::Min(Bottom, Shape.Centre.Z - Shape.HalfLength);
        Height = FMath::Min(Height, Shape.Length());
    }

    if (Removed.IsEmpty() || !FMath::IsFinite(Min) || !FMath::IsFinite(Max) ||
        Max <= Min || !FMath::IsFinite(Bottom) || !FMath::IsFinite(Height) || Height <= 0.0f)
    {
        SetStatus(TEXT("No suitable posts were found for a gate opening here."));
        return false;
    }

    struct FPostBefore
    {
        ALogBuildPiece* Piece = nullptr;
        FTransform Transform = FTransform::Identity;
        EWorldObjectLifecycleState Lifecycle = EWorldObjectLifecycleState::World;
        int64 LifecycleRevision = 0;
        bool bCustomLength = false;
        bool bPlaced = false;
        bool bHeld = false;
    };
    TArray<FPostBefore> BeforePosts;
    TSet<FGuid> RemovedIds;
    for (ALogBuildPiece* Candidate : Removed)
    {
        UWorldObjectIdentityComponent* Identity =
            Candidate ? Candidate->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
        IStructuralPieceInterface* Structural = Cast<IStructuralPieceInterface>(Candidate);
        FWorldOwnedItemRecord Item;
        if (!IsValid(Candidate) || !Identity || !Identity->HasValidIdentity() ||
            !Structural || Identity->GetLifecycleState() != EWorldObjectLifecycleState::Placed ||
            Identity->StateRevision == MAX_int64 ||
            !Ownership->TryGetItem(Structural->GetStructuralNodeId(), Item) ||
            Item.OwnerKind != EWorldItemOwnerKind::World || Item.Quantity != 1 ||
            Item.Revision == MAX_int64 || !Graph->CanSafelyRemove(Structural->GetStructuralNodeId()))
        {
            SetStatus(TEXT("A post here cannot be removed safely; add another support before cutting a gate."));
            return false;
        }

        FPostBefore& State = BeforePosts.AddDefaulted_GetRef();
        State.Piece = Candidate;
        State.Transform = Candidate->GetActorTransform();
        State.Lifecycle = Identity->GetLifecycleState();
        State.LifecycleRevision = Identity->StateRevision;
        State.bCustomLength = Candidate->bHasCustomLengthScale;
        State.bPlaced = Candidate->IsPlaced();
        State.bHeld = Candidate->IsHeld();
        RemovedIds.Add(Structural->GetStructuralNodeId());
    }

    // CanSafelyRemove() preflights every post. DismantlePiece() rechecks after
    // each prior removal; if a group removal makes a later post unsafe, the full
    // transaction below restores the original graph and actor-local links.

    TArray<FWorldStructuralNodeSnapshot> GraphBefore;
    if (!Graph->ExportSnapshot(GraphBefore))
    {
        SetStatus(TEXT("Gate cut blocked: structural snapshot could not be captured."));
        return false;
    }
    TArray<FWorldOwnedItemRecord> ItemsBefore;
    TArray<FWorldItemTransferReceipt> TransfersBefore;
    TArray<FWorldItemSplitReceipt> SplitsBefore;
    Ownership->ExportSnapshot(ItemsBefore, TransfersBefore, SplitsBefore);

    const FVector Centre = PostShape.Centre + WallDirection * (0.5f * (Min + Max));
    const FVector OpeningLocation(Centre.X, Centre.Y, Bottom + 0.5f * Height);
    AConstructionOpeningActor* StagedOpening = World->SpawnActor<AConstructionOpeningActor>(
        OpeningLocation, WallDirection.Rotation());
    if (!IsValid(StagedOpening))
    {
        SetStatus(TEXT("Gate cut failed: could not create the opening frame; posts were not changed."));
        return false;
    }
    StagedOpening->InitializeOpening(EConstructionOpeningKind::Gate, Max - Min, Height);

    auto Rollback = [&]() -> bool
    {
        if (IsValid(StagedOpening))
            StagedOpening->Destroy();

        // Local transforms/states are restored before the canonical graph. The
        // graph restore and adapter rebuild then reestablish actor-local links.
        for (const FPostBefore& State : BeforePosts)
        {
            if (!IsValid(State.Piece))
                continue;
            State.Piece->SetActorTransform(State.Transform, false, nullptr, ETeleportType::TeleportPhysics);
            State.Piece->bHasCustomLengthScale = State.bCustomLength;
            State.Piece->SetHeldState(State.bHeld);
            State.Piece->SetPlacedState(State.bPlaced);
            if (UWorldObjectIdentityComponent* Identity =
                State.Piece->FindComponentByClass<UWorldObjectIdentityComponent>())
            {
                Identity->LifecycleState = State.Lifecycle;
                Identity->StateRevision = State.LifecycleRevision;
            }
        }

        const bool bItemsRestored = Ownership->RestoreSnapshot(ItemsBefore, TransfersBefore, SplitsBefore);
        const bool bGraphRestored = Graph->RestoreSnapshot(GraphBefore);
        const bool bLinksRestored = bGraphRestored && Graph->RebuildLogActorAttachmentsFromSnapshot();
        if (!bItemsRestored || !bGraphRestored || !bLinksRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Gate cut rollback failed; structural state needs recovery."));
            if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
            {
                Coordinator->MarkSimulationFaulted(TEXT("Gate cut failed and rollback was incomplete; stop simulation and recover/reset the world."));
            }
            SetStatus(TEXT("Gate cut failed and rollback was incomplete; reload/recover the world."));
            return false;
        }
        return true;
    };

    for (ALogBuildPiece* Candidate : Removed)
    {
        if (!DismantlePiece(Candidate))
        {
            Rollback();
            SetStatus(TEXT("Gate cut failed while removing a post; original structure was restored."));
            return false;
        }
    }

    OutOpening = StagedOpening;
    SetStatus(FString::Printf(TEXT("Gate opening cut (%d posts out). Fill it with split boards."), Removed.Num()));
    return true;
}
bool ULogBuildToolsComponent::CutOpening(ALogBuildPiece* WallLog, const FVector& AimPoint,
    EConstructionOpeningKind Kind, AConstructionOpeningActor*& OutOpening)
{
    OutOpening = nullptr;
    FLogGeometry Wall;
    UWorld* World = GetWorld();
    UWorldStructuralGraphSubsystem* Graph = World ? World->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership = World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    if (!World || !Graph || !Ownership || !GetOwner() ||
        !GetLogGeometry(WallLog, Wall) || !WallLog->IsPlaced())
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
    if (Along.IsNearlyZero() || !FMath::IsFinite(AimPoint.X) ||
        !FMath::IsFinite(AimPoint.Y) || !FMath::IsFinite(AimPoint.Z))
        return false;
    const FVector Normal = FVector::CrossProduct(FVector::UpVector, Along);
    const FVector Origin = Wall.Centre;
    const float Width = Kind == EConstructionOpeningKind::Window ? WindowWidth : DoorWidth;
    const float Height = Kind == EConstructionOpeningKind::Window ? WindowHeight : DoorHeight;
    if (!FMath::IsFinite(Width) || !FMath::IsFinite(Height) || Width <= 0.0f || Height <= 0.0f)
        return false;
    const float Mid = FVector::DotProduct(AimPoint - Origin, Along);
    const float S0 = Mid - 0.5f * Width;
    const float S1 = Mid + 0.5f * Width;

    struct FWallLog
    {
        ALogBuildPiece* Log = nullptr;
        FLogGeometry Shape;
        float Centre = 0.0f;
        float Sign = 1.0f;
        FTransform OriginalTransform;
        EWorldObjectLifecycleState OriginalLifecycle = EWorldObjectLifecycleState::World;
        int64 OriginalLifecycleRevision = 0;
        bool bOriginalPlaced = false;
        bool bOriginalHeld = false;
        bool bOriginalCustomLengthScale = false;
    };
    TArray<FWallLog> WallLogs;
    float Base = TNumericLimits<float>::Max();
    for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
    {
        FLogGeometry Shape;
        if (!It->IsPlaced() || !GetLogGeometry(*It, Shape) ||
            FMath::Abs(FVector::DotProduct(Shape.Axis, Along)) < 0.95f)
            continue;
        if (FMath::Abs(FVector::DotProduct(Shape.Centre - Origin, Normal)) >
            1.2f * FMath::Max(Shape.Radius, Wall.Radius))
            continue;
        const float Centre = FVector::DotProduct(Shape.Centre - Origin, Along);
        if (Centre + Shape.HalfLength < S0 || Centre - Shape.HalfLength > S1)
            continue;

        FWallLog& Entry = WallLogs.AddDefaulted_GetRef();
        Entry.Log = *It;
        Entry.Shape = Shape;
        Entry.Centre = Centre;
        Entry.Sign = static_cast<float>(FMath::Sign(FVector::DotProduct(Shape.Axis, Along)));
        Entry.OriginalTransform = It->GetActorTransform();
        if (UWorldObjectIdentityComponent* Identity = It->FindComponentByClass<UWorldObjectIdentityComponent>())
        {
            Entry.OriginalLifecycle = Identity->GetLifecycleState();
            Entry.OriginalLifecycleRevision = Identity->StateRevision;
        }
        Entry.bOriginalPlaced = It->IsPlaced();
        Entry.bOriginalHeld = It->IsHeld();
        Entry.bOriginalCustomLengthScale = It->bHasCustomLengthScale;
        Base = FMath::Min(Base, Shape.Centre.Z - Shape.Radius);
    }
    if (WallLogs.IsEmpty())
    {
        SetStatus(Kind == EConstructionOpeningKind::Window
            ? TEXT("No wall logs at window height here.") : TEXT("No wall logs to cut here."));
        return false;
    }
    const float Z0 = Kind == EConstructionOpeningKind::Window ? Base + WindowSill : Base;
    const float Z1 = Z0 + Height;

    // Preflight the structural post rule and every source that would be wholly removed.
    for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
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
        const bool bLeft = OverlapFrom - A >= 25.0f;
        const bool bRight = B - OverlapTo >= 25.0f;
        if (!bLeft && !bRight && !Graph->CanSafelyRemove(Entry.Log->GetStructuralNodeId()))
        {
            SetStatus(TEXT("The opening would remove a log that is still the only support; add another support first."));
            return false;
        }
    }

    TArray<FWorldStructuralNodeSnapshot> GraphBefore;
    if (!Graph->ExportSnapshot(GraphBefore))
    {
        SetStatus(TEXT("Opening cut blocked: structural snapshot could not be captured."));
        return false;
    }
    TArray<FWorldOwnedItemRecord> ItemsBefore;
    TArray<FWorldItemTransferReceipt> TransfersBefore;
    TArray<FWorldItemSplitReceipt> SplitsBefore;
    Ownership->ExportSnapshot(ItemsBefore, TransfersBefore, SplitsBefore);

    TArray<TWeakObjectPtr<ALogBuildPiece>> SpawnedPieces;
    AConstructionOpeningActor* StagedOpening = nullptr;
    auto Rollback = [&]() -> bool
    {
        if (IsValid(StagedOpening))
            StagedOpening->Destroy();
        for (int32 Index = SpawnedPieces.Num() - 1; Index >= 0; --Index)
        {
            if (ALogBuildPiece* Spawned = SpawnedPieces[Index].Get())
                Spawned->Destroy();
        }

        for (const FWallLog& Entry : WallLogs)
        {
            if (!IsValid(Entry.Log))
                continue;
            Entry.Log->SetActorTransform(Entry.OriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);
            if (UWorldObjectIdentityComponent* Identity =
                Entry.Log->FindComponentByClass<UWorldObjectIdentityComponent>())
            {
                Identity->LifecycleState = Entry.OriginalLifecycle;
                Identity->StateRevision = Entry.OriginalLifecycleRevision;
            }
            Entry.Log->bHasCustomLengthScale = Entry.bOriginalCustomLengthScale;
            Entry.Log->SetHeldState(Entry.bOriginalHeld);
            Entry.Log->SetPlacedState(Entry.bOriginalPlaced);
        }

        const bool bItemsRestored = Ownership->RestoreSnapshot(ItemsBefore, TransfersBefore, SplitsBefore);
        const bool bGraphRestored = Graph->RestoreSnapshot(GraphBefore);
        const bool bAttachmentsRestored = bGraphRestored && Graph->RebuildLogActorAttachmentsFromSnapshot();
        if (!bItemsRestored || !bGraphRestored || !bAttachmentsRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Door/window cut rollback failed; world structure requires recovery."));
            SetStatus(TEXT("Cut failed and rollback was incomplete; stop structural edits and reload/recover the world."));
            return false;
        }
        return true;
    };

    int32 Cut = 0;
    for (const FWallLog& Entry : WallLogs)
    {
        if (!IsValid(Entry.Log))
        {
            Rollback();
            return false;
        }
        const float ZCentre = Entry.Shape.Centre.Z;
        if (ZCentre < Z0 || ZCentre > Z1)
            continue;

        const float A = Entry.Centre - Entry.Shape.HalfLength;
        const float B = Entry.Centre + Entry.Shape.HalfLength;
        const float OverlapFrom = FMath::Max(A, S0);
        const float OverlapTo = FMath::Min(B, S1);
        if (OverlapTo - OverlapFrom < 5.0f)
            continue;

        auto ToLog = [&Entry](float S) { return (S - Entry.Centre) * Entry.Sign; };
        const bool bLeft = OverlapFrom - A >= 25.0f;
        const bool bRight = B - OverlapTo >= 25.0f;

        ALogBuildPiece* Offcut = nullptr;
        if (bLeft || bRight)
        {
            Offcut = SpawnLogCopy(Entry.Log, Entry.OriginalTransform);
            if (!Offcut)
            {
                Rollback();
                SetStatus(TEXT("Cut failed: could not create an offcut; original wall restored."));
                return false;
            }
            SpawnedPieces.Add(Offcut);
            if (!TrimLogToRange(Offcut, ToLog(OverlapFrom), ToLog(OverlapTo)))
            {
                Rollback();
                SetStatus(TEXT("Cut failed: offcut geometry could not be trimmed; original wall restored."));
                return false;
            }
        }
        else
        {
            // The original piece itself becomes the whole offcut. Copying it here
            // would duplicate the resource while leaving the full source in the world.
            Offcut = Entry.Log;
        }

        if (bLeft && bRight)
        {
            ALogBuildPiece* RightPart = SpawnLogCopy(Entry.Log, Entry.OriginalTransform);
            if (!RightPart)
            {
                Rollback();
                SetStatus(TEXT("Cut failed: could not create the remaining right-hand piece."));
                return false;
            }
            SpawnedPieces.Add(RightPart);
            if (!TrimLogToRange(RightPart, ToLog(OverlapTo), ToLog(B)))
            {
                Rollback();
                SetStatus(TEXT("Cut failed: right-hand geometry could not be trimmed."));
                return false;
            }
            MakePlaced(RightPart);
            if (!RightPart->IsPlaced() ||
                !TrimLogToRange(Entry.Log, ToLog(A), ToLog(OverlapFrom)))
            {
                Rollback();
                SetStatus(TEXT("Cut failed: source or right-hand segment could not be committed."));
                return false;
            }
        }
        else if (bLeft)
        {
            if (!TrimLogToRange(Entry.Log, ToLog(A), ToLog(OverlapFrom)))
            {
                Rollback();
                SetStatus(TEXT("Cut failed: left-hand source segment could not be trimmed."));
                return false;
            }
        }
        else if (bRight)
        {
            if (!TrimLogToRange(Entry.Log, ToLog(OverlapTo), ToLog(B)))
            {
                Rollback();
                SetStatus(TEXT("Cut failed: right-hand source segment could not be trimmed."));
                return false;
            }
        }
        else if (!DismantlePiece(Entry.Log))
        {
            Rollback();
            SetStatus(TEXT("Cut failed: the source log could not be safely removed from its support."));
            return false;
        }

        const float Side = FVector::DotProduct(GetOwner()->GetActorLocation() - Origin, Normal) >= 0.0f ? 1.0f : -1.0f;
        const float Out = 2.5f * Entry.Shape.Radius + 40.0f + 2.2f * Entry.Shape.Radius * Cut;
        if (Offcut != Entry.Log)
        {
            Offcut->SetActorLocation(Offcut->GetActorLocation() + Normal * Side * Out + FVector::UpVector * 5.0f,
                false, nullptr, ETeleportType::TeleportPhysics);
        }
        else
        {
            Offcut->SetActorLocation(Offcut->GetActorLocation() + Normal * Side * Out + FVector::UpVector * 5.0f,
                false, nullptr, ETeleportType::TeleportPhysics);
            Offcut->SetPlacedState(false);
        }
        if (UStaticMeshComponent* OffcutMesh = Offcut->GetLogMesh())
            OffcutMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        ++Cut;
    }

    if (Cut == 0)
    {
        SetStatus(Kind == EConstructionOpeningKind::Window
            ? TEXT("No wall logs at window height here.") : TEXT("No wall logs to cut here."));
        return false;
    }

    const FVector Centre = Origin + Along * Mid;
    StagedOpening = World->SpawnActor<AConstructionOpeningActor>(
        FVector(Centre.X, Centre.Y, 0.5f * (Z0 + Z1)), Along.Rotation());
    if (!StagedOpening)
    {
        Rollback();
        SetStatus(TEXT("Cut failed: opening frame could not be created; original wall restored."));
        return false;
    }
    StagedOpening->InitializeOpening(Kind, Width, Height);
    OutOpening = StagedOpening;

    SetStatus(Kind == EConstructionOpeningKind::Window
        ? FString::Printf(TEXT("Window cut through %d logs."), Cut)
        : FString::Printf(TEXT("Door opening cut through %d logs. Fill it with %d split boards."), Cut,
            OutOpening->GetRequiredBoards()));
    return true;
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
    else if (Opening->ToggleLock(GetOwner()->GetActorLocation(), Reason))
        SetStatus(Opening->IsLocked() ? TEXT("Locked.") : TEXT("Unlocked."), 1.5f);
    else
        SetStatus(Reason);
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
        (static_cast<uint8>(EBuildPlanKind::DefenseWall) + 1));
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
    if (PlacePlan(SelectedPlan, Hit.ImpactPoint, Rotation.Yaw))
        SetStatus(FString::Printf(TEXT("%s plan placed: carry logs into the ghost parts."), *ABuildPlanActor::GetPlanName(SelectedPlan)));
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
