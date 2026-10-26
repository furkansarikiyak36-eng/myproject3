#include "LogSnapCharacter.h"

#include "LogBuildToolsComponent.h"
#include "PhysicalHealthComponent.h"
#include "ShelterSensorComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
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
        // A rolling or turning log under the feet must not spin the view or
        // carry the player off; pushes against loose logs stay gentle.
        Movement->bImpartBaseAngularVelocity = false;
        Movement->bIgnoreBaseRotation = true;
        Movement->bImpartBaseVelocityX = false;
        Movement->bImpartBaseVelocityY = false;
        Movement->bImpartBaseVelocityZ = false;
        Movement->bEnablePhysicsInteraction = true;
        // Loose sticks and logs feel the body, but are nudged, not shoved.
        Movement->PushForceFactor = 6000.0f;
        Movement->bPushForceScaledToMass = false;
        Movement->RepulsionForce = 0.0f;
        Movement->TouchForceFactor = 0.0f;
        Movement->StandingDownwardForceScale = 0.1f;
        Movement->InitialPushForceFactor = 5.0f;
        Movement->MaxStepHeight = 40.0f;
        // Do not balance on the very edge of a round log.
        Movement->PerchRadiusThreshold = 12.0f;
        Movement->PerchAdditionalHeight = 20.0f;
    }

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;

    // Cheap engine-shape blockout assets: no imported character/weapon package required.
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    auto MakeTint = [this, ShapeMaterial](const FLinearColor& Color) -> UMaterialInterface*
    {
        if (!ShapeMaterial)
            return UMaterial::GetDefaultMaterial(MD_Surface);
        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
        if (Dynamic)
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
        }
        return Dynamic ? static_cast<UMaterialInterface*>(Dynamic) : ShapeMaterial;
    };

    const FLinearColor ClothColor(0.15f, 0.19f, 0.14f);
    const FLinearColor SkinColor(0.48f, 0.29f, 0.18f);
    const FLinearColor WoodColor(0.20f, 0.095f, 0.035f);
    const FLinearColor MetalColor(0.24f, 0.28f, 0.30f);
    UMaterialInterface* ClothMaterial = MakeTint(ClothColor);
    UMaterialInterface* SkinMaterial = MakeTint(SkinColor);
    UMaterialInterface* WoodMaterial = MakeTint(WoodColor);
    UMaterialInterface* MetalMaterial = MakeTint(MetalColor);

    auto SetupBodyPart = [](UStaticMeshComponent* Part, UStaticMesh* PartMesh,
        const FVector& Location, const FVector& Scale, UMaterialInterface* Material)
    {
        if (!Part)
            return;
        if (PartMesh)
            Part->SetStaticMesh(PartMesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        if (Material)
            Part->SetMaterial(0, Material);
    };

    // A simple low-poly humanoid, attached to the capsule; no collision is added.
    BodyTorso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutTorso"));
    BodyTorso->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyTorso, CubeMesh, FVector(0.0f, 0.0f, 1.0f),
        FVector(0.32f, 0.22f, 0.48f), ClothMaterial);

    BodyHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutHead"));
    BodyHead->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyHead, SphereMesh, FVector(0.0f, 0.0f, 49.0f),
        FVector(0.27f, 0.25f, 0.30f), SkinMaterial);

    BodyLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutLeftArm"));
    BodyLeftArm->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyLeftArm, CylinderMesh, FVector(0.0f, -25.0f, 10.0f),
        FVector(0.085f, 0.085f, 0.29f), ClothMaterial);
    BodyRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutRightArm"));
    BodyRightArm->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyRightArm, CylinderMesh, FVector(0.0f, 25.0f, 10.0f),
        FVector(0.085f, 0.085f, 0.29f), ClothMaterial);

    BodyLeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutLeftLeg"));
    BodyLeftLeg->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyLeftLeg, CylinderMesh, FVector(0.0f, -11.0f, -45.0f),
        FVector(0.095f, 0.095f, 0.46f), ClothMaterial);
    BodyRightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutRightLeg"));
    BodyRightLeg->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyRightLeg, CylinderMesh, FVector(0.0f, 11.0f, -45.0f),
        FVector(0.095f, 0.095f, 0.46f), ClothMaterial);

    // Camera-relative forearm and axe make the chop action visible in first person.
    FirstPersonForearm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonForearm"));
    FirstPersonForearm->SetupAttachment(FirstPersonCamera);
    SetupBodyPart(FirstPersonForearm, CylinderMesh, FVector(27.0f, 17.0f, -34.0f),
        FVector(0.075f, 0.075f, 0.31f), SkinMaterial);
    FirstPersonForearm->SetRelativeRotation(
        FRotationMatrix::MakeFromZ(FVector(0.18f, 0.42f, -1.0f).GetSafeNormal()).Rotator());

    AxePivot = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonAxePivot"));
    AxePivot->SetupAttachment(FirstPersonCamera);
    AxePivot->SetRelativeLocation(FVector(44.0f, 27.0f, -26.0f));
    AxePivot->SetRelativeRotation(FRotator(-18.0f, 0.0f, 18.0f));

    AxeHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeHand"));
    AxeHand->SetupAttachment(AxePivot);
    SetupBodyPart(AxeHand, CubeMesh, FVector(0.0f, 0.0f, 0.0f),
        FVector(0.105f, 0.08f, 0.09f), SkinMaterial);

    AxeHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeWoodHandle"));
    AxeHandle->SetupAttachment(AxePivot);
    SetupBodyPart(AxeHandle, CylinderMesh, FVector(0.0f, 0.0f, 22.0f),
        FVector(0.045f, 0.045f, 0.32f), WoodMaterial);

    AxeHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeMetalHead"));
    AxeHead->SetupAttachment(AxePivot);
    SetupBodyPart(AxeHead, CubeMesh, FVector(0.0f, 0.0f, 51.0f),
        FVector(0.08f, 0.34f, 0.12f), MetalMaterial);
    AxePivotBaseRotation = AxePivot->GetRelativeRotation();

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

void ALogSnapCharacter::PlayAxeSwing()
{
    if (!AxePivot)
        return;

    AxeSwingElapsed = 0.0f;
    bAxeSwingActive = true;
}

void ALogSnapCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bAxeSwingActive && AxePivot)
    {
        AxeSwingElapsed += DeltaSeconds;
        const float Alpha = FMath::Clamp(AxeSwingElapsed / AxeSwingDuration, 0.0f, 1.0f);
        const float Sweep = FMath::Sin(Alpha * PI);
        AxePivot->SetRelativeRotation(
            AxePivotBaseRotation + FRotator(-72.0f * Sweep, -8.0f * Sweep, 14.0f * Sweep));
        if (Alpha >= 1.0f)
        {
            bAxeSwingActive = false;
            AxePivot->SetRelativeRotation(AxePivotBaseRotation);
        }
    }

    const APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
        return;
    const auto KeyValue = [PlayerController](const FKey& Key)
    {
        return PlayerController->IsInputKeyDown(Key) ? 1.0f : 0.0f;
    };
    MoveForward(KeyValue(EKeys::W) - KeyValue(EKeys::S));
    MoveRight(KeyValue(EKeys::D) - KeyValue(EKeys::A));

    // Step-ups onto logs and rocks move the capsule in one frame; the camera
    // follows smoothly instead of snapping.
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    const float GroundZ = GetActorLocation().Z;
    if (Movement && Movement->IsMovingOnGround() && bHasLastGroundZ)
    {
        const float Jump = GroundZ - LastGroundZ;
        if (FMath::Abs(Jump) > 2.0f && FMath::Abs(Jump) < 60.0f)
            StepSmoothingOffset -= Jump;
    }
    LastGroundZ = GroundZ;
    bHasLastGroundZ = true;
    StepSmoothingOffset = FMath::Clamp(FMath::FInterpTo(StepSmoothingOffset, 0.0f, DeltaSeconds, 10.0f), -60.0f, 60.0f);
    if (FirstPersonCamera)
        FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraBaseHeight + StepSmoothingOffset));
}
