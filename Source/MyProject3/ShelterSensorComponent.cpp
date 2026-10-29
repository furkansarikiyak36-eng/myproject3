#include "ShelterSensorComponent.h"

#include "ConstructionOpeningActor.h"
#include "StructuralPieceInterface.h"
#include "WorldSurvivalSubsystem.h"

#include "Engine/World.h"

UShelterSensorComponent::UShelterSensorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

namespace
{
    bool HitsBuiltPiece(const UWorld* World, const FVector& From, const FVector& To, const AActor* Ignore)
    {
        if (!World)
            return false;

        TArray<FHitResult> Hits;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(ShelterProbe), false, Ignore);
        if (!World->LineTraceMultiByChannel(Hits, From, To, ECC_Visibility, Params))
            return false;

        for (const FHitResult& Hit : Hits)
        {
            const AActor* Actor = Hit.GetActor();
            if (!IsValid(Actor))
                continue;

            // The frame of an empty door/gate is only an interaction target,
            // not a wall. A built, closed panel provides cover; an open panel
            // must not count. Continue through ignored hits so a real wall
            // behind the opening can still shelter the point.
            if (const AConstructionOpeningActor* Opening = Cast<AConstructionOpeningActor>(Actor))
            {
                if (Opening->HasPanel() && !Opening->IsOpen())
                    return true;
                continue;
            }

            // Only structurally placed pieces shelter: a loose log is not a wall or roof.
            if (const IStructuralPieceInterface* Piece = Cast<IStructuralPieceInterface>(Actor))
            {
                if (Piece->IsStructurallyPlaced())
                    return true;
            }
        }
        return false;
    }
}

void UShelterSensorComponent::MeasureAt(const UWorld* World, const FVector& Location, const AActor* Ignore,
    float& OutRoof, float& OutWalls)
{
    OutRoof = 0.0f;
    OutWalls = 0.0f;
    if (!World)
        return;
    // Nine rays up in a narrow cone: gaps between boards let some weather in.
    int32 RoofHits = HitsBuiltPiece(World, Location, Location + FVector(0, 0, 1500), Ignore) ? 1 : 0;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Angle = Index * PI / 4.0f;
        const FVector Direction = FVector(FMath::Cos(Angle) * 0.35f, FMath::Sin(Angle) * 0.35f, 1.0f).GetSafeNormal();
        RoofHits += HitsBuiltPiece(World, Location, Location + Direction * 1500.0f, Ignore) ? 1 : 0;
    }
    int32 WallHits = 0;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Angle = Index * PI / 4.0f;
        WallHits += HitsBuiltPiece(World, Location, Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * 600.0f,
            Ignore) ? 1 : 0;
    }
    OutRoof = RoofHits / 9.0f;
    OutWalls = WallHits / 8.0f;
}

void UShelterSensorComponent::MeasureNow()
{
    const AActor* Owner = GetOwner();
    if (!Owner)
        return;
    FVector Eyes = Owner->GetActorLocation() + FVector(0, 0, 50);
    MeasureAt(GetWorld(), Eyes, Owner, RoofCover, WallCover);
    Protection = FMath::Clamp(0.75f * RoofCover + 0.25f * WallCover, 0.0f, 1.0f);

    UWorldSurvivalSubsystem* Survival = GetWorld() ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    FWorldSurvivalProfile Profile;
    if (Survival && SurvivalEntityId.IsValid() && Survival->GetProfile(SurvivalEntityId, Profile) &&
        !FMath::IsNearlyEqual(Profile.ShelterProtection, Protection, 0.01f))
    {
        Survival->UpdateProtection(SurvivalEntityId, Profile.ClothingInsulation, Protection, Profile.Revision);
    }
}

void UShelterSensorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    TimeToUpdate -= DeltaTime;
    if (TimeToUpdate <= 0.0f)
    {
        TimeToUpdate = UpdateInterval;
        MeasureNow();
    }
}
