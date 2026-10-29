#include "VerticalSliceCWorldActor.h"
#include "Placement/PlacementKernelSubsystem.h"

#include "ConstructionHelpers.h"
#include "HarvestableTreeActor.h"
#include "GatherableResourceActor.h"
#include "WorldDiscoveryMarkerActor.h"
#include "HeatSourceComponent.h"
#include "HelperCompanionActor.h"
#include "LogAttachmentComponents.h"
#include "LogSnapCharacter.h"
#include "LogBuildPiece.h"
#include "LogContainerActor.h"
#include "LogCarryComponent.h"
#include "InventoryCraftingComponent.h"
#include "InventoryCraftingTypes.h"
#include "ProductionStationActor.h"
#include "WorldGameDataSubsystem.h"
#include "WorldAgricultureSubsystem.h"
#include "WorldProductionSubsystem.h"
#include "RopeLineActor.h"
#include "ShelterSensorComponent.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "VerticalSliceBWorldActor.h"
#include "WildlifeActor.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "StructuralPieceInterface.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldClimateSubsystem.h"
#include "PhysicalHealthComponent.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldEventDirectorSubsystem.h"
#include "WorldCalendar.h"
#include "WorldItemOwnershipSubsystem.h"
#include "WorldSimulationCoordinatorSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/ExponentialHeightFog.h"

using MyProject3Construction::LoadEngineShape;

namespace
{
    float Smooth(float A, float B, float X)
    {
        const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    const FVector2D CampCentre(0.0f, 0.0f);
    const FVector2D CabinCentre(-1350.0f, 380.0f);
    const FVector2D LookoutPoint(600.0f, 2650.0f);
    // Small, intentionally low-cost three-by-three primitive field for P6.2 interaction tests.
    const FVector2D FarmPlotCentre(-350.0f, 950.0f);
    constexpr float FarmPlotCellSize = 180.0f;
    constexpr int32 FarmPlotCellsPerAxis = 3;
    const float BridgeX = 760.0f;
    const float PalisadeX = 2050.0f;

    // Ground colours: mud, rock, trampled dirt, grass, dry grass.
    const FLinearColor GroundColours[] =
    {
        FLinearColor(0.055f, 0.045f, 0.035f),
        FLinearColor(0.09f, 0.075f, 0.06f),
        FLinearColor(0.30f, 0.22f, 0.14f),
        FLinearColor(0.07f, 0.11f, 0.04f),
        FLinearColor(0.13f, 0.14f, 0.06f),
    };

    float DistanceToSegment2D(const FVector2D& P, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D AB = B - A;
        const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0f), 0.0f, 1.0f);
        return FVector2D::Distance(P, A + AB * T);
    }
}

AVerticalSliceCWorldActor::AVerticalSliceCWorldActor()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WorldRoot"));

    Ground = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("C_Ground"));
    Ground->SetupAttachment(RootComponent);
    Ground->bUseComplexAsSimpleCollision = true;
    Ground->SetCollisionProfileName(TEXT("BlockAll"));

    GrassClumps = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("C_Grass"));
    GrassClumps->SetupAttachment(RootComponent);
    GrassClumps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GrassClumps->SetCastShadow(false);

    Pebbles = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("C_Pebbles"));
    Pebbles->SetupAttachment(RootComponent);
    Pebbles->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Water = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("C_Water"));
    Water->SetupAttachment(RootComponent);
    Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Backdrop = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("C_Backdrop"));
    Backdrop->SetupAttachment(RootComponent);
    Backdrop->bUseComplexAsSimpleCollision = true;
    Backdrop->SetCollisionProfileName(TEXT("BlockAll"));

    FarForest = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("C_FarForest"));
    FarForest->SetupAttachment(RootComponent);
    FarForest->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Natural exposure: without this the auto exposure washes the scene out.
    Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("C_Grade"));
    Grade->SetupAttachment(RootComponent);
    Grade->bUnbound = true;
    Grade->Settings.bOverride_AutoExposureBias = true;
    Grade->Settings.AutoExposureBias = -0.8f;
    Grade->Settings.bOverride_ColorSaturation = true;
    Grade->Settings.ColorSaturation = FVector4(0.85f, 0.85f, 0.85f, 1.0f);
    Grade->Settings.bOverride_VignetteIntensity = true;
    Grade->Settings.VignetteIntensity = 0.45f;

    SimulationTimeScale = 20.0;
    FixedStepSeconds = 1.0;
    MaxStepsPerFrame = 8;
    FirstRaidDelaySeconds = 10.0 * 24.0 * 3600.0;
}

// ---------------------------------------------------------------- terrain

float AVerticalSliceCWorldActor::GetCreekCentreY(float X) const
{
    return -1500.0f + 260.0f * FMath::Sin(X * 0.0011f + 0.6f);
}

float AVerticalSliceCWorldActor::GetGroundHeight(float X, float Y) const
{
    float H = 22.0f * FMath::Sin(X * 0.0021f) * FMath::Cos(Y * 0.0017f) +
        12.0f * FMath::Sin(X * 0.0052f + 1.3f) * FMath::Sin(Y * 0.0047f + 0.4f);

    // The hill to the north, with a cliff terrace in the middle and an easy
    // slope around its west end.
    H += 480.0f * Smooth(700.0f, 2900.0f, Y);
    const float Cliff = Smooth(-700.0f, -300.0f, X) * (1.0f - Smooth(1700.0f, 2100.0f, X));
    H += 260.0f * Smooth(1480.0f, 1560.0f, Y) * Cliff + 260.0f * Smooth(900.0f, 2100.0f, Y) * (1.0f - Cliff);

    // Level ground for the camp and the cabin.
    const FVector2D P(X, Y);
    const float CampBlend = 1.0f - Smooth(600.0f, 1000.0f, FVector2D::Distance(P, CampCentre));
    H = FMath::Lerp(H, 0.0f, CampBlend);
    const float CabinBlend = 1.0f - Smooth(380.0f, 650.0f, FVector2D::Distance(P, CabinCentre));
    H = FMath::Lerp(H, 6.0f, CabinBlend);

    // The creek: steep banks too high to climb, except at a shallow ford far east.
    const float D = FMath::Abs(Y - GetCreekCentreY(X));
    const float Depth = FMath::Lerp(240.0f, 70.0f, Smooth(2000.0f, 2500.0f, X));
    const float Bank = 1.0f - Smooth(160.0f, 270.0f, D);
    H -= Depth * Bank + 30.0f * (1.0f - Smooth(0.0f, 160.0f, D)) * Bank;
    return H;
}

bool AVerticalSliceCWorldActor::IsOpenGround(float X, float Y, float Margin) const
{
    const FVector2D P(X, Y);
    return FVector2D::Distance(P, CampCentre) > 900.0f + Margin &&
        FVector2D::Distance(P, CabinCentre) > 600.0f + Margin &&
        // The field patch is reserved from tree placement so its nine soil tiles remain usable.
        FVector2D::Distance(P, FarmPlotCentre) > 360.0f + Margin &&
        FMath::Abs(Y - GetCreekCentreY(X)) > 480.0f + Margin &&
        !(FMath::Abs(X - PalisadeX) < 500.0f + Margin && FMath::Abs(Y) < 900.0f) &&
        !(X > -400.0f && X < 1800.0f && Y > 1350.0f && Y < 1700.0f) &&
        FVector2D::Distance(P, LookoutPoint) > 350.0f + Margin &&
        FMath::Abs(X - BridgeX) > 250.0f + Margin;
}

UMaterialInterface* AVerticalSliceCWorldActor::Tint(const FLinearColor& Colour)
{
    return MyProject3Construction::MakeTint(this, Colour);
}

UStaticMeshComponent* AVerticalSliceCWorldActor::AddProp(UStaticMesh* Mesh, const FTransform& Transform,
    const FLinearColor& Colour, bool bCollision)
{
    UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(this);
    Prop->SetupAttachment(RootComponent);
    Prop->SetStaticMesh(Mesh);
    Prop->SetWorldTransform(Transform);
    Prop->SetMaterial(0, Tint(Colour));
    Prop->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bCollision)
        Prop->SetCollisionProfileName(TEXT("BlockAll"));
    Prop->RegisterComponent();
    return Prop;
}

void AVerticalSliceCWorldActor::BuildGround()
{
    constexpr float Spacing = 50.0f;
    const int32 Points = FMath::RoundToInt(2.0f * HalfExtent / Spacing) + 1;
    TArray<FVector> Grid;
    Grid.SetNum(Points * Points);
    for (int32 Yi = 0; Yi < Points; ++Yi)
    {
        for (int32 Xi = 0; Xi < Points; ++Xi)
        {
            const float X = -HalfExtent + Xi * Spacing;
            const float Y = -HalfExtent + Yi * Spacing;
            Grid[Yi * Points + Xi] = FVector(X, Y, GetGroundHeight(X, Y));
        }
    }

    // Triangles are sorted into colour sections by what the ground is.
    constexpr int32 Sections = UE_ARRAY_COUNT(GroundColours);
    TArray<FVector> Vertices[Sections];
    TArray<int32> Indices[Sections];
    TArray<FVector> Normals[Sections];
    TArray<FVector2D> UVs[Sections];
    const FVector2D TrailStops[] = { CampCentre, CabinCentre, FVector2D(-650, 1100), FVector2D(-450, 2200),
        LookoutPoint, FVector2D(BridgeX, -900), FVector2D(2350, -1350), FVector2D(PalisadeX - 200, 0) };
    auto Classify = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& Normal)
    {
        const FVector Mid = (A + B + C) / 3.0f;
        const FVector2D P(Mid.X, Mid.Y);
        if (Mid.Z < WaterLevel + 40.0f && FMath::Abs(Mid.Y - GetCreekCentreY(Mid.X)) < 260.0f)
            return 0;
        if (Normal.Z < 0.8f)
            return 1;
        if (FVector2D::Distance(P, CampCentre) < 520.0f || FVector2D::Distance(P, CabinCentre) < 360.0f)
            return 2;
        for (int32 Stop = 1; Stop < UE_ARRAY_COUNT(TrailStops); ++Stop)
        {
            const FVector2D From = Stop == 5 || Stop == 7 ? CampCentre : TrailStops[Stop - 1];
            if (DistanceToSegment2D(P, From, TrailStops[Stop]) < 70.0f + 25.0f * FMath::Sin(Mid.X * 0.01f))
                return 2;
        }
        return FMath::Sin(Mid.X * 0.0031f + Mid.Y * 0.0023f) + 0.6f * FMath::Sin(Mid.X * 0.011f) > 0.55f ? 4 : 3;
    };
    for (int32 Yi = 0; Yi + 1 < Points; ++Yi)
    {
        for (int32 Xi = 0; Xi + 1 < Points; ++Xi)
        {
            const FVector& V00 = Grid[Yi * Points + Xi];
            const FVector& V10 = Grid[Yi * Points + Xi + 1];
            const FVector& V01 = Grid[(Yi + 1) * Points + Xi];
            const FVector& V11 = Grid[(Yi + 1) * Points + Xi + 1];
            const FVector Tris[2][3] = { { V00, V01, V11 }, { V00, V11, V10 } };
            for (const auto& Tri : Tris)
            {
                const FVector Normal = FVector::CrossProduct(Tri[2] - Tri[0], Tri[1] - Tri[0]).GetSafeNormal();
                const int32 Section = Classify(Tri[0], Tri[1], Tri[2], Normal);
                for (const FVector& Vertex : Tri)
                {
                    Indices[Section].Add(Vertices[Section].Num());
                    Vertices[Section].Add(Vertex);
                    Normals[Section].Add(Normal);
                    UVs[Section].Add(FVector2D(Vertex.X, Vertex.Y) / 200.0f);
                }
            }
        }
    }
    for (int32 Section = 0; Section < Sections; ++Section)
    {
        Ground->CreateMeshSection_LinearColor(Section, Vertices[Section], Indices[Section], Normals[Section],
            UVs[Section], {}, {}, true);
        Ground->SetMaterial(Section, Tint(GroundColours[Section]));
    }
}

void AVerticalSliceCWorldActor::BuildWater()
{
    UMaterialInterface* WaterMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/EngineMaterials/WaterMaterial.WaterMaterial"));
    Water->SetStaticMesh(LoadEngineShape(TEXT("Plane")));
    // The plane only shows where the ground dips below the water line.
    Water->SetWorldTransform(FTransform(FRotator::ZeroRotator, FVector(0.0f, -1500.0f, WaterLevel),
        FVector(2.0f * HalfExtent / 100.0f, 13.0f, 1.0f)));
    Water->SetMaterial(0, WaterMaterial ? WaterMaterial : Tint(FLinearColor(0.05f, 0.12f, 0.15f)));
}

void AVerticalSliceCWorldActor::BuildBackdrop()
{
    // A coarse ring of rising wooded hills from the edge of the clearing outwards.
    constexpr float Spacing = 600.0f;
    constexpr float Outer = 36000.0f;
    const int32 Points = FMath::RoundToInt(2.0f * Outer / Spacing) + 1;
    auto Height = [this](float X, float Y)
    {
        const float Cx = FMath::Clamp(X, -HalfExtent, HalfExtent);
        const float Cy = FMath::Clamp(Y, -HalfExtent, HalfExtent);
        const float Out = FVector2D::Distance(FVector2D(X, Y), FVector2D(Cx, Cy));
        return GetGroundHeight(Cx, Cy) + 1600.0f * Smooth(0.0f, 16000.0f, Out) +
            Smooth(0.0f, 3000.0f, Out) * (260.0f * FMath::Sin(X * 0.00041f) * FMath::Cos(Y * 0.00037f) +
            120.0f * FMath::Sin(X * 0.0011f + Y * 0.0009f));
    };
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    Vertices.SetNum(Points * Points);
    Normals.SetNum(Points * Points);
    for (int32 Yi = 0; Yi < Points; ++Yi)
    {
        for (int32 Xi = 0; Xi < Points; ++Xi)
        {
            const float X = -Outer + Xi * Spacing;
            const float Y = -Outer + Yi * Spacing;
            Vertices[Yi * Points + Xi] = FVector(X, Y, Height(X, Y));
            const FVector Dx(2.0f * Spacing, 0.0f, Height(X + Spacing, Y) - Height(X - Spacing, Y));
            const FVector Dy(0.0f, 2.0f * Spacing, Height(X, Y + Spacing) - Height(X, Y - Spacing));
            Normals[Yi * Points + Xi] = FVector::CrossProduct(Dx, Dy).GetSafeNormal();
        }
    }
    for (int32 Yi = 0; Yi + 1 < Points; ++Yi)
    {
        for (int32 Xi = 0; Xi + 1 < Points; ++Xi)
        {
            const float X0 = -Outer + Xi * Spacing;
            const float Y0 = -Outer + Yi * Spacing;
            // The clearing's own ground fills the middle.
            if (X0 >= -HalfExtent && X0 + Spacing <= HalfExtent && Y0 >= -HalfExtent && Y0 + Spacing <= HalfExtent)
                continue;
            const int32 A = Yi * Points + Xi;
            const int32 B = A + 1;
            const int32 C = A + Points;
            const int32 D = C + 1;
            Indices.Append({ A, C, D, A, D, B });
        }
    }
    Backdrop->CreateMeshSection_LinearColor(0, Vertices, Indices, Normals, {}, {}, {}, true);
    Backdrop->SetMaterial(0, Tint(FLinearColor(0.045f, 0.07f, 0.03f)));

    // A dense far forest of simple conifers on those hills.
    FRandomStream Random(4242);
    FarForest->SetStaticMesh(LoadEngineShape(TEXT("Cone")));
    FarForest->SetMaterial(0, Tint(FLinearColor(0.025f, 0.05f, 0.025f)));
    FarForest->SetCastShadow(false);
    for (int32 Index = 0; Index < 5000; ++Index)
    {
        const float X = Random.FRandRange(-16000.0f, 16000.0f);
        const float Y = Random.FRandRange(-16000.0f, 16000.0f);
        if (FMath::Abs(X) < HalfExtent + 150.0f && FMath::Abs(Y) < HalfExtent + 150.0f)
            continue;
        // Two stacked cones read as a conifer from a distance.
        const float Size = Random.FRandRange(4.0f, 8.0f);
        const float BaseZ = Height(X, Y);
        FarForest->AddInstance(FTransform(FRotator(0, Random.FRandRange(0, 360), 0),
            FVector(X, Y, BaseZ + Size * 40.0f), FVector(Size * 0.5f, Size * 0.5f, Size * 0.7f)), false);
        FarForest->AddInstance(FTransform(FRotator(0, Random.FRandRange(0, 360), 0),
            FVector(X, Y, BaseZ + Size * 80.0f), FVector(Size * 0.32f, Size * 0.32f, Size * 0.6f)), true);
    }
}

// ---------------------------------------------------------------- spawning helpers

AHarvestableTreeActor* AVerticalSliceCWorldActor::SpawnTree(const FVector& Base, float Height, float TrunkScale, int32 Seed)
{
    FRandomStream Random(Seed);
    const FTransform Transform(FRotator(Random.FRandRange(-3.0f, 3.0f), Random.FRandRange(0, 360), Random.FRandRange(-3.0f, 3.0f)),
        Base + FVector(0, 0, 0.5f * Height - 20.0f));
    AHarvestableTreeActor* Tree = GetWorld()->SpawnActorDeferred<AHarvestableTreeActor>(
        AHarvestableTreeActor::StaticClass(), Transform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Tree)
        return nullptr;
    Tree->TreeMesh->SetStaticMesh(LoadEngineShape(TEXT("Cylinder")));
    Tree->TreeMesh->SetRelativeScale3D(FVector(TrunkScale, TrunkScale, Height / 100.0f));
    Tree->TreeMesh->SetMaterial(0, Tint(FLinearColor(0.11f + Random.FRandRange(-0.02f, 0.02f), 0.075f, 0.05f)));
    Tree->MaxChopHealth = 4.0f + TrunkScale * 4.0f;
    Tree->FallImpulse = 250000.0f;
    // The fallen trunk gives logs by its length: 600 cm logs, then one shorter piece.
    Tree->bYieldFromTrunkLength = true;
    Tree->LogActorClass = ALogBuildPiece::StaticClass();
    Tree->LogMeshVariantSetId = Family.VariantSetId;
    Tree->OutputProcessingMeshSets.Add(Family);

    // Overlapping crown layers of a conifer; the top one hides the trunk tip.
    UStaticMesh* Cone = LoadEngineShape(TEXT("Cone"));
    const int32 CrownLayers = Height > 1100.0f ? 6 : 5;
    const FLinearColor Needles(0.03f + Random.FRandRange(0.0f, 0.02f), 0.075f + Random.FRandRange(0.0f, 0.035f),
        0.035f + Random.FRandRange(0.0f, 0.015f));
    for (int32 Layer = 0; Layer < CrownLayers; ++Layer)
    {
        const float Alpha = Layer / FMath::Max(1.0f, CrownLayers - 1.0f);
        UStaticMeshComponent* Crown = NewObject<UStaticMeshComponent>(Tree);
        Crown->SetupAttachment(Tree->TreeMesh);
        Crown->SetUsingAbsoluteScale(true);
        Crown->SetStaticMesh(Cone);
        Crown->SetMaterial(0, Tint(Needles * (0.85f + 0.3f * Alpha)));
        Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // Cone centres from 38% to 90% of the height; each cone is 30% tall.
        const float Fraction = FMath::Lerp(0.38f, 1.0f, Alpha);
        // Tall trees grow upwards, not wider: the crown width stops growing at 8 m.
        const float Width = FMath::Lerp(0.5f, 0.15f, Alpha) * FMath::Min(Height, 800.0f) / 100.0f * Random.FRandRange(0.9f, 1.1f);
        Crown->SetRelativeLocation(FVector(0, 0, (Fraction - 0.5f) * 100.0f));
        Crown->SetRelativeRotation(FRotator(0, Random.FRandRange(0, 360), 0));
        Crown->SetWorldScale3D(FVector(Width, Width, 0.3f * Height / 100.0f));
        Tree->AddInstanceComponent(Crown);
    }
    Tree->FinishSpawning(Transform);
    for (UActorComponent* Component : Tree->GetComponents())
    {
        if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component); Mesh && Mesh != Tree->TreeMesh && !Mesh->IsRegistered())
            Mesh->RegisterComponent();
    }
    Trees.Add(Tree);
    return Tree;
}

ALogBuildPiece* AVerticalSliceCWorldActor::SpawnLog(const TCHAR* MeshName, const FVector& Centre, const FVector& Axis,
    bool bPlaced, float LengthCm)
{
    using namespace MyProject3Construction;
    UStaticMesh* Mesh = AVerticalSliceBWorldActor::LoadLogMesh(MeshName);
    if (!Mesh || !GetWorld())
        return nullptr;
    const FTransform Spawn(Centre);
    ALogBuildPiece* Log = GetWorld()->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Spawn,
        this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Log)
        return nullptr;
    Log->BaseDefinitionId = TEXT("Wood.Log.VerticalSliceC");
    Log->SelectedProcessingMeshSetId = Family.VariantSetId;
    Log->ProcessingMeshSets.Add(Family);
    Log->SetPieceStaticMesh(Mesh);
    Log->FinishSpawning(Spawn);
    Log->GetLogMesh()->SetSimulatePhysics(false);
    Log->SetActorTransform(MakeLogTransform(Log, Centre, MakeLogRotation(Log, Axis,
        FMath::Abs(Axis.GetSafeNormal().Z) > 0.9f ? FVector::XAxisVector : FVector::UpVector)));
    if (LengthCm > 0.0f)
        TrimLogToRange(Log, -0.5f * LengthCm, 0.5f * LengthCm);
    // Built pieces go in through the placement kernel like the player's; loose ones fall.
    if (!bPlaced || !CommitBuilt(Log))
        Log->GetLogMesh()->SetSimulatePhysics(true);
    return Log;
}

bool AVerticalSliceCWorldActor::CommitBuilt(ALogBuildPiece* Log, AActor* Primary, AActor* Secondary)
{
    UPlacementKernelSubsystem* Kernel = GetWorld() ? GetWorld()->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
    if (!Kernel || !Log)
        return false;
    FPlacementCommitOptions Options;
    Options.FromOwner = EWorldItemOwnerKind::World;
    Options.FromLifecycle = EWorldObjectLifecycleState::World;
    FString Reason;
    if (!Kernel->CommitPlacement(Log, Kernel->MakeDirectSolution(Log, Log->GetActorTransform(), TEXT("scene.built"), Primary, Secondary),
        Options, Reason))
    {
        UE_LOG(LogTemp, Warning, TEXT("VerticalSlice_C: %s was not built: %s"), *Log->GetName(), *Reason);
        return false;
    }
    PlacedLogs.AddUnique(Log);
    return true;
}

bool AVerticalSliceCWorldActor::CommitStacked(ALogBuildPiece* Log, ALogBuildPiece* Lower, const FVector& AimPoint, float CarryYaw)
{
    UPlacementKernelSubsystem* Kernel = GetWorld() ? GetWorld()->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
    if (!Kernel || !Log || !Lower)
        return false;
    FPlacementIntent Intent;
    Intent.Held = Log;
    Intent.AimActor = Lower;
    Intent.AimPoint = AimPoint;
    Intent.CarryYawDeg = CarryYaw;
    Intent.AllowedProfiles = { TEXT("log.parallel_stack") };
    const FPlacementSolution Solution = Kernel->Solve(Intent);
    FPlacementCommitOptions Options;
    Options.FromOwner = EWorldItemOwnerKind::World;
    Options.FromLifecycle = EWorldObjectLifecycleState::World;
    FString Reason = Solution.Reason;
    if (!Solution.bValid || !Kernel->CommitPlacement(Log, Solution, Options, Reason))
    {
        UE_LOG(LogTemp, Warning, TEXT("VerticalSlice_C: %s was not stacked: %s"), *Log->GetName(), *Reason);
        Log->GetLogMesh()->SetSimulatePhysics(true);
        return false;
    }
    PlacedLogs.AddUnique(Log);
    return true;
}

// ---------------------------------------------------------------- places

void AVerticalSliceCWorldActor::BuildForest()
{
    FRandomStream Random(2601);
    // A dense ring of forest closes the clearing in.
    for (int32 Attempt = 0; Attempt < 260 && Trees.Num() < 150; ++Attempt)
    {
        const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
        const float Radius = Random.FRandRange(2350.0f, 3500.0f);
        const float X = FMath::Clamp(Radius * FMath::Cos(Angle), -HalfExtent + 80, HalfExtent - 80);
        const float Y = FMath::Clamp(Radius * FMath::Sin(Angle), -HalfExtent + 80, HalfExtent - 80);
        if (!IsOpenGround(X, Y, 0.0f))
            continue;
        SpawnTree(FVector(X, Y, GetGroundHeight(X, Y)), Random.FRandRange(900.0f, 1350.0f), Random.FRandRange(0.34f, 0.5f), Attempt);
    }
    // Scattered trees inside, each a possible source of logs.
    for (int32 Attempt = 0; Attempt < 120 && Trees.Num() < 185; ++Attempt)
    {
        const float X = Random.FRandRange(-2300.0f, 2300.0f);
        const float Y = Random.FRandRange(-2300.0f, 2300.0f);
        if (!IsOpenGround(X, Y, 120.0f))
            continue;
        SpawnTree(FVector(X, Y, GetGroundHeight(X, Y)), Random.FRandRange(750.0f, 1150.0f), Random.FRandRange(0.3f, 0.44f), 1000 + Attempt);
    }
    // One old giant by the hill path: its trunk can hold a platform.
    SpawnTree(FVector(-1000.0f, 1450.0f, GetGroundHeight(-1000.0f, 1450.0f)), 1500.0f, 0.75f, 77);
}

void AVerticalSliceCWorldActor::BuildRocksAndGrass()
{
    FRandomStream Random(77);
    UStaticMesh* Sphere = LoadEngineShape(TEXT("Sphere"));
    auto Rock = [&](float X, float Y, float Size)
    {
        const FVector Scale(Size * Random.FRandRange(0.8f, 1.4f), Size * Random.FRandRange(0.7f, 1.2f), Size * Random.FRandRange(0.45f, 0.8f));
        const float Shade = Random.FRandRange(0.22f, 0.36f);
        AddProp(Sphere, FTransform(FRotator(Random.FRandRange(-15, 15), Random.FRandRange(0, 360), Random.FRandRange(-15, 15)),
            FVector(X, Y, GetGroundHeight(X, Y) + Scale.Z * 18.0f), Scale), FLinearColor(Shade, Shade * 0.96f, Shade * 0.9f), true);
    };
    // Boulders along the creek banks and under the cliff.
    for (int32 Index = 0; Index < 34; ++Index)
    {
        const float X = Random.FRandRange(-3300.0f, 3300.0f);
        const float Side = (Random.FRand() < 0.5f ? 1.0f : -1.0f);
        const float Y = GetCreekCentreY(X) + Side * Random.FRandRange(300.0f, 600.0f);
        if (FMath::Abs(X - BridgeX) > 300.0f)
            Rock(X, Y, Random.FRandRange(0.8f, 2.6f));
    }
    for (int32 Index = 0; Index < 12; ++Index)
        Rock(Random.FRandRange(-250.0f, 1650.0f), Random.FRandRange(1300.0f, 1420.0f), Random.FRandRange(0.9f, 2.2f));
    // Stepping stones at the ford.
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const float X = 2380.0f + Random.FRandRange(-60.0f, 60.0f);
        const float Y = GetCreekCentreY(X) - 220.0f + Index * 85.0f;
        Rock(X, Y, 0.9f);
    }

    // Grass tufts and pebbles give the ground texture.
    GrassClumps->SetStaticMesh(LoadEngineShape(TEXT("Cone")));
    GrassClumps->SetMaterial(0, Tint(FLinearColor(0.05f, 0.1f, 0.03f)));
    Pebbles->SetStaticMesh(Sphere);
    Pebbles->SetMaterial(0, Tint(FLinearColor(0.32f, 0.31f, 0.29f)));
    for (int32 Index = 0; Index < 2400; ++Index)
    {
        const float X = Random.FRandRange(-HalfExtent, HalfExtent);
        const float Y = Random.FRandRange(-HalfExtent, HalfExtent);
        const float Z = GetGroundHeight(X, Y);
        const float Creek = FMath::Abs(Y - GetCreekCentreY(X));
        if (Creek < 330.0f)
        {
            if (Creek < 230.0f && Index % 3 == 0)
                Pebbles->AddInstance(FTransform(FRotator(0, Random.FRandRange(0, 360), 0), FVector(X, Y, Z + 2.0f),
                    FVector(Random.FRandRange(0.05f, 0.18f)) * FVector(1.0f, 0.8f, 0.5f)), true);
            continue;
        }
        if (FVector2D::Distance(FVector2D(X, Y), CampCentre) < 450.0f ||
            FVector2D::Distance(FVector2D(X, Y), CabinCentre) < 330.0f ||
            FVector2D::Distance(FVector2D(X, Y), FarmPlotCentre) < 360.0f)
            continue;
        const float Height = Random.FRandRange(0.18f, 0.42f);
        GrassClumps->AddInstance(FTransform(FRotator(Random.FRandRange(-8, 8), Random.FRandRange(0, 360), 0),
            FVector(X, Y, Z + Height * 40.0f), FVector(Height * 0.8f, Height * 0.8f, Height * 0.55f)), true);
    }
}

void AVerticalSliceCWorldActor::BuildCamp()
{
    using namespace MyProject3Construction;
    UWorld* World = GetWorld();
    UStaticMesh* Sphere = LoadEngineShape(TEXT("Sphere"));

    // Fire pit ringed with stones.
    const FVector Fire(160.0f, -170.0f, GetGroundHeight(160.0f, -170.0f));
    for (int32 Index = 0; Index < 9; ++Index)
    {
        const float Angle = Index * 2.0f * PI / 9.0f;
        AddProp(Sphere, FTransform(FRotator(0, Index * 40.0f, 0), Fire + FVector(FMath::Cos(Angle) * 62.0f, FMath::Sin(Angle) * 62.0f, 6.0f),
            FVector(0.32f, 0.26f, 0.2f)), FLinearColor(0.2f, 0.19f, 0.18f), true);
    }
    AActor* FireActor = World->SpawnActor<AActor>(AActor::StaticClass(), Fire, FRotator::ZeroRotator);
    UStaticMeshComponent* Embers = NewObject<UStaticMeshComponent>(FireActor);
    Embers->SetStaticMesh(LoadEngineShape(TEXT("Cone")));
    Embers->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.18f));
    Embers->SetMaterial(0, Tint(FLinearColor(0.32f, 0.07f, 0.02f)));
    Embers->SetCollisionProfileName(TEXT("BlockAll"));
    FireActor->SetRootComponent(Embers);
    Embers->RegisterComponent();
    FireActor->SetActorLocation(Fire + FVector(0, 0, 20.0f));
    UHeatSourceComponent* Heat = NewObject<UHeatSourceComponent>(FireActor);
    Heat->SetupAttachment(Embers);
    Heat->HeatOutputCelsius = 26.0f;
    Heat->RadiusCm = 450.0f;
    Heat->RegisterComponent();
    Heat->AddFuel(1800.0f);
    Heat->Ignite();
    // A few split pieces left by the fire.
    for (int32 Index = 0; Index < 4; ++Index)
        SpawnLog(TEXT("SM_RawLog_75_Quarter_04"), Fire + FVector(-120.0f, 60.0f + Index * 22.0f, 20.0f), FVector(1, 0.2f * Index, 0), false);

    // A lean-to that has started to give way: one roof pole lies on the ground.
    // A 450 cm ridge rests on two posts at its Station.00 and Station.04 (300 cm
    // apart), and the roof poles lean on its Station.01-03.
    UPlacementKernelSubsystem* Kernel = World->GetSubsystem<UPlacementKernelSubsystem>();
    const float H = GetGroundHeight(-260.0f, 330.0f);
    constexpr float Module = 75.0f;
    const float RidgeStartX = -440.0f;
    ALogBuildPiece* PostA = SpawnLog(TEXT("SM_RawLog_300"), FVector(RidgeStartX + 0.5f * Module, 330.0f, H + 95.0f), FVector::UpVector, true, 190.0f);
    ALogBuildPiece* PostB = SpawnLog(TEXT("SM_RawLog_300"), FVector(RidgeStartX + 4.5f * Module, 330.0f, H + 95.0f), FVector::UpVector, true, 190.0f);
    ALogBuildPiece* Ridge = SpawnLog(TEXT("SM_RawLog_450"), FVector(RidgeStartX + 225.0f, 330.0f, H + 200.0f), FVector::XAxisVector, false);
    MyProject3Construction::FLogGeometry PostShape, RidgeShape;
    if (PostA && PostB && Ridge && Kernel && MyProject3Construction::GetLogGeometry(PostA, PostShape) &&
        MyProject3Construction::GetLogGeometry(Ridge, RidgeShape))
    {
        Ridge->GetLogMesh()->SetSimulatePhysics(false);
        Ridge->SetActorLocation(Ridge->GetActorLocation() +
            FVector(0, 0, FMath::Max(PostShape.EndA().Z, PostShape.EndB().Z) + RidgeShape.Radius - RidgeShape.Centre.Z));
        CommitBuilt(Ridge, PostA, PostB);
        for (int32 Index = 1; Index <= 3; ++Index)
        {
            FTransform Frame;
            if (!Kernel->GetFeatureFrame(Ridge, FName(*FString::Printf(TEXT("Station.%02d"), Index)),
                RidgeShape.Centre + FVector(0, 0, 1000.0f), Frame))
                continue;
            const FVector Down = FVector(0.0f, 1.0f, -1.05f).GetSafeNormal();
            ALogBuildPiece* Pole = SpawnLog(TEXT("SM_RawLog_300"), Frame.GetLocation(), Down, false, 270.0f);
            MyProject3Construction::FLogGeometry PoleShape;
            if (!Pole || !MyProject3Construction::GetLogGeometry(Pole, PoleShape))
                continue;
            Pole->GetLogMesh()->SetSimulatePhysics(false);
            // Its upper end on the ridge's top at that station.
            Pole->SetActorLocation(Pole->GetActorLocation() + Down * PoleShape.HalfLength + FVector(0, 0, PoleShape.Radius));
            CommitBuilt(Pole, Ridge);
        }
    }
    SpawnLog(TEXT("SM_RawLog_300"), FVector(-80.0f, 560.0f, H + 20.0f), FVector(1, 0.35f, 0), false, 270.0f);

    // A small log store with a little left in it, and a sled.
    auto SpawnContainer = [World](ELogContainerKind Kind, const FVector& Location, float Yaw)
    {
        ALogContainerActor* Container = World->SpawnActorDeferred<ALogContainerActor>(ALogContainerActor::StaticClass(),
            FTransform(FRotator(0, Yaw, 0), Location));
        if (!Container)
            return static_cast<ALogContainerActor*>(nullptr);
        Container->ConfigureKind(Kind);
        Container->FinishSpawning(FTransform(FRotator(0, Yaw, 0), Location, Container->GetActorScale3D()));
        return Container;
    };
    if (ALogContainerActor* Store = SpawnContainer(ELogContainerKind::SmallLogStorage,
        FVector(420.0f, 380.0f, GetGroundHeight(420.0f, 380.0f) + 10.0f), 20.0f))
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (ALogBuildPiece* Log = SpawnLog(TEXT("SM_RawLog_300"), FVector(900.0f, 900.0f + Index * 60.0f, 300.0f), FVector::XAxisVector, false))
                Store->TryStore(Log);
        }
    }
    SpawnContainer(ELogContainerKind::FirewoodStorage, FVector(330.0f, -420.0f, GetGroundHeight(330.0f, -420.0f) + 10.0f), -30.0f);
    SpawnContainer(ELogContainerKind::BasicSled, FVector(620.0f, -60.0f, GetGroundHeight(620.0f, -60.0f) + 12.0f), 70.0f);

    // Felled logs lying where they were cut.
    SpawnLog(TEXT("SM_RawLog_600"), FVector(-600.0f, -380.0f, 40.0f), FVector(1, -0.3f, 0), false);
    SpawnLog(TEXT("SM_RawLog_600"), FVector(760.0f, 420.0f, 40.0f), FVector(0.4f, 1, 0), false);
    SpawnLog(TEXT("SM_RawLog_300"), FVector(-380.0f, -650.0f, 40.0f), FVector(1, 0.9f, 0), false);
}

void AVerticalSliceCWorldActor::BuildCabin()
{
    // Six rows of notched walls (the most a log wall stands), no door, no window
    // and no roof yet, built the way the player stacks: 600 cm logs along X and
    // 450 cm logs along Y, crossing at stations (Station.00 and Station.07 of the
    // long logs, Station.00 and Station.05 of the short ones), so the walls stand
    // 525 x 375 cm apart. Every row rests on both walls under it.
    const float Base = GetGroundHeight(CabinCentre.X, CabinCentre.Y);
    constexpr float Module = 75.0f;
    const float HalfLong = 2.5f * Module;   // Long walls at Y = centre +- this.
    const float HalfShort = 3.5f * Module;  // Short walls at X = centre +- this.
    ALogBuildPiece* Below[2] = {};
    for (int32 Row = 0; Row < 6; ++Row)
    {
        ALogBuildPiece* Longs[2] = {};
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float Y = CabinCentre.Y + (Side == 0 ? -HalfLong : HalfLong);
            ALogBuildPiece* Long = SpawnLog(TEXT("SM_RawLog_600"), FVector(CabinCentre.X, Y, Base + 600.0f + Row * 80.0f), FVector::XAxisVector, false);
            if (!Long)
                continue;
            Long->GetLogMesh()->SetSimulatePhysics(false);
            if (Row == 0)
            {
                MyProject3Construction::FLogGeometry Shape;
                MyProject3Construction::GetLogGeometry(Long, Shape);
                Long->SetActorLocation(Long->GetActorLocation() + FVector(0, 0, Base + Shape.Radius - Shape.Centre.Z));
                CommitBuilt(Long);
            }
            else if (Below[0])
            {
                // Across the short walls, crossing them at their end stations.
                CommitStacked(Long, Below[0], FVector(CabinCentre.X, Y, Base), 0.0f);
            }
            Longs[Side] = Long;
        }
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float X = CabinCentre.X + (Side == 0 ? -HalfShort : HalfShort);
            ALogBuildPiece* Short = SpawnLog(TEXT("SM_RawLog_450"), FVector(X, CabinCentre.Y, Base + 640.0f + Row * 80.0f), FVector::YAxisVector, false);
            if (!Short || !Longs[0])
                continue;
            Short->GetLogMesh()->SetSimulatePhysics(false);
            CommitStacked(Short, Longs[0], FVector(X, CabinCentre.Y, Base), 90.0f);
            Below[Side] = Short;
        }
    }
    // Split boards for a door and long poles for the roof, stacked by the cabin.
    for (int32 Index = 0; Index < 5; ++Index)
        SpawnLog(TEXT("SM_RawLog_300_Plank"), FVector(-1000.0f, -60.0f + Index * 30.0f, Base + 15.0f + Index * 3.0f), FVector::XAxisVector, false);
    for (int32 Index = 0; Index < 7; ++Index)
        SpawnLog(TEXT("SM_RawLog_600"), FVector(-1720.0f, -120.0f + Index * 42.0f, Base + 25.0f), FVector(1, 0.05f, 0), false);
}

void AVerticalSliceCWorldActor::BuildOldBridge()
{
    UWorld* World = GetWorld();
    // Two posts on each bank; only one rope survived. A fallen log lies near.
    const float CreekY = GetCreekCentreY(BridgeX);
    ALogBuildPiece* Posts[2][2] = {};
    for (int32 Bank = 0; Bank < 2; ++Bank)
    {
        const float Y = CreekY + (Bank == 0 ? 360.0f : -360.0f);
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float X = BridgeX + (Side == 0 ? -65.0f : 65.0f);
            Posts[Bank][Side] = SpawnLog(TEXT("SM_RawLog_300"), FVector(X, Y, GetGroundHeight(X, Y) + 60.0f), FVector::UpVector, true, 150.0f);
        }
    }
    if (Posts[0][0] && Posts[1][0])
    {
        FVector OriginA, ExtentA, OriginB, ExtentB;
        Posts[0][0]->GetActorBounds(true, OriginA, ExtentA);
        Posts[1][0]->GetActorBounds(true, OriginB, ExtentB);
        const FVector Top0(OriginA.X, OriginA.Y, OriginA.Z + ExtentA.Z - 15.0f);
        const FVector Top1(OriginB.X, OriginB.Y, OriginB.Z + ExtentB.Z - 15.0f);
        if (ARopeLineActor* Rope = World->SpawnActor<ARopeLineActor>(0.5f * (Top0 + Top1), FRotator::ZeroRotator))
            Rope->InitializeRope(Top0, Posts[0][0], Top1, Posts[1][0]);
    }
    // Planks that fell from the old deck; one is in the creek.
    for (int32 Index = 0; Index < 6; ++Index)
        SpawnLog(TEXT("SM_RawLog_300_Plank"), FVector(BridgeX + 260.0f, CreekY + 440.0f + Index * 28.0f, 60.0f), FVector(0.1f, 1, 0), false);
    SpawnLog(TEXT("SM_RawLog_300_Plank"), FVector(BridgeX - 150.0f, CreekY, WaterLevel + 10.0f), FVector(1, 0.4f, 0), false);
    SpawnLog(TEXT("SM_RawLog_600"), FVector(BridgeX - 480.0f, CreekY + 470.0f, 50.0f), FVector(0.2f, 1, 0), false);
}

void AVerticalSliceCWorldActor::BuildHill()
{
    UWorld* World = GetWorld();
    // Old stair stringers lean against the cliff terrace; the steps are gone.
    for (const float X : { 300.0f, 420.0f })
    {
        const FVector Bottom(X, 1380.0f, GetGroundHeight(X, 1380.0f) + 18.0f);
        const FVector Top(X, 1640.0f, GetGroundHeight(X, 1640.0f) + 18.0f);
        SpawnLog(TEXT("SM_RawLog_600"), 0.5f * (Bottom + Top), Top - Bottom, true, FVector::Dist(Bottom, Top) + 30.0f);
    }
    for (int32 Index = 0; Index < 7; ++Index)
        SpawnLog(TEXT("SM_RawLog_150_Plank"), FVector(200.0f + Index * 25.0f, 1200.0f, GetGroundHeight(200.0f, 1200.0f) + 30.0f), FVector(1, 0.15f, 0), false);

    // The lookout: a tall pole with an old zipline down to the camp store.
    const FVector LookBase(LookoutPoint.X, LookoutPoint.Y, GetGroundHeight(LookoutPoint.X, LookoutPoint.Y));
    if (ALogBuildPiece* Mast = SpawnLog(TEXT("SM_RawLog_600"), LookBase + FVector(0, 0, 290.0f), FVector::UpVector, true, 560.0f))
    {
        const FVector Top = LookBase + FVector(0, 0, 520.0f);
        const FVector Bottom(470.0f, 520.0f, GetGroundHeight(470.0f, 520.0f) + 90.0f);
        if (ARopeLineActor* Zipline = World->SpawnActor<ARopeLineActor>(0.5f * (Top + Bottom), FRotator::ZeroRotator))
            Zipline->InitializeRope(Top, Mast, Bottom, nullptr);
    }
    // Wood cut up on the hill, waiting to be sent down.
    for (int32 Index = 0; Index < 3; ++Index)
        SpawnLog(TEXT("SM_RawLog_300"), LookBase + FVector(-220.0f, -120.0f + Index * 50.0f, 30.0f), FVector(1, 0.1f, 0), false);
}

void AVerticalSliceCWorldActor::BuildPalisade()
{
    // An old stake wall facing the woods; part fallen, a gap where a gate stood.
    FRandomStream Random(9);
    int32 Index = 0;
    for (float Y = -540.0f; Y <= 540.0f; Y += 42.0f, ++Index)
    {
        if (FMath::Abs(Y) < 70.0f)
            continue;
        const float X = PalisadeX + 12.0f * FMath::Sin(Y * 0.01f);
        if (Index % 9 == 4)
        {
            SpawnLog(TEXT("SM_RawLog_300"), FVector(X + 140.0f, Y + 40.0f, GetGroundHeight(X + 140.0f, Y) + 40.0f), FVector(1, 0.2f, 0), false);
            continue;
        }
        const float Height = Random.FRandRange(240.0f, 300.0f);
        ALogBuildPiece* Stake = SpawnLog(TEXT("SM_RawLog_300"), FVector(X, Y, GetGroundHeight(X, Y) + 0.5f * Height - 30.0f),
            FVector(Random.FRandRange(-0.04f, 0.04f), 0.0f, 1.0f), true, Height);
        if (Stake && Index % 3 != 0)
        {
            USharpenedTipComponent* Tip = NewObject<USharpenedTipComponent>(Stake);
            Tip->RegisterComponent();
            Tip->InitializeTip();
        }
    }
}

void AVerticalSliceCWorldActor::BuildLife()
{
    UWorld* World = GetWorld();
    FRandomStream Random(31);
    auto Spawn = [&](EWildlifeKind Kind, float X, float Y)
    {
        const FVector Location(X, Y, GetGroundHeight(X, Y) + 40.0f);
        AWildlifeActor* Animal = World->SpawnActorDeferred<AWildlifeActor>(AWildlifeActor::StaticClass(), FTransform(Location));
        if (!Animal)
            return;
        Animal->ConfigureKind(Kind);
        if (Kind == EWildlifeKind::SmallAnimal)
            Animal->WanderRadius = 500.0f;
        Animal->FinishSpawning(FTransform(FRotator(0, Random.FRandRange(0, 360), 0), Location, Animal->GetActorScale3D()));
    };
    int32 Rabbits = 0, Bushes = 0;
    for (int32 Attempt = 0; Attempt < 200 && (Rabbits < 6 || Bushes < 14); ++Attempt)
    {
        const float X = Random.FRandRange(-2400.0f, 2400.0f);
        const float Y = Random.FRandRange(-2600.0f, 2200.0f);
        if (!IsOpenGround(X, Y, 50.0f))
            continue;
        if (Bushes < 14)
        {
            Spawn(EWildlifeKind::Plant, X, Y);
            ++Bushes;
        }
        else
        {
            Spawn(EWildlifeKind::SmallAnimal, X, Y);
            ++Rabbits;
        }
    }
    // Someone moves in the eastern woods beyond the stake wall.
    Spawn(EWildlifeKind::Enemy, 2950.0f, 250.0f);

    // Stones by the creek and sticks under the trees.
    for (int32 Index = 0; Index < 9; ++Index)
    {
        const float X = Random.FRandRange(-1200.0f, 1800.0f);
        const float Y = GetCreekCentreY(X) + Random.FRandRange(330.0f, 520.0f);
        const FVector Location(X, Y, GetGroundHeight(X, Y) + 30.0f);
        AStoneBuildingPiece* Stone = World->SpawnActorDeferred<AStoneBuildingPiece>(AStoneBuildingPiece::StaticClass(), FTransform(Location), this);
        if (!Stone)
            continue;
        Stone->Mesh->SetStaticMesh(LoadEngineShape(TEXT("Sphere")));
        Stone->DefinitionId = TEXT("Stone.Raw.VerticalSliceC");
        const FVector Scale(Random.FRandRange(0.3f, 0.45f), Random.FRandRange(0.25f, 0.38f), Random.FRandRange(0.18f, 0.26f));
        Stone->FinishSpawning(FTransform(FRotator(0, Random.FRandRange(0, 360), 0), Location, Scale));
        Stone->Mesh->SetMaterial(0, Tint(FLinearColor(0.34f, 0.33f, 0.31f)));
    }
    for (int32 Index = 0; Index < 14; ++Index)
    {
        const float X = Random.FRandRange(-1800.0f, 1600.0f);
        const float Y = Random.FRandRange(-700.0f, 1200.0f);
        if (FVector2D::Distance(FVector2D(X, Y), CabinCentre) < 450.0f)
            continue;
        const FVector Location(X, Y, GetGroundHeight(X, Y) + 25.0f);
        const FRotator Rotation(90.0f, Random.FRandRange(0, 360), 0);
        AStickPrimitivePiece* Stick = World->SpawnActorDeferred<AStickPrimitivePiece>(AStickPrimitivePiece::StaticClass(),
            FTransform(Rotation, Location), this);
        if (!Stick)
            continue;
        Stick->Mesh->SetStaticMesh(LoadEngineShape(TEXT("Cylinder")));
        Stick->DefinitionId = TEXT("Stick.Raw.VerticalSliceC");
        Stick->FinishSpawning(FTransform(Rotation, Location, FVector(0.06f, 0.06f, Random.FRandRange(1.0f, 1.5f))));
        Stick->Mesh->SetMaterial(0, Tint(FLinearColor(0.26f, 0.18f, 0.1f)));
    }

    // Gatherable nodes are placed in the biome that explains the resource:
    // berries in the clearing, reeds beside the creek, flint on the rocky bank,
    // clay/sand along the exposed lower bank. These are real inventory resources,
    // not decorative props; G gathers the nearest node.
    struct FGatherNode
    {
        FName ItemId;
        FVector2D XY;
        const TCHAR* Shape;
        FVector Scale;
        FLinearColor Colour;
        int32 Units;
        int32 PerGather;
        double RefillSeconds;
        FName ToolId;
        bool bLedger;
    };
    const FGatherNode Nodes[] =
    {
        { TEXT("Berries"), FVector2D(-900, 650), TEXT("Sphere"), FVector(0.55f,0.55f,0.65f), FLinearColor(0.42f,0.06f,0.12f), 5, 2, 21600.0, NAME_None, true },
        { TEXT("Berries"), FVector2D(-1150, 820), TEXT("Sphere"), FVector(0.45f,0.45f,0.55f), FLinearColor(0.42f,0.06f,0.12f), 5, 2, 21600.0, NAME_None, true },
        { TEXT("Reed"), FVector2D(1150, -950), TEXT("Cylinder"), FVector(0.12f,0.12f,0.85f), FLinearColor(0.28f,0.36f,0.12f), 8, 2, 43200.0, NAME_None, false },
        { TEXT("Reed"), FVector2D(1350, -1050), TEXT("Cylinder"), FVector(0.10f,0.10f,0.75f), FLinearColor(0.28f,0.36f,0.12f), 8, 2, 43200.0, NAME_None, false },
        { TEXT("Flint"), FVector2D(1750, 950), TEXT("Sphere"), FVector(0.32f,0.28f,0.22f), FLinearColor(0.22f,0.23f,0.25f), 3, 1, 0.0, NAME_None, false },
        { TEXT("Clay"), FVector2D(-1750, -650), TEXT("Sphere"), FVector(0.65f,0.5f,0.18f), FLinearColor(0.43f,0.24f,0.13f), 5, 1, 86400.0, NAME_None, false },
        { TEXT("Sand"), FVector2D(2050, -750), TEXT("Sphere"), FVector(0.7f,0.55f,0.12f), FLinearColor(0.57f,0.49f,0.32f), 5, 1, 86400.0, NAME_None, false },
        { TEXT("Resin"), FVector2D(1450, 1200), TEXT("Sphere"), FVector(0.18f,0.18f,0.18f), FLinearColor(0.48f,0.27f,0.06f), 3, 1, 0.0, TEXT("Tool.Scraper"), false }
    };
    for (const FGatherNode& Node : Nodes)
    {
        const FVector Location(Node.XY.X, Node.XY.Y, GetGroundHeight(Node.XY.X, Node.XY.Y) + 22.0f);
        AGatherableResourceActor* Resource = World->SpawnActor<AGatherableResourceActor>(
            AGatherableResourceActor::StaticClass(), Location, FRotator::ZeroRotator);
        if (!Resource)
            continue;
        Resource->ItemDefinitionId = Node.ItemId;
        Resource->MaxUnits = Node.Units;
        Resource->UnitsPerGather = Node.PerGather;
        Resource->RefillSecondsPerUnit = Node.RefillSeconds;
        Resource->RequiredToolId = Node.ToolId;
        Resource->bGiveLedgerItems = Node.bLedger;
        Resource->Mesh->SetStaticMesh(LoadEngineShape(Node.Shape));
        Resource->Mesh->SetRelativeScale3D(Node.Scale);
        Resource->Mesh->SetMaterial(0, Tint(Node.Colour));
        Resource->Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    }
}

// ---------------------------------------------------------------- creek

FVector AVerticalSliceCWorldActor::GetCreekFlow(float X) const
{
    const float Slope = 260.0f * 0.0011f * FMath::Cos(X * 0.0011f + 0.6f);
    // Fast in the deep channel, slowing where it spreads out over the ford.
    const float Speed = FMath::Lerp(110.0f, 25.0f, Smooth(1900.0f, 2500.0f, X));
    return FVector(1.0f, Slope, 0.0f).GetSafeNormal() * Speed;
}

bool AVerticalSliceCWorldActor::IsInCreek(const FVector& Point) const
{
    return FMath::Abs(Point.X) < HalfExtent && FMath::Abs(Point.Y - GetCreekCentreY(Point.X)) < 270.0f &&
        GetGroundHeight(Point.X, Point.Y) < WaterLevel;
}

void AVerticalSliceCWorldActor::UpdateCreek(float DeltaSeconds)
{
    UWorld* World = GetWorld();
    if (!World || DeltaSeconds <= 0.0f)
        return;

    // Loose logs in the water float half under and drift with the current.
    int32 Drifting = 0;
    for (TActorIterator<ALogBuildPiece> It(World); It; ++It)
    {
        ALogBuildPiece* Log = *It;
        UStaticMeshComponent* LogMesh = Log->GetLogMesh();
        if (Log->IsHeld() || Log->IsPlaced() || !LogMesh || !LogMesh->IsSimulatingPhysics())
            continue;
        const FBoxSphereBounds Bounds = LogMesh->Bounds;
        const float Bottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;
        if (Bottom >= WaterLevel || !IsInCreek(Bounds.Origin))
            continue;
        ++Drifting;
        const float Submerged = FMath::Clamp((WaterLevel - Bottom) / FMath::Max(2.0f * Bounds.BoxExtent.Z, 1.0f), 0.0f, 1.0f);
        const float Mass = LogMesh->GetMass();
        const FVector Velocity = LogMesh->GetPhysicsLinearVelocity();
        const FVector Flow = GetCreekFlow(Bounds.Origin.X);
        FVector Force(0.0f, 0.0f, Mass * 980.0f * 2.0f * Submerged);
        Force += Mass * Submerged * FVector((Flow.X - Velocity.X) * 1.2f, (Flow.Y - Velocity.Y) * 1.2f, -Velocity.Z * 2.5f);
        LogMesh->AddForce(Force);
        LogMesh->SetPhysicsAngularVelocityInDegrees(
            LogMesh->GetPhysicsAngularVelocityInDegrees() * FMath::Max(0.0f, 1.0f - 2.0f * DeltaSeconds));
    }

    // A sled floats, even loaded; left in the water it drifts too.
    for (TActorIterator<ALogContainerActor> It(World); It; ++It)
    {
        if (!It->IsSled())
            continue;
        It->WaterSurfaceZ = WaterLevel;
        const FVector Location = It->GetActorLocation();
        if (It->IsTowed() || !IsInCreek(Location))
            continue;
        FVector Next = Location + GetCreekFlow(Location.X) * 0.6f * DeltaSeconds;
        Next.Z = FMath::Max(GetGroundHeight(Next.X, Next.Y), WaterLevel) + 12.0f;
        It->SetActorLocation(Next, true);
    }

    // Now and then a log comes down the creek from upstream, for a dam to catch.
    DriftSpawnTimer -= DeltaSeconds;
    if (DriftSpawnTimer <= 0.0f)
    {
        DriftSpawnTimer = 35.0f;
        if (Drifting < 3 && DriftSpawned < 40 && Family.BaseRoundMesh)
        {
            const float X = -3200.0f;
            const FVector Start(X, GetCreekCentreY(X), WaterLevel + 14.0f);
            if (SpawnLog(DriftSpawned % 3 == 0 ? TEXT("SM_RawLog_600") : TEXT("SM_RawLog_300"), Start,
                GetCreekFlow(X), false))
            {
                ++DriftSpawned;
            }
        }
    }
}

void AVerticalSliceCWorldActor::BuildCreekDam()
{
    // Downstream of the bridge, three old posts stand in the water where someone
    // began a log catcher. One gap is still open; a log is wedged against them.
    const float X = 1480.0f;
    const float CreekY = GetCreekCentreY(X);
    for (const float Offset : { -150.0f, -60.0f, 130.0f })
    {
        const float Y = CreekY + Offset;
        const float Bed = GetGroundHeight(X, Y);
        SpawnLog(TEXT("SM_RawLog_300"), FVector(X, Y, Bed + 115.0f), FVector(0.03f, 0.0f, 1.0f), true, 260.0f);
    }
    SpawnLog(TEXT("SM_RawLog_300"), FVector(X - 40.0f, CreekY - 100.0f, WaterLevel + 10.0f), FVector(0.15f, 1.0f, 0.0f), false);
}

void AVerticalSliceCWorldActor::BuildHelper()
{
    // A helper waits by the camp; J gives him orders.
    const FVector Location(260.0f, 180.0f, GetGroundHeight(260.0f, 180.0f));
    Helper = GetWorld()->SpawnActor<AHelperCompanionActor>(Location, FRotator(0.0f, -120.0f, 0.0f));
    if (!Helper)
        return;
    Helper->MinLogHeight = WaterLevel + 40.0f;
}

void AVerticalSliceCWorldActor::HelperOrderInput()
{
    if (!Helper)
        return;
    Helper->CycleOrder();
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor(160, 210, 255),
            FString::Printf(TEXT("Helper: %s"), *AHelperCompanionActor::GetOrderName(Helper->GetOrder())));
}

// ---------------------------------------------------------------- play

void AVerticalSliceCWorldActor::BuildFarmPlot()
{
    if (bFarmPlotBuilt || !GetWorld())
        return;

    UStaticMesh* SoilCube = LoadEngineShape(TEXT("Cube"));
    if (!SoilCube)
    {
        UE_LOG(LogTemp, Warning, TEXT("VerticalSlice_C: farm placeholder mesh is not available yet."));
        return;
    }

    FarmPlotTiles.Reset();
    FarmPlotColours.Reset();
    // Visible, non-blocking soil tiles keep the backend logical grid discoverable
    // without creating a large voxel field or interfering with traversal.
    for (int32 Y = 0; Y < FarmPlotCellsPerAxis; ++Y)
    {
        for (int32 X = 0; X < FarmPlotCellsPerAxis; ++X)
        {
            const float WorldX = FarmPlotCentre.X +
                (static_cast<float>(X) - 1.0f) * FarmPlotCellSize;
            const float WorldY = FarmPlotCentre.Y +
                (static_cast<float>(Y) - 1.0f) * FarmPlotCellSize;
            const FVector Location(WorldX, WorldY, GetGroundHeight(WorldX, WorldY) + 5.0f);
            const FTransform Transform(FRotator::ZeroRotator, Location, FVector(1.68f, 1.68f, 0.08f));
            const FLinearColor SoilColour = ((X + Y) % 2 == 0)
                ? FLinearColor(0.20f, 0.125f, 0.065f)
                : FLinearColor(0.16f, 0.095f, 0.045f);
            if (UStaticMeshComponent* Tile = AddProp(SoilCube, Transform, SoilColour, false))
            {
                FarmPlotTiles.Add(Tile);
                FarmPlotColours.Add(FLinearColor::Transparent);
            }
        }
    }

    bFarmPlotBuilt = FarmPlotTiles.Num() == FarmPlotCellsPerAxis * FarmPlotCellsPerAxis;
    RefreshFarmPlotVisuals();
}

void AVerticalSliceCWorldActor::RefreshFarmPlotVisuals()
{
    UWorld* World = GetWorld();
    const UWorldAgricultureSubsystem* Agriculture =
        World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    if (!bFarmPlotBuilt || !Agriculture ||
        FarmPlotTiles.Num() != FarmPlotCellsPerAxis * FarmPlotCellsPerAxis)
        return;

    for (int32 Y = 0; Y < FarmPlotCellsPerAxis; ++Y)
    {
        for (int32 X = 0; X < FarmPlotCellsPerAxis; ++X)
        {
            const int32 Index = Y * FarmPlotCellsPerAxis + X;
            if (!FarmPlotTiles.IsValidIndex(Index) || !IsValid(FarmPlotTiles[Index]))
                continue;

            const float WorldX = FarmPlotCentre.X +
                (static_cast<float>(X) - 1.0f) * FarmPlotCellSize;
            const float WorldY = FarmPlotCentre.Y +
                (static_cast<float>(Y) - 1.0f) * FarmPlotCellSize;
            const FIntVector LogicalCell(
                FMath::RoundToInt(WorldX / 100.0f),
                FMath::RoundToInt(WorldY / 100.0f),
                FMath::RoundToInt(GetGroundHeight(WorldX, WorldY) / 100.0f));
            FWorldFieldCellState Cell;
            const bool bExists = Agriculture->GetFieldCell(LogicalCell, Cell);
            FLinearColor Colour = ((X + Y) % 2 == 0)
                ? FLinearColor(0.20f, 0.125f, 0.065f)
                : FLinearColor(0.16f, 0.095f, 0.045f);
            if (bExists)
            {
                if (Cell.bCleared)
                    Colour = FMath::Lerp(Colour, FLinearColor(0.27f, 0.18f, 0.095f), 0.45f);
                if (Cell.bTilled)
                    Colour = FMath::Lerp(Colour, FLinearColor(0.11f, 0.065f, 0.032f), 0.65f);
                if (Cell.Mulch > 0.0f)
                    Colour = FMath::Lerp(Colour, FLinearColor(0.33f, 0.22f, 0.09f),
                        FMath::Clamp(Cell.Mulch, 0.0f, 1.0f) * 0.55f);
                if (Cell.Fertility > 0.5f)
                    Colour = FMath::Lerp(Colour, FLinearColor(0.20f, 0.27f, 0.075f),
                        FMath::Clamp((Cell.Fertility - 0.5f) * 0.5f, 0.0f, 0.25f));
                if (Cell.bFenced)
                    Colour = FMath::Lerp(Colour, FLinearColor(0.28f, 0.22f, 0.10f), 0.18f);
            }
            if (!FarmPlotColours.IsValidIndex(Index) ||
                !FarmPlotColours[Index].Equals(Colour, 0.001f))
            {
                FarmPlotTiles[Index]->SetMaterial(0, Tint(Colour));
                if (FarmPlotColours.IsValidIndex(Index))
                    FarmPlotColours[Index] = Colour;
            }
        }
    }
}

bool AVerticalSliceCWorldActor::BuildWorld()
{
    if (bBuilt || !GetWorld())
        return bBuilt;
    bBuilt = true;
    Family = AVerticalSliceBWorldActor::BuildLogMeshFamily();
    BuildGround();
    BuildWater();
    BuildBackdrop();
    BuildRocksAndGrass();
    if (Family.BaseRoundMesh)
    {
        BuildForest();
        BuildCamp();
        BuildCabin();
        BuildOldBridge();
        BuildHill();
        BuildPalisade();
        BuildCreekDam();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VerticalSlice_C: log meshes missing under /Game/DemoAssets/Logs."));
    }
    BuildLife();
    BuildFarmPlot();
    BuildHelper();

    // Phase 4: make the data-driven work stations discoverable in the continuous
    // world instead of leaving their records accessible only to tests/tools.
    // Primitive meshes are intentional placeholders; registration still uses
    // the same station IDs and recipe data as the production subsystem.
    if (UWorld* World = GetWorld())
    {
        struct FStationPlacement
        {
            const TCHAR* Id;
            FVector Location;
            FVector Scale;
            const TCHAR* Shape;
            FLinearColor Colour;
        };
        const FStationPlacement Stations[] =
        {
            { TEXT("workbench_log"), FVector(-250.0f, -420.0f, GetGroundHeight(-250.0f, -420.0f)), FVector(1.5f, 0.55f, 0.28f), TEXT("Cube"), FLinearColor(0.22f, 0.12f, 0.055f) },
            { TEXT("pit_kiln"), FVector(520.0f, -420.0f, GetGroundHeight(520.0f, -420.0f)), FVector(0.75f, 0.75f, 0.35f), TEXT("Sphere"), FLinearColor(0.28f, 0.22f, 0.16f) },
            { TEXT("charcoal_pit"), FVector(680.0f, -520.0f, GetGroundHeight(680.0f, -520.0f)), FVector(0.9f, 0.65f, 0.22f), TEXT("Cylinder"), FLinearColor(0.12f, 0.11f, 0.10f) },
            { TEXT("smoke_rack"), FVector(-420.0f, -500.0f, GetGroundHeight(-420.0f, -500.0f)), FVector(0.8f, 0.12f, 0.85f), TEXT("Cube"), FLinearColor(0.28f, 0.19f, 0.10f) },
            { TEXT("drying_rack"), FVector(-520.0f, -300.0f, GetGroundHeight(-520.0f, -300.0f)), FVector(0.8f, 0.12f, 0.7f), TEXT("Cube"), FLinearColor(0.38f, 0.28f, 0.15f) },
            { TEXT("grinding_stone"), FVector(360.0f, -520.0f, GetGroundHeight(360.0f, -520.0f)), FVector(0.55f, 0.55f, 0.22f), TEXT("Sphere"), FLinearColor(0.27f, 0.27f, 0.25f) },
            { TEXT("hide_frame"), FVector(-600.0f, -480.0f, GetGroundHeight(-600.0f, -480.0f)), FVector(0.7f, 0.12f, 0.8f), TEXT("Cube"), FLinearColor(0.3f, 0.2f, 0.12f) },
            { TEXT("threshing_floor"), FVector(900.0f, -380.0f, GetGroundHeight(900.0f, -380.0f)), FVector(1.25f, 1.0f, 0.08f), TEXT("Cylinder"), FLinearColor(0.42f, 0.32f, 0.19f) },
            { TEXT("compost_heap"), FVector(-750.0f, -650.0f, GetGroundHeight(-750.0f, -650.0f)), FVector(0.7f, 0.7f, 0.45f), TEXT("Sphere"), FLinearColor(0.18f, 0.14f, 0.08f) }
        };
        for (const FStationPlacement& Placement : Stations)
        {
            AProductionStationActor* Station = World->SpawnActor<AProductionStationActor>(
                AProductionStationActor::StaticClass(), Placement.Location, FRotator::ZeroRotator);
            if (!Station)
                continue;
            Station->StationDataId = FName(Placement.Id);
            if (UStaticMeshComponent* Mesh = Station->Mesh)
            {
                Mesh->SetStaticMesh(LoadEngineShape(Placement.Shape));
                Mesh->SetRelativeScale3D(Placement.Scale);
                Mesh->SetMaterial(0, Tint(Placement.Colour));
                Mesh->SetCollisionProfileName(TEXT("BlockAll"));
            }
            Station->EnsureRegistered();
        }
    }
    return true;
}

void AVerticalSliceCWorldActor::BeginPlay()
{
    Super::BeginPlay();
    BuildWorld();
    // -StartHour=21 starts at night; -NoStartView keeps a camera set from the command line.
    FParse::Value(FCommandLine::Get(), TEXT("StartHour="), StartHour);
    Hours = StartHour;
    UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
    const UWorldClimateSubsystem* Climate =
        GetWorld()->GetSubsystem<UWorldClimateSubsystem>();
    if (Survival && (!Climate || !Climate->IsClimateInitialized()))
        Survival->SetWeather(EWorldWeatherState::Wind, 0.35f);
    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
    {
        Sun = *It;
        break;
    }
    // Haze softens the distance and hides where the far forest ends.
    for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
    {
        UExponentialHeightFogComponent* Fog = It->GetComponent();
        Fog->SetFogDensity(0.035f);
        Fog->SetFogHeightFalloff(0.12f);
        Fog->SetFogInscatteringColor(FLinearColor(0.42f, 0.5f, 0.58f));
        Fog->SetVolumetricFog(true);
    }
}

void AVerticalSliceCWorldActor::EnsureStarterTools()
{
    if (bStarterToolsGranted)
        return;

    UWorld* World = GetWorld();
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    if (!World || !GameData || !GameData->IsApplied() || !Pawn || !Inventory)
        return;

    // The loader backfills inventories that existed before ApplyToWorld, and
    // inventory BeginPlay applies data when it starts after the loader. Only
    // retry this fallback if the required weights are still absent; do not
    // rebuild/compare every hand recipe on every Tick while the bag is full.
    if ((!Inventory->ItemWeightGrams.Contains(TEXT("Tool.Axe")) ||
         !Inventory->ItemWeightGrams.Contains(TEXT("Tool.DiggingStick"))) &&
        !GameData->ApplyToInventory(Inventory))
        return;

    TArray<FInventoryStack> MissingTools;
    const FName ToolIds[] = { FName(TEXT("Tool.Axe")), FName(TEXT("Tool.DiggingStick")) };
    for (const FName ToolId : ToolIds)
    {
        if (Inventory->GetItemCount(ToolId) > 0)
            continue;

        float Strength = 0.0f;
        if (!GameData->GetToolStrength(ToolId, Strength))
            return;

        FInventoryStack Stack;
        Stack.ItemId = ToolId;
        Stack.Quantity = 1;
        Stack.Category = EInventoryItemCategory::Tool;
        Stack.bIsTool = true;
        Stack.MaxStack = 1;
        MissingTools.Add(Stack);
    }

    if (!MissingTools.IsEmpty() && !Inventory->CanAddItems(MissingTools))
        return;
    for (const FInventoryStack& Stack : MissingTools)
    {
        if (!Inventory->TryAddItem(Stack))
            return; // Capacity was preflighted; retry on the next Tick if the state changed.
    }

    // Prototype C starts with the visible stone axe and primitive digging stick.
    // They are normal inventory tool stacks, consistent with hand-craft outputs.
    bStarterToolsGranted =
        Inventory->GetItemCount(TEXT("Tool.Axe")) > 0 &&
        Inventory->GetItemCount(TEXT("Tool.DiggingStick")) > 0;
}

void AVerticalSliceCWorldActor::AssignGatherableZones()
{
    if (bGatherableZonesAssigned)
        return;
    UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !GameData || !GameData->IsApplied() || GameData->GetZones().IsEmpty())
        return;

    bool bAllAssigned = true;
    for (TActorIterator<AGatherableResourceActor> It(World); It; ++It)
    {
        AGatherableResourceActor* Resource = *It;
        if (!IsValid(Resource) || !Resource->ZoneId.IsNone())
            continue;
        const FGameDataZone* Zone = GameData->FindZoneAtLocation(Resource->GetActorLocation());
        if (Zone)
            Resource->ZoneId = Zone->Id;
        else
            bAllAssigned = false;
    }
    bGatherableZonesAssigned = bAllAssigned;
}

void AVerticalSliceCWorldActor::EnsureDiscoveryMarkers()
{
    if (bDiscoveryMarkersSpawned)
        return;
    UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !GameData || !GameData->IsApplied() || GameData->GetDiscoveries().IsEmpty())
        return;

    bool bAllMarkersExist = true;
    for (const FGameDataDiscovery& Definition : GameData->GetDiscoveries())
    {
        if (Definition.Id.IsNone())
        {
            bAllMarkersExist = false;
            continue;
        }

        bool bAlreadySpawned = false;
        for (TActorIterator<AWorldDiscoveryMarkerActor> It(World); It; ++It)
        {
            if (IsValid(*It) && (*It)->DiscoveryId == Definition.Id)
            {
                bAlreadySpawned = true;
                break;
            }
        }
        if (bAlreadySpawned)
            continue;

        const FVector MarkerLocation(Definition.Location.X, Definition.Location.Y,
            GetGroundHeight(Definition.Location.X, Definition.Location.Y) + 38.0f);
        AWorldDiscoveryMarkerActor* Marker = World->SpawnActor<AWorldDiscoveryMarkerActor>(
            AWorldDiscoveryMarkerActor::StaticClass(), MarkerLocation, FRotator::ZeroRotator);
        if (!Marker)
        {
            bAllMarkersExist = false;
            continue;
        }

        Marker->DiscoveryId = Definition.Id;
        Marker->DisplayName = Definition.Name;
        Marker->RevealRadiusCm = Definition.RevealRadiusCm;
        Marker->RewardItemId = Definition.RewardItemId;
        Marker->RewardQuantity = Definition.RewardQuantity;
        Marker->bRewardUniqueItems = Definition.bRewardUniqueItems;
    }
    bDiscoveryMarkersSpawned = bAllMarkersExist;
}

void AVerticalSliceCWorldActor::SetupPlayer()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
    if (!Pawn || !Survival)
        return;
    FWorldSurvivalProfile Profile;
    Profile.EntityId = FGuid::NewGuid();
    Profile.Location = Pawn->GetActorLocation();
    Profile.ClothingInsulation = 0.3f;
    if (!Survival->RegisterProfile(Profile))
        return;
    PlayerSurvivalId = Profile.EntityId;
    // Start by the cold fire looking towards the unfinished cabin.
    if (!FParse::Param(FCommandLine::Get(), TEXT("NoStartView")))
        Controller->SetControlRotation(FRotator(-6.0f, 148.0f, 0.0f));
    if (UShelterSensorComponent* Shelter = Pawn->FindComponentByClass<UShelterSensorComponent>())
        Shelter->SurvivalEntityId = PlayerSurvivalId;

    if (!bInputBound)
    {
        EnableInput(Controller);
        if (InputComponent)
        {
            // An axe swing; every other key belongs to the building tools.
            FInputKeyBinding& Binding = InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this,
                &AVerticalSliceCWorldActor::ChopInput);
            Binding.bConsumeInput = false;
            InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AVerticalSliceCWorldActor::HelperOrderInput);
            FInputKeyBinding& FillWaterBinding = InputComponent->BindKey(
                EKeys::I, IE_Pressed, this, &AVerticalSliceCWorldActor::FillCreekWaterInput);
            FillWaterBinding.bConsumeInput = false;
            FInputKeyBinding& DrinkWaterBinding = InputComponent->BindKey(
                EKeys::M, IE_Pressed, this, &AVerticalSliceCWorldActor::DrinkWaterInput);
            DrinkWaterBinding.bConsumeInput = false;
            FInputKeyBinding& BoilWaterBinding = InputComponent->BindKey(
                EKeys::F5, IE_Pressed, this, &AVerticalSliceCWorldActor::BoilWaterInput);
            BoilWaterBinding.bConsumeInput = false;
            FInputKeyBinding& RestBinding = InputComponent->BindKey(
                EKeys::F6, IE_Pressed, this, &AVerticalSliceCWorldActor::RestInput);
            RestBinding.bConsumeInput = false;
            FInputKeyBinding& SurvivalStatusBinding = InputComponent->BindKey(
                EKeys::F7, IE_Pressed, this, &AVerticalSliceCWorldActor::ShowSurvivalStatusInput);
            SurvivalStatusBinding.bConsumeInput = false;
            FInputKeyBinding& RefuelBinding = InputComponent->BindKey(
                EKeys::O, IE_Pressed, this, &AVerticalSliceCWorldActor::RefuelFireInput);
            RefuelBinding.bConsumeInput = false;
            FInputKeyBinding& StationStartBinding = InputComponent->BindKey(
                EKeys::F2, IE_Pressed, this, &AVerticalSliceCWorldActor::StationStartCollectInput);
            StationStartBinding.bConsumeInput = false;
            FInputKeyBinding& StationDepositBinding = InputComponent->BindKey(
                EKeys::F3, IE_Pressed, this, &AVerticalSliceCWorldActor::StationDepositInput);
            StationDepositBinding.bConsumeInput = false;
            FInputKeyBinding& StationTakeBinding = InputComponent->BindKey(
                EKeys::F4, IE_Pressed, this, &AVerticalSliceCWorldActor::StationTakeInput);
            StationTakeBinding.bConsumeInput = false;
            FInputKeyBinding& GatherBinding = InputComponent->BindKey(
                EKeys::G, IE_Pressed, this, &AVerticalSliceCWorldActor::GatherInput);
            GatherBinding.bConsumeInput = false;

            FInputKeyBinding& CycleFieldBinding = InputComponent->BindKey(
                EKeys::F9, IE_Pressed, this, &AVerticalSliceCWorldActor::CycleFieldActionInput);
            CycleFieldBinding.bConsumeInput = false;
            FInputKeyBinding& ApplyFieldBinding = InputComponent->BindKey(
                EKeys::F10, IE_Pressed, this, &AVerticalSliceCWorldActor::ApplyFieldActionInput);
            ApplyFieldBinding.bConsumeInput = false;
            bInputBound = true;
        }
    }
}

void AVerticalSliceCWorldActor::RefuelFireInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!World || !Pawn)
        return;

    UInventoryCraftingComponent* Inventory = Pawn->FindComponentByClass<UInventoryCraftingComponent>();
    UWorldItemOwnershipSubsystem* Ownership = World->GetSubsystem<UWorldItemOwnershipSubsystem>();
    if (!Inventory || !Ownership)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red,
                TEXT("Cannot refuel: inventory or item ownership system unavailable."));
        return;
    }

    UHeatSourceComponent* NearestHeat = nullptr;
    float NearestDistanceSquared = FMath::Square(400.0f);
    TArray<AActor*> NearbyActors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
            continue;
        TArray<UHeatSourceComponent*> Sources;
        Actor->GetComponents<UHeatSourceComponent>(Sources);
        for (UHeatSourceComponent* Source : Sources)
        {
            if (!IsValid(Source) || Source->FuelDefinitions.IsEmpty())
                continue;
            const float DistanceSquared = FVector::DistSquared(
                Pawn->GetActorLocation(), Source->GetComponentLocation());
            if (DistanceSquared <= NearestDistanceSquared)
            {
                NearestDistanceSquared = DistanceSquared;
                NearestHeat = Source;
            }
        }
    }

    if (!NearestHeat)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver,
                TEXT("Move close to a campfire or hearth to refuel it. (O)"));
        return;
    }

    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    if (!InventoryOwnerId.IsValid())
        return;

    for (const FInventoryStack& Stack : Inventory->Items)
    {
        if (!Stack.OwnedWorldItemId.IsValid() || Stack.Quantity != 1 || Stack.MaxStack != 1 ||
            !NearestHeat->FuelDefinitions.ContainsByPredicate(
                [&Stack](const FHeatFuelDefinition& Definition)
                { return Definition.ItemDefinitionId == Stack.ItemId; }))
            continue;

        FWorldOwnedItemRecord FuelItem;
        if (!Ownership->TryGetItem(Stack.OwnedWorldItemId, FuelItem) ||
            FuelItem.OwnerKind != EWorldItemOwnerKind::Inventory ||
            FuelItem.OwnerId != InventoryOwnerId ||
            FuelItem.DefinitionId != Stack.ItemId)
            continue;

        const bool bAdded = NearestHeat->TryAddFuelItem(
            FGuid::NewGuid(), FuelItem.ItemId, FuelItem.Revision, InventoryOwnerId);
        if (!bAdded)
            continue;

        const bool bWasLit = NearestHeat->IsLit();
        const bool bIgnited = bWasLit || NearestHeat->Ignite();
        if (GEngine)
        {
            const FColor MessageColor = bIgnited ? FColor(255, 190, 90) : FColor::Orange;
            const FString Message = bIgnited
                ? FString::Printf(TEXT("Added %s to the fire."), *Stack.ItemId.ToString())
                : FString::Printf(TEXT("Added %s, but the fire could not be lit."), *Stack.ItemId.ToString());
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, MessageColor, Message);
        }
        return;
    }

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
            TEXT("Carry an owned firewood or charcoal item to refuel this fire."));
}

AProductionStationActor* AVerticalSliceCWorldActor::FindNearestStation(
    const FVector& Location, float MaxDistanceCm) const
{
    UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !GameData || !GameData->IsApplied() ||
        !FMath::IsFinite(MaxDistanceCm) || MaxDistanceCm <= 0.0f)
        return nullptr;

    AProductionStationActor* Nearest = nullptr;
    float BestDistanceSquared = FMath::Square(MaxDistanceCm);
    for (TActorIterator<AProductionStationActor> It(World); It; ++It)
    {
        AProductionStationActor* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->StationDataId.IsNone() ||
            !GameData->GetStations().Contains(Candidate->StationDataId) ||
            !Candidate->EnsureRegistered())
            continue;
        const float DistanceSquared = FVector::DistSquared(Location, Candidate->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            Nearest = Candidate;
        }
    }
    return Nearest;
}

void AVerticalSliceCWorldActor::StationStartCollectInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !Pawn || !GameData || !GameData->IsApplied())
        return;

    AProductionStationActor* Station = FindNearestStation(Pawn->GetActorLocation(), 300.0f);
    if (!Station)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver,
                TEXT("No registered station within 300 cm. (F2 start/collect, F3 deposit, F4 take)"));
        return;
    }

    const int32 CollectedJobs = Station->CollectFinishedJobs();
    if (CollectedJobs > 0)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor(120, 220, 150),
                FString::Printf(TEXT("Collected %d finished job(s) into station stock. Press F4 to take an item."),
                    CollectedJobs));
        return;
    }

    UWorldProductionSubsystem* Production = World->GetSubsystem<UWorldProductionSubsystem>();
    FWorldProductionStation State;
    if (!Production || !Production->GetStation(Station->GetStationId(), State))
        return;

    bool bInputsReadyButFireMissing = false;
    for (const FGameDataTransformation& Transformation : GameData->GetTransformations())
    {
        if (Transformation.Where != Station->StationDataId || Transformation.Inputs.IsEmpty())
            continue;

        bool bReady = true;
        for (const FGameDataItemAmount& Input : Transformation.Inputs)
            if (State.Stock.FindRef(Input.ItemId) < Input.Quantity)
                bReady = false;
        for (const FName ToolId : Transformation.Tools)
            if (State.Stock.FindRef(ToolId) < 1)
                bReady = false;
        if (!bReady)
            continue;

        bInputsReadyButFireMissing = !Station->HasRequiredFire();
        FGuid JobId;
        if (Station->TryStartRecipe(Transformation.Id, JobId))
        {
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor(140, 210, 255),
                    FString::Printf(TEXT("Started %s at %s."),
                        *Transformation.Name, *Station->StationDataId.ToString()));
            return;
        }
    }

    if (bInputsReadyButFireMissing)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                TEXT("Station inputs are ready, but its required nearby fire/charcoal-hot fire is missing."));
        return;
    }

    const bool bWorking = State.Jobs.ContainsByPredicate([](const FWorldProductionJob& Job)
    {
        return Job.State == EProductionJobState::Queued || Job.State == EProductionJobState::Running;
    });
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver,
            bWorking
                ? TEXT("Station is already working. Press F2 again when a job finishes.")
                : TEXT("No recipe is ready. Press F3 to deposit required items/tools; F4 takes station stock."));
}

void AVerticalSliceCWorldActor::StationDepositInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldProductionSubsystem* Production =
        World ? World->GetSubsystem<UWorldProductionSubsystem>() : nullptr;
    if (!World || !Pawn || !Inventory || !GameData || !GameData->IsApplied() || !Production)
        return;

    AProductionStationActor* Station = FindNearestStation(Pawn->GetActorLocation(), 300.0f);
    if (!Station)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver,
                TEXT("Move within 300 cm of a registered station before depositing."));
        return;
    }

    FWorldProductionStation State;
    if (!Production->GetStation(Station->GetStationId(), State))
        return;

    // Build one requirement map for every recipe registered at this station.
    // F3 moves all currently available generic deficits in one press, rather than
    // forcing one key press per ingredient. Unique ledger-owned items stay excluded.
    TMap<FName, int32> DesiredStock;
    for (const FGameDataTransformation& Transformation : GameData->GetTransformations())
    {
        if (Transformation.Where != Station->StationDataId || Transformation.Inputs.IsEmpty())
            continue;
        for (const FGameDataItemAmount& Input : Transformation.Inputs)
        {
            if (!Input.ItemId.IsNone() && Input.Quantity > 0)
                DesiredStock.FindOrAdd(Input.ItemId) =
                    FMath::Max(DesiredStock.FindRef(Input.ItemId), Input.Quantity);
        }
        for (const FName ToolId : Transformation.Tools)
        {
            if (!ToolId.IsNone())
                DesiredStock.FindOrAdd(ToolId) = FMath::Max(DesiredStock.FindRef(ToolId), 1);
        }
    }

    TArray<FName> ItemIds;
    DesiredStock.GetKeys(ItemIds);
    ItemIds.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

    int32 DepositedKinds = 0;
    int32 DepositedUnits = 0;
    for (const FName ItemId : ItemIds)
    {
        if (!Production->GetStation(Station->GetStationId(), State))
            break;
        const int32 Missing = DesiredStock.FindRef(ItemId) - State.Stock.FindRef(ItemId);
        if (Missing <= 0)
            continue;

        int32 PlainAvailable = 0;
        bool bContainsUniqueIdentity = false;
        for (const FInventoryStack& Stack : Inventory->Items)
        {
            if (Stack.ItemId != ItemId)
                continue;
            if (Stack.OwnedWorldItemId.IsValid())
                bContainsUniqueIdentity = true;
            else
                PlainAvailable += Stack.Quantity;
        }
        if (bContainsUniqueIdentity || PlainAvailable <= 0)
            continue;

        const int32 ToDeposit = FMath::Min(Missing, PlainAvailable);
        if (Station->TryDeposit(Inventory, ItemId, ToDeposit))
        {
            ++DepositedKinds;
            DepositedUnits += ToDeposit;
        }
    }

    if (GEngine)
    {
        const FString Message = DepositedUnits > 0
            ? FString::Printf(TEXT("Deposited %d items across %d types at %s."),
                DepositedUnits, DepositedKinds, *Station->StationDataId.ToString())
            : TEXT("No missing recipe inputs/tools could be deposited; check generic inventory stacks and station capacity.");
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, DepositedUnits > 0 ? FColor(150, 205, 255) : FColor::Orange, Message);
    }
}

void AVerticalSliceCWorldActor::StationTakeInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    if (!World || !Pawn || !Inventory)
        return;

    AProductionStationActor* Station = FindNearestStation(Pawn->GetActorLocation(), 300.0f);
    if (!Station)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver,
                TEXT("Move within 300 cm of a registered station before taking stock."));
        return;
    }

    UWorldProductionSubsystem* Production = World->GetSubsystem<UWorldProductionSubsystem>();
    FWorldProductionStation State;
    if (!Production || !Production->GetStation(Station->GetStationId(), State))
        return;

    TArray<FName> StockIds;
    State.Stock.GetKeys(StockIds);
    StockIds.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    int32 WithdrawnKinds = 0;
    int32 WithdrawnUnits = 0;
    for (const FName ItemId : StockIds)
    {
        if (!Production->GetStation(Station->GetStationId(), State))
            break;
        const int32 Available = State.Stock.FindRef(ItemId);
        if (Available <= 0)
            continue;

        // Withdraw the largest whole stack that fits; proceed to other item types
        // if this one cannot fit, preserving station stock on every failed attempt.
        for (int32 Quantity = Available; Quantity >= 1; --Quantity)
        {
            if (Station->TryTake(Inventory, ItemId, Quantity))
            {
                ++WithdrawnKinds;
                WithdrawnUnits += Quantity;
                break;
            }
        }
    }

    if (GEngine)
    {
        const FString Message = WithdrawnUnits > 0
            ? FString::Printf(TEXT("Withdrew %d items across %d types from %s."),
                WithdrawnUnits, WithdrawnKinds, *Station->StationDataId.ToString())
            : TEXT("No station stock could fit in inventory.");
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, WithdrawnUnits > 0 ? FColor(140, 220, 150) : FColor::Orange, Message);
    }
}

void AVerticalSliceCWorldActor::FillCreekWaterInput()
{
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn || !PlayerSurvivalId.IsValid())
        return;

    const FVector PlayerLocation = Pawn->GetActorLocation();
    if (!IsInCreek(PlayerLocation))
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver,
                TEXT("Move into the creek to collect water. (I: fill fired pot)"));
        return;
    }

    UInventoryCraftingComponent* Inventory = Pawn->FindComponentByClass<UInventoryCraftingComponent>();
    UWorldItemOwnershipSubsystem* Ownership =
        GetWorld()->GetSubsystem<UWorldItemOwnershipSubsystem>();
    UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
    if (!Inventory || !Ownership || !Survival)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red,
                TEXT("Water collection is unavailable: inventory or survival system missing."));
        return;
    }

    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    FWorldSurvivalProfile Profile;
    if (!InventoryOwnerId.IsValid() || !Survival->GetProfile(PlayerSurvivalId, Profile))
        return;

    for (const FInventoryStack& Stack : Inventory->Items)
    {
        if (Stack.ItemId != FName(TEXT("Pot.Fired")) ||
            !Stack.OwnedWorldItemId.IsValid() || Stack.Quantity != 1 || Stack.MaxStack != 1)
            continue;

        FWorldOwnedItemRecord Container;
        if (!Ownership->TryGetItem(Stack.OwnedWorldItemId, Container) ||
            Container.OwnerKind != EWorldItemOwnerKind::Inventory ||
            Container.OwnerId != InventoryOwnerId ||
            Container.DefinitionId != FName(TEXT("Pot.Fired")))
            continue;

        FGuid WaterItemId;
        if (Survival->TryFillWaterFromCreek(
            FGuid::NewGuid(), PlayerSurvivalId, Profile.Revision,
            Container.ItemId, Container.Revision, InventoryOwnerId, PlayerLocation, WaterItemId))
        {
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor(150, 205, 255),
                    TEXT("Filled the fired pot with dirty creek water. Boil it before drinking."));
        }
        else if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                TEXT("Could not fill the pot. Check inventory space and try again."));
        }
        return;
    }

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange,
            TEXT("You need a fired clay pot (Pot.Fired) to collect creek water."));
}

void AVerticalSliceCWorldActor::DrinkWaterInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!World || !Pawn || !Inventory || !Ownership || !Survival || !PlayerSurvivalId.IsValid())
        return;

    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    FWorldSurvivalProfile Profile;
    if (!InventoryOwnerId.IsValid() || !Survival->GetProfile(PlayerSurvivalId, Profile))
        return;

    // Drink safe water first. Dirty creek water still relieves thirst but the
    // shared consumable definition applies its configured illness condition.
    const FName WaterDefinitions[] = {
        FName(TEXT("Water.Boiled")), FName(TEXT("Water.Dirty"))
    };
    for (const FName WaterDefinitionId : WaterDefinitions)
    {
        for (const FInventoryStack& Stack : Inventory->Items)
        {
            if (Stack.ItemId != WaterDefinitionId || !Stack.OwnedWorldItemId.IsValid() ||
                Stack.Quantity != 1 || Stack.MaxStack != 1)
                continue;

            FWorldOwnedItemRecord Item;
            if (!Ownership->TryGetItem(Stack.OwnedWorldItemId, Item) ||
                Item.OwnerKind != EWorldItemOwnerKind::Inventory ||
                Item.OwnerId != InventoryOwnerId || Item.DefinitionId != WaterDefinitionId ||
                Item.Quantity != 1)
                continue;

            const bool bConsumed = Survival->TryConsumeFoodItem(
                FGuid::NewGuid(), PlayerSurvivalId, Profile.Revision,
                Item.ItemId, Item.Revision, InventoryOwnerId);
            if (GEngine)
            {
                const FColor Color = bConsumed
                    ? (WaterDefinitionId == FName(TEXT("Water.Boiled")) ? FColor::Green : FColor::Orange)
                    : FColor::Red;
                const TCHAR* Message = bConsumed
                    ? (WaterDefinitionId == FName(TEXT("Water.Boiled"))
                        ? TEXT("Drank boiled water. Thirst relieved.")
                        : TEXT("Drank dirty creek water. Thirst eased, illness risk increased."))
                    : TEXT("Could not drink water: its ownership or survival state changed.");
                GEngine->AddOnScreenDebugMessage(9202, 3.5f, Color, Message);
            }
            return;
        }
    }

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(9202, 3.0f, FColor::Silver,
            TEXT("No owned water. Fill a fired pot at the creek (I), boil it by a fire (F5), then drink (M)."));
}

void AVerticalSliceCWorldActor::BoilWaterInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    UWorldItemOwnershipSubsystem* Ownership =
        World ? World->GetSubsystem<UWorldItemOwnershipSubsystem>() : nullptr;
    UWorldSurvivalSubsystem* Survival =
        World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!World || !Pawn || !Inventory || !Ownership || !Survival ||
        !Clock || !Clock->IsClockInitialized() || !GameData || !GameData->IsApplied() ||
        !PlayerSurvivalId.IsValid())
        return;

    const FGuid InventoryOwnerId = Inventory->GetInventoryOwnerId();
    FWorldSurvivalProfile Profile;
    if (!InventoryOwnerId.IsValid() || !Survival->GetProfile(PlayerSurvivalId, Profile))
        return;

    FWorldOwnedItemRecord Pot;
    bool bHasPot = false;
    for (const FInventoryStack& Stack : Inventory->Items)
    {
        if (Stack.ItemId != FName(TEXT("Pot.Fired")) || !Stack.OwnedWorldItemId.IsValid() ||
            Stack.Quantity != 1 || Stack.MaxStack != 1)
            continue;
        if (Ownership->TryGetItem(Stack.OwnedWorldItemId, Pot) &&
            Pot.OwnerKind == EWorldItemOwnerKind::Inventory &&
            Pot.OwnerId == InventoryOwnerId && Pot.DefinitionId == FName(TEXT("Pot.Fired")) &&
            Pot.Quantity == 1)
        {
            bHasPot = true;
            break;
        }
    }
    if (!bHasPot)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(9203, 3.0f, FColor::Orange,
                TEXT("Boiling water requires your ledger-owned fired clay pot (Pot.Fired)."));
        return;
    }

    const FName WaterDefinitions[] = {
        FName(TEXT("Water.Boiling")), FName(TEXT("Water.Dirty"))
    };
    for (const FName WaterDefinitionId : WaterDefinitions)
    {
        for (const FInventoryStack& Stack : Inventory->Items)
        {
            if (Stack.ItemId != WaterDefinitionId || !Stack.OwnedWorldItemId.IsValid() ||
                Stack.Quantity != 1 || Stack.MaxStack != 1)
                continue;

            FWorldOwnedItemRecord Water;
            if (!Ownership->TryGetItem(Stack.OwnedWorldItemId, Water) ||
                Water.OwnerKind != EWorldItemOwnerKind::Inventory ||
                Water.OwnerId != InventoryOwnerId ||
                Water.DefinitionId != WaterDefinitionId || Water.Quantity != 1)
                continue;

            // Do not ask the transaction to finish a job that cannot have
            // completed yet. Its receipt remains the authority for start time.
            if (WaterDefinitionId == FName(TEXT("Water.Boiling")))
            {
                const FWorldSurvivalActionReceipt* Start = Profile.ActionReceipts.FindByPredicate(
                    [&Water, &Pot, &InventoryOwnerId](const FWorldSurvivalActionReceipt& Receipt)
                    {
                        return Receipt.Action == EWorldSurvivalActionKind::BoilWater &&
                            Receipt.ItemId == Water.ItemId && Receipt.RelatedItemId == Pot.ItemId &&
                            Receipt.ExpectedItemOwnerId == InventoryOwnerId &&
                            Receipt.ItemDefinitionId == FName(TEXT("Water.Dirty")) &&
                            Receipt.ResultingItemDefinitionId == FName(TEXT("Water.Boiling"));
                    });
                if (!Start)
                {
                    if (GEngine)
                        GEngine->AddOnScreenDebugMessage(9203, 3.0f, FColor::Red,
                            TEXT("This water has no valid boiling-start receipt. Refill the pot from the creek."));
                    return;
                }
                const FGameDataTransformation* Recipe =
                    GameData->FindTransformation(FName(TEXT("boil_water")));
                const double Duration = Recipe ? Recipe->DurationSeconds : Start->RequestedDurationSeconds;
                const double Remaining = Start->RequestedStartTimeSeconds + Duration -
                    Clock->GetSimulationTimeSeconds();
                if (Remaining > 0.001)
                {
                    if (GEngine)
                        GEngine->AddOnScreenDebugMessage(9203, 3.0f, FColor(255, 210, 100),
                            FString::Printf(TEXT("Water is boiling. %.1f game minutes remain."),
                                Remaining / 60.0));
                    return;
                }
            }

            bool bStarted = false;
            const bool bSucceeded = Survival->TryBoilWaterAtFire(
                FGuid::NewGuid(), PlayerSurvivalId, Profile.Revision,
                Water.ItemId, Water.Revision, Pot.ItemId, Pot.Revision,
                InventoryOwnerId, bStarted);
            if (GEngine)
            {
                const TCHAR* Message = bSucceeded
                    ? (bStarted
                        ? TEXT("Boiling started. Keep the pot near a lit fire and wait before pressing F5 again.")
                        : TEXT("Water is boiled. Press M to drink it."))
                    : TEXT("Boiling needs dirty water or a waiting boiling item, a fired pot and a nearby lit fire.");
                GEngine->AddOnScreenDebugMessage(9203, 4.0f,
                    bSucceeded ? FColor::Green : FColor::Orange, Message);
            }
            return;
        }
    }

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(9203, 3.0f, FColor::Silver,
            TEXT("No owned dirty or boiling water. Fill your fired pot at the creek first (I)."));
}

void AVerticalSliceCWorldActor::RestInput()
{
    UWorld* World = GetWorld();
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    if (!World || !Survival || !Clock || !Clock->IsClockInitialized() || !PlayerSurvivalId.IsValid())
        return;

    FWorldSurvivalProfile Profile;
    if (!Survival->GetProfile(PlayerSurvivalId, Profile))
        return;

    const double Now = Clock->GetSimulationTimeSeconds();
    if (Profile.RestUntilTimeSeconds > Now)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(9204, 2.5f, FColor::Silver,
                TEXT("You are already resting. Movement is disabled until the rest interval ends."));
        return;
    }

    if (Survival->TryBeginRest(FGuid::NewGuid(), PlayerSurvivalId, Profile.Revision, 1800.0))
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(9204, 4.0f, FColor(150, 220, 255),
                TEXT("Rest started for 30 game minutes. Warm, dry shelter improves recovery."));
    }
    else if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(9204, 3.0f, FColor::Orange,
            TEXT("Rest could not start. Wait for the current action to finish and try again."));
    }
}

void AVerticalSliceCWorldActor::ShowSurvivalStatusInput()
{
    UWorld* World = GetWorld();
    UWorldSurvivalSubsystem* Survival = World ? World->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock =
        World ? World->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UPhysicalHealthComponent* PhysicalHealth = Pawn
        ? Pawn->FindComponentByClass<UPhysicalHealthComponent>() : nullptr;
    FWorldSurvivalProfile Profile;
    if (!Survival || !PlayerSurvivalId.IsValid() ||
        !Survival->GetProfile(PlayerSurvivalId, Profile))
        return;

    const float PhysicalHp = PhysicalHealth ? PhysicalHealth->Health : -1.0f;
    const float PhysicalMaxHp = PhysicalHealth ? PhysicalHealth->MaxHealth : -1.0f;

    FString Conditions = TEXT("none");
    if (!Profile.Conditions.IsEmpty())
    {
        Conditions.Empty();
        for (int32 Index = 0; Index < Profile.Conditions.Num(); ++Index)
        {
            if (Index > 0)
                Conditions += TEXT(", ");
            Conditions += Profile.Conditions[Index].DefinitionId.ToString();
        }
    }
    const bool bResting = Clock && Clock->IsClockInitialized() &&
        Profile.RestUntilTimeSeconds > Clock->GetSimulationTimeSeconds();
    FString DateStatus = TEXT("date unavailable");
    FWorldCalendarDate Date;
    if (Clock && Clock->GetCalendarDate(Date))
    {
        const TCHAR* SeasonName = TEXT("Winter");
        switch (Date.Season)
        {
        case EWorldSeason::Spring: SeasonName = TEXT("Spring"); break;
        case EWorldSeason::Summer: SeasonName = TEXT("Summer"); break;
        case EWorldSeason::Autumn: SeasonName = TEXT("Autumn"); break;
        default: SeasonName = TEXT("Winter"); break;
        }
        DateStatus = FString::Printf(TEXT("Y%d M%02d D%02d %s %02d:%02d"),
            Date.Year, Date.Month, Date.DayOfMonth, SeasonName,
            FMath::FloorToInt(Date.HourOfDay),
            FMath::FloorToInt((Date.HourOfDay - FMath::FloorToFloat(Date.HourOfDay)) * 60.0f));
    }
    UWorldEventDirectorSubsystem* Events = World ? World->GetSubsystem<UWorldEventDirectorSubsystem>() : nullptr;
    FString EventStatus = Events ? Events->GetEventStatusText() : FString();
    if (EventStatus.IsEmpty())
        EventStatus = TEXT("none");
    const FString Status = FString::Printf(
        TEXT("SURVIVAL | Body HP %.0f/%.0f | Survival HP %.0f | Thirst %.0f | Stamina %.0f | Fatigue %.0f | Wet %.0f%% | Core %.1f C | Resting %s | Conditions: %s | %s | Events: %s"),
        PhysicalHp, PhysicalMaxHp, Profile.Health, Profile.Thirst, Profile.Stamina, Profile.Fatigue,
        Profile.Wetness * 100.0f, Profile.CoreTemperatureCelsius,
        bResting ? TEXT("yes") : TEXT("no"), *Conditions, *DateStatus, *EventStatus);
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(9205, 6.0f, FColor::Cyan, Status);
}

void AVerticalSliceCWorldActor::GatherInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    if (!World || !Pawn || !Inventory)
        return;

    AGatherableResourceActor* Nearest = nullptr;
    float BestDistanceSquared = FMath::Square(250.0f);
    for (TActorIterator<AGatherableResourceActor> It(World); It; ++It)
    {
        AGatherableResourceActor* Candidate = *It;
        if (!IsValid(Candidate))
            continue;
        const float DistanceSquared = FVector::DistSquared(Pawn->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            Nearest = Candidate;
        }
    }
    if (!Nearest)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 1.8f, FColor::Silver,
                TEXT("No gatherable resource within 250 cm. (G: gather)"));
        return;
    }

    int32 Gathered = 0;
    if (Nearest->TryGather(Inventory, Gathered))
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor(140, 220, 150),
                FString::Printf(TEXT("Gathered %d x %s. Remaining: %d."),
                    Gathered, *Nearest->ItemDefinitionId.ToString(), Nearest->GetAvailableUnits()));
        return;
    }
    const FString Reason = Nearest->GetAvailableUnits() <= 0
        ? TEXT("This resource is depleted.")
        : (!Nearest->RequiredToolId.IsNone() && Inventory->GetItemCount(Nearest->RequiredToolId) <= 0
            ? FString::Printf(TEXT("Requires tool: %s."), *Nearest->RequiredToolId.ToString())
            : TEXT("Inventory is full or gathering failed."));
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, Reason);
}

void AVerticalSliceCWorldActor::CycleFieldActionInput()
{
    UWorld* World = GetWorld();
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    if (!GameData || !GameData->IsApplied() || GameData->GetFieldActions().IsEmpty())
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                TEXT("Farm action data is not ready."));
        return;
    }

    SelectedFieldActionIndex = (SelectedFieldActionIndex + 1) % GameData->GetFieldActions().Num();
    const FGameDataFieldAction& Action = GameData->GetFieldActions()[SelectedFieldActionIndex];
    TArray<FString> Requirements;
    for (const FGameDataItemAmount& Input : Action.Inputs)
        Requirements.Add(FString::Printf(TEXT("%d x %s"), Input.Quantity, *Input.ItemId.ToString()));
    for (const FName ToolId : Action.RequiredTools)
        Requirements.Add(FString::Printf(TEXT("tool: %s (kept)"), *ToolId.ToString()));
    const FString CostText = Requirements.IsEmpty()
        ? TEXT("no materials") : FString::Join(Requirements, TEXT(", "));
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor(160, 215, 255),
            FString::Printf(TEXT("FIELD ACTION: %s | %s | labor %.0f game seconds | F9 apply"),
                *Action.Id.ToString(), *CostText, Action.LaborSeconds));
}

void AVerticalSliceCWorldActor::ApplyFieldActionInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    const UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    UWorldAgricultureSubsystem* Agriculture =
        World ? World->GetSubsystem<UWorldAgricultureSubsystem>() : nullptr;
    if (!World || !Pawn || !Inventory || !GameData || !GameData->IsApplied() ||
        !Agriculture || !PlayerSurvivalId.IsValid() ||
        GameData->GetFieldActions().IsEmpty())
        return;

    if (!GameData->GetFieldActions().IsValidIndex(SelectedFieldActionIndex))
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver,
                TEXT("Press F9 to choose a field action, then F10 to apply it to a soil tile."));
        return;
    }
    const FGameDataFieldAction& Action = GameData->GetFieldActions()[SelectedFieldActionIndex];

    FIntVector FieldCell = FIntVector::ZeroValue;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    bool bFoundCell = false;
    for (int32 Y = 0; Y < FarmPlotCellsPerAxis; ++Y)
    {
        for (int32 X = 0; X < FarmPlotCellsPerAxis; ++X)
        {
            const float WorldX = FarmPlotCentre.X +
                (static_cast<float>(X) - 1.0f) * FarmPlotCellSize;
            const float WorldY = FarmPlotCentre.Y +
                (static_cast<float>(Y) - 1.0f) * FarmPlotCellSize;
            const float DistanceSquared = FVector::DistSquared2D(
                Pawn->GetActorLocation(), FVector(WorldX, WorldY, 0.0f));
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                FieldCell = FIntVector(
                    FMath::RoundToInt(WorldX / 100.0f),
                    FMath::RoundToInt(WorldY / 100.0f),
                    FMath::RoundToInt(GetGroundHeight(WorldX, WorldY) / 100.0f));
                bFoundCell = true;
            }
        }
    }

    if (!bFoundCell || BestDistanceSquared > FMath::Square(135.0f))
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver,
                TEXT("Farm work must be done on a visible soil tile. Move close to the 3x3 farm patch."));
        return;
    }

    FWorldFieldCellState ExistingCell;
    const int64 ExpectedRevision = Agriculture->GetFieldCell(FieldCell, ExistingCell)
        ? ExistingCell.Revision : 0;
    FWorldFieldCellState UpdatedCell;
    if (!Agriculture->TryApplyFieldActionWithInventory(
        FGuid::NewGuid(), Inventory, PlayerSurvivalId, FieldCell,
        Action.Action, ExpectedRevision, UpdatedCell))
    {
        if (GEngine)
        {
            TArray<FString> Requirements;
            for (const FGameDataItemAmount& Input : Action.Inputs)
            {
                const int32 Have = Inventory->GetItemCount(Input.ItemId);
                if (Have < Input.Quantity)
                    Requirements.Add(FString::Printf(TEXT("%s %d/%d"),
                        *Input.ItemId.ToString(), Have, Input.Quantity));
            }
            for (const FName ToolId : Action.RequiredTools)
                if (Inventory->GetItemCount(ToolId) <= 0)
                    Requirements.Add(FString::Printf(TEXT("tool %s missing"), *ToolId.ToString()));
            const FString Detail = Requirements.IsEmpty()
                ? TEXT("field stage, rest or stamina may be blocking the action")
                : FString::Join(Requirements, TEXT(", "));
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
                FString::Printf(TEXT("FIELD ACTION FAILED: %s | %s"),
                    *Action.Id.ToString(), *Detail));
        }
        return;
    }

    RefreshFarmPlotVisuals();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor(140, 220, 160),
            FString::Printf(TEXT("FIELD: %s cell (%d,%d) | revision %lld | stamina charged"),
                *Action.Id.ToString(), FieldCell.X, FieldCell.Y,
                static_cast<long long>(UpdatedCell.Revision)));
    }
}

void AVerticalSliceCWorldActor::ChopInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UInventoryCraftingComponent* Inventory =
        Pawn ? Pawn->FindComponentByClass<UInventoryCraftingComponent>() : nullptr;
    UWorldGameDataSubsystem* GameData =
        World ? World->GetSubsystem<UWorldGameDataSubsystem>() : nullptr;
    float AxeStrength = 0.0f;
    if (!World || !Controller || !Pawn || !Inventory || !GameData || !GameData->IsApplied() ||
        Inventory->GetItemCount(TEXT("Tool.Axe")) <= 0 ||
        !GameData->GetToolStrength(TEXT("Tool.Axe"), AxeStrength))
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                TEXT("A usable Tool.Axe is required for chopping and log processing."));
        return;
    }

    UWorldSurvivalSubsystem* Survival = World->GetSubsystem<UWorldSurvivalSubsystem>();

    FVector Location;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Location, Rotation);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VerticalSliceCChop), false, Controller->GetPawn());
    const bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
        Hit, Location, Location + Rotation.Vector() * 260.0f, ECC_Visibility, Params);

    // The axe repairs damaged placed pieces only when the matching repair material
    // is available. Graph damage and inventory are rolled back together on failure.
    ULogCarryComponent* Carry = Controller->GetPawn()
        ? Controller->GetPawn()->FindComponentByClass<ULogCarryComponent>() : nullptr;
    if (bHitSomething && Carry && !Carry->IsCarryingLog() && !Carry->IsHoldingMaterialPiece())
    {
        UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>();
        IStructuralPieceInterface* Piece = Cast<IStructuralPieceInterface>(Hit.GetActor());
        FStructuralNodeRecord Node;
        if (Piece && Piece->IsStructurallyPlaced() && Graph &&
            Graph->GetNode(Piece->GetStructuralNodeId(), Node) && Node.Damage > 0.0f)
        {
            FName RepairMaterial = NAME_None;
            switch (Node.Kind)
            {
            case EStructuralNodeKind::Wood: RepairMaterial = TEXT("Firewood"); break;
            case EStructuralNodeKind::Stone: RepairMaterial = TEXT("Stone"); break;
            case EStructuralNodeKind::Stick: RepairMaterial = TEXT("Stick"); break;
            default: break;
            }
            int32 GenericRepairUnits = 0;
            bool bHasUniqueRepairItem = false;
            for (const FInventoryStack& Stack : Inventory->Items)
            {
                if (Stack.ItemId != RepairMaterial)
                    continue;
                if (Stack.OwnedWorldItemId.IsValid())
                    bHasUniqueRepairItem = true;
                else
                    GenericRepairUnits += FMath::Max(0, Stack.Quantity);
            }
            if (RepairMaterial.IsNone() || GenericRepairUnits <= 0 || bHasUniqueRepairItem)
            {
                if (GEngine)
                    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                        RepairMaterial.IsNone()
                            ? TEXT("This structural material has no repair rule yet.")
                            : FString::Printf(TEXT("Repair requires one generic %s (not a unique physical piece)."), *RepairMaterial.ToString()));
                return;
            }

            if (Survival && PlayerSurvivalId.IsValid() &&
                !Survival->CanRecordActivity(PlayerSurvivalId, ESurvivalActivity::Build, 0.75f, 0.0f))
            {
                if (GEngine)
                    GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange,
                        TEXT("Too exhausted or resting. Wait for rest to end or recover stamina."));
                return;
            }

            const float RepairEffectiveness = Survival && PlayerSurvivalId.IsValid()
                ? FMath::Clamp(Survival->GetWorkEffectiveness(PlayerSurvivalId), 0.1f, 1.5f) : 1.0f;
            const float RepairAmount = 25.0f * RepairEffectiveness;
            TArray<FWorldStructuralNodeSnapshot> GraphBefore;
            FGuid OwnerActorId;
            FGuid InventoryOwnerBefore;
            TArray<FInventoryStack> InventoryBefore;
            FName QuickAccessBefore = NAME_None;
            if (!Graph->ExportSnapshot(GraphBefore))
            {
                Carry->SetPlacementStatus(TEXT("Repair failed: structural state could not be snapshotted."));
                return;
            }
            Inventory->ExportSnapshot(OwnerActorId, InventoryOwnerBefore, InventoryBefore, QuickAccessBefore);
            if (!InventoryOwnerBefore.IsValid() || InventoryOwnerBefore != Inventory->GetInventoryOwnerId())
                return;

            auto RollbackRepair = [&]() -> bool
            {
                const bool bInventoryRestored =
                    Inventory->RestoreSnapshot(InventoryOwnerBefore, InventoryBefore, QuickAccessBefore);
                const bool bGraphRestored = Graph->RestoreSnapshot(GraphBefore);
                if (!bInventoryRestored || !bGraphRestored)
                {
                    if (UWorldSimulationCoordinatorSubsystem* Coordinator =
                        World->GetSubsystem<UWorldSimulationCoordinatorSubsystem>())
                        Coordinator->MarkSimulationFaulted(
                            TEXT("Structural repair rollback failed; stop simulation and recover/reset the world."));
                    return false;
                }
                return true;
            };

            if (!Inventory->TryRemoveItem(RepairMaterial, 1))
            {
                Carry->SetPlacementStatus(TEXT("Repair failed: repair material changed."));
                return;
            }
            if (!Graph->RepairDamage(Node.NodeId, RepairAmount))
            {
                RollbackRepair();
                Carry->SetPlacementStatus(TEXT("Repair failed: structural damage changed."));
                return;
            }
            if (Survival && PlayerSurvivalId.IsValid() &&
                !Survival->RecordActivity(PlayerSurvivalId, ESurvivalActivity::Build, 0.75f, 0.0f))
            {
                RollbackRepair();
                Carry->SetPlacementStatus(TEXT("Repair rolled back because the survival action could not be committed."));
                return;
            }

            if (ALogSnapCharacter* Character = Cast<ALogSnapCharacter>(Pawn))
                Character->PlayAxeSwing();
            FStructuralNodeRecord UpdatedNode;
            const float RemainingDamage = Graph->GetNode(Node.NodeId, UpdatedNode)
                ? UpdatedNode.Damage : Node.Damage;
            Carry->SetPlacementStatus(FString::Printf(
                TEXT("Repaired with 1 %s. Remaining damage: %.0f."),
                *RepairMaterial.ToString(), RemainingDamage));
            return;
        }

        if (ALogBuildPiece* Log = Cast<ALogBuildPiece>(Hit.GetActor()))
        {
            Carry->ProcessAimedPiece();
            return;
        }
    }

    ++ChopSwingCount;
    if (ALogSnapCharacter* Character = Cast<ALogSnapCharacter>(Controller->GetPawn()))
        Character->PlayAxeSwing();

    if (!bHitSomething)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::Silver,
                FString::Printf(TEXT("AXE SWING #%d | MISS - no target in range"), ChopSwingCount));
        }
        return;
    }

    AHarvestableTreeActor* Tree = Cast<AHarvestableTreeActor>(Hit.GetActor());
    if (!Tree)
    {
        DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 12.0f, 8, FColor::Silver,
            false, 0.8f, 0, 1.8f);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::Silver,
                FString::Printf(TEXT("AXE SWING #%d | Hit %s - not a chopable tree"),
                    ChopSwingCount, Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("world")));
        }
        return;
    }

    if (Survival && PlayerSurvivalId.IsValid())
    {
        if (!Survival->RecordActivity(PlayerSurvivalId, ESurvivalActivity::Chop, 0.75f, 0.0f))
        {
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange,
                    TEXT("Too exhausted or resting. Wait for rest to end or recover stamina."));
            return;
        }
        AxeStrength *= Survival->GetWorkEffectiveness(PlayerSurvivalId);
    }

    const bool bAccepted = Tree->ApplyChopDamage(AxeStrength, Rotation.Vector());
    if (bAccepted)
        ++SuccessfulChopCount;

    const bool bFelled = Tree->bFellingStarted;
    const FColor FeedbackColor = !bAccepted ? FColor::Red : (bFelled ? FColor::Green : FColor::Orange);
    const FString Feedback = FString::Printf(
        TEXT("AXE SWING #%d | TREE HIT #%d | %s | Damage: %.1f | Tree HP: %.1f / %.1f%s"),
        ChopSwingCount, SuccessfulChopCount, *Tree->GetName(), bAccepted ? AxeStrength : 0.0f,
        Tree->ChopHealth, Tree->MaxChopHealth, !bAccepted ? TEXT(" | BLOCKED") : (bFelled ? TEXT(" | FELLED") : TEXT("")));

    DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 15.0f, 10, FeedbackColor,
        false, 1.1f, 0, 2.5f);
    DrawDebugString(GetWorld(), Hit.ImpactPoint + FVector(0.0f, 0.0f, 30.0f),
        FString::Printf(TEXT("HIT #%d  HP %.1f"), SuccessfulChopCount, Tree->ChopHealth),
        nullptr, FeedbackColor, 1.1f, true, 1.1f);

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FeedbackColor, Feedback);
}

void AVerticalSliceCWorldActor::UpdateSun()
{
    ADirectionalLight* Light = Sun.Get();
    if (!Light || !bDayNightCycle)
        return;
    // Sun height follows the hour; below the horizon the night is dark and cold.
    const float Elevation = 62.0f * FMath::Sin(2.0f * PI * (Hours - 6.0f) / 24.0f);
    Light->SetActorRotation(FRotator(-Elevation, 15.0f * Hours - 90.0f, 0.0f));
}

void AVerticalSliceCWorldActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!PlayerSurvivalId.IsValid())
        SetupPlayer();
    EnsureStarterTools();
    AssignGatherableZones();
    EnsureDiscoveryMarkers();

    FarmPlotVisualRefreshTimer -= DeltaSeconds;
    if (FarmPlotVisualRefreshTimer <= 0.0f)
    {
        FarmPlotVisualRefreshTimer = 1.0f;
        RefreshFarmPlotVisuals();
    }

    UpdateCreek(DeltaSeconds);
    if (Helper && Helper->GetStatus() != LastHelperStatus && GEngine)
    {
        LastHelperStatus = Helper->GetStatus();
        GEngine->AddOnScreenDebugMessage(4711, 4.0f, FColor(160, 210, 255),
            FString::Printf(TEXT("Helper: %s"), *LastHelperStatus));
    }

    Hours = FMath::Fmod(Hours + DeltaSeconds * static_cast<float>(SimulationTimeScale) / 3600.0f, 24.0f);
    UpdateSun();

    TimeToSurvivalUpdate -= DeltaSeconds;
    if (TimeToSurvivalUpdate <= 0.0f && PlayerSurvivalId.IsValid())
    {
        TimeToSurvivalUpdate = 1.0f;
        UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
        const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
        if (Survival && Controller && Controller->GetPawn())
        {
            const FVector PlayerLocation = Controller->GetPawn()->GetActorLocation();
            Survival->UpdateLocation(PlayerSurvivalId, PlayerLocation);
            if (IsInCreek(PlayerLocation))
                Survival->ApplyWetnessExposure(PlayerSurvivalId, 0.12f);

            // When the shared climate is running, it is authoritative. Do not
            // overwrite rain/storm with this scene's old wind/snow visual cycle,
            // otherwise P2 wetness can never observe the authored weather.
            const UWorldClimateSubsystem* Climate =
                GetWorld()->GetSubsystem<UWorldClimateSubsystem>();
            if (Climate && Climate->IsClimateInitialized())
            {
                Survival->SetWeather(Climate->GetWeather(), Climate->GetWeatherSeverity());
            }
            else
            {
                const bool bNight = Hours < 6.0f || Hours > 19.5f;
                Survival->SetWeather(
                    bNight ? EWorldWeatherState::Snow : EWorldWeatherState::Wind,
                    bNight ? 0.5f : 0.35f);
            }

            // Give the player a discrete, low-noise signal when exposure changes
            // between dry/damp/wet/soaked bands. The profile remains authoritative;
            // this is feedback only and never changes the simulation state.
            FWorldSurvivalProfile WetnessProfile;
            if (Survival->GetProfile(PlayerSurvivalId, WetnessProfile))
            {
                const int32 WetnessBand = WetnessProfile.Wetness >= 0.75f ? 3
                    : WetnessProfile.Wetness >= 0.45f ? 2
                    : WetnessProfile.Wetness >= 0.15f ? 1 : 0;
                if (LastReportedWetnessBand == INDEX_NONE)
                {
                    LastReportedWetnessBand = WetnessBand;
                }
                else if (WetnessBand != LastReportedWetnessBand)
                {
                    if (GEngine)
                    {
                        const bool bGettingWetter = WetnessBand > LastReportedWetnessBand;
                        const TCHAR* Message = bGettingWetter
                            ? (WetnessBand >= 3
                                ? TEXT("You are soaked. Find shelter or a heat source to dry off.")
                                : WetnessBand == 2
                                    ? TEXT("You are getting wet. Shelter and warmth will help.")
                                    : TEXT("Your clothes are damp. Avoid prolonged cold exposure."))
                            : (WetnessBand == 0
                                ? TEXT("You are dry again.")
                                : WetnessBand == 1
                                    ? TEXT("You are drying, but still damp.")
                                    : TEXT("You are less wet; stay warm to keep drying."));
                        GEngine->AddOnScreenDebugMessage(
                            9206, 3.5f, bGettingWetter ? FColor::Orange : FColor(130, 220, 190), Message);
                    }
                    LastReportedWetnessBand = WetnessBand;
                }
            }
        }
    }

    if (!bIntroShown && GEngine && PlayerSurvivalId.IsValid() && !FParse::Param(FCommandLine::Get(), TEXT("NoStartView")))
    {
        bIntroShown = true;
        GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor(230, 210, 160),
            TEXT("The camp is abandoned. The cabin has no door and no roof, the bridge is gone, and the nights are cold. Controls: LMB chop, I fill pot, F5 boil, M drink, F6 rest, F7 status, O refuel fire, J helper."));
    }
}
