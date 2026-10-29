#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldStructuralGraphSubsystem.h"
#include "CarryLoadComponent.generated.h"

/** How many pieces of one material a carrier can hold and how dense they are. */
USTRUCT(BlueprintType)
struct FCarrySlotRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carry")
    EStructuralNodeKind Kind = EStructuralNodeKind::Wood;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carry", meta=(ClampMin="0"))
    int32 MaxCount = 1;

    /** Used with the mesh bounds volume to estimate mass; kg per cubic metre. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carry", meta=(ClampMin="1.0"))
    float DensityKgPerCubicMetre = 600.0f;
};

/**
 * One carry budget for every physical material (SOTF-C01). Logs held by the
 * log carry component register here; stones, sticks and extra logs can be
 * stowed directly. Total mass slows the owning character.
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class MYPROJECT3_API UCarryLoadComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCarryLoadComponent();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carry")
    TArray<FCarrySlotRule> SlotRules;

    /** Mass at which the speed multiplier reaches its minimum. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carry", meta=(ClampMin="1.0"))
    float MaxCarryMassKg = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carry", meta=(ClampMin="0.05", ClampMax="1.0"))
    float MinSpeedMultiplier = 0.35f;

    /** Bounding boxes overstate log and stone volume; this corrects the estimate. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carry", meta=(ClampMin="0.05", ClampMax="1.0"))
    float BoundsFillFactor = 0.7f;

    UFUNCTION(BlueprintPure, Category="Carry")
    bool CanCarryMore(AActor* Piece, FString& OutReason) const;

    /** Stows a loose world piece (log, stone or stick) through the ownership ledger. */
    UFUNCTION(BlueprintCallable, Category="Carry")
    bool TryTakePiece(AActor* Piece, FString& OutReason);

    /** Returns a stowed piece to the world at a location. */
    UFUNCTION(BlueprintCallable, Category="Carry")
    bool TryDropPiece(AActor* Piece, FVector WorldLocation);

    /** Called by the log carry component for the log in the player's hands. */
    bool RegisterCarried(AActor* Piece);
    void UnregisterCarried(AActor* Piece);

    UFUNCTION(BlueprintPure, Category="Carry")
    int32 GetCarriedCount(EStructuralNodeKind Kind) const;

    UFUNCTION(BlueprintPure, Category="Carry")
    TArray<AActor*> GetCarriedPieces() const;

    UFUNCTION(BlueprintPure, Category="Carry")
    float GetCarriedMassKg() const;

    UFUNCTION(BlueprintPure, Category="Carry")
    float GetSpeedMultiplier() const;

    /** Combines traversal effects with the carried-mass multiplier in one movement-speed write. */
    UFUNCTION(BlueprintCallable, Category="Carry")
    bool SetTraversalEnvironmentMultiplier(float Multiplier);

    UFUNCTION(BlueprintPure, Category="Carry")
    float GetTraversalEnvironmentMultiplier() const { return TraversalEnvironmentMultiplier; }

    UFUNCTION(BlueprintPure, Category="Carry")
    float EstimateMassKg(const AActor* Piece) const;

    FGuid GetCarrierId() const { return CarrierId; }

private:
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AActor>> CarriedPieces;

    UPROPERTY(Transient)
    FGuid CarrierId;

    UPROPERTY(Transient)
    float BaseWalkSpeed = -1.0f;

    UPROPERTY(Transient)
    float TraversalEnvironmentMultiplier = 1.0f;

    const FCarrySlotRule* FindRule(EStructuralNodeKind Kind) const;
    void ApplyMovementSpeed();
};
