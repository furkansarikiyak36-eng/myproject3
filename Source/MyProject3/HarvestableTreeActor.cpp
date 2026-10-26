#include "HarvestableTreeActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "LogBuildPiece.h"

void AHarvestableTreeActor::BeginPlay()
{
    Super::BeginPlay();
    // Capture true standing height before physics rotates the trunk. The world-space
    // Z extent after falling is mostly trunk diameter and would under-yield wood.
    if (TreeMesh && TreeMesh->GetStaticMesh() && StandingWorldHeightCm <= KINDA_SMALL_NUMBER)
        StandingWorldHeightCm = TreeMesh->Bounds.BoxExtent.Z * 2.0f;
    if (Identity)
    {
        Identity->EnsureIdentity();
        if (Identity->DefinitionId.IsNone())
            Identity->SetDefinitionId(FName(TEXT("Tree.Harvestable")));
    }
    if (!bFellingStarted && !bGrounded && !bLogsSpawned)
        ChopHealth = FMath::Max(1.0f, MaxChopHealth);
}

AHarvestableTreeActor::AHarvestableTreeActor()
{
    PrimaryActorTick.bCanEverTick = true;

    TreeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeMesh"));
    SetRootComponent(TreeMesh);
    Identity = CreateDefaultSubobject<UWorldObjectIdentityComponent>(TEXT("WorldObjectIdentity"));
    TreeMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TreeMesh->SetSimulatePhysics(false);
}

bool AHarvestableTreeActor::ApplyChopDamage(float Damage, FVector ChopDirection)
{
    if (!CanBeChopped() || !FMath::IsFinite(Damage) || Damage <= 0.0f ||
        !TreeMesh || !TreeMesh->GetStaticMesh() || !LogActorClass)
    {
        return false;
    }

    ChopHealth = FMath::Max(0.0f, (ChopHealth > 0.0f ? ChopHealth : MaxChopHealth) - Damage);
    if (ChopHealth > 0.0f)
        return true;

    // Do not start a fall that cannot yield the configured, existing log actor type.
    if (LogsToSpawn < 1 || LogMeshVariantSetId.IsNone())
    {
        ChopHealth = KINDA_SMALL_NUMBER;
        return false;
    }

    bFellingStarted = true;
    TreeMesh->SetSimulatePhysics(true);
    TreeMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    TreeMesh->WakeAllRigidBodies();

    FVector Direction = ChopDirection.GetSafeNormal2D();
    if (Direction.IsNearlyZero())
        Direction = GetActorForwardVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero())
        Direction = FVector::ForwardVector;

    // Existing trees retain their original impulse behaviour by default. A
    // configured mesh-family tree uses an off-centre impulse so the trunk has
    // a chance to rotate/fall rather than only translating sideways.
    if (!OutputProcessingMeshSets.IsEmpty())
    {
        const FVector ImpulsePoint = TreeMesh->GetComponentLocation()
            + FVector(0.0f, 0.0f, TreeMesh->Bounds.BoxExtent.Z * 0.85f);
        TreeMesh->AddImpulseAtLocation(Direction * FallImpulse, ImpulsePoint);
    }
    else
    {
        TreeMesh->AddImpulse(Direction * FallImpulse, NAME_None, true);
    }
    return true;
}

void AHarvestableTreeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bFellingStarted || bGrounded || bLogsSpawned || !TreeMesh)
        return;

    const FVector Velocity = TreeMesh->GetPhysicsLinearVelocity();
    if (Velocity.Size() <= GroundedSpeedThreshold && HasGroundContact())
        StableGroundedSeconds += DeltaSeconds;
    else
        StableGroundedSeconds = 0.0f;

    if (StableGroundedSeconds < GroundedHoldSeconds)
        return;

    // Grounded is committed only after contact plus a stable low-velocity interval.
    bGrounded = true;
    TreeMesh->SetSimulatePhysics(false);
    TreeMesh->SetCollisionProfileName(TEXT("BlockAll"));
    TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    if (!SpawnHarvestLogs())
    {
        // Keep the tree in a visible, recoverable failed-harvest state. Never retry
        // automatically after partial spawn because that could duplicate resources.
        UE_LOG(LogTemp, Error, TEXT("Tree '%s' grounded but could not create its configured log outputs."),
            *GetName());
    }
}

bool AHarvestableTreeActor::HasGroundContact() const
{
    if (!GetWorld() || !TreeMesh)
        return false;

    const FBoxSphereBounds Bounds = TreeMesh->Bounds;
    const FVector Start = Bounds.Origin;
    const FVector End(Start.X, Start.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z - 35.0f);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(TreeGroundContact), false, this);
    Params.AddIgnoredActor(this);
    FHitResult Hit;
    return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) &&
        Hit.bBlockingHit && Hit.GetActor() != this;
}

bool AHarvestableTreeActor::SpawnHarvestLogs()
{
    if (bLogsSpawned || !bGrounded || !GetWorld() || !LogActorClass ||
        LogsToSpawn < 1 || LogMeshVariantSetId.IsNone())
    {
        return false;
    }

    // Resolve yield from the actual tree mesh bounds when rules are authored.
    // An ambiguous or uncovered size band fails closed before any output is spawned.
    int32 ResolvedLogCount = LogsToSpawn;
    ELogPieceType ResolvedPieceType = ELogPieceType::WholeLog;
    if (!YieldRules.IsEmpty())
    {
        const float TreeWorldHeight = StandingWorldHeightCm > KINDA_SMALL_NUMBER
            ? StandingWorldHeightCm
            : TreeMesh->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.0f * TreeMesh->GetComponentScale().Z;
        const FHarvestTreeYieldRule* Match = nullptr;
        for (const FHarvestTreeYieldRule& Rule : YieldRules)
        {
            const bool bAboveMin = TreeWorldHeight >= Rule.MinWorldHeight;
            const bool bBelowMax = Rule.MaxWorldHeight <= 0.0f || TreeWorldHeight < Rule.MaxWorldHeight;
            if (bAboveMin && bBelowMax)
            {
                if (Match)
                {
                    UE_LOG(LogTemp, Error, TEXT("Tree '%s' has overlapping yield height bands."), *GetName());
                    return false;
                }
                Match = &Rule;
            }
        }
        if (!Match || Match->LogCount < 1)
        {
            UE_LOG(LogTemp, Error, TEXT("Tree '%s' has no valid yield rule for world height %.1f."), *GetName(), TreeWorldHeight);
            return false;
        }
        ResolvedLogCount = Match->LogCount;
        ResolvedPieceType = Match->OutputPieceType;
    }

    // Preflight all required outputs before spawning any actor. The log Blueprint
    // must author its matching ProcessingMeshSets table; BeginPlay validates and
    // registers each physical item in the canonical ownership ledger.
    TArray<ALogBuildPiece*> Spawned;
    Spawned.Reserve(ResolvedLogCount);

    // If one output fails validation after FinishSpawning, remove every sibling
    // created by this harvest attempt. This prevents a failed harvest from
    // leaving a partial visible resource set that the tree will never retry.
    auto RollbackSpawnedOutputs = [&Spawned]()
    {
        for (ALogBuildPiece* Created : Spawned)
        {
            if (!IsValid(Created))
                continue;

            Created->SetActorHiddenInGame(true);
            Created->SetActorEnableCollision(false);
            Created->Destroy();
        }
    };
    // The actor origin is near the middle of the standing trunk. After felling,
    // use the fallen mesh bounds to place outputs near ground level instead of
    // spawning them hundreds of centimetres above the terrain.
    const FBoxSphereBounds FallenBounds = TreeMesh->Bounds;
    const FVector Base(FallenBounds.Origin.X, FallenBounds.Origin.Y,
        FallenBounds.Origin.Z - FallenBounds.BoxExtent.Z + 45.0f);
    const FVector Forward = TreeMesh->GetForwardVector().GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

    // Keep independently spawned full-length logs apart rather than stacking all
    // outputs at nearly the same point.
    const float OutputSpacingCm = 350.0f;
    for (int32 Index = 0; Index < ResolvedLogCount; ++Index)
    {
        const float Side = (static_cast<float>(Index) - (ResolvedLogCount - 1) * 0.5f) * OutputSpacingCm;
        const FVector Location = Base + Right * Side;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ALogBuildPiece* Log = GetWorld()->SpawnActorDeferred<ALogBuildPiece>(
            LogActorClass, FTransform(GetActorRotation(), Location), this, nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Log)
        {
            RollbackSpawnedOutputs();
            return false;
        }

        // New demo trees may provide an exact measured mesh family at runtime.
        // When unset, preserve the existing Blueprint-authored family contract.
        if (!OutputProcessingMeshSets.IsEmpty())
            Log->ProcessingMeshSets = OutputProcessingMeshSets;
        Log->SelectedProcessingMeshSetId = LogMeshVariantSetId;
        Log->PieceType = ResolvedPieceType;
        Log->FinishSpawning(FTransform(GetActorRotation(), Location));
        Spawned.Add(Log);
        if (ResolvedPieceType != ELogPieceType::WholeLog &&
            !Log->TrySetInitialPieceType(ResolvedPieceType, FGuid::NewGuid()))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Tree '%s' output %d could not resolve its configured mesh variant for piece type %d."),
                *GetName(), Index, static_cast<int32>(ResolvedPieceType));
            RollbackSpawnedOutputs();
            return false;
        }
    }

    // The actors' BeginPlay path owns item registration. Confirm all were actually
    // materialized and visible before sealing the one-shot harvest transition.
    for (ALogBuildPiece* Log : Spawned)
    {
        if (!IsValid(Log) || Log->IsActorBeingDestroyed() || Log->IsHidden())
        {
            // A bad Blueprint/table can quarantine a spawned log. Remove every
            // sibling from this attempt so the visible resource count is all-or-none.
            // EndPlay uses the canonical ownership lifecycle for registered actors.
            RollbackSpawnedOutputs();
            return false;
        }
    }

    bLogsSpawned = true;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    return true;
}
