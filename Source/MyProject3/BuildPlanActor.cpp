#include "BuildPlanActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"
#include "LogContainerActor.h"
#include "HarvestableTreeActor.h"
#include "LogAttachmentComponents.h"
#include "WildlifeActor.h"
#include "WorldInfrastructureRouteLinkComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

using namespace MyProject3Construction;

ABuildPlanActor::ABuildPlanActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

FString ABuildPlanActor::GetPlanName(EBuildPlanKind InKind)
{
    const UEnum* Enum = StaticEnum<EBuildPlanKind>();
    return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(InKind)).ToString() : TEXT("Plan");
}

void ABuildPlanActor::AddSlot(const FVector& Centre, const FRotator& Direction, float Length, int32 Support)
{
    FBuildPlanSlot& Slot = Slots.AddDefaulted_GetRef();
    Slot.Local = FTransform(Direction, Centre);
    Slot.LengthCm = Length;
    Slot.SupportSlot = Support;
}

void ABuildPlanActor::InitializePlan(EBuildPlanKind InKind)
{
    Kind = InKind;
    Slots.Reset();
    const FRotator AlongX(0, 0, 0), AlongY(0, 90, 0), Up(90, 0, 0);
    constexpr float R = 18.0f; // Typical log radius used to stack slots.
    switch (Kind)
    {
    case EBuildPlanKind::LogWall:
        for (int32 Row = 0; Row < 5; ++Row)
            AddSlot(FVector(0, 0, R + Row * 2.0f * R * 0.9f), AlongY, 300.0f, Row == 0 ? INDEX_NONE : Row - 1);
        break;
    case EBuildPlanKind::LeanToShelter:
        AddSlot(FVector(0, -150, 100), Up, 200.0f, INDEX_NONE);
        AddSlot(FVector(0, 150, 100), Up, 200.0f, INDEX_NONE);
        AddSlot(FVector(0, 0, 200 + R), AlongY, 340.0f, 0);
        for (int32 Index = 0; Index < 5; ++Index)
            AddSlot(FVector(-95, -140 + Index * 70.0f, 110), FRotator(-48, 0, 0), 280.0f, 2);
        break;
    case EBuildPlanKind::Watchtower:
        AddSlot(FVector(-100, -100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(100, -100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(-100, 100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(100, 100, 200), Up, 400.0f, INDEX_NONE);
        AddSlot(FVector(0, -100, 400 + R), AlongX, 260.0f, 0);
        AddSlot(FVector(0, 100, 400 + R), AlongX, 260.0f, 2);
        for (int32 Index = 0; Index < 6; ++Index)
            AddSlot(FVector(-100 + Index * 40.0f, 0, 400 + 3.0f * R), AlongY, 260.0f, 4);
        // Ladder rails up one side.
        AddSlot(FVector(-210, -40, 200), FRotator(70, 0, 0), 440.0f, INDEX_NONE);
        AddSlot(FVector(-210, 40, 200), FRotator(70, 0, 0), 440.0f, INDEX_NONE);
        break;
    case EBuildPlanKind::SmallLogStorage:
    case EBuildPlanKind::LargeLogStorage:
    {
        const float HalfDepth = Kind == EBuildPlanKind::LargeLogStorage ? 120.0f : 70.0f;
        const float PostHeight = Kind == EBuildPlanKind::LargeLogStorage ? 200.0f : 120.0f;
        AddSlot(FVector(0, -HalfDepth, R), AlongX, 300.0f, INDEX_NONE);
        AddSlot(FVector(0, HalfDepth, R), AlongX, 300.0f, INDEX_NONE);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float X = Index < 2 ? -140.0f : 140.0f;
            const float Y = Index % 2 == 0 ? -HalfDepth : HalfDepth;
            AddSlot(FVector(X, Y, 2.0f * R + 0.5f * PostHeight), Up, PostHeight, Index % 2);
        }
        break;
    }
    case EBuildPlanKind::LogSled:
        AddSlot(FVector(0, -50, R), AlongX, 250.0f, INDEX_NONE);
        AddSlot(FVector(0, 50, R), AlongX, 250.0f, INDEX_NONE);
        AddSlot(FVector(-80, 0, 3.0f * R), AlongY, 150.0f, 0);
        AddSlot(FVector(80, 0, 3.0f * R), AlongY, 150.0f, 0);
        break;
    case EBuildPlanKind::DefenseWall:
    {
        // A drawn line: one stake every 40 cm along local Y, centred on the plan.
        const float Line = LineLengthCm > 0.0f ? LineLengthCm : 320.0f;
        const int32 Count = FMath::Clamp(FMath::RoundToInt(Line / 40.0f) + 1, 2, 60);
        const float Step = Line / (Count - 1);
        for (int32 Index = 0; Index < Count; ++Index)
            AddSlot(FVector(0, -0.5f * Line + Index * Step, 150), Up, 300.0f, INDEX_NONE);
        break;
    }
    case EBuildPlanKind::HuntingShelter:
        // A ridge on two posts with poles leaning on it from both sides.
        AddSlot(FVector(0, -140, 75), Up, 150.0f, INDEX_NONE);
        AddSlot(FVector(0, 140, 75), Up, 150.0f, INDEX_NONE);
        AddSlot(FVector(0, 0, 150 + R), AlongY, 300.0f, 0);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Y = -105.0f + Index * 70.0f;
            AddSlot(FVector(-65, Y, 80), FRotator(-58, 0, 0), 190.0f, 2);
            AddSlot(FVector(65, Y, 80), FRotator(-58, 180, 0), 190.0f, 2);
        }
        break;
    case EBuildPlanKind::SmallPlatform:
        // Four posts, two beams and a deck of seven logs, free of any tree.
        for (int32 Index = 0; Index < 4; ++Index)
            AddSlot(FVector(Index < 2 ? -120.0f : 120.0f, Index % 2 == 0 ? -100.0f : 100.0f, 125), Up, 250.0f, INDEX_NONE);
        AddSlot(FVector(0, -100, 250 + R), AlongX, 300.0f, 0);
        AddSlot(FVector(0, 100, 250 + R), AlongX, 300.0f, 1);
        for (int32 Index = 0; Index < 7; ++Index)
            AddSlot(FVector(-120 + Index * 40.0f, 0, 250 + 3.0f * R), AlongY, 260.0f, 4);
        break;
    case EBuildPlanKind::Bench:
        AddSlot(FVector(-70, 0, 37), Up, 75.0f, INDEX_NONE);
        AddSlot(FVector(70, 0, 37), Up, 75.0f, INDEX_NONE);
        AddSlot(FVector(0, 0, 75 + R), AlongX, 200.0f, 0);
        break;
    case EBuildPlanKind::WeaponRack:
        AddSlot(FVector(-80, 0, 75), Up, 150.0f, INDEX_NONE);
        AddSlot(FVector(80, 0, 75), Up, 150.0f, INDEX_NONE);
        AddSlot(FVector(0, 15, 60), AlongX, 200.0f, 0);
        AddSlot(FVector(0, 15, 130), AlongX, 200.0f, 0);
        break;
    case EBuildPlanKind::SmallLogCabin:
    {
        // 4.2 x 3 m, five rows of notched walls with a door gap in the front
        // (local -Y) wall, two gable posts, a ridge and four poles per roof side.
        const float Step = 2.0f * R * 0.9f;
        int32 Back = INDEX_NONE, Left = INDEX_NONE, Right = INDEX_NONE;
        int32 FrontLeft = INDEX_NONE, FrontRight = INDEX_NONE, Front = INDEX_NONE;
        for (int32 Row = 0; Row < 5; ++Row)
        {
            const float LongZ = R + Row * Step;
            const float ShortZ = LongZ + 0.5f * Step;
            AddSlot(FVector(0, 150, LongZ), AlongX, 440.0f, Back);
            Back = Slots.Num() - 1;
            if (Row < 4)
            {
                AddSlot(FVector(-135, -150, LongZ), AlongX, 170.0f, FrontLeft);
                FrontLeft = Slots.Num() - 1;
                AddSlot(FVector(135, -150, LongZ), AlongX, 170.0f, FrontRight);
                FrontRight = Slots.Num() - 1;
            }
            else
            {
                AddSlot(FVector(0, -150, LongZ), AlongX, 440.0f, FrontLeft);
                Front = Slots.Num() - 1;
            }
            AddSlot(FVector(-200, 0, ShortZ), AlongY, 360.0f, Left);
            Left = Slots.Num() - 1;
            AddSlot(FVector(200, 0, ShortZ), AlongY, 360.0f, Right);
            Right = Slots.Num() - 1;
        }
        const float Top = R + 4.0f * Step + 0.5f * Step + R;
        AddSlot(FVector(-200, 0, Top + 45), Up, 90.0f, Left);
        const int32 GableA = Slots.Num() - 1;
        AddSlot(FVector(200, 0, Top + 45), Up, 90.0f, Right);
        AddSlot(FVector(0, 0, Top + 90 + R), AlongX, 460.0f, GableA);
        const int32 Ridge = Slots.Num() - 1;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float X = -165.0f + Index * 110.0f;
            AddSlot(FVector(X, -80, Top + 50), FRotator(31, 90, 0), 210.0f, Ridge);
            AddSlot(FVector(X, 80, Top + 50), FRotator(31, -90, 0), 210.0f, Ridge);
        }
        (void)Front;
        break;
    }
    case EBuildPlanKind::TreePlatform:
    case EBuildPlanKind::TreeShelter:
    {
        // Two beams resting on the trunk (they fall with the tree), and a deck.
        constexpr float DeckZ = 300.0f;
        AddSlot(FVector(0, -60, DeckZ), AlongX, 300.0f, INDEX_NONE);
        AddSlot(FVector(0, 60, DeckZ), AlongX, 300.0f, INDEX_NONE);
        for (int32 Index = 0; Index < 6; ++Index)
            AddSlot(FVector(-110 + Index * 44.0f, 0, DeckZ + 2.0f * R), AlongY, 240.0f, Index % 2);
        if (Kind == EBuildPlanKind::TreeShelter)
        {
            // Corner posts on the deck beams, two roof beams and a roof.
            for (int32 Index = 0; Index < 4; ++Index)
                AddSlot(FVector(Index < 2 ? -120.0f : 120.0f, Index % 2 == 0 ? -60.0f : 60.0f, DeckZ + R + 75), Up, 150.0f, Index % 2);
            const int32 FirstPost = Slots.Num() - 4;
            AddSlot(FVector(0, -60, DeckZ + R + 150 + R), AlongX, 300.0f, FirstPost);
            AddSlot(FVector(0, 60, DeckZ + R + 150 + R), AlongX, 300.0f, FirstPost + 1);
            const int32 RoofBeam = Slots.Num() - 2;
            for (int32 Index = 0; Index < 6; ++Index)
                AddSlot(FVector(-110 + Index * 44.0f, 0, DeckZ + R + 150 + 3.0f * R), AlongY, 240.0f, RoofBeam);
        }
        break;
    }
    case EBuildPlanKind::Chair:
        for (int32 Index = 0; Index < 4; ++Index)
            AddSlot(FVector(Index < 2 ? -25.0f : 25.0f, Index % 2 == 0 ? -25.0f : 25.0f, 25), Up, 50.0f, INDEX_NONE);
        AddSlot(FVector(0, -13, 50 + R), AlongX, 75.0f, 0);
        AddSlot(FVector(0, 13, 50 + R), AlongX, 75.0f, 1);
        AddSlot(FVector(-25, 30, 50 + 2.0f * R + 35), Up, 70.0f, 5);
        AddSlot(FVector(25, 30, 50 + 2.0f * R + 35), Up, 70.0f, 5);
        break;
    case EBuildPlanKind::Couch:
        for (int32 Index = 0; Index < 4; ++Index)
            AddSlot(FVector(Index < 2 ? -95.0f : 95.0f, Index % 2 == 0 ? -35.0f : 35.0f, 25), Up, 50.0f, INDEX_NONE);
        for (int32 Index = 0; Index < 3; ++Index)
            AddSlot(FVector(0, -30 + Index * 30.0f, 50 + R), AlongX, 220.0f, 0);
        AddSlot(FVector(-95, 50, 55), Up, 110.0f, INDEX_NONE);
        AddSlot(FVector(95, 50, 55), Up, 110.0f, INDEX_NONE);
        AddSlot(FVector(0, 50, 80), AlongX, 220.0f, 7);
        AddSlot(FVector(0, 50, 80 + 2.0f * R * 0.9f), AlongX, 220.0f, 9);
        break;
    case EBuildPlanKind::Shelf:
        AddSlot(FVector(-80, 0, 90), Up, 180.0f, INDEX_NONE);
        AddSlot(FVector(80, 0, 90), Up, 180.0f, INDEX_NONE);
        for (int32 Index = 0; Index < 3; ++Index)
            AddSlot(FVector(0, 0, 45 + Index * 55.0f), AlongX, 190.0f, 0);
        break;
    case EBuildPlanKind::RabbitHutch:
        AddSlot(FVector(0, -40, R), AlongX, 150.0f, INDEX_NONE);
        AddSlot(FVector(0, 40, R), AlongX, 150.0f, INDEX_NONE);
        for (int32 Index = 0; Index < 4; ++Index)
            AddSlot(FVector(Index < 2 ? -65.0f : 65.0f, Index % 2 == 0 ? -40.0f : 40.0f, 2.0f * R + 40), Up, 80.0f, Index % 2);
        AddSlot(FVector(0, -40, 2.0f * R + 80 + R), AlongX, 150.0f, 2);
        AddSlot(FVector(0, 40, 2.0f * R + 80 + R), AlongX, 150.0f, 3);
        for (int32 Index = 0; Index < 4; ++Index)
            AddSlot(FVector(-54 + Index * 36.0f, 0, 2.0f * R + 80 + 3.0f * R), AlongY, 110.0f, 6);
        break;
    case EBuildPlanKind::CreekBridge:
    {
        // 720 cm bank-to-bank crossing. Three overlapping 300 cm beam segments
        // per side sit on six ground-supported posts. Cross-deck logs span both
        // beams; 300 cm inclined end logs meet the deck at z=234 and reach ground
        // near x=+/-540. The route is not enabled until all 29 slots are filled.
        constexpr float Span = 720.0f;
        constexpr float DeckZ = 234.0f;
        constexpr float BeamZ = DeckZ - 2.0f * R;
        constexpr float HalfSpan = Span * 0.5f;
        constexpr float SupportSpacing = 240.0f;
        constexpr float BeamOffsetY = 90.0f;

        // Intermediate abutment/piers, centred under each of the three beam segments.
        for (int32 Segment = 0; Segment < 3; ++Segment)
        {
            const float X = -SupportSpacing + Segment * SupportSpacing;
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const float Y = Side == 0 ? -BeamOffsetY : BeamOffsetY;
                AddSlot(FVector(X, Y, 90.0f), Up, 180.0f, INDEX_NONE);
            }
        }

        // Six overlapping longitudinal beam segments. Each segment bears on a post.
        const int32 FirstBeamSlot = Slots.Num();
        for (int32 Segment = 0; Segment < 3; ++Segment)
        {
            const float X = -SupportSpacing + Segment * SupportSpacing;
            for (int32 Side = 0; Side < 2; ++Side)
            {
                const float Y = Side == 0 ? -BeamOffsetY : BeamOffsetY;
                const int32 SupportSlot = Segment * 2 + Side;
                AddSlot(FVector(X, Y, BeamZ), AlongX, 300.0f, SupportSlot);
            }
        }

        // Thirteen transverse deck logs overlap at 60 cm pitch over the 720 cm span.
        for (int32 Index = 0; Index <= 12; ++Index)
        {
            const float X = -HalfSpan + Index * 60.0f;
            const int32 Segment = FMath::Clamp(
                FMath::RoundToInt((X + SupportSpacing) / SupportSpacing), 0, 2);
            const int32 SupportSlot = FirstBeamSlot + Segment * 2 + (Index % 2);
            AddSlot(FVector(X, 0.0f, DeckZ), AlongY, 180.0f, SupportSlot);
        }

        // Two sloped approach logs per bank, parallel to the crossing direction.
        // Pitch is chosen so a 300 cm log rises from ground to the deck height.
        const float RampPitch = FMath::RadiansToDegrees(FMath::Asin(DeckZ / 300.0f));
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float Y = Side == 0 ? -60.0f : 60.0f;
            const int32 LeftSupport = FirstBeamSlot + Side;
            const int32 RightSupport = FirstBeamSlot + 4 + Side;
            AddSlot(FVector(-450.0f, Y, DeckZ * 0.5f),
                FRotator(RampPitch, 0.0f, 0.0f), 300.0f, LeftSupport);
            AddSlot(FVector(450.0f, Y, DeckZ * 0.5f),
                FRotator(-RampPitch, 0.0f, 0.0f), 300.0f, RightSupport);
        }
        break;
    }

    case EBuildPlanKind::CornerRamp:
    {
        // A ramp up to a landing, then a second ramp turning 90 degrees.
        constexpr float Rise = 150.0f;
        const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, 300.0f));
        AddSlot(FVector(150, -45, 0.5f * Rise), FRotator(Pitch, 0, 0), 340.0f, INDEX_NONE);
        AddSlot(FVector(150, 45, 0.5f * Rise), FRotator(Pitch, 0, 0), 340.0f, INDEX_NONE);
        for (int32 Index = 0; Index < 5; ++Index)
        {
            const float X = 40.0f + Index * 55.0f;
            AddSlot(FVector(X, 0, X / 300.0f * Rise + 2.0f * R), AlongY, 140.0f, 0);
        }
        const int32 FirstPost = Slots.Num();
        for (int32 Index = 0; Index < 4; ++Index)
            AddSlot(FVector(Index < 2 ? 310.0f : 430.0f, Index % 2 == 0 ? -60.0f : 60.0f, 0.5f * Rise), Up, Rise, INDEX_NONE);
        AddSlot(FVector(310, 0, Rise + R), AlongY, 160.0f, FirstPost);
        AddSlot(FVector(430, 0, Rise + R), AlongY, 160.0f, FirstPost + 2);
        const int32 LandingBeam = Slots.Num() - 2;
        for (int32 Index = 0; Index < 3; ++Index)
            AddSlot(FVector(370, -45 + Index * 45.0f, Rise + 3.0f * R), AlongX, 160.0f, LandingBeam);
        AddSlot(FVector(325, 210, 1.5f * Rise), FRotator(Pitch, 90, 0), 340.0f, LandingBeam + 1);
        AddSlot(FVector(415, 210, 1.5f * Rise), FRotator(Pitch, 90, 0), 340.0f, LandingBeam + 1);
        const int32 UpperStringer = Slots.Num() - 2;
        for (int32 Index = 0; Index < 5; ++Index)
        {
            const float Y = 100.0f + Index * 55.0f;
            AddSlot(FVector(370, Y, Rise + (Y - 60.0f) / 300.0f * Rise + 2.0f * R), AlongX, 140.0f, UpperStringer);
        }
        break;
    }
    }

    UStaticMesh* Cylinder = LoadEngineShape(TEXT("Cylinder"));
    UMaterialInterface* Ghost = GetGhostMaterial();
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->RegisterComponent();
        Mesh->SetStaticMesh(Cylinder);
        if (Ghost)
            Mesh->SetMaterial(0, Ghost);
        // Cylinder is 100 cm along Z: turn it to lie along the slot's X.
        const FTransform& Local = Slots[Index].Local;
        const FQuat ZToX = FQuat::FindBetweenNormals(FVector::ZAxisVector, FVector::XAxisVector);
        Mesh->SetRelativeTransform(FTransform(Local.GetRotation() * ZToX, Local.GetLocation(),
            FVector(0.36f, 0.36f, Slots[Index].LengthCm / 100.0f)));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Ghosts.Add(Mesh);
    }
}

int32 ABuildPlanActor::GetFilledCount() const
{
    int32 Count = 0;
    for (const FBuildPlanSlot& Slot : Slots)
        Count += Slot.bFilled ? 1 : 0;
    return Count;
}

FTransform ABuildPlanActor::GetSlotWorld(int32 Index) const
{
    return Slots.IsValidIndex(Index) ? Slots[Index].Local * GetActorTransform() : FTransform::Identity;
}

int32 ABuildPlanActor::FindSlot(const UPrimitiveComponent* Component, const FVector& WorldPoint) const
{
    const int32 Direct = Ghosts.IndexOfByPredicate([Component](const TObjectPtr<UStaticMeshComponent>& Ghost)
    {
        return Ghost.Get() == Component;
    });
    if (Slots.IsValidIndex(Direct) && !Slots[Direct].bFilled)
        return Direct;
    int32 Best = INDEX_NONE;
    float BestDistance = 150.0f;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        const float Distance = FVector::Dist(GetSlotWorld(Index).GetLocation(), WorldPoint);
        if (!Slots[Index].bFilled && Distance < BestDistance)
        {
            Best = Index;
            BestDistance = Distance;
        }
    }
    return Best;
}

bool ABuildPlanActor::CanFillSlot(int32 Index, FString& OutReason) const
{
    if (!Slots.IsValidIndex(Index) || Slots[Index].bFilled)
    {
        OutReason = TEXT("That part of the plan is already built.");
        return false;
    }
    const int32 Support = Slots[Index].SupportSlot;
    if (Support != INDEX_NONE && (!Slots[Support].bFilled || !Slots[Support].Piece.IsValid()))
    {
        OutReason = TEXT("Build the part it rests on first.");
        return false;
    }
    return true;
}

ALogBuildPiece* ABuildPlanActor::GetSupportPiece(int32 Index) const
{
    if (!Slots.IsValidIndex(Index) || Slots[Index].SupportSlot == INDEX_NONE)
        return nullptr;
    return Slots[Slots[Index].SupportSlot].Piece.Get();
}

void ABuildPlanActor::MarkFilled(int32 Index, ALogBuildPiece* Piece)
{
    if (!Slots.IsValidIndex(Index))
        return;
    Slots[Index].bFilled = true;
    Slots[Index].Piece = Piece;
    // Parts that rest on nothing in a tree plan rest on the trunk: they fall when it is cut.
    if (AnchorTree.IsValid() && Slots[Index].SupportSlot == INDEX_NONE && Piece)
    {
        UTreeAnchorComponent* Anchor = NewObject<UTreeAnchorComponent>(Piece);
        Anchor->RegisterComponent();
        Anchor->SetTree(AnchorTree.Get());
    }
    if (Ghosts.IsValidIndex(Index) && Ghosts[Index])
    {
        Ghosts[Index]->SetVisibility(false);
        Ghosts[Index]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (IsComplete())
        Complete();
}

void ABuildPlanActor::Complete()
{
    UWorld* World = GetWorld();
    ELogContainerKind ContainerKind = ELogContainerKind::SmallLogStorage;
    bool bSpawnContainer = false;
    switch (Kind)
    {
    case EBuildPlanKind::SmallLogStorage: bSpawnContainer = true; break;
    case EBuildPlanKind::LargeLogStorage: ContainerKind = ELogContainerKind::LargeLogStorage; bSpawnContainer = true; break;
    case EBuildPlanKind::LogSled: ContainerKind = ELogContainerKind::BasicSled; bSpawnContainer = true; break;
    default: break;
    }
    if (Kind == EBuildPlanKind::CreekBridge && World)
    {
        // A route link is a separate invisible root, not one of the deck logs.
        // It only comes into existence when Complete() is reached (all slots filled).
        const FTransform RouteTransform(GetActorRotation(), GetActorLocation());
        AActor* RouteRoot = World->SpawnActorDeferred<AActor>(
            AActor::StaticClass(), RouteTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (RouteRoot)
        {
            USceneComponent* Root = NewObject<USceneComponent>(RouteRoot, TEXT("BridgeRouteRoot"));
            RouteRoot->SetRootComponent(Root);
            Root->RegisterComponent();

            UWorldInfrastructureRouteLinkComponent* RouteLink =
                NewObject<UWorldInfrastructureRouteLinkComponent>(RouteRoot, TEXT("CreekBridgeRouteLink"));
            RouteRoot->AddInstanceComponent(RouteLink);
            RouteLink->InfrastructureId = TEXT("Infrastructure.CreekBridge");
            RouteLink->bInfrastructureUsable = false;
            RouteLink->RegisterComponent();
            RouteRoot->FinishSpawning(RouteTransform);
            if (!RouteLink->SetInfrastructureUsable(true))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("Creek bridge completed physically but route graph has no matching Infrastructure.CreekBridge edge."));
            }
        }
    }

    if (Kind == EBuildPlanKind::RabbitHutch && World)
    {
        // A rabbit moves into the finished hutch.
        const FVector Inside = GetActorLocation() + FVector(0, 0, 45.0f);
        if (AWildlifeActor* Rabbit = World->SpawnActorDeferred<AWildlifeActor>(AWildlifeActor::StaticClass(), FTransform(Inside)))
        {
            Rabbit->ConfigureKind(EWildlifeKind::SmallAnimal);
            Rabbit->WanderRadius = 25.0f;
            Rabbit->FinishSpawning(FTransform(GetActorRotation(), Inside, Rabbit->GetActorScale3D()));
        }
    }
    if (bSpawnContainer && World)
    {
        // A sled is a single movable object: its logs become the sled.
        if (Kind == EBuildPlanKind::LogSled)
        {
            for (FBuildPlanSlot& Slot : Slots)
                ConsumePiece(Slot.Piece.Get());
        }
        const FVector Location = GetActorLocation() + FVector(0, 0, Kind == EBuildPlanKind::LogSled ? 20.0f : 45.0f);
        ALogContainerActor* Container = World->SpawnActorDeferred<ALogContainerActor>(ALogContainerActor::StaticClass(),
            FTransform(GetActorRotation(), Location));
        if (Container)
        {
            Container->ConfigureKind(ContainerKind);
            Container->FinishSpawning(FTransform(GetActorRotation(), Location, Container->GetActorScale3D()));
        }
    }
    Destroy();
}
