#include "ConstructionOpeningActor.h"

#include "ConstructionHelpers.h"
#include "LogBuildPiece.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

using namespace MyProject3Construction;

AConstructionOpeningActor::AConstructionOpeningActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Frame = CreateDefaultSubobject<UBoxComponent>(TEXT("Frame"));
    SetRootComponent(Frame);
    // Only the crosshair sees the empty opening; people walk through it.
    Frame->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Frame->SetCollisionResponseToAllChannels(ECR_Ignore);
    Frame->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
    Hinge->SetupAttachment(Frame);
}

void AConstructionOpeningActor::InitializeOpening(EConstructionOpeningKind InKind, float InWidth, float InHeight)
{
    Kind = InKind;
    Width = InWidth;
    Height = InHeight;
    // Local X runs along the wall, Y through it, Z up.
    Frame->SetBoxExtent(FVector(0.5f * Width, 20.0f, 0.5f * Height));
    if (Kind == EConstructionOpeningKind::Window)
        Frame->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

int32 AConstructionOpeningActor::GetRequiredBoards() const
{
    return Kind == EConstructionOpeningKind::Window ? 0 : FMath::Max(2, FMath::CeilToInt(Width / 25.0f));
}

float AConstructionOpeningActor::SideOf(const FVector& Location) const
{
    return FVector::DotProduct(Location - GetActorLocation(), GetActorRightVector()) >= 0.0f ? 1.0f : -1.0f;
}

bool AConstructionOpeningActor::TryAddBoard(ALogBuildPiece* Board, const FVector& BuilderLocation,
    const FVector& AimPoint, FString& OutReason)
{
    if (Kind == EConstructionOpeningKind::Window)
    {
        OutReason = TEXT("A window opening stays open.");
        return false;
    }
    if (bPanelBuilt)
    {
        OutReason = TEXT("The door is already built.");
        return false;
    }
    if (!IsSplitBoard(Board))
    {
        OutReason = TEXT("Doors and gates need split boards (plank, half or quarter log).");
        return false;
    }
    if (BoardsAdded == 0)
    {
        // The first board decides the hinge: the side of the opening it went in.
        HingeSide = FVector::DotProduct(AimPoint - GetActorLocation(), GetActorForwardVector()) >= 0.0f ? 1.0f : -1.0f;
    }
    if (!ConsumePiece(Board))
    {
        OutReason = TEXT("The board could not be used.");
        return false;
    }
    ++BoardsAdded;
    if (BoardsAdded >= GetRequiredBoards())
    {
        InsideSide = SideOf(BuilderLocation);
        SwingSide = -InsideSide; // Opens outward, away from the builder.
        BuildPanel();
    }
    return true;
}

void AConstructionOpeningActor::BuildPanel()
{
    bPanelBuilt = true;
    Frame->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Hinge->SetRelativeLocation(FVector(HingeSide * 0.5f * Width, 0.0f, 0.0f));
    UStaticMesh* Cube = LoadEngineShape(TEXT("Cube"));
    UMaterialInterface* Wood = MakeTint(this, FLinearColor(0.42f, 0.28f, 0.15f));
    const int32 Count = GetRequiredBoards();
    const float BoardWidth = Width / Count;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UStaticMeshComponent* BoardMesh = NewObject<UStaticMeshComponent>(this);
        BoardMesh->SetupAttachment(Hinge);
        BoardMesh->RegisterComponent();
        BoardMesh->SetStaticMesh(Cube);
        BoardMesh->SetMaterial(0, Wood);
        BoardMesh->SetCollisionProfileName(TEXT("BlockAll"));
        // Boards run from the hinge across the opening.
        const float X = -HingeSide * (BoardWidth * (Index + 0.5f));
        BoardMesh->SetRelativeLocation(FVector(X, 0.0f, 0.0f));
        BoardMesh->SetRelativeScale3D(FVector((BoardWidth - 1.5f) / 100.0f, 0.06f, (Height - 4.0f) / 100.0f));
        PanelBoards.Add(BoardMesh);
    }
}

bool AConstructionOpeningActor::ToggleOpen(const FVector& UserLocation, FString& OutReason)
{
    if (!bPanelBuilt)
    {
        OutReason = FString::Printf(TEXT("Add split boards to build it (%d/%d)."), BoardsAdded, GetRequiredBoards());
        return false;
    }
    if (bLocked)
    {
        OutReason = TEXT("Locked.");
        return false;
    }
    bOpen = !bOpen;
    return true;
}

bool AConstructionOpeningActor::ToggleLock(const FVector& UserLocation, FString& OutReason)
{
    if (!bPanelBuilt)
    {
        OutReason = TEXT("There is no door to lock yet.");
        return false;
    }
    if (bOpen)
    {
        OutReason = TEXT("Close it first.");
        return false;
    }
    if (SideOf(UserLocation) != InsideSide)
    {
        OutReason = TEXT("The bar is on the other side.");
        return false;
    }
    bLocked = !bLocked;
    return true;
}

bool AConstructionOpeningActor::OwnsComponent(const UPrimitiveComponent* Component) const
{
    return Component && (Component == Frame || PanelBoards.Contains(Component));
}

void AConstructionOpeningActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPanelBuilt)
        return;
    const float Target = bOpen ? 95.0f * SwingSide * HingeSide : 0.0f;
    CurrentYaw = FMath::FInterpTo(CurrentYaw, Target, DeltaSeconds, 8.0f);
    Hinge->SetRelativeRotation(FRotator(0.0f, CurrentYaw, 0.0f));
}
