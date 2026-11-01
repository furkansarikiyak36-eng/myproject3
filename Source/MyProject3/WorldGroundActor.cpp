#include "WorldGroundActor.h"

#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "TerrainDigToolComponent.h"
#include "TextureResource.h"
#include "VoxelTerrainChunkComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGround, Log, All);

AWorldGroundActor::AWorldGroundActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FIntPoint AWorldGroundActor::ColumnOf(double X, double Y) const
{
    return FIntPoint(FMath::FloorToInt(X / ChunkSizeCm()), FMath::FloorToInt(Y / ChunkSizeCm()));
}

FIntVector AWorldGroundActor::ChunkOf(const FVector& World) const
{
    return FIntVector(FMath::FloorToInt(World.X / ChunkSizeCm()), FMath::FloorToInt(World.Y / ChunkSizeCm()),
        FMath::FloorToInt(World.Z / ChunkSizeCm()));
}

bool AWorldGroundActor::SurfaceZ(double X, double Y, float& OutZ) const
{
    if (!Landscape)
        return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WorldGroundSurface), true);
    if (!Landscape->ActorLineTraceSingle(Hit, FVector(X, Y, 1.0e6), FVector(X, Y, -1.0e6), ECC_Visibility, Params))
        return false;
    OutZ = static_cast<float>(Hit.ImpactPoint.Z);
    return true;
}

float AWorldGroundActor::SoilDepthCmAt(double X, double Y) const
{
    if (SoilGridSize <= 0 || SoilDepthM.Num() != SoilGridSize * SoilGridSize || SoilCellCm <= 0.0f)
        return 0.0f;
    const int32 C = FMath::Clamp(FMath::RoundToInt((X - SoilOriginCm.X) / SoilCellCm), 0, SoilGridSize - 1);
    const int32 R = FMath::Clamp(FMath::RoundToInt((Y - SoilOriginCm.Y) / SoilCellCm), 0, SoilGridSize - 1);
    return SoilDepthM[R * SoilGridSize + C] * 100.0f;
}

bool AWorldGroundActor::IsDugColumn(const FVector& WorldLocation) const
{
    return DugColumns.Contains(ColumnOf(WorldLocation.X, WorldLocation.Y));
}

const AWorldGroundActor::FColumnData& AWorldGroundActor::Column(const FIntPoint& Key)
{
    if (const FColumnData* Found = ColumnCache.Find(Key))
        return *Found;
    FColumnData Data;
    const int32 N = ChunkCells + 1;
    Data.SurfaceZ.SetNumUninitialized(N * N);
    Data.FloorZ.SetNumUninitialized(N * N);
    Data.MinSurface = TNumericLimits<float>::Max();
    Data.MaxSurface = TNumericLimits<float>::Lowest();
    Data.MinFloor = TNumericLimits<float>::Max();
    float Last = 0.0f;
    for (int32 J = 0; J < N; ++J)
        for (int32 I = 0; I < N; ++I)
        {
            const double X = Key.X * ChunkSizeCm() + I * VoxelSizeCm;
            const double Y = Key.Y * ChunkSizeCm() + J * VoxelSizeCm;
            float Z = Last;
            if (SurfaceZ(X, Y, Z))
                Last = Z;
            const float Depth = SoilDepthCmAt(X, Y);
            // Rock (no soil) gets a floor above the surface: nothing there can be dug.
            const float Floor = Depth > 0.0f ? Z - Depth : Z + 1000.0f;
            Data.SurfaceZ[J * N + I] = Z;
            Data.FloorZ[J * N + I] = Floor;
            Data.MinSurface = FMath::Min(Data.MinSurface, Z);
            Data.MaxSurface = FMath::Max(Data.MaxSurface, Z);
            if (Depth > 0.0f)
                Data.MinFloor = FMath::Min(Data.MinFloor, Floor);
        }
    if (Data.MinFloor == TNumericLimits<float>::Max())
        Data.MinFloor = Data.MinSurface;
    return ColumnCache.Add(Key, MoveTemp(Data));
}

UVoxelTerrainChunkComponent* AWorldGroundActor::EnsureChunk(const FIntVector& Key, bool bSolidAboveSurface)
{
    if (TObjectPtr<UVoxelTerrainChunkComponent>* Found = Chunks.Find(Key))
        return *Found;
    const FIntPoint ColumnKey(Key.X, Key.Y);
    const FColumnData& Data = Column(ColumnKey);
    const FVector Origin(Key.X * ChunkSizeCm(), Key.Y * ChunkSizeCm(), Key.Z * ChunkSizeCm());

    UVoxelTerrainChunkComponent* Chunk = NewObject<UVoxelTerrainChunkComponent>(this);
    Chunk->CellsPerAxis = ChunkCells;
    Chunk->VoxelSizeCm = VoxelSizeCm;
    Chunk->bAllowPartialEdits = true;
    Chunk->bGroundColours = true;
    Chunk->MaxDigRadiusCm = 400.0f;
    const float CentreDepth = SoilDepthCmAt(Origin.X + ChunkSizeCm() * 0.5, Origin.Y + ChunkSizeCm() * 0.5);
    Chunk->SoilClass = (CentreDepth > 0.0f && !bSolidAboveSurface) ? EVoxelSoilClass::SoftSoil : EVoxelSoilClass::Rock;
    if (VoxelMaterial)
        Chunk->SetMaterial(0, VoxelMaterial);
    Chunk->SetupAttachment(RootComponent);
    Chunk->RegisterComponent();
    Chunk->SetWorldLocation(Origin);

    TArray<float> SurfaceLocal, FloorLocal;
    SurfaceLocal.Reserve(Data.SurfaceZ.Num());
    FloorLocal.Reserve(Data.FloorZ.Num());
    for (const float Z : Data.SurfaceZ)
        SurfaceLocal.Add(Z - static_cast<float>(Origin.Z));
    for (const float Z : Data.FloorZ)
        FloorLocal.Add(Z - static_cast<float>(Origin.Z));
    Chunk->InitializeFromColumns(SurfaceLocal, FloorLocal, bSolidAboveSurface);
    Chunk->RebuildMesh();
    Chunks.Add(Key, Chunk);
    return Chunk;
}

void AWorldGroundActor::DigColumn(const FIntPoint& Key)
{
    if (DugColumns.Contains(Key))
        return;
    const FColumnData& Data = Column(Key);
    const float Bottom = FMath::Min(Data.MinFloor, Data.MinSurface) - 200.0f;
    const float Top = Data.MaxSurface + 200.0f;
    for (int32 Z = FMath::FloorToInt(Bottom / ChunkSizeCm()); Z <= FMath::FloorToInt(Top / ChunkSizeCm()); ++Z)
    {
        const FIntVector ChunkKey(Key.X, Key.Y, Z);
        if (TObjectPtr<UVoxelTerrainChunkComponent>* Existing = Chunks.Find(ChunkKey))
        {
            // A cave chunk under a shown landscape was fully solid; open the air above the ground.
            (*Existing)->OpenAboveSurface();
            (*Existing)->RebuildMesh();
        }
        else
        {
            EnsureChunk(ChunkKey, false);
        }
    }

    // Pawns and physics bodies inside this box stop colliding with the (now hidden) landscape.
    UBoxComponent* Box = NewObject<UBoxComponent>(this);
    Box->SetupAttachment(RootComponent);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldDynamic);
    Box->SetCollisionResponseToAllChannels(ECR_Overlap);
    Box->SetGenerateOverlapEvents(true);
    Box->SetBoxExtent(FVector(ChunkSizeCm() * 0.5, ChunkSizeCm() * 0.5, (Top - Bottom) * 0.5 + 300.0f));
    Box->RegisterComponent();
    Box->SetWorldLocation(FVector((Key.X + 0.5) * ChunkSizeCm(), (Key.Y + 0.5) * ChunkSizeCm(), (Top + Bottom) * 0.5));
    DugColumns.Add(Key, Box);
    SetMask(Key);
}

bool AWorldGroundActor::DigAt(const FVector& WorldCenter, float RadiusCm, float ToolStrength, FString& OutStatus)
{
    if (!Landscape || WorldCenter.ContainsNaN() || RadiusCm <= 0.0f || ToolStrength <= 0.0f)
    {
        OutStatus = TEXT("Kazı yapılamadı: zemin hazır değil.");
        return false;
    }
    const FIntVector CentreChunk = ChunkOf(WorldCenter);
    if (!DugColumns.Contains(ColumnOf(WorldCenter.X, WorldCenter.Y)) && !Chunks.Contains(CentreChunk) &&
        SoilDepthCmAt(WorldCenter.X, WorldCenter.Y) <= 0.0f)
    {
        OutStatus = TEXT("Burası kaya: bu aletle kazılamaz.");
        return false;
    }

    // Columns whose surface the sphere reaches (plus a 1 m margin, so a pawn standing at a
    // column border is always on voxel ground once it ignores the landscape) turn into voxels.
    const float Reach = RadiusCm + 100.0f;
    const FIntPoint MinCol = ColumnOf(WorldCenter.X - Reach, WorldCenter.Y - Reach);
    const FIntPoint MaxCol = ColumnOf(WorldCenter.X + Reach, WorldCenter.Y + Reach);
    for (int32 Y = MinCol.Y; Y <= MaxCol.Y; ++Y)
        for (int32 X = MinCol.X; X <= MaxCol.X; ++X)
        {
            const FIntPoint Key(X, Y);
            if (DugColumns.Contains(Key))
                continue;
            const double PX = FMath::Clamp(WorldCenter.X, X * ChunkSizeCm(), (X + 1) * ChunkSizeCm());
            const double PY = FMath::Clamp(WorldCenter.Y, Y * ChunkSizeCm(), (Y + 1) * ChunkSizeCm());
            float Surface = 0.0f;
            if (SurfaceZ(PX, PY, Surface) && WorldCenter.Z + Reach > Surface)
                DigColumn(Key);
        }

    const FIntVector MinChunk = ChunkOf(WorldCenter - FVector(RadiusCm));
    const FIntVector MaxChunk = ChunkOf(WorldCenter + FVector(RadiusCm));
    int32 Applied = 0;
    float Removed = 0.0f;
    bool bTooWeak = false;
    for (int32 Z = MinChunk.Z; Z <= MaxChunk.Z; ++Z)
        for (int32 Y = MinChunk.Y; Y <= MaxChunk.Y; ++Y)
            for (int32 X = MinChunk.X; X <= MaxChunk.X; ++X)
            {
                const FIntVector Key(X, Y, Z);
                UVoxelTerrainChunkComponent* Chunk = nullptr;
                if (TObjectPtr<UVoxelTerrainChunkComponent>* Found = Chunks.Find(Key))
                    Chunk = *Found;
                else if (!DugColumns.Contains(FIntPoint(X, Y)) && Chunks.Contains(CentreChunk))
                    Chunk = EnsureChunk(Key, true); // extending a cave into solid rock
                if (!Chunk)
                    continue;
                const float Needed = Chunk->SoilClass == EVoxelSoilClass::Rock ? Chunk->RockToolStrength
                    : Chunk->SoilClass == EVoxelSoilClass::HardSoil ? Chunk->HardSoilToolStrength : Chunk->SoftSoilToolStrength;
                if (ToolStrength < Needed)
                {
                    bTooWeak = true;
                    continue;
                }
                const FVector Local = Chunk->GetComponentTransform().InverseTransformPosition(WorldCenter);
                float ChunkRemoved = 0.0f;
                int64 Revision = 0;
                if (Chunk->TryApplyBoundedDig(FGuid::NewGuid(), Local, RadiusCm, ToolStrength,
                    Chunk->GetDensityRevision(), ChunkRemoved, Revision))
                {
                    ++Applied;
                    Removed += ChunkRemoved;
                }
            }

    if (Applied > 0)
    {
        OutStatus = FString::Printf(TEXT("Kazıldı: %.2f m³"), Removed);
        return true;
    }
    OutStatus = bTooWeak ? TEXT("Kaya: daha güçlü bir alet gerekir.")
        : TEXT("Daha fazla kazılamaz: kaya tabanına (toprak bitti) ulaşıldı veya burada toprak yok.");
    return false;
}

void AWorldGroundActor::CarveCaves()
{
    TSet<UVoxelTerrainChunkComponent*> Touched;
    for (const FWorldCaveSpec& Cave : Caves)
    {
        // Entrances: where a tunnel sphere reaches the surface, the landscape opens.
        for (const FVector4f& S : Cave.Spheres)
        {
            const FVector C(S.X, S.Y, S.Z);
            float Surface = 0.0f;
            if (!SurfaceZ(C.X, C.Y, Surface) || C.Z + S.W < Surface - 150.0f)
                continue;
            const FIntPoint MinCol = ColumnOf(C.X - S.W - 100.0, C.Y - S.W - 100.0);
            const FIntPoint MaxCol = ColumnOf(C.X + S.W + 100.0, C.Y + S.W + 100.0);
            for (int32 Y = MinCol.Y; Y <= MaxCol.Y; ++Y)
                for (int32 X = MinCol.X; X <= MaxCol.X; ++X)
                    DigColumn(FIntPoint(X, Y));
        }
        for (const FVector4f& S : Cave.Spheres)
        {
            const FVector C(S.X, S.Y, S.Z);
            const FIntVector MinChunk = ChunkOf(C - FVector(S.W));
            const FIntVector MaxChunk = ChunkOf(C + FVector(S.W));
            for (int32 Z = MinChunk.Z; Z <= MaxChunk.Z; ++Z)
                for (int32 Y = MinChunk.Y; Y <= MaxChunk.Y; ++Y)
                    for (int32 X = MinChunk.X; X <= MaxChunk.X; ++X)
                    {
                        const FIntVector Key(X, Y, Z);
                        UVoxelTerrainChunkComponent* Chunk = EnsureChunk(Key, !DugColumns.Contains(FIntPoint(X, Y)));
                        Chunk->CarveIgnoringFloor(Chunk->GetComponentTransform().InverseTransformPosition(C), S.W);
                        Touched.Add(Chunk);
                    }
        }
    }
    for (UVoxelTerrainChunkComponent* Chunk : Touched)
        Chunk->RebuildMesh();
    UE_LOG(LogWorldGround, Display, TEXT("Caves: %d carved, %d chunks, %d open columns"), Caves.Num(), Chunks.Num(), DugColumns.Num());
}

void AWorldGroundActor::CreateMask()
{
    const int32 Res = FMath::Max(1, MaskResolution);
    MaskData.Init(0, Res * Res);
    MaskTexture = UTexture2D::CreateTransient(Res, Res, PF_G8);
    if (!MaskTexture)
        return;
    MaskTexture->Filter = TF_Nearest;
    MaskTexture->SRGB = false;
    MaskTexture->AddressX = TA_Clamp;
    MaskTexture->AddressY = TA_Clamp;
    MaskTexture->NeverStream = true;
    FTexture2DMipMap& Mip = MaskTexture->GetPlatformData()->Mips[0];
    void* Pixels = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memzero(Pixels, Res * Res);
    Mip.BulkData.Unlock();
    MaskTexture->UpdateResource();
    if (Landscape)
        Landscape->SetLandscapeMaterialTextureParameterValue(MaskParameterName, MaskTexture);
}

void AWorldGroundActor::SetMask(const FIntPoint& Key)
{
    const int32 Res = FMath::Max(1, MaskResolution);
    const int32 TX = Key.X - FMath::FloorToInt(MaskOriginCm / ChunkSizeCm());
    const int32 TY = Key.Y - FMath::FloorToInt(MaskOriginCm / ChunkSizeCm());
    if (!MaskTexture || TX < 0 || TY < 0 || TX >= Res || TY >= Res)
        return;
    MaskData[TY * Res + TX] = 255;
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(TX, TY, 0, 0, 1, 1);
    uint8* Pixel = new uint8(255);
    MaskTexture->UpdateTextureRegions(0, 1, Region, 1, 1, Pixel,
        [](uint8* Data, const FUpdateTextureRegion2D* Regions) { delete Data; delete Regions; });
}

void AWorldGroundActor::UpdateBypass()
{
    TSet<UPrimitiveComponent*> Inside;
    TArray<UPrimitiveComponent*> Overlaps;
    for (const TPair<FIntPoint, TObjectPtr<UBoxComponent>>& Pair : DugColumns)
    {
        Overlaps.Reset();
        Pair.Value->GetOverlappingComponents(Overlaps);
        for (UPrimitiveComponent* Prim : Overlaps)
        {
            AActor* PrimOwner = Prim ? Prim->GetOwner() : nullptr;
            // Moving things only: a character's capsule (its root) or a simulating body.
            if (!PrimOwner || PrimOwner == this || Prim->Mobility != EComponentMobility::Movable)
                continue;
            if (Prim->IsSimulatingPhysics() || (Cast<APawn>(PrimOwner) && PrimOwner->GetRootComponent() == Prim))
                Inside.Add(Prim);
        }
    }
    for (UPrimitiveComponent* Prim : Inside)
    {
        if (!Bypassing.Contains(Prim))
        {
            Bypassing.Add(Prim, Prim->GetCollisionResponseToChannel(ECC_WorldGround));
            Prim->SetCollisionResponseToChannel(ECC_WorldGround, ECR_Ignore);
        }
    }
    for (auto It = Bypassing.CreateIterator(); It; ++It)
    {
        UPrimitiveComponent* Prim = It.Key().Get();
        if (!Prim)
        {
            It.RemoveCurrent();
        }
        else if (!Inside.Contains(Prim))
        {
            Prim->SetCollisionResponseToChannel(ECC_WorldGround, It.Value());
            It.RemoveCurrent();
        }
    }
}

void AWorldGroundActor::GiveTestDigTool()
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!bGivePlayerTestDigTool || !Pawn || !Pawn->InputComponent || Pawn->FindComponentByClass<UTerrainDigToolComponent>())
        return;
    UTerrainDigToolComponent* Tool = NewObject<UTerrainDigToolComponent>(Pawn);
    Tool->TraceDistanceCm = TestDigReachCm;
    Tool->DigRadiusCm = TestDigRadiusCm;
    Tool->bRequireInventoryTool = false;
    Tool->RegisterComponent();
    Tool->BindDefaultInput(Pawn->InputComponent);
    UE_LOG(LogWorldGround, Display, TEXT("Test dig tool (N) given to %s"), *Pawn->GetName());
}

void AWorldGroundActor::BeginPlay()
{
    Super::BeginPlay();
    if (!Landscape)
    {
        UE_LOG(LogWorldGround, Warning, TEXT("WorldGroundActor has no landscape; digging is off."));
        return;
    }
    CreateMask();
    CarveCaves();
}

void AWorldGroundActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    GiveTestDigTool();
    UpdateBypass();
}
