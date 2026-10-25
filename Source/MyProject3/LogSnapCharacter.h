#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LogSnapCharacter.generated.h"

class UCameraComponent;
class ULogCarryComponent;
class UInputComponent;

UCLASS(Blueprintable)
class MYPROJECT3_API ALogSnapCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ALogSnapCharacter();

    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Building")
    TObjectPtr<ULogCarryComponent> LogCarry;

protected:
    void MoveForward(float Value);
    void MoveBackward(float Value);
    void MoveRight(float Value);
    void MoveLeft(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void HandleJumpPressed();
    void HandleJumpReleased();
};
