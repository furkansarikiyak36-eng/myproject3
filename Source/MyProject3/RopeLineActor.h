#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StructuralPieceInterface.h"
#include "RopeLineActor.generated.h"

class UStaticMeshComponent;

/**
 * A rope tied between two points. Level ropes carry bridge boards; a sloped
 * rope is a zipline that slides logs from its upper end to its lower end.
 */
UCLASS()
class MYPROJECT3_API ARopeLineActor : public AActor, public IStructuralPieceInterface
{
    GENERATED_BODY()

public:
    ARopeLineActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope")
    TObjectPtr<UStaticMeshComponent> RopeMesh;

    /** Minimum slope (degrees) for the rope to carry loads downhill. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
    float MinZiplineSlopeDegrees = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
    float MaxRideSpeed = 1500.0f;

    void InitializeRope(const FVector& InStart, AActor* InStartAnchor, const FVector& InEnd, AActor* InEndAnchor);

    FVector GetStart() const { return Start; }
    FVector GetEnd() const { return End; }
    FVector GetUpperPoint() const { return Start.Z >= End.Z ? Start : End; }
    FVector GetLowerPoint() const { return Start.Z >= End.Z ? End : Start; }
    float GetSlopeDegrees() const;
    bool IsZipline() const { return GetSlopeDegrees() >= MinZiplineSlopeDegrees; }
    int32 GetRidingCount() const { return Riders.Num(); }

    /** Hangs a released piece on the upper end and lets it slide down. */
    bool TrySendAlong(AActor* Piece);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // IStructuralPieceInterface: a rope is a support other pieces can rest on.
    virtual EStructuralNodeKind GetStructuralKind() const override { return EStructuralNodeKind::Other; }
    virtual FGuid GetStructuralNodeId() const override { return NodeId; }
    virtual bool GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const override { return false; }
    virtual void GetStructuralAnchorIds(TArray<FName>& OutAnchorIds) const override { OutAnchorIds.Reset(); }
    virtual bool SetStructuralAnchorOccupied(FName AnchorId, bool bOccupied) override { return false; }
    virtual bool IsStructurallyPlaced() const override { return bTied; }
    virtual bool CanAttachAsCrossMaterialChild() const override { return false; }
    virtual void ApplyCrossMaterialAttachment(AActor* Parent) override {}
    virtual void ClearCrossMaterialAttachment() override {}
    virtual AActor* GetCrossMaterialParent() const override { return nullptr; }

private:
    struct FRider
    {
        TWeakObjectPtr<AActor> Piece;
        float Distance = 0.0f;
        float Speed = 0.0f;
        double AccumulatorSeconds = 0.0;
    };
    TArray<FRider> Riders;

    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    TWeakObjectPtr<AActor> StartAnchor;
    TWeakObjectPtr<AActor> EndAnchor;
    FGuid NodeId;
    bool bTied = false;

    void UpdateVisual();
};
