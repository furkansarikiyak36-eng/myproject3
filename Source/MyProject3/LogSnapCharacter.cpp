#include "LogSnapCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "InputCoreTypes.h"
#include "LogCarryComponent.h"
#include "Math/RotationMatrix.h"

ALogSnapCharacter::ALogSnapCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    bUseControllerRotationPitch = true;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->bOrientRotationToMovement = false;
        Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
        Movement->MaxWalkSpeed = 420.0f;
        Movement->JumpZVelocity = 420.0f;
        Movement->AirControl = 0.25f;
    }

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;

    LogCarry = CreateDefaultSubobject<ULogCarryComponent>(TEXT("LogCarry"));
}

void ALogSnapCharacter::BeginPlay()
{
    Super::BeginPlay();

    const FString Controls =
        TEXT("LOG TEST | WASD move | Mouse look | E pick/place | G drop | R rotate | RMB placement mode | C cut mode | F process");
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1, 8.0f, FColor::White, Controls);
    }
}

void ALogSnapCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (!PlayerInputComponent)
    {
        return;
    }

    // Direct key-axis bindings keep the prototype independent of project-wide
    // legacy Axis Mappings. Enhanced Input can replace these later.
    PlayerInputComponent->BindAxisKey(EKeys::W, this, &ALogSnapCharacter::MoveForward);
    PlayerInputComponent->BindAxisKey(EKeys::S, this, &ALogSnapCharacter::MoveBackward);
    PlayerInputComponent->BindAxisKey(EKeys::D, this, &ALogSnapCharacter::MoveRight);
    PlayerInputComponent->BindAxisKey(EKeys::A, this, &ALogSnapCharacter::MoveLeft);
    PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ALogSnapCharacter::Turn);
    PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ALogSnapCharacter::LookUp);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, FName(TEXT("Jump")));
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, FName(TEXT("StopJumping")));

    if (LogCarry)
    {
        LogCarry->BindDefaultInput(PlayerInputComponent);
    }
}

void ALogSnapCharacter::MoveForward(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
    }
}

void ALogSnapCharacter::MoveBackward(float Value)
{
    MoveForward(-Value);
}

void ALogSnapCharacter::MoveRight(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
    }
}

void ALogSnapCharacter::MoveLeft(float Value)
{
    MoveRight(-Value);
}

void ALogSnapCharacter::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void ALogSnapCharacter::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}
