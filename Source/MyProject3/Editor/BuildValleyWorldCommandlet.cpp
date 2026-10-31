#include "Editor/BuildValleyWorldCommandlet.h"

#if WITH_EDITOR
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LogSnapGameMode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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

int32 UBuildValleyWorldCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    FString MapPath = TEXT("/Game/World/Valley");
    FParse::Value(*Params, TEXT("Map="), MapPath);
    const FString WorldDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt"), TEXT("World"));

    // Import settings written by the heightmap builder.
    FString JsonText;
    TSharedPtr<FJsonObject> Info;
    if (!FFileHelper::LoadFileToString(JsonText, *FPaths::Combine(WorldDir, TEXT("valley_import.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Info) || !Info.IsValid())
    {
        UE_LOG(LogBuildValleyWorld, Error, TEXT("Missing SourceArt/World/valley_import.json; run tools/heightmaps/build_world_heightmap.py"));
        return 1;
    }
    const int32 Resolution = static_cast<int32>(Info->GetNumberField(TEXT("resolution")));
    const TArray<TSharedPtr<FJsonValue>>& Scale = Info->GetArrayField(TEXT("landscape_scale"));
    const TArray<TSharedPtr<FJsonValue>>& Location = Info->GetArrayField(TEXT("landscape_location_cm"));
    const FVector LandscapeScale(Scale[0]->AsNumber(), Scale[1]->AsNumber(), Scale[2]->AsNumber());
    const FVector LandscapeLocation(Location[0]->AsNumber(), Location[1]->AsNumber(), Location[2]->AsNumber());

    // 2017 = 16 components x 2 sections x 63 quads + 1.
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

    // The landscape.
    ALandscape* Landscape = World->SpawnActor<ALandscape>(LandscapeLocation, FRotator::ZeroRotator);
    Landscape->SetActorRelativeScale3D(LandscapeScale);
    Landscape->SetActorLabel(TEXT("ValleyLandscape"));
    TMap<FGuid, TArray<uint16>> HeightData;
    HeightData.Add(FGuid(), MoveTemp(Heights));
    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayers;
    MaterialLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
    const FString HeightmapFile = FPaths::Combine(WorldDir, TEXT("valley.r16"));
    Landscape->Import(FGuid::NewGuid(), 0, 0, Resolution - 1, Resolution - 1, SectionsPerComponent, QuadsPerSection,
        HeightData, *HeightmapFile, MaterialLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
    if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
        LandscapeInfo->UpdateLayerInfoMap(Landscape);

    // Height of the valley centre, for the player start.
    const TArray<uint16>& Stored = HeightData.FindChecked(FGuid());
    const int32 Centre = (Resolution / 2) * Resolution + Resolution / 2;
    const double CentreZ = LandscapeLocation.Z + (static_cast<double>(Stored[Centre]) - 32768.0) * LandscapeScale.Z / 128.0;

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

    // Player start and the world simulation.
    World->SpawnActor<APlayerStart>(FVector(0.0, 0.0, CentreZ + 200.0), FRotator::ZeroRotator)->SetActorLabel(TEXT("PlayerStart"));
    World->SpawnActor<AWorldDemoBootstrapActor>(FVector(0.0, 0.0, CentreZ + 100.0), FRotator::ZeroRotator)->SetActorLabel(TEXT("WorldSimulation"));
    if (AWorldSettings* Settings = World->GetWorldSettings())
        Settings->DefaultGameMode = ALogSnapGameMode::StaticClass();

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPath))
    {
        UE_LOG(LogBuildValleyWorld, Error, TEXT("Could not save %s"), *MapPath);
        return 1;
    }
    UE_LOG(LogBuildValleyWorld, Display, TEXT("Saved %s: landscape %dx%d, centre height %.0f cm"), *MapPath, Resolution, Resolution, CentreZ);
    return 0;
#else
    return 1;
#endif
}
