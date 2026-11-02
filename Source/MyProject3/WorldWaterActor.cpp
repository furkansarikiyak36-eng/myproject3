#include "WorldWaterActor.h"

#include "ProceduralMeshComponent.h"

AWorldWaterActor::AWorldWaterActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Surface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surface"));
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->SetCastShadow(false);
    RootComponent = Surface;
}

void AWorldWaterActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildSurfaces();
}

void AWorldWaterActor::BuildSurfaces()
{
    Surface->ClearAllMeshSections();
    if (!bDrawSurfaces)
        return;
    const FVector Origin = GetActorLocation();
    int32 Section = 0;

    // Lake: a flat disc at the water level.
    if (Lake.W > 0.0f)
    {
        constexpr int32 Segments = 96;
        TArray<FVector> V;
        TArray<int32> T;
        TArray<FVector> N;
        TArray<FVector2D> UV;
        const FVector Centre(Lake.X, Lake.Y, Lake.Z);
        V.Add(Centre - Origin);
        UV.Add(FVector2D(0.5, 0.5));
        for (int32 I = 0; I < Segments; ++I)
        {
            const double A = 2.0 * PI * I / Segments;
            V.Add(Centre + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * Lake.W - Origin);
            UV.Add(FVector2D(0.5 + 0.5 * FMath::Cos(A), 0.5 + 0.5 * FMath::Sin(A)));
        }
        for (int32 I = 0; I < Segments; ++I)
        {
            // Wound to face up.
            T.Add(0);
            T.Add(1 + (I + 1) % Segments);
            T.Add(1 + I);
        }
        N.Init(FVector::UpVector, V.Num());
        Surface->CreateMeshSection(Section, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        if (WaterMaterial)
            Surface->SetMaterial(Section, WaterMaterial);
        ++Section;
    }

    // Lakes from the flow map: one quad per rectangle at its water level.
    if (LakeRects.Num() > 0 && LakeRects.Num() == LakeRectZ.Num())
    {
        TArray<FVector> V;
        TArray<int32> T;
        TArray<FVector2D> UV;
        for (int32 I = 0; I < LakeRects.Num(); ++I)
        {
            const FVector4f& R = LakeRects[I];
            const int32 B = V.Num();
            V.Add(FVector(R.X, R.Y, LakeRectZ[I]) - Origin);
            V.Add(FVector(R.Z, R.Y, LakeRectZ[I]) - Origin);
            V.Add(FVector(R.Z, R.W, LakeRectZ[I]) - Origin);
            V.Add(FVector(R.X, R.W, LakeRectZ[I]) - Origin);
            for (int32 K = 0; K < 4; ++K)
                UV.Add(FVector2D(V[B + K].X / 1000.0, V[B + K].Y / 1000.0));
            T.Append({B, B + 2, B + 1, B, B + 3, B + 2});
        }
        TArray<FVector> N;
        N.Init(FVector::UpVector, V.Num());
        Surface->CreateMeshSection(Section, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        if (WaterMaterial)
            Surface->SetMaterial(Section, WaterMaterial);
        ++Section;
    }

    // Rivers: a ribbon along each centre line, each point at its own water level.
    TArray<int32> Starts = RiverStarts;
    if (Starts.Num() == 0 && RiverPoints.Num() > 0)
        Starts.Add(0);
    TArray<FVector> V;
    TArray<int32> T;
    TArray<FVector2D> UV;
    for (int32 S = 0; S < Starts.Num(); ++S)
    {
        const int32 First = Starts[S];
        const int32 Last = S + 1 < Starts.Num() ? Starts[S + 1] - 1 : RiverPoints.Num() - 1;
        if (Last - First < 1)
            continue;
        double Along = 0.0;
        for (int32 I = First; I <= Last; ++I)
        {
            const FVector P(RiverPoints[I].X, RiverPoints[I].Y, RiverPoints[I].Z);
            const int32 Prev = FMath::Max(First, I - 1), Next = FMath::Min(Last, I + 1);
            FVector Dir(RiverPoints[Next].X - RiverPoints[Prev].X, RiverPoints[Next].Y - RiverPoints[Prev].Y, 0.0);
            Dir.Normalize();
            const FVector Side(-Dir.Y, Dir.X, 0.0);
            if (I > First)
                Along += FVector::Dist2D(P, FVector(RiverPoints[I - 1].X, RiverPoints[I - 1].Y, 0.0));
            const int32 B = V.Num();
            V.Add(P + Side * RiverPoints[I].W - Origin);
            V.Add(P - Side * RiverPoints[I].W - Origin);
            UV.Add(FVector2D(0.0, Along / 1000.0));
            UV.Add(FVector2D(1.0, Along / 1000.0));
            if (I > First)
                T.Append({B - 2, B - 1, B, B, B - 1, B + 1});
        }
    }
    if (V.Num() > 0)
    {
        TArray<FVector> N;
        N.Init(FVector::UpVector, V.Num());
        Surface->CreateMeshSection(Section, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        if (WaterMaterial)
            Surface->SetMaterial(Section, WaterMaterial);
    }
}
