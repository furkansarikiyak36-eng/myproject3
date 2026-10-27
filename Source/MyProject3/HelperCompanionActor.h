#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HelperCompanionActor.generated.h"

class ABuildPlanActor;
class AHarvestableTreeActor;
class ALogBuildPiece;
class ALogContainerActor;
class UStaticMeshComponent;

/** What the helper is told to do (Kelvin's log orders in Sons of the Forest). */
UENUM(BlueprintType)
enum class ECompanionOrder : uint8
{
    Follow UMETA(DisplayName = "Follow me"),
    CollectLogs UMETA(DisplayName = "Collect logs into storage"),
    PileLogsHere UMETA(DisplayName = "Bring logs here"),
    BuildPlan UMETA(DisplayName = "Finish the nearest plan"),
    MaintainWalls UMETA(DisplayName = "Sharpen and repair walls"),
    Stay UMETA(DisplayName = "Stay here")
};

/**
 * A helper who walks the ground and carries one log at a time: he follows the
 * player, gathers loose logs into the nearest storage that takes them, or brings
 * logs to the open parts of the nearest build plan and puts them in place.
 * While carried, a log is in transit in the item ledger.
 */
UCLASS()
class MYPROJECT3_API AHelperCompanionActor : public AActor
{
    GENERATED_BODY()

public:
    AHelperCompanionActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helper", meta = (ClampMin = "50.0"))
    float WalkSpeed = 340.0f;

    /** How far from himself he looks for logs, storage and plans. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helper", meta = (ClampMin = "100.0"))
    float SearchRadius = 3200.0f;

    /** When no loose log is left, he fells a tree for more (away from buildings and the player). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helper")
    bool bMayChopTrees = true;

    /** Logs below this height (in the creek) are left alone. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helper")
    float MinLogHeight = -1.0e6f;

    void SetOrder(ECompanionOrder NewOrder);
    ECompanionOrder GetOrder() const { return Order; }
    void CycleOrder();
    static FString GetOrderName(ECompanionOrder InOrder);
    const FString& GetStatus() const { return Status; }
    ALogBuildPiece* GetCarriedLog() const { return Carried.Get(); }
    int32 GetDeliveredCount() const { return Delivered; }
    int32 GetTreesFelled() const { return TreesFelled; }
    int32 GetWallsTended() const { return WallsTended; }
    const FVector& GetPilePoint() const { return PilePoint; }
    /** Where "Bring logs here" piles them; set from the player's position when the order is given. */
    void SetPilePoint(const FVector& Point) { PilePoint = Point; }

private:
    UPROPERTY(VisibleAnywhere, Category = "Helper")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, Category = "Helper")
    TObjectPtr<UStaticMeshComponent> Head;

    ECompanionOrder Order = ECompanionOrder::Follow;
    TWeakObjectPtr<ALogBuildPiece> Target;
    TWeakObjectPtr<ALogBuildPiece> Carried;
    TWeakObjectPtr<ABuildPlanActor> TargetPlan;
    int32 TargetSlot = INDEX_NONE;
    FGuid CompanionId;
    FString Status;
    int32 Delivered = 0;
    float ThinkTimer = 0.0f;
    FVector PilePoint = FVector::ZeroVector;
    int32 PileCount = 0;
    TWeakObjectPtr<AHarvestableTreeActor> TargetTree;
    TWeakObjectPtr<ALogBuildPiece> TargetStake;
    float WorkTimer = 0.0f;
    float SettleWait = 0.0f;
    int32 TreesFelled = 0;
    int32 WallsTended = 0;

    /** Walks towards Goal over the ground; true once within AcceptRadius. */
    bool MoveTowards(const FVector& Goal, float DeltaSeconds, float AcceptRadius);
    void FollowPlayer(float DeltaSeconds, float Distance);

    bool IsLooseLog(const ALogBuildPiece* Log) const;
    ALogBuildPiece* FindLooseLog(float MinLength, bool bNeedStorage, float AvoidPileRadius = 0.0f) const;
    AHarvestableTreeActor* FindTreeToChop() const;
    bool NeedsTending(const ALogBuildPiece* Log) const;
    ALogBuildPiece* FindStakeToTend() const;
    /** Walks to a tree and chops it; true while there is tree work to do. */
    bool TickChop(float DeltaSeconds);
    ALogContainerActor* FindStorageFor(const AActor* Piece) const;
    bool FindOpenPlanSlot(ABuildPlanActor*& OutPlan, int32& OutSlot) const;

    bool PickUp(ALogBuildPiece* Log);
    /** Puts the carried log back on the ground in front of him as a loose world log. */
    ALogBuildPiece* PutDown();
    bool PlaceInPlan(ABuildPlanActor* Plan, int32 Slot);

    void TickCollect(float DeltaSeconds);
    void TickBuild(float DeltaSeconds);
    void TickPile(float DeltaSeconds);
    void TickMaintain(float DeltaSeconds);
};
