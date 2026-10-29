#include "LogSnapCharacter.h"

#include "LogBuildToolsComponent.h"
#include "PhysicalHealthComponent.h"
#include "ShelterSensorComponent.h"
#include "WorldSurvivalSubsystem.h"
#include "WorldSimulationClockSubsystem.h"
#include "WorldClimateSubsystem.h"
#include "TerrainDigToolComponent.h"

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
#include "InventoryCraftingComponent.h"
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
        BaseWalkSpeed = Movement->MaxWalkSpeed;
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
    auto SetupBodyPart = [](UStaticMeshComponent* Part, UStaticMesh* PartMesh,
        const FVector& Location, const FVector& Scale)
    {
        if (!Part)
            return;
        if (PartMesh)
            Part->SetStaticMesh(PartMesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
    };

    // A simple low-poly humanoid, attached to the capsule; no collision is added.
    BodyTorso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutTorso"));
    BodyTorso->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyTorso, CubeMesh, FVector(0.0f, 0.0f, 1.0f),
        FVector(0.32f, 0.22f, 0.48f));

    BodyHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutHead"));
    BodyHead->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyHead, SphereMesh, FVector(0.0f, 0.0f, 49.0f),
        FVector(0.27f, 0.25f, 0.30f));

    BodyLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutLeftArm"));
    BodyLeftArm->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyLeftArm, CylinderMesh, FVector(0.0f, -25.0f, 10.0f),
        FVector(0.065f, 0.065f, 0.52f));
    BodyRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutRightArm"));
    BodyRightArm->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyRightArm, CylinderMesh, FVector(0.0f, 25.0f, 10.0f),
        FVector(0.065f, 0.065f, 0.52f));

    BodyLeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutLeftLeg"));
    BodyLeftLeg->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyLeftLeg, CylinderMesh, FVector(0.0f, -11.0f, -45.0f),
        FVector(0.085f, 0.085f, 0.86f));
    BodyRightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutRightLeg"));
    BodyRightLeg->SetupAttachment(GetCapsuleComponent());
    SetupBodyPart(BodyRightLeg, CylinderMesh, FVector(0.0f, 11.0f, -45.0f),
        FVector(0.085f, 0.085f, 0.86f));

    // Camera-relative forearm and axe make the chop action visible in first person.
    FirstPersonForearm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonForearm"));
    FirstPersonForearm->SetupAttachment(FirstPersonCamera);
    SetupBodyPart(FirstPersonForearm, CylinderMesh, FVector(27.0f, 17.0f, -34.0f),
        FVector(0.065f, 0.065f, 0.48f));
    FirstPersonForearm->SetRelativeRotation(
        FRotationMatrix::MakeFromZ(FVector(0.18f, 0.42f, -1.0f).GetSafeNormal()).Rotator());

    AxePivot = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonAxePivot"));
    AxePivot->SetupAttachment(FirstPersonCamera);
    AxePivot->SetRelativeLocation(FVector(44.0f, 27.0f, -43.0f));
    AxePivot->SetRelativeRotation(FRotator(-18.0f, 0.0f, 18.0f));

    AxeHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeHand"));
    AxeHand->SetupAttachment(AxePivot);
    SetupBodyPart(AxeHand, CubeMesh, FVector(0.0f, 0.0f, 0.0f),
        FVector(0.105f, 0.08f, 0.09f));

    AxeHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeWoodHandle"));
    AxeHandle->SetupAttachment(AxePivot);
    SetupBodyPart(AxeHandle, CylinderMesh, FVector(0.0f, 0.0f, 22.0f),
        FVector(0.045f, 0.045f, 0.55f));

    AxeHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeMetalHead"));
    AxeHead->SetupAttachment(AxePivot);
    SetupBodyPart(AxeHead, CubeMesh, FVector(0.0f, 0.0f, 51.0f),
        FVector(0.08f, 0.34f, 0.12f));
    AxePivotBaseRotation = AxePivot->GetRelativeRotation();

    Inventory = CreateDefaultSubobject<UInventoryCraftingComponent>(TEXT("Inventory"));
    LogCarry = CreateDefaultSubobject<ULogCarryComponent>(TEXT("LogCarry"));
    CarryLoad = CreateDefaultSubobject<UCarryLoadComponent>(TEXT("CarryLoad"));
    BuildTools = CreateDefaultSubobject<ULogBuildToolsComponent>(TEXT("BuildTools"));
    PhysicalHealth = CreateDefaultSubobject<UPhysicalHealthComponent>(TEXT("PhysicalHealth"));
    ShelterSensor = CreateDefaultSubobject<UShelterSensorComponent>(TEXT("ShelterSensor"));
    TerrainDigTool = CreateDefaultSubobject<UTerrainDigToolComponent>(TEXT("TerrainDigTool"));
}

void ALogSnapCharacter::ApplyBlockoutMaterials()
{
    UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!ShapeMaterial)
        ShapeMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

    auto MakeTint = [this, ShapeMaterial](const FLinearColor& Color) -> UMaterialInterface*
    {
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

    if (BodyTorso) BodyTorso->SetMaterial(0, ClothMaterial);
    if (BodyHead) BodyHead->SetMaterial(0, SkinMaterial);
    if (BodyLeftArm) BodyLeftArm->SetMaterial(0, ClothMaterial);
    if (BodyRightArm) BodyRightArm->SetMaterial(0, ClothMaterial);
    if (BodyLeftLeg) BodyLeftLeg->SetMaterial(0, ClothMaterial);
    if (BodyRightLeg) BodyRightLeg->SetMaterial(0, ClothMaterial);
    if (FirstPersonForearm) FirstPersonForearm->SetMaterial(0, SkinMaterial);
    if (AxeHand) AxeHand->SetMaterial(0, SkinMaterial);
    if (AxeHandle) AxeHandle->SetMaterial(0, WoodMaterial);
    if (AxeHead) AxeHead->SetMaterial(0, MetalMaterial);
}

void ALogSnapCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplyBlockoutMaterials();

    const FString Controls =
        TEXT("WASD move | Mouse look | E pick/place/use | N dig bounded voxel terrain | Q throw | RMB placement mode | F1 all building keys");
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1, 8.0f, FColor::White, Controls);
    }
}

void ALogSnapCharacter::ApplyFallImpact(float ImpactSpeedCmPerSecond)
{
    if (!FMath::IsFinite(ImpactSpeedCmPerSecond) || ImpactSpeedCmPerSecond < 850.0f)
        return;

    const float Damage = FMath::Clamp((ImpactSpeedCmPerSecond - 800.0f) * 0.06f, 5.0f, 40.0f);
    if (IsValid(PhysicalHealth))
        PhysicalHealth->ApplyPhysicalDamage(Damage, FVector(0.0f, 0.0f, 100.0f), nullptr);

    if (!IsValid(ShelterSensor) || !ShelterSensor->SurvivalEntityId.IsValid() || !GetWorld())
        return;
    UWorldSurvivalSubsystem* Survival = GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>();
    FWorldSurvivalProfile Profile;
    if (!Survival || !Survival->GetProfile(ShelterSensor->SurvivalEntityId, Profile))
        return;

    const float Severity = FMath::Clamp((ImpactSpeedCmPerSecond - 850.0f) / 700.0f, 0.25f, 1.0f);
    Survival->TryApplyCondition(FGuid::NewGuid(), Profile.EntityId, Profile.Revision,
        FName(TEXT("Injury.Fall")), Severity);
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
    if (TerrainDigTool)
    {
        TerrainDigTool->BindDefaultInput(PlayerInputComponent);
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
    UWorldSurvivalSubsystem* Survival = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldSimulationClockSubsystem* Clock = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    FWorldSurvivalProfile Profile;
    if (Survival && Clock && Clock->IsClockInitialized() &&
        IsValid(ShelterSensor) && ShelterSensor->SurvivalEntityId.IsValid() &&
        Survival->GetProfile(ShelterSensor->SurvivalEntityId, Profile) &&
        Profile.RestUntilTimeSeconds > Clock->GetSimulationTimeSeconds())
    {
        return; // Rest is an actual movement-locked interval, not only a recovery bonus.
    }
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

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        if (Movement->IsFalling())
        {
            bWasFalling = true;
            PeakFallSpeedCmPerSecond = FMath::Max(
                PeakFallSpeedCmPerSecond, FMath::Max(0.0f, -Movement->Velocity.Z));
        }
        else if (bWasFalling)
        {
            const float ImpactSpeed = PeakFallSpeedCmPerSecond;
            bWasFalling = false;
            PeakFallSpeedCmPerSecond = 0.0f;
            ApplyFallImpact(ImpactSpeed);
        }
    }

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
    const float ForwardInput = KeyValue(EKeys::W) - KeyValue(EKeys::S);
    const float RightInput = KeyValue(EKeys::D) - KeyValue(EKeys::A);
    const bool bHasMovementInput = !FMath::IsNearlyZero(ForwardInput) ||
        !FMath::IsNearlyZero(RightInput);

    UWorldSurvivalSubsystem* Survival = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldSurvivalSubsystem>() : nullptr;
    UWorldClimateSubsystem* Climate = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldClimateSubsystem>() : nullptr;
    UCarryLoadComponent* CarryLoad = FindComponentByClass<UCarryLoadComponent>();
    const float CarriedMassKg = CarryLoad ? CarryLoad->GetCarriedMassKg() : 0.0f;
    const float CarrySpeedFactor = CarryLoad
        ? FMath::Clamp(CarryLoad->GetSpeedMultiplier(), 0.05f, 1.0f) : 1.0f;
    float SlopeSpeedFactor = 1.0f;
    FHitResult GroundHit;
    FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(PlayerSlopeCost), false, this);
    const FVector GroundStart = GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
    const FVector GroundEnd = GetActorLocation() - FVector(0.0f, 0.0f, 180.0f);
    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
        GroundHit, GroundStart, GroundEnd, ECC_Visibility, GroundParams))
    {
        const float GroundNormalZ = FMath::Clamp(GroundHit.ImpactNormal.Z, 0.0f, 1.0f);
        SlopeSpeedFactor = FMath::Lerp(0.65f, 1.0f, GroundNormalZ);
    }

    float WeatherSpeedFactor = 1.0f;
    if (Climate && Climate->IsClimateInitialized())
    {
        const float Severity = FMath::Clamp(Climate->GetWeatherSeverity(), 0.0f, 1.0f);
        switch (Climate->GetWeather())
        {
        case EWorldWeatherState::Rain: WeatherSpeedFactor = 1.0f - 0.12f * Severity; break;
        case EWorldWeatherState::Wind: WeatherSpeedFactor = 1.0f - 0.06f * Severity; break;
        case EWorldWeatherState::Snow: WeatherSpeedFactor = 1.0f - 0.18f * Severity; break;
        case EWorldWeatherState::Storm: WeatherSpeedFactor = 1.0f - 0.24f * Severity; break;
        case EWorldWeatherState::Heatwave: WeatherSpeedFactor = 1.0f - 0.05f * Severity; break;
        default: break;
        }
        const float SnowRatio = FMath::Clamp(Climate->GetSnowDepthCm() / 100.0f, 0.0f, 1.0f);
        WeatherSpeedFactor *= 1.0f - 0.12f * SnowRatio;
    }

    FWorldSurvivalProfile SurvivalProfile;
    const bool bHasSurvivalProfile = Survival && IsValid(ShelterSensor) &&
        ShelterSensor->SurvivalEntityId.IsValid() &&
        Survival->GetProfile(ShelterSensor->SurvivalEntityId, SurvivalProfile);
    const UWorldSimulationClockSubsystem* Clock = GetWorld()
        ? GetWorld()->GetSubsystem<UWorldSimulationClockSubsystem>() : nullptr;
    const bool bIsResting = bHasSurvivalProfile && Clock && Clock->IsClockInitialized() &&
        SurvivalProfile.RestUntilTimeSeconds > Clock->GetSimulationTimeSeconds();
    bool bSprintActive = false;
    float FatigueSpeedFactor = 1.0f;
    if (bHasSurvivalProfile)
    {
        FatigueSpeedFactor = FMath::Lerp(1.0f, 0.45f,
            FMath::Clamp(SurvivalProfile.Fatigue / 100.0f, 0.0f, 1.0f));
        bSprintActive = !bIsResting && bHasMovementInput &&
            PlayerController->IsInputKeyDown(EKeys::LeftShift) &&
            SurvivalProfile.Stamina > 20.0f && SurvivalProfile.Fatigue < 85.0f;
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
            Movement->MaxWalkSpeed = BaseWalkSpeed * FatigueSpeedFactor * CarrySpeedFactor *
                SlopeSpeedFactor * WeatherSpeedFactor * (bSprintActive ? 1.5f : 1.0f);

        if (bIsResting)
        {
            SurvivalActivityAccumulator = 0.0f;
        }
        else if (bHasMovementInput)
        {
            SurvivalActivityAccumulator += DeltaSeconds;
            if (SurvivalActivityAccumulator >= 1.0f)
            {
                const float ActivitySeconds = FMath::Clamp(SurvivalActivityAccumulator, 1.0f, 10.0f);
                SurvivalActivityAccumulator = 0.0f;
                const bool bRecorded = Survival->RecordActivity(
                    ShelterSensor->SurvivalEntityId,
                    bSprintActive ? ESurvivalActivity::Run : ESurvivalActivity::Walk,
                    ActivitySeconds, CarriedMassKg);
                if (bSprintActive && !bRecorded)
                {
                    bSprintActive = false;
                    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
                        Movement->MaxWalkSpeed = BaseWalkSpeed * FatigueSpeedFactor * CarrySpeedFactor *
                            SlopeSpeedFactor * WeatherSpeedFactor;
                }
            }
        }
        else
        {
            SurvivalActivityAccumulator = 0.0f;
        }
    }
    else
    {
        SurvivalActivityAccumulator = 0.0f;
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
            Movement->MaxWalkSpeed = BaseWalkSpeed;
    }

    // VerticalSlice_C rest is a bounded sleep-like interval on the shared
    // clock. The pawn polls WASD directly, so movement calls must be gated here.
    if (!bIsResting)
    {
        MoveForward(ForwardInput);
        MoveRight(RightInput);
    }

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
