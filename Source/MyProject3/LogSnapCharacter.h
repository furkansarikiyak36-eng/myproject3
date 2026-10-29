#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LogSnapCharacter.generated.h"

class UCameraComponent;
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

public:
    /** Called by the active vertical-slice chopping handler on every mouse swing. */
    UFUNCTION(BlueprintCallable, Category = "Character|Axe")
    void PlayAxeSwing();

    /** Applies fall consequences once for an impact speed measured by the character movement loop. */
    UFUNCTION(BlueprintCallable, Category = "Survival")
    void ApplyFallImpact(float ImpactSpeedCmPerSecond);
};
