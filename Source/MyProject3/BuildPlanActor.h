#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildPlanActor.generated.h"

class ALogBuildPiece;
class UStaticMeshComponent;
class AHarvestableTreeActor;

UENUM(BlueprintType)
enum class EBuildPlanKind : uint8
{
    LogWall UMETA(DisplayName = "Log Wall (3 m)"),
    LeanToShelter UMETA(DisplayName = "Lean-to Shelter"),
    Watchtower UMETA(DisplayName = "Watchtower"),
    SmallLogStorage UMETA(DisplayName = "Small Log Storage"),
    LargeLogStorage UMETA(DisplayName = "Large Log Storage"),
    LogSled UMETA(DisplayName = "Log Sled"),
    DefenseWall UMETA(DisplayName = "Defensive Stake Wall"),
    HuntingShelter UMETA(DisplayName = "Hunting Shelter"),
    SmallPlatform UMETA(DisplayName = "Small Platform"),
    Bench UMETA(DisplayName = "Bench"),
    WeaponRack UMETA(DisplayName = "Weapon Rack"),
    SmallLogCabin UMETA(DisplayName = "Small Log Cabin"),
    TreePlatform UMETA(DisplayName = "Tree Platform"),
    TreeShelter UMETA(DisplayName = "Tree Shelter"),
    Chair UMETA(DisplayName = "Chair"),
    Couch UMETA(DisplayName = "Couch"),
    Shelf UMETA(DisplayName = "Shelf"),
    RabbitHutch UMETA(DisplayName = "Rabbit Hutch"),
    CornerRamp UMETA(DisplayName = "Corner Ramp")
};

/** One log position in a plan, in the plan's local space. */
USTRUCT()
struct FBuildPlanSlot
{
    GENERATED_BODY()

    /** Centre of the log and the direction of its length (local X of this transform). */
    UPROPERTY()
    FTransform Local;

    UPROPERTY()
    float LengthCm = 300.0f;

    /** Slot whose log this one rests on; INDEX_NONE stands on the ground. */
    UPROPERTY()
    int32 SupportSlot = INDEX_NONE;

    UPROPERTY()
    bool bFilled = false;

    TWeakObjectPtr<ALogBuildPiece> Piece;
};

/**
 * A ready-made building plan shown as ghost logs. Carried logs are dropped
 * into the slots (cut to length if needed); the finished pieces are ordinary
 * placed logs, so the player can extend the structure freely.
 */
UCLASS()
class MYPROJECT3_API ABuildPlanActor : public AActor
{
    GENERATED_BODY()

public:
    ABuildPlanActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plan")
    EBuildPlanKind Kind = EBuildPlanKind::LogWall;

    void InitializePlan(EBuildPlanKind InKind);

    /** Length of a drawn defensive wall line (cm); one stake every 40 cm. Set before InitializePlan. */
    float LineLengthCm = 0.0f;

    /** Plans built in a tree: ground-level parts rest on this trunk and fall with it. */
    TWeakObjectPtr<AHarvestableTreeActor> AnchorTree;

    /** Tree platform and tree shelter need a standing tree. */
    static bool NeedsTree(EBuildPlanKind InKind)
    {
        return InKind == EBuildPlanKind::TreePlatform || InKind == EBuildPlanKind::TreeShelter;
    }

    /** The last plan kind, for cycling through them. */
    static constexpr EBuildPlanKind LastKind = EBuildPlanKind::CornerRamp;

    int32 GetSlotCount() const { return Slots.Num(); }
    int32 GetFilledCount() const;
    bool IsComplete() const { return GetFilledCount() == Slots.Num(); }
    const FBuildPlanSlot& GetSlot(int32 Index) const { return Slots[Index]; }

    FTransform GetSlotWorld(int32 Index) const;
    /** The slot whose ghost is Component, or the nearest open slot to WorldPoint. */
    int32 FindSlot(const UPrimitiveComponent* Component, const FVector& WorldPoint) const;
    bool CanFillSlot(int32 Index, FString& OutReason) const;
    ALogBuildPiece* GetSupportPiece(int32 Index) const;
    void MarkFilled(int32 Index, ALogBuildPiece* Piece);

    static FString GetPlanName(EBuildPlanKind Kind);

private:
    UPROPERTY()
    TArray<FBuildPlanSlot> Slots;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Ghosts;

    void AddSlot(const FVector& Centre, const FRotator& Direction, float Length, int32 Support);
    void Complete();
};
