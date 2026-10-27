#include "HelperCompanionActor.h"

#include "BuildPlanActor.h"
#include "HarvestableTreeActor.h"
#include "LogAttachmentComponents.h"
#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "LogContainerActor.h"
#include "StructuralPieceInterface.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

using namespace MyProject3Construction;

AHelperCompanionActor::AHelperCompanionActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Feet")));

    // A plain blockout figure: a body and a head. It only blocks traces, so the
    // player can aim at him but he never shoves logs or the player around.
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(GetRootComponent());
    Body->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
    Body->SetRelativeScale3D(FVector(0.45f, 0.38f, 1.6f));
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
    Head->SetupAttachment(GetRootComponent());
    Head->SetRelativeLocation(FVector(0.0f, 0.0f, 178.0f));
    Head->SetRelativeScale3D(FVector(0.32f));
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CompanionId = FGuid::NewGuid();
}

void AHelperCompanionActor::BeginPlay()
{
    Super::BeginPlay();
    // Blockout look from the engine's basic shapes.
    if (!Body->GetStaticMesh())
    {
        Body->SetStaticMesh(LoadEngineShape(TEXT("Cylinder")));
        Body->SetMaterial(0, MakeTint(this, FLinearColor(0.12f, 0.16f, 0.22f)));
    }
    if (!Head->GetStaticMesh())
    {
        Head->SetStaticMesh(LoadEngineShape(TEXT("Sphere")));
        Head->SetMaterial(0, MakeTint(this, FLinearColor(0.55f, 0.4f, 0.3f)));
    }
    Status = GetOrderName(Order);
}

FString AHelperCompanionActor::GetOrderName(ECompanionOrder InOrder)
{
    const UEnum* Enum = StaticEnum<ECompanionOrder>();
    return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(InOrder)).ToString() : TEXT("Order");
}

void AHelperCompanionActor::SetOrder(ECompanionOrder NewOrder)
{
    Order = NewOrder;
    Target.Reset();
    TargetPlan.Reset();
    TargetSlot = INDEX_NONE;
    ThinkTimer = 0.0f;
    TargetTree.Reset();
    TargetStake.Reset();
    Status = GetOrderName(Order);
    if (Order == ECompanionOrder::PileLogsHere)
    {
        // The pile goes where the player stood when giving the order.
        APawn* Player = FindPlayerPawn(GetWorld());
        PilePoint = Player ? Player->GetActorLocation() : GetActorLocation();
        PileCount = 0;
    }
}

void AHelperCompanionActor::CycleOrder()
{
    SetOrder(static_cast<ECompanionOrder>((static_cast<uint8>(Order) + 1) %
        (static_cast<uint8>(ECompanionOrder::Stay) + 1)));
}

// ---------------------------------------------------------------- movement

bool AHelperCompanionActor::MoveTowards(const FVector& Goal, float DeltaSeconds, float AcceptRadius)
{
    const FVector Here = GetActorLocation();
    FVector Flat = Goal - Here;
    Flat.Z = 0.0f;
    const float Distance = Flat.Size();
    if (Distance <= AcceptRadius)
        return true;

    const FVector Direction = Flat / Distance;
    FVector Next = Here + Direction * FMath::Min(Distance, WalkSpeed * DeltaSeconds);

    // Stay on whatever is underfoot: ground, a log, a deck.
    FHitResult Ground;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HelperGround), false, this);
    if (ALogBuildPiece* Log = Carried.Get())
        Params.AddIgnoredActor(Log);
    if (APawn* Player = FindPlayerPawn(GetWorld()))
        Params.AddIgnoredActor(Player);
    if (GetWorld()->LineTraceSingleByChannel(Ground, Next + FVector(0, 0, 150.0f), Next - FVector(0, 0, 1200.0f),
        ECC_Visibility, Params))
    {
        Next.Z = Ground.ImpactPoint.Z;
    }
    SetActorLocationAndRotation(Next, FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
    return FVector::Dist2D(Next, Goal) <= AcceptRadius;
}

void AHelperCompanionActor::FollowPlayer(float DeltaSeconds, float Distance)
{
    if (APawn* Player = FindPlayerPawn(GetWorld()))
        MoveTowards(Player->GetActorLocation(), DeltaSeconds, Distance);
}

// ---------------------------------------------------------------- finding work

bool AHelperCompanionActor::IsLooseLog(const ALogBuildPiece* Log) const
{
    EWorldItemOwnerKind ItemOwner = EWorldItemOwnerKind::World;
    return IsValid(Log) && !Log->IsHeld() && !Log->IsPlaced() && !Log->GetAttachParentActor() &&
        Log->GetActorLocation().Z > MinLogHeight &&
        GetPieceOwnerKind(Log, ItemOwner) && ItemOwner == EWorldItemOwnerKind::World;
}

ALogContainerActor* AHelperCompanionActor::FindStorageFor(const AActor* Piece) const
{
    ALogContainerActor* Best = nullptr;
    float BestDistance = SearchRadius;
    for (TActorIterator<ALogContainerActor> It(GetWorld()); It; ++It)
    {
        FString Reason;
        const float Distance = FVector::Dist(It->GetActorLocation(), GetActorLocation());
        // Storage, or a sled nobody is pulling (Kelvin helps fill sleds).
        if ((It->IsSled() && It->IsTowed()) || Distance >= BestDistance || !It->CanAccept(Piece, Reason))
            continue;
        Best = *It;
        BestDistance = Distance;
    }
    return Best;
}

ALogBuildPiece* AHelperCompanionActor::FindLooseLog(float MinLength, bool bNeedStorage, float AvoidPileRadius) const
{
    ALogBuildPiece* Best = nullptr;
    float BestDistance = SearchRadius;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        ALogBuildPiece* Log = *It;
        const float Distance = FVector::Dist(Log->GetActorLocation(), GetActorLocation());
        if (Distance >= BestDistance || !IsLooseLog(Log))
            continue;
        FLogGeometry Shape;
        if (MinLength > 0.0f && (!GetLogGeometry(Log, Shape) || Shape.Length() < MinLength))
            continue;
        if (bNeedStorage && !FindStorageFor(Log))
            continue;
        if (AvoidPileRadius > 0.0f && FVector::Dist2D(Log->GetActorLocation(), PilePoint) < AvoidPileRadius)
            continue;
        Best = Log;
        BestDistance = Distance;
    }
    return Best;
}

bool AHelperCompanionActor::FindOpenPlanSlot(ABuildPlanActor*& OutPlan, int32& OutSlot) const
{
    OutPlan = nullptr;
    OutSlot = INDEX_NONE;
    float BestDistance = SearchRadius;
    for (TActorIterator<ABuildPlanActor> It(GetWorld()); It; ++It)
    {
        const float Distance = FVector::Dist(It->GetActorLocation(), GetActorLocation());
        if (Distance >= BestDistance)
            continue;
        for (int32 Slot = 0; Slot < It->GetSlotCount(); ++Slot)
        {
            FString Reason;
            if (It->CanFillSlot(Slot, Reason))
            {
                OutPlan = *It;
                OutSlot = Slot;
                BestDistance = Distance;
                break;
            }
        }
    }
    return OutPlan != nullptr;
}

AHarvestableTreeActor* AHelperCompanionActor::FindTreeToChop() const
{
    // Only trees well away from anything built (tree platforms, walls) and from the player.
    TArray<FVector> Built;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        if (It->IsPlaced())
            Built.Add(It->GetActorLocation());
    }
    const APawn* Player = FindPlayerPawn(GetWorld());
    AHarvestableTreeActor* Best = nullptr;
    float BestDistance = SearchRadius;
    for (TActorIterator<AHarvestableTreeActor> It(GetWorld()); It; ++It)
    {
        AHarvestableTreeActor* Tree = *It;
        const FVector Location = Tree->GetActorLocation();
        const float Distance = FVector::Dist2D(Location, GetActorLocation());
        if (!Tree->CanBeChopped() || Distance >= BestDistance ||
            (Player && FVector::Dist2D(Location, Player->GetActorLocation()) < 400.0f))
            continue;
        bool bNearBuilt = false;
        for (const FVector& Piece : Built)
        {
            if (FVector::Dist2D(Piece, Location) < 500.0f)
            {
                bNearBuilt = true;
                break;
            }
        }
        if (bNearBuilt)
            continue;
        Best = Tree;
        BestDistance = Distance;
    }
    return Best;
}

bool AHelperCompanionActor::TickChop(float DeltaSeconds)
{
    if (!bMayChopTrees)
        return false;
    // A felled tree takes a while to settle before its logs can be picked up.
    if (SettleWait > 0.0f)
    {
        SettleWait -= DeltaSeconds;
        Status = TEXT("Waiting for the felled tree to settle.");
        return true;
    }
    AHarvestableTreeActor* Tree = TargetTree.Get();
    if (!IsValid(Tree) || !Tree->CanBeChopped())
    {
        Tree = FindTreeToChop();
        TargetTree = Tree;
    }
    if (!IsValid(Tree))
        return false;
    Status = TEXT("Chopping a tree for logs.");
    const FVector Trunk = Tree->GetActorLocation();
    if (!MoveTowards(Trunk, DeltaSeconds, 130.0f))
        return true;
    WorkTimer -= DeltaSeconds;
    if (WorkTimer > 0.0f)
        return true;
    WorkTimer = 0.7f;
    const FVector Swing = (Trunk - GetActorLocation()).GetSafeNormal2D();
    if (Tree->ApplyChopDamage(1.0f, Swing.IsNearlyZero() ? GetActorForwardVector() : Swing) && Tree->bFellingStarted)
    {
        ++TreesFelled;
        TargetTree.Reset();
        SettleWait = 8.0f;
    }
    return true;
}

bool AHelperCompanionActor::NeedsTending(const ALogBuildPiece* Log) const
{
    if (!IsValid(Log) || !Log->IsPlaced())
        return false;
    const UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    FStructuralNodeRecord Node;
    if (Graph && Graph->GetNode(Log->GetStructuralNodeId(), Node) && Node.Damage > 0.0f)
        return true;
    if (!IsVerticalLog(Log) || Log->FindComponentByClass<USharpenedTipComponent>())
        return false;
    // A stake is an upright log standing in a row with other upright logs.
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        if (*It != Log && It->IsPlaced() && FVector::Dist2D(It->GetActorLocation(), Log->GetActorLocation()) < 70.0f &&
            IsVerticalLog(*It))
            return true;
    }
    return false;
}

ALogBuildPiece* AHelperCompanionActor::FindStakeToTend() const
{
    ALogBuildPiece* Best = nullptr;
    float BestDistance = SearchRadius;
    for (TActorIterator<ALogBuildPiece> It(GetWorld()); It; ++It)
    {
        const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
        if (Distance < BestDistance && NeedsTending(*It))
        {
            Best = *It;
            BestDistance = Distance;
        }
    }
    return Best;
}

// ---------------------------------------------------------------- carrying

bool AHelperCompanionActor::PickUp(ALogBuildPiece* Log)
{
    if (!IsLooseLog(Log) || Carried.IsValid() ||
        !TransferPiece(Log, EWorldItemOwnerKind::InTransit, CompanionId, GetActorLocation()))
        return false;
    if (UStaticMeshComponent* LogMesh = Log->GetLogMesh())
    {
        LogMesh->SetSimulatePhysics(false);
        LogMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    // On the shoulder, lying along his walking direction.
    const FTransform Shoulder = FTransform(GetActorRotation(), GetActorLocation() + FVector(0, 0, 150.0f));
    Log->SetActorTransform(MakeLogTransform(Log, Shoulder.GetLocation(),
        MakeLogRotation(Log, GetActorForwardVector(), FVector::UpVector)));
    Log->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
    Carried = Log;
    return true;
}

ALogBuildPiece* AHelperCompanionActor::PutDown()
{
    ALogBuildPiece* Log = Carried.Get();
    Carried.Reset();
    if (!IsValid(Log))
        return nullptr;
    Log->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    const FVector Drop = GetActorLocation() + GetActorForwardVector() * 90.0f + FVector(0, 0, 40.0f);
    Log->SetActorLocation(Drop, false, nullptr, ETeleportType::TeleportPhysics);
    TransferPiece(Log, EWorldItemOwnerKind::World, FGuid(), Drop);
    if (UStaticMeshComponent* LogMesh = Log->GetLogMesh())
    {
        LogMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        LogMesh->SetSimulatePhysics(true);
    }
    return Log;
}

bool AHelperCompanionActor::PlaceInPlan(ABuildPlanActor* Plan, int32 Slot)
{
    FString Reason;
    ALogBuildPiece* Log = Carried.Get();
    if (!Plan || !Log || !Plan->CanFillSlot(Slot, Reason))
        return false;
    Carried.Reset();
    Log->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    TransferPiece(Log, EWorldItemOwnerKind::World, FGuid(), Log->GetActorLocation());
    if (UStaticMeshComponent* LogMesh = Log->GetLogMesh())
    {
        LogMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        LogMesh->SetSimulatePhysics(false);
    }

    // Cut it to the part's length; the offcut drops at his feet.
    const float Needed = Plan->GetSlot(Slot).LengthCm;
    FLogGeometry Shape;
    if (GetLogGeometry(Log, Shape) && Shape.Length() > Needed * 1.1f)
        CutOffRemainder(Log, Needed, GetActorLocation() + GetActorForwardVector() * 80.0f + FVector(0, 0, 30.0f));

    const FTransform SlotWorld = Plan->GetSlotWorld(Slot);
    Log->SetActorTransform(MakeLogTransform(Log, SlotWorld.GetLocation(),
        MakeLogRotation(Log, SlotWorld.GetRotation().GetAxisX(), SlotWorld.GetRotation().GetAxisZ())),
        false, nullptr, ETeleportType::TeleportPhysics);
    if (UWorldObjectIdentityComponent* Identity = Log->FindComponentByClass<UWorldObjectIdentityComponent>())
        Identity->TryTransitionLifecycle(EWorldObjectLifecycleState::World, EWorldObjectLifecycleState::Placed);
    Log->SetPlacedState(true);

    // Rest on the part below it in the structural graph, as the player's placement does.
    ALogBuildPiece* Support = Plan->GetSupportPiece(Slot);
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    const FGuid NodeId = Log->GetStructuralNodeId();
    if (Graph && Support && Support->IsStructurallyPlaced() &&
        Graph->TryConnect(Support->GetStructuralNodeId(), FName(*FString::Printf(TEXT("Helper_%s"), *NodeId.ToString())),
            NodeId, TEXT("HelperBase")))
    {
        Log->AttachToActor(Support, FAttachmentTransformRules::KeepWorldTransform);
    }
    Plan->MarkFilled(Slot, Log);
    ++Delivered;
    return true;
}

// ---------------------------------------------------------------- orders

void AHelperCompanionActor::TickCollect(float DeltaSeconds)
{
    if (ALogBuildPiece* Log = Carried.Get())
    {
        ALogContainerActor* Store = FindStorageFor(Log);
        if (!Store)
        {
            // Nowhere to put it: bring it to the player.
            Status = TEXT("No storage takes this log; bringing it to you.");
            if (APawn* Player = FindPlayerPawn(GetWorld()))
            {
                if (MoveTowards(Player->GetActorLocation(), DeltaSeconds, 220.0f))
                    PutDown();
            }
            return;
        }
        Status = TEXT("Carrying a log to storage.");
        if (MoveTowards(Store->GetActorLocation(), DeltaSeconds, 170.0f))
        {
            ALogBuildPiece* Loose = PutDown();
            if (Loose && Store->TryStore(Loose))
                ++Delivered;
        }
        return;
    }

    ThinkTimer -= DeltaSeconds;
    if ((!IsLooseLog(Target.Get()) || ThinkTimer <= 0.0f))
    {
        ThinkTimer = 2.0f;
        Target = FindLooseLog(0.0f, true);
    }
    ALogBuildPiece* Log = Target.Get();
    if (!IsLooseLog(Log))
    {
        if (TickChop(DeltaSeconds))
            return;
        Status = TEXT("No loose logs nearby that a storage takes.");
        FollowPlayer(DeltaSeconds, 300.0f);
        return;
    }
    Status = TEXT("Fetching a log.");
    if (MoveTowards(Log->GetActorLocation(), DeltaSeconds, 140.0f))
    {
        PickUp(Log);
        Target.Reset();
    }
}

void AHelperCompanionActor::TickBuild(float DeltaSeconds)
{
    ABuildPlanActor* Plan = TargetPlan.Get();
    FString Reason;
    if (!IsValid(Plan) || !Plan->CanFillSlot(TargetSlot, Reason))
    {
        int32 Slot = INDEX_NONE;
        FindOpenPlanSlot(Plan, Slot);
        TargetPlan = Plan;
        TargetSlot = Slot;
    }
    if (!IsValid(Plan))
    {
        if (Carried.IsValid())
            PutDown();
        Status = TEXT("No plan to work on. Place one with Z / Y.");
        FollowPlayer(DeltaSeconds, 300.0f);
        return;
    }

    const float Needed = Plan->GetSlot(TargetSlot).LengthCm;
    if (ALogBuildPiece* Log = Carried.Get())
    {
        FLogGeometry Shape;
        if (!GetLogGeometry(Log, Shape) || Shape.Length() < Needed * 0.85f)
        {
            PutDown();
            return;
        }
        Status = FString::Printf(TEXT("Building the %s."), *ABuildPlanActor::GetPlanName(Plan->Kind));
        if (MoveTowards(Plan->GetSlotWorld(TargetSlot).GetLocation(), DeltaSeconds, 230.0f))
            PlaceInPlan(Plan, TargetSlot);
        return;
    }

    ThinkTimer -= DeltaSeconds;
    if (!IsLooseLog(Target.Get()) || ThinkTimer <= 0.0f)
    {
        ThinkTimer = 2.0f;
        Target = FindLooseLog(Needed * 0.85f, false);
    }
    ALogBuildPiece* Log = Target.Get();
    if (!IsLooseLog(Log))
    {
        Status = FString::Printf(TEXT("Needs a log of about %.0f cm for the plan."), Needed);
        FollowPlayer(DeltaSeconds, 300.0f);
        return;
    }
    Status = TEXT("Fetching a log for the plan.");
    if (MoveTowards(Log->GetActorLocation(), DeltaSeconds, 140.0f))
    {
        PickUp(Log);
        Target.Reset();
    }
}

void AHelperCompanionActor::TickPile(float DeltaSeconds)
{
    if (Carried.IsValid())
    {
        Status = TEXT("Carrying a log to the pile.");
        if (MoveTowards(PilePoint, DeltaSeconds, 120.0f))
        {
            if (ALogBuildPiece* Log = PutDown())
            {
                // Side by side in rows of four, then a layer up.
                const FVector Offset(0.0f, (PileCount % 4) * 45.0f - 70.0f, 30.0f + (PileCount / 4) * 40.0f);
                Log->SetActorLocationAndRotation(PilePoint + GetActorRotation().RotateVector(Offset),
                    MakeLogRotation(Log, GetActorForwardVector(), FVector::UpVector), false, nullptr, ETeleportType::TeleportPhysics);
                ++PileCount;
                ++Delivered;
            }
        }
        return;
    }
    ThinkTimer -= DeltaSeconds;
    if (!IsLooseLog(Target.Get()) || ThinkTimer <= 0.0f)
    {
        ThinkTimer = 2.0f;
        Target = FindLooseLog(0.0f, false, 400.0f);
    }
    ALogBuildPiece* Log = Target.Get();
    if (!IsLooseLog(Log))
    {
        if (TickChop(DeltaSeconds))
            return;
        Status = TEXT("No loose logs left to bring.");
        MoveTowards(PilePoint, DeltaSeconds, 250.0f);
        return;
    }
    Status = TEXT("Fetching a log for the pile.");
    if (MoveTowards(Log->GetActorLocation(), DeltaSeconds, 140.0f))
    {
        PickUp(Log);
        Target.Reset();
    }
}

void AHelperCompanionActor::TickMaintain(float DeltaSeconds)
{
    if (Carried.IsValid())
        PutDown();
    ALogBuildPiece* Stake = TargetStake.Get();
    if (!NeedsTending(Stake))
    {
        Stake = FindStakeToTend();
        TargetStake = Stake;
    }
    if (!Stake)
    {
        Status = TEXT("The walls are sharp and sound.");
        FollowPlayer(DeltaSeconds, 300.0f);
        return;
    }
    Status = TEXT("Tending the walls.");
    if (!MoveTowards(Stake->GetActorLocation(), DeltaSeconds, 150.0f))
        return;
    WorkTimer -= DeltaSeconds;
    if (WorkTimer > 0.0f)
        return;
    WorkTimer = 1.0f;
    // One swing repairs a part of the damage, or sharpens the top of a stake.
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();
    if (Graph && Graph->RepairDamage(Stake->GetStructuralNodeId(), 25.0f))
        return;
    if (IsVerticalLog(Stake) && !Stake->FindComponentByClass<USharpenedTipComponent>())
    {
        USharpenedTipComponent* Tip = NewObject<USharpenedTipComponent>(Stake);
        Tip->RegisterComponent();
        if (Tip->InitializeTip())
            ++WallsTended;
        else
            Tip->DestroyComponent();
    }
}

void AHelperCompanionActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld())
        return;
    switch (Order)
    {
    case ECompanionOrder::Follow:
        if (Carried.IsValid())
            PutDown();
        Status = GetOrderName(Order);
        FollowPlayer(DeltaSeconds, 280.0f);
        break;
    case ECompanionOrder::CollectLogs:
        TickCollect(DeltaSeconds);
        break;
    case ECompanionOrder::PileLogsHere:
        TickPile(DeltaSeconds);
        break;
    case ECompanionOrder::BuildPlan:
        TickBuild(DeltaSeconds);
        break;
    case ECompanionOrder::MaintainWalls:
        TickMaintain(DeltaSeconds);
        break;
    case ECompanionOrder::Stay:
    default:
        Status = GetOrderName(Order);
        break;
    }
}

void AHelperCompanionActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Carried.IsValid())
        PutDown();
    Super::EndPlay(Reason);
}
