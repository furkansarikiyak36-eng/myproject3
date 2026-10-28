#include "VoxelTerrainChunkComponent.h"

#include "Generators/MarchingCubes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStructuralGraphSubsystem.h"

UVoxelTerrainChunkComponent::UVoxelTerrainChunkComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bUseComplexAsSimpleCollision = true;
    SetCollisionProfileName(TEXT("BlockAll"));
}

int32 UVoxelTerrainChunkComponent::PointIndex(int32 X, int32 Y, int32 Z) const
{
    const int32 N = PointsPerAxis();
    return (Z * N + Y) * N + X;
}

namespace
{
    // A sample exactly on the iso value lets Marching Cubes leave pinholes at
    // grid points; keep stored values strictly off the surface.
    float AvoidIsoValue(float Value)
    {
        constexpr float Epsilon = 0.01f;
        return FMath::Abs(Value) < Epsilon ? Epsilon : Value;
    }
}

float UVoxelTerrainChunkComponent::GetPoint(int32 X, int32 Y, int32 Z) const
{
    const int32 N = PointsPerAxis();
    X = FMath::Clamp(X, 0, N - 1);
    Y = FMath::Clamp(Y, 0, N - 1);
    Z = FMath::Clamp(Z, 0, N - 1);
    return Density.IsValidIndex(PointIndex(X, Y, Z)) ? Density[PointIndex(X, Y, Z)] : 1.0f;
}

void UVoxelTerrainChunkComponent::InitializeFlatGround(float GroundHeightCm)
{
    if (CellsPerAxis < 4 || CellsPerAxis > 128 ||
        !FMath::IsFinite(VoxelSizeCm) || VoxelSizeCm < 5.0f ||
        !FMath::IsFinite(GroundHeightCm))
    {
        Density.Reset();
        DensityRevision = 0;
        DigReceipts.Reset();
        ClearAllMeshSections();
        return;
    }

    const int32 N = PointsPerAxis();
    Density.SetNumUninitialized(N * N * N);
    for (int32 Z = 0; Z < N; ++Z)
    {
        // Signed distance to the ground plane: negative below ground (solid).
        const float Height = Z * VoxelSizeCm;
        for (int32 Y = 0; Y < N; ++Y)
            for (int32 X = 0; X < N; ++X)
                Density[PointIndex(X, Y, Z)] = AvoidIsoValue(Height - GroundHeightCm);
    }
    DensityRevision = 1;
    DigReceipts.Reset();
}

float UVoxelTerrainChunkComponent::SampleDensity(const FVector& LocalPosition) const
{
    // Trilinear interpolation keeps the extracted surface smooth between samples.
    const FVector Grid = LocalPosition / VoxelSizeCm;
    const int32 X0 = FMath::FloorToInt(Grid.X);
    const int32 Y0 = FMath::FloorToInt(Grid.Y);
    const int32 Z0 = FMath::FloorToInt(Grid.Z);
    const float TX = static_cast<float>(Grid.X - X0);
    const float TY = static_cast<float>(Grid.Y - Y0);
    const float TZ = static_cast<float>(Grid.Z - Z0);
    const float C00 = FMath::Lerp(GetPoint(X0, Y0, Z0), GetPoint(X0 + 1, Y0, Z0), TX);
    const float C10 = FMath::Lerp(GetPoint(X0, Y0 + 1, Z0), GetPoint(X0 + 1, Y0 + 1, Z0), TX);
    const float C01 = FMath::Lerp(GetPoint(X0, Y0, Z0 + 1), GetPoint(X0 + 1, Y0, Z0 + 1), TX);
    const float C11 = FMath::Lerp(GetPoint(X0, Y0 + 1, Z0 + 1), GetPoint(X0 + 1, Y0 + 1, Z0 + 1), TX);
    return FMath::Lerp(FMath::Lerp(C00, C10, TY), FMath::Lerp(C01, C11, TY), TZ);
}

bool UVoxelTerrainChunkComponent::HasInitializedDensity() const
{
    if (CellsPerAxis < 4 || CellsPerAxis > 128 ||
        !FMath::IsFinite(VoxelSizeCm) || VoxelSizeCm < 5.0f)
        return false;
    const int64 N = static_cast<int64>(CellsPerAxis) + 1;
    return Density.Num() == N * N * N && DensityRevision > 0;
}

float UVoxelTerrainChunkComponent::GetRequiredToolStrength() const
{
    switch (SoilClass)
    {
    case EVoxelSoilClass::SoftSoil:
        return SoftSoilToolStrength;
    case EVoxelSoilClass::HardSoil:
        return HardSoilToolStrength;
    case EVoxelSoilClass::Rock:
        return RockToolStrength;
    default:
        return TNumericLimits<float>::Max();
    }
}

float UVoxelTerrainChunkComponent::ApplySphereEdit(FVector LocalCenter, float RadiusCm, bool bAdd)
{
    if (!HasInitializedDensity() || !FMath::IsFinite(RadiusCm) ||
        RadiusCm <= 0.0f || LocalCenter.ContainsNaN() || DensityRevision == MAX_int64)
        return 0.0f;

    const float VolumeBefore = GetSolidVolumeCubicMetres();
    bool bChangedDensity = false;
    const int32 N = PointsPerAxis();
    const int32 MinX = FMath::Max(0, FMath::FloorToInt((LocalCenter.X - RadiusCm) / VoxelSizeCm) - 1);
    const int32 MaxX = FMath::Min(N - 1, FMath::CeilToInt((LocalCenter.X + RadiusCm) / VoxelSizeCm) + 1);
    const int32 MinY = FMath::Max(0, FMath::FloorToInt((LocalCenter.Y - RadiusCm) / VoxelSizeCm) - 1);
    const int32 MaxY = FMath::Min(N - 1, FMath::CeilToInt((LocalCenter.Y + RadiusCm) / VoxelSizeCm) + 1);
    const int32 MinZ = FMath::Max(0, FMath::FloorToInt((LocalCenter.Z - RadiusCm) / VoxelSizeCm) - 1);
    const int32 MaxZ = FMath::Min(N - 1, FMath::CeilToInt((LocalCenter.Z + RadiusCm) / VoxelSizeCm) + 1);
    for (int32 Z = MinZ; Z <= MaxZ; ++Z)
        for (int32 Y = MinY; Y <= MaxY; ++Y)
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                const FVector Point(X * VoxelSizeCm, Y * VoxelSizeCm, Z * VoxelSizeCm);
                // Signed distance to the sphere, negative inside it.
                const float Sphere = static_cast<float>(FVector::Dist(Point, LocalCenter)) - RadiusCm;
                float& Value = Density[PointIndex(X, Y, Z)];
                const float Updated = AvoidIsoValue(bAdd ? FMath::Min(Value, Sphere) : FMath::Max(Value, -Sphere));
                if (Updated != Value)
                {
                    Value = Updated;
                    bChangedDensity = true;
                }
            }

    if (bChangedDensity)
        ++DensityRevision;
    return GetSolidVolumeCubicMetres() - VolumeBefore;
}

bool UVoxelTerrainChunkComponent::TryApplyBoundedDig(
    const FGuid& OperationId, FVector LocalCenter, float RadiusCm, float ToolStrength,
    int64 ExpectedRevision, float& OutRemovedVolumeCubicMetres, int64& OutResultingRevision)
{
    OutRemovedVolumeCubicMetres = 0.0f;
    OutResultingRevision = DensityRevision;

    if (!OperationId.IsValid() || !HasInitializedDensity() ||
        LocalCenter.ContainsNaN() || !FMath::IsFinite(RadiusCm) || RadiusCm <= 0.0f ||
        !FMath::IsFinite(ToolStrength) || ToolStrength <= 0.0f ||
        !FMath::IsFinite(MaxDigRadiusCm) || MaxDigRadiusCm <= 0.0f ||
        !FMath::IsFinite(SupportSafetyMarginCm) || SupportSafetyMarginCm < 0.0f ||
        !FMath::IsFinite(SoftSoilToolStrength) || SoftSoilToolStrength <= 0.0f ||
        !FMath::IsFinite(HardSoilToolStrength) || HardSoilToolStrength <= 0.0f ||
        !FMath::IsFinite(RockToolStrength) || RockToolStrength <= 0.0f ||
        static_cast<uint8>(SoilClass) > static_cast<uint8>(EVoxelSoilClass::Rock) ||
        RadiusCm > MaxDigRadiusCm || DensityRevision == MAX_int64 ||
        MaxRememberedDigOperations <= 0)
        return false;

    if (const FVoxelTerrainDigReceipt* ExistingReceipt = DigReceipts.Find(OperationId))
    {
        const bool bSameRequest =
            ExistingReceipt->LocalCenter.Equals(LocalCenter, 0.01f) &&
            FMath::IsNearlyEqual(ExistingReceipt->RadiusCm, RadiusCm, 0.01f) &&
            FMath::IsNearlyEqual(ExistingReceipt->ToolStrength, ToolStrength, 0.01f) &&
            ExistingReceipt->ExpectedRevision == ExpectedRevision;
        if (!bSameRequest)
            return false;
        OutRemovedVolumeCubicMetres = ExistingReceipt->RemovedVolumeCubicMetres;
        OutResultingRevision = ExistingReceipt->ResultingRevision;
        return true;
    }

    if (ExpectedRevision != DensityRevision ||
        ToolStrength < GetRequiredToolStrength() ||
        DigReceipts.Num() >= MaxRememberedDigOperations ||
        !EditableBoundsLocal.IsValid())
        return false;

    const FVector RadiusVector(RadiusCm);
    const FVector Min = LocalCenter - RadiusVector;
    const FVector Max = LocalCenter + RadiusVector;
    const float ExtentCm = CellsPerAxis * VoxelSizeCm;
    const FBox ChunkBounds(FVector::ZeroVector, FVector(ExtentCm));
    if (!EditableBoundsLocal.IsInsideOrOn(Min) || !EditableBoundsLocal.IsInsideOrOn(Max) ||
        !ChunkBounds.IsInsideOrOn(Min) || !ChunkBounds.IsInsideOrOn(Max))
        return false;

    // Do not undermine a registered, ground-supported structural root. The
    // graph is authoritative for support topology; actor locations are used
    // only as a conservative spatial exclusion zone, never to infer support.
    if (UWorld* World = GetWorld())
    {
        if (UWorldStructuralGraphSubsystem* Graph = World->GetSubsystem<UWorldStructuralGraphSubsystem>())
        {
            if (Graph->GetNodeCount() > 0)
            {
                TArray<FWorldStructuralNodeSnapshot> GraphSnapshot;
                if (!Graph->ExportSnapshot(GraphSnapshot))
                    return false;

                TMap<FGuid, AActor*> ActorsByIdentity;
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Candidate = *It;
                    UWorldObjectIdentityComponent* Identity =
                        IsValid(Candidate) ? Candidate->FindComponentByClass<UWorldObjectIdentityComponent>() : nullptr;
                    if (!IsValid(Identity) || !Identity->InstanceId.IsValid())
                        continue;
                    if (ActorsByIdentity.Contains(Identity->InstanceId))
                        return false;
                    ActorsByIdentity.Add(Identity->InstanceId, Candidate);
                }

                const FVector WorldDigCenter = GetComponentTransform().TransformPosition(LocalCenter);
                const float SafeDistance = RadiusCm + FMath::Max(0.0f, SupportSafetyMarginCm);
                const float SafeDistanceSquared = FMath::Square(SafeDistance);
                for (const FWorldStructuralNodeSnapshot& Node : GraphSnapshot)
                {
                    if (!Node.bGroundSupported)
                        continue;
                    AActor* SupportActor = ActorsByIdentity.FindRef(Node.ActorIdentityId);
                    if (!IsValid(SupportActor))
                        return false;
                    if (FVector::DistSquared(SupportActor->GetActorLocation(), WorldDigCenter) <= SafeDistanceSquared)
                        return false;
                }
            }
        }
    }

    // Keep only the bounded samples needed for a rollback if the edit makes no
    // material-volume change. This avoids copying the entire density grid.
    const int32 N = PointsPerAxis();
    const int32 MinX = FMath::Max(0, FMath::FloorToInt(Min.X / VoxelSizeCm) - 1);
    const int32 MaxX = FMath::Min(N - 1, FMath::CeilToInt(Max.X / VoxelSizeCm) + 1);
    const int32 MinY = FMath::Max(0, FMath::FloorToInt(Min.Y / VoxelSizeCm) - 1);
    const int32 MaxY = FMath::Min(N - 1, FMath::CeilToInt(Max.Y / VoxelSizeCm) + 1);
    const int32 MinZ = FMath::Max(0, FMath::FloorToInt(Min.Z / VoxelSizeCm) - 1);
    const int32 MaxZ = FMath::Min(N - 1, FMath::CeilToInt(Max.Z / VoxelSizeCm) + 1);
    TArray<int32> AffectedIndices;
    TArray<float> PreviousValues;
    AffectedIndices.Reserve((MaxX - MinX + 1) * (MaxY - MinY + 1) * (MaxZ - MinZ + 1));
    PreviousValues.Reserve((MaxX - MinX + 1) * (MaxY - MinY + 1) * (MaxZ - MinZ + 1));
    for (int32 Z = MinZ; Z <= MaxZ; ++Z)
        for (int32 Y = MinY; Y <= MaxY; ++Y)
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                const int32 Index = PointIndex(X, Y, Z);
                AffectedIndices.Add(Index);
                PreviousValues.Add(Density[Index]);
            }

    const int64 PreviousRevision = DensityRevision;
    const float SignedVolumeChange = ApplySphereEdit(LocalCenter, RadiusCm, false);
    if (SignedVolumeChange >= -KINDA_SMALL_NUMBER || DensityRevision == PreviousRevision)
    {
        for (int32 Index = 0; Index < AffectedIndices.Num(); ++Index)
            Density[AffectedIndices[Index]] = PreviousValues[Index];
        DensityRevision = PreviousRevision;
        return false;
    }

    // Rendering and collision are part of the edit's visible commit, not a
    // separate best-effort caller responsibility. If no surface can be rebuilt,
    // restore the bounded density samples and revision before rejecting.
    if (RebuildMesh() <= 0)
    {
        for (int32 Index = 0; Index < AffectedIndices.Num(); ++Index)
            Density[AffectedIndices[Index]] = PreviousValues[Index];
        DensityRevision = PreviousRevision;
        RebuildMesh();
        return false;
    }

    FVoxelTerrainDigReceipt Receipt;
    Receipt.OperationId = OperationId;
    Receipt.LocalCenter = LocalCenter;
    Receipt.RadiusCm = RadiusCm;
    Receipt.ToolStrength = ToolStrength;
    Receipt.ExpectedRevision = ExpectedRevision;
    Receipt.ResultingRevision = DensityRevision;
    Receipt.RemovedVolumeCubicMetres = -SignedVolumeChange;
    DigReceipts.Add(OperationId, Receipt);
    OutRemovedVolumeCubicMetres = Receipt.RemovedVolumeCubicMetres;
    OutResultingRevision = DensityRevision;
    return true;
}

bool UVoxelTerrainChunkComponent::ExportDensitySnapshot(
    TArray<float>& OutDensity, int64& OutRevision, TArray<FVoxelTerrainDigReceipt>& OutReceipts) const
{
    OutDensity.Reset();
    OutReceipts.Reset();
    OutRevision = 0;
    if (!HasInitializedDensity())
        return false;
    for (const float Value : Density)
        if (!FMath::IsFinite(Value))
            return false;

    OutDensity = Density;
    OutRevision = DensityRevision;
    DigReceipts.GenerateValueArray(OutReceipts);
    OutReceipts.Sort([](const FVoxelTerrainDigReceipt& A, const FVoxelTerrainDigReceipt& B)
    {
        return A.OperationId.ToString() < B.OperationId.ToString();
    });
    return true;
}

bool UVoxelTerrainChunkComponent::ValidateDensitySnapshot(
    int32 InCellsPerAxis, float InVoxelSizeCm, int64 InRevision,
    const TArray<float>& InDensity, const TArray<FVoxelTerrainDigReceipt>& InReceipts) const
{
    if (InCellsPerAxis < 4 || InCellsPerAxis > 128 ||
        !FMath::IsFinite(InVoxelSizeCm) || InVoxelSizeCm < 5.0f ||
        !FMath::IsFinite(MaxDigRadiusCm) || MaxDigRadiusCm <= 0.0f ||
        !FMath::IsFinite(SupportSafetyMarginCm) || SupportSafetyMarginCm < 0.0f ||
        !FMath::IsFinite(SoftSoilToolStrength) || SoftSoilToolStrength <= 0.0f ||
        !FMath::IsFinite(HardSoilToolStrength) || HardSoilToolStrength <= 0.0f ||
        !FMath::IsFinite(RockToolStrength) || RockToolStrength <= 0.0f ||
        InRevision <= 0 || MaxRememberedDigOperations <= 0 ||
        InReceipts.Num() > MaxRememberedDigOperations)
        return false;

    const int64 N = static_cast<int64>(InCellsPerAxis) + 1;
    if (InDensity.Num() != N * N * N)
        return false;
    for (const float Value : InDensity)
        if (!FMath::IsFinite(Value))
            return false;

    TSet<FGuid> SeenOperationIds;
    for (const FVoxelTerrainDigReceipt& Receipt : InReceipts)
    {
        if (!Receipt.OperationId.IsValid() || SeenOperationIds.Contains(Receipt.OperationId) ||
            Receipt.LocalCenter.ContainsNaN() || !FMath::IsFinite(Receipt.RadiusCm) ||
            Receipt.RadiusCm <= 0.0f || Receipt.RadiusCm > MaxDigRadiusCm ||
            !FMath::IsFinite(Receipt.ToolStrength) || Receipt.ToolStrength <= 0.0f ||
            Receipt.ExpectedRevision <= 0 || Receipt.ExpectedRevision == MAX_int64 ||
            Receipt.ResultingRevision != Receipt.ExpectedRevision + 1 ||
            Receipt.ResultingRevision > InRevision ||
            !FMath::IsFinite(Receipt.RemovedVolumeCubicMetres) ||
            Receipt.RemovedVolumeCubicMetres <= 0.0f)
            return false;

        const FVector R(Receipt.RadiusCm);
        const FVector Min = Receipt.LocalCenter - R;
        const FVector Max = Receipt.LocalCenter + R;
        const FBox ChunkBounds(FVector::ZeroVector, FVector(InCellsPerAxis * InVoxelSizeCm));
        if (!EditableBoundsLocal.IsValid() ||
            !EditableBoundsLocal.IsInsideOrOn(Min) || !EditableBoundsLocal.IsInsideOrOn(Max) ||
            !ChunkBounds.IsInsideOrOn(Min) || !ChunkBounds.IsInsideOrOn(Max))
            return false;
        SeenOperationIds.Add(Receipt.OperationId);
    }
    return true;
}

bool UVoxelTerrainChunkComponent::RestoreDensitySnapshot(
    int32 InCellsPerAxis, float InVoxelSizeCm, int64 InRevision,
    const TArray<float>& InDensity, const TArray<FVoxelTerrainDigReceipt>& InReceipts)
{
    if (!ValidateDensitySnapshot(InCellsPerAxis, InVoxelSizeCm, InRevision, InDensity, InReceipts))
        return false;

    // Stage the prior live state. A rejected restore must not leave density and
    // collision describing different worlds, particularly during transaction rollback.
    const int32 PreviousCellsPerAxis = CellsPerAxis;
    const float PreviousVoxelSizeCm = VoxelSizeCm;
    const int64 PreviousRevision = DensityRevision;
    const TArray<float> PreviousDensity = Density;
    const TMap<FGuid, FVoxelTerrainDigReceipt> PreviousReceipts = DigReceipts;

    CellsPerAxis = InCellsPerAxis;
    VoxelSizeCm = InVoxelSizeCm;
    Density = InDensity;
    DensityRevision = InRevision;
    DigReceipts.Reset();
    for (const FVoxelTerrainDigReceipt& Receipt : InReceipts)
        DigReceipts.Add(Receipt.OperationId, Receipt);

    bool bHasSolid = false;
    bool bHasAir = false;
    for (const float Value : Density)
    {
        bHasSolid |= Value < 0.0f;
        bHasAir |= Value >= 0.0f;
        if (bHasSolid && bHasAir)
            break;
    }

    const int32 TriangleCount = RebuildMesh();
    // A fully solid or fully empty chunk legitimately has no extracted surface.
    // Mixed density must produce a surface; otherwise keep the previous state.
    if (bHasSolid && bHasAir && TriangleCount <= 0)
    {
        CellsPerAxis = PreviousCellsPerAxis;
        VoxelSizeCm = PreviousVoxelSizeCm;
        DensityRevision = PreviousRevision;
        Density = PreviousDensity;
        DigReceipts = PreviousReceipts;
        RebuildMesh();
        return false;
    }
    return true;
}

float UVoxelTerrainChunkComponent::GetSolidVolumeCubicMetres() const
{
    int32 SolidPoints = 0;
    for (const float Value : Density)
        SolidPoints += Value < 0.0f ? 1 : 0;
    const double CellVolume = FMath::Pow(static_cast<double>(VoxelSizeCm) / 100.0, 3.0);
    return static_cast<float>(SolidPoints * CellVolume);
}

int32 UVoxelTerrainChunkComponent::RebuildMesh()
{
    ClearAllMeshSections();
    if (!HasInitializedDensity())
        return 0;

    UE::Geometry::FMarchingCubes Extractor;
    const double Extent = CellsPerAxis * VoxelSizeCm;
    Extractor.Bounds = UE::Geometry::TAxisAlignedBox3<double>(FVector3d::Zero(), FVector3d(Extent));
    Extractor.CubeSize = VoxelSizeCm;
    Extractor.IsoValue = 0.0;
    Extractor.bParallelCompute = true;
    Extractor.Implicit = [this](UE::Math::TVector<double> Position)
    {
        return static_cast<double>(SampleDensity(FVector(Position)));
    };
    Extractor.Generate();

    TArray<FVector> Vertices;
    Vertices.Reserve(Extractor.Vertices.Num());
    for (const FVector3d& Vertex : Extractor.Vertices)
        Vertices.Add(FVector(Vertex));

    TArray<int32> Triangles;
    Triangles.Reserve(Extractor.Triangles.Num() * 3);
    const double MinArea = FMath::Square(VoxelSizeCm * 0.001);
    for (const UE::Geometry::FIndex3i& Triangle : Extractor.Triangles)
    {
        // Samples exactly on the iso value make zero-area triangles; physics
        // cooking rejects such meshes, so they never reach collision.
        if (Triangle.A == Triangle.B || Triangle.B == Triangle.C || Triangle.A == Triangle.C)
            continue;
        const FVector3d& A = Extractor.Vertices[Triangle.A];
        const FVector3d& B = Extractor.Vertices[Triangle.B];
        const FVector3d& C = Extractor.Vertices[Triangle.C];
        if (((B - A).Cross(C - A)).SquaredLength() <= MinArea * MinArea)
            continue;
        // Wind triangles so faces point out of the solid (density increases outward).
        Triangles.Add(Triangle.A);
        Triangles.Add(Triangle.C);
        Triangles.Add(Triangle.B);
    }

    TArray<FVector> Normals;
    Normals.Reserve(Vertices.Num());
    const float Step = VoxelSizeCm * 0.5f;
    for (const FVector& Vertex : Vertices)
    {
        // Density gradient points from solid to air: that is the surface normal.
        const FVector Gradient(
            SampleDensity(Vertex + FVector(Step, 0, 0)) - SampleDensity(Vertex - FVector(Step, 0, 0)),
            SampleDensity(Vertex + FVector(0, Step, 0)) - SampleDensity(Vertex - FVector(0, Step, 0)),
            SampleDensity(Vertex + FVector(0, 0, Step)) - SampleDensity(Vertex - FVector(0, 0, Step)));
        Normals.Add(Gradient.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
    }

    CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, TArray<FVector2D>(),
        TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
    return Triangles.Num() / 3;
}
