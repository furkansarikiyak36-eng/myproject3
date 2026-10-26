#include "VerticalSliceBWorldActor.h"

#include "CarryLoadComponent.h"
#include "HarvestableTreeActor.h"
#include "HeatSourceComponent.h"
#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "MaterialConnectionSubsystem.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "StructuralPieceInterface.h"
#include "VoxelTerrainChunkComponent.h"
#include "WorldSurvivalSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    constexpr int32 DefaultQuadsPerAxis = 96;
    const TCHAR* DemoLogAssetRoot = TEXT("/Game/DemoAssets/Logs/");

    struct FZonePoint
    {
        const TCHAR* Label;
        FVector2D Centre;
        bool bActive;
    };

    const FZonePoint Zones[] =
    {
        { TEXT("01  CAMP  |  SURVIVAL"), FVector2D(0.0, 0.0), true },
        { TEXT("02  TIMBER  |  V: CHOP"), FVector2D(3500.0, 1500.0), true },
        { TEXT("03  WOOD YARD  |  C / F"), FVector2D(2500.0, -2000.0), true },
        { TEXT("04  STONE + TOOLS  |  T / Y"), FVector2D(-2000.0, -2000.0), true },
        { TEXT("05  BUILD YARD  |  SUPPORT"), FVector2D(-2000.0, 1500.0), true },
        { TEXT("06  FARM  |  FUTURE SYSTEM"), FVector2D(4500.0, -3500.0), false },
        { TEXT("07  STOREHOUSE  |  FUTURE SYSTEM"), FVector2D(0.0, -5500.0), false },
        { TEXT("08  DEFENCE  |  FUTURE SYSTEM"), FVector2D(-4500.0, 4500.0), false }
    };

    const int32 ZoneCount = UE_ARRAY_COUNT(Zones);

    const int32 RoutePairs[][2] =
    {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 },
        { 4, 0 }, { 2, 6 }, { 6, 5 }, { 4, 7 }
    };

    const int32 RouteCount = UE_ARRAY_COUNT(RoutePairs);

    float SmoothBlend(float Value)
    {
        const float T = FMath::Clamp(Value, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    float DistanceToSegment(const FVector2D& Point, const FVector2D& A,
        const FVector2D& B, float& OutT)
    {
        const FVector2D AB = B - A;
        const float Denominator = AB.SizeSquared();
        OutT = Denominator > KINDA_SMALL_NUMBER
            ? FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / Denominator, 0.0f, 1.0f)
            : 0.0f;
        return FVector2D::Distance(Point, A + AB * OutT);
    }
}

AVerticalSliceBWorldActor::AVerticalSliceBWorldActor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WorldRoot"));
    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("B_TerrainHeightfield"));
    TerrainMesh->SetupAttachment(RootComponent);
    TerrainMesh->bUseComplexAsSimpleCollision = true;
    TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    TrailMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("B_TrailNetwork"));
    TrailMesh->SetupAttachment(RootComponent);
    TrailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // One explicit driver for the shared world simulation in this map.
    SimulationTimeScale = 30.0;
    FixedStepSeconds = 1.0;
    MaxStepsPerFrame = 8;
    FirstRaidDelaySeconds = 10.0 * 24.0 * 3600.0;
}

void AVerticalSliceBWorldActor::BeginPlay()
{
    Super::BeginPlay();
    BuildWorld();
}

void AVerticalSliceBWorldActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!PlayerSurvivalId.IsValid())
        SetupPlayer();

    UpdatePlayerSurvival(DeltaSeconds);
    DrawStatus();
}

bool AVerticalSliceBWorldActor::BuildWorld()
{
    if (bWorldBuilt)
        return true;

    // Set the guard before creating components/actors so an accidental second
    // call cannot duplicate the world if a later optional asset is missing.
    bWorldBuilt = true;

    if (!GetWorld() || !BuildTerrain())
    {
        LastActionMessage = TEXT("SETUP BLOCKED: terrain could not be created.");
        UE_LOG(LogTemp, Error, TEXT("VerticalSlice_B could not create its terrain mesh."));
        return false;
    }

    const FLogProcessingMeshSet Family = BuildLogMeshFamily();
    if (!Family.BaseRoundMesh)
    {
        LastActionMessage = TEXT("Terrain is ready; log meshes are missing. Run BuildLogAssetManifest.py.");
        UE_LOG(LogTemp, Error, TEXT("VerticalSlice_B: no base log mesh found under %s."), DemoLogAssetRoot);
    }

    // The labels distinguish working mechanics from future staging zones.
    for (const FZonePoint& Zone : Zones)
        AddZoneSign(Zone.Label, Zone.Centre,
            Zone.bActive ? FLinearColor(0.86f, 0.73f, 0.42f) : FLinearColor(0.48f, 0.52f, 0.54f),
            Zone.bActive);

    BuildTrailMesh();
    BuildCamp(Family);
    BuildTimberStand(Family);
    BuildWoodYard(Family);
    BuildStoneYard();
    BuildConstructionYard(Family);
    BuildFutureZones();

    if (UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>())
        Survival->SetWeather(EWorldWeatherState::Wind, 0.55f);

    LastActionMessage = TEXT("VerticalSlice_B ready. Follow the marked trail: camp -> timber -> wood yard -> build yard.");
    return true;
}

bool AVerticalSliceBWorldActor::BuildTerrain()
{
    const int32 Quads = FMath::Clamp(TerrainQuadsPerAxis, 32, 160);
    TerrainPointCount = Quads + 1;
    TerrainSpacingCm = (WorldHalfExtentCm * 2.0f) / static_cast<float>(Quads);
    if (!FMath::IsFinite(TerrainSpacingCm) || TerrainSpacingCm <= 0.0f)
        return false;

    const int64 PointTotal = static_cast<int64>(TerrainPointCount) * TerrainPointCount;
    if (PointTotal <= 0 || PointTotal > 30000)
        return false;

    TerrainHeights.SetNumUninitialized(static_cast<int32>(PointTotal));
    for (int32 Y = 0; Y < TerrainPointCount; ++Y)
    {
        for (int32 X = 0; X < TerrainPointCount; ++X)
        {
            const float WorldX = -WorldHalfExtentCm + X * TerrainSpacingCm;
            const float WorldY = -WorldHalfExtentCm + Y * TerrainSpacingCm;
            TerrainHeights[TerrainIndex(X, Y)] = InitialHeightAt(WorldX, WorldY);
        }
    }

    RebuildTerrainMesh();

    UMaterialInterface* GroundMaterial = MakeTintedMaterial(FLinearColor(0.30f, 0.38f, 0.24f));
    if (GroundMaterial)
        TerrainMesh->SetMaterial(0, GroundMaterial);

    return TerrainMesh->GetNumSections() > 0;
}

int32 AVerticalSliceBWorldActor::TerrainIndex(int32 X, int32 Y) const
{
    return Y * TerrainPointCount + X;
}

float AVerticalSliceBWorldActor::RawHeightAt(float Xcm, float Ycm) const
{
    const float X = Xcm / 100.0f;
    const float Y = Ycm / 100.0f;
    return 165.0f * FMath::Sin(X / 25.0f) * FMath::Cos(Y / 31.0f)
        + 105.0f * FMath::Sin((X + Y) / 37.0f)
        + 65.0f * FMath::Cos((X - 2.0f * Y) / 19.0f);
}

float AVerticalSliceBWorldActor::InitialHeightAt(float Xcm, float Ycm) const
{
    float Height = RawHeightAt(Xcm, Ycm);

    // Broad flat pads around the working zones; blend into the surrounding hills.
    float BestZoneBlend = 0.0f;
    float BestZoneHeight = Height;
    for (const FZonePoint& Zone : Zones)
    {
        const FVector2D Centre = Zone.Centre;
        const float Distance = FVector2D::Distance(FVector2D(Xcm, Ycm), Centre);
        const float Blend = 1.0f - SmoothBlend((Distance - 1300.0f) / 1900.0f);
        if (Blend > BestZoneBlend)
        {
            BestZoneBlend = Blend;
            BestZoneHeight = RawHeightAt(Centre.X, Centre.Y);
        }
    }

    // Flatten the travel corridors between zones without creating giant ramps.
    for (const int32* Route : RoutePairs)
    {
        const FVector2D A = Zones[Route[0]].Centre;
        const FVector2D B = Zones[Route[1]].Centre;
        float T = 0.0f;
        const float Distance = DistanceToSegment(FVector2D(Xcm, Ycm), A, B, T);
        const float Blend = 1.0f - SmoothBlend((Distance - 300.0f) / 900.0f);
        if (Blend > 0.0f)
        {
            const float RouteHeight = FMath::Lerp(RawHeightAt(A.X, A.Y), RawHeightAt(B.X, B.Y), T);
            Height = FMath::Lerp(Height, RouteHeight, Blend * 0.85f);
        }
    }

    Height = FMath::Lerp(Height, BestZoneHeight, BestZoneBlend);
    return Height;
}

float AVerticalSliceBWorldActor::GetTerrainHeightAt(float Xcm, float Ycm) const
{
    if (TerrainHeights.IsEmpty() || TerrainPointCount < 2)
        return InitialHeightAt(Xcm, Ycm);

    const float GridX = FMath::Clamp((Xcm + WorldHalfExtentCm) / TerrainSpacingCm,
        0.0f, static_cast<float>(TerrainPointCount - 1));
    const float GridY = FMath::Clamp((Ycm + WorldHalfExtentCm) / TerrainSpacingCm,
        0.0f, static_cast<float>(TerrainPointCount - 1));
    const int32 X0 = FMath::FloorToInt(GridX);
    const int32 Y0 = FMath::FloorToInt(GridY);
    const int32 X1 = FMath::Min(X0 + 1, TerrainPointCount - 1);
    const int32 Y1 = FMath::Min(Y0 + 1, TerrainPointCount - 1);
    const float TX = GridX - X0;
    const float TY = GridY - Y0;

    const float H00 = TerrainHeights[TerrainIndex(X0, Y0)];
    const float H10 = TerrainHeights[TerrainIndex(X1, Y0)];
    const float H01 = TerrainHeights[TerrainIndex(X0, Y1)];
    const float H11 = TerrainHeights[TerrainIndex(X1, Y1)];
    return FMath::Lerp(FMath::Lerp(H00, H10, TX), FMath::Lerp(H01, H11, TX), TY);
}

void AVerticalSliceBWorldActor::RebuildTerrainMesh()
{
    if (!TerrainMesh || TerrainHeights.IsEmpty() || TerrainPointCount < 2)
        return;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    const int32 VertexCount = TerrainHeights.Num();
    const int32 Quads = TerrainPointCount - 1;
    Vertices.Reserve(VertexCount);
    Normals.Reserve(VertexCount);
    UVs.Reserve(VertexCount);
    VertexColors.Reserve(VertexCount);
    Tangents.Reserve(VertexCount);
    Triangles.Reserve(Quads * Quads * 6);

    for (int32 Y = 0; Y < TerrainPointCount; ++Y)
    {
        for (int32 X = 0; X < TerrainPointCount; ++X)
        {
            const float WorldX = -WorldHalfExtentCm + X * TerrainSpacingCm;
            const float WorldY = -WorldHalfExtentCm + Y * TerrainSpacingCm;
            const float Height = TerrainHeights[TerrainIndex(X, Y)];
            Vertices.Add(FVector(WorldX, WorldY, Height));

            const int32 XM = FMath::Max(0, X - 1);
            const int32 XP = FMath::Min(Quads, X + 1);
            const int32 YM = FMath::Max(0, Y - 1);
            const int32 YP = FMath::Min(Quads, Y + 1);
            const float DX = FMath::Max(TerrainSpacingCm, (XP - XM) * TerrainSpacingCm);
            const float DY = FMath::Max(TerrainSpacingCm, (YP - YM) * TerrainSpacingCm);
            const float SlopeX = (TerrainHeights[TerrainIndex(XP, Y)] - TerrainHeights[TerrainIndex(XM, Y)]) / DX;
            const float SlopeY = (TerrainHeights[TerrainIndex(X, YP)] - TerrainHeights[TerrainIndex(X, YM)]) / DY;
            Normals.Add(FVector(-SlopeX, -SlopeY, 1.0f).GetSafeNormal());

            UVs.Add(FVector2D(static_cast<float>(X) / Quads, static_cast<float>(Y) / Quads));
            VertexColors.Add(FColor(255, 255, 255, 255));
            Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));

            if (X < Quads && Y < Quads)
            {
                const int32 A = TerrainIndex(X, Y);
                const int32 B = TerrainIndex(X + 1, Y);
                const int32 C = TerrainIndex(X, Y + 1);
                const int32 D = TerrainIndex(X + 1, Y + 1);
                Triangles.Add(A); Triangles.Add(B); Triangles.Add(C);
                Triangles.Add(B); Triangles.Add(D); Triangles.Add(C);
            }
        }
    }

    TerrainMesh->ClearAllMeshSections();
    TerrainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs,
        VertexColors, Tangents, true);
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void AVerticalSliceBWorldActor::BuildTrailMesh()
{
    if (!TrailMesh || TerrainHeights.IsEmpty())
        return;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    constexpr float HalfWidthCm = 220.0f;
    for (const int32* Route : RoutePairs)
    {
        const FVector2D A = Zones[Route[0]].Centre;
        const FVector2D B = Zones[Route[1]].Centre;
        const float Length = FVector2D::Distance(A, B);
        const int32 Steps = FMath::Max(2, FMath::CeilToInt(Length / 200.0f));
        const FVector2D Direction = (B - A).GetSafeNormal();
        const FVector2D Side(-Direction.Y, Direction.X);
        const int32 Base = Vertices.Num();

        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            const float T = static_cast<float>(Step) / Steps;
            const FVector2D Centre = FMath::Lerp(A, B, T);
            for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
            {
                const float Sign = SideIndex == 0 ? 1.0f : -1.0f;
                const FVector2D Point = Centre + Side * (HalfWidthCm * Sign);
                const float Z = GetTerrainHeightAt(Point.X, Point.Y) + 7.0f;
                Vertices.Add(FVector(Point.X, Point.Y, Z));
                Normals.Add(FVector::UpVector);
                UVs.Add(FVector2D(T * Length / 500.0f, static_cast<float>(SideIndex)));
                Colors.Add(FColor(255, 255, 255, 255));
                Tangents.Add(FProcMeshTangent(FVector(Direction.X, Direction.Y, 0.0f), false));
            }

            if (Step < Steps)
            {
                const int32 A0 = Base + Step * 2;
                const int32 B0 = A0 + 1;
                const int32 A1 = A0 + 2;
                const int32 B1 = A0 + 3;
                Triangles.Add(A0); Triangles.Add(B0); Triangles.Add(A1);
                Triangles.Add(B0); Triangles.Add(B1); Triangles.Add(A1);
            }
        }
    }

    TrailMesh->ClearAllMeshSections();
    TrailMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
    if (UMaterialInterface* TrailMaterial = MakeTintedMaterial(FLinearColor(0.34f, 0.25f, 0.15f)))
        TrailMesh->SetMaterial(0, TrailMaterial);
}

UStaticMesh* AVerticalSliceBWorldActor::LoadLogMesh(const FString& Name) const
{
    const FString Path = FString(DemoLogAssetRoot) + Name + TEXT(".") + Name;
    return LoadObject<UStaticMesh>(nullptr, *Path);
}

UStaticMesh* AVerticalSliceBWorldActor::LoadBasicMesh(const TCHAR* Name) const
{
    return LoadObject<UStaticMesh>(nullptr,
        *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
}

UMaterialInterface* AVerticalSliceBWorldActor::MakeTintedMaterial(const FLinearColor& Tint) const
{
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Base)
        return UMaterial::GetDefaultMaterial(MD_Surface);

    UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(
        Base, const_cast<AVerticalSliceBWorldActor*>(this));
    if (Dynamic)
    {
        Dynamic->SetVectorParameterValue(TEXT("Color"), Tint);
        Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Tint);
    }
    return Dynamic ? static_cast<UMaterialInterface*>(Dynamic) : Base;
}

UStaticMeshComponent* AVerticalSliceBWorldActor::AddProp(const TCHAR* MeshName,
    const FVector& Location, const FVector& Scale, const FRotator& Rotation,
    const FLinearColor& Tint, bool bCollision)
{
    UStaticMesh* Mesh = LoadBasicMesh(MeshName);
    if (!Mesh)
        return nullptr;

    const FName BaseName(*FString::Printf(TEXT("BProp_%s"), MeshName));
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(
        this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), BaseName));
    if (!Component)
        return nullptr;

    Component->SetupAttachment(RootComponent);
    Component->SetStaticMesh(Mesh);
    Component->SetRelativeLocation(Location);
    Component->SetRelativeRotation(Rotation);
    Component->SetRelativeScale3D(Scale);
    Component->SetCollisionEnabled(bCollision
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bCollision)
        Component->SetCollisionProfileName(TEXT("BlockAll"));
    if (UMaterialInterface* Material = MakeTintedMaterial(Tint))
        Component->SetMaterial(0, Material);
    Component->RegisterComponent();
    return Component;
}

FLogProcessingMeshSet AVerticalSliceBWorldActor::BuildLogMeshFamily() const
{
    FLogProcessingMeshSet Family;
    Family.VariantSetId = TEXT("Log600");
    Family.NominalLengthLabel = TEXT("600 cm family");
    Family.BaseRoundMesh = LoadLogMesh(TEXT("SM_RawLog_600"));

    struct FLengthFamily
    {
        ELogLengthFraction Fraction;
        const TCHAR* Prefix;
        const TCHAR* RoundMesh;
    };
    const FLengthFamily Lengths[] =
    {
        { ELogLengthFraction::FullLength, TEXT("SM_RawLog_600"), TEXT("SM_RawLog_600") },
        { ELogLengthFraction::HalfLength, TEXT("SM_RawLog_300"), TEXT("SM_RawLog_300") },
        { ELogLengthFraction::QuarterLength, TEXT("SM_RawLog_150"), TEXT("SM_RawLog_150") },
        { ELogLengthFraction::EighthLength, TEXT("SM_RawLog_75"), TEXT("SM_RawLog_Long_01") }
    };
    struct FProfileSuffix
    {
        const TCHAR* Suffix;
        ELogCrossSectionProfile Profile;
    };
    const FProfileSuffix Profiles[] =
    {
        { TEXT("_Half"), ELogCrossSectionProfile::HalfRound },
        { TEXT("_Quarter_04"), ELogCrossSectionProfile::QuarterRound },
        { TEXT("_Hewn1"), ELogCrossSectionProfile::Hewn },
        { TEXT("_Hewn2"), ELogCrossSectionProfile::HewnTwoFaces },
        { TEXT("_Kalas"), ELogCrossSectionProfile::SquareBeam },
        { TEXT("_Plank"), ELogCrossSectionProfile::Plank },
        { TEXT("_Gutter"), ELogCrossSectionProfile::GroovedHalfRound },
        { TEXT("_Trough"), ELogCrossSectionProfile::Trough }
    };

    auto AddRow = [&Family](ELogLengthFraction Fraction,
        ELogCrossSectionProfile Profile, UStaticMesh* Mesh)
    {
        if (!Mesh)
            return;
        FLogGeometryVariantMesh Row;
        Row.LengthFraction = Fraction;
        Row.CrossSectionProfile = Profile;
        Row.Mesh = Mesh;
        if (Profile == ELogCrossSectionProfile::HalfRound)
        {
            Row.SecondPieceCenterOffsetLocal = FVector(0.0, 0.0, -3.0);
            Row.SecondPieceRotationOffsetLocal = FRotator(0.0, 0.0, 180.0);
        }
        else if (Profile == ELogCrossSectionProfile::QuarterRound)
        {
            Row.SecondPieceCenterOffsetLocal = FVector(0.0, 3.0, 0.0);
            Row.SecondPieceRotationOffsetLocal = FRotator(0.0, 180.0, 0.0);
        }
        Family.MeshVariants.Add(Row);
    };

    for (const FLengthFamily& Length : Lengths)
    {
        AddRow(Length.Fraction, ELogCrossSectionProfile::Round, LoadLogMesh(Length.RoundMesh));
        for (const FProfileSuffix& Profile : Profiles)
            AddRow(Length.Fraction, Profile.Profile,
                LoadLogMesh(FString(Length.Prefix) + Profile.Suffix));
    }

    auto AddCut = [&Family](ELogCutAction Action, ELogLengthFraction Source,
        ELogLengthFraction First, ELogLengthFraction Second, float FirstX, float SecondX)
    {
        FLogLengthCutVariant Cut;
        Cut.Action = Action;
        Cut.SourceLengthFraction = Source;
        Cut.CrossSectionProfile = ELogCrossSectionProfile::Round;
        Cut.FirstOutputLengthFraction = First;
        Cut.SecondOutputLengthFraction = Second;
        Cut.FirstOutputCenterOffsetLocal = FVector(FirstX, 0.0f, 0.0f);
        Cut.SecondOutputCenterOffsetLocal = FVector(SecondX, 0.0f, 0.0f);
        Family.LengthCutVariants.Add(Cut);
    };

    AddCut(ELogCutAction::SplitLengthHalf, ELogLengthFraction::FullLength,
        ELogLengthFraction::HalfLength, ELogLengthFraction::HalfLength, -152.0f, 152.0f);
    // Retain this recipe as an explicit blocked case when the exact 450 cm
    // mesh is absent; the log processor should reject it rather than fake a cut.
    AddCut(ELogCutAction::SplitLengthThreeQuarter, ELogLengthFraction::FullLength,
        ELogLengthFraction::ThreeQuarterLength, ELogLengthFraction::QuarterLength, -77.0f, 227.0f);
    AddCut(ELogCutAction::SplitHalfLengthIntoQuarters, ELogLengthFraction::HalfLength,
        ELogLengthFraction::QuarterLength, ELogLengthFraction::QuarterLength, -77.0f, 77.0f);
    AddCut(ELogCutAction::SplitQuarterLengthIntoEighths, ELogLengthFraction::QuarterLength,
        ELogLengthFraction::EighthLength, ELogLengthFraction::EighthLength, -39.0f, 39.0f);

    return Family;
}

ALogBuildPiece* AVerticalSliceBWorldActor::SpawnLog(const FLogProcessingMeshSet& Family,
    const FVector& Location, const FRotator& Rotation)
{
    if (!Family.BaseRoundMesh || !GetWorld())
        return nullptr;

    const FTransform Transform(Rotation, Location);
    ALogBuildPiece* Log = GetWorld()->SpawnActorDeferred<ALogBuildPiece>(
        ALogBuildPiece::StaticClass(), Transform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Log)
        return nullptr;

    Log->BaseDefinitionId = TEXT("Wood.Log.VerticalSliceB");
    Log->SelectedProcessingMeshSetId = Family.VariantSetId;
    Log->ProcessingMeshSets.Add(Family);
    Log->SetPieceStaticMesh(Family.BaseRoundMesh);
    Log->FinishSpawning(Transform);
    if (IsValid(Log) && !Log->IsHidden())
    {
        SpawnedLogs.Add(Log);
        return Log;
    }

    if (IsValid(Log))
        Log->Destroy();
    return nullptr;
}

void AVerticalSliceBWorldActor::SpawnTree(const FLogProcessingMeshSet& Family,
    const FVector& GroundLocation)
{
    UStaticMesh* Cylinder = LoadBasicMesh(TEXT("Cylinder"));
    UStaticMesh* Cone = LoadBasicMesh(TEXT("Cone"));
    if (!GetWorld() || !Cylinder || !Cone || !Family.BaseRoundMesh)
        return;

    // The trunk is a simple, deliberately cheap stand-in; it is genuinely
    // harvestable and yields logs only after the tree is grounded.
    const FVector TreeCentre = GroundLocation + FVector(0.0, 0.0, 275.0);
    const FTransform Transform(FRotator::ZeroRotator, TreeCentre);
    AHarvestableTreeActor* Tree = GetWorld()->SpawnActorDeferred<AHarvestableTreeActor>(
        AHarvestableTreeActor::StaticClass(), Transform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Tree)
        return;

    Tree->TreeMesh->SetStaticMesh(Cylinder);
    Tree->TreeMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 5.5f));
    Tree->MaxChopHealth = 4.0f;
    Tree->FallImpulse = 250000.0f;
    Tree->LogsToSpawn = 3;
    Tree->LogActorClass = ALogBuildPiece::StaticClass();
    Tree->LogMeshVariantSetId = Family.VariantSetId;
    Tree->OutputProcessingMeshSets.Add(Family);

    const float CrownHeights[] = { 100.0f, 220.0f, 335.0f };
    const float CrownScales[] = { 2.4f, 2.0f, 1.5f };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(CrownHeights); ++Index)
    {
        UStaticMeshComponent* Crown = NewObject<UStaticMeshComponent>(Tree,
            MakeUniqueObjectName(Tree, UStaticMeshComponent::StaticClass(), TEXT("B_TreeCrown")));
        Crown->SetupAttachment(Tree->TreeMesh);
        Crown->SetStaticMesh(Cone);
        Crown->SetRelativeLocation(FVector(0.0, 0.0, CrownHeights[Index]));
        Crown->SetRelativeScale3D(FVector(CrownScales[Index], CrownScales[Index], CrownScales[Index]));
        Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Crown->SetMaterial(0, MakeTintedMaterial(FLinearColor(0.15f, 0.32f, 0.13f)));
        Crown->RegisterComponent();
    }

    Tree->FinishSpawning(Transform);
    if (IsValid(Tree))
        HarvestableTrees.Add(Tree);
}

AStoneBuildingPiece* AVerticalSliceBWorldActor::SpawnLooseStone(
    const FVector& Location, const FVector& Scale)
{
    UStaticMesh* Cube = LoadBasicMesh(TEXT("Cube"));
    if (!GetWorld() || !Cube)
        return nullptr;

    AStoneBuildingPiece* Stone = GetWorld()->SpawnActorDeferred<AStoneBuildingPiece>(
        AStoneBuildingPiece::StaticClass(), FTransform(FRotator::ZeroRotator, Location),
        this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Stone)
        return nullptr;

    Stone->Mesh->SetStaticMesh(Cube);
    Stone->SetActorScale3D(Scale);
    Stone->DefinitionId = TEXT("Stone.Raw.VerticalSliceB");
    Stone->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
    if (IsValid(Stone) && !Stone->IsHidden())
    {
        Stone->Mesh->SetMaterial(0, MakeTintedMaterial(FLinearColor(0.38f, 0.40f, 0.40f)));
        return Stone;
    }
    return nullptr;
}

AStickPrimitivePiece* AVerticalSliceBWorldActor::SpawnLooseStick(
    const FVector& Location, const FRotator& Rotation)
{
    UStaticMesh* Cylinder = LoadBasicMesh(TEXT("Cylinder"));
    if (!GetWorld() || !Cylinder)
        return nullptr;

    AStickPrimitivePiece* Stick = GetWorld()->SpawnActorDeferred<AStickPrimitivePiece>(
        AStickPrimitivePiece::StaticClass(), FTransform(Rotation, Location),
        this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Stick)
        return nullptr;

    Stick->Mesh->SetStaticMesh(Cylinder);
    Stick->SetActorScale3D(FVector(0.08f, 0.08f, 1.45f));
    Stick->DefinitionId = TEXT("Stick.Raw.VerticalSliceB");
    Stick->FinishSpawning(FTransform(Rotation, Location));
    if (IsValid(Stick) && !Stick->IsHidden())
    {
        Stick->Mesh->SetMaterial(0, MakeTintedMaterial(FLinearColor(0.34f, 0.22f, 0.12f)));
        return Stick;
    }
    return nullptr;
}

void AVerticalSliceBWorldActor::AddZoneSign(const FString& Label, const FVector2D& Centre,
    const FLinearColor& Tint, bool bActive)
{
    const float Ground = GetTerrainHeightAt(Centre.X, Centre.Y);
    AddProp(TEXT("Cube"), FVector(Centre.X, Centre.Y, Ground - 6.0f),
        bActive ? FVector(9.0f, 7.0f, 0.06f) : FVector(6.0f, 4.0f, 0.04f),
        FRotator::ZeroRotator,
        bActive ? FLinearColor(0.32f, 0.33f, 0.28f) : FLinearColor(0.28f, 0.29f, 0.28f));

    const FVector SignBase(Centre.X, Centre.Y - 850.0f, Ground);
    AddProp(TEXT("Cube"), SignBase + FVector(0.0, 0.0, 170.0f),
        FVector(0.12f, 0.12f, 1.7f), FRotator::ZeroRotator,
        FLinearColor(0.26f, 0.17f, 0.10f));
    AddProp(TEXT("Cube"), SignBase + FVector(0.0, 0.0, 330.0f),
        FVector(3.8f, 0.14f, 0.85f), FRotator::ZeroRotator, Tint);

    UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this,
        MakeUniqueObjectName(this, UTextRenderComponent::StaticClass(), TEXT("B_ZoneLabel")));
    Text->SetupAttachment(RootComponent);
    Text->SetText(FText::FromString(Label));
    Text->SetWorldSize(72.0f);
    Text->SetTextRenderColor(FColor::Black);
    Text->SetRelativeLocation(SignBase + FVector(0.0, -930.0f, 330.0f));
    Text->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    Text->RegisterComponent();
}

void AVerticalSliceBWorldActor::BuildCamp(const FLogProcessingMeshSet& Family)
{
    const FVector2D Centre = Zones[0].Centre;
    const float H = GetTerrainHeightAt(Centre.X, Centre.Y);

    for (const FVector2D Offset : { FVector2D(-190.0, -160.0), FVector2D(190.0, -160.0),
                                    FVector2D(-190.0, 160.0), FVector2D(190.0, 160.0) })
        AddProp(TEXT("Cube"), FVector(Centre.X + Offset.X, Centre.Y + Offset.Y, H + 135.0f),
            FVector(0.15f, 0.15f, 2.7f), FRotator::ZeroRotator, FLinearColor(0.29f, 0.19f, 0.11f));

    AddProp(TEXT("Cube"), FVector(Centre.X - 95.0f, Centre.Y, H + 310.0f),
        FVector(2.1f, 2.5f, 0.16f), FRotator(0.0f, 0.0f, -18.0f), FLinearColor(0.31f, 0.22f, 0.13f));
    AddProp(TEXT("Cube"), FVector(Centre.X + 95.0f, Centre.Y, H + 310.0f),
        FVector(2.1f, 2.5f, 0.16f), FRotator(0.0f, 0.0f, 18.0f), FLinearColor(0.31f, 0.22f, 0.13f));

    if (Family.BaseRoundMesh)
    {
        SpawnLog(Family, FVector(Centre.X - 420.0f, Centre.Y + 250.0f, H + 25.0f), FRotator(0.0f, 0.0f, 0.0f));
        SpawnLog(Family, FVector(Centre.X + 420.0f, Centre.Y + 250.0f, H + 25.0f), FRotator(0.0f, 14.0f, 0.0f));
    }
    BuildFire();
}

void AVerticalSliceBWorldActor::BuildTimberStand(const FLogProcessingMeshSet& Family)
{
    const FVector2D Centre = Zones[1].Centre;
    const FVector2D TreeOffsets[] =
    {
        FVector2D(-2100, -700), FVector2D(-1600, 1000), FVector2D(-500, 1600),
        FVector2D(900, 1300), FVector2D(1800, 600), FVector2D(1600, -1100),
        FVector2D(400, -1700), FVector2D(-1200, -1500), FVector2D(2500, 1400)
    };

    for (const FVector2D& Offset : TreeOffsets)
    {
        const float X = Centre.X + Offset.X;
        const float Y = Centre.Y + Offset.Y;
        SpawnTree(Family, FVector(X, Y, GetTerrainHeightAt(X, Y)));
    }

    if (Family.BaseRoundMesh)
    {
        const float H = GetTerrainHeightAt(Centre.X, Centre.Y);
        SpawnLog(Family, FVector(Centre.X - 600.0f, Centre.Y - 2800.0f, H + 25.0f), FRotator::ZeroRotator);
        SpawnLog(Family, FVector(Centre.X + 250.0f, Centre.Y - 3000.0f, H + 25.0f), FRotator(0.0f, 7.0f, 0.0f));
    }
}

void AVerticalSliceBWorldActor::BuildWoodYard(const FLogProcessingMeshSet& Family)
{
    const FVector2D Centre = Zones[2].Centre;
    const float H = GetTerrainHeightAt(Centre.X, Centre.Y);

    // Low-cost workbench and separated material bays; the carried log itself
    // remains the authority for whether an operation is allowed.
    AddProp(TEXT("Cube"), FVector(Centre.X + 450.0f, Centre.Y + 350.0f, H + 70.0f),
        FVector(4.0f, 1.25f, 0.7f), FRotator::ZeroRotator, FLinearColor(0.32f, 0.21f, 0.12f));
    for (int32 Index = 0; Index < 3; ++Index)
        AddProp(TEXT("Cube"), FVector(Centre.X - 650.0f + Index * 520.0f,
            Centre.Y - 650.0f, H + 12.0f), FVector(2.0f, 1.25f, 0.12f),
            FRotator::ZeroRotator, FLinearColor(0.38f, 0.30f, 0.19f));

    if (Family.BaseRoundMesh)
    {
        SpawnLog(Family, FVector(Centre.X - 700.0f, Centre.Y + 250.0f, H + 25.0f), FRotator::ZeroRotator);
        SpawnLog(Family, FVector(Centre.X + 500.0f, Centre.Y - 150.0f, H + 25.0f), FRotator(0.0f, 8.0f, 0.0f));
        SpawnLog(Family, FVector(Centre.X - 500.0f, Centre.Y + 900.0f, H + 25.0f), FRotator(0.0f, -6.0f, 0.0f));
    }
}

void AVerticalSliceBWorldActor::BuildStoneYard()
{
    const FVector2D Centre = Zones[3].Centre;
    const float H = GetTerrainHeightAt(Centre.X, Centre.Y);

    AddProp(TEXT("Cube"), FVector(Centre.X + 250.0f, Centre.Y - 250.0f, H + 80.0f),
        FVector(3.5f, 2.0f, 0.8f), FRotator::ZeroRotator, FLinearColor(0.30f, 0.29f, 0.26f));

    const FVector2D StoneOffsets[] =
    {
        FVector2D(-500, 150), FVector2D(100, 250), FVector2D(650, 100), FVector2D(350, 650)
    };
    for (const FVector2D& Offset : StoneOffsets)
    {
        const float X = Centre.X + Offset.X;
        const float Y = Centre.Y + Offset.Y;
        SpawnLooseStone(FVector(X, Y, GetTerrainHeightAt(X, Y) + 45.0f),
            FVector(0.38f + FMath::Abs(Offset.X) / 3000.0f, 0.30f, 0.28f));
    }

    for (int32 Index = 0; Index < 6; ++Index)
    {
        const float X = Centre.X - 650.0f + Index * 260.0f;
        const float Y = Centre.Y + 1050.0f + (Index % 2) * 120.0f;
        SpawnLooseStick(FVector(X, Y, GetTerrainHeightAt(X, Y) + 45.0f),
            FRotator(0.0f, Index * 17.0f, 90.0f));
    }
}

void AVerticalSliceBWorldActor::BuildConstructionYard(const FLogProcessingMeshSet& Family)
{
    const FVector2D Centre = Zones[4].Centre;
    const float H = GetTerrainHeightAt(Centre.X, Centre.Y);
    UStaticMesh* Cube = LoadBasicMesh(TEXT("Cube"));
    UMaterialConnectionSubsystem* Connections = GetWorld()
        ? GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>() : nullptr;
    if (!Family.BaseRoundMesh || !Cube || !Connections)
        return;

    const FVector FootingLocation(Centre.X + 250.0f, Centre.Y + 250.0f, H + 20.0f);
    AStoneBuildingPiece* Footing = GetWorld()->SpawnActorDeferred<AStoneBuildingPiece>(
        AStoneBuildingPiece::StaticClass(), FTransform(FRotator::ZeroRotator, FootingLocation),
        this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Footing)
        return;

    Footing->Mesh->SetStaticMesh(Cube);
    Footing->SetActorScale3D(FVector(1.2f, 1.2f, 0.4f));
    Footing->DefinitionId = TEXT("Stone.Foundation.VerticalSliceB");
    Footing->AddCustomAnchor(TEXT("Top"), TEXT("Stone"),
        FTransform(FVector(0.0f, 0.0f, 50.0f)), true, false);
    Footing->FinishSpawning(FTransform(FRotator::ZeroRotator, FootingLocation));

    ALogBuildPiece* Log = SpawnLog(Family,
        FVector(FootingLocation.X, FootingLocation.Y, H + 120.0f), FRotator::ZeroRotator);
    if (!Log)
        return;

    Log->AddCustomSnapPoint(TEXT("Bottom"), TEXT("LogEnd"),
        FTransform(FVector(0.0f, 0.0f, -17.5f)), true, false, true, false);
    FString Reason;
    if (!Connections->TryConnect(Footing, TEXT("Top"), Log, TEXT("Bottom"), Reason))
    {
        LastActionMessage = FString::Printf(TEXT("Foundation test joint blocked: %s"), *Reason);
        UE_LOG(LogTemp, Warning, TEXT("VerticalSlice_B foundation/log joint failed: %s"), *Reason);
    }
}

void AVerticalSliceBWorldActor::BuildFutureZones()
{
    // These are intentionally labelled staging areas, not fake farming,
    // storage, production or raid systems. Their gameplay adapters are not yet
    // part of this map's accepted end-to-end slice.
    const FVector2D Farm = Zones[5].Centre;
    const float FarmH = GetTerrainHeightAt(Farm.X, Farm.Y);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        AddProp(TEXT("Cube"), FVector(Farm.X - 500.0f + Index * 330.0f,
            Farm.Y, FarmH + 15.0f), FVector(1.4f, 3.2f, 0.15f),
            FRotator::ZeroRotator, FLinearColor(0.27f, 0.22f, 0.14f));
    }
    AddProp(TEXT("Cylinder"), FVector(Farm.X + 900.0f, Farm.Y + 450.0f, FarmH + 90.0f),
        FVector(0.8f, 0.8f, 0.9f), FRotator::ZeroRotator, FLinearColor(0.22f, 0.39f, 0.55f));

    const FVector2D Store = Zones[6].Centre;
    const float StoreH = GetTerrainHeightAt(Store.X, Store.Y);
    AddProp(TEXT("Cube"), FVector(Store.X, Store.Y, StoreH + 120.0f),
        FVector(5.0f, 3.0f, 1.2f), FRotator::ZeroRotator, FLinearColor(0.30f, 0.22f, 0.13f));
    for (int32 Index = 0; Index < 3; ++Index)
        AddProp(TEXT("Cube"), FVector(Store.X - 450.0f + Index * 450.0f,
            Store.Y - 450.0f, StoreH + 25.0f), FVector(1.6f, 1.6f, 0.25f),
            FRotator::ZeroRotator, FLinearColor(0.36f, 0.25f, 0.15f));

    const FVector2D Defence = Zones[7].Centre;
    const float DefenceH = GetTerrainHeightAt(Defence.X, Defence.Y);
    for (int32 Index = 0; Index < 7; ++Index)
        AddProp(TEXT("Cube"), FVector(Defence.X - 900.0f + Index * 300.0f,
            Defence.Y, DefenceH + 90.0f), FVector(1.1f, 0.5f, 0.9f),
            FRotator::ZeroRotator, FLinearColor(0.33f, 0.32f, 0.29f));
}

void AVerticalSliceBWorldActor::BuildFire()
{
    if (!GetWorld())
        return;

    const FVector2D Camp = Zones[0].Centre;
    const float H = GetTerrainHeightAt(Camp.X, Camp.Y);
    const FVector FireLocation(Camp.X + 450.0f, Camp.Y + 150.0f, H + 25.0f);
    AActor* FireActor = GetWorld()->SpawnActor<AActor>(
        AActor::StaticClass(), FireLocation, FRotator::ZeroRotator);
    if (!FireActor)
        return;

    UStaticMeshComponent* Pile = NewObject<UStaticMeshComponent>(FireActor, TEXT("B_FirePile"));
    Pile->SetStaticMesh(LoadBasicMesh(TEXT("Cone")));
    Pile->SetRelativeScale3D(FVector(0.65f, 0.65f, 0.45f));
    Pile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pile->SetMaterial(0, MakeTintedMaterial(FLinearColor(0.74f, 0.31f, 0.08f)));
    FireActor->SetRootComponent(Pile);
    Pile->RegisterComponent();

    Fire = NewObject<UHeatSourceComponent>(FireActor, TEXT("B_HeatSource"));
    Fire->SetupAttachment(Pile);
    Fire->HeatOutputCelsius = 26.0f;
    Fire->RadiusCm = 450.0f;
    Fire->WeatherProtection = 0.0f;
    Fire->RegisterComponent();
    Fire->AddFuel(2.0f * 3600.0f);
    Fire->Ignite();
}

void AVerticalSliceBWorldActor::SetupPlayer()
{
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UWorldSurvivalSubsystem* Survival = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    if (!Pawn || !Survival)
        return;

    FWorldSurvivalProfile Profile;
    Profile.EntityId = FGuid::NewGuid();
    Profile.Location = Pawn->GetActorLocation();
    Profile.ClothingInsulation = 0.3f;
    if (Survival->RegisterProfile(Profile))
    {
        PlayerSurvivalId = Profile.EntityId;
        BindDemoInput();
    }
}

void AVerticalSliceBWorldActor::BindDemoInput()
{
    if (bInputBound)
        return;

    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Controller)
        return;

    EnableInput(Controller);
    if (!InputComponent)
        return;

    InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AVerticalSliceBWorldActor::ChopInput);
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &AVerticalSliceBWorldActor::DigInput);
    InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AVerticalSliceBWorldActor::FillInput);
    InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AVerticalSliceBWorldActor::TakeInput);
    InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AVerticalSliceBWorldActor::DropInput);
    InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AVerticalSliceBWorldActor::TendFireInput);
    bInputBound = true;
}

bool AVerticalSliceBWorldActor::AimTrace(FHitResult& OutHit, float Range) const
{
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Controller)
        return false;

    FVector Location;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Location, Rotation);
    FCollisionQueryParams Params(TEXT("VerticalSliceB_Aim"), false, Controller->GetPawn());
    return GetWorld()->LineTraceSingleByChannel(Hit, Location,
        Location + Rotation.Vector() * Range, ECC_Visibility, Params);
}

void AVerticalSliceBWorldActor::ChopInput()
{
    FHitResult Hit;
    if (!AimTrace(Hit, 500.0f))
    {
        LastActionMessage = TEXT("Aim at a marked tree and press V.");
        return;
    }

    AHarvestableTreeActor* Tree = Cast<AHarvestableTreeActor>(Hit.GetActor());
    if (!Tree)
    {
        LastActionMessage = TEXT("Aim directly at a tree trunk to chop it.");
        return;
    }

    const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const FVector Direction = Pawn ? Pawn->GetActorForwardVector() : GetActorForwardVector();
    LastActionMessage = Tree->ApplyChopDamage(2.0f, Direction)
        ? FString::Printf(TEXT("Chop landed. Tree health: %.1f"), Tree->ChopHealth)
        : TEXT("Chop rejected: tree/output configuration is not valid.");
}

float AVerticalSliceBWorldActor::EditTerrainAt(const FVector& WorldPoint, float RadiusCm, bool bAdd)
{
    if (TerrainHeights.IsEmpty() || !FMath::IsFinite(RadiusCm) || RadiusCm <= 0.0f)
        return 0.0f;

    const float RadiusSq = RadiusCm * RadiusCm;
    const float CellAreaM2 = FMath::Square(TerrainSpacingCm / 100.0f);
    float VolumeChangeM3 = 0.0f;

    for (int32 Y = 0; Y < TerrainPointCount; ++Y)
    {
        const float Py = -WorldHalfExtentCm + Y * TerrainSpacingCm;
        const float DY = Py - WorldPoint.Y;
        if (FMath::Abs(DY) > RadiusCm)
            continue;

        for (int32 X = 0; X < TerrainPointCount; ++X)
        {
            const float Px = -WorldHalfExtentCm + X * TerrainSpacingCm;
            const float DX = Px - WorldPoint.X;
            const float DistSq = DX * DX + DY * DY;
            if (DistSq > RadiusSq)
                continue;

            const float Depth = FMath::Sqrt(FMath::Max(0.0f, RadiusSq - DistSq));
            const int32 Index = TerrainIndex(X, Y);
            const float Before = TerrainHeights[Index];
            const float Target = bAdd ? WorldPoint.Z + Depth : WorldPoint.Z - Depth;
            TerrainHeights[Index] = bAdd ? FMath::Max(Before, Target) : FMath::Min(Before, Target);
            VolumeChange += (TerrainHeights[Index] - Before) / 100.0f * CellAreaM2;
        }
    }

    if (!FMath::IsNearlyZero(VolumeChange))
    {
        RebuildTerrainMesh();
        BuildTrailMesh();
    }
    return VolumeChange;
}

void AVerticalSliceBWorldActor::DigInput()
{
    FHitResult Hit;
    if (!AimTrace(Hit, 700.0f) || Hit.GetComponent() != TerrainMesh)
    {
        LastActionMessage = TEXT("Aim at the open ground (not a prop) and press X to dig a pit.");
        return;
    }

    const float Removed = -EditTerrainAt(Hit.ImpactPoint, DigRadiusCm, false);
    if (UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>())
        Survival->RecordActivity(PlayerSurvivalId, ESurvivalActivity::Dig, 30.0f, 0.0f);
    LastActionMessage = FString::Printf(TEXT("Pit edited: %.2f m3 terrain-volume estimate. Runtime-only, not saved."), Removed);
}

void AVerticalSliceBWorldActor::FillInput()
{
    FHitResult Hit;
    if (!AimTrace(Hit, 700.0f) || Hit.GetComponent() != TerrainMesh)
    {
        LastActionMessage = TEXT("Aim at a pit or ground section and press Z to fill/raise it.");
        return;
    }

    const FVector FillCentre = Hit.ImpactPoint + Hit.ImpactNormal * DigRadiusCm * 0.5f;
    const float Added = EditTerrainAt(FillCentre, DigRadiusCm, true);
    LastActionMessage = FString::Printf(TEXT("Fill estimate: %.2f m3. Runtime-only, not saved."), Added);
}

void AVerticalSliceBWorldActor::TakeInput()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UCarryLoadComponent* Load = Pawn ? Pawn->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    FHitResult Hit;
    if (!Load || !AimTrace(Hit, 400.0f) || !Cast<IStructuralPieceInterface>(Hit.GetActor())
        || Cast<ALogBuildPiece>(Hit.GetActor()))
    {
        LastActionMessage = TEXT("Aim at a loose stone or stick and press T (logs use E).");
        return;
    }

    FString Reason;
    LastActionMessage = Load->TryTakePiece(Hit.GetActor(), Reason)
        ? FString::Printf(TEXT("Stowed %s."), *Hit.GetActor()->GetClass()->GetName())
        : FString::Printf(TEXT("Cannot stow piece: %s"), *Reason);
}

void AVerticalSliceBWorldActor::DropInput()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UCarryLoadComponent* Load = Pawn ? Pawn->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    if (!Load)
        return;

    const TArray<AActor*> Carried = Load->GetCarriedPieces();
    for (int32 Index = Carried.Num() - 1; Index >= 0; --Index)
    {
        if (Cast<ALogBuildPiece>(Carried[Index]))
            continue;
        const FVector Front = Pawn->GetActorLocation()
            + Pawn->GetActorForwardVector() * 150.0f + FVector(0.0, 0.0, 40.0f);
        LastActionMessage = Load->TryDropPiece(Carried[Index], Front)
            ? TEXT("Dropped a stowed piece.")
            : TEXT("Drop failed; the ownership transition was rejected.");
        return;
    }

    LastActionMessage = TEXT("No stowed stone/stick to drop.");
}

void AVerticalSliceBWorldActor::TendFireInput()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Fire || !Pawn || FVector::Dist(Pawn->GetActorLocation(), Fire->GetComponentLocation()) > 450.0f)
    {
        LastActionMessage = TEXT("Stand next to the campfire and press H to add fuel.");
        return;
    }

    Fire->AddFuel(1800.0f);
    LastActionMessage = Fire->Ignite()
        ? TEXT("Added fuel; the campfire is burning.")
        : TEXT("The campfire has no usable fuel.");
}

void AVerticalSliceBWorldActor::UpdatePlayerSurvival(float DeltaSeconds)
{
    UWorldSurvivalSubsystem* Survival = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Survival || !Pawn || !PlayerSurvivalId.IsValid())
        return;

    Survival->UpdateLocation(PlayerSurvivalId, Pawn->GetActorLocation());
    const float Speed = Pawn->GetVelocity().Size2D();
    if (Speed > 50.0f)
    {
        const UCarryLoadComponent* Load = Pawn->FindComponentByClass<UCarryLoadComponent>();
        const float SimSeconds = FMath::Min(3600.0f,
            DeltaSeconds * static_cast<float>(SimulationTimeScale));
        Survival->RecordActivity(PlayerSurvivalId,
            Speed > 450.0f ? ESurvivalActivity::Run : ESurvivalActivity::Walk,
            SimSeconds, Load ? Load->GetCarriedMassKg() : 0.0f);
    }
}

void AVerticalSliceBWorldActor::DrawStatus() const
{
    if (!GEngine)
        return;

    const UWorldSurvivalSubsystem* Survival = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const UCarryLoadComponent* Load = Pawn ? Pawn->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    const ULogCarryComponent* LogCarry = Pawn ? Pawn->FindComponentByClass<ULogCarryComponent>() : nullptr;

    int32 Key = 7200;
    if (Survival && PlayerSurvivalId.IsValid())
    {
        FWorldSurvivalProfile Profile;
        if (Survival->GetProfile(PlayerSurvivalId, Profile))
        {
            const FColor TempColor = Profile.FeltTemperatureCelsius < Survival->ComfortTemperatureCelsius
                ? FColor::Cyan : FColor::Orange;
            GEngine->AddOnScreenDebugMessage(Key++, 0.0f, TempColor,
                FString::Printf(TEXT("Felt %.1f C | Core %.2f C | Wetness %.0f%%"),
                    Profile.FeltTemperatureCelsius, Profile.CoreTemperatureCelsius, Profile.Wetness * 100.0f));
            GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White,
                FString::Printf(TEXT("Health %.0f | Calories %.0f kcal | Stamina %.0f | Thirst %.0f"),
                    Profile.Health, Profile.CaloriesKcal, Profile.Stamina, Profile.Thirst));
        }
    }

    const FString CutAction = LogCarry
        ? StaticEnum<ELogCutAction>()->GetDisplayNameTextByValue(
            static_cast<int64>(LogCarry->GetSelectedCutAction())).ToString()
        : TEXT("-");
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White,
        FString::Printf(TEXT("VerticalSlice_B | Logs %d | Carry %.0f kg | Cut: %s"),
            SpawnedLogs.Num(), Load ? Load->GetCarriedMassKg() : 0.0f, *CutAction));
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Yellow, LastActionMessage);
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
        TEXT("WASD + Mouse | E log pick/place | G drop | C action | F process | V chop | X dig | Z fill | T stow | Y drop stowed | H fire"));
}
