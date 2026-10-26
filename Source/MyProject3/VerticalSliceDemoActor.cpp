#include "VerticalSliceDemoActor.h"

#include "CarryLoadComponent.h"
#include "HeatSourceComponent.h"
#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "MaterialConnectionSubsystem.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "StructuralPieceInterface.h"
#include "VoxelTerrainChunkComponent.h"
#include "WorldSurvivalSubsystem.h"

#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const TCHAR* LogAssetRoot = TEXT("/Game/DemoAssets/Logs/");
    constexpr int32 ChunkCells = 32;
    constexpr float VoxelSize = 50.0f;
    constexpr float ChunkSizeCm = ChunkCells * VoxelSize;
    constexpr float GroundDepthCm = 800.0f;

    UStaticMesh* LoadLogMesh(const FString& Name)
    {
        const FString Path = FString(LogAssetRoot) + Name + TEXT(".") + Name;
        return LoadObject<UStaticMesh>(nullptr, *Path);
    }

    UStaticMesh* LoadDemoShape(const TCHAR* Name)
    {
        return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
    }

    void MakeLoosePhysical(UStaticMeshComponent* Mesh)
    {
        if (!Mesh)
            return;
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetSimulatePhysics(true);
    }
}

AVerticalSliceDemoActor::AVerticalSliceDemoActor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SliceRoot"));
    // Hours-scale survival must be visible within minutes; raids are out of scope for this slice.
    SimulationTimeScale = 30.0;
    FixedStepSeconds = 1.0;
    MaxStepsPerFrame = 8;
    FirstRaidDelaySeconds = 10.0 * 24.0 * 3600.0;
}

void AVerticalSliceDemoActor::BeginPlay()
{
    Super::BeginPlay();
    BuildSlice();
}

bool AVerticalSliceDemoActor::BuildSlice()
{
    if (bSliceBuilt || !GetWorld())
        return bSliceBuilt;
    bSliceBuilt = true;

    BuildTerrain();
    const FLogProcessingMeshSet Family = BuildLogMeshFamily();
    if (Family.BaseRoundMesh)
    {
        SpawnLog(Family, FVector(-200.0, 250.0, 40.0), FRotator(0.0, 10.0, 0.0));
        SpawnLog(Family, FVector(-200.0, 450.0, 40.0), FRotator(0.0, -5.0, 0.0));
        SpawnLog(Family, FVector(-200.0, 650.0, 40.0), FRotator(0.0, 3.0, 0.0));
        BuildFootingWithLog(Family);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Vertical slice: log meshes missing under %s; run BuildLogAssetManifest.py."), LogAssetRoot);
    }
    SpawnStonesAndSticks();
    SpawnFire();

    if (UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>())
        Survival->SetWeather(EWorldWeatherState::Wind, 0.7f);
    LastActionMessage = TEXT("Welcome. Stay near the fire: the wind is cold.");
    return true;
}

void AVerticalSliceDemoActor::BuildTerrain()
{
    const float Half = ChunksPerSide * ChunkSizeCm * 0.5f;
    for (int32 X = 0; X < ChunksPerSide; ++X)
    {
        for (int32 Y = 0; Y < ChunksPerSide; ++Y)
        {
            UVoxelTerrainChunkComponent* Chunk = NewObject<UVoxelTerrainChunkComponent>(this);
            Chunk->CellsPerAxis = ChunkCells;
            Chunk->VoxelSizeCm = VoxelSize;
            Chunk->SetupAttachment(RootComponent);
            Chunk->SetRelativeLocation(FVector(X * ChunkSizeCm - Half, Y * ChunkSizeCm - Half, -GroundDepthCm));
            Chunk->RegisterComponent();
            Chunk->InitializeFlatGround(GroundDepthCm);
            Chunk->RebuildMesh();
            Chunks.Add(Chunk);
        }
    }
}

float AVerticalSliceDemoActor::EditTerrainAt(FVector WorldPoint, float RadiusCm, bool bAdd)
{
    float VolumeChange = 0.0f;
    for (UVoxelTerrainChunkComponent* Chunk : Chunks)
    {
        if (!Chunk)
            continue;
        const FVector Local = Chunk->GetComponentTransform().InverseTransformPosition(WorldPoint);
        // Neighbouring chunks share their border samples; editing every chunk
        // the sphere touches keeps the seam closed.
        if (Local.X < -RadiusCm || Local.Y < -RadiusCm || Local.Z < -RadiusCm ||
            Local.X > ChunkSizeCm + RadiusCm || Local.Y > ChunkSizeCm + RadiusCm || Local.Z > ChunkSizeCm + RadiusCm)
            continue;
        VolumeChange += Chunk->ApplySphereEdit(Local, RadiusCm, bAdd);
        Chunk->RebuildMesh();
    }
    return VolumeChange;
}

FLogProcessingMeshSet AVerticalSliceDemoActor::BuildLogMeshFamily() const
{
    FLogProcessingMeshSet Family;
    Family.VariantSetId = TEXT("Log600");
    Family.NominalLengthLabel = TEXT("600 cm (measured, see LOG_ASSET_MANIFEST.md)");
    Family.BaseRoundMesh = LoadLogMesh(TEXT("SM_RawLog_600"));

    struct FLengthFamily { ELogLengthFraction Fraction; const TCHAR* Prefix; const TCHAR* RoundMesh; };
    const FLengthFamily Lengths[] =
    {
        { ELogLengthFraction::FullLength, TEXT("SM_RawLog_600"), nullptr },
        { ELogLengthFraction::HalfLength, TEXT("SM_RawLog_300"), TEXT("SM_RawLog_300") },
        { ELogLengthFraction::QuarterLength, TEXT("SM_RawLog_150"), TEXT("SM_RawLog_150") },
        // No plain 75 cm round mesh exists; the 75 cm "Long_01" trunk is the measured stand-in.
        { ELogLengthFraction::EighthLength, TEXT("SM_RawLog_75"), TEXT("SM_RawLog_Long_01") },
    };
    struct FProfileSuffix { const TCHAR* Suffix; ELogCrossSectionProfile Profile; };
    const FProfileSuffix Profiles[] =
    {
        { TEXT("_Half"), ELogCrossSectionProfile::HalfRound },
        { TEXT("_Quarter_04"), ELogCrossSectionProfile::QuarterRound },
        { TEXT("_Hewn1"), ELogCrossSectionProfile::Hewn },
        { TEXT("_Hewn2"), ELogCrossSectionProfile::HewnTwoFaces },
        { TEXT("_Kalas"), ELogCrossSectionProfile::SquareBeam },
        { TEXT("_Plank"), ELogCrossSectionProfile::Plank },
        { TEXT("_Gutter"), ELogCrossSectionProfile::GroovedHalfRound },
        { TEXT("_Trough"), ELogCrossSectionProfile::Trough },
        { TEXT("_FlatNotch_TopBottom"), ELogCrossSectionProfile::Notched },
    };

    auto AddRow = [&Family](ELogLengthFraction Fraction, ELogCrossSectionProfile Profile, UStaticMesh* Mesh)
    {
        if (!Mesh)
            return;
        FLogGeometryVariantMesh Row;
        Row.LengthFraction = Fraction;
        Row.CrossSectionProfile = Profile;
        Row.Mesh = Mesh;
        // Measured meshes (35 cm diameter, X axis): the half-round sits above its
        // pivot and its rolled twin below it; quarter-rounds mirror across Y.
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
        if (Length.RoundMesh)
            AddRow(Length.Fraction, ELogCrossSectionProfile::Round, LoadLogMesh(Length.RoundMesh));
        for (const FProfileSuffix& Profile : Profiles)
            AddRow(Length.Fraction, Profile.Profile, LoadLogMesh(FString(Length.Prefix) + Profile.Suffix));
    }

    auto AddCut = [&Family](ELogCutAction Action, ELogLengthFraction Source, ELogLengthFraction First,
        ELogLengthFraction Second, float FirstX, float SecondX)
    {
        FLogLengthCutVariant Cut;
        Cut.Action = Action;
        Cut.SourceLengthFraction = Source;
        Cut.FirstOutputLengthFraction = First;
        Cut.SecondOutputLengthFraction = Second;
        Cut.FirstOutputCenterOffsetLocal = FVector(FirstX, 0.0, 0.0);
        Cut.SecondOutputCenterOffsetLocal = FVector(SecondX, 0.0, 0.0);
        // Split and hewn pieces (planks, half and quarter logs...) are cut to
        // length the same way as round logs; each output needs its own mesh.
        for (const ELogCrossSectionProfile Profile : { ELogCrossSectionProfile::Round,
            ELogCrossSectionProfile::HalfRound, ELogCrossSectionProfile::QuarterRound,
            ELogCrossSectionProfile::Hewn, ELogCrossSectionProfile::HewnTwoFaces,
            ELogCrossSectionProfile::SquareBeam, ELogCrossSectionProfile::Plank,
            ELogCrossSectionProfile::GroovedHalfRound, ELogCrossSectionProfile::Trough })
        {
            Cut.CrossSectionProfile = Profile;
            Family.LengthCutVariants.Add(Cut);
        }
    };
    AddCut(ELogCutAction::SplitLengthHalf, ELogLengthFraction::FullLength,
        ELogLengthFraction::HalfLength, ELogLengthFraction::HalfLength, -152.0f, 152.0f);
    // No 450 cm mesh exists, so this recipe stays blocked and shows its reason.
    AddCut(ELogCutAction::SplitLengthThreeQuarter, ELogLengthFraction::FullLength,
        ELogLengthFraction::ThreeQuarterLength, ELogLengthFraction::QuarterLength, -77.0f, 227.0f);
    AddCut(ELogCutAction::SplitHalfLengthIntoQuarters, ELogLengthFraction::HalfLength,
        ELogLengthFraction::QuarterLength, ELogLengthFraction::QuarterLength, -77.0f, 77.0f);
    AddCut(ELogCutAction::SplitQuarterLengthIntoEighths, ELogLengthFraction::QuarterLength,
        ELogLengthFraction::EighthLength, ELogLengthFraction::EighthLength, -39.0f, 39.0f);
    return Family;
}

ALogBuildPiece* AVerticalSliceDemoActor::SpawnLog(
    const FLogProcessingMeshSet& Family, const FVector& Location, const FRotator& Rotation)
{
    const FTransform Transform(Rotation, GetActorLocation() + Location);
    ALogBuildPiece* Log = GetWorld()->SpawnActorDeferred<ALogBuildPiece>(
        ALogBuildPiece::StaticClass(), Transform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Log)
        return nullptr;
    Log->BaseDefinitionId = TEXT("Wood.Log.Demo");
    Log->ProcessingMeshSets.Add(Family);
    Log->SetPieceStaticMesh(Family.BaseRoundMesh);
    Log->FinishSpawning(Transform);
    SpawnedLogs.Add(Log);
    return Log;
}

void AVerticalSliceDemoActor::BuildFootingWithLog(const FLogProcessingMeshSet& Family)
{
    UStaticMesh* Cube = LoadDemoShape(TEXT("Cube"));
    AStoneBuildingPiece* Footing = GetWorld()->SpawnActor<AStoneBuildingPiece>(
        GetActorLocation() + FVector(350.0, -450.0, 20.0), FRotator::ZeroRotator);
    ALogBuildPiece* Log = SpawnLog(Family, FVector(350.0, -450.0, 120.0), FRotator::ZeroRotator);
    UMaterialConnectionSubsystem* Connections = GetWorld()->GetSubsystem<UMaterialConnectionSubsystem>();
    if (!Footing || !Log || !Cube || !Connections)
        return;

    Footing->Mesh->SetStaticMesh(Cube);
    Footing->SetActorScale3D(FVector(1.2, 1.2, 0.4));
    Footing->AddCustomAnchor(TEXT("Top"), TEXT("Stone"), FTransform(FVector(0.0, 0.0, 50.0)), true, false);
    Footing->SetHeldState(true);
    Footing->TryCommitGroundPlacement(Footing->GetActorLocation(), FRotator::ZeroRotator);

    Log->AddCustomSnapPoint(TEXT("Bottom"), TEXT("LogEnd"), FTransform(FVector(0.0, 0.0, -17.5)),
        true, false, true, false);
    FString Reason;
    bFootingJointBuilt = Connections->TryConnect(Footing, TEXT("Top"), Log, TEXT("Bottom"), Reason);
    if (!bFootingJointBuilt)
        UE_LOG(LogTemp, Warning, TEXT("Vertical slice: stone footing joint failed: %s"), *Reason);
}

void AVerticalSliceDemoActor::SpawnStonesAndSticks()
{
    UStaticMesh* Cube = LoadDemoShape(TEXT("Cube"));
    UStaticMesh* Cylinder = LoadDemoShape(TEXT("Cylinder"));
    for (int32 Index = 0; Index < 4; ++Index)
    {
        AStoneBuildingPiece* Stone = GetWorld()->SpawnActor<AStoneBuildingPiece>(
            GetActorLocation() + FVector(150.0 + Index * 60.0, 300.0, 40.0), FRotator(0.0, Index * 25.0, 0.0));
        if (!Stone || !Cube)
            continue;
        Stone->Mesh->SetStaticMesh(Cube);
        Stone->SetActorScale3D(FVector(0.3, 0.25, 0.22));
        Stone->AddCustomAnchor(TEXT("Top"), TEXT("Stone"), FTransform(FVector(0.0, 0.0, 50.0)), true, false);
        MakeLoosePhysical(Stone->Mesh);
    }
    for (int32 Index = 0; Index < 6; ++Index)
    {
        AStickPrimitivePiece* Stick = GetWorld()->SpawnActor<AStickPrimitivePiece>(
            GetActorLocation() + FVector(150.0 + Index * 35.0, 520.0, 30.0), FRotator(90.0, Index * 15.0, 0.0));
        if (!Stick || !Cylinder)
            continue;
        Stick->Mesh->SetStaticMesh(Cylinder);
        Stick->SetActorScale3D(FVector(0.05, 0.05, 1.0));
        MakeLoosePhysical(Stick->Mesh);
    }
}

void AVerticalSliceDemoActor::SpawnFire()
{
    AActor* FireActor = GetWorld()->SpawnActor<AActor>(GetActorLocation() + FVector(450.0, 150.0, 0.0), FRotator::ZeroRotator);
    if (!FireActor)
        return;
    UStaticMeshComponent* Pile = NewObject<UStaticMeshComponent>(FireActor);
    Pile->SetStaticMesh(LoadDemoShape(TEXT("Cone")));
    FireActor->SetRootComponent(Pile);
    Pile->SetWorldScale3D(FVector(0.6, 0.6, 0.4));
    Pile->RegisterComponent();
    FireActor->SetActorLocation(GetActorLocation() + FVector(450.0, 150.0, 20.0));

    Fire = NewObject<UHeatSourceComponent>(FireActor);
    Fire->SetupAttachment(Pile);
    Fire->HeatOutputCelsius = 26.0f;
    Fire->RadiusCm = 450.0f;
    Fire->WeatherProtection = 0.7f; // stone ring around the fire
    Fire->RegisterComponent();
    Fire->AddFuel(2.0f * 3600.0f);
    Fire->Ignite();
}

void AVerticalSliceDemoActor::SetupPlayer()
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
    if (Survival->RegisterProfile(Profile))
        PlayerSurvivalId = Profile.EntityId;
    BindDemoInput();
}

void AVerticalSliceDemoActor::BindDemoInput()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    if (!Controller)
        return;
    EnableInput(Controller);
    if (!InputComponent)
        return;
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &AVerticalSliceDemoActor::DigInput);
    InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AVerticalSliceDemoActor::FillInput);
    InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AVerticalSliceDemoActor::TakeInput);
    InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AVerticalSliceDemoActor::DropInput);
    InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AVerticalSliceDemoActor::TendFireInput);
}

bool AVerticalSliceDemoActor::AimTrace(FHitResult& OutHit, float Range) const
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    if (!Controller)
        return false;
    FVector Location;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Location, Rotation);
    FCollisionQueryParams Params(TEXT("VerticalSliceAim"), false, Controller->GetPawn());
    return GetWorld()->LineTraceSingleByChannel(OutHit, Location, Location + Rotation.Vector() * Range,
        ECC_Visibility, Params);
}

void AVerticalSliceDemoActor::DigInput()
{
    FHitResult Hit;
    if (!AimTrace(Hit, 600.0f) || !Hit.GetComponent() || !Hit.GetComponent()->IsA<UVoxelTerrainChunkComponent>())
    {
        LastActionMessage = TEXT("Aim at the ground to dig.");
        return;
    }
    const float Removed = -EditTerrainAt(Hit.ImpactPoint, DigRadiusCm, false);
    if (UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>())
        Survival->RecordActivity(PlayerSurvivalId, ESurvivalActivity::Dig, 30.0f, 0.0f);
    LastActionMessage = FString::Printf(TEXT("Dug %.2f m3 of soil."), Removed);
}

void AVerticalSliceDemoActor::FillInput()
{
    FHitResult Hit;
    if (!AimTrace(Hit, 600.0f) || !Hit.GetComponent() || !Hit.GetComponent()->IsA<UVoxelTerrainChunkComponent>())
    {
        LastActionMessage = TEXT("Aim at the ground to fill.");
        return;
    }
    const float Added = EditTerrainAt(Hit.ImpactPoint + Hit.ImpactNormal * DigRadiusCm * 0.5f, DigRadiusCm, true);
    LastActionMessage = FString::Printf(TEXT("Filled %.2f m3 of soil."), Added);
}

void AVerticalSliceDemoActor::TakeInput()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UCarryLoadComponent* Load = Pawn ? Pawn->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    FHitResult Hit;
    if (!Load || !AimTrace(Hit, 400.0f) || !Cast<IStructuralPieceInterface>(Hit.GetActor()) ||
        Cast<ALogBuildPiece>(Hit.GetActor()))
    {
        LastActionMessage = TEXT("Aim at a stone or stick to take it (logs use E).");
        return;
    }
    FString Reason;
    LastActionMessage = Load->TryTakePiece(Hit.GetActor(), Reason)
        ? FString::Printf(TEXT("Took %s."), *Hit.GetActor()->GetClass()->GetName())
        : FString::Printf(TEXT("Cannot take: %s"), *Reason);
}

void AVerticalSliceDemoActor::DropInput()
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
        const FVector Front = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 120.0f + FVector(0.0, 0.0, 30.0);
        LastActionMessage = Load->TryDropPiece(Carried[Index], Front)
            ? TEXT("Dropped a stowed piece.") : TEXT("Could not drop the piece.");
        return;
    }
    LastActionMessage = TEXT("Nothing stowed to drop.");
}

void AVerticalSliceDemoActor::TendFireInput()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Fire || !Pawn || FVector::Dist(Pawn->GetActorLocation(), Fire->GetComponentLocation()) > 350.0f)
    {
        LastActionMessage = TEXT("Stand next to the fire to tend it.");
        return;
    }
    Fire->AddFuel(1800.0f);
    LastActionMessage = Fire->Ignite() ? TEXT("Added fuel; the fire burns.") : TEXT("The fire has no fuel.");
}

void AVerticalSliceDemoActor::UpdatePlayerSurvival(float DeltaSeconds)
{
    UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Survival || !Pawn || !PlayerSurvivalId.IsValid())
        return;

    Survival->UpdateLocation(PlayerSurvivalId, Pawn->GetActorLocation());
    const float Speed = Pawn->GetVelocity().Size2D();
    if (Speed > 50.0f)
    {
        const UCarryLoadComponent* Load = Pawn->FindComponentByClass<UCarryLoadComponent>();
        const float SimSeconds = FMath::Min(3600.0f, DeltaSeconds * static_cast<float>(SimulationTimeScale));
        Survival->RecordActivity(PlayerSurvivalId,
            Speed > 450.0f ? ESurvivalActivity::Run : ESurvivalActivity::Walk,
            SimSeconds, Load ? Load->GetCarriedMassKg() : 0.0f);
    }
}

void AVerticalSliceDemoActor::DrawStatus() const
{
    if (!GEngine)
        return;
    const UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    FWorldSurvivalProfile Profile;
    if (!Survival || !Survival->GetProfile(PlayerSurvivalId, Profile))
        return;

    const UCarryLoadComponent* Load = Pawn ? Pawn->FindComponentByClass<UCarryLoadComponent>() : nullptr;
    const ULogCarryComponent* LogCarry = Pawn ? Pawn->FindComponentByClass<ULogCarryComponent>() : nullptr;
    const FString CutAction = LogCarry
        ? StaticEnum<ELogCutAction>()->GetDisplayNameTextByValue(static_cast<int64>(LogCarry->GetSelectedCutAction())).ToString()
        : TEXT("-");
    const FColor Cold = Profile.FeltTemperatureCelsius < Survival->ComfortTemperatureCelsius ? FColor::Cyan : FColor::Orange;

    int32 Key = 7100;
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, Cold, FString::Printf(
        TEXT("Felt %.1f C | Core %.2f C | Wetness %.0f%%"), Profile.FeltTemperatureCelsius,
        Profile.CoreTemperatureCelsius, Profile.Wetness * 100.0f));
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White, FString::Printf(
        TEXT("Health %.0f | Calories %.0f kcal | Stamina %.0f | Thirst %.0f"), Profile.Health,
        Profile.CaloriesKcal, Profile.Stamina, Profile.Thirst));
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White, FString::Printf(
        TEXT("Carry %.0f kg (speed x%.2f) | Fire %s, %.0f min fuel | Cut action: %s"),
        Load ? Load->GetCarriedMassKg() : 0.0f, Load ? Load->GetSpeedMultiplier() : 1.0f,
        Fire && Fire->IsLit() ? TEXT("lit") : TEXT("out"), Fire ? Fire->GetFuelSeconds() / 60.0f : 0.0f, *CutAction));
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Yellow, LastActionMessage);
    GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
        TEXT("E log pick/place | G drop | C cut action | F process | X dig | Z fill | T take stone/stick | Y drop | H tend fire"));
}

void AVerticalSliceDemoActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!PlayerSurvivalId.IsValid())
        SetupPlayer();
    UpdatePlayerSurvival(DeltaSeconds);
    DrawStatus();
}
