#include "Editor/BuildValleyWorldCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LogSnapGameMode.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WorldDemoBootstrapActor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBuildValleyWorld, Log, All);

UBuildValleyWorldCommandlet::UBuildValleyWorldCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

#if WITH_EDITOR
namespace ValleyWorld
{
    /** Saves a new asset package to disk. */
    bool SaveAsset(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost();
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Asset, *File, Args);
    }

    /** A fresh, empty material at PackagePath (an older one of the same name is moved aside). */
    UMaterial* NewMaterial(const FString& PackagePath)
    {
        const FString Name = FPackageName::GetShortName(PackagePath);
        UPackage* Package = CreatePackage(*PackagePath);
        Package->FullyLoad();
        if (UObject* Old = StaticFindObject(UObject::StaticClass(), Package, *Name))
            Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        return NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
    }

    template <typename T>
    T* Add(UMaterial* M, int32 X, int32 Y)
    {
        return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass(), X, Y));
    }

    UMaterialExpression* Colour(UMaterial* M, const FLinearColor& C, int32 X, int32 Y)
    {
        UMaterialExpressionConstant3Vector* E = Add<UMaterialExpressionConstant3Vector>(M, X, Y);
        E->Constant = C;
        return E;
    }

    /** saturate((In - Start) * Gain) */
    UMaterialExpression* Ramp(UMaterial* M, UMaterialExpression* In, float Start, float Gain, int32 X, int32 Y)
    {
        UMaterialExpressionSubtract* Sub = Add<UMaterialExpressionSubtract>(M, X, Y);
        Sub->ConstB = Start;
        UMaterialEditingLibrary::ConnectMaterialExpressions(In, TEXT(""), Sub, TEXT("A"));
        UMaterialExpressionMultiply* Mul = Add<UMaterialExpressionMultiply>(M, X + 150, Y);
        Mul->ConstB = Gain;
        UMaterialEditingLibrary::ConnectMaterialExpressions(Sub, TEXT(""), Mul, TEXT("A"));
        UMaterialExpressionSaturate* Sat = Add<UMaterialExpressionSaturate>(M, X + 300, Y);
        UMaterialEditingLibrary::ConnectMaterialExpressions(Mul, TEXT(""), Sat, TEXT(""));
        return Sat;
    }

    UMaterialExpression* Lerp(UMaterial* M, UMaterialExpression* A, UMaterialExpression* B, UMaterialExpression* Alpha, int32 X, int32 Y)
    {
        UMaterialExpressionLinearInterpolate* L = Add<UMaterialExpressionLinearInterpolate>(M, X, Y);
        UMaterialEditingLibrary::ConnectMaterialExpressions(A, TEXT(""), L, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(B, TEXT(""), L, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(Alpha, TEXT(""), L, TEXT("Alpha"));
        return L;
    }

    void Finish(UMaterial* M, UMaterialExpression* BaseColor, float Roughness)
    {
        UMaterialEditingLibrary::ConnectMaterialProperty(BaseColor, TEXT(""), MP_BaseColor);
        UMaterialExpressionConstant* R = Add<UMaterialExpressionConstant>(M, 600, 300);
        R->R = Roughness;
        UMaterialEditingLibrary::ConnectMaterialProperty(R, TEXT(""), MP_Roughness);
        UMaterialEditingLibrary::RecompileMaterial(M);
        SaveAsset(M);
    }

    /** One plain colour, for the stand-in trees, bushes and rocks. */
    UMaterial* FlatMaterial(const FString& PackagePath, const FLinearColor& C)
    {
        UMaterial* M = NewMaterial(PackagePath);
        Finish(M, Colour(M, C, 0, 0), 0.9f);
        return M;
    }

    /** Terrain colouring without textures: grass on flat ground, dirt on slopes, rock on steep
     *  faces, snow on high flat-ish ground. Colours come from the world normal and height. */
    UMaterial* TerrainMaterial(const FString& PackagePath, float SnowStartCm)
    {
        UMaterial* M = NewMaterial(PackagePath);
        UMaterialExpressionVertexNormalWS* Normal = Add<UMaterialExpressionVertexNormalWS>(M, -1200, 0);
        UMaterialExpressionComponentMask* Up = Add<UMaterialExpressionComponentMask>(M, -1050, 0);
        Up->R = false; Up->G = false; Up->B = true; Up->A = false;
        UMaterialEditingLibrary::ConnectMaterialExpressions(Normal, TEXT(""), Up, TEXT(""));
        UMaterialExpressionWorldPosition* Pos = Add<UMaterialExpressionWorldPosition>(M, -1200, 300);
        UMaterialExpressionComponentMask* Z = Add<UMaterialExpressionComponentMask>(M, -1050, 300);
        Z->R = false; Z->G = false; Z->B = true; Z->A = false;
        UMaterialEditingLibrary::ConnectMaterialExpressions(Pos, TEXT(""), Z, TEXT(""));

        // Normal Z: 0.70 = 45 deg, 0.82 = 35 deg, 0.92 = 23 deg.
        UMaterialExpression* NotRock = Ramp(M, Up, 0.70f, 8.0f, -850, -100);   // 0 at 45 deg, 1 at 35 deg
        UMaterialExpression* Grassy = Ramp(M, Up, 0.82f, 9.0f, -850, 50);      // 0 at 35 deg, 1 at 23 deg
        UMaterialExpression* Snowy = Ramp(M, Z, SnowStartCm, 1.0f / 6000.0f, -850, 300);

        UMaterialExpression* Grass = Colour(M, FLinearColor(0.10f, 0.22f, 0.05f), -400, -300);
        UMaterialExpression* Dirt = Colour(M, FLinearColor(0.22f, 0.15f, 0.08f), -400, -200);
        UMaterialExpression* Rock = Colour(M, FLinearColor(0.20f, 0.19f, 0.18f), -400, -100);
        UMaterialExpression* Snow = Colour(M, FLinearColor(0.80f, 0.82f, 0.85f), -400, 400);

        UMaterialExpression* Soil = Lerp(M, Dirt, Grass, Grassy, -200, -250);
        UMaterialExpression* Ground = Lerp(M, Rock, Soil, NotRock, 0, -150);
        UMaterialExpressionMultiply* SnowOnFlat = Add<UMaterialExpressionMultiply>(M, -200, 300);
        UMaterialEditingLibrary::ConnectMaterialExpressions(Snowy, TEXT(""), SnowOnFlat, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(NotRock, TEXT(""), SnowOnFlat, TEXT("B"));
        Finish(M, Lerp(M, Ground, Snow, SnowOnFlat, 200, 0), 0.95f);
        return M;
    }

    double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default)
    {
        double V = Default;
        if (O.IsValid())
            O->TryGetNumberField(Key, V);
        return V;
    }

    TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
    {
        const TSharedPtr<FJsonObject>* Out = nullptr;
        return O.IsValid() && O->TryGetObjectField(Key, Out) ? *Out : TSharedPtr<FJsonObject>();
    }

    /** Heights of the imported landscape, sampled in world space. */
    struct FTerrain
    {
        const TArray<uint16>* Data = nullptr;
        int32 Res = 0;
        FVector Loc, Scale;

        float Z(double X, double Y) const
        {
            const double U = FMath::Clamp((X - Loc.X) / Scale.X, 0.0, Res - 1.001);
            const double V = FMath::Clamp((Y - Loc.Y) / Scale.Y, 0.0, Res - 1.001);
            const int32 U0 = static_cast<int32>(U), V0 = static_cast<int32>(V);
            const double FU = U - U0, FV = V - V0;
            auto H = [&](int32 I, int32 J) { return static_cast<double>((*Data)[J * Res + I]); };
            const double Raw = FMath::Lerp(FMath::Lerp(H(U0, V0), H(U0 + 1, V0), FU), FMath::Lerp(H(U0, V0 + 1), H(U0 + 1, V0 + 1), FU), FV);
            return static_cast<float>(Loc.Z + (Raw - 32768.0) * Scale.Z / 128.0);
        }

        float SlopeDeg(double X, double Y) const
        {
            const double D = Scale.X * 2.0;
            const double GX = (Z(X + D, Y) - Z(X - D, Y)) / (2.0 * D);
            const double GY = (Z(X, Y + D) - Z(X, Y - D)) / (2.0 * D);
            return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(GX * GX + GY * GY))));
        }

        double HalfSizeCm() const { return (Res - 1) * Scale.X * 0.5; }
    };

    UHierarchicalInstancedStaticMeshComponent* Instances(AActor* Owner, const TCHAR* Name, const TCHAR* MeshPath, UMaterialInterface* Mat,
        bool bCollide, bool bShadow, float CullCm)
    {
        UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, Name);
        C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
        C->SetMaterial(0, Mat);
        C->SetMobility(EComponentMobility::Static);
        C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        C->SetCastShadow(bShadow);
        C->SetCullDistances(0, static_cast<int32>(CullCm));
        C->SetupAttachment(Owner->GetRootComponent());
        Owner->AddInstanceComponent(C);
        C->RegisterComponent();
        return C;
    }

    /** Stand-in forest: conifers (cylinder + cone), broadleaf trees (cylinder + sphere), bushes and
     *  rocks from engine basic shapes, as instanced meshes. Placement follows slope, height and a
     *  patch noise so there are woods, clearings and open slopes. */
    void Dress(UWorld* World, const FTerrain& T, const TSharedPtr<FJsonObject>& Cfg, const FString& MatDir)
    {
        if (!Cfg.IsValid())
            return;
        UMaterial* Bark = FlatMaterial(MatDir / TEXT("M_Flat_Bark"), FLinearColor(0.12f, 0.07f, 0.04f));
        UMaterial* Needle = FlatMaterial(MatDir / TEXT("M_Flat_Needles"), FLinearColor(0.03f, 0.10f, 0.04f));
        UMaterial* Leaf = FlatMaterial(MatDir / TEXT("M_Flat_Leaves"), FLinearColor(0.08f, 0.18f, 0.04f));
        UMaterial* Bush = FlatMaterial(MatDir / TEXT("M_Flat_Bush"), FLinearColor(0.06f, 0.14f, 0.04f));
        UMaterial* Stone = FlatMaterial(MatDir / TEXT("M_Flat_Stone"), FLinearColor(0.25f, 0.25f, 0.24f));

        AActor* Forest = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
        Forest->SetActorLabel(TEXT("Forest"));
        USceneComponent* Root = NewObject<USceneComponent>(Forest, TEXT("Root"));
        Root->SetMobility(EComponentMobility::Static);
        Forest->SetRootComponent(Root);
        Forest->AddInstanceComponent(Root);
        Root->RegisterComponent();

        const TCHAR* Cyl = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
        const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
        const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
        UHierarchicalInstancedStaticMeshComponent* Trunks = Instances(Forest, TEXT("Trunks"), Cyl, Bark, true, true, 200000.f);
        UHierarchicalInstancedStaticMeshComponent* Cones = Instances(Forest, TEXT("ConiferCrowns"), Cone, Needle, false, true, 250000.f);
        UHierarchicalInstancedStaticMeshComponent* Crowns = Instances(Forest, TEXT("BroadleafCrowns"), Sphere, Leaf, false, true, 250000.f);
        UHierarchicalInstancedStaticMeshComponent* Bushes = Instances(Forest, TEXT("Bushes"), Sphere, Bush, false, false, 60000.f);
        UHierarchicalInstancedStaticMeshComponent* Rocks = Instances(Forest, TEXT("Rocks"), Sphere, Stone, true, true, 150000.f);

        FRandomStream Rng(static_cast<int32>(Num(Cfg, TEXT("seed"), 7)));
        const double Half = T.HalfSizeCm();
        const double Clear = Num(Cfg, TEXT("clear_radius_m"), 40) * 100.0;
        TArray<FTransform> TrunkX, ConeX, CrownX, BushX, RockX;

        // Patch noise in 0..1: forests where it is high, meadows where it is low.
        const double PatchCm = Num(Obj(Cfg, TEXT("trees")), TEXT("patch_scale_m"), 250) * 100.0;
        auto Patch = [&](double X, double Y)
        {
            const double A = FMath::PerlinNoise2D(FVector2D(X, Y) / PatchCm);
            const double B = FMath::PerlinNoise2D(FVector2D(X + 7.3e5, Y - 2.1e5) / (PatchCm * 0.35));
            return 0.5 + 0.5 * FMath::Clamp(A * 0.75 + B * 0.35, -1.0, 1.0);
        };

        auto Scatter = [&](const TSharedPtr<FJsonObject>& S, TFunctionRef<void(double, double, float, float, double)> Place)
        {
            if (!S.IsValid())
                return;
            const double Cell = Num(S, TEXT("cell_m"), 10) * 100.0;
            const double MaxSlope = Num(S, TEXT("max_slope_deg"), 35);
            const double MinSlope = Num(S, TEXT("min_slope_deg"), 0);
            const double MaxZ = Num(S, TEXT("max_height_m"), 1e5) * 100.0;
            for (double Y = -Half + Cell * 0.5; Y < Half; Y += Cell)
            {
                for (double X = -Half + Cell * 0.5; X < Half; X += Cell)
                {
                    const double PX = X + Rng.FRandRange(-0.45, 0.45) * Cell;
                    const double PY = Y + Rng.FRandRange(-0.45, 0.45) * Cell;
                    if (FMath::Sqrt(PX * PX + PY * PY) < Clear)
                        continue;
                    const float Z = T.Z(PX, PY);
                    const float Slope = T.SlopeDeg(PX, PY);
                    if (Z > MaxZ || Slope > MaxSlope || Slope < MinSlope)
                        continue;
                    Place(PX, PY, Z, Slope, Patch(PX, PY));
                }
            }
        };

        const TSharedPtr<FJsonObject> TreeCfg = Obj(Cfg, TEXT("trees"));
        const double Threshold = Num(TreeCfg, TEXT("patch_threshold"), 0.45);
        const double MinH = Num(TreeCfg, TEXT("min_height_m"), 7) * 100.0, MaxH = Num(TreeCfg, TEXT("max_height_m_tree"), 16) * 100.0;
        const double Broadleaf = Num(TreeCfg, TEXT("broadleaf_share"), 0.3);
        Scatter(TreeCfg, [&](double X, double Y, float Z, float Slope, double P)
        {
            // Dense inside a patch, thinning out over its edge.
            if (Rng.FRand() > FMath::Clamp((P - Threshold) / 0.12, 0.0, 1.0))
                return;
            const double H = Rng.FRandRange(MinH, MaxH);
            const double TrunkD = H * Rng.FRandRange(0.022, 0.03);
            const FRotator Yaw(0.0, Rng.FRandRange(0.0, 360.0), 0.0);
            if (Rng.FRand() < Broadleaf)
            {
                const double TrunkH = H * 0.55, CrownD = H * Rng.FRandRange(0.45, 0.6);
                TrunkX.Add(FTransform(Yaw, FVector(X, Y, Z + TrunkH * 0.5 - 30.0), FVector(TrunkD / 100.0, TrunkD / 100.0, TrunkH / 100.0)));
                CrownX.Add(FTransform(Yaw, FVector(X, Y, Z + H - CrownD * 0.45), FVector(CrownD / 100.0, CrownD / 100.0, CrownD * 0.85 / 100.0)));
            }
            else
            {
                const double TrunkH = H * 0.4, CrownH = H * 0.75, CrownD = H * Rng.FRandRange(0.28, 0.36);
                TrunkX.Add(FTransform(Yaw, FVector(X, Y, Z + TrunkH * 0.5 - 30.0), FVector(TrunkD / 100.0, TrunkD / 100.0, TrunkH / 100.0)));
                ConeX.Add(FTransform(Yaw, FVector(X, Y, Z + H - CrownH * 0.5), FVector(CrownD / 100.0, CrownD / 100.0, CrownH / 100.0)));
            }
        });

        const TSharedPtr<FJsonObject> BushCfg = Obj(Cfg, TEXT("bushes"));
        const double BushChance = Num(BushCfg, TEXT("chance"), 0.35);
        Scatter(BushCfg, [&](double X, double Y, float Z, float Slope, double P)
        {
            // Mostly along forest edges and inside woods.
            if (Rng.FRand() > BushChance * FMath::Clamp((P - Threshold + 0.15) / 0.2, 0.15, 1.0))
                return;
            const double S = Rng.FRandRange(80.0, 220.0);
            BushX.Add(FTransform(FRotator(0.0, Rng.FRandRange(0.0, 360.0), 0.0), FVector(X, Y, Z + S * 0.2),
                FVector(S / 100.0 * Rng.FRandRange(0.8, 1.3), S / 100.0, S * 0.6 / 100.0)));
        });

        const TSharedPtr<FJsonObject> RockCfg = Obj(Cfg, TEXT("rocks"));
        const double RockChance = Num(RockCfg, TEXT("chance"), 0.5);
        const double RockMin = Num(RockCfg, TEXT("min_size_m"), 0.8) * 100.0, RockMax = Num(RockCfg, TEXT("max_size_m"), 3.5) * 100.0;
        Scatter(RockCfg, [&](double X, double Y, float Z, float Slope, double P)
        {
            // More rocks on slopes.
            if (Rng.FRand() > RockChance * FMath::Clamp(Slope / 30.0, 0.2, 1.0))
                return;
            const double S = FMath::Lerp(RockMin, RockMax, FMath::Pow(Rng.FRand(), 2.0));
            RockX.Add(FTransform(FRotator(Rng.FRandRange(-15.0, 15.0), Rng.FRandRange(0.0, 360.0), Rng.FRandRange(-15.0, 15.0)),
                FVector(X, Y, Z + S * 0.1), FVector(S / 100.0 * Rng.FRandRange(0.9, 1.4), S / 100.0, S * Rng.FRandRange(0.45, 0.7) / 100.0)));
        });

        Trunks->AddInstances(TrunkX, false, true);
        Cones->AddInstances(ConeX, false, true);
        Crowns->AddInstances(CrownX, false, true);
        Bushes->AddInstances(BushX, false, true);
        Rocks->AddInstances(RockX, false, true);
        UE_LOG(LogBuildValleyWorld, Display, TEXT("Forest: %d trees (%d conifer, %d broadleaf), %d bushes, %d rocks"),
            TrunkX.Num(), ConeX.Num(), CrownX.Num(), BushX.Num(), RockX.Num());
    }
}
#endif

int32 UBuildValleyWorldCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    using namespace ValleyWorld;
    FString MapPath = TEXT("/Game/World/Valley");
    FParse::Value(*Params, TEXT("Map="), MapPath);
    FString GameModeName = TEXT("ThirdPerson");
    FParse::Value(*Params, TEXT("GameMode="), GameModeName);
    const FString WorldDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt"), TEXT("World"));
    const FString MatDir = FPackageName::GetLongPackagePath(MapPath) / TEXT("Materials");

    // Import settings written by the heightmap builder.
    FString JsonText;
    TSharedPtr<FJsonObject> Info;
    if (!FFileHelper::LoadFileToString(JsonText, *FPaths::Combine(WorldDir, TEXT("valley_import.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Info) || !Info.IsValid())
    {
        UE_LOG(LogBuildValleyWorld, Error, TEXT("Missing SourceArt/World/valley_import.json; run tools/heightmaps/build_world_heightmap.py"));
        return 1;
    }
    // World dressing settings (trees, bushes, rocks) live in the layout.
    TSharedPtr<FJsonObject> Layout;
    FString LayoutText;
    if (FFileHelper::LoadFileToString(LayoutText, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Config"), TEXT("GameData"), TEXT("world_layout.json"))))
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LayoutText), Layout);

    const int32 Resolution = static_cast<int32>(Info->GetNumberField(TEXT("resolution")));
    const TArray<TSharedPtr<FJsonValue>>& Scale = Info->GetArrayField(TEXT("landscape_scale"));
    const TArray<TSharedPtr<FJsonValue>>& Location = Info->GetArrayField(TEXT("landscape_location_cm"));
    const FVector LandscapeScale(Scale[0]->AsNumber(), Scale[1]->AsNumber(), Scale[2]->AsNumber());
    const FVector LandscapeLocation(Location[0]->AsNumber(), Location[1]->AsNumber(), Location[2]->AsNumber());

    // n x 126 + 1 = n components x 2 sections x 63 quads + 1.
    constexpr int32 SectionsPerComponent = 2;
    constexpr int32 QuadsPerSection = 63;
    if ((Resolution - 1) % (SectionsPerComponent * QuadsPerSection) != 0)
    {
        UE_LOG(LogBuildValleyWorld, Error, TEXT("Resolution %d is not (n x 126) + 1"), Resolution);
        return 1;
    }

    TArray<uint8> Raw;
    if (!FFileHelper::LoadFileToArray(Raw, *FPaths::Combine(WorldDir, TEXT("valley.r16"))) ||
        Raw.Num() != Resolution * Resolution * 2)
    {
        UE_LOG(LogBuildValleyWorld, Error, TEXT("valley.r16 missing or not %d x %d x 16-bit"), Resolution, Resolution);
        return 1;
    }
    TArray<uint16> Heights;
    Heights.SetNumUninitialized(Resolution * Resolution);
    FMemory::Memcpy(Heights.GetData(), Raw.GetData(), Raw.Num()); // little-endian on Windows

    UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World)
    {
        UE_LOG(LogBuildValleyWorld, Error, TEXT("Could not create a blank map"));
        return 1;
    }

    // The landscape, coloured by slope and height.
    const double SnowStartM = Num(Obj(Layout, TEXT("dressing")), TEXT("snow_start_m"), 600);
    UMaterial* TerrainMat = TerrainMaterial(MatDir / TEXT("M_WorldTerrain"), static_cast<float>(SnowStartM * 100.0));
    ALandscape* Landscape = World->SpawnActor<ALandscape>(LandscapeLocation, FRotator::ZeroRotator);
    Landscape->SetActorRelativeScale3D(LandscapeScale);
    Landscape->SetActorLabel(TEXT("WorldLandscape"));
    Landscape->LandscapeMaterial = TerrainMat;
    TMap<FGuid, TArray<uint16>> HeightData;
    HeightData.Add(FGuid(), MoveTemp(Heights));
    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayers;
    MaterialLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
    const FString HeightmapFile = FPaths::Combine(WorldDir, TEXT("valley.r16"));
    Landscape->Import(FGuid::NewGuid(), 0, 0, Resolution - 1, Resolution - 1, SectionsPerComponent, QuadsPerSection,
        HeightData, *HeightmapFile, MaterialLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
    if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
        LandscapeInfo->UpdateLayerInfoMap(Landscape);

    FTerrain Terrain;
    Terrain.Data = &HeightData.FindChecked(FGuid());
    Terrain.Res = Resolution;
    Terrain.Loc = LandscapeLocation;
    Terrain.Scale = LandscapeScale;
    const double CentreZ = Terrain.Z(0.0, 0.0);

    // Sun, sky and fog.
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, CentreZ + 2000.0), FRotator(-40.0, 30.0, 0.0));
    Sun->SetActorLabel(TEXT("Sun"));
    if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        SunLight->SetAtmosphereSunLight(true);
    World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator)->SetActorLabel(TEXT("SkyAtmosphere"));
    ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.0, 0.0, CentreZ + 1000.0), FRotator::ZeroRotator);
    Sky->SetActorLabel(TEXT("SkyLight"));
    if (USkyLightComponent* SkyComponent = Sky->GetLightComponent())
        SkyComponent->bRealTimeCapture = true;
    World->SpawnActor<AExponentialHeightFog>(FVector(0.0, 0.0, CentreZ), FRotator::ZeroRotator)->SetActorLabel(TEXT("HeightFog"));

    // Trees, bushes and rocks (stand-ins).
    Dress(World, Terrain, Obj(Layout, TEXT("dressing")), MatDir);

    // Player start, the world simulation and the game mode (the template third person character by default).
    World->SpawnActor<APlayerStart>(FVector(0.0, 0.0, CentreZ + 200.0), FRotator::ZeroRotator)->SetActorLabel(TEXT("PlayerStart"));
    World->SpawnActor<AWorldDemoBootstrapActor>(FVector(0.0, 0.0, CentreZ + 100.0), FRotator::ZeroRotator)->SetActorLabel(TEXT("WorldSimulation"));
    UClass* GameMode = ALogSnapGameMode::StaticClass();
    if (GameModeName == TEXT("ThirdPerson"))
    {
        if (UClass* ThirdPerson = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C")))
            GameMode = ThirdPerson;
        else
            UE_LOG(LogBuildValleyWorld, Warning, TEXT("BP_ThirdPersonGameMode not found; using LogSnapGameMode"));
    }
    if (AWorldSettings* Settings = World->GetWorldSettings())
        Settings->DefaultGameMode = GameMode;

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPath))
    {
        UE_LOG(LogBuildValleyWorld, Error, TEXT("Could not save %s"), *MapPath);
        return 1;
    }
    UE_LOG(LogBuildValleyWorld, Display, TEXT("Saved %s: landscape %dx%d, centre height %.0f cm, game mode %s"),
        *MapPath, Resolution, Resolution, CentreZ, *GameMode->GetName());
    return 0;
#else
    return 1;
#endif
}
