#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConstructionOpeningActor.generated.h"

class ALogBuildPiece;
class UBoxComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EConstructionOpeningKind : uint8
{
    Door UMETA(DisplayName = "Door Opening"),
    Window UMETA(DisplayName = "Window Opening"),
    Gate UMETA(DisplayName = "Defensive Wall Gate")
};

/**
 * A gap cut into a wall. Door and gate openings are filled with split boards;
 * once complete the panel swings on a hinge and can be locked from inside.
 */
UCLASS()
class MYPROJECT3_API AConstructionOpeningActor : public AActor
{
    GENERATED_BODY()

public:
    AConstructionOpeningActor();

    UPROPERTY(VisibleAnywhere, Category = "Opening")
    TObjectPtr<UBoxComponent> Frame;

    UPROPERTY(VisibleAnywhere, Category = "Opening")
    TObjectPtr<USceneComponent> Hinge;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Opening")
    EConstructionOpeningKind Kind = EConstructionOpeningKind::Door;

    /** Width along the wall and height, in cm. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Opening")
    float Width = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Opening")
    float Height = 200.0f;

    void InitializeOpening(EConstructionOpeningKind InKind, float InWidth, float InHeight);

    int32 GetRequiredBoards() const;
    int32 GetBoardsAdded() const { return BoardsAdded; }
    bool HasPanel() const { return bPanelBuilt; }
    bool IsOpen() const { return bOpen; }
    bool IsLocked() const { return bLocked; }

    /** Adds one split board held by the builder standing at BuilderLocation. */
    bool TryAddBoard(ALogBuildPiece* Board, const FVector& BuilderLocation, const FVector& AimPoint, FString& OutReason);
    bool ToggleOpen(const FVector& UserLocation, FString& OutReason);
    bool ToggleLock(const FVector& UserLocation, FString& OutReason);
    /** True when the panel or the opening frame belongs to this actor. */
    bool OwnsComponent(const UPrimitiveComponent* Component) const;

    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> PanelBoards;

    int32 BoardsAdded = 0;
    bool bPanelBuilt = false;
    bool bOpen = false;
    bool bLocked = false;
    /** +1/-1: which side of the opening the hinge is on (along the wall). */
    float HingeSide = 1.0f;
    /** +1/-1: the side the panel swings toward, away from the builder. */
    float SwingSide = 1.0f;
    /** +1/-1: the inside, where the builder stood when finishing. */
    float InsideSide = 1.0f;
    float CurrentYaw = 0.0f;

    float SideOf(const FVector& Location) const;
    void BuildPanel();
};
