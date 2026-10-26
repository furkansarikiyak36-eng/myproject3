#include "LogSnapCharacter.h"

#include "LogBuildToolsComponent.h"
#include "PhysicalHealthComponent.h"
#include "ShelterSensorComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "LogCarryComponent.h"
#include "CarryLoadComponent.h"
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
    CarryLoad = CreateDefaultSubobject<UCarryLoadComponent>(TEXT("CarryLoad"));
    BuildTools = CreateDefaultSubobject<ULogBuildToolsComponent>(TEXT("BuildTools"));
    PhysicalHealth = CreateDefaultSubobject<UPhysicalHealthComponent>(TEXT("PhysicalHealth"));
    ShelterSensor = CreateDefaultSubobject<UShelterSensorComponent>(TEXT("ShelterSensor"));
}

void ALogSnapCharacter::BeginPlay()
{
    Super::BeginPlay();

    const FString Controls =
        TEXT("WASD move | Mouse look | E pick/place/use | Q throw | RMB placement mode | F1 all building keys");
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
    // WASD are digital keys; UE 5.8 only accepts 1D axes in BindAxisKey, so
    // movement keys are polled in Tick instead.
    PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ALogSnapCharacter::Turn);
    PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ALogSnapCharacter::LookUp);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ALogSnapCharacter::HandleJumpPressed);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ALogSnapCharacter::HandleJumpReleased);

    if (LogCarry)
    {
        LogCarry->BindDefaultInput(PlayerInputComponent);
    }
    if (BuildTools)
    {
        BuildTools->BindDefaultInput(PlayerInputComponent);
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
    // Mouse Y is positive when moving up, while positive pitch input looks down.
    AddControllerPitchInput(-Value);
}

void ALogSnapCharacter::HandleJumpPressed()
{
    Jump();
}

void ALogSnapCharacter::HandleJumpReleased()
{
    StopJumping();
}

void ALogSnapCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
        return;
    const auto KeyValue = [PlayerController](const FKey& Key)
    {
        return PlayerController->IsInputKeyDown(Key) ? 1.0f : 0.0f;
    };
    MoveForward(KeyValue(EKeys::W) - KeyValue(EKeys::S));
    MoveRight(KeyValue(EKeys::D) - KeyValue(EKeys::A));
}
