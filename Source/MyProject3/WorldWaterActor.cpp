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

    // River: a ribbon along the centre line, each point at its own water level.
    if (RiverPoints.Num() >= 2)
    {
        TArray<FVector> V;
        TArray<int32> T;
        TArray<FVector> N;
        TArray<FVector2D> UV;
        double Along = 0.0;
        for (int32 I = 0; I < RiverPoints.Num(); ++I)
        {
            const FVector P(RiverPoints[I].X, RiverPoints[I].Y, RiverPoints[I].Z);
            const int32 Prev = FMath::Max(0, I - 1), Next = FMath::Min(RiverPoints.Num() - 1, I + 1);
            FVector Dir(RiverPoints[Next].X - RiverPoints[Prev].X, RiverPoints[Next].Y - RiverPoints[Prev].Y, 0.0);
            Dir.Normalize();
            const FVector Side(-Dir.Y, Dir.X, 0.0);
            if (I > 0)
                Along += FVector::Dist2D(P, FVector(RiverPoints[I - 1].X, RiverPoints[I - 1].Y, 0.0));
            V.Add(P + Side * RiverPoints[I].W - Origin);
            V.Add(P - Side * RiverPoints[I].W - Origin);
            UV.Add(FVector2D(0.0, Along / 1000.0));
            UV.Add(FVector2D(1.0, Along / 1000.0));
            if (I > 0)
            {
                const int32 A = (I - 1) * 2, B = I * 2;
                T.Append({A, A + 1, B});
                T.Append({B, A + 1, B + 1});
            }
        }
        N.Init(FVector::UpVector, V.Num());
        // Make every triangle face up whatever the side order turned out to be.
        for (int32 I = 0; I + 2 < T.Num(); I += 3)
        {
            const FVector Normal = FVector::CrossProduct(V[T[I + 1]] - V[T[I]], V[T[I + 2]] - V[T[I]]);
            if (Normal.Z < 0.0)
                Swap(T[I + 1], T[I + 2]);
        }
        Surface->CreateMeshSection(Section, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        if (WaterMaterial)
            Surface->SetMaterial(Section, WaterMaterial);
    }
}
