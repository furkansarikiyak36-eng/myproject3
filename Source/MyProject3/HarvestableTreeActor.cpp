#include "HarvestableTreeActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "LogBuildPiece.h"
#include "WorldObjectIdentityComponent.h"
#include "Math/RotationMatrix.h"

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

    // A chopped tree must leave a stationary stump before the trunk starts moving.
    // If stump creation fails, do not launch a tree that cannot complete its harvest.
    if (!SpawnStump())
    {
        ChopHealth = FMath::Max(1.0f, MaxChopHealth * 0.25f);
        return false;
    }

    bFellingStarted = true;
    TreeMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TreeMesh->SetLinearDamping(0.8f);
    TreeMesh->SetAngularDamping(0.8f);
    TreeMesh->SetSimulatePhysics(true);
    TreeMesh->WakeAllRigidBodies();

    FVector Direction = ChopDirection.GetSafeNormal2D();
    if (Direction.IsNearlyZero())
        Direction = GetActorForwardVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero())
        Direction = FVector::ForwardVector;

    // FallImpulse is a legacy authored value. Clamp its per-mass effect so a
    // value such as 250000 cannot launch the tree across the map.
    const float BodyMassKg = FMath::Max(1.0f, TreeMesh->GetMass());
    const float RequestedImpulsePerMass = FallImpulse > 0.0f
        ? FallImpulse / BodyMassKg
        : 15.0f;
    const float SafeImpulse = BodyMassKg * FMath::Clamp(RequestedImpulsePerMass, 5.0f, 25.0f);
    const FVector ImpulsePoint = TreeMesh->GetComponentLocation()
        + TreeMesh->GetUpVector() * TreeMesh->Bounds.BoxExtent.Z * 0.85f;
    const FVector FallAxis = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();

    TreeMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    // Give the trunk a controlled tipping angular velocity. A horizontal impulse
    // alone mostly translates tall, heavy tree meshes instead of reliably felling them.
    TreeMesh->SetPhysicsAngularVelocityInDegrees(FallAxis * 90.0f);
    TreeMesh->AddImpulseAtLocation(Direction * SafeImpulse, ImpulsePoint);
    return true;
}

bool AHarvestableTreeActor::SpawnStump()
{
    if (bStumpSpawned)
        return IsValid(StumpActor);

    if (!GetWorld() || !TreeMesh || !TreeMesh->GetStaticMesh())
        return false;

    UStaticMesh* TrunkAsset = TreeMesh->GetStaticMesh();
    const float LocalMeshHeight = TrunkAsset->GetBounds().BoxExtent.Z * 2.0f;
    if (!FMath::IsFinite(LocalMeshHeight) || LocalMeshHeight <= KINDA_SMALL_NUMBER)
        return false;

    const float StandingHeight = StandingWorldHeightCm > KINDA_SMALL_NUMBER
        ? StandingWorldHeightCm
        : LocalMeshHeight * TreeMesh->GetComponentScale().Z;
    const FVector TrunkCentre = TreeMesh->GetComponentLocation();
    const float TrunkBaseZ = TrunkCentre.Z - StandingHeight * 0.5f;
    const float ActualStumpHeight = FMath::Clamp(StumpHeightCm, 10.0f, 100.0f);
    const FRotator StumpRotation(0.0f, TreeMesh->GetComponentRotation().Yaw, 0.0f);
    const FVector StumpLocation(TrunkCentre.X, TrunkCentre.Y, TrunkBaseZ + ActualStumpHeight * 0.5f);
    const FTransform StumpTransform(StumpRotation, StumpLocation);

    AActor* NewStump = GetWorld()->SpawnActorDeferred<AActor>(
        AActor::StaticClass(), StumpTransform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!NewStump)
        return false;

    UStaticMeshComponent* StumpMesh = NewObject<UStaticMeshComponent>(NewStump, TEXT("StumpMesh"));
    if (!StumpMesh)
    {
        NewStump->Destroy();
        return false;
    }

    NewStump->AddInstanceComponent(StumpMesh);
    NewStump->SetRootComponent(StumpMesh);
    StumpMesh->SetStaticMesh(TrunkAsset);
    const FVector TreeScale = TreeMesh->GetComponentScale();
    StumpMesh->SetRelativeScale3D(FVector(
        TreeScale.X, TreeScale.Y, ActualStumpHeight / LocalMeshHeight));
    StumpMesh->SetMaterial(0, TreeMesh->GetMaterial(0));
    // The upright/falling trunk initially overlaps the stump footprint. Keep
    // stump collision disabled during the fall to avoid a depenetration kick;
    // it is enabled only after the trunk has been removed from the scene.
    StumpMesh->SetCollisionProfileName(TEXT("NoCollision"));
    StumpMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StumpMesh->SetSimulatePhysics(false);
    StumpMesh->RegisterComponent();
    NewStump->FinishSpawning(StumpTransform);

    if (!IsValid(NewStump) || !IsValid(StumpMesh) || !StumpMesh->GetStaticMesh())
    {
        if (IsValid(NewStump))
            NewStump->Destroy();
        return false;
    }

    StumpActor = NewStump;
    StumpMeshComponent = StumpMesh;
    bStumpSpawned = true;
    return true;
}

void AHarvestableTreeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bFellingStarted || bGrounded || bLogsSpawned || !TreeMesh)
        return;

    const FVector Velocity = TreeMesh->GetPhysicsLinearVelocity();
    const FVector AngularVelocity = TreeMesh->GetPhysicsAngularVelocityInDegrees();
    const bool bLinearSettled = Velocity.Size() <= GroundedSpeedThreshold;
    const bool bAngularSettled = AngularVelocity.Size() <= GroundedAngularSpeedThreshold;
    if (bLinearSettled && bAngularSettled && HasGroundContact())
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

TArray<float> AHarvestableTreeActor::GetTrunkLogLengths(float StandingHeightCm, float StumpCm, float TopShare)
{
    TArray<float> Lengths;
    float Usable = StandingHeightCm * (1.0f - FMath::Clamp(TopShare, 0.0f, 0.6f)) - StumpCm;
    while (Usable >= 600.0f)
    {
        Lengths.Add(600.0f);
        Usable -= 600.0f;
    }
    for (const float Piece : { 450.0f, 300.0f, 150.0f })
    {
        if (Usable >= Piece)
        {
            Lengths.Add(Piece);
            break;
        }
    }
    return Lengths;
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

    // Logs by trunk length: each piece gets the round mesh of its own length.
    TArray<float> TrunkPieces;
    if (YieldRules.IsEmpty() && bYieldFromTrunkLength)
    {
        const float TreeWorldHeight = StandingWorldHeightCm > KINDA_SMALL_NUMBER
            ? StandingWorldHeightCm
            : TreeMesh->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.0f * TreeMesh->GetComponentScale().Z;
        TrunkPieces = GetTrunkLogLengths(TreeWorldHeight, FMath::Clamp(StumpHeightCm, 10.0f, 100.0f), UnusableTopShare);
        if (TrunkPieces.IsEmpty())
            TrunkPieces.Add(150.0f);
        ResolvedLogCount = TrunkPieces.Num();
    }

    // Runtime-spawned trees in VerticalSlice_C supply the selected family but
    // the base ALogBuildPiece class has no authored default mesh. Resolve it
    // explicitly before FinishSpawning so BeginPlay can register a visible item.
    UStaticMesh* OutputBaseMesh = nullptr;
    for (const FLogProcessingMeshSet& MeshSet : OutputProcessingMeshSets)
    {
        if (MeshSet.VariantSetId == LogMeshVariantSetId)
        {
            OutputBaseMesh = MeshSet.BaseRoundMesh.Get();
            break;
        }
    }
    if (!OutputProcessingMeshSets.IsEmpty() && !IsValid(OutputBaseMesh))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Tree '%s' has no base round mesh for output family '%s'."),
            *GetName(), *LogMeshVariantSetId.ToString());
        return false;
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
        FallenBounds.Origin.Z - FallenBounds.BoxExtent.Z + 28.0f);

    // C's tree trunks are cylinders whose long axis is local Z; log assets use
    // local X. Align output logs to the fallen trunk axis, projected onto the ground.
    const FVector TreeAxis3D = TreeMesh->GetUpVector().GetSafeNormal();
    FVector Forward(TreeAxis3D.X, TreeAxis3D.Y, 0.0f);
    Forward = Forward.GetSafeNormal();
    if (Forward.IsNearlyZero())
        Forward = TreeMesh->GetForwardVector().GetSafeNormal2D();
    if (Forward.IsNearlyZero())
        Forward = FVector::ForwardVector;
    const FRotator LogRotation = FRotationMatrix::MakeFromX(Forward).Rotator();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

    // Keep independently spawned full-length logs apart rather than stacking all
    // outputs at nearly the same point.
    const float OutputSpacingCm = 350.0f;
    // Trunk pieces lie end to end from the stump outwards, as if the trunk was cut up.
    float TrunkStart = 0.0f;
    if (!TrunkPieces.IsEmpty())
    {
        float Total = 0.0f;
        for (const float Piece : TrunkPieces)
            Total += Piece + 8.0f;
        TrunkStart = -0.5f * Total;
    }
    for (int32 Index = 0; Index < ResolvedLogCount; ++Index)
    {
        const float Side = (static_cast<float>(Index) - (ResolvedLogCount - 1) * 0.5f) * OutputSpacingCm;
        FVector Location = Base + Right * Side;
        ELogPieceType PieceTypeForThis = ResolvedPieceType;
        if (!TrunkPieces.IsEmpty())
        {
            const float Piece = TrunkPieces[Index];
            Location = Base + Forward * (TrunkStart + 0.5f * Piece);
            TrunkStart += Piece + 8.0f;
            PieceTypeForThis = Piece >= 600.0f ? ELogPieceType::WholeLog
                : Piece >= 450.0f ? ELogPieceType::ThreeQuarterLog
                : Piece >= 300.0f ? ELogPieceType::HalfLengthLog : ELogPieceType::QuarterLog;
        }
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ALogBuildPiece* Log = GetWorld()->SpawnActorDeferred<ALogBuildPiece>(
            LogActorClass, FTransform(LogRotation, Location), this, nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Log)
        {
            RollbackSpawnedOutputs();
            return false;
        }

        // New demo trees may provide an exact measured mesh family at runtime.
        // When unset, preserve the existing Blueprint-authored family contract.
        if (!OutputProcessingMeshSets.IsEmpty())
        {
            Log->ProcessingMeshSets = OutputProcessingMeshSets;
            Log->SetPieceStaticMesh(OutputBaseMesh);
        }
        Log->SelectedProcessingMeshSetId = LogMeshVariantSetId;
        Log->BaseDefinitionId = FName(TEXT("Wood.Log.Harvested"));
        Log->PieceType = TrunkPieces.IsEmpty() ? ResolvedPieceType : ELogPieceType::WholeLog;
        Log->FinishSpawning(FTransform(LogRotation, Location));
        Spawned.Add(Log);
        if (PieceTypeForThis != ELogPieceType::WholeLog &&
            !Log->TrySetInitialPieceType(PieceTypeForThis, FGuid::NewGuid()))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Tree '%s' output %d could not resolve its configured mesh variant for piece type %d."),
                *GetName(), Index, static_cast<int32>(PieceTypeForThis));
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

    // The stump becomes a solid world object only after the fallen trunk has
    // been removed, so the two pieces cannot launch each other through overlap.
    if (IsValid(StumpMeshComponent))
    {
        StumpMeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
        StumpMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }

    bLogsSpawned = true;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    return true;
}
