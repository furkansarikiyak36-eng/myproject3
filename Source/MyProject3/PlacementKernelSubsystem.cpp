#include "PlacementKernelSubsystem.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodyInstance.h"
#include "CollisionShape.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlacementKernel, Log, All);

namespace
{
    bool ReadFiniteNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, float& OutValue)
    {
        double Number = 0.0;
        if (!Object.IsValid() || !Object->TryGetNumberField(Key, Number) ||
            !FMath::IsFinite(Number) || Number < -MAX_flt || Number > MAX_flt)
        {
            return false;
        }
        OutValue = static_cast<float>(Number);
        return true;
    }

    void ReadBoolIfPresent(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, bool& OutValue)
    {
        if (Object.IsValid())
        {
            bool Value = false;
            if (Object->TryGetBoolField(Key, Value))
                OutValue = Value;
        }
    }

    void ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, TArray<FString>& OutValues)
    {
        OutValues.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || !Values)
            return;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            if (Value.IsValid() && Value->Type == EJson::String)
                OutValues.Add(Value->AsString());
        }
    }

    void ReadNumberArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, TArray<float>& OutValues)
    {
        OutValues.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || !Values)
            return;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            if (Value.IsValid() && Value->Type == EJson::Number && FMath::IsFinite(Value->AsNumber()))
                OutValues.Add(static_cast<float>(Value->AsNumber()));
        }
    }

    bool IsUsableTransform(const FTransform& Transform)
    {
        const FVector Location = Transform.GetLocation();
        const FVector Scale = Transform.GetScale3D();
        const FQuat Rotation = Transform.GetRotation();
        return !Transform.ContainsNaN() &&
            FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) && FMath::IsFinite(Location.Z) &&
            FMath::IsFinite(Scale.X) && FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z) &&
            !Scale.IsNearlyZero() &&
            FMath::IsFinite(Rotation.X) && FMath::IsFinite(Rotation.Y) &&
            FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W);
    }
}

void UPlacementKernelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FString Failure;
    if (!ReloadProfileCatalog(&Failure))
    {
        CatalogFailureReason = Failure;
        UE_LOG(LogPlacementKernel, Error,
            TEXT("Global placement catalog rejected. Runtime snap is fail-closed: %s"), *Failure);
    }
}

void UPlacementKernelSubsystem::Deinitialize()
{
    Profiles.Reset();
    bCatalogReady = false;
    Super::Deinitialize();
}

bool UPlacementKernelSubsystem::ReloadProfileCatalog(FString* OutFailureReason)
{
    const FString CatalogPath = FPaths::Combine(
        FPaths::ProjectConfigDir(), TEXT("Placement"), TEXT("SnapVariationLibrary.json"));
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *CatalogPath))
    {
        const FString Failure = FString::Printf(TEXT("Cannot read profile catalog: %s"), *CatalogPath);
        if (OutFailureReason) *OutFailureReason = Failure;
        bCatalogReady = false;
        Profiles.Reset();
        CatalogFailureReason = Failure;
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        const FString Failure = FString::Printf(TEXT("Profile catalog is not valid JSON: %s"), *CatalogPath);
        if (OutFailureReason) *OutFailureReason = Failure;
        bCatalogReady = false;
        Profiles.Reset();
        CatalogFailureReason = Failure;
        return false;
    }

    FString SchemaVersion;
    if (!Root->TryGetStringField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != TEXT("2.0.0"))
    {
        const FString Failure = TEXT("Unsupported snap profile catalog schemaVersion; expected 2.0.0.");
        if (OutFailureReason) *OutFailureReason = Failure;
        bCatalogReady = false;
        Profiles.Reset();
        CatalogFailureReason = Failure;
        return false;
    }

    TMap<FString, FPlacementProfileDefinition> Loaded;
    FString Failure;
    const TCHAR* Arrays[] = {
        TEXT("connectionProfiles"),
        TEXT("surfacePlacementProfiles"),
        TEXT("receiverInteractionProfiles"),
        TEXT("voxelOperationProfiles"),
        TEXT("geometryProcessingProfiles")
    };
    for (const TCHAR* ArrayName : Arrays)
    {
        if (!LoadProfileArray(Root, ArrayName, Loaded, Failure))
        {
            if (OutFailureReason) *OutFailureReason = Failure;
            bCatalogReady = false;
            Profiles.Reset();
            CatalogFailureReason = Failure;
            return false;
        }
    }

    if (Loaded.IsEmpty())
    {
        const FString EmptyFailure = TEXT("Profile catalog contains no valid profiles.");
        if (OutFailureReason) *OutFailureReason = EmptyFailure;
        bCatalogReady = false;
        Profiles.Reset();
        CatalogFailureReason = EmptyFailure;
        return false;
    }

    Profiles = MoveTemp(Loaded);
    bCatalogReady = true;
    CatalogFailureReason.Reset();
    UE_LOG(LogPlacementKernel, Log, TEXT("Loaded %d placement profiles from %s."),
        Profiles.Num(), *CatalogPath);
    return true;
}

bool UPlacementKernelSubsystem::LoadProfileArray(
    const TSharedPtr<FJsonObject>& Root,
    const FString& ArrayName,
    TMap<FString, FPlacementProfileDefinition>& InOutProfiles,
    FString& OutFailureReason) const
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Root.IsValid() || !Root->TryGetArrayField(ArrayName, Values) || !Values)
    {
        OutFailureReason = FString::Printf(TEXT("Required profile array '%s' is missing."), *ArrayName);
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            OutFailureReason = FString::Printf(TEXT("Array '%s' contains a non-object entry."), *ArrayName);
            return false;
        }
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FPlacementProfileDefinition Profile;
        if (!Object.IsValid() ||
            !Object->TryGetStringField(TEXT("id"), Profile.Id) || Profile.Id.IsEmpty() ||
            !Object->TryGetStringField(TEXT("family"), Profile.Family) || Profile.Family.IsEmpty() ||
            !Object->TryGetStringField(TEXT("operationKind"), Profile.OperationKindText) || Profile.OperationKindText.IsEmpty() ||
            !Object->TryGetStringField(TEXT("geometry"), Profile.Geometry) || Profile.Geometry.IsEmpty() ||
            !Object->TryGetStringField(TEXT("description"), Profile.Description) || Profile.Description.IsEmpty())
        {
            OutFailureReason = FString::Printf(TEXT("Array '%s' contains a profile missing id/family/operationKind/geometry/description."), *ArrayName);
            return false;
        }
        const TMap<FString, FString> RequiredKinds = {
            { TEXT("connectionProfiles"), TEXT("connection") },
            { TEXT("surfacePlacementProfiles"), TEXT("surface_placement") },
            { TEXT("receiverInteractionProfiles"), TEXT("receiver_interaction") },
            { TEXT("voxelOperationProfiles"), TEXT("voxel_operation") },
            { TEXT("geometryProcessingProfiles"), TEXT("geometry_processing") }
        };
        const FString* ExpectedKind = RequiredKinds.Find(ArrayName);
        if (!ExpectedKind || Profile.OperationKindText != *ExpectedKind)
        {
            OutFailureReason = FString::Printf(
                TEXT("Profile '%s' has operationKind '%s' but is stored in '%s'."),
                *Profile.Id, *Profile.OperationKindText, *ArrayName);
            return false;
        }
        if (InOutProfiles.Contains(Profile.Id))
        {
            OutFailureReason = FString::Printf(TEXT("Duplicate placement profile ID: %s"), *Profile.Id);
            return false;
        }

        Object->TryGetStringField(TEXT("solver"), Profile.Solver);
        Object->TryGetStringField(TEXT("orientationPolicy"), Profile.OrientationPolicy);
        Object->TryGetStringField(TEXT("physicsPolicy"), Profile.PhysicsPolicy);
        Object->TryGetStringField(TEXT("failurePolicy"), Profile.FailurePolicy);
        ReadFiniteNumber(Object, TEXT("translationToleranceCm"), Profile.TranslationToleranceCm);
        ReadFiniteNumber(Object, TEXT("angularToleranceDeg"), Profile.AngularToleranceDeg);
        ReadFiniteNumber(Object, TEXT("maxLengthMismatchCm"), Profile.MaxLengthMismatchCm);
        double IntegerValue = 0.0;
        if (Object->TryGetNumberField(TEXT("minRequiredConnections"), IntegerValue))
            Profile.MinRequiredConnections = FMath::Max(0, static_cast<int32>(IntegerValue));
        if (Object->TryGetNumberField(TEXT("minimumDistinctSupports"), IntegerValue))
            Profile.MinimumDistinctSupports = FMath::Max(0, static_cast<int32>(IntegerValue));
        ReadBoolIfPresent(Object, TEXT("requiresCollisionValidation"), Profile.bRequiresCollisionValidation);
        ReadBoolIfPresent(Object, TEXT("requiresStructuralValidation"), Profile.bRequiresStructuralValidation);
        ReadBoolIfPresent(Object, TEXT("atomicCommit"), Profile.bAtomicCommit);
        ReadBoolIfPresent(Object, TEXT("allowsPhysicsFallback"), Profile.bAllowsPhysicsFallback);
        ReadBoolIfPresent(Object, TEXT("requiresExplicitProfile"), Profile.bRequiresExplicitProfile);
        ReadBoolIfPresent(Object, TEXT("requiresJoineryOrExplicitOverlapPolicy"), Profile.bRequiresJoineryOrExplicitOverlapPolicy);
        ReadStringArray(Object, TEXT("sourceFeatures"), Profile.SourceFeatures);
        ReadStringArray(Object, TEXT("targetFeatures"), Profile.TargetFeatures);
        ReadNumberArray(Object, TEXT("angleRangeDeg"), Profile.AngleRangeDeg);
        ReadNumberArray(Object, TEXT("preferredAnglesDeg"), Profile.PreferredAnglesDeg);

        if (Profile.TranslationToleranceCm < 0.0f || Profile.AngularToleranceDeg < 0.0f ||
            Profile.AngularToleranceDeg > 180.0f ||
            (!Profile.AngleRangeDeg.IsEmpty() && (Profile.AngleRangeDeg.Num() != 2 ||
             Profile.AngleRangeDeg[0] < 0.0f || Profile.AngleRangeDeg[1] > 180.0f ||
             Profile.AngleRangeDeg[0] > Profile.AngleRangeDeg[1])))
        {
            OutFailureReason = FString::Printf(TEXT("Profile '%s' has invalid tolerances or angle limits."), *Profile.Id);
            return false;
        }
        if (Profile.Geometry == TEXT("multi_anchor_beam") &&
            (!Profile.bAtomicCommit || Profile.MinRequiredConnections < 2 ||
             Profile.MinimumDistinctSupports < 2 || Profile.bAllowsPhysicsFallback))
        {
            OutFailureReason = FString::Printf(TEXT("Profile '%s' violates the two-support fail-closed contract."), *Profile.Id);
            return false;
        }

        InOutProfiles.Add(Profile.Id, MoveTemp(Profile));
    }
    return true;
}

bool UPlacementKernelSubsystem::HasProfile(const FString& ProfileId) const
{
    return bCatalogReady && Profiles.Contains(ProfileId);
}

bool UPlacementKernelSubsystem::GetProfile(
    const FString& ProfileId, FPlacementProfileDefinition& OutProfile) const
{
    if (!bCatalogReady)
        return false;
    const FPlacementProfileDefinition* Found = Profiles.Find(ProfileId);
    if (!Found)
        return false;
    OutProfile = *Found;
    return true;
}

bool UPlacementKernelSubsystem::ProfileRequiresCollisionValidation(const FString& ProfileId) const
{
    const FPlacementProfileDefinition* Profile = Profiles.Find(ProfileId);
    return bCatalogReady && Profile && Profile->bRequiresCollisionValidation;
}

bool UPlacementKernelSubsystem::SolveAnchorPair(
    const FString& ProfileId,
    const FTransform& SourceLocalAnchor,
    const FTransform& TargetWorldAnchor,
    const FTransform& CurrentActorWorldTransform,
    FPlacementSolution& OutSolution,
    FString& OutFailureReason) const
{
    OutSolution = FPlacementSolution();
    FPlacementProfileDefinition Profile;
    if (!GetProfile(ProfileId, Profile) ||
        (!Profile.IsConnection() && Profile.OperationKindText != TEXT("surface_placement")))
    {
        OutFailureReason = FString::Printf(TEXT("Profile '%s' is missing or cannot solve a feature-to-transform placement."), *ProfileId);
        return false;
    }
    if (!IsUsableTransform(SourceLocalAnchor) || !IsUsableTransform(TargetWorldAnchor) ||
        !IsUsableTransform(CurrentActorWorldTransform))
    {
        OutFailureReason = TEXT("Anchor or actor transform contains invalid coordinates, rotation or scale.");
        return false;
    }

    FTransform DesiredActor = SourceLocalAnchor.Inverse() * TargetWorldAnchor;
    const FVector ActorScale = CurrentActorWorldTransform.GetScale3D();
    if (ActorScale.ContainsNaN() || ActorScale.IsNearlyZero())
    {
        OutFailureReason = TEXT("The source actor scale is invalid.");
        return false;
    }
    DesiredActor.SetScale3D(ActorScale);
    const FVector ScaledSourceLocation = SourceLocalAnchor.GetLocation() * ActorScale;
    DesiredActor.SetLocation(
        TargetWorldAnchor.GetLocation() - DesiredActor.GetRotation().RotateVector(ScaledSourceLocation));
    if (!IsUsableTransform(DesiredActor))
    {
        OutFailureReason = TEXT("Paired-anchor solve produced an invalid transform.");
        return false;
    }

    const float PositionResidual = FVector::Distance(
        (SourceLocalAnchor * DesiredActor).GetLocation(), TargetWorldAnchor.GetLocation());
    const float AllowedPositionError = FMath::Max(Profile.TranslationToleranceCm, 0.01f);
    if (!FMath::IsFinite(PositionResidual) || PositionResidual > AllowedPositionError)
    {
        OutFailureReason = FString::Printf(
            TEXT("Paired-anchor residual %.2f cm exceeds %.2f cm."), PositionResidual, AllowedPositionError);
        return false;
    }

    OutSolution.bValid = true;
    OutSolution.ProfileId = ProfileId;
    OutSolution.ActorWorldTransform = DesiredActor;
    OutSolution.Score = 0.0f;
    return true;
}

bool UPlacementKernelSubsystem::SolveTwoAnchorAlignment(
    const FString& ProfileId,
    const FTransform& SourceLocalAnchorA,
    const FTransform& SourceLocalAnchorB,
    const FTransform& TargetWorldAnchorA,
    const FTransform& TargetWorldAnchorB,
    const FTransform& CurrentActorWorldTransform,
    FPlacementSolution& OutSolution,
    FString& OutFailureReason) const
{
    OutSolution = FPlacementSolution();
    FPlacementProfileDefinition Profile;
    if (!GetProfile(ProfileId, Profile) || !Profile.IsConnection())
    {
        OutFailureReason = FString::Printf(TEXT("Multi-anchor profile '%s' is missing or has the wrong operation kind."), *ProfileId);
        return false;
    }
    if (!Profile.bAtomicCommit || Profile.MinRequiredConnections < 2 ||
        Profile.MinimumDistinctSupports < 2)
    {
        OutFailureReason = FString::Printf(TEXT("Profile '%s' does not declare an atomic multi-support solution."), *ProfileId);
        return false;
    }
    if (!IsUsableTransform(SourceLocalAnchorA) || !IsUsableTransform(SourceLocalAnchorB) ||
        !IsUsableTransform(TargetWorldAnchorA) || !IsUsableTransform(TargetWorldAnchorB) ||
        !IsUsableTransform(CurrentActorWorldTransform))
    {
        OutFailureReason = TEXT("A multi-anchor frame contains invalid coordinates, rotation or scale.");
        return false;
    }

    const FVector SourceWorldA = (SourceLocalAnchorA * CurrentActorWorldTransform).GetLocation();
    const FVector SourceWorldB = (SourceLocalAnchorB * CurrentActorWorldTransform).GetLocation();
    const FVector TargetVector =
        TargetWorldAnchorB.GetLocation() - TargetWorldAnchorA.GetLocation();
    const float SourceSpan = FVector::Distance(SourceWorldA, SourceWorldB);
    const float TargetSpan = TargetVector.Size();
    const float AllowedMismatch = Profile.MaxLengthMismatchCm > 0.0f
        ? Profile.MaxLengthMismatchCm : Profile.TranslationToleranceCm;
    if (SourceSpan <= KINDA_SMALL_NUMBER || TargetSpan <= KINDA_SMALL_NUMBER ||
        FMath::Abs(SourceSpan - TargetSpan) > AllowedMismatch)
    {
        OutFailureReason = FString::Printf(
            TEXT("Required beam span mismatch: source %.2f cm, supports %.2f cm, tolerance %.2f cm."),
            SourceSpan, TargetSpan, AllowedMismatch);
        return false;
    }

    // Anchor A fixes the full rigid transform (including roll). Anchor B is a
    // hard constraint, not another opportunity to silently bend/scale the beam.
    FTransform DesiredActor = SourceLocalAnchorA.Inverse() * TargetWorldAnchorA;
    const FVector ActorScale = CurrentActorWorldTransform.GetScale3D();
    if (ActorScale.ContainsNaN() || ActorScale.IsNearlyZero())
    {
        OutFailureReason = TEXT("The source actor scale is invalid.");
        return false;
    }
    DesiredActor.SetScale3D(ActorScale);
    const FVector ScaledSourceLocation = SourceLocalAnchorA.GetLocation() * ActorScale;
    DesiredActor.SetLocation(
        TargetWorldAnchorA.GetLocation() - DesiredActor.GetRotation().RotateVector(ScaledSourceLocation));
    const FVector SolvedAnchorB = (SourceLocalAnchorB * DesiredActor).GetLocation();
    const float Residual = FVector::Distance(SolvedAnchorB, TargetWorldAnchorB.GetLocation());
    const FVector SolvedAxis = SolvedAnchorB - TargetWorldAnchorA.GetLocation();
    const float AxisAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(SolvedAxis.GetSafeNormal(), TargetVector.GetSafeNormal()), -1.0f, 1.0f)));
    if (!IsUsableTransform(DesiredActor) || Residual > AllowedMismatch ||
        !FMath::IsFinite(AxisAngle) || AxisAngle > Profile.AngularToleranceDeg)
    {
        OutFailureReason = FString::Printf(
            TEXT("Second anchor residual %.2f cm exceeds %.2f cm; the rigid placement does not fit both supports."),
            Residual, AllowedMismatch);
        return false;
    }

    OutSolution.bValid = true;
    OutSolution.ProfileId = ProfileId;
    OutSolution.ActorWorldTransform = DesiredActor;
    OutSolution.Score = Residual;
    return true;
}

bool UPlacementKernelSubsystem::ValidateCandidatePose(
    AActor* MovingActor,
    const FTransform& CandidateWorldTransform,
    const TArray<AActor*>& IgnoredActors,
    FString& OutFailureReason) const
{
    if (!IsValid(MovingActor) || !GetWorld() || !IsUsableTransform(CandidateWorldTransform))
    {
        OutFailureReason = TEXT("The moving actor, world or candidate pose is invalid.");
        return false;
    }

    FBox CurrentBounds = MovingActor->GetComponentsBoundingBox(true);
    if (!CurrentBounds.IsValid)
    {
        OutFailureReason = TEXT("The moving actor has no valid bounds for the broad-phase clearance query.");
        return false;
    }

    const FTransform CurrentActor = MovingActor->GetActorTransform();
    const FVector LocalCenter = CurrentActor.InverseTransformPosition(CurrentBounds.GetCenter());
    const FVector CandidateCenter = CandidateWorldTransform.TransformPosition(LocalCenter);
    FVector Extent = CurrentBounds.GetExtent() + FVector(0.5f);
    if (Extent.ContainsNaN() || Extent.IsNearlyZero())
    {
        OutFailureReason = TEXT("The candidate broad-phase bounds are invalid.");
        return false;
    }

    FCollisionObjectQueryParams ObjectQuery;
    ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GlobalPlacementClearance), false, MovingActor);
    QueryParams.AddIgnoredActor(MovingActor);
    for (AActor* Ignored : IgnoredActors)
    {
        if (IsValid(Ignored))
            QueryParams.AddIgnoredActor(Ignored);
    }

    TArray<FOverlapResult> Overlaps;
    const bool bOverlapped = GetWorld()->OverlapMultiByObjectType(
        Overlaps, CandidateCenter, CandidateWorldTransform.GetRotation(),
        ObjectQuery, FCollisionShape::MakeBox(Extent), QueryParams);
    if (bOverlapped)
    {
        for (const FOverlapResult& Overlap : Overlaps)
        {
            AActor* HitActor = Overlap.GetActor();
            if (!IsValid(HitActor) || IgnoredActors.Contains(HitActor) || HitActor == MovingActor)
                continue;
            if (Overlap.Component.IsValid() &&
                Overlap.Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
            {
                OutFailureReason = FString::Printf(TEXT("Candidate pose overlaps blocking object '%s'."), *HitActor->GetName());
                return false;
            }
        }
    }
    return true;
}

float UPlacementKernelSubsystem::ScoreCandidate(
    const FPlacementSolution& Solution,
    const FTransform& CurrentActorTransform,
    const FVector& AimPoint,
    const FVector& TargetFeatureWorldLocation) const
{
    if (!Solution.bValid)
        return TNumericLimits<float>::Max();

    const float AimDistance = FVector::Distance(AimPoint, TargetFeatureWorldLocation);
    const float TranslationCost = FVector::Distance(
        CurrentActorTransform.GetLocation(), Solution.ActorWorldTransform.GetLocation()) * 0.025f;
    const float AngularCost = CurrentActorTransform.GetRotation().AngularDistance(
        Solution.ActorWorldTransform.GetRotation()) * 25.0f;
    return AimDistance + TranslationCost + AngularCost;
}
