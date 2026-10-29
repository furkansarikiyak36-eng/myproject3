#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LogSnapCharacter.generated.h"

class UCameraComponent;
class AStoneBuildingPiece;
class USceneComponent;
class UStaticMeshComponent;
class ULogCarryComponent;
class UInventoryCraftingComponent;
class UCarryLoadComponent;
class ULogBuildToolsComponent;
class UPhysicalHealthComponent;
class UShelterSensorComponent;
class UTerrainDigToolComponent;
class UInputComponent;

UCLASS(Blueprintable)
class MYPROJECT3_API ALogSnapCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ALogSnapCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    /** Low-poly body blockout so the first-person pawn has a visible silhouette/shadow. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Blockout")
    TObjectPtr<UStaticMeshComponent> BodyTorso;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Blockout")
    TObjectPtr<UStaticMeshComponent> BodyHead;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Blockout")
    TObjectPtr<UStaticMeshComponent> BodyLeftArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Blockout")
    TObjectPtr<UStaticMeshComponent> BodyRightArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Blockout")
    TObjectPtr<UStaticMeshComponent> BodyLeftLeg;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Blockout")
    TObjectPtr<UStaticMeshComponent> BodyRightLeg;

    /** Camera-relative arm, hand, and primitive axe for readable first-person chopping. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Axe")
    TObjectPtr<UStaticMeshComponent> FirstPersonForearm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Axe")
    TObjectPtr<USceneComponent> AxePivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Axe")
    TObjectPtr<UStaticMeshComponent> AxeHand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Axe")
    TObjectPtr<UStaticMeshComponent> AxeHandle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Axe")
    TObjectPtr<UStaticMeshComponent> AxeHead;

    /** Canonical item inventory for survival consumables, tools and material items. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<UInventoryCraftingComponent> Inventory;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Building")
    TObjectPtr<ULogCarryComponent> LogCarry;

    /** Shared carry budget for logs, stones and sticks; mass slows walking. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Building")
    TObjectPtr<UCarryLoadComponent> CarryLoad;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Building")
    TObjectPtr<ULogBuildToolsComponent> BuildTools;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survival")
    TObjectPtr<UPhysicalHealthComponent> PhysicalHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survival")
    TObjectPtr<UShelterSensorComponent> ShelterSensor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
    TObjectPtr<UTerrainDigToolComponent> TerrainDigTool;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character|Traversal", meta=(ClampMin="50.0"))
    float RopeInteractionRangeCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character|Traversal", meta=(ClampMin="25.0"))
    float RopeTraversalSpeedCmPerSecond = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character|Traversal", meta=(ClampMin="1.0"))
    float RopeBaseTravelerWeightKg = 75.0f;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Character|Traversal")
    TWeakObjectPtr<AStoneBuildingPiece> ActiveRopeEndpoint;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Character|Traversal")
    TWeakObjectPtr<AStoneBuildingPiece> ActiveRopeOtherEnd;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Character|Traversal")
    FGuid ActiveRopeTraversalId;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Character|Traversal")
    bool bRopeTraversalActive = false;

protected:
    /** Camera offset that eases out height jumps from steps, logs and rocks. */
    float StepSmoothingOffset = 0.0f;
    float LastGroundZ = 0.0f;
    bool bHasLastGroundZ = false;
    float CameraBaseHeight = 64.0f;
    /** Cached base speed so fatigue/sprint modifiers can be recomputed without compounding. */
    float BaseWalkSpeed = 420.0f;
    float SurvivalActivityAccumulator = 0.0f;
    bool bWasFalling = false;
    float PeakFallSpeedCmPerSecond = 0.0f;

    float AxeSwingElapsed = 0.0f;
    float AxeSwingDuration = 0.30f;
    bool bAxeSwingActive = false;
    FRotator AxePivotBaseRotation = FRotator::ZeroRotator;

    void ApplyBlockoutMaterials();
    void MoveForward(float Value);
    void MoveBackward(float Value);
    void MoveRight(float Value);
    void MoveLeft(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void HandleJumpPressed();
    void HandleJumpReleased();
    void HandleRopeTraversalPressed();
    void EndRopeTraversal(bool bCancelTicket);

    FVector RopeTraversalStartLocation = FVector::ZeroVector;
    FVector RopeTraversalEndLocation = FVector::ZeroVector;
    float RopeTraversalElapsedSeconds = 0.0f;
    float RopeTraversalDurationSeconds = 0.0f;
    uint8 RopePreviousMovementMode = 1;
    uint8 RopePreviousCustomMovementMode = 0;

public:
    /** Called by the active vertical-slice chopping handler on every mouse swing. */
    UFUNCTION(BlueprintCallable, Category = "Character|Axe")
    void PlayAxeSwing();

    /** Applies fall consequences once for an impact speed measured by the character movement loop. */
    UFUNCTION(BlueprintCallable, Category = "Survival")
    void ApplyFallImpact(float ImpactSpeedCmPerSecond);
};
