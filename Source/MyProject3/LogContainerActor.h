#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LogContainerActor.generated.h"

class APawn;
class UStaticMeshComponent;
class UWorldObjectIdentityComponent;

UENUM(BlueprintType)
enum class ELogContainerKind : uint8
{
    SmallLogStorage UMETA(DisplayName = "Small Log Storage (8)"),
    LargeLogStorage UMETA(DisplayName = "Large Log Storage (24)"),
    FirewoodStorage UMETA(DisplayName = "Firewood Storage (32)"),
    BasicSled UMETA(DisplayName = "Log Sled (8 logs)"),
    AdvancedSled UMETA(DisplayName = "Advanced Sled (16, logs/stones/sticks)")
};

/**
 * A log rack, firewood store or pushable sled. Stored pieces stay real actors,
 * stacked visibly on the container and owned by it in the world-item ledger.
 */
UCLASS()
class MYPROJECT3_API ALogContainerActor : public AActor
{
    GENERATED_BODY()

public:
    ALogContainerActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
    TObjectPtr<UStaticMeshComponent> Base;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container|Identity")
    TObjectPtr<UWorldObjectIdentityComponent> Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
    ELogContainerKind Kind = ELogContainerKind::SmallLogStorage;

    /** Loose logs dropped this close to a storage are put away automatically. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
    float CollectRadius = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
    bool bAutoCollect = true;

    void ConfigureKind(ELogContainerKind NewKind);

    int32 GetCapacity() const;
    int32 GetStoredCount() const { return GetValidStoredCount(); }
    bool IsSled() const { return Kind == ELogContainerKind::BasicSled || Kind == ELogContainerKind::AdvancedSled; }
    bool IsTowed() const { return TowedBy.IsValid(); }
    FGuid GetStorageId() const { return StorageId; }

    bool CanAccept(const AActor* Piece, FString& OutReason) const;
    /** Puts a loose or released piece away; it must not be in anyone's hands. */
    bool TryStore(AActor* Piece);
    /** Takes the top piece out and leaves it world-owned at DropLocation. */
    AActor* TakeOne(const FVector& DropLocation);
    bool OwnsPiece(const AActor* Piece) const;

    /** Starts or stops a pawn pulling the sled behind it. */
    bool ToggleTow(APawn* Puller);

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPROPERTY()
    TArray<TObjectPtr<AActor>> Stored;

    /** Runtime storage ledger registration; StorageId is the authoritative owner ID for stored items. */
    FGuid StorageId;
    bool bStorageRegistered = false;
    float TimeToCollect = 0.0f;
    TWeakObjectPtr<APawn> TowedBy;
    float PullerBaseSpeed = 0.0f;

    FTransform GetSlotTransform(int32 Index, const AActor* Piece) const;
    int32 GetValidStoredCount() const;
    void CompactStoredItems();
    void ApplyTowSpeed() const;
    void CollectNearbyLogs();
};
