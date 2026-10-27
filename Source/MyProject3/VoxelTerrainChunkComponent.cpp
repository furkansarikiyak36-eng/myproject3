#include "VoxelTerrainChunkComponent.h"

#include "Generators/MarchingCubes.h"
#include "PlacementKernelSubsystem.h"

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
    if (!FMath::IsFinite(GroundHeightCm) ||
        !FMath::IsFinite(VoxelSizeCm) || VoxelSizeCm <= 0.0f ||
        CellsPerAxis < 1 || CellsPerAxis > 128 || VoxelRevision == MAX_int64)
    {
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

    ++VoxelRevision;
    EditReceipts.Reset();
    EditReceiptOrder.Reset();
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

float UVoxelTerrainChunkComponent::ApplySphereEdit(FVector LocalCenter, float RadiusCm, bool bAdd)
{
    float DeltaVolume = 0.0f;
    int64 CommittedRevision = INDEX_NONE;
    FString Failure;
    if (!TryApplySphereEdit(FGuid::NewGuid(), VoxelRevision, LocalCenter, RadiusCm, bAdd,
        DeltaVolume, CommittedRevision, Failure))
    {
        UE_LOG(LogTemp, Warning, TEXT("Voxel sphere edit rejected: %s"), *Failure);
        return 0.0f;
    }
    return DeltaVolume;
}

bool UVoxelTerrainChunkComponent::TryApplySphereEdit(
    FGuid OperationId, int64 ExpectedRevision, FVector LocalCenter, float RadiusCm,
    bool bAdd, float& OutDeltaVolumeCubicMetres, int64& OutCommittedRevision,
    FString& OutFailureReason)
{
    OutDeltaVolumeCubicMetres = 0.0f;
    OutCommittedRevision = VoxelRevision;
    OutFailureReason.Reset();

    if (!OperationId.IsValid() || ExpectedRevision < 0 ||
        !FMath::IsFinite(RadiusCm) || RadiusCm <= 0.0f || LocalCenter.ContainsNaN() ||
        Density.IsEmpty() || !FMath::IsFinite(VoxelSizeCm) || VoxelSizeCm <= 0.0f ||
        VoxelRevision == MAX_int64)
    {
        OutFailureReason = TEXT("Invalid operation ID, expected revision, density grid or sphere geometry.");
        return false;
    }

    // Idempotent replay check precedes optimistic revision validation.
    if (const FVoxelEditOperationReceipt* Previous = EditReceipts.Find(OperationId))
    {
        const bool bSameRequest = Previous->ExpectedRevision == ExpectedRevision &&
            Previous->LocalCenter.Equals(LocalCenter, 0.001f) &&
            FMath::IsNearlyEqual(Previous->RadiusCm, RadiusCm, 0.001f) &&
            Previous->bAdd == bAdd;
        if (!bSameRequest)
        {
            OutFailureReason = TEXT("This voxel operation ID was already used for a different edit request.");
            return false;
        }

        OutDeltaVolumeCubicMetres = Previous->DeltaVolumeCubicMetres;
        OutCommittedRevision = Previous->CommittedRevision;
        return true;
    }

    if (ExpectedRevision != VoxelRevision)
    {
        OutFailureReason = FString::Printf(
            TEXT("Voxel revision conflict: expected %lld, current %lld."),
            static_cast<long long>(ExpectedRevision),
            static_cast<long long>(VoxelRevision));
        return false;
    }

    UPlacementKernelSubsystem* Kernel =
        GetWorld() ? GetWorld()->GetSubsystem<UPlacementKernelSubsystem>() : nullptr;
    FPlacementProfileDefinition Profile;
    if (!Kernel || !Kernel->IsCatalogReady() ||
        !Kernel->GetProfile(TEXT("voxel.multi_cell_edit"), Profile) ||
        Profile.OperationKindText != FString(TEXT("voxel_operation")) ||
        Profile.Geometry != TEXT("multi_cell_edit") || !Profile.bAtomicCommit)
    {
        OutFailureReason = TEXT("The global voxel.multi_cell_edit profile is missing or invalid.");
        return false;
    }

    const TArray<float> PreviousDensity = Density;
    const int64 PreviousRevision = VoxelRevision;
    const float Delta = ApplySphereEditInternal(LocalCenter, RadiusCm, bAdd);

    bool bValidDensity = FMath::IsFinite(Delta) && Density.Num() == PreviousDensity.Num();
    bool bDensityChanged = false;
    for (int32 Index = 0; bValidDensity && Index < Density.Num(); ++Index)
    {
        bValidDensity = FMath::IsFinite(Density[Index]);
        bDensityChanged |= Density[Index] != PreviousDensity[Index];
    }

    if (!bValidDensity)
    {
        Density = PreviousDensity;
        VoxelRevision = PreviousRevision;
        RebuildMesh();
        OutFailureReason = TEXT("The voxel edit produced invalid density state; the previous grid was restored.");
        return false;
    }

    // Render/collision data is derived from density and is rebuilt only after
    // every cell value has passed validation.
    RebuildMesh();
    if (bDensityChanged)
        ++VoxelRevision;

    FVoxelEditOperationReceipt Receipt;
    Receipt.ExpectedRevision = ExpectedRevision;
    Receipt.LocalCenter = LocalCenter;
    Receipt.RadiusCm = RadiusCm;
    Receipt.bAdd = bAdd;
    Receipt.DeltaVolumeCubicMetres = Delta;
    Receipt.CommittedRevision = VoxelRevision;
    RememberEditReceipt(OperationId, Receipt);

    OutDeltaVolumeCubicMetres = Delta;
    OutCommittedRevision = VoxelRevision;
    return true;
}

void UVoxelTerrainChunkComponent::RememberEditReceipt(
    FGuid OperationId, const FVoxelEditOperationReceipt& Receipt)
{
    constexpr int32 MaxReceipts = 512;
    if (!EditReceipts.Contains(OperationId))
        EditReceiptOrder.Add(OperationId);
    EditReceipts.Add(OperationId, Receipt);

    while (EditReceiptOrder.Num() > MaxReceipts)
    {
        const FGuid ExpiredId = EditReceiptOrder[0];
        EditReceiptOrder.RemoveAt(0);
        EditReceipts.Remove(ExpiredId);
    }
}

float UVoxelTerrainChunkComponent::ApplySphereEditInternal(
    FVector LocalCenter, float RadiusCm, bool bAdd)
{
    const float VolumeBefore = GetSolidVolumeCubicMetres();
    const int32 N = PointsPerAxis();
    const int32 MinX = FMath::Max(0, FMath::FloorToInt((LocalCenter.X - RadiusCm) / VoxelSizeCm) - 1);
    const int32 MaxX = FMath::Min(N - 1, FMath::CeilToInt((LocalCenter.X + RadiusCm) / VoxelSizeCm) + 1);
    const int32 MinY = FMath::Max(0, FMath::FloorToInt((LocalCenter.Y - RadiusCm) / VoxelSizeCm) - 1);
    const int32 MaxY = FMath::Min(N - 1, FMath::CeilToInt((LocalCenter.Y + RadiusCm) / VoxelSizeCm) + 1);
    const int32 MinZ = FMath::Max(0, FMath::FloorToInt((LocalCenter.Z - RadiusCm) / VoxelSizeCm) - 1);
    const int32 MaxZ = FMath::Min(N - 1, FMath::CeilToInt((LocalCenter.Z + RadiusCm) / VoxelSizeCm) + 1);

    for (int32 Z = MinZ; Z <= MaxZ; ++Z)
    {
        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                const FVector Point(X * VoxelSizeCm, Y * VoxelSizeCm, Z * VoxelSizeCm);
                const float Sphere = static_cast<float>(FVector::Dist(Point, LocalCenter)) - RadiusCm;
                float& Value = Density[PointIndex(X, Y, Z)];
                Value = AvoidIsoValue(bAdd ? FMath::Min(Value, Sphere) : FMath::Max(Value, -Sphere));
            }
        }
    }
    return GetSolidVolumeCubicMetres() - VolumeBefore;
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
    if (Density.IsEmpty())
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
