#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LogSnapCharacter.generated.h"

class UCameraComponent;
class ULogCarryComponent;
class UCarryLoadComponent;
class ULogBuildToolsComponent;
class UPhysicalHealthComponent;
class UShelterSensorComponent;
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

protected:
    /** Camera offset that eases out height jumps from steps, logs and rocks. */
    float StepSmoothingOffset = 0.0f;
    float LastGroundZ = 0.0f;
    bool bHasLastGroundZ = false;
    float CameraBaseHeight = 64.0f;

    void MoveForward(float Value);
    void MoveBackward(float Value);
    void MoveRight(float Value);
    void MoveLeft(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void HandleJumpPressed();
    void HandleJumpReleased();
};
