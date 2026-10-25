#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LogBuildingTypes.h"
#include "LogCarryComponent.generated.h"

class ALogBuildPiece;
class UInputComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FLogSnapStateChanged, bool, bHasValidSnap);

UCLASS(ClassGroup = (Gameplay), BlueprintType, Blueprintable,
    meta = (BlueprintSpawnableComponent))
class MYPROJECT3_API ULogCarryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULogCarryComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Trace",
        meta = (ClampMin = "50.0"))
    float PickupRange = 350.0f;

    /** Impulse used by the optional loose-log push/roll interaction. Tune per project physics scale. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics",
        meta = (ClampMin = "0.0"))
    float PushImpulseStrength = 25000.0f;

    /** Hard cap prevents accidental Blueprint values from launching a log at extreme speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Physics",
        meta = (ClampMin = "0.0"))
    float MaxPushImpulseMagnitude = 50000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Trace",
        meta = (ClampMin = "100.0"))
    float PlacementTraceDistance = 4500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement",
        meta = (ClampMin = "20.0"))
    float SnapSearchRadius = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement",
        meta = (ClampMin = "50.0"))
    float CarryDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    bool bAllowFreePlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    bool bRequireSurfaceForFreePlacement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    ELogPlacementMode PlacementMode = ELogPlacementMode::Horizontal;

    UPROPERTY(BlueprintAssignable, Category = "Log Carry|Events")
    FLogSnapStateChanged OnSnapStateChanged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    TObjectPtr<ALogBuildPiece> HeldPiece;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    TObjectPtr<ALogBuildPiece> CurrentSnapTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    int32 CurrentSourceSnapPointIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    int32 CurrentTargetSnapPointIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    bool bHasValidSnap = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Carry|State")
    bool bHasPlacementSurface = false;

    // Can be called from a Character/Controller Blueprint with its InputComponent.
    // If using Enhanced Input, bind these same functions to your Input Actions instead.
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Input")
    void BindDefaultInput(UInputComponent* InputComponent);

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void Interact();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool TryPickupTarget();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool PlaceHeldPiece();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void DropHeldPiece();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void RotateHeldPiece();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void CyclePlacementMode();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    void CycleCutAction();

    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool ProcessAimedPiece();

    /** Applies a bounded off-centre impulse to an aimed, loose, world-owned physics log. */
    UFUNCTION(BlueprintCallable, Category = "Log Carry|Actions")
    bool TryPushOrRollAimedLog();

    UFUNCTION(BlueprintPure, Category = "Log Carry|State")
    ELogCutAction GetSelectedCutAction() const { return SelectedCutAction; }

    UFUNCTION(BlueprintPure, Category = "Log Carry|State")
    bool IsCarryingLog() const { return HeldPiece.Get() != nullptr; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Carry|Placement")
    float SnapRefreshInterval = 0.06f;

private:
    float TimeUntilSnapRefresh = 0.0f;
    FGuid CarryOwnerId;
    float HeldYawOffset = 0.0f;
    ELogCutAction SelectedCutAction = ELogCutAction::SplitHalf;
    FTransform CurrentSnapActorTransform = FTransform::Identity;

    bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
    bool TraceFromView(float Distance, FHitResult& OutHit) const;
    void UpdateHeldPreview();
    void FindBestSnapCandidate();
    void ClearSnapCandidate();
    void SetSnapCandidate(ALogBuildPiece* Target, int32 SourceIndex,
        int32 TargetIndex, const FTransform& DesiredActorTransform);
    FRotator MakePlacementRotation(float ViewYaw) const;
    void ProcessAimedPieceInput();
    void PushOrRollAimedLogInput();
};
